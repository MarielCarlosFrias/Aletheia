#ifndef BACKGROUNDREMOVER_H
#define BACKGROUNDREMOVER_H

#include <QImage>
#include <QString>
#include <QDateTime>
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

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

    // Execution provider ativo (CPU, CUDA, DML, CoreML).
    QString provider() const { return provider_; }

    BackgroundRemover(const BackgroundRemover&) = delete;
    BackgroundRemover& operator=(const BackgroundRemover&) = delete;

private:
    std::vector<float> buildInputBlob(const QImage& img) const;
    QImage buildAlphaMask(const float* mask, int mask_w, int mask_h,
                          int orig_w, int orig_h) const;
    QImage composite(const QImage& img, const QImage& alpha) const;
    int detectInputSize(const QString& modelPath) const;

    // Deteta alteracoes ao ficheiro do modelo sem o re-ler:
    // compara tamanho e data de modificacao com os do carregamento.
    bool modelFileUnchanged() const;

    Ort::Env env_;
    Ort::Session session_;
    std::string input_name_;
    std::string output_name_;
    QString provider_ = QStringLiteral("CPU");

    static constexpr int kDefaultInputSize = 320;
    // U2-Net/Silueta usam 320; ISNet usa 1024. Detetado a partir do
    // shape do modelo (com fallback pelo nome quando e dinamico).
    int input_size_ = kDefaultInputSize;

    // Metadados do ficheiro carregado.
    QString modelPath_;
    qint64 modelSize_ = 0;
    QDateTime modelModified_;

    mutable std::atomic<bool> cancelRequested_{false};
    mutable std::mutex runMutex_;
    // Conjunto de RunOptions ativos: cada inferencia em curso regista o
    // seu shared_ptr aqui; requestCancel() itera e chama SetTerminate()
    // em TODOS, sem race entre workers do lote que partilham a sessao.
    mutable std::vector<std::shared_ptr<Ort::RunOptions>> activeRunOptions_;

private:
    static std::mutex cacheMutex_;
    static std::unordered_map<QString, std::weak_ptr<BackgroundRemover>> cache_;
};

#endif
