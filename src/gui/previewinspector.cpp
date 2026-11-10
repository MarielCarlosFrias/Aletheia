#include "previewinspector.h"
#include "maskeditcanvas.h"
#include "i18n.h"

#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTimer>
#include <QPixmap>

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
    // Troca primeiro para a area de edicao: o preview so tem tamanho
    // util depois de o stack mostrar a pagina do editor.
    updateWorkspaceStack();
    updatePreviewZoom();
}

void PreviewInspector::setResult(const QImage& img)
{
    m_resultImage = img;
    updatePreviewZoom();
}

void PreviewInspector::clearResult()
{
    m_resultImage = QImage();
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
    const bool overCanvas =
        canvasPos.x() >= 0 && canvasPos.y() >= 0
        && canvasPos.x() < canvas->width()
        && canvasPos.y() < canvas->height();
    if (!overCanvas) {
        // Cursor fora do resultado: zoom sem ancoragem.
        setPreviewZoom(percent);
        return;
    }

    const QSize imageSize = m_resultImage.size();
    if (imageSize.width() <= 0) return;
    const double oldScale = double(canvas->width()) / imageSize.width();
    setPreviewZoom(percent);
    const double newScale =
        double(m_maskEditCanvas->width()) / imageSize.width();
    if (qFuzzyCompare(oldScale, newScale)) return;

    // Mantem o mesmo pixel da imagem sob o cursor: ajusta as barras
    // de rolagem para a nova escala.
    const QPointF imgPt(canvasPos.x() / oldScale,
                        canvasPos.y() / oldScale);
    if (!m_resultScroll) return;
    const QPoint viewportPos =
        m_resultScroll->viewport()->mapFromGlobal(globalPos);
    const QPoint target(
        static_cast<int>(imgPt.x() * newScale) - viewportPos.x(),
        static_cast<int>(imgPt.y() * newScale) - viewportPos.y());

    // Os limites das barras atualizam-se apos o resize; agenda o
    // ajuste para o proximo ciclo de eventos.
    QScrollBar* h = m_resultScroll->horizontalScrollBar();
    QScrollBar* v = m_resultScroll->verticalScrollBar();
    QTimer::singleShot(0, this, [h, v, target]() {
        h->setValue(target.x());
        v->setValue(target.y());
    });
}

void PreviewInspector::updatePreviewZoom()
{
    if (m_originalImage.isNull()) return;
    if (!m_originalLabel || !m_originalScroll) return;

    // O painel Original e referencia fixa: fica sempre ajustado ao
    // espaco disponivel, independentemente do zoom.
    const QSize originalViewport = m_originalScroll->viewport()->size();
    if (originalViewport.isEmpty()) return;
    const QSize fittedOriginal = m_originalImage.size().scaled(
        originalViewport, Qt::KeepAspectRatio);
    m_originalLabel->setPixmap(QPixmap::fromImage(m_originalImage).scaled(
        fittedOriginal, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_originalLabel->resize(fittedOriginal);

    // O zoom (botoes, Ctrl+/- e Ctrl+roda) aplica-se so ao Resultado,
    // que e a area de trabalho.
    if (m_resultImage.isNull() || !m_maskEditCanvas || !m_resultScroll)
        return;
    const QSize resultViewport = m_resultScroll->viewport()->size();
    if (resultViewport.isEmpty()) return;
    const QSize fittedResult = m_resultImage.size().scaled(
        resultViewport, Qt::KeepAspectRatio);
    const QSize resultDisplay = fittedResult * m_previewZoomPercent / 100;
    m_maskEditCanvas->setDisplaySize(resultDisplay);
}

void PreviewInspector::updateWorkspaceStack()
{
    if (!m_workspaceStack) return;
    const bool hasImage = !m_originalImage.isNull();
    m_workspaceStack->setCurrentIndex(hasImage ? 1 : 0);
    if (!m_imageStatusLabel) return;
    if (hasImage) {
        m_imageStatusLabel->setText(
            QString("%1 \u00d7 %2 px \u2022 Zoom %3%")
                .arg(m_originalImage.width())
                .arg(m_originalImage.height())
                .arg(m_previewZoomPercent));
    } else {
        m_imageStatusLabel->setText(
            TR("Nenhuma imagem selecionada", "No image selected"));
    }
}
