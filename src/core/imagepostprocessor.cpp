#include "imagepostprocessor.h"

#include <QColorSpace>
#include <QFile>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QFileInfo>
#include <QSaveFile>
#include <QHash>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <vector>

// Todas as operacoes trabalham em Format_RGBA8888, cujo canal alfa
// fica no byte 3 de cada pixel. O acesso por scanline evita
// pixel()/setPixel() por pixel (lento em imagens grandes).

namespace {

quint16 readUint16(const QByteArray& data, qsizetype offset, bool littleEndian)
{
    if (offset < 0 || offset + 2 > data.size()) return 0;
    const quint16 a = static_cast<quint8>(data.at(offset));
    const quint16 b = static_cast<quint8>(data.at(offset + 1));
    return littleEndian ? static_cast<quint16>(a | (b << 8))
                         : static_cast<quint16>((a << 8) | b);
}

quint32 readUint32(const QByteArray& data, qsizetype offset, bool littleEndian)
{
    if (offset < 0 || offset + 4 > data.size()) return 0;
    const quint32 a = static_cast<quint8>(data.at(offset));
    const quint32 b = static_cast<quint8>(data.at(offset + 1));
    const quint32 c = static_cast<quint8>(data.at(offset + 2));
    const quint32 d = static_cast<quint8>(data.at(offset + 3));
    if (littleEndian) return a | (b << 8) | (c << 16) | (d << 24);
    return (a << 24) | (b << 16) | (c << 8) | d;
}

void writeUint16(QByteArray& data, qsizetype offset, quint16 value,
                 bool littleEndian)
{
    if (offset < 0 || offset + 2 > data.size()) return;
    const quint8 a = static_cast<quint8>(value & 0xff);
    const quint8 b = static_cast<quint8>((value >> 8) & 0xff);
    if (littleEndian) {
        data[offset] = static_cast<char>(a);
        data[offset + 1] = static_cast<char>(b);
    } else {
        data[offset] = static_cast<char>(b);
        data[offset + 1] = static_cast<char>(a);
    }
}

void writeUint32(QByteArray& data, qsizetype offset, quint32 value,
                 bool littleEndian)
{
    if (offset < 0 || offset + 4 > data.size()) return;
    const quint8 a = static_cast<quint8>(value & 0xff);
    const quint8 b = static_cast<quint8>((value >> 8) & 0xff);
    const quint8 c = static_cast<quint8>((value >> 16) & 0xff);
    const quint8 d = static_cast<quint8>((value >> 24) & 0xff);
    if (littleEndian) {
        data[offset] = static_cast<char>(a);
        data[offset + 1] = static_cast<char>(b);
        data[offset + 2] = static_cast<char>(c);
        data[offset + 3] = static_cast<char>(d);
    } else {
        data[offset] = static_cast<char>(d);
        data[offset + 1] = static_cast<char>(c);
        data[offset + 2] = static_cast<char>(b);
        data[offset + 3] = static_cast<char>(a);
    }
}

// Normaliza a orientacao EXIF para 1 (top-left). Os pixels ja foram
// corrigidos por QImageReader::setAutoTransform(true); manter a tag
// original faria o visualizador rodar a imagem uma segunda vez.
bool normalizeExifOrientation(QByteArray& tiff)
{
    if (tiff.size() < 8) return false;
    bool littleEndian = false;
    if (tiff.at(0) == 'I' && tiff.at(1) == 'I') {
        littleEndian = true;
    } else if (tiff.at(0) == 'M' && tiff.at(1) == 'M') {
        littleEndian = false;
    } else {
        return false;
    }
    if (readUint16(tiff, 2, littleEndian) != 42) return false;

    const qsizetype ifdOffset = static_cast<qsizetype>(
        readUint32(tiff, 4, littleEndian));
    if (ifdOffset <= 0 || ifdOffset + 2 > tiff.size()) return false;
    const quint16 entries = readUint16(tiff, ifdOffset, littleEndian);
    bool found = false;

    for (quint16 i = 0; i < entries; ++i) {
        const qsizetype entry = ifdOffset + 2 + static_cast<qsizetype>(i) * 12;
        if (entry + 12 > tiff.size()) break;
        if (readUint16(tiff, entry, littleEndian) != 0x0112) continue;

        const quint16 type = readUint16(tiff, entry + 2, littleEndian);
        const quint32 count = readUint32(tiff, entry + 4, littleEndian);
        if (type == 3 && count >= 1) {
            writeUint16(tiff, entry + 8, 1, littleEndian);
            found = true;
        } else if (type == 4 && count >= 1) {
            writeUint32(tiff, entry + 8, 1, littleEndian);
            found = true;
        }
    }
    return found;
}

void setExifDimensionValue(QByteArray& tiff, qsizetype entry,
                            bool littleEndian, int value)
{
    const quint16 type = readUint16(tiff, entry + 2, littleEndian);
    if (type == 3) {
        writeUint16(tiff, entry + 8, static_cast<quint16>(value), littleEndian);
    } else if (type == 4) {
        writeUint32(tiff, entry + 8, static_cast<quint32>(value), littleEndian);
    }
}

// Atualiza as dimensoes existentes para o tamanho final. As tags nao
// sao inventadas quando faltam: e preferivel nao ter PixelXDimension
// do que ter um valor obsoleto apos um crop/resize.
void updateExifDimensions(QByteArray& tiff, int width, int height)
{
    if (tiff.size() < 8 || width <= 0 || height <= 0) return;
    const bool littleEndian = tiff.at(0) == 'I' && tiff.at(1) == 'I';
    if (!littleEndian && !(tiff.at(0) == 'M' && tiff.at(1) == 'M')) return;
    if (readUint16(tiff, 2, littleEndian) != 42) return;

    const qsizetype ifdOffset = static_cast<qsizetype>(
        readUint32(tiff, 4, littleEndian));
    if (ifdOffset <= 0 || ifdOffset + 2 > tiff.size()) return;
    const quint16 entries = readUint16(tiff, ifdOffset, littleEndian);
    qsizetype exifIfdOffset = 0;

    for (quint16 i = 0; i < entries; ++i) {
        const qsizetype entry = ifdOffset + 2 + static_cast<qsizetype>(i) * 12;
        if (entry + 12 > tiff.size()) break;
        const quint16 tag = readUint16(tiff, entry, littleEndian);
        if (tag == 0x0100) {
            setExifDimensionValue(tiff, entry, littleEndian, width);
        } else if (tag == 0x0101) {
            setExifDimensionValue(tiff, entry, littleEndian, height);
        } else if (tag == 0x8769) {
            exifIfdOffset = static_cast<qsizetype>(
                readUint32(tiff, entry + 8, littleEndian));
        }
    }

    if (exifIfdOffset <= 0 || exifIfdOffset + 2 > tiff.size()) return;
    const quint16 exifEntries =
        readUint16(tiff, exifIfdOffset, littleEndian);
    for (quint16 i = 0; i < exifEntries; ++i) {
        const qsizetype entry =
            exifIfdOffset + 2 + static_cast<qsizetype>(i) * 12;
        if (entry + 12 > tiff.size()) break;
        const quint16 tag = readUint16(tiff, entry, littleEndian);
        if (tag == 0xa002) {
            setExifDimensionValue(tiff, entry, littleEndian, width);
        } else if (tag == 0xa003) {
            setExifDimensionValue(tiff, entry, littleEndian, height);
        }
    }
}

QByteArray extractJpegExif(const QByteArray& jpeg)
{
    if (jpeg.size() < 4
        || static_cast<quint8>(jpeg.at(0)) != 0xff
        || static_cast<quint8>(jpeg.at(1)) != 0xd8) {
        return {};
    }

    qsizetype pos = 2;
    while (pos + 4 <= jpeg.size()) {
        if (static_cast<quint8>(jpeg.at(pos)) != 0xff) {
            ++pos;
            continue;
        }
        const quint8 marker = static_cast<quint8>(jpeg.at(pos + 1));
        if (marker == 0xda || marker == 0xd9) break; // SOS / EOI
        if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd8)) {
            pos += 2;
            continue;
        }
        const quint16 length = readUint16(jpeg, pos + 2, false);
        if (length < 2) break;
        const qsizetype payloadStart = pos + 4;
        const qsizetype payloadLength = static_cast<qsizetype>(length) - 2;
        if (payloadStart + payloadLength > jpeg.size()) break;
        if (marker == 0xe1) {
            const QByteArray payload = jpeg.mid(payloadStart, payloadLength);
            if (payload.startsWith(QByteArray("Exif\0\0", 6)))
                return payload;
        }
        pos = payloadStart + payloadLength;
    }
    return {};
}

QByteArray extractPngExif(const QByteArray& png)
{
    static const QByteArray signature("\x89PNG\r\n\x1a\n", 8);
    if (!png.startsWith(signature)) return {};

    qsizetype pos = signature.size();
    while (pos + 12 <= png.size()) {
        const quint32 length = readUint32(png, pos, false);
        const QByteArray type = png.mid(pos + 4, 4);
        const qsizetype dataStart = pos + 8;
        const qsizetype dataEnd = dataStart + static_cast<qsizetype>(length);
        if (dataEnd + 4 > png.size()) break;
        if (type == QByteArrayLiteral("eXIf")) {
            return png.mid(dataStart, static_cast<qsizetype>(length));
        }
        if (type == QByteArrayLiteral("IDAT")) break;
        pos = dataEnd + 4;
    }
    return {};
}

quint32 pngChunkCrc(const QByteArray& typeAndData)
{
    quint32 crc = 0xffffffffu;
    for (const char byte : typeAndData) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xedb88320u : (crc >> 1);
        }
    }
    return crc ^ 0xffffffffu;
}

QByteArray makePngChunk(const QByteArray& type, const QByteArray& payload)
{
    QByteArray chunk;
    chunk.reserve(12 + payload.size());
    const quint32 length = static_cast<quint32>(payload.size());
    chunk.append(static_cast<char>((length >> 24) & 0xff));
    chunk.append(static_cast<char>((length >> 16) & 0xff));
    chunk.append(static_cast<char>((length >> 8) & 0xff));
    chunk.append(static_cast<char>(length & 0xff));
    chunk.append(type);
    chunk.append(payload);
    const quint32 crc = pngChunkCrc(type + payload);
    chunk.append(static_cast<char>((crc >> 24) & 0xff));
    chunk.append(static_cast<char>((crc >> 16) & 0xff));
    chunk.append(static_cast<char>((crc >> 8) & 0xff));
    chunk.append(static_cast<char>(crc & 0xff));
    return chunk;
}

bool writeBinaryFile(const QString& path, const QByteArray& data)
{
    // Escrita atomica: se a actualizacao do EXIF falhar, o JPEG/PNG
    // valido que o writer ja produziu permanece intacto.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

bool injectJpegExif(const QString& outputPath, const QByteArray& sourceTiff,
                    int width, int height)
{
    if (sourceTiff.isEmpty()) return false;
    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::ReadOnly)) return false;
    QByteArray output = outputFile.readAll();
    outputFile.close();
    if (output.size() < 4
        || static_cast<quint8>(output.at(0)) != 0xff
        || static_cast<quint8>(output.at(1)) != 0xd8) {
        return false;
    }
    if (!extractJpegExif(output).isEmpty()) return true;

    QByteArray tiff = sourceTiff;
    normalizeExifOrientation(tiff);
    updateExifDimensions(tiff, width, height);
    QByteArray payload("Exif\0\0", 6);
    payload.append(tiff);
    if (payload.size() + 2 > 65535) return false;

    QByteArray segment;
    segment.reserve(payload.size() + 4);
    segment.append(static_cast<char>(0xff));
    segment.append(static_cast<char>(0xe1));
    const quint16 length = static_cast<quint16>(payload.size() + 2);
    segment.append(static_cast<char>((length >> 8) & 0xff));
    segment.append(static_cast<char>(length & 0xff));
    segment.append(payload);
    output.insert(2, segment);
    return writeBinaryFile(outputPath, output);
}

bool injectPngExif(const QString& outputPath, const QByteArray& sourceTiff,
                   int width, int height)
{
    if (sourceTiff.isEmpty()) return false;
    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::ReadOnly)) return false;
    QByteArray output = outputFile.readAll();
    outputFile.close();
    static const QByteArray signature("\x89PNG\r\n\x1a\n", 8);
    if (!output.startsWith(signature)) return false;
    if (!extractPngExif(output).isEmpty()) return true;

    QByteArray tiff = sourceTiff;
    normalizeExifOrientation(tiff);
    updateExifDimensions(tiff, width, height);

    qsizetype insertAt = -1;
    qsizetype pos = signature.size();
    while (pos + 12 <= output.size()) {
        const quint32 length = readUint32(output, pos, false);
        const QByteArray type = output.mid(pos + 4, 4);
        const qsizetype dataEnd = pos + 8 + static_cast<qsizetype>(length);
        if (dataEnd + 4 > output.size()) break;
        if (type == QByteArrayLiteral("IHDR")) {
            insertAt = dataEnd + 4;
            break;
        }
        if (type == QByteArrayLiteral("IDAT")) break;
        pos = dataEnd + 4;
    }
    if (insertAt < 0) return false;

    output.insert(insertAt, makePngChunk(QByteArrayLiteral("eXIf"), tiff));
    return writeBinaryFile(outputPath, output);
}

} // namespace

QImage ImagePostProcessor::applyMaskEffects(const QImage& input,
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
    } else if (options.backgroundStyle == ProcessingOptions::BgSolidColor) {
        result = compositeBackground(result, options.backgroundColor);
    }

    return result;
}

QImage ImagePostProcessor::apply(const QImage& input,
                                 const ProcessingOptions& options,
                                 const QImage& originalImage)
{
    // Efeitos de máscara (limiar, refinamento, sticker, sombra, fundo)
    // aplicados primeiro; a geometria (recorte/rotação/redimensionamento)
    // fica para a exportação, para não ser aplicada duas vezes no preview.
    QImage result = applyMaskEffects(input, options, originalImage);
    result = applyGeometry(result, options);
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

    // 2. Dilatação morfológica separável (horizontal + vertical):
    // custo O(w·h·r) em vez de O(w·h·r²) do kernel circular 2-D.
    // Aproxima o disco com um quadrado de lado 2r+1 — visualmente
    // equivalente para espessuras de contorno.
    std::vector<uint8_t> dilated(mask.size(), 0);
    std::vector<uint8_t> tmp(mask.size(), 0);
    // Passagem horizontal.
    for (int y = 0; y < h; ++y) {
        const size_t rowStart = static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            bool any = false;
            for (int dx = -r; dx <= r && !any; ++dx) {
                const int nx = x + dx;
                if (nx >= 0 && nx < w && mask[rowStart + static_cast<size_t>(nx)] > 0)
                    any = true;
            }
            tmp[rowStart + static_cast<size_t>(x)] = any ? 255 : 0;
        }
    }
    // Passagem vertical.
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            bool any = false;
            for (int dy = -r; dy <= r && !any; ++dy) {
                const int ny = y + dy;
                if (ny >= 0 && ny < h
                    && tmp[static_cast<size_t>(ny) * static_cast<size_t>(w)
                           + static_cast<size_t>(x)] > 0)
                    any = true;
            }
            dilated[static_cast<size_t>(y) * static_cast<size_t>(w)
                    + static_cast<size_t>(x)] = any ? 255 : 0;
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
                                        bool preservePixelArt,
                                        float despill,
                                        int featherRadius)
{
    if (input.isNull() || !targetColor.isValid()) return input.copy();

    QImage result = input.convertToFormat(QImage::Format_RGBA8888);
    const int w = result.width();
    const int h = result.height();
    if (w <= 0 || h <= 0) return result;

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

    // A mascara vive num buffer continuo: permite suavizar (feather) e
    // decontaminar a cor (despill) sem tocar nos pixeis varias vezes.
    std::vector<uchar> alpha(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (int y = 0; y < h; ++y) {
        const uchar* row = result.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w)
                               + static_cast<size_t>(x);
            alpha[idx] = row[x * 4 + 3];
        }
    }

    if (!contiguous) {
        for (int y = 0; y < h; ++y) {
            const uchar* row = result.constScanLine(y);
            for (int x = 0; x < w; ++x) {
                const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w)
                                   + static_cast<size_t>(x);
                alpha[idx] = static_cast<uchar>(
                    calcAlpha(row[x * 4 + 0], row[x * 4 + 1], row[x * 4 + 2],
                              row[x * 4 + 3]));
            }
        }
    } else {
        // Inundacao por regiao: a pilha guarda o indice do pixel (4
        // bytes) em vez de um QPoint (8 bytes) - numa foto de 12 MP a
        // diferenca e de dezenas de MB de memoria, e o acesso as linhas
        // fica mais rapido.
        std::vector<uint8_t> visited(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
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

            const uchar* p = result.constScanLine(y) + x * 4;
            alpha[idx] = static_cast<uchar>(
                calcAlpha(p[0], p[1], p[2], p[3]));

            tryPush(x + 1, y);
            tryPush(x - 1, y);
            tryPush(x, y + 1);
            tryPush(x, y - 1);
        }
    }

    // Feather: desfoque em caixa separavel (horizontal + vertical) sobre
    // o canal alfa. Raio 0 (omissao) nao altera nada.
    const int radius = std::clamp(featherRadius, 0, 5);
    if (radius > 0) {
        std::vector<uint32_t> temp(alpha.size());
        const int window = radius * 2 + 1;
        // Horizontal.
        for (int y = 0; y < h; ++y) {
            const size_t rowStart = static_cast<size_t>(y) * static_cast<size_t>(w);
            uint32_t sum = 0;
            for (int x = -radius; x <= radius; ++x) {
                sum += alpha[rowStart + static_cast<size_t>(std::clamp(x, 0, w - 1))];
            }
            for (int x = 0; x < w; ++x) {
                temp[rowStart + static_cast<size_t>(x)] = sum / static_cast<uint32_t>(window);
                const int add = std::clamp(x + radius + 1, 0, w - 1);
                const int sub = std::clamp(x - radius, 0, w - 1);
                sum += alpha[rowStart + static_cast<size_t>(add)];
                sum -= alpha[rowStart + static_cast<size_t>(sub)];
            }
        }
        // Vertical.
        for (int x = 0; x < w; ++x) {
            uint32_t sum = 0;
            for (int y = -radius; y <= radius; ++y) {
                sum += temp[static_cast<size_t>(std::clamp(y, 0, h - 1))
                            * static_cast<size_t>(w) + static_cast<size_t>(x)];
            }
            for (int y = 0; y < h; ++y) {
                const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w)
                                   + static_cast<size_t>(x);
                alpha[idx] = static_cast<uchar>(sum / static_cast<uint32_t>(window));
                const int add = std::clamp(y + radius + 1, 0, h - 1);
                const int sub = std::clamp(y - radius, 0, h - 1);
                sum += temp[static_cast<size_t>(add) * static_cast<size_t>(w)
                            + static_cast<size_t>(x)];
                sum -= temp[static_cast<size_t>(sub) * static_cast<size_t>(w)
                            + static_cast<size_t>(x)];
            }
        }
    }

    const float despillAmount = std::clamp(despill, 0.0f, 1.0f);
    // Sem tolerancia, o despill usa uma janela de 50% da distancia RGB
    // maxima: e onde live a contaminacao de uma borda antialiasada. Com
    // tolerancia explicita, essa distancia define a janela.
    const float despillRadius = tolerance <= 0
        ? 0.50f * 441.67295593f : std::max(40.0f, maxDist);
    const float despillRadiusSq = despillRadius * despillRadius;

    // A cor-chave pode ter um canal dominante (verde, ciano, etc.) ou
    // dois (magenta). A supressao do canal dominante e mais estavel
    // que um unmix em RGB sem informacao de cobertura.
    int keyOrder[3] = {0, 1, 2};
    const int keyValues[3] = {tr, tg, tb};
    std::sort(keyOrder, keyOrder + 3, [keyValues](int a, int b) {
        return keyValues[a] > keyValues[b];
    });
    const bool singleDominant =
        keyValues[keyOrder[0]] - keyValues[keyOrder[1]] >= 40;
    const bool pairDominant = !singleDominant
        && keyValues[keyOrder[1]] - keyValues[keyOrder[2]] >= 40;

    for (int y = 0; y < h; ++y) {
        uchar* row = result.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w)
                               + static_cast<size_t>(x);
            const int a = alpha[idx];
            const size_t offset = static_cast<size_t>(x) * 4;

            if (despillAmount > 0.0f && (singleDominant || pairDominant)) {
                const float dr = static_cast<float>(row[offset + 0]) - tr;
                const float dg = static_cast<float>(row[offset + 1]) - tg;
                const float db = static_cast<float>(row[offset + 2]) - tb;
                const float distanceSq = dr * dr + dg * dg + db * db;
                if (distanceSq <= despillRadiusSq) {
                    if (singleDominant) {
                        const int dominant = keyOrder[0];
                        const int otherA = keyOrder[1];
                        const int otherB = keyOrder[2];
                        const int otherMax = std::max(
                            static_cast<int>(row[offset + otherA]),
                            static_cast<int>(row[offset + otherB]));
                        const int spill = static_cast<int>(row[offset + dominant])
                                       - otherMax;
                        if (spill > 0) {
                            row[offset + dominant] = static_cast<uchar>(
                                row[offset + dominant]
                                - std::lround(spill * despillAmount));
                        }
                    } else {
                        const int highA = keyOrder[0];
                        const int highB = keyOrder[1];
                        const int low = keyOrder[2];
                        const int common = std::min(
                            static_cast<int>(row[offset + highA]),
                            static_cast<int>(row[offset + highB]))
                            - static_cast<int>(row[offset + low]);
                        if (common > 0) {
                            const int removed = static_cast<int>(
                                std::lround(common * despillAmount));
                            row[offset + highA] = static_cast<uchar>(
                                row[offset + highA] - removed);
                            row[offset + highB] = static_cast<uchar>(
                                row[offset + highB] - removed);
                            // Para uma chave magenta, parte da cor tambem
                            // contaminou o canal baixo. Repor metade evita
                            // transformar um cinzento claro em cinzento
                            // demasiado escuro.
                            row[offset + low] = static_cast<uchar>(std::clamp(
                                static_cast<int>(row[offset + low]) + removed / 2,
                                0, 255));
                        }
                    }
                }
            }

            row[offset + 3] = static_cast<uchar>(a);
        }
    }

    return result;
}

QImage ImagePostProcessor::applyGeometry(const QImage& input,
                                        const ProcessingOptions& options)
{
    if (input.isNull()) return input;
    QImage result = input;

    // 1. Recorte em coordenadas normalizadas.
    if (options.hasCrop
        && options.cropRect.width() > 0.0
        && options.cropRect.height() > 0.0) {
        const QRectF norm = options.cropRect.normalized();
        const int left = std::clamp(
            static_cast<int>(std::floor(norm.x() * result.width())), 0,
            result.width() - 1);
        const int top = std::clamp(
            static_cast<int>(std::floor(norm.y() * result.height())), 0,
            result.height() - 1);
        const int right = std::clamp(
            static_cast<int>(std::ceil((norm.x() + norm.width()) * result.width())),
            left + 1, result.width());
        const int bottom = std::clamp(
            static_cast<int>(std::ceil((norm.y() + norm.height()) * result.height())),
            top + 1, result.height());
        const QRect rect(left, top, right - left, bottom - top);
        if (rect.isValid() && rect != result.rect()) result = result.copy(rect);
    }

    // 2. Rotacao (multiplos de 90 no sentido horario).
    int rotation = ((options.rotationDegrees % 360) + 360) % 360;
    if (rotation != 0 && rotation % 90 == 0) {
        result = result.transformed(QTransform().rotate(rotation),
                                    Qt::FastTransformation);
    }

    // 3. Redimensionamento.
    const int targetW = options.resizeWidth;
    const int targetH = options.resizeHeight;
    if (targetW > 0 || targetH > 0) {
        QSize target(targetW, targetH);
        if (targetW > 0 && targetH == 0) {
            target = QSize(targetW, std::max(1, static_cast<int>(std::lround(
                             static_cast<double>(result.height()) * targetW
                             / result.width()))));
        } else if (targetH > 0 && targetW == 0) {
            target = QSize(std::max(1, static_cast<int>(std::lround(
                             static_cast<double>(result.width()) * targetH
                             / result.height()))), targetH);
        }
        if (target.isValid() && target != result.size()) {
            result = result.scaled(target, Qt::IgnoreAspectRatio,
                                   Qt::SmoothTransformation);
        }
    }

    return result;
}

bool ImagePostProcessor::saveImage(const QImage& image,
                                   const QString& outputPath,
                                   const ProcessingOptions& options,
                                   const QString& sourcePath,
                                   QString* error)
{
    auto fail = [&](const QString& message) {
        if (error) *error = message;
        return false;
    };

    if (image.isNull()) return fail(QStringLiteral("empty image"));
    if (outputPath.isEmpty()) return fail(QStringLiteral("empty output path"));

    QString format = options.outputFormat.trimmed().toLower();
    if (format.isEmpty()) format = QFileInfo(outputPath).suffix().toLower();
    if (format == QLatin1String("jpg")
        || format == QLatin1String("jpeg")) {
        format = QStringLiteral("jpeg");
    } else if (format == QLatin1String("tif")) {
        format = QStringLiteral("tiff");
    }

    QImage out = image;

    // JPEG nao possui canal alfa. Compomos aqui (e nao apenas no
    // chamador) para que todos os fluxos -- GUI, lote e CLI -- evitem a
    // area transparente prehistoricamente preta.
    if (format == QLatin1String("jpeg") && out.hasAlphaChannel()) {
        const QColor background = options.backgroundColor.isValid()
            ? options.backgroundColor : QColor(Qt::white);
        out = compositeBackground(out, background);
    }

    QImageReader sourceReader;
    QHash<QString, QString> sourceText;
    QByteArray sourceBytes;
    QByteArray sourceTiff;
    if (options.preserveMetadata && !sourcePath.isEmpty()) {
        sourceReader.setFileName(sourcePath);
        // Os metadados textuais devem ser lidos antes da imagem: depois
        // de read(), alguns handlers PNG tentam reler o ficheiro a partir
        // do fim e emitem "Read Error" espúrio.
        const QStringList textKeys = sourceReader.textKeys();
        for (const QString& key : textKeys) {
            const QString value = sourceReader.text(key);
            if (!value.isEmpty()) sourceText.insert(key, value);
        }

        // So descodificamos a origem quando a imagem processada perdeu
        // o perfil de cor. Numa exportacao normal o pipeline ja o
        // transporta, evitando uma segunda decodificacao (importante em
        // lotes paralelos com fotos de 12 MP).
        if (!out.colorSpace().isValid()) {
            const QImage sourceImage = sourceReader.read();
            const QColorSpace sourceSpace = sourceImage.colorSpace();
            if (sourceSpace.isValid()) out.setColorSpace(sourceSpace);
        }

        QFile sourceFile(sourcePath);
        if (sourceFile.open(QIODevice::ReadOnly)) {
            sourceBytes = sourceFile.readAll();
            sourceFile.close();
        }
        // O EXIF e extraido do contentor da SOURCE, independentemente
        // do formato de saida. Assim JPEG->PNG e PNG->JPEG preservam
        // os metadados tecnicos.
        if (sourceBytes.size() >= 2
            && static_cast<quint8>(sourceBytes.at(0)) == 0xff
            && static_cast<quint8>(sourceBytes.at(1)) == 0xd8) {
            const QByteArray payload = extractJpegExif(sourceBytes);
            if (payload.startsWith(QByteArray("Exif\0\0", 6)))
                sourceTiff = payload.mid(6);
        } else {
            sourceTiff = extractPngExif(sourceBytes);
        }
    }

    // JPEG e WebP nao transportam um perfil ICCarbitrario no writer do
    // Qt. Convertemos para sRGB para que as cores nao dependam do visor.
    // O PNG mantem o perfil ICC da imagem processada.
    if ((format == QLatin1String("jpeg")
         || format == QLatin1String("webp"))
        && out.colorSpace().isValid()
        && out.colorSpace() != QColorSpace::SRgb) {
        out = out.convertedToColorSpace(QColorSpace::SRgb);
    }

    // Escrita atomica: QSaveFile cria um temporario na mesma diretoria
    // e o commit() faz rename atomico para outputPath. Um crash a meio
    // nao deixa o ficheiro de saida truncado/corrompido.
    QSaveFile tmpFile(outputPath);
    if (!tmpFile.open(QIODevice::WriteOnly))
        return fail(QStringLiteral("could not open file for writing: ") + outputPath);

    QImageWriter writer(&tmpFile, format.toUtf8());
    if (format == QLatin1String("png")) {
        if (options.pngCompression >= 0) {
            writer.setCompression(std::clamp(options.pngCompression, 0, 9));
        }
    } else if (format == QLatin1String("jpeg")) {
        writer.setQuality(std::clamp(options.jpegQuality, 0, 100));
    } else if (format == QLatin1String("webp")) {
        writer.setQuality(std::clamp(options.webpQuality, 0, 100));
    }

    // Metadados textuais (descricao, autor, copyright, etc.) sao
    // copiados quando o writer suporta o tipo. O EXIF binario e tratado
    // abaixo, depois da escrita, para JPEG e PNG.
    if (options.preserveMetadata && !sourceText.isEmpty()) {
        for (auto it = sourceText.constBegin();
             it != sourceText.constEnd(); ++it) {
            writer.setText(it.key(), it.value());
        }
    }

    if (!writer.write(out)) {
        tmpFile.cancelWriting();
        return fail(writer.errorString());
    }
    if (!tmpFile.commit())
        return fail(QStringLiteral("could not finalize file: ") + tmpFile.errorString());

    // EXIF binario e injetado no ficheiro final (ja commited).
    if (options.preserveMetadata && !sourceTiff.isEmpty()) {
        bool metadataWritten = true;
        if (format == QLatin1String("jpeg")) {
            metadataWritten = injectJpegExif(
                outputPath, sourceTiff, out.width(), out.height());
        } else if (format == QLatin1String("png")) {
            metadataWritten = injectPngExif(
                outputPath, sourceTiff, out.width(), out.height());
        }
        if (!metadataWritten) {
            // O ficheiro de imagem existe e e valido; a falha de
            // metadados e apenas um aviso, nao um erro de escrita.
            if (error) *error = QStringLiteral(
                "image written, but EXIF metadata could not be preserved");
            return true;
        }
    }
    return true;
}
