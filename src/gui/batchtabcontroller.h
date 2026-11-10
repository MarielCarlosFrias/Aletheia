#ifndef BATCHTABCONTROLLER_H
#define BATCHTABCONTROLLER_H

#include <QObject>

class BatchTabController : public QObject {
    Q_OBJECT
public:
    explicit BatchTabController(QObject* parent = nullptr);
    ~BatchTabController();

public slots:
    void processBatch();
    void processBatchStep();
    void onBatchStepFinished();
    void selectFolder();
    void selectOutputFolder();

signals:
    void batchFinished();
};

#endif // BATCHTABCONTROLLER_H
