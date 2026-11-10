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
#include <QSemaphore>
#include <QMutex>
#include <iostream>
#include <utility>
#include <future>
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

    try {
        // Carrega o modelo uma vez (deteta falhas fatais antes de
        // processar). A sessao e reutilizada por todas as imagens: evita
        // re-hashear o modelo e recarrega-lo a cada ficheiro.
        auto modelCheck = BackgroundRemover::get(options_.modelPath);

        const QString format = writerFormat;
        // PNG e WebP suportam canal alfa; JPEG nao.
        const bool formatHasAlpha =
            (writerFormat == QLatin1String("png")
             || writerFormat == QLatin1String("webp"));

        int concurrency = std::max(1, options_.maxConcurrentImages);
        QSemaphore sem(concurrency);
        QMutex resultMutex;
        QMutex coutMutex;

        std::vector<std::future<void>> futures;
        futures.reserve(plan.size());

        for (int i = 0; i < plan.size(); ++i) {
            const auto entry = plan[i];
            const int idx = i;
            futures.emplace_back(std::async(std::launch::async, [&, entry, idx, format, formatHasAlpha]() {
                sem.acquire();
                const QString file = QFileInfo(entry.inputPath).fileName();
                const QString outName = QFileInfo(entry.outputPath).fileName();
                try {
                    const QImage img = ImagePostProcessor::loadImageExif(entry.inputPath);
                    if (img.isNull()) {
                        QMutexLocker lock(&resultMutex);
                        result.errors++;
                        const QString err = I18n::tr("Falha ao carregar a imagem", "Failed to load the image");
                        result.errorMessages.append("[ERRO] " + file + ": " + err);
                        QMutexLocker clog(&coutMutex);
                        std::cerr << "[ERRO] " << file.toStdString() << ": " << err.toStdString() << std::endl;
                    } else {
                        // Reutiliza a sessao carregada uma unica vez:
                        // chamar get() por imagem re-hasheava o modelo
                        // inteiro (176 MB) a cada ficheiro.
                        const std::shared_ptr<BackgroundRemover> remover =
                            modelCheck;
                        QImage processed = ImagePostProcessor::apply(remover->removeBackground(img), options_);
                        // JPEG nao tem canal alfa: ao gravar, o Qt
                        // descartaria o alfa e a area removida ficaria
                        // com a cor original do fundo. Compomos sobre
                        // branco, como a GUI faz.
                        if (!formatHasAlpha && !processed.isNull()) {
                            processed = ImagePostProcessor::compositeBackground(
                                processed, Qt::white);
                        }
                        QImageWriter writer(entry.outputPath, format.toUtf8());
                        if (format == "png" && options_.pngCompression >= 0) {
                            writer.setCompression(options_.pngCompression);
                        } else if (format == "jpg" || format == "jpeg") {
                            writer.setQuality(options_.jpegQuality);
                        } else if (format == "webp") {
                            writer.setQuality(options_.webpQuality);
                        }
                        if (!writer.write(processed)) {
                            QMutexLocker lock(&resultMutex);
                            result.errors++;
                            const QString err = I18n::tr("Falha ao salvar a imagem", "Failed to save the image");
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
                sem.release();
            }));
        }

        for (auto &f : futures) {
            f.get();
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