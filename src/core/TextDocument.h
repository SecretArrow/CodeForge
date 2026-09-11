#pragma once
// TextDocument: one open document (buffer + file metadata + encoding/EOL).
// The actual text lives in a QTextDocument (efficient buffer for QPlainTextEdit);
// this class owns file I/O, encoding detection, line-ending handling, dirty
// state, and external-change detection.
#include <QDateTime>
#include <QObject>
#include <QString>

#include "core/Common.h"
#include "core/Encoding.h"

class QTextDocument;

namespace cf {

class TextDocument : public QObject {
    Q_OBJECT
public:
    explicit TextDocument(QObject* parent = nullptr);

    // ---- identity ----
    QString filePath() const { return m_filePath; }
    void setFilePath(const QString& path);
    bool isUntitled() const { return m_filePath.isEmpty(); }
    QString docId() const;            // path, or "untitled:<n>" for scratch buffers
    QString displayName() const;      // base name for tab titles

    // ---- content ----
    QTextDocument* document() const { return m_document; }
    bool isDirty() const;
    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool ro);

    // ---- metadata ----
    enc::Info encoding() const { return m_encoding; }
    void setEncoding(const enc::Info& e) { m_encoding = e; }
    LineEndings lineEndings() const { return m_eol; }
    void setLineEndings(LineEndings e) { m_eol = e; }
    QString languageId() const;       // resolved via LanguageRegistry ("" if unknown)

    // ---- disk state ----
    qint64 diskFileSize() const { return m_diskSize; }
    QDateTime diskModified() const { return m_diskModified; }
    bool existsOnDisk() const;

    // ---- I/O ----
    bool load(QString* error);
    bool save(QString* error);
    bool saveAs(const QString& path, QString* error);
    void reloadFromDisk();
    void markClean();

    // ---- external change detection ----
    void snapshotDiskState();
    bool hasExternalChange() const;   // mtime/size (or hash) differs from snapshot

signals:
    void dirtyChanged(bool dirty);
    void saved();
    void pathChanged();
    void externalChanged();           // file on disk changed (not by us)
    void deletedOnDisk();

private:
    void finishLoad(const QString& text, const enc::Info& e, LineEndings eol, qint64 size, const QDateTime& mtime);
    void onModificationChanged(bool modified);
    QString serializeText(QString* error) const;   // applies EOL + trim settings

    QTextDocument* m_document;
    QString m_filePath;
    QString m_untitledId;
    enc::Info m_encoding;
    LineEndings m_eol = LineEndings::Lf;
    bool m_readOnly = false;
    bool m_saving = false;

    qint64 m_diskSize = 0;
    QDateTime m_diskModified;
    QByteArray m_diskHash;            // small files only
};

}  // namespace cf
