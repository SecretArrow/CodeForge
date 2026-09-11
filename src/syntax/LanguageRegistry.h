#pragma once
// Language registry: extension/filename -> language detection for the
// syntax highlighting engine. Extensible by adding entries to languages().
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace cf {

struct Language {
    QString id;
    QString name;
    QStringList extensions;   // lowercase, no dot ("cpp", "h")
    QStringList filenames;    // exact names ("CMakeLists.txt", "Makefile")
};

class LanguageRegistry {
public:
    static LanguageRegistry& instance();

    const QList<Language>& languages() const { return m_languages; }
    QString detectByPath(const QString& path) const;          // returns language id or ""
    QString languageName(const QString& id) const;
    bool hasBraceFolding(const QString& id) const;            // brace vs indent folding
    bool usesIndentFolding(const QString& id) const;

private:
    LanguageRegistry();
    void add(const QString& id, const QString& name, const QStringList& exts, const QStringList& files = {});
    QList<Language> m_languages;
    QHash<QString, QString> m_byExtension;
    QHash<QString, QString> m_byFilename;
};

}  // namespace cf
