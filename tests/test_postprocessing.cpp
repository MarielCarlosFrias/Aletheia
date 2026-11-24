#include <QImage>
#include <QImageWriter>
#include <iostream>

#include "imagepostprocessor.h"

int main()
{
    QImage alphaImage(3, 3, QImage::Format_RGBA8888);
    alphaImage.fill(qRgba(10, 20, 30, 20));
    alphaImage.setPixel(1, 1, qRgba(10, 20, 30, 255));

    const QImage thresholded = ImagePostProcessor::applyAlphaThreshold(
        alphaImage, 30.0f);
    if (qAlpha(thresholded.pixel(0, 0)) != 0
        || qAlpha(thresholded.pixel(1, 1)) != 255) {
        std::cerr << "FAIL: threshold" << std::endl;
        return 1;
    }

    const QImage refined = ImagePostProcessor::applyEdgeRefinement(
        thresholded, 0.5f);
    if (qAlpha(refined.pixel(0, 1)) == 0) {
        std::cerr << "FAIL: refinement" << std::endl;
        return 1;
    }

    QImage translucent(1, 1, QImage::Format_RGBA8888);
    translucent.setPixel(0, 0, qRgba(255, 0, 0, 128));
    const QImage composited = ImagePostProcessor::compositeBackground(
        translucent, Qt::white);
    const QRgb pixel = composited.pixel(0, 0);
    if (qRed(pixel) != 255 || qGreen(pixel) < 126 || qGreen(pixel) > 128
        || qBlue(pixel) < 126 || qBlue(pixel) > 128 || qAlpha(pixel) != 255) {
        std::cerr << "FAIL: composite" << std::endl;
        return 1;
    }

    // Teste de Auto-Crop
    QImage cropSource(30, 30, QImage::Format_RGBA8888);
    cropSource.fill(Qt::transparent);
    for (int y = 10; y < 20; ++y) {
        for (int x = 10; x < 20; ++x) {
            cropSource.setPixel(x, y, qRgba(255, 0, 0, 255));
        }
    }
    const QImage cropped = ImagePostProcessor::autoCrop(cropSource, 0);
    if (cropped.width() != 10 || cropped.height() != 10) {
        std::cerr << "FAIL: autoCrop dimensions expected 10x10, got "
                  << cropped.width() << "x" << cropped.height() << std::endl;
        return 1;
    }

    // Teste de Sticker Outline
    const QImage sticker = ImagePostProcessor::applyStickerOutline(
        cropSource, 2, Qt::white);
    if (sticker.size() != cropSource.size() || qAlpha(sticker.pixel(8, 10)) == 0) {
        std::cerr << "FAIL: sticker outline" << std::endl;
        return 1;
    }

    // Teste de Drop Shadow
    const QImage shadowed = ImagePostProcessor::applyDropShadow(
        cropSource, 2, 2, 4, 0.5f);
    if (shadowed.size() != cropSource.size() || qAlpha(shadowed.pixel(21, 21)) == 0) {
        std::cerr << "FAIL: drop shadow" << std::endl;
        return 1;
    }

    // Teste de Blur
    const QImage blurred = ImagePostProcessor::blurImage(cropSource, 3);
    if (blurred.size() != cropSource.size()) {
        std::cerr << "FAIL: blur" << std::endl;
        return 1;
    }

    // Teste de visualizeMask: o destino e RGB888 (3 bytes/pixel) e a
    // origem RGBA8888 (4 bytes/pixel). Escrever com QRgb no destino
    // corrompia a heap e abortava o processo (ASAN: heap-buffer-overflow).
    QImage maskSource(4, 2, QImage::Format_RGBA8888);
    maskSource.setPixel(0, 0, qRgba(10, 20, 30, 0));    // alfa 0
    maskSource.setPixel(1, 0, qRgba(10, 20, 30, 128));  // alfa 128
    maskSource.setPixel(2, 0, qRgba(10, 20, 30, 255));  // alfa 255
    maskSource.setPixel(3, 0, qRgba(10, 20, 30, 64));
    maskSource.setPixel(0, 1, qRgba(10, 20, 30, 255));
    maskSource.setPixel(1, 1, qRgba(10, 20, 30, 255));
    maskSource.setPixel(2, 1, qRgba(10, 20, 30, 255));
    maskSource.setPixel(3, 1, qRgba(10, 20, 30, 255));

    const QImage maskOverlay =
        ImagePostProcessor::visualizeMask(maskSource, true);
    if (maskOverlay.size() != maskSource.size()) {
        std::cerr << "FAIL: visualizeMask dimensions" << std::endl;
        return 1;
    }
    // Overlay: vermelho (255-a) no canal R, verde (a) no canal G.
    const QColor overlay0 = maskOverlay.pixelColor(0, 0);
    const QColor overlay255 = maskOverlay.pixelColor(2, 0);
    if (overlay0.red() != 255 || overlay0.green() != 0
        || overlay255.red() != 0 || overlay255.green() != 255
        || overlay255.blue() != 0) {
        std::cerr << "FAIL: visualizeMask overlay colors: "
                  << overlay0.name().toStdString() << " "
                  << overlay255.name().toStdString() << std::endl;
        return 1;
    }

    const QImage maskGrey = ImagePostProcessor::visualizeMask(maskSource, false);
    const QColor grey128 = maskGrey.pixelColor(1, 0);
    if (grey128.red() != 128 || grey128.green() != 128
        || grey128.blue() != 128) {
        std::cerr << "FAIL: visualizeMask grayscale: "
                  << grey128.name().toStdString() << std::endl;
        return 1;
    }

    // O ultimo pixel da ultima linha e o que rebentava com o
    // heap-buffer-overflow: escreve 4 bytes num buffer de 3.
    const QImage maskEdge = ImagePostProcessor::visualizeMask(
        maskSource, true);
    if (maskEdge.constScanLine(maskEdge.height() - 1) == nullptr) {
        std::cerr << "FAIL: visualizeMask scanline" << std::endl;
        return 1;
    }

    // Teste de detectCornerColor e removeByColor (Pixel Art / Chroma Key)
    QImage sprite(20, 20, QImage::Format_RGBA8888);
    sprite.fill(qRgb(255, 0, 255)); // Magenta de fundo
    // Desenha um anel verde com buraco magenta no centro
    for (int y = 5; y <= 15; ++y) {
        for (int x = 5; x <= 15; ++x) {
            sprite.setPixel(x, y, qRgb(0, 255, 0));
        }
    }
    sprite.setPixel(10, 10, qRgb(255, 0, 255)); // Buraco isolado

    const QColor detected = ImagePostProcessor::detectCornerColor(sprite);
    if (detected.red() != 255 || detected.green() != 0 || detected.blue() != 255) {
        std::cerr << "FAIL: detectCornerColor" << std::endl;
        return 1;
    }

    // detectCornerColor com cantos transparentes (alpha=0): deve ignorar
    // os pixels transparentes e detetar a cor opaca da borda interna.
    QImage transpCorners(20, 20, QImage::Format_RGBA8888);
    transpCorners.fill(Qt::transparent); // cantos transparentes (RGB=0)
    for (int y = 3; y < 17; ++y)
        for (int x = 3; x < 17; ++x)
            transpCorners.setPixel(x, y, qRgb(255, 0, 255));
    const QColor detTransp = ImagePostProcessor::detectCornerColor(transpCorners);
    if (detTransp.name().toUpper() != "#FF00FF") {
        std::cerr << "FAIL: detectCornerColor com cantos transparentes: "
                  << detTransp.name().toUpper().toStdString() << std::endl;
        return 1;
    }

    // detectCornerColor em imagem 100% transparente: fallback magenta.
    QImage allTransp(10, 10, QImage::Format_RGBA8888);
    allTransp.fill(Qt::transparent);
    const QColor detFallback = ImagePostProcessor::detectCornerColor(allTransp);
    if (detFallback.name().toUpper() != "#FF00FF") {
        std::cerr << "FAIL: detectCornerColor fallback: "
                  << detFallback.name().toUpper().toStdString() << std::endl;
        return 1;
    }

    // detectCornerColor com fundo verde solido (chroma key classico).
    QImage greenBg(30, 30, QImage::Format_RGB32);
    greenBg.fill(qRgb(0, 255, 0));
    const QColor detGreen = ImagePostProcessor::detectCornerColor(greenBg);
    if (detGreen.name().toUpper() != "#00FF00") {
        std::cerr << "FAIL: detectCornerColor verde: "
                  << detGreen.name().toUpper().toStdString() << std::endl;
        return 1;
    }

    // Modo contíguo (deve manter o buraco interno isolado com alfa 255)
    const QImage contiguousResult = ImagePostProcessor::removeByColor(
        sprite, QColor(255, 0, 255), 0, true, true);
    if (qAlpha(contiguousResult.pixel(0, 0)) != 0
        || qAlpha(contiguousResult.pixel(5, 5)) != 255
        || qAlpha(contiguousResult.pixel(10, 10)) != 255) {
        std::cerr << "FAIL: removeByColor contiguous" << std::endl;
        return 1;
    }

    // Modo global (deve remover o buraco interno também)
    const QImage globalResult = ImagePostProcessor::removeByColor(
        sprite, QColor(255, 0, 255), 0, false, true);
    if (qAlpha(globalResult.pixel(0, 0)) != 0
        || qAlpha(globalResult.pixel(5, 5)) != 255
        || qAlpha(globalResult.pixel(10, 10)) != 0) {
        std::cerr << "FAIL: removeByColor global" << std::endl;
        return 1;
    }

    // Teste de applyGeometry: recorte normalizado.
    ProcessingOptions cropOpts;
    cropOpts.hasCrop = true;
    cropOpts.cropRect = QRectF(0.25, 0.25, 0.5, 0.5);
    QImage cropSrc(40, 40, QImage::Format_RGBA8888);
    cropSrc.fill(qRgba(255, 0, 0, 255));
    const QImage geometryCropped = ImagePostProcessor::applyGeometry(cropSrc, cropOpts);
    if (geometryCropped.size() != QSize(20, 20)) {
        std::cerr << "FAIL: applyGeometry crop size expected 20x20, got "
                  << geometryCropped.width() << "x" << geometryCropped.height() << std::endl;
        return 1;
    }

    // Teste de applyGeometry: rotacao 180.
    ProcessingOptions rotateOpts;
    rotateOpts.rotationDegrees = 180;
    QImage rotateSrc(20, 10, QImage::Format_RGBA8888);
    rotateSrc.fill(Qt::transparent);
    rotateSrc.setPixel(5, 2, qRgba(255, 255, 255, 255));
    const QImage rotated = ImagePostProcessor::applyGeometry(rotateSrc, rotateOpts);
    // (5,2) com rotacao 180 em 20x10 -> (14,7)
    if (rotated.pixelColor(14, 7) != QColor(255, 255, 255, 255)) {
        std::cerr << "FAIL: applyGeometry rotate 180 moved pixel incorrectly"
                  << std::endl;
        return 1;
    }

    // Teste de applyGeometry: redimensionamento para largura fixa.
    ProcessingOptions resizeOpts;
    resizeOpts.resizeWidth = 10;
    QImage resizeSrc(20, 20, QImage::Format_RGBA8888);
    resizeSrc.fill(Qt::transparent);
    const QImage resized = ImagePostProcessor::applyGeometry(resizeSrc, resizeOpts);
    if (resized.size() != QSize(10, 10)) {
        std::cerr << "FAIL: applyGeometry resize expected 10x10, got "
                  << resized.width() << "x" << resized.height() << std::endl;
        return 1;
    }

    // Imagem totalmente transparente, usada para testar saida JPEG/PNG/WebP.
    QImage jpegSrc(2, 2, QImage::Format_RGBA8888);
    jpegSrc.fill(qRgba(0, 0, 0, 0));

    // Teste de saveImage: JPEG sem alfa compoe sobre branco.
    if (QImageWriter::supportedImageFormats().contains("jpeg")) {
        ProcessingOptions jpegOpts;
        jpegOpts.outputFormat = "jpg";
        jpegOpts.jpegQuality = 80;
        QString tmpJpeg = QStringLiteral("/tmp/test_postprocessing_jpeg.jpg");
        QString jpegError;
        if (!ImagePostProcessor::saveImage(jpegSrc, tmpJpeg, jpegOpts,
                                           QString(), &jpegError)) {
            std::cerr << "FAIL: saveImage JPEG: " << jpegError.toStdString()
                      << std::endl;
            return 1;
        }
        QImage jpegRead(tmpJpeg);
        if (jpegRead.isNull()) {
            std::cerr << "FAIL: saveImage JPEG output nao pode ser lido"
                      << std::endl;
            return 1;
        }
        // Fundo transparente deve virar branco no JPEG.
        const QRgb jpegPixel = jpegRead.pixel(0, 0);
        if (qRed(jpegPixel) < 250 || qGreen(jpegPixel) < 250
            || qBlue(jpegPixel) < 250) {
            std::cerr << "FAIL: saveImage JPEG fundo nao e branco: "
                      << qRed(jpegPixel) << "," << qGreen(jpegPixel) << ","
                      << qBlue(jpegPixel) << std::endl;
            return 1;
        }
    }

    // Teste de saveImage: qualidade WebP (apenas se o plugin estiver instalado).
    if (QImageWriter::supportedImageFormats().contains("webp")) {
        ProcessingOptions webpOpts;
        webpOpts.outputFormat = "webp";
        webpOpts.webpQuality = 50;
        QString tmpWebp = QStringLiteral("/tmp/test_postprocessing.webp");
        QString webpError;
        if (!ImagePostProcessor::saveImage(jpegSrc, tmpWebp, webpOpts,
                                           QString(), &webpError)) {
            std::cerr << "FAIL: saveImage WebP: " << webpError.toStdString()
                      << std::endl;
            return 1;
        }
    }

    // Teste de saveImage: PNG compressao definida.
    ProcessingOptions pngOpts;
    pngOpts.outputFormat = "png";
    pngOpts.pngCompression = 9;
    QString tmpPng = QStringLiteral("/tmp/test_postprocessing.png");
    QString pngError;
    if (!ImagePostProcessor::saveImage(jpegSrc, tmpPng, pngOpts,
                                       QString(), &pngError)) {
        std::cerr << "FAIL: saveImage PNG: " << pngError.toStdString()
                  << std::endl;
        return 1;
    }

    // Teste de removeByColor com despill: fundo magenta + centro magenta-claro,
    // despill reduz os canais dominantes (R e B) nos pixeis de borda.
    QImage despillSrc(5, 5, QImage::Format_RGBA8888);
    despillSrc.fill(qRgba(255, 0, 255, 255)); // magenta
    for (int y = 1; y <= 3; ++y) {
        for (int x = 1; x <= 3; ++x) {
            despillSrc.setPixel(x, y, qRgba(200, 50, 200, 255)); // magenta claro
        }
    }
    ProcessingOptions despillOpts;
    despillOpts.chromaKeyColor = QColor(255, 0, 255);
    despillOpts.chromaKeyTolerance = 100;
    despillOpts.chromaDespill = 1.0f;
    const QImage despillResult =
        ImagePostProcessor::removeByColor(despillSrc, despillOpts.chromaKeyColor,
                                          despillOpts.chromaKeyTolerance,
                                          false, true,
                                          despillOpts.chromaDespill,
                                          despillOpts.chromaFeather);
    // O pixel central deve ter R e B reduzidos pelo despill da cor-chave magenta.
    const QRgb centerPixel = despillResult.pixel(2, 2);
    if (qRed(centerPixel) >= 128 || qBlue(centerPixel) >= 128) {
        std::cerr << "FAIL: despill nao reduziu canais R/B: "
                  << qRed(centerPixel) << "," << qGreen(centerPixel) << ","
                  << qBlue(centerPixel) << std::endl;
        return 1;
    }

    std::cout << "PASS" << std::endl;
    return 0;
}
