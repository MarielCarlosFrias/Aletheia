#include "inferenceworker.h"
#include "backgroundremover.h"
#include "imagepostprocessor.h"
#include "i18n.h"

#include <QtConcurrent/QtConcurrent>

InferenceWorker::InferenceWorker(QObject* parent)
    : QObject(parent),
      cache_(std::make_shared<ModelCache>())
{
    connect(&watcher_, &QFutureWatcher<InferenceResult>::finished,
            this, [this]() {
        // result() volta a lancar se a thread lancou uma excecao; a
        // propria thread de trabalho ja apanha tudo, por isso aqui
        // nunca deve falhar.
        emit finished(watcher_.result());
    });
}

bool InferenceWorker::isRunning() const
{
    return watcher_.isRunning();
}

void InferenceWorker::resetModel()
{
    QMutexLocker lock(&cache_->mutex);
    cache_->remover.reset();
    cache_->loadedPath.clear();
}

void InferenceWorker::cancel()
{
    std::shared_ptr<BackgroundRemover> remover;
    {
        QMutexLocker lock(&cache_->mutex);
        remover = cache_->remover;
    }
    if (remover) {
        remover->requestCancel();
    }
}

std::shared_ptr<BackgroundRemover> InferenceWorker::obtainRemover(
    const std::shared_ptr<ModelCache>& cache,
    const QString& modelPath,
    QString* errorMessage)
{
    if (errorMessage) errorMessage->clear();
    if (!cache) return nullptr;
    {
        QMutexLocker lock(&cache->mutex);
        if (cache->remover && cache->loadedPath == modelPath)
            return cache->remover;
    }

    // Carrega fora do lock: a GUI pode pedir resetModel() enquanto o
    // ONNX carrega (segundos) sem ficar bloqueada.
    std::shared_ptr<BackgroundRemover> loaded;
    try {
        loaded = std::make_shared<BackgroundRemover>(modelPath);
    } catch (const Ort::Exception& e) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(e.what());
        return nullptr;
    } catch (const std::exception& e) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8(e.what());
        return nullptr;
    }

    {
        QMutexLocker lock(&cache->mutex);
        if (cache->remover && cache->loadedPath == modelPath)
            return cache->remover;
        cache->remover = loaded;
        cache->loadedPath = modelPath;
    }
    return loaded;
}

void InferenceWorker::start(const QImage& image,
                            const ProcessingOptions& options,
                            const QString& modelPath,
                            quint64 generation)
{
    // Mensagens traduzidas antes da thread: I18n le QSettings, que nao
    // deve ser usado fora da thread principal.
    const QString errModel = TR("modelo indisponível", "model unavailable");
    const QString errOnnx = TR("Erro ONNX: ", "ONNX error: ");
    const QString errGeneric = TR("Falha: ", "Failure: ");
    const QString errUnknown = TR("Erro desconhecido durante a inferência.",
                                  "Unknown error during inference.");
    const QString errCancelled = TR("Inferência cancelada pelo usuário.",
                                    "Inference cancelled by user.");

    watcher_.setFuture(QtConcurrent::run(
        [cache = cache_, img = image, options, modelPath, generation,
         errModel, errOnnx, errGeneric, errUnknown, errCancelled]()
            -> InferenceResult {
            try {
                QString modelError;
                const std::shared_ptr<BackgroundRemover> remover =
                    InferenceWorker::obtainRemover(cache, modelPath,
                                                 &modelError);
                if (!remover) {
                    return { QImage(),
                             modelError.isEmpty()
                                 ? errModel
                                 : errModel + QStringLiteral(": ") + modelError,
                             generation };
                }
                return { ImagePostProcessor::apply(
                             remover->removeBackground(img), options),
                         QString(), generation };
            } catch (const Ort::Exception& e) {
                return { QImage(),
                         errOnnx + QString::fromUtf8(e.what()),
                         generation };
            } catch (const std::exception& e) {
                const QString msg = QString::fromUtf8(e.what());
                if (msg.contains("cancelada", Qt::CaseInsensitive)
                    || msg.contains("cancelled", Qt::CaseInsensitive)) {
                    return { QImage(), errCancelled, generation };
                }
                return { QImage(), errGeneric + msg, generation };
            } catch (...) {
                // Uma excecao desconhecida nao pode escapar do
                // QtConcurrent: result() voltaria a lancar na GUI.
                return { QImage(), errUnknown, generation };
            }
        }));
}
