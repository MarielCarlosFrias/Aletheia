#ifndef BATCHPROCESSOR_H
#define BATCHPROCESSOR_H

#include <QString>
#include <QStringList>
#include <atomic>
#include "processingoptions.h"

class BatchProcessor {
public:
    struct Result {
        int total = 0;
        int ok = 0;
        int errors = 0;
        QStringList errorMessages;
        // Erro que impede o lote de continuar (entrada/saida invalida,
        // falha ao carregar o modelo). O CLI usa isto para o exit code.
        bool fatal = false;
        // Verdadeiro se o lote foi interrompido por pedido externo
        // (sinal SIGINT no CLI).
        bool cancelled = false;
    };

    explicit BatchProcessor(ProcessingOptions options);

    Result processDirectory(
        const QString& inputDir,
        const QString& outputDir);

    // Aponta para uma flag externa de cancelamento (ex.: handler de
    // SIGINT no CLI). O lote para no proximo indice livre.
    void setCancelFlag(std::atomic<bool>* flag) { cancelFlag_ = flag; }

private:
    ProcessingOptions options_;
    std::atomic<bool>* cancelFlag_ = nullptr;
};

#endif
