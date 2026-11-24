#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRectF>
#include <QStringList>
#include <atomic>
#include <csignal>
#include <iostream>
#include "batchprocessor.h"
#include "i18n.h"
#include "modelpathresolver.h"

namespace {
#ifdef ALETHEIA_VERSION
const QString kVersion = QStringLiteral(ALETHEIA_VERSION);
#else
const QString kVersion = QStringLiteral("1.0.0");
#endif

// Flag global de interrupcao: o handler de SIGINT/SIGTERM apenas marca a
// flag (async-signal-safe); o BatchProcessor verifica-a entre imagens.
std::atomic<bool> g_interrupted{false};

void onSignal(int)
{
    g_interrupted.store(true);
}

bool parseInteger(const QString& text, int minimum, int maximum, int& value)
{
    bool ok = false;
    const int parsed = text.toInt(&ok);
    if (!ok || parsed < minimum || parsed > maximum) return false;
    value = parsed;
    return true;
}

bool parseColor(const QString& text, QColor& color)
{
    const QColor parsed(text.trimmed());
    if (!parsed.isValid()) return false;
    color = parsed;
    color.setAlpha(255);
    return true;
}

// Aceita "LARGURA" (altura automatica) ou "LARGURAxALTURA".
bool parseSize(const QString& text, int& width, int& height)
{
    const QString normalized = text.trimmed().toLower().remove(' ');
    const QStringList parts = normalized.split(QLatin1Char('x'));
    if (parts.isEmpty() || parts.size() > 2) return false;
    if (!parseInteger(parts.at(0), 1, 20000, width)) return false;
    if (parts.size() == 1) {
        height = 0;
        return true;
    }
    return parseInteger(parts.at(1), 1, 20000, height);
}

// Recorte normalizado: x,y,largura,altura (todos entre 0 e 1).
bool parseCrop(const QString& text, QRectF& crop)
{
    const QStringList parts = text.trimmed().split(QLatin1Char(','));
    if (parts.size() != 4) return false;
    bool okX = false;
    bool okY = false;
    bool okW = false;
    bool okH = false;
    const double x = parts.at(0).toDouble(&okX);
    const double y = parts.at(1).toDouble(&okY);
    const double width = parts.at(2).toDouble(&okW);
    const double height = parts.at(3).toDouble(&okH);
    if (!okX || !okY || !okW || !okH) return false;
    if (x < 0.0 || y < 0.0 || width <= 0.0 || height <= 0.0) return false;
    if (x + width > 1.000001 || y + height > 1.000001) return false;
    crop = QRectF(x, y, width, height);
    return true;
}

QString extensionForFormat(const QString& format)
{
    const QString normalized = format.trimmed().toLower();
    if (normalized == "jpg" || normalized == "jpeg") return QStringLiteral("jpg");
    if (normalized == "webp") return QStringLiteral("webp");
    return QStringLiteral("png");
}

QString singleFileOutputPath(const QString& inputPath,
                              const QString& outputArgument,
                              const QString& extension)
{
    const QFileInfo inputInfo(inputPath);
    if (outputArgument.isEmpty()) {
        return QDir(inputInfo.absolutePath()).filePath(
            inputInfo.completeBaseName() + QStringLiteral("_rembg.")
            + extension);
    }

    const QFileInfo outputInfo(outputArgument);
    if (outputInfo.isDir()) {
        return QDir(outputInfo.absoluteFilePath()).filePath(
            inputInfo.completeBaseName() + QStringLiteral("_rembg.")
            + extension);
    }

    QString path = outputArgument;
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix.isEmpty()) {
        path += QLatin1Char('.') + extension;
    } else if (suffix != extension) {
        path = QFileInfo(path).absolutePath() + QLatin1Char('/')
             + QFileInfo(path).completeBaseName() + QLatin1Char('.') + extension;
    }
    return path;
}

void printOptionError(const QString& optionName, const QString& expected)
{
    std::cerr << I18n::tr("Valor inválido para %1: ",
                          "Invalid value for %1: ").toStdString()
              << optionName.toStdString() << ". "
              << expected.toStdString() << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("aletheia_cli");
    app.setApplicationVersion(kVersion);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        I18n::tr("Remoção de fundos de imagens localmente.",
                 "Offline image background removal."));

    const QCommandLineOption helpOption(
        QStringList() << "h" << "help",
        I18n::tr("Mostra esta ajuda.", "Show this help."));
    const QCommandLineOption versionOption(
        QStringList() << "v" << "version",
        I18n::tr("Mostra a versão.", "Show the version."));
    const QCommandLineOption outputOption(
        QStringList() << "o" << "output",
        I18n::tr("Diretório de saída (padrão: _rembg_output).",
                 "Output directory (default: _rembg_output)."),
        I18n::tr("pasta", "folder"));
    const QCommandLineOption modelOption(
        QStringList() << "m" << "model",
        I18n::tr("Caminho do modelo ONNX.",
                 "Path to the ONNX model."),
        I18n::tr("modelo.onnx", "model.onnx"));
    const QCommandLineOption formatOption(
        QStringList() << "f" << "format",
        I18n::tr("Formato de saída (png, jpg, webp).",
                 "Output format (png, jpg, webp)."),
        I18n::tr("formato", "format"),
        QStringLiteral("png"));
    const QCommandLineOption jobsOption(
        QStringList() << "j" << "jobs",
        I18n::tr("Número de imagens processadas em paralelo (1 = sequencial).",
                 "Number of images processed in parallel (1 = sequential)."),
        I18n::tr("N", "N"),
        QStringLiteral("1"));

    const QCommandLineOption qualityOption(
        QStringLiteral("quality"),
        I18n::tr("Qualidade JPEG/WebP (1-100).",
                 "JPEG/WebP quality (1-100)."),
        I18n::tr("N", "N"), QStringLiteral("90"));
    const QCommandLineOption pngCompressionOption(
        QStringLiteral("png-compression"),
        I18n::tr("Compressão PNG (0-9; 0 sem compressão).",
                 "PNG compression (0-9; 0 means no compression)."),
        I18n::tr("N", "N"), QStringLiteral("6"));
    const QCommandLineOption thresholdOption(
        QStringLiteral("threshold"),
        I18n::tr("Limiar alfa (0-255).", "Alpha threshold (0-255)."),
        I18n::tr("N", "N"), QStringLiteral("30"));
    const QCommandLineOption cleanNoiseOption(
        QStringLiteral("clean-noise"),
        I18n::tr("Limita pixels com alfa abaixo do limiar.",
                 "Make pixels below the alpha threshold transparent."));
    const QCommandLineOption refineOption(
        QStringLiteral("refine-intensity"),
        I18n::tr("Intensidade do refinamento de bordas (0-100).",
                 "Edge refinement intensity (0-100)."),
        I18n::tr("N", "N"), QStringLiteral("50"));
    const QCommandLineOption stickerOption(
        QStringLiteral("sticker"),
        I18n::tr("Adiciona contorno de sticker.", "Add a sticker outline."));
    const QCommandLineOption stickerWidthOption(
        QStringLiteral("sticker-width"),
        I18n::tr("Espessura do contorno (1-50 px).",
                 "Sticker outline width (1-50 px)."),
        I18n::tr("N", "N"), QStringLiteral("6"));
    const QCommandLineOption stickerColorOption(
        QStringLiteral("sticker-color"),
        I18n::tr("Cor do contorno (#RRGGBB).",
                 "Sticker outline color (#RRGGBB)."),
        I18n::tr("cor", "color"), QStringLiteral("#ffffff"));
    const QCommandLineOption shadowOption(
        QStringLiteral("shadow"),
        I18n::tr("Adiciona sombra projetada.", "Add a drop shadow."));
    const QCommandLineOption shadowBlurOption(
        QStringLiteral("shadow-blur"),
        I18n::tr("Desfoque da sombra (0-80 px).",
                 "Shadow blur (0-80 px)."),
        I18n::tr("N", "N"), QStringLiteral("10"));
    const QCommandLineOption shadowOffsetXOption(
        QStringLiteral("shadow-offset-x"),
        I18n::tr("Deslocamento horizontal da sombra (-100 a 100).",
                 "Horizontal shadow offset (-100 to 100)."),
        I18n::tr("N", "N"), QStringLiteral("4"));
    const QCommandLineOption shadowOffsetYOption(
        QStringLiteral("shadow-offset-y"),
        I18n::tr("Deslocamento vertical da sombra (-100 a 100).",
                 "Vertical shadow offset (-100 to 100)."),
        I18n::tr("N", "N"), QStringLiteral("6"));
    const QCommandLineOption shadowOpacityOption(
        QStringLiteral("shadow-opacity"),
        I18n::tr("Opacidade da sombra (0-100).",
                 "Shadow opacity (0-100)."),
        I18n::tr("N", "N"), QStringLiteral("50"));
    const QCommandLineOption backgroundOption(
        QStringLiteral("background"),
        I18n::tr("Fundo sólido de saída (#RRGGBB).",
                 "Solid output background (#RRGGBB)."),
        I18n::tr("cor", "color"));
    const QCommandLineOption resizeOption(
        QStringLiteral("resize"),
        I18n::tr("Tamanho final: LARGURA ou LARGURAxALTURA.",
                 "Final size: WIDTH or WIDTHxHEIGHT."),
        I18n::tr("WxH", "WxH"));
    const QCommandLineOption rotateOption(
        QStringLiteral("rotate"),
        I18n::tr("Rotação final (0, 90, 180 ou 270 graus).",
                 "Final rotation (0, 90, 180 or 270 degrees)."),
        I18n::tr("N", "N"), QStringLiteral("0"));
    const QCommandLineOption cropOption(
        QStringLiteral("crop"),
        I18n::tr("Recorte normalizado: x,y,largura,altura.",
                 "Normalized crop: x,y,width,height."),
        I18n::tr("x,y,w,h", "x,y,w,h"));
    const QCommandLineOption chromaKeyOption(
        QStringLiteral("chroma-key"),
        I18n::tr("Remoção por cor em vez de IA (#RRGGBB).",
                 "Color removal instead of AI (#RRGGBB)."),
        I18n::tr("cor", "color"));
    const QCommandLineOption chromaToleranceOption(
        QStringLiteral("chroma-tolerance"),
        I18n::tr("Tolerância da cor-chave (0-100).",
                 "Chroma key tolerance (0-100)."),
        I18n::tr("N", "N"), QStringLiteral("0"));
    const QCommandLineOption chromaContiguousOption(
        QStringLiteral("chroma-contiguous"),
        I18n::tr("Remove apenas a cor conectada às bordas.",
                 "Remove only color connected to the image borders."));
    const QCommandLineOption chromaSoftOption(
        QStringLiteral("chroma-soft-edges"),
        I18n::tr("Desativa bordas rígidas de Pixel Art.",
                 "Disable hard Pixel Art edges."));
    const QCommandLineOption despillOption(
        QStringLiteral("despill"),
        I18n::tr("Descontaminação da cor-chave (0-100).",
                 "Chroma key despill (0-100)."),
        I18n::tr("N", "N"), QStringLiteral("0"));
    const QCommandLineOption featherOption(
        QStringLiteral("feather"),
        I18n::tr("Suavização da máscara (0-5 px).",
                 "Mask feather radius (0-5 px)."),
        I18n::tr("N", "N"), QStringLiteral("0"));
    const QCommandLineOption noMetadataOption(
        QStringLiteral("no-metadata"),
        I18n::tr("Não copia EXIF, GPS ou outros metadados.",
                 "Do not copy EXIF, GPS or other metadata."));

    parser.addOption(helpOption);
    parser.addOption(versionOption);
    parser.addOption(outputOption);
    parser.addOption(modelOption);
    parser.addOption(formatOption);
    parser.addOption(jobsOption);
    parser.addOption(qualityOption);
    parser.addOption(pngCompressionOption);
    parser.addOption(thresholdOption);
    parser.addOption(cleanNoiseOption);
    parser.addOption(refineOption);
    parser.addOption(stickerOption);
    parser.addOption(stickerWidthOption);
    parser.addOption(stickerColorOption);
    parser.addOption(shadowOption);
    parser.addOption(shadowBlurOption);
    parser.addOption(shadowOffsetXOption);
    parser.addOption(shadowOffsetYOption);
    parser.addOption(shadowOpacityOption);
    parser.addOption(backgroundOption);
    parser.addOption(resizeOption);
    parser.addOption(rotateOption);
    parser.addOption(cropOption);
    parser.addOption(chromaKeyOption);
    parser.addOption(chromaToleranceOption);
    parser.addOption(chromaContiguousOption);
    parser.addOption(chromaSoftOption);
    parser.addOption(despillOption);
    parser.addOption(featherOption);
    parser.addOption(noMetadataOption);
    parser.addPositionalArgument(
        I18n::tr("entrada", "input"),
        I18n::tr("Ficheiro ou diretório com imagens.",
                 "Image file or directory."));
    parser.addPositionalArgument(
        I18n::tr("saída", "output"),
        I18n::tr("Diretório de saída (opcional).",
                 "Output directory (optional)."),
        I18n::tr("[saída]", "[output]"));
    parser.addPositionalArgument(
        I18n::tr("modelo", "model"),
        I18n::tr("Modelo ONNX (opcional).",
                 "ONNX model (optional)."),
        I18n::tr("[modelo.onnx]", "[model.onnx]"));

    const auto printHelp = [&parser]() {
        std::cout << parser.helpText().toStdString()
                  << I18n::tr(
                         "\nExemplos:\n"
                         "  aletheia_cli ~/imagens ~/saida\n"
                         "  aletheia_cli foto.png -o foto_rembg.png\n"
                         "  aletheia_cli ~/imagens --model u2net.onnx\n"
                         "  aletheia_cli ~/imagens -f jpg --quality 92 -j 4\n"
                         "  aletheia_cli sprite.png --chroma-key '#00ff00' --despill 80\n"
                         "  aletheia_cli foto.png --resize 1000x1000 --rotate 90\n",
                         "\nExamples:\n"
                         "  aletheia_cli ~/pictures ~/output\n"
                         "  aletheia_cli photo.png -o photo_rembg.png\n"
                         "  aletheia_cli ~/pictures --model u2net.onnx\n"
                         "  aletheia_cli ~/pictures -f jpg --quality 92 -j 4\n"
                         "  aletheia_cli sprite.png --chroma-key '#00ff00' --despill 80\n"
                         "  aletheia_cli photo.png --resize 1000x1000 --rotate 90\n")
                         .toStdString()
                  << std::flush;
    };

    if (!parser.parse(app.arguments())) {
        std::cerr << parser.errorText().toStdString() << "\n\n";
        printHelp();
        return 1;
    }

    if (parser.isSet(helpOption)) {
        printHelp();
        return 0;
    }
    if (parser.isSet(versionOption)) {
        std::cout << app.applicationName().toStdString() << " "
                  << app.applicationVersion().toStdString() << "\n";
        return 0;
    }

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        std::cerr << I18n::tr("Informe o ficheiro ou pasta de entrada.",
                              "The input file or folder is required.")
                         .toStdString()
                  << "\n\n";
        printHelp();
        return 1;
    }
    if (positional.size() > 3) {
        std::cerr << I18n::tr("Argumentos a mais. Use --help para ver o uso.",
                              "Too many arguments. Use --help for usage.")
                         .toStdString()
                  << "\n";
        return 1;
    }

    const QString inputPath = positional[0];
    QString outputPath = parser.value(outputOption);
    QString modelPath = parser.value(modelOption);

    // Compatibilidade com os formatos posicionais antigos:
    //   <entrada> <saída> [modelo.onnx]
    //   <entrada> <modelo.onnx> [saída]
    const QString pos2 = positional.size() > 1 ? positional[1] : QString();
    const QString pos3 = positional.size() > 2 ? positional[2] : QString();
    if (!pos2.isEmpty()) {
        if (pos2.endsWith(".onnx", Qt::CaseInsensitive)) {
            if (modelPath.isEmpty()) modelPath = pos2;
            if (!pos3.isEmpty() && outputPath.isEmpty()) outputPath = pos3;
        } else {
            if (outputPath.isEmpty()) outputPath = pos2;
            if (!pos3.isEmpty()) {
                if (!pos3.endsWith(".onnx", Qt::CaseInsensitive)) {
                    std::cerr << I18n::tr(
                        "O modelo deve terminar em .onnx: ",
                        "The model must end in .onnx: ").toStdString()
                              << pos3.toStdString() << "\n";
                    return 1;
                }
                if (modelPath.isEmpty()) modelPath = pos3;
            }
        }
    }

    const QFileInfo inputInfo(inputPath);
    if (!inputInfo.exists()) {
        std::cerr << I18n::tr("Entrada não existe: ",
                              "Input does not exist: ").toStdString()
                  << inputPath.toStdString() << std::endl;
        return 1;
    }

    ProcessingOptions options;
    options.outputFormat = parser.value(formatOption).trimmed().toLower();
    if (options.outputFormat.isEmpty()) options.outputFormat = QStringLiteral("png");
    if (options.outputFormat != "png" && options.outputFormat != "jpg"
        && options.outputFormat != "jpeg" && options.outputFormat != "webp") {
        std::cerr << I18n::tr("Formato de saída inválido: ",
                              "Invalid output format: ").toStdString()
                  << options.outputFormat.toStdString() << ". "
                  << I18n::tr("Use png, jpg ou webp.",
                              "Use png, jpg or webp.").toStdString()
                  << std::endl;
        return 1;
    }

    int quality = 90;
    if (!parseInteger(parser.value(qualityOption), 1, 100, quality)) {
        printOptionError(QStringLiteral("--quality"),
                         I18n::tr("use um inteiro entre 1 e 100.",
                                  "use an integer between 1 and 100."));
        return 1;
    }
    options.jpegQuality = quality;
    options.webpQuality = quality;

    int pngCompression = 6;
    if (!parseInteger(parser.value(pngCompressionOption), 0, 9, pngCompression)) {
        printOptionError(QStringLiteral("--png-compression"),
                         I18n::tr("use um inteiro entre 0 e 9.",
                                  "use an integer between 0 and 9."));
        return 1;
    }
    options.pngCompression = pngCompression;

    int threshold = 30;
    if (!parseInteger(parser.value(thresholdOption), 0, 255, threshold)) {
        printOptionError(QStringLiteral("--threshold"),
                         I18n::tr("use um inteiro entre 0 e 255.",
                                  "use an integer between 0 and 255."));
        return 1;
    }
    options.alphaThreshold = threshold;
    options.cleanNoise = parser.isSet(cleanNoiseOption);

    if (parser.isSet(refineOption)) {
        int intensity = 50;
        if (!parseInteger(parser.value(refineOption), 0, 100, intensity)) {
            printOptionError(QStringLiteral("--refine-intensity"),
                             I18n::tr("use um inteiro entre 0 e 100.",
                                      "use an integer between 0 and 100."));
            return 1;
        }
        options.refineEdges = true;
        options.edgeRefinementIntensity = intensity / 100.0f;
    }

    if (parser.isSet(stickerOption)) {
        int width = 6;
        if (!parseInteger(parser.value(stickerWidthOption), 1, 50, width)) {
            printOptionError(QStringLiteral("--sticker-width"),
                             I18n::tr("use um inteiro entre 1 e 50.",
                                      "use an integer between 1 and 50."));
            return 1;
        }
        QColor color;
        if (!parseColor(parser.value(stickerColorOption), color)) {
            printOptionError(QStringLiteral("--sticker-color"),
                             I18n::tr("use uma cor #RRGGBB.",
                                      "use a #RRGGBB color."));
            return 1;
        }
        options.stickerOutline = true;
        options.stickerWidth = width;
        options.stickerColor = color;
    }

    if (parser.isSet(shadowOption)) {
        int blur = 10;
        int offsetX = 4;
        int offsetY = 6;
        int opacity = 50;
        if (!parseInteger(parser.value(shadowBlurOption), 0, 80, blur)
            || !parseInteger(parser.value(shadowOffsetXOption), -100, 100, offsetX)
            || !parseInteger(parser.value(shadowOffsetYOption), -100, 100, offsetY)
            || !parseInteger(parser.value(shadowOpacityOption), 0, 100, opacity)) {
            std::cerr << I18n::tr("Valores inválidos para as opções de sombra.",
                                  "Invalid values for shadow options.")
                             .toStdString() << "\n";
            return 1;
        }
        options.dropShadow = true;
        options.shadowBlur = blur;
        options.shadowOffsetX = offsetX;
        options.shadowOffsetY = offsetY;
        options.shadowOpacity = opacity / 100.0f;
    }

    if (parser.isSet(backgroundOption)) {
        QColor color;
        if (!parseColor(parser.value(backgroundOption), color)) {
            printOptionError(QStringLiteral("--background"),
                             I18n::tr("use uma cor #RRGGBB.",
                                      "use a #RRGGBB color."));
            return 1;
        }
        options.backgroundStyle = ProcessingOptions::BgSolidColor;
        options.backgroundColor = color;
    }

    if (parser.isSet(resizeOption)) {
        if (!parseSize(parser.value(resizeOption),
                       options.resizeWidth, options.resizeHeight)) {
            printOptionError(QStringLiteral("--resize"),
                             I18n::tr("use LARGURA ou LARGURAxALTURA.",
                                      "use WIDTH or WIDTHxHEIGHT."));
            return 1;
        }
    }

    if (parser.isSet(rotateOption)) {
        int rotation = 0;
        if (!parseInteger(parser.value(rotateOption), 0, 359, rotation)
            || rotation % 90 != 0) {
            printOptionError(QStringLiteral("--rotate"),
                             I18n::tr("use 0, 90, 180 ou 270.",
                                      "use 0, 90, 180 or 270."));
            return 1;
        }
        options.rotationDegrees = rotation;
    }

    if (parser.isSet(cropOption)) {
        if (!parseCrop(parser.value(cropOption), options.cropRect)) {
            printOptionError(QStringLiteral("--crop"),
                             I18n::tr("use x,y,largura,altura normalizados.",
                                      "use normalized x,y,width,height."));
            return 1;
        }
        options.hasCrop = true;
    }

    if (parser.isSet(chromaKeyOption)) {
        QColor color;
        if (!parseColor(parser.value(chromaKeyOption), color)) {
            printOptionError(QStringLiteral("--chroma-key"),
                             I18n::tr("use uma cor #RRGGBB.",
                                      "use a #RRGGBB color."));
            return 1;
        }
        int tolerance = 0;
        if (!parseInteger(parser.value(chromaToleranceOption), 0, 100, tolerance)) {
            printOptionError(QStringLiteral("--chroma-tolerance"),
                             I18n::tr("use um inteiro entre 0 e 100.",
                                      "use an integer between 0 and 100."));
            return 1;
        }
        options.useColorRemoval = true;
        options.chromaKeyColor = color;
        options.chromaKeyTolerance = tolerance;
        options.chromaKeyContiguous = parser.isSet(chromaContiguousOption);
        options.chromaKeyPreservePixelArt = !parser.isSet(chromaSoftOption);
    }

    int despill = 0;
    if (!parseInteger(parser.value(despillOption), 0, 100, despill)) {
        printOptionError(QStringLiteral("--despill"),
                         I18n::tr("use um inteiro entre 0 e 100.",
                                  "use an integer between 0 and 100."));
        return 1;
    }
    options.chromaDespill = despill / 100.0f;

    int feather = 0;
    if (!parseInteger(parser.value(featherOption), 0, 5, feather)) {
        printOptionError(QStringLiteral("--feather"),
                         I18n::tr("use um inteiro entre 0 e 5.",
                                  "use an integer between 0 and 5."));
        return 1;
    }
    options.chromaFeather = feather;
    options.preserveMetadata = !parser.isSet(noMetadataOption);

    if (!options.useColorRemoval
        && (parser.isSet(chromaToleranceOption)
            || parser.isSet(chromaContiguousOption)
            || parser.isSet(chromaSoftOption)
            || parser.isSet(despillOption)
            || parser.isSet(featherOption))) {
        std::cerr << I18n::tr(
            "As opções de Chroma Key exigem --chroma-key.",
            "Chroma Key options require --chroma-key.").toStdString()
                  << "\n";
        return 1;
    }

    bool jobsOk = false;
    const int jobs = parser.value(jobsOption).toInt(&jobsOk);
    if (!jobsOk || jobs < 1) {
        std::cerr << I18n::tr("Número de tarefas inválido: ",
                              "Invalid number of jobs: ").toStdString()
                  << parser.value(jobsOption).toStdString()
                  << I18n::tr(". Use um inteiro >= 1.",
                              ". Use an integer >= 1.").toStdString()
                  << std::endl;
        return 1;
    }
    options.maxConcurrentImages = jobs;

    if (!options.useColorRemoval) {
        if (modelPath.isEmpty()) modelPath = ModelPathResolver::resolve("u2netp.onnx");
        if (!QFile::exists(modelPath)) {
            std::cerr << I18n::tr("Modelo não encontrado: ",
                                  "Model not found: ").toStdString()
                      << modelPath.toStdString() << std::endl;
            return 1;
        }
        options.modelPath = modelPath;
    }

    if (inputInfo.isDir() && outputPath.isEmpty())
        outputPath = QStringLiteral("_rembg_output");

    std::cout << I18n::tr("Entrada: ", "Input: ").toStdString()
              << inputPath.toStdString();
    if (!options.useColorRemoval) {
        std::cout << "\n" << I18n::tr("Modelo: ", "Model: ").toStdString()
                  << options.modelPath.toStdString();
    }
    std::cout << "\n" << I18n::tr("Saída: ", "Output: ").toStdString()
              << (inputInfo.isDir() ? outputPath
                                    : singleFileOutputPath(
                                          inputPath, outputPath,
                                          extensionForFormat(options.outputFormat)))
                         .toStdString()
              << "\n---\n" << std::flush;

    // Ctrl+C interrompe o lote de forma limpa: as imagens ja processadas
    // ficam guardadas e as restantes contam como nao processadas.
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    BatchProcessor processor(options);
    processor.setCancelFlag(&g_interrupted);
    BatchProcessor::Result r;
    if (inputInfo.isDir()) {
        r = processor.processDirectory(inputPath, outputPath);
    } else {
        const QString destination = singleFileOutputPath(
            inputPath, outputPath,
            extensionForFormat(options.outputFormat));
        const QString inputCanonical = QFileInfo(inputPath).canonicalFilePath();
        const QString outputCanonical = QFileInfo(destination).canonicalFilePath();
        if ((!inputCanonical.isEmpty()
             && inputCanonical == outputCanonical)
            || QFileInfo(inputPath).absoluteFilePath()
                   == QFileInfo(destination).absoluteFilePath()) {
            std::cerr << I18n::tr(
                "A saída não pode substituir o ficheiro de entrada.",
                "The output cannot overwrite the input file.").toStdString()
                      << "\n";
            return 1;
        }
        r = processor.processFile(inputPath, destination);
    }

    std::cout << "---\n"
              << (r.fatal
                      ? I18n::tr("Lote interrompido: ",
                                 "Batch interrupted: ")
                      : r.cancelled
                          ? I18n::tr("Cancelado pelo utilizador: ",
                                     "Cancelled by user: ")
                          : I18n::tr("Concluido: ", "Completed: "))
                     .toStdString()
              << r.ok << "/" << r.total << " OK, "
              << r.errors << " "
              << I18n::tr("erros", "errors").toStdString() << "\n";

    if (!r.errorMessages.isEmpty()) {
        std::cout << I18n::tr("Erros:", "Errors:").toStdString() << "\n";
        for (const QString& msg : r.errorMessages) {
            std::cout << "  " << msg.toStdString() << "\n";
        }
    }

    // Cancelamento pelo utilizador (Ctrl+C) sai com codigo 130 (convencao
    // Unix para SIGINT); erros/fatal com 1; sucesso com 0.
    if (r.cancelled) return 130;
    return (r.fatal || r.errors > 0) ? 1 : 0;
}
