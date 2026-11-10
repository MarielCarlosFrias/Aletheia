#include "inferenceworker.h"
#include "backgroundremover.h"
#include "imagepostprocessor.h"
#include <QtConcurrent/QtConcurrent>
#include "i18n.h"

InferenceWorker::InferenceWorker(std::shared_ptr<ModelCache> cache, QObject* parent)
    : QObject(parent), m_cache(std::move(cache))
{
    connect(&m_watcher, &QFutureWatcher<SingleResult>::finished, this, [this]() {
        emit finished(m_watcher.result());
        m_currentRemover.reset();
    });
}

InferenceWorker::~InferenceWorker() {}

void InferenceWorker::cancel()
{
    if (m_currentRemover) {
        m_currentRemover->requestCancel();
    }
}

std::shared_ptr<BackgroundRemover> InferenceWorker::obtainRemover(const QString& modelPath)
{
    if (!m_cache) return nullptr;
    {
        QMutexLocker lock(&m_cache->mutex);
        if (m_cache->remover && m_cache->loadedPath == modelPath)
            return m_cache->remover;
    }
    const auto loaded = std::make_shared<BackgroundRemover>(modelPath);
    {
        QMutexLocker lock(&m_cache->mutex);
        if (m_cache->remover && m_cache->loadedPath == modelPath)
            return m_cache->remover;
        m_cache->remover = loaded;
        m_cache->loadedPath = modelPath;
    }
    return loaded;
}

void InferenceWorker::removeBackground(const QImage& img, const ProcessingOptions& options, const QString& modelPath)
{
    const QString errModel = TR("modelo indisponível", "model unavailable");
    const QString errOnnx = TR("Erro ONNX: ", "ONNX error: ");
    const QString errGeneric = TR("Falha: ", "Failure: ");
    const QString errUnknown = TR("Erro desconhecido durante a inferência.", "Unknown error during inference.");

    const quint64 generation = m_generation;
    const auto remover = obtainRemover(modelPath);
    m_currentRemover = remover;

    m_watcher.setFuture(QtConcurrent::run([remover, img, options, generation, errModel, errOnnx, errGeneric, errUnknown]() -> SingleResult {
        try {
            if (!remover)
                return { QImage(), errModel, generation };
            return { ImagePostProcessor::apply(remover->removeBackground(img), options), QString(), generation };
        } catch (const Ort::Exception& e) {
            return { QImage(), errOnnx + QString::fromUtf8(e.what()), generation };
        } catch (const std::exception& e) {
            const QString msg = QString::fromUtf8(e.what());
            if (msg.contains("cancelada", Qt::CaseInsensitive) || msg.contains("cancelled", Qt::CaseInsensitive)) {
                return { QImage(), TR("Inferência cancelada pelo usuário.", "Inference cancelled by user."), generation };
            }
            return { QImage(), errGeneric + msg, generation };
        } catch (...) {
            return { QImage(), errUnknown, generation };
        }
    }));
}
