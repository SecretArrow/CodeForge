#include "syntax/LanguageRegistry.h"

#include "core/Common.h"

namespace cf {

LanguageRegistry& LanguageRegistry::instance()
{
    static LanguageRegistry reg;
    return reg;
}

LanguageRegistry::LanguageRegistry()
{
    add(QStringLiteral("cpp"),        QStringLiteral("C++"),        { QStringLiteral("cpp"), QStringLiteral("cc"), QStringLiteral("cxx"), QStringLiteral("c++"), QStringLiteral("hpp"), QStringLiteral("hh"), QStringLiteral("hxx"), QStringLiteral("h"), QStringLiteral("inl"), QStringLiteral("ipp") });
    add(QStringLiteral("c"),          QStringLiteral("C"),          { QStringLiteral("c") });
    add(QStringLiteral("csharp"),     QStringLiteral("C#"),         { QStringLiteral("cs") });
    add(QStringLiteral("java"),       QStringLiteral("Java"),       { QStringLiteral("java") });
    add(QStringLiteral("javascript"), QStringLiteral("JavaScript"), { QStringLiteral("js"), QStringLiteral("jsx"), QStringLiteral("mjs"), QStringLiteral("cjs") });
    add(QStringLiteral("typescript"), QStringLiteral("TypeScript"), { QStringLiteral("ts"), QStringLiteral("tsx") });
    add(QStringLiteral("python"),     QStringLiteral("Python"),     { QStringLiteral("py"), QStringLiteral("pyw"), QStringLiteral("pyi") });
    add(QStringLiteral("rust"),       QStringLiteral("Rust"),       { QStringLiteral("rs") });
    add(QStringLiteral("go"),         QStringLiteral("Go"),         { QStringLiteral("go") });
    add(QStringLiteral("php"),        QStringLiteral("PHP"),        { QStringLiteral("php"), QStringLiteral("phtml") });
    add(QStringLiteral("html"),       QStringLiteral("HTML"),       { QStringLiteral("html"), QStringLiteral("htm"), QStringLiteral("xhtml") });
    add(QStringLiteral("css"),        QStringLiteral("CSS"),        { QStringLiteral("css"), QStringLiteral("scss"), QStringLiteral("less") });
    add(QStringLiteral("json"),       QStringLiteral("JSON"),       { QStringLiteral("json"), QStringLiteral("jsonc") });
    add(QStringLiteral("xml"),        QStringLiteral("XML"),        { QStringLiteral("xml"), QStringLiteral("svg"), QStringLiteral("xsl"), QStringLiteral("xslt"), QStringLiteral("csproj"), QStringLiteral("props"), QStringLiteral("config"), QStringLiteral("plist") });
    add(QStringLiteral("yaml"),       QStringLiteral("YAML"),       { QStringLiteral("yml"), QStringLiteral("yaml") });
    add(QStringLiteral("markdown"),   QStringLiteral("Markdown"),   { QStringLiteral("md"), QStringLiteral("markdown") });
    add(QStringLiteral("sql"),        QStringLiteral("SQL"),        { QStringLiteral("sql") });
    add(QStringLiteral("shell"),      QStringLiteral("Shell"),      { QStringLiteral("sh"), QStringLiteral("bash"), QStringLiteral("zsh") }, { QStringLiteral("Makefile"), QStringLiteral("Dockerfile"), QStringLiteral(".bashrc"), QStringLiteral(".zshrc") });
    add(QStringLiteral("powershell"), QStringLiteral("PowerShell"), { QStringLiteral("ps1"), QStringLiteral("psm1"), QStringLiteral("psd1") });
    add(QStringLiteral("cmake"),      QStringLiteral("CMake"),      { QStringLiteral("cmake") }, { QStringLiteral("CMakeLists.txt") });
    add(QStringLiteral("ini"),        QStringLiteral("INI"),        { QStringLiteral("ini"), QStringLiteral("cfg"), QStringLiteral("conf"), QStringLiteral("editorconfig") });
    add(QStringLiteral("toml"),       QStringLiteral("TOML"),       { QStringLiteral("toml") }, { QStringLiteral("Cargo.lock") });
}

void LanguageRegistry::add(const QString& id, const QString& name, const QStringList& exts, const QStringList& files)
{
    Language lang;
    lang.id = id;
    lang.name = name;
    lang.extensions = exts;
    lang.filenames = files;
    m_languages.append(lang);
    for (const QString& e : exts) m_byExtension.insert(e, id);
    for (const QString& f : files) m_byFilename.insert(f, id);
}

QString LanguageRegistry::detectByPath(const QString& path) const
{
    if (path.isEmpty()) return QString();
    const QString name = QFileInfo(path).fileName();

    const auto fit = m_byFilename.constFind(name);
    if (fit != m_byFilename.constEnd()) return fit.value();

    const QString ext = fileExtensionOf(name);
    const auto eit = m_byExtension.constFind(ext);
    if (eit != m_byExtension.constEnd()) return eit.value();
    return QString();
}

QString LanguageRegistry::languageName(const QString& id) const
{
    for (const Language& l : m_languages)
        if (l.id == id) return l.name;
    return QStringLiteral("Plain Text");
}

bool LanguageRegistry::hasBraceFolding(const QString& id) const
{
    return id == QStringLiteral("cpp") || id == QStringLiteral("c") || id == QStringLiteral("csharp") ||
           id == QStringLiteral("java") || id == QStringLiteral("javascript") || id == QStringLiteral("typescript") ||
           id == QStringLiteral("rust") || id == QStringLiteral("go") || id == QStringLiteral("php") ||
           id == QStringLiteral("json") || id == QStringLiteral("css") || id == QStringLiteral("scss");
}

bool LanguageRegistry::usesIndentFolding(const QString& id) const
{
    return id == QStringLiteral("python") || id == QStringLiteral("yaml");
}

}  // namespace cf
