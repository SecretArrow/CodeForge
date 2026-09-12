#pragma once
// DocumentManager: registry of open documents, open/close/save orchestration,
// autosave, recovery snapshots (encrypted), external-change coordination.
// UI prompts are injected as callbacks so core stays headless (testable).
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

#include "core/TextDocument.h"
#include "settings/SettingsManager.h"

namespace cf {

class DocumentManager : public QObject {
    Q_OBJECT
public:
    static DocumentManager& instance();

    explicit DocumentManager(QObject* parent = nullptr);

    // ---- prompts wired by MainWindow (return user's decision) ----
    enum class SaveDecision { Save, Discard, Cancel };
    enum class LargeFileDecision { Normal, ReadOnly, Cancel };

    std::function<SaveDecision(TextDocument*)> promptSaveBeforeClose;       // dirty on close
    std::function<LargeFileDecision(qint64 bytes)> promptLargeFile;         // big file open
    std::function<void(const QString&)> showError;                          // friendly error box
    std::function<int(TextDocument*)> promptExternalChange;                 // 0=Reload 1=Keep 2=Compare

    // ---- lifecycle ----
    TextDocument* openDocument(const QString& path, bool readOnly = false, QString* error = nullptr);
    TextDocument* createUntitled();
    bool closeDocument(TextDocument* doc);            // asks to save when dirty; false if cancelled
    void closeAllDocuments();                         // best effort (used on quit; session handles prompts)
    QList<TextDocument*> documents() const;
    TextDocument* documentByPath(const QString& path) const;
    TextDocument* documentById(const QString& docId) const;

    // ---- save ----
    bool saveDocument(TextDocument* doc, QString* error);
    bool saveDocumentAs(TextDocument* doc, const QString& path, QString* error);
    int saveAllModified();                            // returns count saved

    // ---- autosave ----
    void configureAutosave();
    void autosaveOnFocusChange();
    bool isAutosaveOnFocusChange() const
    {
        return SettingsManager::instance().getString(QStringLiteral("files.autosave")) == QLatin1String("onFocusChange");
    }

    // ---- recovery (encrypted) ----
    void writeRecoverySnapshots();                    // called periodically + on commitData
    void clearRecoveryFor(TextDocument* doc);
    struct RecoveryEntry {
        QString docId;
        QString path;            // may be empty (untitled)
        QString untitledId;
        QString title;
    };
    QList<RecoveryEntry> listRecoveryEntries() const;
    // Restores content into doc; returns false on decrypt failure.
    bool restoreRecoveryInto(TextDocument* doc);
    void discardRecoveryEntry(const QString& docId);

signals:
    void documentOpened(cf::TextDocument* doc);
    void documentClosed(const QString& docId);
    void documentSaved(cf::TextDocument* doc);
    void dirtyChanged(cf::TextDocument* doc, bool dirty);
    void externalChangeDetected(cf::TextDocument* doc);   // after user chose "keep"
    void requestCompareWithDisk(cf::TextDocument* doc);   // user chose Compare

public slots:
    void checkExternalChanges();                      // invoked by FileWatcher (debounced)

private:
    QString recoveryFilePath(const QString& docId) const;
    void writeRecoveryFor(TextDocument* doc);
    void connectDoc(TextDocument* doc);

    QVector<QPointer<TextDocument>> m_documents;
    QTimer m_autosaveTimer;
    QTimer m_recoveryTimer;
};

}  // namespace cf
