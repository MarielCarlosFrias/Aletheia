// Verifica que o Qt consegue gravar/ler WebP quando o plugin esta
// instalado. Em instalacoes sem qt6-image-formats-plugins o teste e
// ignorado (SKIP), para o ctest continuar verde em ambientes minimos.
// Tambem testa isFormatSupported() para formatos com e sem plugin.
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QTemporaryDir>
#include <cstdio>

#include "imagepostprocessor.h"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // --- Teste de isFormatSupported ---
    // PNG e sempre suportado pelo Qt base.
    {
        QString hint;
        if (!ImagePostProcessor::isFormatSupported("/tmp/foto.png", &hint)) {
            std::printf("FALHOU: PNG deveria ser suportado\n");
            return 1;
        }
    }
    // Formatos que precisam de plugin devem estar na lista de
    // pluginDependentExtensions.
    {
        const auto deps = ImagePostProcessor::pluginDependentExtensions();
        if (!deps.contains("heic") || !deps.contains("avif")
            || !deps.contains("webp")) {
            std::printf("FALHOU: pluginDependentExtensions incompleta\n");
            return 1;
        }
    }
    // Um ficheiro .heic sem plugin: isFormatSupported devolve false
    // e o hint menciona o formato (se o plugin nao existir).
    {
        QTemporaryDir tmp;
        const QString heicPath = tmp.path() + "/foto.heic";
        QFile f(heicPath);
        if (!f.open(QIODevice::WriteOnly)) {
            std::printf("FALHOU: nao foi possivel criar ficheiro .heic de teste\n");
            return 1;
        }
        f.write("nao e um HEIC real");
        f.close();
        QString hint;
        const bool supported = ImagePostProcessor::isFormatSupported(
            heicPath, &hint);
        if (QImageReader::supportedImageFormats().contains("heic")) {
            // Com plugin instalado, o conteudo invalido nao importa para
            // isFormatSupported (ele so verifica se o formato e conhecido).
            if (!supported) {
                std::printf("FALHOU: heic suportado mas devolveu false\n");
                return 1;
            }
        } else {
            // Sem plugin: deve devolver false e dar hint.
            if (supported || hint.isEmpty()) {
                std::printf("FALHOU: heic sem plugin deveria dar hint\n");
                return 1;
            }
        }
    }

    // --- Teste WebP original ---
    if (!QImageWriter::supportedImageFormats().contains("webp")) {
        std::printf("SKIP: plugin WebP nao instalado "
                    "(qt6-image-formats-plugins)\n");
        return 0;
    }

    QImage image(4, 4, QImage::Format_RGBA8888);
    image.fill(QColor(10, 20, 30, 128));

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        std::printf("FALHOU: nao foi possivel criar diretorio temporario\n");
        return 1;
    }

    const QString path = tmp.path() + "/test.webp";
    if (!image.save(path, "webp")) {
        std::printf("FALHOU: nao foi possivel gravar WebP\n");
        return 1;
    }

    const QImage read(path);
    if (read.isNull() || read.size() != image.size()) {
        std::printf("FALHOU: nao foi possivel ler o WebP gravado\n");
        return 1;
    }
    if (qAlpha(read.pixel(0, 0)) == 0) {
        std::printf("FALHOU: WebP perdeu o canal alfa\n");
        return 1;
    }

    std::printf("PASS: WebP com alfa + isFormatSupported\n");
    return 0;
}
