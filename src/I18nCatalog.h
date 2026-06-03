#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class I18nCatalog final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(int generation READ generation NOTIFY languageChanged)

public:
    explicit I18nCatalog(QObject *parent = nullptr);

    QString language() const;
    void setLanguage(const QString &language);
    int generation() const;

    Q_INVOKABLE QString textFor(const QString &key) const;
    Q_INVOKABLE QString format(const QString &key, const QString &arg) const;
    Q_INVOKABLE QString statusText(const QString &value) const;

signals:
    void languageChanged();

private:
    const QHash<QString, QString> &catalogFor(const QString &language) const;
    void ensureLoaded(const QString &language) const;

    QString m_language = QStringLiteral("en");
    int m_generation = 0;
    mutable QHash<QString, QHash<QString, QString>> m_catalogs;
};