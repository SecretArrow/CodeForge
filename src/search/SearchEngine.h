#pragma once
// Asynchronous project-wide search (literal / regex / whole-word) with
// replace. Runs on a worker thread; UI never blocks. Results are batched
// per file. Replace applies in-buffer for open documents and on disk for
// closed ones.
#include <QObject>
#include <QRegularExpression>
#include <QStringList>
#include <QThread>

namespace cf {

struct SearchQuery {
    QString text;
    bool isRegex = false;
    bool caseSensitive = false;
    bool wholeWord = false;
    QStringList includeGlobs;   // empty = all
    QStringList excludeGlobs;
    bool includeHidden = false;
    qint64 maxFileBytes = 10 * 1024 * 1024;
    int maxResultsPerFile = 1000;
    int maxFiles = 5000;
};

struct SearchHit {
    int line = 0;          // 0-based
    int colStart = 0;
    int colEnd = 0;
    QString lineText;      // clipped around match
};

struct FileResult {
    QString path;
    QVector<SearchHit> hits;
};

struct SearchStats {
    int filesScanned = 0;
    int filesMatched = 0;
    int totalMatches = 0;
    bool cancelled = false;
};

class SearchEngine : public QObject {
    Q_OBJECT
public:
    explicit SearchEngine(QObject* parent = nullptr);
    ~SearchEngine() override;

    void start(const SearchQuery& query, const QString& workspaceRoot, const QStringList& relativeFiles);
    void cancel();
    bool isRunning() const { return m_running; }

    // Replace across files; openDocs maps path -> QTextDocument* for in-buffer replace.
    void replaceAll(const SearchQuery& query, const QString& replacement,
                    const QString& workspaceRoot, const QStringList& relativeFiles,
                    const QHash<QString, void*>& openDocs);

signals:
    void fileResult(const cf::FileResult& result);
    void finished(const cf::SearchStats& stats);
    void replacedFile(const QString& path, int count);

private:
    void runSearch(SearchQuery query, QString root, QStringList files);
    void runReplace(SearchQuery query, QString replacement, QString root, QStringList files, QHash<QString, void*> openDocs);

    QThread m_thread;
    bool m_running = false;
};

// Free helpers shared with tests.
namespace search_detail {
int findMatchesInText(const QString& text, const SearchQuery& q, QVector<SearchHit>* out, int limit);
bool globMatches(const QString& relPath, const QStringList& globs);
bool globMatchesOne(const QString& pathOrName, const QString& glob);
}

}  // namespace cf

Q_DECLARE_METATYPE(cf::FileResult)
Q_DECLARE_METATYPE(cf::SearchStats)
