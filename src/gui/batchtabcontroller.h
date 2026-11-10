#ifndef BATCHTABCONTROLLER_H
#define BATCHTABCONTROLLER_H

#include <QObject>
#include <QFutureWatcher>
#include <QString>
#include <QList>
#include <functional>
#include <memory>

#include "batchplan.h"
#include "processingoptions.h"

class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QWidget;
struct ModelCache;

// Aba "Lote": processamento de pastas inteiras de imagens.
//
// A aba e o seu estado (pastas, plano, progresso) vivem aqui; a janela
// principal fornece apenas o que e partilhado: o log, a barra de
// progresso, a cache do modelo e as opcoes de processamento.
class BatchTabController : public QObject {
    Q_OBJECT
public:
    struct Config {
        // Widgets partilhados com a janela principal.
        QPlainTextEdit* log = nullptr;
        QProgressBar* progress = nullptr;
        // Cache do modelo ONNX (partilhada com a inferencia individual).
        std::shared_ptr<ModelCache> modelCache;
        // Opcoes de processamento (modelo, fundo, refinamentos...).
        std::function<ProcessingOptions()> optionsProvider;
        // Extensao de saida escolhida no painel (png/jpg/webp).
        std::function<QString()> outputExtensionProvider;
        // Verdadeiro se o formato escolhido suporta canal alfa.
        std::function<bool()> outputFormatHasAlpha;
    };

    explicit BatchTabController(QObject* parent = nullptr);
    ~BatchTabController() override;

    void init(const Config& config);

    // Constroi a aba (titulo, pastas, botao e log).
    QWidget* createTab();

    void selectFolder();
    void selectOutputFolder();

    // Inicia o lote (ou pede o cancelamento, se ja estiver a correr).
    void processBatch();
    void processBatchStep();
    void onBatchStepFinished();
    void finishBatch();

    bool isRunning() const;
    // Pede o cancelamento (usado quando a janela esta a fechar).
    void requestCancel();
    // Desativa os controlos da aba durante uma operacao.
    void setBusy(bool busy);

    QLineEdit* folderEdit() const { return folderEdit_; }
    QLineEdit* outputDirEdit() const { return outputDirEdit_; }

signals:
    // A janela principal reage (bloquear a UI, atualizar o estado).
    void busyChanged(bool busy);
    // O lote terminou (ok, erros).
    void finished(int ok, int errors);

private:
    Config config_;

    QLineEdit* folderEdit_ = nullptr;
    QLineEdit* outputDirEdit_ = nullptr;
    QPushButton* btnSelectFolder_ = nullptr;
    QPushButton* btnSelectOutput_ = nullptr;
    QPushButton* btnProcessBatch_ = nullptr;

    QString batchFolder_;
    QString batchOutputDir_;
    QString batchOutputExtension_;
    QList<BatchPlan::Entry> batchPlan_;
    ProcessingOptions batchOptions_;
    int batchIndex_ = 0;
    int batchOk_ = 0;
    int batchErrors_ = 0;
    bool batchRunning_ = false;
    bool batchCancelRequested_ = false;
    bool closing_ = false;
    QFutureWatcher<QString> watcher_;
};

#endif // BATCHTABCONTROLLER_H
