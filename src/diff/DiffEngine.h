#pragma once
// DiffEngine: pure line-diff (LCS based) producing side-by-side rows and
// hunks. Used by DiffViewer (compare files, git diff, external changes).
// No Qt GUI dependency so it is unit-testable.
#include <QString>
#include <QStringList>
#include <QVector>

namespace cf {

struct DiffRow {
    enum class Type { Equal, Delete, Add };
    Type type = Type::Equal;
    QString left;      // side A text ("" for Add rows)
    QString right;     // side B text ("" for Delete rows)
    int aLine = -1;    // 0-based line in A, -1 if none
    int bLine = -1;    // 0-based line in B, -1 if none
};

struct DiffHunk {
    int rowStart = 0;   // index into DiffEngine::rows()
    int rowCount = 0;
    int aStart = -1;    // 0-based start line in A (first non-equal row)
    int bStart = -1;    // 0-based start line in B
};

class DiffEngine {
public:
    // Split into lines; normalizes \r\n and \r to \n.
    static QStringList splitLines(const QString& text);

    // Compute the diff. For very large inputs (N*M > 16M cells) a cheap
    // fallback alignment is used so the UI stays responsive.
    void compute(const QString& a, const QString& b);

    const QVector<DiffRow>& rows() const { return m_rows; }
    const QVector<DiffHunk>& hunks() const { return m_hunks; }
    bool usedFallback() const { return m_fallback; }

    // Pure LCS diff on pre-split lines (unit tested).
    static QVector<DiffRow> diffLines(const QStringList& a, const QStringList& b);

private:
    QVector<DiffRow> m_rows;
    QVector<DiffHunk> m_hunks;
    bool m_fallback = false;
};

}  // namespace cf
