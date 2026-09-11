#include "core/TextDocument.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QPlainTextDocumentLayout>
#include <QTextDocument>

#include "core/FileUtils.h"
#include "core/Logger.h"
#include "settings/SettingsManager.h"
#include "syntax/LanguageRegistry.h"

namespace cf {

static int s_untitledCounter = 0;

TextDocument::TextDocument(QObject* parent)
    : QObject(parent), m_document(new QTextDocument(this))
{
    m_document->setDocumentLayout(new QPlainTextDocumentLayout(m_document));
    ++s_untitledCounter;
    m_untitledId = QStringLiteral("untitled:%1").arg(s_untitledCounter);
    connect(m_document, &QTextDocument::modificationChanged, this, &TextDocument::onModificationChanged);
}

QString TextDocument::docId() const
{
    return m_filePath.isEmpty() ? m_untitledId : m_filePath;
}

QString TextDocument::displayName() const
{
    if (!m_filePath.isEmpty()) return QFileInfo(m_filePath).fileName();
    return QStringLiteral("Untitled-%1").arg(m_untitledId.mid(9));
}

void TextDocument::setFilePath(const QString& path)
{
    if (m_filePath == path) return;
    m_filePath = path;
    emit pathChanged();
}

bool TextDocument::isDirty() const
{
    return m_document->isModified();
}

void TextDocument::markClean()
{
    m_document->setModified(false);
}

void TextDocument::setReadOnly(bool ro)
{
    if (m_readOnly == ro) return;
    m_readOnly = ro;
    m_document->setModified(false);  // read-only docs are never "dirty"
}

QString TextDocument::languageId() const
{
    return LanguageRegistry::instance().detectByPath(m_filePath);
}

bool TextDocument::existsOnDisk() const
{
    return !m_filePath.isEmpty() && QFileInfo::exists(m_filePath);
}

void TextDocument::onModificationChanged(bool modified)
{
    emit dirtyChanged(modified);
}

void TextDocument::snapshotDiskState()
{
    if (m_filePath.isEmpty()) return;
    const QFileInfo info(m_filePath);
    m_diskSize = info.size();
    m_diskModified = info.lastModified();
    m_diskHash.clear();
    // Hash small files so timestamp-equal changes are still detected.
    if (m_diskSize > 0 && m_diskSize <= 20 * 1024 * 1024) {
        QFile f(m_filePath);
        if (f.open(QIODevice::ReadOnly))
            m_diskHash = QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha1);
    }
}

bool TextDocument::hasExternalChange() const
{
    if (m_filePath.isEmpty()) return false;
    const QFileInfo info(m_filePath);
    if (!info.exists()) return true;   // deleted counts as external change
    if (m_diskSize != info.size()) return true;
    if (m_diskModified != info.lastModified()) {
        if (m_diskHash.isEmpty()) return true;  // large file: fall back to mtime
        QFile f(m_filePath);
        if (!f.open(QIODevice::ReadOnly)) return true;
        return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha1) != m_diskHash;
    }
    return false;
}

void TextDocument::finishLoad(const QString& text, const enc::Info& e, LineEndings eol, qint64 size, const QDateTime& mtime)
{
    m_encoding = e;
    m_eol = eol;
    m_diskSize = size;
    m_diskModified = mtime;
    m_document->setPlainText(text);   // editor text is normalized to \n
    m_document->setModified(false);
    m_diskHash.clear();
    if (m_filePath.isEmpty()) return;
    QFile f(m_filePath);
    if (f.open(QIODevice::ReadOnly) && size <= 20 * 1024 * 1024)
        m_diskHash = QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha1);
}

bool TextDocument::load(QString* error)
{
    if (m_filePath.isEmpty()) return false;
    QByteArray bytes;
    if (!fs::readAll(m_filePath, bytes, error)) return false;

    const enc::Info e = enc::detect(bytes);
    bool ok = true;
    QString text = enc::decode(bytes, e, &ok);
    if (!ok) CF_LOG_WARNING(QStringLiteral("Document: substitution while decoding %1").arg(m_filePath));

    const LineEndings eol = enc::detectLineEndings(text, LineEndings::Lf);
    // Normalize to \n for editing (serializes back with original EOL on save).
    text.replace(QLatin1String("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    finishLoad(text, e, eol, bytes.size(), QFileInfo(m_filePath).lastModified());
    return true;
}

QString TextDocument::serializeText(QString* error) const
{
    Q_UNUSED(error);
    QString text = m_document->toPlainText();

    // Trailing whitespace / final newline options (settings-driven, applied on save).
    SettingsManager& s = SettingsManager::instance();
    if (s.getBool(QStringLiteral("files.trimTrailingWhitespace"))) {
        QStringList lines = text.split(QLatin1Char('\n'));
        for (QString& line : lines) {
            int end = line.size();
            while (end > 0 && (line.at(end - 1) == u' ' || line.at(end - 1) == u'\t')) --end;
            if (end != line.size()) line.truncate(end);
        }
        text = lines.join(QLatin1Char('\n'));
    }
    if (s.getBool(QStringLiteral("files.insertFinalNewline")) && !text.isEmpty() && !text.endsWith(QLatin1Char('\n')))
        text += QLatin1Char('\n');

    // Convert internal \n to the document's EOL convention.
    const QString eolStr = eolCharacters(m_eol);
    if (eolStr != QStringLiteral("\n"))
        text.replace(QLatin1Char('\n'), eolStr);
    return text;
}

bool TextDocument::save(QString* error)
{
    if (m_filePath.isEmpty()) { if (error) *error = QStringLiteral("No file path"); return false; }
    return saveAs(m_filePath, error);
}

bool TextDocument::saveAs(const QString& path, QString* error)
{
    const QString text = serializeText(error);
    bool ok = true;
    const QByteArray bytes = enc::encode(text, m_encoding, &ok);
    if (!ok) CF_LOG_WARNING(QStringLiteral("Document: characters replaced while encoding %1").arg(path));

    if (!fs::writeAllAtomic(path, bytes, error)) return false;

    m_filePath = path;
    m_saving = true;
    m_document->setModified(false);
    m_saving = false;
    snapshotDiskState();
    emit pathChanged();
    emit saved();
    CF_LOG_INFO(QStringLiteral("Document: saved %1 (%2 bytes)").arg(path).arg(bytes.size()));
    return true;
}

void TextDocument::reloadFromDisk()
{
    QString err;
    load(&err);
}

}  // namespace cf
