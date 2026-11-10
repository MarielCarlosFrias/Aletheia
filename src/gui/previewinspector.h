#ifndef PREVIEWINSPECTOR_H
#define PREVIEWINSPECTOR_H

#include <QObject>
#include <QImage>

class QLabel;
class QScrollArea;
class QStackedWidget;
class MaskEditCanvas;
class QTimer;

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

    void setOriginal(const QImage& img);
    void setResult(const QImage& img);
    void setPreviewZoom(int percent);
    void zoomAt(int percent, const QPoint& globalPos);
    void updatePreviewZoom();

    int previewZoomPercent() const { return m_previewZoomPercent; }
    const QImage& originalImage() const { return m_originalImage; }
    const QImage& resultImage() const { return m_resultImage; }

public slots:
    void onResize();

private:
    void updateWorkspaceStack();

    QLabel* m_originalLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QScrollArea* m_originalScroll = nullptr;
    QScrollArea* m_resultScroll = nullptr;
    MaskEditCanvas* m_maskEditCanvas = nullptr;
    QStackedWidget* m_workspaceStack = nullptr;
    QLabel* m_imageStatusLabel = nullptr;
    QTimer* m_zoomDebounce = nullptr;

    QImage m_originalImage;
    QImage m_resultImage;
    int m_previewZoomPercent = 100;
};

#endif // PREVIEWINSPECTOR_H
