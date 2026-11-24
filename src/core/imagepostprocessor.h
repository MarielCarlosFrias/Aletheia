#ifndef IMAGEPOSTPROCESSOR_H
#define IMAGEPOSTPROCESSOR_H

#include <QImage>
#include <QString>
#include "processingoptions.h"

class ImagePostProcessor {
public:
    static QImage apply(const QImage& input,
                        const ProcessingOptions& options,
                        const QImage& originalImage = QImage());

    // Geometria de saída (recorte/rotação/redimensionamento) definida em
    // ProcessingOptions. Chamada automaticamente no fim de apply() e
    // exposta para reutilização (preview, testes, CLI).
    static QImage applyGeometry(const QImage& input,
                                const ProcessingOptions& options);

    // Grava a imagem respeitando compressão/qualidade de
    // ProcessingOptions e preservando metadados do ficheiro de origem
    // quando possível:
    //   * JPEG -> APP1/EXIF copiado do original, com o tag de orientação
    //     normalizado para 1 (os pixéis já foram corrigidos no load);
    //   * PNG  -> perfil de cor ICC (iCCP) via QImage::setColorSpace;
    //   * EXIF de origem pode ser passado em sourcePath (vazio = sem
    //     metadados a preservar).
    // Devolve false e preenche error em caso de falha.
    static bool saveImage(const QImage& image,
                          const QString& outputPath,
                          const ProcessingOptions& options,
                          const QString& sourcePath = QString(),
                          QString* error = nullptr);
    static QImage applyAlphaThreshold(const QImage& input,
                                      float threshold);
    static QImage applyEdgeRefinement(const QImage& input,
                                      float intensity);
    static QImage compositeBackground(const QImage& input,
                                     const QColor& backgroundColor);

    // Auto-recorte: remove margens transparentes com padding percentual opcional.
    static QRect contentBounds(const QImage& img, int alphaThreshold = 5);
    static QImage autoCrop(const QImage& img, int paddingPercent = 0);

    // Efeitos visuais: contorno de adesivo (sticker) e sombra projetada (drop shadow).
    static QImage applyStickerOutline(const QImage& input, int width,
                                      const QColor& color = Qt::white);
    static QImage applyDropShadow(const QImage& input, int offsetX = 4,
                                  int offsetY = 6, int blurRadius = 10,
                                  float opacity = 0.5f);

    // Desfoque rápido e composições avançadas de fundo.
    static QImage blurImage(const QImage& input, int radius);
    static QImage compositeBlurredBackground(const QImage& foreground,
                                             const QImage& original,
                                             int blurRadius = 15);
    static QImage compositeBackgroundImage(const QImage& foreground,
                                           const QImage& backgroundImage);

    // Remoção por cor / Chroma Key (ideal para Pixel Art, Sprites e fundos sólidos).
    static QColor detectCornerColor(const QImage& img);
    static QImage removeByColor(const QImage& input,
                                const QColor& targetColor,
                                int tolerance = 0,
                                bool contiguous = false,
                                bool preservePixelArt = true,
                                float despill = 0.0f,
                                int featherRadius = 0);

    // Carrega uma imagem respeitando a orientação EXIF (fotos de telemóvel).
    static QImage loadImage(const QString& path);
    // Função centralizada para carregamento com correção EXIF.
    static QImage loadImageExif(const QString& path);

    // Verifica se o formato do ficheiro e suportado pelo Qt instalado.
    // Devolve true se suportado; se false, hint() recebe uma mensagem
    // util (ex.: "instale qt6-image-formats-plugins para HEIC/AVIF/WebP").
    static bool isFormatSupported(const QString& path, QString* hint = nullptr);
    // Extensoes que exigem plugins externos (nao incluidos no Qt base).
    static QStringList pluginDependentExtensions();

    // Visualização da máscara alfa para inspeção rápida (escala de cinza sobre fundo vermelho/verde).
    static QImage visualizeMask(const QImage& rgbaImage, bool overlay = true);
};

#endif