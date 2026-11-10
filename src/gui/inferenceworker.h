#ifndef INFERENCEWORKER_H
#define INFERENCEWORKER_H

#include <QObject>
#include <QFutureWatcher>
#include <QImage>
#include <QString>
#include <QMutex>
#include <memory>

class BackgroundRemover;
struct ProcessingOptions;

// Resultado de uma inferencia: a imagem processada ou a mensagem de
// erro, mais a geracao a que pertence (para descartar resultados de
// imagens ja substituidas).
struct InferenceResult {
    QImage image;
    QString error;
    quint64 generation = 0;
};

// Sessao ONNX partilhada entre a inferencia individual e o lote.
// O mutex protege a troca de modelo enquanto uma thread o carrega.
struct ModelCache {
    QMutex mutex;
    std::shared_ptr<BackgroundRemover> remover;
    QString loadedPath;
};

// Executa a inferencia numa thread de trabalho.
//
// O modelo e carregado dentro da thread (a janela principal nao
// congela enquanto o ONNX le os 176 MB do ficheiro) e a sessao fica
// em cache para as inferencias seguintes.
class InferenceWorker : public QObject {
    Q_OBJECT
public:
    explicit InferenceWorker(QObject* parent = nullptr);

    // Cache partilhado (tambem usado pelo lote).
    std::shared_ptr<ModelCache> cache() const { return cache_; }

    // Carrega (se necessario) e aplica o modelo a imagem. O resultado
    // chega pelo sinal finished().
    void start(const QImage& image,
               const ProcessingOptions& options,
               const QString& modelPath,
               quint64 generation);

    // Pede o cancelamento da inferencia em curso.
    void cancel();

    bool isRunning() const;

    // Descarta o modelo em cache (usado quando o utilizador muda de
    // modelo ou apaga o ficheiro).
    void resetModel();

    // Devolve nullptr se o modelo nao existir ou for invalido; nesse
    // caso errorMessage recebe o motivo. E estatico para poder ser
    // chamado da thread de trabalho sem capturar o `this`.
    static std::shared_ptr<BackgroundRemover> obtainRemover(
        const std::shared_ptr<ModelCache>& cache,
        const QString& modelPath,
        QString* errorMessage);

signals:
    void finished(const InferenceResult& result);

private:
    std::shared_ptr<ModelCache> cache_;
    QFutureWatcher<InferenceResult> watcher_;
};

#endif // INFERENCEWORKER_H
