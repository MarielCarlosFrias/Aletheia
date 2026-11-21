#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;

// Verificacao de atualizacoes (GitHub Releases API), assincrona e
// silenciosa: falha de rede nao incomoda quem trabalha offline.
// Extraido do MainWindow para reduzir o tamanho da janela principal.
//
// Uso: criar o UpdateChecker, inserir banner() no layout e chamar
// check(). Quando uma versao nova existe, emite updateAvailable().
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    // Inicia a verificacao (uma vez por sessao). Respeita as env vars
    // ALETHEIA_NO_UPDATE_CHECK / BG_REMOVER_NO_UPDATE_CHECK e a
    // preferencia QSettings "update/check".
    void check();

    // Widget do banner: inserir no layout onde deve aparecer.
    QLabel* banner() const { return banner_; }

    // Ultima versao remota detetada (vazia se nenhuma/erro).
    QString latestVersion() const { return latestRemoteVersion_; }

signals:
    // Emitido quando existe uma versao mais recente que a local.
    void updateAvailable(const QString& version);

private slots:
    void onReplyFinished();

private:
    QLabel* banner_;
    QNetworkAccessManager* nam_ = nullptr;
    QString latestRemoteVersion_;
};

#endif // UPDATECHECKER_H
