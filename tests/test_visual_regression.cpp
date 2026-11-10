#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QDebug>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cmath>
#include "backgroundremover.h"
#include "imagepostprocessor.h"
#include "processingoptions.h"

static double computeAverageAlpha(const QImage& img) {
    if (img.isNull()) return 0.0;
    QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
    qint64 sum = 0;
    int count = 0;
    for (int y = 0; y < rgba.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(rgba.constScanLine(y));
        for (int x = 0; x < rgba.width(); ++x) {
            sum += qAlpha(line[x]);
            ++count;
        }
    }
    if (count == 0) return 0.0;
    return static_cast<double>(sum) / (count * 255.0);
}

static QString perceptualHash(const QImage& img) {
    QImage small = img.convertToFormat(QImage::Format_Grayscale8);
    small = small.scaled(8, 8, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    int sum = 0;
    for (int y = 0; y < 8; ++y) {
        const uchar* line = small.constScanLine(y);
        for (int x = 0; x < 8; ++x) {
            sum += line[x];
        }
    }
    int avg = sum / 64;
    quint64 hash = 0;
    for (int y = 0; y < 8; ++y) {
        const uchar* line = small.constScanLine(y);
        for (int x = 0; x < 8; ++x) {
            hash <<= 1;
            if (line[x] > avg) hash |= 1;
        }
    }
    return QString("%1").arg(hash, 16, 16, QChar('0'));
}

static int hammingDistance(const QString& a, const QString& b) {
    bool ok1, ok2;
    quint64 ha = a.toULongLong(&ok1, 16);
    quint64 hb = b.toULongLong(&ok2, 16);
    if (!ok1 || !ok2) return INT_MAX;
    quint64 x = ha ^ hb;
    int cnt = 0;
    while (x) {
        cnt += x & 1;
        x >>= 1;
    }
    return cnt;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <modelo.onnx>" << std::endl;
        return 1;
    }
    QString modelPath = argv[1];
    if (!QFile::exists(modelPath)) {
        std::cerr << "FAIL: Modelo não encontrado: " << modelPath.toStdString() << std::endl;
        return 1;
    }

    double alphaTol = 0.05;
    int hashTol = 10;
    if (const char* env = std::getenv("VISUAL_REGRESSION_ALPHA_TOLERANCE")) {
        alphaTol = std::atof(env);
    }
    if (const char* env2 = std::getenv("VISUAL_REGRESSION_HASH_TOLERANCE")) {
        hashTol = std::atoi(env2);
    }

    QString baseDir = QCoreApplication::applicationDirPath();
    // Tentar localizar assets relativo ao executável ou ao source
    QString inputsDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../assets/visual_regression/inputs");
    QString refsDir   = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../assets/visual_regression/refs");
    if (!QDir(inputsDir).exists()) {
        // fallback para source tree
        inputsDir = QString("%1/../assets/visual_regression/inputs").arg(QCoreApplication::applicationDirPath());
        refsDir   = QString("%1/../assets/visual_regression/refs").arg(QCoreApplication::applicationDirPath());
    }
    // Último fallback: caminho absoluto do source em build
    if (!QDir(inputsDir).exists()) {
        inputsDir = "/home/theking1x/projetos/bg-remover/assets/visual_regression/inputs";
        refsDir   = "/home/theking1x/projetos/bg-remover/assets/visual_regression/refs";
    }

    QDir inDir(inputsDir);
    if (!inDir.exists()) {
        std::cerr << "FAIL: Diretório de entrada não encontrado: " << inputsDir.toStdString() << std::endl;
        return 1;
    }
    QDir::setCurrent(QCoreApplication::applicationDirPath());
    QDir().mkpath(refsDir);

    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.webp";
    QFileInfoList files = inDir.entryInfoList(filters, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        std::cerr << "FAIL: Nenhuma imagem de referência encontrada em " << inputsDir.toStdString() << std::endl;
        return 1;
    }

    BackgroundRemover remover(modelPath);
    ProcessingOptions opts;
    opts.alphaThreshold = 30.0f;
    opts.backgroundStyle = ProcessingOptions::BgTransparent;

    bool allOk = true;
    for (const QFileInfo& fi : files) {
        QString inputPath = fi.absoluteFilePath();
        QImage input(inputPath);
        if (input.isNull()) {
            std::cerr << "FAIL: Não foi possível carregar " << inputPath.toStdString() << std::endl;
            allOk = false;
            continue;
        }

        QImage removed = remover.removeBackground(input);
        QImage finalImg = ImagePostProcessor::apply(removed, opts, input);

        double avgAlpha = computeAverageAlpha(finalImg);
        QString pHash = perceptualHash(finalImg);

        QString baseName = fi.baseName();
        QString refPath = QDir(refsDir).filePath(baseName + ".ref");
        QFile refFile(refPath);
        double refAlpha = -1;
        QString refHash;
        bool refExists = refFile.exists();
        if (refExists) {
            if (!refFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                std::cerr << "WARN: Não foi possível ler ref " << refPath.toStdString() << std::endl;
            } else {
                QTextStream in(&refFile);
                while (!in.atEnd()) {
                    QString line = in.readLine().trimmed();
                    if (line.startsWith("avgAlpha=")) {
                        refAlpha = line.mid(QString("avgAlpha=").length()).toDouble();
                    } else if (line.startsWith("hash=")) {
                        refHash = line.mid(QString("hash=").length());
                    }
                }
                refFile.close();
            }
        }

        if (!refExists || refAlpha < 0) {
            // Criar baseline
            if (refFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&refFile);
                out << "avgAlpha=" << avgAlpha << "\n";
                out << "hash=" << pHash << "\n";
                refFile.close();
                std::cout << "BASELINE criado para " << baseName.toStdString()
                          << " alpha=" << avgAlpha << " hash=" << pHash.toStdString() << std::endl;
            } else {
                std::cerr << "FAIL: Não foi possível criar baseline em " << refPath.toStdString() << std::endl;
                allOk = false;
            }
            continue;
        }

        double alphaDiff = std::abs(avgAlpha - refAlpha);
        int hashDist = hammingDistance(pHash, refHash);
        std::cout << baseName.toStdString()
                  << " alphaDiff=" << alphaDiff
                  << " hashDist=" << hashDist << std::endl;

        if (alphaDiff > alphaTol) {
            std::cerr << "FAIL: Alpha divergiu em " << baseName.toStdString()
                      << " ref=" << refAlpha << " cur=" << avgAlpha
                      << " tol=" << alphaTol << std::endl;
            allOk = false;
        }
        if (hashDist > hashTol) {
            std::cerr << "FAIL: Perceptual hash divergiu em " << baseName.toStdString()
                      << " dist=" << hashDist << " tol=" << hashTol << std::endl;
            allOk = false;
        }
    }

    if (allOk) {
        std::cout << "PASS" << std::endl;
        return 0;
    } else {
        std::cerr << "FAIL: Regressão visual detectada" << std::endl;
        return 1;
    }
}
