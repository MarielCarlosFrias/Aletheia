#include "imagepostprocessor.h"

#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <vector>

// Todas as operacoes trabalham em Format_RGBA8888, cujo canal alfa
// fica no byte 3 de cada pixel. O acesso por scanline evita
// pixel()/setPixel() por pixel (lento em imagens grandes).

QImage ImagePostProcessor::apply(const QImage& input,
                                 const ProcessingOptions& options,
                                 const QImage& originalImage)
{
    QImage result = input;
    if (options.cleanNoise) {
        result = applyAlphaThreshold(result, options.alphaThreshold);
    }
    if (options.refineEdges) {
        result = applyEdgeRefinement(result, options.edgeRefinementIntensity);
    }
    if (options.stickerOutline) {
        result = applyStickerOutline(result, options.stickerWidth, options.stickerColor);
    }
    if (options.dropShadow) {
        result = applyDropShadow(result, options.shadowOffsetX, options.shadowOffsetY,
                                 options.shadowBlur, options.shadowOpacity);
    }

    if (options.backgroundStyle == ProcessingOptions::BgBlurredOriginal && !originalImage.isNull()) {
        result = compositeBlurredBackground(result, originalImage, options.blurRadius);
    } else if (options.backgroundStyle == ProcessingOptions::BgCustomImage && !options.backgroundImagePath.isEmpty()) {
        QImage bgImg = loadImage(options.backgroundImagePath);
        if (!bgImg.isNull()) {
            result = compositeBackgroundImage(result, bgImg);
        }
    } else if (options.backgroundStyle == ProcessingOptions::BgSolidColor || options.backgroundColor.isValid()) {
        result = compositeBackground(result, options.backgroundColor);
    }

    return result;
}

QImage ImagePostProcessor::applyAlphaThreshold(
    const QImage& input, float threshold)
{
    if (input.isNull() || threshold <= 0.0f) return input.copy();
    QImage result = input.convertToFormat(QImage::Format_RGBA8888);
    const int cutoff = std::clamp(static_cast<int>(threshold), 0, 255);
    for (int y = 0; y < result.height(); ++y) {
        uchar* row = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            if (row[x * 4 + 3] < cutoff) row[x * 4 + 3] = 0;
        }
    }
    return result;
}

QImage ImagePostProcessor::applyEdgeRefinement(
    const QImage& input, float intensity)
{
    if (input.isNull() || intensity <= 0.0f) return input.copy();
    QImage result = input.convertToFormat(QImage::Format_RGBA8888);
    QImage refined = result;
    const int width = result.width();
    const int height = result.height();
    for (int y = 0; y < height; ++y) {
        const uchar* r0 = result.constScanLine(qMax(0, y - 1));
        const uchar* r1 = result.constScanLine(y);
        const uchar* r2 = result.constScanLine(qMin(height - 1, y + 1));
        uchar* dst = refined.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const int left  = qMax(0, x - 1);
            const int right = qMin(width - 1, x + 1);
            const int sum =
                r0[left * 4 + 3] + r0[x * 4 + 3] + r0[right * 4 + 3] +
                r1[left * 4 + 3] + r1[x * 4 + 3] + r1[right * 4 + 3] +
                r2[left * 4 + 3] + r2[x * 4 + 3] + r2[right * 4 + 3];
            const float avg = sum / 9.0f;
            const float alpha = r1[x * 4 + 3];
            dst[x * 4 + 3] = static_cast<uchar>(std::clamp(
                alpha + (avg - alpha) * intensity, 0.0f, 255.0f));
        }
    }
    return refined;
}

QImage ImagePostProcessor::compositeBackground(
    const QImage& input, const QColor& backgroundColor)
{
    if (input.isNull()) return QImage();
    QImage result(input.size(), QImage::Format_RGBA8888);
    result.fill(backgroundColor);
    QPainter painter(&result);
    painter.drawImage(0, 0, input);
    painter.end();
    return result;
}

QImage ImagePostProcessor::loadImage(const QString& path)
{
    return loadImageExif(path);
}

QImage ImagePostProcessor::loadImageExif(const QString& path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    return reader.read();
}

QStringList ImagePostProcessor::pluginDependentExtensions()
{
    // Formatos que o Qt base nao le sem plugins de image formats.
    return { QStringLiteral("heic"), QStringLiteral("heif"),
             QStringLiteral("avif"), QStringLiteral("webp"),
             QStringLiteral("jp2"),  QStringLiteral("jxl") };
}

bool ImagePostProcessor::isFormatSupported(const QString& path, QString* hint)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    const QList<QByteArray> supported = QImageReader::supportedImageFormats();
    const QByteArray fmt = suffix.toLatin1();
    if (supported.contains(fmt)) {
        if (hint) hint->clear();
        return true;
    }
    // O Qt deteta o formato pelo conteudo, nao so pela extensao: se o
    // conteudo for suportado, mesmo com extensao rara, da true.
    QImageReader probe(path);
    if (probe.canRead()) {
        if (hint) hint->clear();
        return true;
    }
    if (hint) {
        if (pluginDependentExtensions().contains(suffix)) {
            *hint = QStringLiteral(
                "formato .%1 requer plugin externo").arg(suffix);
        } else {
            *hint = QStringLiteral("formato .%1 nao suportado").arg(suffix);
        }
    }
    return false;
}

QImage ImagePostProcessor::visualizeMask(const QImage& rgbaImage, bool overlay)
{
    if (rgbaImage.isNull()) return QImage();
    QImage img = rgbaImage.convertToFormat(QImage::Format_RGBA8888);
    const int w = img.width();
    const int h = img.height();
    QImage out(w, h, QImage::Format_RGB888);
    QPainter p(&out);
    if (overlay) {
        // Fundo vermelho; máscara verde sobreposta para destacar contornos
        p.fillRect(out.rect(), QColor(255, 0, 0));
    } else {
        p.fillRect(out.rect(), Qt::black);
    }
    p.end();

    // Copiar canal alfa como brilho.
    // Acesso por bytes: a origem e RGBA8888 (4 bytes/pixel) e o destino
    // e RGB888 (3 bytes/pixel). Escrever com QRgb (4 bytes) num buffer
    // de 3 bytes corrompia a heap e chegava a abortar a aplicacao.
    for (int y = 0; y < h; ++y) {
        const quint8* srcRow = img.constScanLine(y);
        quint8* dstRow = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const int a = srcRow[x * 4 + 3]; // canal alfa
            quint8* px = dstRow + x * 3;
            if (overlay) {
                // misturar vermelho com verde proporcional ao alpha
                px[0] = static_cast<quint8>(255 - a); // R
                px[1] = static_cast<quint8>(a);       // G
                px[2] = 0;                            // B
            } else {
                px[0] = px[1] = px[2] = static_cast<quint8>(a);
            }
        }
    }
    return out;
}

QRect ImagePostProcessor::contentBounds(const QImage& img, int alphaThreshold)
{
    if (img.isNull()) return QRect();
    const QImage rgba = img.format() == QImage::Format_RGBA8888
        ? img
        : img.convertToFormat(QImage::Format_RGBA8888);

    const int width = rgba.width();
    const int height = rgba.height();
    const int cutoff = std::clamp(alphaThreshold, 1, 255);

    int minX = width;
    int maxX = -1;
    int minY = height;
    int maxY = -1;

    for (int y = 0; y < height; ++y) {
        const uchar* row = rgba.constScanLine(y);
        for (int x = 0; x < width; ++x) {
            if (row[x * 4 + 3] >= cutoff) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return QRect(0, 0, width, height);
    }

    return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

QImage ImagePostProcessor::autoCrop(const QImage& img, int paddingPercent)
{
    if (img.isNull()) return img;
    QRect bounds = contentBounds(img);
    if (bounds.isEmpty() || bounds == img.rect()) return img;

    if (paddingPercent > 0) {
        const int padX = std::max(1, bounds.width() * paddingPercent / 100);
        const int padY = std::max(1, bounds.height() * paddingPercent / 100);
        bounds.adjust(-padX, -padY, padX, padY);
        bounds = bounds.intersected(img.rect());
    }

    return img.copy(bounds);
}

QImage ImagePostProcessor::blurImage(const QImage& input, int radius)
{
    if (input.isNull() || radius <= 0) return input.copy();
    const int r = std::clamp(radius, 1, 80);
    QImage src = input.format() == QImage::Format_RGBA8888
        ? input
        : input.convertToFormat(QImage::Format_RGBA8888);

    const int width = src.width();
    const int height = src.height();
    if (width == 0 || height == 0) return src;

    QImage intermediate(width, height, QImage::Format_RGBA8888);
    QImage dst(width, height, QImage::Format_RGBA8888);

    // Passagem 1: Horizontal deslizante
    for (int y = 0; y < height; ++y) {
        const uchar* inRow = src.constScanLine(y);
        uchar* outRow = intermediate.scanLine(y);

        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
        for (int i = -r; i <= r; ++i) {
            const int px = std::clamp(i, 0, width - 1);
            sumR += inRow[px * 4];
            sumG += inRow[px * 4 + 1];
            sumB += inRow[px * 4 + 2];
            sumA += inRow[px * 4 + 3];
        }

        const int div = 2 * r + 1;
        for (int x = 0; x < width; ++x) {
            outRow[x * 4]     = static_cast<uchar>(sumR / div);
            outRow[x * 4 + 1] = static_cast<uchar>(sumG / div);
            outRow[x * 4 + 2] = static_cast<uchar>(sumB / div);
            outRow[x * 4 + 3] = static_cast<uchar>(sumA / div);

            const int nextX = std::clamp(x + r + 1, 0, width - 1);
            const int prevX = std::clamp(x - r, 0, width - 1);

            sumR += inRow[nextX * 4]     - inRow[prevX * 4];
            sumG += inRow[nextX * 4 + 1] - inRow[prevX * 4 + 1];
            sumB += inRow[nextX * 4 + 2] - inRow[prevX * 4 + 2];
            sumA += inRow[nextX * 4 + 3] - inRow[prevX * 4 + 3];
        }
    }

    // Passagem 2: Vertical deslizante
    const int div = 2 * r + 1;
    for (int x = 0; x < width; ++x) {
        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
        for (int i = -r; i <= r; ++i) {
            const int py = std::clamp(i, 0, height - 1);
            const uchar* row = intermediate.constScanLine(py);
            sumR += row[x * 4];
            sumG += row[x * 4 + 1];
            sumB += row[x * 4 + 2];
            sumA += row[x * 4 + 3];
        }

        for (int y = 0; y < height; ++y) {
            uchar* outRow = dst.scanLine(y);
            outRow[x * 4]     = static_cast<uchar>(sumR / div);
            outRow[x * 4 + 1] = static_cast<uchar>(sumG / div);
            outRow[x * 4 + 2] = static_cast<uchar>(sumB / div);
            outRow[x * 4 + 3] = static_cast<uchar>(sumA / div);

            const int nextY = std::clamp(y + r + 1, 0, height - 1);
            const int prevY = std::clamp(y - r, 0, height - 1);

            const uchar* nextRow = intermediate.constScanLine(nextY);
            const uchar* prevRow = intermediate.constScanLine(prevY);

            sumR += nextRow[x * 4]     - prevRow[x * 4];
            sumG += nextRow[x * 4 + 1] - prevRow[x * 4 + 1];
            sumB += nextRow[x * 4 + 2] - prevRow[x * 4 + 2];
            sumA += nextRow[x * 4 + 3] - prevRow[x * 4 + 3];
        }
    }

    return dst;
}

QImage ImagePostProcessor::applyStickerOutline(const QImage& input, int width,
                                              const QColor& color)
{
    if (input.isNull() || width <= 0) return input.copy();
    const int r = std::clamp(width, 1, 50);

    const QImage rgba = input.format() == QImage::Format_RGBA8888
        ? input
        : input.convertToFormat(QImage::Format_RGBA8888);

    const int w = rgba.width();
    const int h = rgba.height();

    // 1. Extrair mascara binaria de opacidade
    std::vector<uint8_t> mask(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    for (int y = 0; y < h; ++y) {
        const uchar* row = rgba.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            if (row[x * 4 + 3] > 15) {
                mask[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = 255;
            }
        }
    }

    // 2. Dilatação morfológica com kernel circular
    std::vector<uint8_t> dilated = mask;
    std::vector<std::pair<int, int>> offsets;
    const int r2 = r * r;
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy <= r2) {
                offsets.emplace_back(dx, dy);
            }
        }
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (mask[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] == 255) {
                for (const auto& [dx, dy] : offsets) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        dilated[static_cast<size_t>(ny) * static_cast<size_t>(w) + static_cast<size_t>(nx)] = 255;
                    }
                }
            }
        }
    }

    // 3. Montar camada de contorno
    QImage result(w, h, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);

    const uchar red = static_cast<uchar>(color.red());
    const uchar green = static_cast<uchar>(color.green());
    const uchar blue = static_cast<uchar>(color.blue());
    const uchar alpha = static_cast<uchar>(color.alpha());

    for (int y = 0; y < h; ++y) {
        uchar* row = result.scanLine(y);
        for (int x = 0; x < w; ++x) {
            if (dilated[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] > 0) {
                row[x * 4]     = red;
                row[x * 4 + 1] = green;
                row[x * 4 + 2] = blue;
                row[x * 4 + 3] = alpha;
            }
        }
    }

    // 4. Desenhar a imagem recortada por cima
    QPainter p(&result);
    p.drawImage(0, 0, rgba);
    p.end();

    return result;
}

QImage ImagePostProcessor::applyDropShadow(const QImage& input, int offsetX,
                                          int offsetY, int blurRadius,
                                          float opacity)
{
    if (input.isNull()) return input.copy();
    const QImage rgba = input.format() == QImage::Format_RGBA8888
        ? input
        : input.convertToFormat(QImage::Format_RGBA8888);

    const int w = rgba.width();
    const int h = rgba.height();
    const float op = std::clamp(opacity, 0.0f, 1.0f);

    // Cria a camada de silhueta preta com alfa proporcional
    QImage shadow(w, h, QImage::Format_RGBA8888);
    shadow.fill(Qt::transparent);

    for (int y = 0; y < h; ++y) {
        const uchar* srcRow = rgba.constScanLine(y);
        uchar* shadowRow = shadow.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const uchar srcA = srcRow[x * 4 + 3];
            if (srcA > 0) {
                shadowRow[x * 4]     = 0;
                shadowRow[x * 4 + 1] = 0;
                shadowRow[x * 4 + 2] = 0;
                shadowRow[x * 4 + 3] = static_cast<uchar>(srcA * op);
            }
        }
    }

    if (blurRadius > 0) {
        shadow = blurImage(shadow, blurRadius);
    }

    QImage result(w, h, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);

    QPainter p(&result);
    p.drawImage(offsetX, offsetY, shadow);
    p.drawImage(0, 0, rgba);
    p.end();

    return result;
}

QImage ImagePostProcessor::compositeBlurredBackground(const QImage& foreground,
                                                      const QImage& original,
                                                      int blurRadius)
{
    if (foreground.isNull()) return original;
    const QImage base = original.isNull() ? foreground : original;

    QImage bg = base;
    if (bg.size() != foreground.size()) {
        bg = bg.scaled(foreground.size(), Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation);
        const int cropX = (bg.width() - foreground.width()) / 2;
        const int cropY = (bg.height() - foreground.height()) / 2;
        bg = bg.copy(cropX, cropY, foreground.width(), foreground.height());
    }

    bg = blurImage(bg, blurRadius);
    QImage result = bg.convertToFormat(QImage::Format_RGBA8888);

    QPainter p(&result);
    p.drawImage(0, 0, foreground);
    p.end();

    return result;
}

QImage ImagePostProcessor::compositeBackgroundImage(const QImage& foreground,
                                                    const QImage& backgroundImage)
{
    if (foreground.isNull()) return backgroundImage;
    if (backgroundImage.isNull()) return foreground;

    QImage bg = backgroundImage.scaled(foreground.size(),
                                       Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
    const int cropX = (bg.width() - foreground.width()) / 2;
    const int cropY = (bg.height() - foreground.height()) / 2;
    bg = bg.copy(cropX, cropY, foreground.width(), foreground.height());

    QImage result = bg.convertToFormat(QImage::Format_RGBA8888);
    QPainter p(&result);
    p.drawImage(0, 0, foreground);
    p.end();

    return result;
}

QColor ImagePostProcessor::detectCornerColor(const QImage& img)
{
    if (img.isNull()) return Qt::magenta;
    const QImage src = img.convertToFormat(QImage::Format_RGBA8888);
    const int w = src.width();
    const int h = src.height();

    // Amostra uma faixa de borda (borda de ~3px ou 2% da dimensao,
    // no minimo 1px) em vez de apenas 8 pontos: muito mais robusta
    // contra cantos com alpha, anti-aliasing ou sprites que tocam a borda.
    const int margin = std::max(1, std::min(w, h) * 2 / 100);

    // Conta frequencia de cores opacas (alpha >= 128) na faixa de borda.
    // Cores sao quantizadas em passos de 8 (com arredondamento) para
    // agrupar tons quase identicos sem perder a cor exata nos extremos.
    auto quantize = [](int v) -> quint32 {
        return static_cast<quint32>(std::min(255, (v + 4) / 8 * 8));
    };
    std::map<quint32, int> freq;
    auto samplePixel = [&](int x, int y) {
        if (x < 0 || x >= w || y < 0 || y >= h) return;
        const QRgb px = src.pixel(x, y);
        if (qAlpha(px) < 128) return; // ignora transparente
        const quint32 key =
            (quantize(qRed(px))   << 16) |
            (quantize(qGreen(px)) << 8)  |
             quantize(qBlue(px));
        freq[key]++;
    };

    // Borda superior e inferior
    for (int x = 0; x < w; ++x) {
        for (int d = 0; d < margin; ++d) {
            samplePixel(x, d);
            samplePixel(x, h - 1 - d);
        }
    }
    // Borda esquerda e direita (sem repetir os cantos ja amostrados)
    for (int y = margin; y < h - margin; ++y) {
        for (int d = 0; d < margin; ++d) {
            samplePixel(d, y);
            samplePixel(w - 1 - d, y);
        }
    }

    if (!freq.empty()) {
        // Encontra a cor quantizada mais frequente
        quint32 bestKey = 0;
        int bestCount = 0;
        for (const auto& [key, count] : freq) {
            if (count > bestCount) {
                bestCount = count;
                bestKey = key;
            }
        }
        return QColor((bestKey >> 16) & 0xFF,
                      (bestKey >> 8) & 0xFF,
                      bestKey & 0xFF);
    }

    // Fallback: se toda a borda e transparente, procura a cor opaca
    // mais comum numa amostra da imagem inteira (grade de ~64x64).
    std::map<quint32, int> globalFreq;
    const int stepX = std::max(1, w / 64);
    const int stepY = std::max(1, h / 64);
    for (int y = 0; y < h; y += stepY) {
        for (int x = 0; x < w; x += stepX) {
            const QRgb px = src.pixel(x, y);
            if (qAlpha(px) < 128) continue;
            const quint32 key =
                (quantize(qRed(px))   << 16) |
                (quantize(qGreen(px)) << 8)  |
                 quantize(qBlue(px));
            globalFreq[key]++;
        }
    }
    if (!globalFreq.empty()) {
        quint32 bestKey = 0;
        int bestCount = 0;
        for (const auto& [key, count] : globalFreq) {
            if (count > bestCount) {
                bestCount = count;
                bestKey = key;
            }
        }
        return QColor((bestKey >> 16) & 0xFF,
                      (bestKey >> 8) & 0xFF,
                      bestKey & 0xFF);
    }

    // Nenhum pixel opaco: devolve magenta (fallback padrao chroma key)
    return Qt::magenta;
}

QImage ImagePostProcessor::removeByColor(const QImage& input,
                                        const QColor& targetColor,
                                        int tolerance,
                                        bool contiguous,
                                        bool preservePixelArt)
{
    if (input.isNull() || !targetColor.isValid()) return input.copy();

    QImage result = input.convertToFormat(QImage::Format_RGBA8888);
    const int w = result.width();
    const int h = result.height();

    const int tr = targetColor.red();
    const int tg = targetColor.green();
    const int tb = targetColor.blue();

    const float maxDist = (std::clamp(tolerance, 0, 100) / 100.0f) * 441.67295593f;
    const float maxDistSq = maxDist * maxDist;

    auto matchesColor = [&](int r, int g, int b) -> bool {
        const float dr = static_cast<float>(r - tr);
        const float dg = static_cast<float>(g - tg);
        const float db = static_cast<float>(b - tb);
        return (dr * dr + dg * dg + db * db) <= maxDistSq;
    };

    auto calcAlpha = [&](int r, int g, int b, int currentAlpha) -> int {
        if (preservePixelArt) {
            return matchesColor(r, g, b) ? 0 : currentAlpha;
        }
        const float dr = static_cast<float>(r - tr);
        const float dg = static_cast<float>(g - tg);
        const float db = static_cast<float>(b - tb);
        const float dist = std::sqrt(dr * dr + dg * dg + db * db);
        if (dist <= maxDist * 0.7f) return 0;
        if (dist >= maxDist) return currentAlpha;
        const float t = (dist - maxDist * 0.7f) / (maxDist * 0.3f);
        return static_cast<int>(currentAlpha * t);
    };

    if (!contiguous) {
        for (int y = 0; y < h; ++y) {
            uchar* row = result.scanLine(y);
            for (int x = 0; x < w; ++x) {
                const int r = row[x * 4 + 0];
                const int g = row[x * 4 + 1];
                const int b = row[x * 4 + 2];
                const int a = row[x * 4 + 3];
                row[x * 4 + 3] = static_cast<uchar>(calcAlpha(r, g, b, a));
            }
        }
    } else {
        // Inundacao por regiao: a pilha guarda o indice do pixel (4
        // bytes) em vez de um QPoint (8 bytes) - numa foto de 12 MP a
        // diferenca e dezenas de MB de memoria, e o acesso as linhas
        // fica mais rapido.
        std::vector<uint8_t> visited(static_cast<size_t>(w * h), 0);
        std::vector<uint32_t> stack;
        stack.reserve(static_cast<size_t>(w) * 4);

        auto tryPush = [&](int x, int y) {
            if (x < 0 || x >= w || y < 0 || y >= h) return;
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w)
                               + static_cast<size_t>(x);
            if (visited[idx]) return;
            const uchar* pixel = result.constScanLine(y) + x * 4;
            if (matchesColor(pixel[0], pixel[1], pixel[2])) {
                visited[idx] = 1;
                stack.push_back(static_cast<uint32_t>(idx));
            }
        };

        for (int x = 0; x < w; ++x) {
            tryPush(x, 0);
            tryPush(x, h - 1);
        }
        for (int y = 0; y < h; ++y) {
            tryPush(0, y);
            tryPush(w - 1, y);
        }

        while (!stack.empty()) {
            const uint32_t idx = stack.back();
            stack.pop_back();
            const int x = static_cast<int>(idx % static_cast<uint32_t>(w));
            const int y = static_cast<int>(idx / static_cast<uint32_t>(w));

            uchar* p = result.scanLine(y) + x * 4;
            p[3] = static_cast<uchar>(calcAlpha(p[0], p[1], p[2], p[3]));

            tryPush(x + 1, y);
            tryPush(x - 1, y);
            tryPush(x, y + 1);
            tryPush(x, y - 1);
        }
    }

    return result;
}