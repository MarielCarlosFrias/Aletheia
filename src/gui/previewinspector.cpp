#include "previewinspector.h"
#include "maskeditcanvas.h"
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QPixmap>
#include <QScrollBar>

PreviewInspector::PreviewInspector(QObject* parent)
    : QObject(parent)
{
}

void PreviewInspector::init(QLabel* originalLabel,
                            QLabel* zoomLabel,
                            QScrollArea* originalScroll,
                            QScrollArea* resultScroll,
                            MaskEditCanvas* maskEditCanvas,
                            QStackedWidget* workspaceStack,
                            QLabel* imageStatusLabel)
{
    m_originalLabel = originalLabel;
    m_zoomLabel = zoomLabel;
    m_originalScroll = originalScroll;
    m_resultScroll = resultScroll;
    m_maskEditCanvas = maskEditCanvas;
    m_workspaceStack = workspaceStack;
    m_imageStatusLabel = imageStatusLabel;
}

void PreviewInspector::setOriginal(const QImage& img)
{
    m_originalImage = img;
    updateWorkspaceStack();
    updatePreviewZoom();
}

void PreviewInspector::setResult(const QImage& img)
{
    m_resultImage = img;
    updatePreviewZoom();
}

void PreviewInspector::setPreviewZoom(int percent)
{
    m_previewZoomPercent = qBound(50, percent, 400);
    if (m_zoomLabel)
        m_zoomLabel->setText(QString::number(m_previewZoomPercent) + "%");
    updatePreviewZoom();
    updateWorkspaceStack();
}

void PreviewInspector::zoomAt(int percent, const QPoint& globalPos)
{
    if (m_resultImage.isNull() || !m_maskEditCanvas) return;
    const QWidget* canvas = m_maskEditCanvas;
    const QPoint canvasPos = canvas->mapFromGlobal(globalPos);
    const bool overCanvas = canvasPos.x() >= 0 && canvasPos.y() >= 0
        && canvasPos.x() < canvas->width() && canvasPos.y() < canvas->height();
    if (!overCanvas) {
        setPreviewZoom(percent);
        return;
    }
    const QSize imageSize = m_resultImage.size();
    double oldScale = double(canvas->width()) / imageSize.width();
    setPreviewZoom(percent);
    double newScale = double(m_maskEditCanvas->width()) / imageSize.width();
    if (qFuzzyCompare(oldScale, newScale)) return;
    const QPointF imgPt(canvasPos.x() / oldScale, canvasPos.y() / oldScale);
    const QPoint viewportPos = m_resultScroll->viewport()->mapFromGlobal(globalPos);
    const QPoint target(
        static_cast<int>(imgPt.x() * newScale) - viewportPos.x(),
        static_cast<int>(imgPt.y() * newScale) - viewportPos.y());
    QScrollBar* h = m_resultScroll->horizontalScrollBar();
    QScrollBar* v = m_resultScroll->verticalScrollBar();
    QTimer::singleShot(0, this, [h, v, target]() {
        h->setValue(target.x());
        v->setValue(target.y());
    });
}

void PreviewInspector::updatePreviewZoom()
{
    if (m_originalImage.isNull() || !m_originalLabel || !m_originalScroll) return;
    const QSize originalViewport = m_originalScroll->viewport()->size();
    if (originalViewport.isEmpty()) return;
    const QSize fittedOriginal = m_originalImage.size().scaled(originalViewport, Qt::KeepAspectRatio);
    m_originalLabel->setPixmap(QPixmap::fromImage(m_originalImage).scaled(fittedOriginal, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_originalLabel->resize(fittedOriginal);

    if (m_resultImage.isNull() || !m_maskEditCanvas || !m_resultScroll) return;
    const QSize resultViewport = m_resultScroll->viewport()->size();
    if (resultViewport.isEmpty()) return;
    const QSize fittedResult = m_resultImage.size().scaled(resultViewport, Qt::KeepAspectRatio);
    const QSize resultDisplay = fittedResult * m_previewZoomPercent / 100;
    m_maskEditCanvas->setDisplaySize(resultDisplay);
}

void PreviewInspector::updateWorkspaceStack()
{
    if (!m_workspaceStack) return;
    const bool hasImage = !m_originalImage.isNull();
    m_workspaceStack->setCurrentIndex(hasImage ? 1 : 0);
    if (m_imageStatusLabel) {
        if (hasImage) {
            m_imageStatusLabel->setText(
                QString("%1 × %2 px • Zoom %3%")
                    .arg(m_originalImage.width())
                    .arg(m_originalImage.height())
                    .arg(m_previewZoomPercent));
        } else {
            m_imageStatusLabel->setText(QObject::tr("Nenhuma imagem selecionada"));
        }
    }
}

void PreviewInspector::onResize()
{
    updatePreviewZoom();
}
