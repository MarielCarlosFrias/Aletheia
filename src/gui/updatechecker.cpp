#include "updatechecker.h"
#include "i18n.h"

#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#ifndef ALETHEIA_VERSION
#define ALETHEIA_VERSION "1.0.0"
#endif

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent),
      banner_(new QLabel(nullptr))
{
    banner_->setVisible(false);
    banner_->setObjectName("updateBanner");
    banner_->setWordWrap(true);
    banner_->setTextFormat(Qt::RichText);
    banner_->setOpenExternalLinks(false);
    banner_->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    banner_->setCursor(Qt::PointingHandCursor);
}

void UpdateChecker::check()
{
    // Uma vez por sessao, assincrono e silencioso.
    if (qEnvironmentVariableIsSet("ALETHEIA_NO_UPDATE_CHECK") ||
        qEnvironmentVariableIsSet("BG_REMOVER_NO_UPDATE_CHECK")) return;
    if (!QSettings("aletheia", "aletheia")
             .value("update/check", true).toBool()) {
        return;
    }
    if (nam_) return;
    nam_ = new QNetworkAccessManager(this);

    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.github.com/repos/MarielCarlosFrias/aletheia"
        "/releases/latest")));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent",
                         QByteArray("aletheia/") + ALETHEIA_VERSION);
    request.setTransferTimeout(5000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = nam_->get(request);
    connect(reply, &QNetworkReply::finished, this,
            &UpdateChecker::onReplyFinished);
}

void UpdateChecker::onReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // Offline ou rate limit: sigiloso.
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isObject()) {
        latestRemoteVersion_ =
            doc.object().value("tag_name").toString();
    }
    if (latestRemoteVersion_.isEmpty()) return;

    // Comparacao semantica simples (major.minor.patch), ignorando o "v".
    const QString remote = latestRemoteVersion_.startsWith('v')
        ? latestRemoteVersion_.mid(1) : latestRemoteVersion_;
    const QString local = QStringLiteral(ALETHEIA_VERSION);
    auto parse = [](const QString& s) {
        const QStringList parts = s.split('.');
        QList<int> nums;
        for (const QString& p : parts) nums << p.toInt();
        while (nums.size() < 3) nums << 0;
        return nums;
    };
    const QList<int> r = parse(remote);
    const QList<int> l = parse(local);

    if (r.at(0) > l.at(0)
        || (r.at(0) == l.at(0) && r.at(1) > l.at(1))
        || (r.at(0) == l.at(0) && r.at(1) == l.at(1) && r.at(2) > l.at(2))) {
        emit updateAvailable(latestRemoteVersion_);
    }
}
