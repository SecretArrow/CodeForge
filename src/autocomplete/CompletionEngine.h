#pragma once
// CompletionEngine: IntelliSense-style autocomplete for CodeEditor.
//
// Completion sources, merged and ranked by filterRank():
//   1. document words   - [A-Za-z_][A-Za-z0-9_]+ scan of the buffer, cached in
//                         a QSet and refreshed with a 500 ms debounced timer on
//                         textChanged (skipped for buffers > 2,000,000 chars);
//   2. keywords         - per-language tables (setKeywords / auto-seeded from
//                         the document language id);
//   3. snippets         - set via setSnippets();
//   4. external provider- optional async source (e.g. LSP); stale results are
//                         dropped via a generation counter.
//
// The engine registers itself as an EditorKeyInterceptor on the editor: while
// the popup is open it consumes navigation/accept/escape keys, everything else
// falls through to the editor and the filter is re-synced with a queued
// invocation. When closed, Ctrl+Space triggers explicitly and a printable word
// character starts a 120 ms debounce (auto trigger).
#include <functional>

#include <QMetaType>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include "editor/CodeEditor.h"

class QKeyEvent;
class QTimer;
class QTextCursor;

namespace cf {

class CompletionPopup;

struct CompletionItem {
    QString label;        // shown in the popup (left)
    QString insertText;   // text inserted on accept (if empty -> label)
    QString detail;       // dim hint shown on the right ("keyword", "snippet: log", "function")
    int kind = 0;         // 0 text, 1 keyword, 2 function, 3 variable, 4 snippet, 5 class/type
    bool isSnippet = false;
    QString snippetBody;  // used when isSnippet
};

class CompletionEngine : public QObject, public EditorKeyInterceptor {
    Q_OBJECT
public:
    explicit CompletionEngine(CodeEditor* editor, QObject* parent = nullptr);
    ~CompletionEngine() override;                       // removes interceptor, hides popup

    void setKeywords(const QStringList& words);
    void setSnippets(const QVector<CompletionItem>& snippets);
    void setExternalProvider(                           // async source (e.g. LSP), results merged on arrival
        std::function<void(const QString& prefix, int line, int col,
                           std::function<void(QVector<CompletionItem>)>)> provider);
    void triggerNow();                                  // Ctrl+Space entry
    void closePopup();
    bool isPopupVisible() const;

    // pure, unit-tested helpers:
    static QString wordBeforeCursor(const QTextCursor& cursor);          // [A-Za-z0-9_]+ ending at cursor position
    static QVector<CompletionItem> filterRank(const QVector<CompletionItem>& items, const QString& prefix);
    static QStringList keywordsForLanguage(const QString& languageId);   // compact per-language keyword table

    bool editorKeyPress(CodeEditor* editor, QKeyEvent* e) override;

signals:
    void popupOpened();
    void itemActivated(const cf::CompletionItem& item);

private slots:
    void refreshFilter();       // recompute prefix + filtered list (queued after typing keys)
    void onDebounceTimeout();   // 120 ms auto-trigger after a typed word character
    void onWordScanTimeout();   // 500 ms debounced document word scan

private:
    void cycle(int delta);
    void cyclePage(int delta);
    bool expandSnippetAtCursor();
    void insertSnippetBody(QTextCursor c, const QString& body);
    void openPopup(const QString& prefix);
    QVector<CompletionItem> collectSources(const QString& prefix) const;
    void requestExternal(const QString& prefix);
    void onExternalResults(int generation, const QString& prefix, QVector<CompletionItem> results);
    void acceptCurrent();
    void acceptItem(const CompletionItem& item);
    void scheduleWordScan();
    void scanDocumentWords();

    QPointer<CodeEditor> m_editor;
    CompletionPopup* m_popup = nullptr;

    QVector<CompletionItem> m_keywordItems;
    QVector<CompletionItem> m_snippetItems;
    std::function<void(const QString&, int, int, std::function<void(QVector<CompletionItem>)>)> m_provider;

    QSet<QString> m_docWords;
    QTimer* m_debounceTimer = nullptr;   // 120 ms typing debounce
    QTimer* m_wordTimer = nullptr;       // 500 ms word-cache refresh
    QString m_prefix;                    // prefix the open popup was built for
    QVector<CompletionItem> m_filtered;  // items currently shown in the popup
    QVector<CompletionItem> m_external;  // last external-provider results (merged on open)
    QString m_externalPrefix;            // prefix the external request was issued for
    int m_generation = 0;                // bumped on open/close/refresh; stale async results dropped
    bool m_applying = false;             // true while inserting an accepted item
};

}  // namespace cf

Q_DECLARE_METATYPE(cf::CompletionItem)
