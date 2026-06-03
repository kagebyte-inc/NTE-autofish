#include "I18nCatalog.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QString normalizeLanguage(const QString &language)
{
    if (language == QStringLiteral("ru")
        || language == QStringLiteral("ja")
        || language == QStringLiteral("zh_CN")) {
        return language;
    }
    return QStringLiteral("en");
}
} // namespace

I18nCatalog::I18nCatalog(QObject *parent)
    : QObject(parent)
{
    ensureLoaded(QStringLiteral("en"));
}

QString I18nCatalog::language() const
{
    return m_language;
}

int I18nCatalog::generation() const
{
    return m_generation;
}

void I18nCatalog::setLanguage(const QString &language)
{
    const QString normalized = normalizeLanguage(language);
    if (m_language == normalized) {
        return;
    }
    m_language = normalized;
    ensureLoaded(m_language);
    ++m_generation;
    emit languageChanged();
}

QString I18nCatalog::textFor(const QString &key) const
{
    const auto &active = catalogFor(m_language);
    const auto it = active.constFind(key);
    if (it != active.constEnd()) {
        return it.value();
    }

    const auto &english = catalogFor(QStringLiteral("en"));
    const auto englishIt = english.constFind(key);
    if (englishIt != english.constEnd()) {
        return englishIt.value();
    }

    return key;
}

QString I18nCatalog::format(const QString &key, const QString &arg) const
{
    return textFor(key).arg(arg);
}

QString I18nCatalog::statusText(const QString &value) const
{
    const QString key = QStringLiteral("status.%1").arg(value);
    const QString translated = textFor(key);
    return translated == key ? value.toUpper() : translated;
}

const QHash<QString, QString> &I18nCatalog::catalogFor(const QString &language) const
{
    ensureLoaded(language);
    return m_catalogs[normalizeLanguage(language)];
}

void I18nCatalog::ensureLoaded(const QString &language) const
{
    const QString normalized = normalizeLanguage(language);
    if (m_catalogs.contains(normalized)) {
        return;
    }

    QFile file(QStringLiteral(":/qt/qml/Autofish/qml/i18n/%1.json").arg(normalized));
    QHash<QString, QString> entries;
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        const QJsonObject object = document.object();
        entries.reserve(object.size());
        for (auto it = object.begin(); it != object.end(); ++it) {
            entries.insert(it.key(), it.value().toString());
        }
    }

    m_catalogs.insert(normalized, entries);
}