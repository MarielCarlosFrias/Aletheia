#ifndef BACKGROUNDREMOVER_H
#define BACKGROUNDREMOVER_H

#include <QImage>
#include <QString>
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <unordered_map>

class BackgroundRemover {
public:
    explicit BackgroundRemover(const QString& modelPath);

    // Fábrica com cache estático: reutiliza a mesma sessão ONNX para o mesmo caminho.
    static std::shared_ptr<BackgroundRemover> get(const QString& modelPath);

    QImage removeBackground(const QImage& input);

    // Solicita cancelamento da inferência em curso.
    void requestCancel();

    bool isCancelled() const { return cancelRequested_.load(); }

    // Resolucao de entrada que o modelo espera (320 na U2-Net/Silueta,
    // 1024 na ISNet). Detetada automaticamente no construtor.
    int inputSize() const { return input_size_; }

    BackgroundRemover(const BackgroundRemover&) = delete;
    BackgroundRemover& operator=(const BackgroundRemover&) = delete;

private:
    std::vector<float> buildInputBlob(const QImage& img) const;
    QImage buildAlphaMask(const float* mask, int mask_w, int mask_h,
                          int orig_w, int orig_h) const;
    QImage composite(const QImage& img, const QImage& alpha) const;
    int detectInputSize(const QString& modelPath) const;

    static QString computeSha256(const QString& path);
    static void saveModelMetadata(const QString& path, const QString& sha);
    static QString loadModelMetadataSha(const QString& path);

    Ort::Env env_;
    Ort::Session session_;
    std::string input_name_;
    std::string output_name_;

    static constexpr int kDefaultInputSize = 320;
    // U2-Net/Silueta usam 320; ISNet usa 1024. Detetado a partir do
    // shape do modelo (com fallback pelo nome quando e dinamico).
    int input_size_ = kDefaultInputSize;

    mutable std::atomic<bool> cancelRequested_{false};
    mutable std::mutex runMutex_;
    mutable Ort::RunOptions* activeRunOptions_ = nullptr;

private:
    static std::mutex cacheMutex_;
    static std::unordered_map<QString, std::weak_ptr<BackgroundRemover>> cache_;
};

#endif
