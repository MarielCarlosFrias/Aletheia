#include "batchtabcontroller.h"

BatchTabController::BatchTabController(QObject* parent)
    : QObject(parent)
{
}

BatchTabController::~BatchTabController() {}

void BatchTabController::processBatch() {}
void BatchTabController::processBatchStep() {}
void BatchTabController::onBatchStepFinished() {}
void BatchTabController::selectFolder() {}
void BatchTabController::selectOutputFolder() {}
