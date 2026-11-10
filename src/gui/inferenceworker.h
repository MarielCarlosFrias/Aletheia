#ifndef INFERENCEWORKER_H
#define INFERENCEWORKER_H

#include <QObject>
#include <QFutureWatcher>
#include <QImage>
#include <memory>
#include <QMutex>

class BackgroundRemover;
class ProcessingOptions;

struct SingleResult {
    QImage image;
    QString error;
    quint64 generation = 0;
};

class ModelCache {
public:
    QMutex mutex;
    std::shared_ptr<BackgroundRemover> remover;
    QString loadedPath;
};

class InferenceWorker : public QObject {
    Q_OBJECT
public:
    explicit InferenceWorker(std::shared_ptr<ModelCache> cache, QObject* parent = nullptr);
    ~InferenceWorker();

    void setGeneration(quint64 generation) { m_generation = generation; }
    void cancel();

    void removeBackground(const QImage& img, const ProcessingOptions& options, const QString& modelPath);

signals:
    void finished(const SingleResult& result);

private:
    std::shared_ptr<BackgroundRemover> obtainRemover(const QString& modelPath);
    std::shared_ptr<ModelCache> m_cache;
    QFutureWatcher<SingleResult> m_watcher;
    quint64 m_generation = 0;
    std::shared_ptr<BackgroundRemover> m_currentRemover;
};

#endif // INFERENCEWORKER_H
