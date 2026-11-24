#include "i18n.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>
#include <QStringList>

namespace I18n {
namespace {

// Cache do dicionario carregado. O acesso acontece normalmente na thread
// da interface (e, no CLI, antes de arrancar os workers), mas o mutex
// garante que uma chamada simultanea de uma thread de trabalho nunca
// le o hash enquanto outra o esta a reconstruir.
QMutex g_dictionaryMutex;
QHash<QString, QString> g_dictionary;
bool g_dictionaryTried = false;
Lang g_dictionaryLang = Lang::En;

QString normalizedCode(const QString& raw)
{
    QString value = raw.trimmed().toLower();
    value.replace(QLatin1Char('-'), QLatin1Char('_'));
    return value;
}

QString envLang()
{
    QString env = qEnvironmentVariable("ALETHEIA_LANG").trimmed();
    if (env.isEmpty()) env = qEnvironmentVariable("BG_REMOVER_LANG").trimmed();
    return env;
}

bool isKnownCode(const QString& value)
{
    const QString prefix = normalizedCode(value).section(QLatin1Char('_'), 0, 0);
    return prefix == QLatin1String("pt")
        || prefix == QLatin1String("en")
        || prefix == QLatin1String("es");
}

// Locais onde o dicionario pode estar, por ordem de preferencia. O
// recurso embutido (:...) e o caminho normal nas builds de pacote; os
// restantes permitem testar/instalar traducoes sem recompilar.
QStringList dictionaryCandidates(const QString& languageCode)
{
    QStringList candidates;
    const QString fileName =
        QStringLiteral("aletheia_%1.json").arg(languageCode);
    candidates << QStringLiteral(":/i18n/") + fileName;

    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        candidates << appDir + QStringLiteral("/translations/") + fileName;
        candidates << appDir + QStringLiteral("/../share/aletheia/translations/")
                          + fileName;
        // Bundle macOS: Contents/MacOS -> Contents/Resources.
        candidates << appDir + QStringLiteral("/../Resources/translations/")
                          + fileName;
    }
    return candidates;
}

// O mutex tem de estar adquirido por quem chama esta funcao.
bool loadDictionaryLocked(Lang language)
{
    if (g_dictionaryTried && g_dictionaryLang == language)
        return !g_dictionary.isEmpty();
    if (g_dictionaryTried && g_dictionaryLang != language) {
        g_dictionary.clear();
        g_dictionaryTried = false;
    }

    g_dictionaryTried = true;
    g_dictionaryLang = language;
    g_dictionary.clear();
    if (language == Lang::PtBr || language == Lang::En) return false;

    const QString wanted = code(language);
    const QStringList candidates = dictionaryCandidates(wanted);
    for (const QString& path : candidates) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject()) continue;

        const QJsonObject obj = doc.object();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            if (it.value().isString()) {
                g_dictionary.insert(it.key(), it.value().toString());
            }
        }
        if (!g_dictionary.isEmpty()) return true;
    }
    return false;
}

} // namespace

QString code(Lang language)
{
    switch (language) {
    case Lang::PtBr: return QStringLiteral("pt");
    case Lang::Es:   return QStringLiteral("es");
    case Lang::En:
    default:         return QStringLiteral("en");
    }
}

Lang fromCode(const QString& value, Lang fallback)
{
    const QString normalized = normalizedCode(value);
    if (normalized.isEmpty()) return fallback;

    const QString prefix = normalized.section(QLatin1Char('_'), 0, 0);
    if (prefix == QLatin1String("pt")) return Lang::PtBr;
    if (prefix == QLatin1String("en")) return Lang::En;
    if (prefix == QLatin1String("es")) return Lang::Es;
    return fallback;
}

Lang detectLang()
{
    const QString env = envLang();
    if (!env.isEmpty() && isKnownCode(env)) return fromCode(env);

    QSettings settings("aletheia", "aletheia");
    QString saved = settings.value("lang").toString();
    if (saved.isEmpty()) {
        QSettings legacySettings("bg-remover", "bg-remover");
        saved = legacySettings.value("lang").toString();
    }
    if (!saved.isEmpty() && isKnownCode(saved)) return fromCode(saved);

    switch (QLocale::system().language()) {
    case QLocale::Portuguese: return Lang::PtBr;
    case QLocale::Spanish:    return Lang::Es;
    case QLocale::English:    return Lang::En;
    default:                  return Lang::En;
    }
}

Lang lang()
{
    static const Lang cached = detectLang();
    return cached;
}

QString nativeName(Lang language)
{
    switch (language) {
    case Lang::PtBr: return QStringLiteral("Portugu\u00eas");
    case Lang::Es:   return QStringLiteral("Espa\u00f1ol");
    case Lang::En:
    default:         return QStringLiteral("English");
    }
}

bool dictionaryAvailable(Lang language)
{
    if (language == Lang::PtBr || language == Lang::En) return true;

    QMutexLocker locker(&g_dictionaryMutex);
    return loadDictionaryLocked(language);
}

QString tr(const char* ptBr, const char* en)
{
    const Lang active = lang();
    if (active == Lang::PtBr) return QString::fromUtf8(ptBr);
    if (active == Lang::En) return QString::fromUtf8(en);

    QMutexLocker locker(&g_dictionaryMutex);
    loadDictionaryLocked(active);
    const QString key = QString::fromUtf8(ptBr);
    const auto it = g_dictionary.constFind(key);
    if (it != g_dictionary.constEnd() && !it.value().isEmpty()) return it.value();

    // Sem traducao (ou dicionario em falta): EN e sempre preferivel a
    // mostrar a chave em PT num idioma estrangeiro.
    return QString::fromUtf8(en);
}

} // namespace I18n
