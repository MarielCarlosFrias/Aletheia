#include "batchtabcontroller.h"
#include "backgroundremover.h"
#include "imagepostprocessor.h"
#include "inferenceworker.h"
#include "i18n.h"

#include <QDir>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

BatchTabController::BatchTabController(QObject* parent)
    : QObject(parent)
{
    connect(&watcher_, &QFutureWatcher<QString>::finished,
            this, &BatchTabController::onBatchStepFinished);
}

BatchTabController::~BatchTabController() = default;

void BatchTabController::init(const Config& config)
{
    config_ = config;
}

QWidget* BatchTabController::createTab()
{
    auto* tab = new QWidget;
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    auto* headerBox = new QVBoxLayout;
    headerBox->setSpacing(4);
    auto* title = new QLabel(TR("Processamento em Lote", "Batch Processing"));
    title->setObjectName("batchPageTitle");
    auto* desc = new QLabel(TR(
        "Processe pastas inteiras de imagens sem bloquear o computador. As imagens recortadas são salvas automaticamente.",
        "Process entire folders of images without freezing your computer. Cutout images are saved automatically."));
    desc->setObjectName("batchPageSubtitle");
    desc->setWordWrap(true);
    headerBox->addWidget(title);
    headerBox->addWidget(desc);
    layout->addLayout(headerBox);

    auto* foldersCard = new QFrame;
    foldersCard->setObjectName("batchCard");
    auto* foldersLayout = new QFormLayout(foldersCard);
    foldersLayout->setContentsMargins(18, 16, 18, 16);
    foldersLayout->setSpacing(12);
    foldersLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);

    auto* inRow = new QHBoxLayout;
    folderEdit_ = new QLineEdit;
    folderEdit_->setObjectName("batchInput");
    folderEdit_->setPlaceholderText(
        TR("Selecione ou arraste uma pasta com imagens...",
           "Select or drop a folder with images..."));
    inRow->addWidget(folderEdit_, 1);
    btnSelectFolder_ = new QPushButton(TR("Escolher pasta...", "Choose folder..."));
    btnSelectFolder_->setObjectName("batchSelectFolder");
    inRow->addWidget(btnSelectFolder_);
    foldersLayout->addRow(TR("Pasta de origem:", "Source folder:"), inRow);

    auto* outRow = new QHBoxLayout;
    outputDirEdit_ = new QLineEdit;
    outputDirEdit_->setObjectName("batchOutput");
    outRow->addWidget(outputDirEdit_, 1);
    btnSelectOutput_ = new QPushButton(TR("Salvar em...", "Save to..."));
    btnSelectOutput_->setObjectName("batchSelectOutput");
    outRow->addWidget(btnSelectOutput_);
    foldersLayout->addRow(TR("Pasta de destino:", "Destination folder:"), outRow);

    layout->addWidget(foldersCard);

    auto* actionCard = new QFrame;
    actionCard->setObjectName("batchCard");
    auto* actionLayout = new QVBoxLayout(actionCard);
    actionLayout->setContentsMargins(18, 16, 18, 16);
    actionLayout->setSpacing(12);

    auto* actionRow = new QHBoxLayout;
    btnProcessBatch_ = new QPushButton(TR("Processar lote", "Process batch"));
    btnProcessBatch_->setObjectName("processBatchButton");
    btnProcessBatch_->setMinimumHeight(38);
    actionRow->addWidget(btnProcessBatch_, 0);
    actionRow->addStretch();
    actionLayout->addLayout(actionRow);

    layout->addWidget(actionCard);

    auto* logHeaderRow = new QHBoxLayout;
    auto* btnToggleBatchLog = new QPushButton(TR("Mostrar log de processamento \u25be",
                                                 "Show processing log \u25be"));
    btnToggleBatchLog->setObjectName("batchLogToggle");
    logHeaderRow->addWidget(btnToggleBatchLog);
    logHeaderRow->addStretch();
    layout->addLayout(logHeaderRow);

    if (config_.log) {
        config_.log->setObjectName("logEdit");
        layout->addWidget(config_.log, 1);
    }

    connect(btnToggleBatchLog, &QPushButton::clicked, this,
            [this, btnToggleBatchLog]() {
        if (!config_.log) return;
        bool nowVisible = !config_.log->isVisible();
        config_.log->setVisible(nowVisible);
        btnToggleBatchLog->setText(nowVisible
            ? TR("Esconder log de processamento \u25b4", "Hide processing log \u25b4")
            : TR("Mostrar log de processamento \u25be", "Show processing log \u25be"));
    });
    connect(btnSelectFolder_, &QPushButton::clicked,
            this, &BatchTabController::selectFolder);
    connect(btnSelectOutput_, &QPushButton::clicked,
            this, &BatchTabController::selectOutputFolder);
    connect(btnProcessBatch_, &QPushButton::clicked,
            this, &BatchTabController::processBatch);

    return tab;
}

void BatchTabController::setBusy(bool busy)
{
    if (folderEdit_) folderEdit_->setEnabled(!busy);
    if (outputDirEdit_) outputDirEdit_->setEnabled(!busy);
    if (btnSelectFolder_) btnSelectFolder_->setEnabled(!busy);
    if (btnSelectOutput_) btnSelectOutput_->setEnabled(!busy);
    if (btnProcessBatch_) btnProcessBatch_->setEnabled(!busy);
}

bool BatchTabController::isRunning() const
{
    return batchRunning_;
}

void BatchTabController::requestCancel()
{
    if (!batchRunning_) return;
    batchCancelRequested_ = true;
    closing_ = true;
}

void BatchTabController::selectFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(
        nullptr, TR("Escolher pasta de imagens", "Choose image folder"),
        folderEdit_->text().isEmpty() ? QDir::homePath() : folderEdit_->text());
    if (!dir.isEmpty()) folderEdit_->setText(dir);
}

void BatchTabController::selectOutputFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(
        nullptr, TR("Escolher pasta de destino", "Choose destination folder"),
        outputDirEdit_->text().isEmpty() ? QDir::homePath() : outputDirEdit_->text());
    if (!dir.isEmpty()) outputDirEdit_->setText(dir);
}

void BatchTabController::processBatch()
{
    // Em execucao: o botao funciona como "Cancelar lote".
    if (batchRunning_) {
        batchCancelRequested_ = true;
        if (btnProcessBatch_) btnProcessBatch_->setEnabled(false);
        if (config_.log) {
            config_.log->appendPlainText("[BATCH] " + TR(
                "Cancelamento solicitado; a terminar a imagem em curso...",
                "Cancellation requested; finishing the current image..."));
        }
        return;
    }

    if (!folderEdit_ || folderEdit_->text().isEmpty()) {
        QMessageBox::warning(nullptr, TR("Aviso", "Warning"),
            TR("Selecione uma pasta primeiro.",
               "Select a folder first."));
        return;
    }
    batchFolder_ = folderEdit_->text();
    batchOutputDir_ = outputDirEdit_->text();

    // O formato escolhido no painel tambem vale para o lote. Se o
    // plugin nao existir (ex.: WebP no Linux), avisar antes de
    // comecar em vez de falhar imagem a imagem.
    batchOutputExtension_ = config_.outputExtensionProvider
        ? config_.outputExtensionProvider() : QStringLiteral("png");
    if (!QImageWriter::supportedImageFormats().contains(
            batchOutputExtension_.toLatin1())) {
        QMessageBox::warning(nullptr, TR("Aviso", "Warning"),
            TR("O formato %1 não é suportado nesta instalação do Qt "
               "(no Linux instale o pacote "
               "'qt6-image-formats-plugins').",
               "Format %1 is not supported by this Qt installation "
               "(on Linux install the 'qt6-image-formats-plugins' "
               "package).")
                .arg(batchOutputExtension_.toUpper()));
        return;
    }

    const QDir inDir(batchFolder_);
    if (!inDir.exists()) {
        QMessageBox::warning(nullptr, TR("Aviso", "Warning"),
            TR("A pasta de origem não existe.",
               "The source folder does not exist."));
        return;
    }

    QDir outDir(batchOutputDir_);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        QMessageBox::critical(nullptr, TR("Erro", "Error"),
            TR("Não foi possível criar a pasta de destino:\n",
               "Could not create the output directory:\n")
                + batchOutputDir_);
        return;
    }

    // O plano e construido aqui (o nome do ficheiro segue o formato).
    batchPlan_ = BatchPlan::build(inDir, outDir, batchOutputExtension_);
    if (batchPlan_.isEmpty()) {
        QMessageBox::warning(nullptr, TR("Aviso", "Warning"),
            TR("Nenhuma imagem encontrada na pasta.",
               "No images found in the folder."));
        return;
    }

    emit busyChanged(true);
    // O botao continua ativo, agora como Cancelar.
    if (btnProcessBatch_) {
        btnProcessBatch_->setEnabled(true);
        btnProcessBatch_->setText(TR("Cancelar lote", "Cancel batch"));
    }
    if (config_.progress) {
        config_.progress->setRange(0, batchPlan_.size());
        config_.progress->setValue(0);
        config_.progress->setVisible(true);
    }
    if (config_.log) {
        config_.log->clear();
        config_.log->appendPlainText(
            QString("[BATCH] %1 -> %2")
                .arg(TR("Processando %1 arquivo(s)",
                        "Processing %1 file(s)").arg(batchPlan_.size()))
                .arg(batchOutputDir_));
    }

    batchIndex_ = 0;
    batchOk_ = 0;
    batchErrors_ = 0;
    batchCancelRequested_ = false;
    batchRunning_ = true;
    batchOptions_ = config_.optionsProvider ? config_.optionsProvider()
                                            : ProcessingOptions();
    batchOptions_.outputFormat = batchOutputExtension_;
    // JPEG nao tem canal alfa: compor sobre branco quando o fundo
    // escolhido e transparente. PNG/WebP mantem o alfa.
    const bool hasAlpha = !config_.outputFormatHasAlpha
                              || config_.outputFormatHasAlpha();
    if (!hasAlpha && !batchOptions_.backgroundColor.isValid()) {
        batchOptions_.backgroundColor = Qt::white;
    }

    // O modelo e carregado dentro da thread de trabalho (obtainRemover
    // no primeiro passo), por isso esta funcao nunca bloqueia a GUI.
    QTimer::singleShot(0, this, &BatchTabController::processBatchStep);
}

void BatchTabController::processBatchStep()
{
    if (!batchRunning_) return;
    if (batchCancelRequested_ || batchIndex_ >= batchPlan_.size()) {
        finishBatch();
        return;
    }

    const BatchPlan::Entry& entry = batchPlan_[batchIndex_];
    const QString file = QFileInfo(entry.inputPath).fileName();
    if (config_.log) {
        config_.log->appendPlainText(
            QString("[BATCH] %1/%2: %3")
                .arg(batchIndex_ + 1).arg(batchPlan_.size()).arg(file));
    }

    // Mensagens de erro traduzidas na GUI antes da thread (I18n usa
    // QSettings, que nao deve ser lido fora da thread principal).
    const QString errLoad = TR("Falha ao carregar a imagem",
                               "Failed to load the image");
    const QString errSave = TR("Falha ao salvar a imagem",
                               "Failed to save the image");
    const QString errModel = TR("modelo indisponível",
                                "model unavailable");
    const QString errOnnx = TR("Erro ONNX: ", "ONNX error: ");
    const QString errUnknown = TR("Erro desconhecido ao processar a imagem",
                                  "Unknown error processing the image");

    // Cada imagem corre na thread de trabalho; a GUI so recebe o
    // resultado via watcher e agenda o passo seguinte.
    watcher_.setFuture(QtConcurrent::run(
        [cache = config_.modelCache, options = batchOptions_,
         in = entry.inputPath, out = entry.outputPath,
         format = batchOutputExtension_,
         errLoad, errSave, errModel, errOnnx, errUnknown]() -> QString {
            try {
                // setAutoTransform respeita a orientacao EXIF, como no
                // CLI; sem isto fotos de telemovel saiam rodadas no lote.
                const QImage img = ImagePostProcessor::loadImageExif(in);
                if (img.isNull())
                    return errLoad;
                QString modelError;
                const std::shared_ptr<BackgroundRemover> r =
                    InferenceWorker::obtainRemover(cache, options.modelPath,
                                                 &modelError);
                if (!r) {
                    return modelError.isEmpty()
                        ? errModel
                        : errModel + QStringLiteral(": ") + modelError;
                }
                const QImage processed = ImagePostProcessor::apply(
                    r->removeBackground(img), options);
                if (!processed.save(out, format.toUtf8().constData()))
                    return errSave;
                return QString();
            } catch (const Ort::Exception& e) {
                return errOnnx + QString::fromUtf8(e.what());
            } catch (const std::exception& e) {
                return QString::fromUtf8(e.what());
            } catch (...) {
                return errUnknown;
            }
        }));
}

void BatchTabController::onBatchStepFinished()
{
    if (!batchRunning_) return;
    const BatchPlan::Entry& entry = batchPlan_[batchIndex_];
    const QString file = QFileInfo(entry.inputPath).fileName();
    const QString outName = QFileInfo(entry.outputPath).fileName();

    const QString err = watcher_.result();
    if (err.isEmpty()) {
        ++batchOk_;
        if (config_.log)
            config_.log->appendPlainText("[OK] " + file + " -> " + outName);
    } else {
        ++batchErrors_;
        if (config_.log)
            config_.log->appendPlainText("[ERROR] " + file + ": " + err);
    }
    ++batchIndex_;
    if (config_.progress) config_.progress->setValue(batchIndex_);
    // Durante o fecho/cancelamento nao agendar mais imagens; fecha o
    // lote e deixa a janela concluir o fecho.
    if (batchCancelRequested_ || closing_) {
        finishBatch();
    } else {
        QTimer::singleShot(0, this, &BatchTabController::processBatchStep);
    }
}

void BatchTabController::finishBatch()
{
    batchRunning_ = false;
    closing_ = false;
    if (config_.progress) {
        config_.progress->setValue(batchPlan_.size());
        config_.progress->setVisible(false);
    }
    if (config_.log) {
        config_.log->appendPlainText(
            QString("[BATCH] %1: %2 OK, %3 erro(s).")
                .arg(batchCancelRequested_
                         ? TR("Cancelado", "Cancelled")
                         : TR("Concluido", "Completed"))
                .arg(batchOk_).arg(batchErrors_));
    }
    if (btnProcessBatch_)
        btnProcessBatch_->setText(TR("Processar lote", "Process batch"));
    batchCancelRequested_ = false;
    emit busyChanged(false);
    emit finished(batchOk_, batchErrors_);
}
