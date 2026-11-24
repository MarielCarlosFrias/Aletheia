#include "batchprocessor.h"
#include "backgroundremover.h"
#include "batchplan.h"
#include "i18n.h"
#include "imagepostprocessor.h"
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QImageReader>
#include <QMutex>
#include <iostream>
#include <utility>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>

BatchProcessor::BatchProcessor(ProcessingOptions options)
    : options_(std::move(options))
{}

BatchProcessor::Result BatchProcessor::processDirectory(
    const QString& inputDir,
    const QString& outputDir)
{
    Result result;
    QDir dir(inputDir);

    if (!dir.exists()) {
        std::cerr << I18n::tr("Diretorio de entrada nao existe: ",
                              "Input directory does not exist: ").toStdString()
                  << inputDir.toStdString() << std::endl;
        result.fatal = true;
        return result;
    }

    QDir outDir(outputDir);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        std::cerr << I18n::tr("Nao foi possivel criar diretorio de saida: ",
                              "Could not create output directory: ").toStdString()
                  << outputDir.toStdString() << std::endl;
        result.fatal = true;
        return result;
    }

    // Formato de saida: o formato pedido decide tanto o codigo do
    // escritor como a extensao dos ficheiros. Antes o plano era criado
    // sem extensao, pelo que "-f jpg" gravava JPEG dentro de ficheiros
    // .png (nome e conteudo discordantes).
    const QString requested = options_.outputFormat.trimmed().toLower();
    QString writerFormat = QStringLiteral("png");
    QString extension = QStringLiteral("png");
    if (requested.isEmpty() || requested == "png") {
        writerFormat = QStringLiteral("png");
        extension = QStringLiteral("png");
    } else if (requested == "jpg" || requested == "jpeg") {
        writerFormat = QStringLiteral("jpeg");
        extension = QStringLiteral("jpg");
    } else if (requested == "webp") {
        writerFormat = QStringLiteral("webp");
        extension = QStringLiteral("webp");
    } else {
        std::cerr << I18n::tr("Formato de saida invalido: ",
                              "Invalid output format: ").toStdString()
                  << requested.toStdString() << ". "
                  << I18n::tr("Use png, jpg ou webp.",
                              "Use png, jpg or webp.").toStdString()
                  << std::endl;
        result.fatal = true;
        return result;
    }

    // O plugin de escrita tem de existir neste build (o WebP e opcional
    // no Qt): sem esta verificacao o erro apareceria, imagem a imagem,
    // como "Falha ao salvar a imagem".
    if (!QImageWriter::supportedImageFormats().contains(
            writerFormat.toUtf8())) {
        std::cerr << I18n::tr("Este build nao suporta o formato pedido: ",
                              "This build does not support the requested format: ")
                         .toStdString()
                  << requested.toStdString() << " ("
                  << writerFormat.toStdString() << ")" << std::endl;
        result.fatal = true;
        return result;
    }

    // Filtragem de imagens e atribuicao de nomes de saida agora
    // compartilhados com a GUI via BatchPlan (antes duplicado aqui
    // e em MainWindow::processBatch). A filtragem e case-insensitive,
    // portanto "foto.JPG" tambem e processado.
    const QList<BatchPlan::Entry> plan =
        BatchPlan::build(dir, outDir, extension);
    if (plan.isEmpty()) {
        std::cout << I18n::tr("Nenhuma imagem encontrada em: ",
                              "No images found in: ").toStdString()
                  << inputDir.toStdString() << std::endl;
        return result;
    }

    result.total = plan.size();
    std::cout << I18n::tr("Processando ", "Processing ").toStdString()
              << plan.size()
              << I18n::tr(" imagens...", " images...").toStdString() << std::endl;

    // O idioma e detetado uma unica vez (cache estatico): garantir que
    // a leitura do QSettings acontece na thread principal e nao numa
    // das threads de trabalho.
    (void)I18n::lang();

    try {
        // Carrega o modelo uma vez (deteta falhas fatais antes de
        // processar), exceto no modo Chroma Key que nao usa ONNX. A
        // sessao e reutilizada por todas as imagens: evita re-hashear o
        // modelo e recarrega-lo a cada ficheiro.
        std::shared_ptr<BackgroundRemover> modelCheck;
        if (!options_.useColorRemoval) {
            modelCheck = BackgroundRemover::get(options_.modelPath);
        }

        const int concurrency = std::max(1, options_.maxConcurrentImages);
        QMutex resultMutex;
        QMutex coutMutex;

        // Pool fixo de threads: antes era criado um std::async (e uma
        // thread do sistema) por imagem, o que esgota os recursos em
        // lotes grandes. As threads vao buscar o proximo indice a um
        // contador atomico.
        std::atomic<int> nextIndex{0};
        const auto processEntry = [&](const BatchPlan::Entry& entry, int idx) {
            {
                const QString file = QFileInfo(entry.inputPath).fileName();
                const QString outName = QFileInfo(entry.outputPath).fileName();
                try {
                    const QImage img = ImagePostProcessor::loadImageExif(entry.inputPath);
                    if (img.isNull()) {
                        QMutexLocker lock(&resultMutex);
                        result.errors++;
                        QString err = I18n::tr("Falha ao carregar a imagem",
                                              "Failed to load the image");
                        // Se o formato precisa de plugin externo (HEIC/AVIF/
                        // WebP), diz quais os pacotes em falta.
                        QString hint;
                        if (!ImagePostProcessor::isFormatSupported(
                                entry.inputPath, &hint) && !hint.isEmpty()) {
                            err += QStringLiteral(" (%1)").arg(hint);
                        }
                        result.errorMessages.append("[ERRO] " + file + ": " + err);
                        QMutexLocker clog(&coutMutex);
                        std::cerr << "[ERRO] " << file.toStdString() << ": "
                                  << err.toStdString() << std::endl;
                    } else {
                        // Reutiliza a sessao carregada uma unica vez:
                        // chamar get() por imagem re-hasheava o modelo
                        // inteiro (176 MB) a cada ficheiro.
                        QImage cutout;
                        if (options_.useColorRemoval) {
                            cutout = ImagePostProcessor::removeByColor(
                                img, options_.chromaKeyColor,
                                options_.chromaKeyTolerance,
                                options_.chromaKeyContiguous,
                                options_.chromaKeyPreservePixelArt,
                                options_.chromaDespill,
                                options_.chromaFeather);
                        } else {
                            cutout = modelCheck->removeBackground(img);
                        }
                        QImage processed = ImagePostProcessor::apply(cutout, options_);
                        QString saveError;
                        if (!ImagePostProcessor::saveImage(
                                processed, entry.outputPath, options_,
                                entry.inputPath, &saveError)) {
                            QMutexLocker lock(&resultMutex);
                            result.errors++;
                            const QString err = I18n::tr("Falha ao salvar a imagem", "Failed to save the image")
                                + (saveError.isEmpty()
                                       ? QString()
                                       : QStringLiteral(": ") + saveError);
                            result.errorMessages.append("[ERRO] " + file + ": " + err);
                            QMutexLocker clog(&coutMutex);
                            std::cerr << "[ERRO] " << file.toStdString() << ": " << err.toStdString() << std::endl;
                        } else {
                            QMutexLocker lock(&resultMutex);
                            result.ok++;
                            QMutexLocker clog(&coutMutex);
                            std::cout << "[" << (idx + 1) << "/" << result.total << "] " << file.toStdString() << " -> " << outName.toStdString() << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    QMutexLocker lock(&resultMutex);
                    result.errors++;
                    const QString msg = I18n::tr("Falha no modelo: %1", "Model failure: %1").arg(QString::fromUtf8(e.what()));
                    result.errorMessages.append("[ERRO] " + file + ": " + msg);
                    QMutexLocker clog(&coutMutex);
                    std::cerr << "[ERRO] " << file.toStdString() << ": " << msg.toStdString() << std::endl;
                } catch (...) {
                    QMutexLocker lock(&resultMutex);
                    result.errors++;
                    const QString msg = I18n::tr("Falha desconhecida no modelo.", "Unknown model failure.");
                    result.errorMessages.append("[ERRO] " + file + ": " + msg);
                    QMutexLocker clog(&coutMutex);
                    std::cerr << "[ERRO] " << file.toStdString() << ": " << msg.toStdString() << std::endl;
                }
            }
        };

        const int workerCount = std::min<int>(concurrency, plan.size());
        std::vector<std::thread> workers;
        workers.reserve(workerCount);
        for (int w = 0; w < workerCount; ++w) {
            workers.emplace_back([&]() {
                for (;;) {
                    // Verifica o pedido de cancelamento externo (SIGINT)
                    // antes de iniciar a proxima imagem.
                    if (cancelFlag_ && cancelFlag_->load()) break;
                    const int idx = nextIndex.fetch_add(1);
                    if (idx >= plan.size()) break;
                    processEntry(plan[idx], idx);
                }
            });
        }
        for (auto& worker : workers) {
            worker.join();
        }

        // Se houve cancelamento externo, conta as imagens nao processadas
        // como erros e marca o resultado como cancelado (nao fatal).
        if (cancelFlag_ && cancelFlag_->load()) {
            const int processed = result.ok + result.errors;
            if (processed < result.total) {
                result.errors += result.total - processed;
                result.cancelled = true;
            }
        }
    } catch (const std::exception& e) {
        result.fatal = true;
        const QString msg = I18n::tr(
            "Falha no modelo: %1", "Model failure: %1")
            .arg(QString::fromUtf8(e.what()));
        result.errorMessages.append(msg);
        std::cerr << msg.toStdString() << std::endl;
        // As imagens ainda nao processadas tambem contam como erro,
        // sem apagar o que ja foi feito com sucesso.
        const int processed = result.ok + result.errors;
        if (processed < result.total)
            result.errors += result.total - processed;
    } catch (...) {
        result.fatal = true;
        const QString msg = I18n::tr(
            "Falha desconhecida no modelo.",
            "Unknown model failure.");
        result.errorMessages.append(msg);
        std::cerr << msg.toStdString() << std::endl;
        const int processed = result.ok + result.errors;
        if (processed < result.total)
            result.errors += result.total - processed;
    }

    return result;
}

BatchProcessor::Result BatchProcessor::processFile(
    const QString& inputPath,
    const QString& outputPath)
{
    Result result;
    result.total = 1;

    const QFileInfo inputInfo(inputPath);
    if (!inputInfo.exists() || !inputInfo.isFile()) {
        std::cerr << I18n::tr("Ficheiro de entrada inexistente: ",
                              "Input file does not exist: ").toStdString()
                  << inputPath.toStdString() << std::endl;
        result.fatal = true;
        return result;
    }

    const QString requested = options_.outputFormat.trimmed().toLower();
    QString writerFormat;
    if (requested.isEmpty() || requested == "png") {
        writerFormat = QStringLiteral("png");
    } else if (requested == "jpg" || requested == "jpeg") {
        writerFormat = QStringLiteral("jpeg");
    } else if (requested == "webp") {
        writerFormat = QStringLiteral("webp");
    } else {
        std::cerr << I18n::tr("Formato de saida invalido: ",
                              "Invalid output format: ").toStdString()
                  << requested.toStdString() << ". "
                  << I18n::tr("Use png, jpg ou webp.",
                              "Use png, jpg or webp.").toStdString()
                  << std::endl;
        result.fatal = true;
        return result;
    }
    if (!QImageWriter::supportedImageFormats().contains(
            writerFormat.toUtf8())) {
        std::cerr << I18n::tr(
            "Este build nao suporta o formato pedido: ",
            "This build does not support the requested format: ").toStdString()
                  << requested.toStdString() << std::endl;
        result.fatal = true;
        return result;
    }

    const QFileInfo outputInfo(outputPath);
    const QString outputDir = outputInfo.absolutePath();
    if (!QDir(outputDir).exists() && !QDir(outputDir).mkpath(".")) {
        std::cerr << I18n::tr("Nao foi possivel criar diretorio de saida: ",
                              "Could not create output directory: ").toStdString()
                  << outputDir.toStdString() << std::endl;
        result.fatal = true;
        return result;
    }

    try {
        if (cancelFlag_ && cancelFlag_->load()) {
            result.cancelled = true;
            result.errors = 1;
            return result;
        }

        const QImage image =
            ImagePostProcessor::loadImageExif(inputPath);
        if (image.isNull()) {
            QString error = I18n::tr("Falha ao carregar a imagem",
                                    "Failed to load the image");
            QString hint;
            if (!ImagePostProcessor::isFormatSupported(inputPath, &hint)
                && !hint.isEmpty()) {
                error += QStringLiteral(" (%1)").arg(hint);
            }
            result.errors = 1;
            result.errorMessages.append(error);
            std::cerr << error.toStdString() << std::endl;
            return result;
        }

        QImage cutout;
        if (options_.useColorRemoval) {
            cutout = ImagePostProcessor::removeByColor(
                image, options_.chromaKeyColor,
                options_.chromaKeyTolerance,
                options_.chromaKeyContiguous,
                options_.chromaKeyPreservePixelArt,
                options_.chromaDespill,
                options_.chromaFeather);
        } else {
            const auto remover = BackgroundRemover::get(options_.modelPath);
            cutout = remover->removeBackground(image);
        }

        const QImage processed =
            ImagePostProcessor::apply(cutout, options_);
        QString saveError;
        if (!ImagePostProcessor::saveImage(
                processed, outputPath, options_, inputPath, &saveError)) {
            const QString error = I18n::tr(
                "Falha ao salvar a imagem", "Failed to save the image")
                + (saveError.isEmpty()
                       ? QString()
                       : QStringLiteral(": ") + saveError);
            result.errors = 1;
            result.errorMessages.append(error);
            std::cerr << error.toStdString() << std::endl;
            return result;
        }

        result.ok = 1;
        std::cout << inputInfo.fileName().toStdString() << " -> "
                  << outputInfo.fileName().toStdString() << std::endl;
    } catch (const std::exception& e) {
        result.errors = 1;
        const QString message = I18n::tr(
            "Falha no modelo: %1", "Model failure: %1")
            .arg(QString::fromUtf8(e.what()));
        result.errorMessages.append(message);
        std::cerr << message.toStdString() << std::endl;
    } catch (...) {
        result.errors = 1;
        const QString message = I18n::tr(
            "Falha desconhecida no modelo.",
            "Unknown model failure.");
        result.errorMessages.append(message);
        std::cerr << message.toStdString() << std::endl;
    }

    return result;
}