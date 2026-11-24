#ifndef PROCESSINGOPTIONS_H
#define PROCESSINGOPTIONS_H

#include <QColor>
#include <QRectF>
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

    // ------------------------------------------------------------------
    // Geometria de saída (crop / rotação / redimensionamento).
    // Aplicada por ImagePostProcessor::apply() no fim do pipeline, para
    // que os efeitos (sombra, contorno, fundo) já estejam compostos na
    // imagem recortada.
    // ------------------------------------------------------------------
    // Recorte opcional, em coordenadas normalizadas (0..1) sobre a
    // imagem já processada. Só é aplicado quando hasCrop == true.
    bool hasCrop = false;
    QRectF cropRect;

    // Rotação aplicada em graus, no sentido horário. Aceita 0, 90, 180
    // e 270 (outros valores são ignorados).
    int rotationDegrees = 0;

    // Tamanho de saída em pixels. Comportamento:
    //   * ambos 0            -> não redimensiona (tamanho da imagem)
    //   * largura > 0        -> escala mantendo a proporção
    //   * altura  > 0        -> escala mantendo a proporção
    //   * ambos > 0          -> tamanho exato (pode distorcer)
    int resizeWidth = 0;
    int resizeHeight = 0;

    // ------------------------------------------------------------------
    // Chroma Key / remoção por cor.
    // ------------------------------------------------------------------
    // Quando true, o pipeline usa remoção por cor em vez da inferência
    // ONNX. É usado pelo CLI e pode ser usado por integrações futuras.
    bool useColorRemoval = false;
    QColor chromaKeyColor = QColor(255, 0, 255);
    int chromaKeyTolerance = 0;
    bool chromaKeyContiguous = false;
    bool chromaKeyPreservePixelArt = true;

    // 0..1: decontaminação da cor-chave nas bordas semi-transparentes
    // (evita o "halo" da cor de fundo em pixéis de borda).
    float chromaDespill = 0.0f;
    // 0..5: raio (em pixels) de suavização da máscara gerada por cor.
    int chromaFeather = 0;

    // Preservar, tanto quanto possível, metadados e perfil de cor da
    // imagem de origem durante a exportação.
    bool preserveMetadata = true;
};

#endif
