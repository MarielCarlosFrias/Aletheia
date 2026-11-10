#ifndef PROCESSINGOPTIONS_H
#define PROCESSINGOPTIONS_H

#include <QColor>
#include <QString>

struct ProcessingOptions {
    QString modelPath;
    bool cleanNoise = false;
    float alphaThreshold = 30.0f;
    bool refineEdges = false;
    float edgeRefinementIntensity = 0.5f;
    QColor backgroundColor;
    QString outputFormat; // "png", "jpg", "webp" - formato de saida
    // Qualidade/compressão de exportação
    int pngCompression = -1; // -1 = padrão do Qt, 0-9 (0 = sem compressão, 9 = máxima)
    int jpegQuality = 90;    // 0-100
    int webpQuality = 90;    // 0-100
    int maxConcurrentImages = 1;

    enum BackgroundStyle {
        BgTransparent,
        BgSolidColor,
        BgBlurredOriginal,
        BgCustomImage
    };
    BackgroundStyle backgroundStyle = BgTransparent;
    QString backgroundImagePath;
    int blurRadius = 15;

    bool stickerOutline = false;
    int stickerWidth = 6;
    QColor stickerColor = Qt::white;

    bool dropShadow = false;
    int shadowBlur = 10;
    int shadowOffsetX = 4;
    int shadowOffsetY = 6;
    float shadowOpacity = 0.5f;
};

#endif
