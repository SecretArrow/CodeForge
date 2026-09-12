#include "diff/DiffEngine.h"

#include <QHash>
#include <QtGlobal>

namespace cf {

QStringList DiffEngine::splitLines(const QString& text)
{
    QString normalized = text;
    normalized.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    QStringList lines;
    int start = 0;
    const qsizetype n = normalized.size();
    for (qsizetype i = 0; i < n; ++i) {
        if (normalized.at(i) == u'\n') {
            lines.append(normalized.mid(start, i - start));
            start = i + 1;
        }
    }
    if (start < n || lines.isEmpty())
        lines.append(normalized.mid(start));
    return lines;
}

QVector<DiffRow> DiffEngine::diffLines(const QStringList& a, const QStringList& b)
{
    const int n = a.size();
    const int m = b.size();
    QVector<DiffRow> rows;

    // Guard against pathological sizes: LCS DP table would be n*m cells.
    // Above the budget, align identical prefixes/suffixes and mark the
    // middle as a single replace block (correct, just not minimal).
    if (static_cast<qint64>(n) * qMax(1, m) > 16 * 1000 * 1000) {
        int p = 0;
        while (p < n && p < m && a.at(p) == b.at(p))
            ++p;
        int sa = n - 1, sb = m - 1;
        while (sa > p && sb > p && a.at(sa) == b.at(sb)) {
            --sa;
            --sb;
        }
        for (int i = 0; i < p; ++i) {
            DiffRow r;
            r.type = DiffRow::Type::Equal;
            r.left = a.at(i);
            r.right = b.at(i);
            r.aLine = i;
            r.bLine = i;
            rows.append(r);
        }
        for (int i = p; i <= sa; ++i) {
            DiffRow r;
            r.type = DiffRow::Type::Delete;
            r.left = a.at(i);
            r.aLine = i;
            rows.append(r);
        }
        for (int j = p; j <= sb; ++j) {
            DiffRow r;
            r.type = DiffRow::Type::Add;
            r.right = b.at(j);
            r.bLine = j;
            rows.append(r);
        }
        for (int i = sa + 1; i < n && sb + 1 + (i - sa - 1) < m; ++i) {
            const int j = sb + 1 + (i - sa - 1);
            DiffRow r;
            r.type = DiffRow::Type::Equal;
            r.left = a.at(i);
            r.right = b.at(j);
            r.aLine = i;
            r.bLine = j;
            rows.append(r);
        }
        return rows;
    }

    // Standard LCS DP.
    QVector<int> lens((static_cast<qint64>(n) + 1) * (m + 1), 0);
    auto at = [&lens, m](int i, int j) -> int& { return lens[static_cast<qsizetype>(i) * (m + 1) + j]; };
    for (int i = n - 1; i >= 0; --i) {
        for (int j = m - 1; j >= 0; --j) {
            if (a.at(i) == b.at(j))
                at(i, j) = at(i + 1, j + 1) + 1;
            else
                at(i, j) = qMax(at(i + 1, j), at(i, j + 1));
        }
    }
    int i = 0, j = 0;
    while (i < n && j < m) {
        if (a.at(i) == b.at(j)) {
            DiffRow r;
            r.type = DiffRow::Type::Equal;
            r.left = a.at(i);
            r.right = b.at(j);
            r.aLine = i;
            r.bLine = j;
            rows.append(r);
            ++i;
            ++j;
        } else if (at(i + 1, j) >= at(i, j + 1)) {
            DiffRow r;
            r.type = DiffRow::Type::Delete;
            r.left = a.at(i);
            r.aLine = i;
            rows.append(r);
            ++i;
        } else {
            DiffRow r;
            r.type = DiffRow::Type::Add;
            r.right = b.at(j);
            r.bLine = j;
            rows.append(r);
            ++j;
        }
    }
    for (; i < n; ++i) {
        DiffRow r;
        r.type = DiffRow::Type::Delete;
        r.left = a.at(i);
        r.aLine = i;
        rows.append(r);
    }
    for (; j < m; ++j) {
        DiffRow r;
        r.type = DiffRow::Type::Add;
        r.right = b.at(j);
        r.bLine = j;
        rows.append(r);
    }
    return rows;
}

void DiffEngine::compute(const QString& a, const QString& b)
{
    const QStringList la = splitLines(a);
    const QStringList lb = splitLines(b);
    m_rows = diffLines(la, lb);
    m_fallback = static_cast<qint64>(la.size()) * qMax(1, lb.size()) > 16 * 1000 * 1000;

    // group consecutive non-equal rows into hunks
    m_hunks.clear();
    int rowIdx = 0;
    while (rowIdx < m_rows.size()) {
        if (m_rows.at(rowIdx).type == DiffRow::Type::Equal) {
            ++rowIdx;
            continue;
        }
        const int start = rowIdx;
        int aStart = -1, bStart = -1;
        while (rowIdx < m_rows.size() && m_rows.at(rowIdx).type != DiffRow::Type::Equal) {
            const DiffRow& r = m_rows.at(rowIdx);
            if (aStart < 0 && r.type == DiffRow::Type::Delete)
                aStart = r.aLine;
            if (bStart < 0 && r.type == DiffRow::Type::Add)
                bStart = r.bLine;
            ++rowIdx;
        }
        DiffHunk h;
        h.rowStart = start;
        h.rowCount = rowIdx - start;
        h.aStart = aStart;
        h.bStart = bStart;
        m_hunks.append(h);
    }
}

}  // namespace cf
