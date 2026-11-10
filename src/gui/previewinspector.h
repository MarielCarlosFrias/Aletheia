#ifndef PREVIEWINSPECTOR_H
#define PREVIEWINSPECTOR_H

#include <QObject>
#include <QImage>
#include <QPoint>

class QLabel;
class QScrollArea;
class QStackedWidget;
class MaskEditCanvas;

// Pre-visualizacao e zoom do editor.
//
// Guarda as duas imagens (original e resultado), o nivel de zoom e toda
// a logica de escalar/ancorar o preview. Extraido do MainWindow para
// que a janela principal fique menor e a logica de preview possa ser
// testada isoladamente.
//
// Os widgets nao sao criados aqui: sao os da janela principal, ligados
// com init() depois de construidos.
class PreviewInspector : public QObject {
    Q_OBJECT
public:
    explicit PreviewInspector(QObject* parent = nullptr);

    void init(QLabel* originalLabel,
              QLabel* zoomLabel,
              QScrollArea* originalScroll,
              QScrollArea* resultScroll,
              MaskEditCanvas* maskEditCanvas,
              QStackedWidget* workspaceStack,
              QLabel* imageStatusLabel);

    // Define a imagem original (abre a area de edicao).
    void setOriginal(const QImage& img);
    // Define o resultado (a mascara editada).
    void setResult(const QImage& img);
    void clearResult();

    void setPreviewZoom(int percent);
    void zoomAt(int percent, const QPoint& globalPos);
    void updatePreviewZoom();
    void updateWorkspaceStack();

    int previewZoomPercent() const { return m_previewZoomPercent; }
    const QImage& originalImage() const { return m_originalImage; }
    const QImage& resultImage() const { return m_resultImage; }

private:
    QLabel* m_originalLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QScrollArea* m_originalScroll = nullptr;
    QScrollArea* m_resultScroll = nullptr;
    MaskEditCanvas* m_maskEditCanvas = nullptr;
    QStackedWidget* m_workspaceStack = nullptr;
    QLabel* m_imageStatusLabel = nullptr;

    QImage m_originalImage;
    QImage m_resultImage;
    int m_previewZoomPercent = 100;
};

#endif // PREVIEWINSPECTOR_H
