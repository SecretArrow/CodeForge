#include "search/SearchEngine.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextDocument>

#include "core/Encoding.h"
#include "core/FileUtils.h"

namespace cf {

// ---------- helpers ----------

namespace search_detail {

bool globMatchesOne(const QString& pathOrName, const QString& glob)
{
    if (glob.isEmpty()) return false;
    // Convert a glob to an anchored regex where '*' crosses path separators.
    // This keeps "build/**" and "**/.git/**" working on full relative paths.
    QString re;
    re.reserve(glob.size() * 2);
    for (const QChar c : glob) {
        switch (c.toLatin1()) {
        case '*':  re += QStringLiteral(".*"); break;
        case '?':  re += QLatin1Char('.'); break;
        default:   re += QRegularExpression::escape(c); break;
        }
    }
    static thread_local QHash<QString, QRegularExpression> cache;
    auto it = cache.find(re);
    if (it == cache.end()) {
        it = cache.insert(re, QRegularExpression(QRegularExpression::anchoredPattern(re)));
    }
    return it->match(pathOrName).hasMatch();
}

bool globMatches(const QString& relPath, const QStringList& globs)
{
    const QString name = QFileInfo(relPath).fileName();
    for (const QString& g : globs) {
        if (globMatchesOne(relPath, g) || globMatchesOne(name, g)) return true;
    }
    return false;
}

QRegularExpression buildRegex(const SearchQuery& q)
{
    QString pattern = q.isRegex ? q.text : QRegularExpression::escape(q.text);
    if (q.wholeWord && !q.isRegex)
        pattern = QStringLiteral("\\b%1\\b").arg(pattern);
    QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
    if (!q.caseSensitive) opts |= QRegularExpression::CaseInsensitiveOption;
    return QRegularExpression(pattern, opts);
}

int findMatchesInText(const QString& text, const SearchQuery& q, QVector<SearchHit>* out, int limit)
{
    if (q.text.isEmpty()) return 0;
    const QRegularExpression re = buildRegex(q);
    if (!re.isValid()) return 0;

    const QStringList lines = text.split(QLatin1Char('\n'));
    int count = 0;
    for (int i = 0; i < lines.size() && count < limit; ++i) {
        const QString& line = lines.at(i);
        QRegularExpressionMatchIterator it = re.globalMatch(line);
        while (it.hasNext() && count < limit) {
            const QRegularExpressionMatch m = it.next();
            SearchHit hit;
            hit.line = i;
            hit.colStart = m.capturedStart();
            hit.colEnd = m.capturedEnd();
            // Clip context around the match.
            const int pad = 48;
            int from = qMax(0, hit.colStart - pad);
            int to = qMin(line.size(), hit.colEnd + pad);
            hit.lineText = (from > 0 ? QStringLiteral("...") : QString()) + line.mid(from, to - from) + (to < line.size() ? QStringLiteral("...") : QString());
            if (out) out->append(hit);
            ++count;
        }
    }
    return count;
}

}  // namespace search_detail

using namespace search_detail;

// ---------- engine ----------

SearchEngine::SearchEngine(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<FileResult>("cf::FileResult");
    qRegisterMetaType<SearchStats>("cf::SearchStats");

    // The engine object itself lives on the worker thread; public API posts
    // queued invocations so UI calls are cheap and safe.
    moveToThread(&m_thread);
    m_thread.start();
}

SearchEngine::~SearchEngine()
{
    cancel();
    m_thread.quit();
    m_thread.wait(5000);
}

void SearchEngine::start(const SearchQuery& query, const QString& workspaceRoot, const QStringList& relativeFiles)
{
    QMetaObject::invokeMethod(this, [this, query, workspaceRoot, relativeFiles]() {
        runSearch(query, workspaceRoot, relativeFiles);
    }, Qt::QueuedConnection);
}

void SearchEngine::cancel()
{
    // cooperative cancellation via lambda capture of m_running (atomic via QMetaObject thread affinity)
    QMetaObject::invokeMethod(this, [this]() { m_running = false; }, Qt::QueuedConnection);
}

void SearchEngine::runSearch(SearchQuery query, QString root, QStringList files)
{
    m_running = true;
    SearchStats stats;

    const QRegularExpression re = buildRegex(query);
    if (!re.isValid()) {
        emit finished(stats);
        return;
    }

    for (const QString& rel : files) {
        if (!m_running) { stats.cancelled = true; break; }
        if (stats.filesScanned >= query.maxFiles) { stats.cancelled = true; break; }
        if (globMatches(rel, query.excludeGlobs)) continue;
        if (!query.includeGlobs.isEmpty() && !globMatches(rel, query.includeGlobs)) continue;

        const QString abs = root + QLatin1Char('/') + rel;
        QFileInfo info(abs);
        if (!info.isFile() || info.size() > query.maxFileBytes) continue;

        ++stats.filesScanned;

        QByteArray bytes;
        if (!fs::readAll(abs, bytes, nullptr)) continue;
        bool ok = true;
        const enc::Info e = enc::detect(bytes);
        const QString text = enc::decode(bytes, e, &ok);
        if (!ok || text.isEmpty()) continue;

        QVector<SearchHit> hits;
        const int n = findMatchesInText(text, query, &hits, query.maxResultsPerFile);
        if (n > 0) {
            stats.filesMatched += 1;
            stats.totalMatches += n;
            FileResult fr;
            fr.path = abs;
            fr.hits = hits;
            emit fileResult(fr);
        }
    }
    m_running = false;
    emit finished(stats);
}

void SearchEngine::replaceAll(const SearchQuery& query, const QString& replacement,
                              const QString& workspaceRoot, const QStringList& relativeFiles,
                              const QHash<QString, void*>& openDocs)
{
    QMetaObject::invokeMethod(this, [this, query, replacement, workspaceRoot, relativeFiles, openDocs]() {
        const QRegularExpression re = buildRegex(query);
        if (!re.isValid()) return;

        for (const QString& rel : relativeFiles) {
            if (globMatches(rel, query.excludeGlobs)) continue;
            if (!query.includeGlobs.isEmpty() && !globMatches(rel, query.includeGlobs)) continue;

            const QString abs = workspaceRoot + QLatin1Char('/') + rel;
            QFileInfo info(abs);
            if (!info.isFile() || info.size() > query.maxFileBytes) continue;

            auto* doc = static_cast<QTextDocument*>(openDocs.value(abs, nullptr));
            if (doc) {
                // In-buffer replace (preserves undo stack, marks dirty).
                QString text = doc->toPlainText();
                const int before = text.size();
                text.replace(re, replacement);
                if (text.size() != before || text != doc->toPlainText()) {
                    int count = 0;
                    {
                        QRegularExpressionMatchIterator it = re.globalMatch(doc->toPlainText());
                        while (it.hasNext()) { it.next(); ++count; }
                    }
                    doc->setPlainText(text);
                    emit replacedFile(abs, count);
                }
            } else {
                QByteArray bytes;
                if (!fs::readAll(abs, bytes, nullptr)) continue;
                const enc::Info e = enc::detect(bytes);
                const QString text = enc::decode(bytes, e);
                int count = 0;
                QRegularExpressionMatchIterator it = re.globalMatch(text);
                while (it.hasNext()) { it.next(); ++count; }
                if (count == 0) continue;

                QString replaced = text;
                replaced.replace(re, replacement);
                if (fs::writeAllAtomic(abs, enc::encode(replaced, e), nullptr)) {
                    emit replacedFile(abs, count);
                }
            }
        }
    }, Qt::QueuedConnection);
}

}  // namespace cf
