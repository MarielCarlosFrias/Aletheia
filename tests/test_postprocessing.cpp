#include <QImage>
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

    std::cout << "PASS" << std::endl;
    return 0;
}
