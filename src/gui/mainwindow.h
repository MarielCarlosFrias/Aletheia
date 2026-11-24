#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFutureWatcher>
#include <QMainWindow>
#include <QImage>
#include <QMutex>
#include <QStringList>
#include <QList>
#include <memory>
#include "backgroundremover.h"
#include "processingoptions.h"
#include "inferenceworker.h"

class QLabel;
class QScrollArea;
class QPushButton;
class QLineEdit;
class QProgressBar;
class QPlainTextEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QTabWidget;
class QMenu;
class QResizeEvent;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QTimer;
class QNetworkAccessManager;
class QNetworkReply;
class QVBoxLayout;
class QStackedWidget;
class MaskEditCanvas;
class ModelManagerWidget;
class PreviewInspector;
class InferenceWorker;
class BatchTabController;
class UpdateChecker;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // Sessao ONNX partilhada entre a GUI e as threads de trabalho.
    // O acesso e protegido por mutex porque o modelo e carregado
    // dentro da thread de inferencia (para nao bloquear a janela).
    explicit MainWindow(QWidget* parent = nullptr,
                        const QString& modelPath = "");

public slots:
    // Abre a imagem indicada (tambem usada por drag & drop e testes).
    void openPath(const QString& path);
    // Remove o fundo da imagem carregada; o resultado chega pelo
    // sinal jobFinished().
    void removeBackground();

signals:
    // Emitido no fim do processamento (sucesso ou erro) e do lote.
    void jobFinished();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void openImage();
    void onRemoveFinished(const InferenceResult& result);
    void saveResult();
    void copyResult();
    void copyTransparentResult();
    void loadModel();
    void modelIndexChanged(int index);
    void bgColorChanged(int index);
    void editingDone();
    void onSaveFormatChanged(int index);
    void showUpdateBanner(const QString& version);
    void pasteImageFromClipboard();
    void autoCropResult();
    void presetChanged(int index);
    void startDragOut();
    void removalMethodChanged(int index);
    void pickKeyColor();
    void pickStickerColor();
    void autoDetectKeyColor();
    void removeByColor();
    void onColorRemovalFinished();
    void onSaveProcessingFinished();
    void onPostProcessFinished();
    void onAutoCropFinished();
    void openRecentFile();

private:
    void updateWindowTitle();
    void addToRecentFiles(const QString& path);
    void rebuildRecentMenu();
    void updateKeyColorButton();
    void updateStickerColorButton();
    void setPreviewZoom(int percent);
    void zoomAt(int percent, const QPoint& globalPos);
    void updatePreviewZoom();
    void setOriginal(const QImage& img);
    void setResult(const QImage& img);
    void updateResultDisplay();
    void setUiBusy(bool busy);
    // Fecha a janela depois de a inferencia/lote em curso terminar.
    void maybeCloseAfterJobs();
    void resetModel();
    void populateModelCombo();
    void restoreSettings();
    void saveSettings();
    QWidget* createEmptyStateWidget();
    void updateWorkspaceStack();
    QVBoxLayout* workspaceLayout();
    QVBoxLayout* mainLayout_ = nullptr;
    QString saveFormatFilter() const;
    QString saveFormatExtension() const;
    bool saveFormatIsAlphaCapable() const;
    ProcessingOptions currentProcessingOptions() const;
    QColor currentBackgroundColor() const;

    QStackedWidget* workspaceStack_ = nullptr;
    QWidget* dropZoneWidget_ = nullptr;
    QWidget* previewContainer_ = nullptr;
    QLabel* imageStatusLabel_ = nullptr;
    QLabel* originalLabel_;
    QLabel* imageNameLabel_;
    QLabel* zoomLabel_;
    QScrollArea* originalScroll_;
    QScrollArea* resultScroll_;
    QComboBox* modelCombo_;
    QLineEdit* modelPathEdit_;
    QPushButton* btnOpen_;
    QPushButton* btnThemeToggle_;
    QComboBox* langCombo_;
    QPushButton* btnRemove_;
    QPushButton* btnSave_;
    QPushButton* btnCopy_;
    QPushButton* btnCopyTransparent_;
    QAction* actCopyTransparent_;
    QPushButton* btnLoadModel_;
    QPushButton* btnUndo_;
    QPushButton* btnRedo_;
    QPushButton* btnAutoCrop_;
    QPushButton* btnDragOut_;
    QComboBox* presetCombo_;
    QComboBox* bgColorCombo_;
    QComboBox* brushModeCombo_;
    QSpinBox* brushRadiusSpin_;
    QSpinBox* noiseThresholdSpin_;
    QCheckBox* refineCheckBox_;
    QDoubleSpinBox* edgeRefinementSpin_;
    QCheckBox* noiseCheckBox_;
    QCheckBox* stickerCheckBox_;
    QSpinBox* stickerWidthSpin_;
    QPushButton* stickerColorButton_;
    QCheckBox* shadowCheckBox_;
    QSpinBox* shadowBlurSpin_;
    QSpinBox* shadowOffsetXSpin_;
    QSpinBox* shadowOffsetYSpin_;
    QDoubleSpinBox* shadowOpacitySpin_;
    QComboBox* removalMethodCombo_;
    QWidget* aiRemovalWidget_;
    QWidget* colorRemovalWidget_;
    QPushButton* btnKeyColorSample_;
    QPushButton* btnAutoDetectKeyColor_;
    QSpinBox* colorToleranceSpin_;
    QCheckBox* colorContiguousCheckBox_;
    QCheckBox* colorPixelArtCheckBox_;
    QDoubleSpinBox* chromaDespillSpin_;
    QSpinBox* chromaFeatherSpin_;
    QPushButton* btnRemoveByColor_;
    QColor keyColor_ = QColor(255, 0, 255);
    QString customBgImagePath_;
    QComboBox* saveFormatCombo_;
    QSpinBox* exportQualitySpin_;
    QSpinBox* pngCompressionSpin_;
    QCheckBox* preserveMetadataCheckBox_;
    QSpinBox* outputWidthSpin_;
    QSpinBox* outputHeightSpin_;
    QComboBox* rotationCombo_;
    QProgressBar* progressBar_;
    QPlainTextEdit* logEdit_;
    MaskEditCanvas* maskEditCanvas_;
    QTimer* zoomDebounce_;
    QTabWidget* mainTabs_;
    ModelManagerWidget* modelManager_;

    // Verdadeiro enquanto uma inferencia ou lote esta em curso. Os
    // botoes sao desativados, mas os atalhos tambem precisam de ser
    // bloqueados para nao salvar/copiar um resultado antigo.
    bool isBusy_ = false;
    // O utilizador tentou fechar durante uma inferencia/lote; o
    // fecho e concluido quando o watcher correspondente terminar.
    bool closeRequested_ = false;
    QColor customBgColor_;
    QColor stickerColor_ = Qt::white;
    bool showMaskPreview_ = false;
    QAction *actionShowMaskPreview_ = nullptr;

    QString currentImagePath_;
    QStringList recentFiles_;
    QMenu* recentMenu_ = nullptr;
    // Muda sempre que uma imagem nova e carregada; resultados de
    // inferencias antigas sao descartados em onRemoveFinished().
    quint64 imageGeneration_ = 0;
    // Inferencia em thread de trabalho (cache do modelo + ONNX).
    InferenceWorker* inference_ = nullptr;
    std::shared_ptr<ModelCache> modelCache_;
    // Pre-visualizacao e zoom: imagens (original/resultado) e logica de
    // escala/ancoragem do zoom.
    PreviewInspector* preview_ = nullptr;
    // Aba "Lote": pastas, plano e processamento em serie.
    BatchTabController* batch_ = nullptr;
    // Verificacao de atualizacoes (GitHub Releases API).
    UpdateChecker* updateChecker_ = nullptr;
    // Remocao por cor: a inundacao de regiao pode demorar segundos
    // numa foto grande, por isso corre na thread de trabalho.
    QFutureWatcher<QImage> colorWatcher_;
    // Pós-processamento e escrita do save (sticker/sombra, geometria,
    // qualidade e metadados) correm na thread de trabalho para não
    // bloquear a interface com imagens grandes.
    struct SaveOutcome {
        bool success = false;
        QString error;
    };
    QFutureWatcher<SaveOutcome> saveWatcher_;
    QString pendingSavePath_;

    // Watcher generico para copy/drag-out: o pós-processamento pesado
    // (sticker/sombra) tambem corre fora da thread da UI.
    enum class PendingAction { None, CopyToClipboard, CopyTransparent, DragOut };
    QFutureWatcher<QImage> postProcessWatcher_;
    PendingAction pendingAction_ = PendingAction::None;
    ProcessingOptions pendingPostOptions_;
    // Auto-recorte (contentBounds percorre todos os pixels) tambem
    // corre na thread de trabalho.
    QFutureWatcher<QImage> autoCropWatcher_;
};

#endif
