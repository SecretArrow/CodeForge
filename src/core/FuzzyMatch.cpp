#include "core/FuzzyMatch.h"

#include <algorithm>

namespace cf {

static bool isBoundaryChar(QChar c)
{
    switch (c.toLatin1()) {
    case '/': case '\\': case '_': case '-': case '.': case ' ': case '$':
        return true;
    }
    return false;
}

FuzzyResult FuzzyMatch::score(QStringView query, QStringView candidate)
{
    FuzzyResult r;
    const int ql = query.size(), cl = candidate.size();
    if (ql == 0) { r.score = 0; return r; }
    if (ql > cl) return r;

    int qi = 0, total = 0, lastIdx = -2;
    bool lastWasBoundary = true;
    QVector<int> indices;
    indices.reserve(ql);

    for (int ci = 0; ci < cl && qi < ql; ++ci) {
        const QChar cc = candidate.at(ci);
        const QChar qc = query.at(qi);
        if (cc.toLower() != qc.toLower()) { lastWasBoundary = isBoundaryChar(cc) || (ci > 0 && candidate.at(ci-1).isLower() && cc.isUpper()); continue; }

        int s = 16;                       // base match
        if (ci == lastIdx + 1) s += 8;    // consecutive
        if (lastWasBoundary || ci == 0) s += 12;  // boundary / start
        if (cc.isUpper() && cc.isLetter()) s += 2;
        // gap penalty (small): distance since last match
        if (lastIdx >= 0 && ci > lastIdx + 1) s -= qMin(6, ci - lastIdx - 1);

        total += s;
        indices.append(ci);
        lastIdx = ci;
        ++qi;
        lastWasBoundary = isBoundaryChar(cc) || (ci > 0 && candidate.at(ci-1).isLower() && cc.isUpper());
    }

    if (qi < ql) return r;  // not fully matched
    r.score = total;
    r.indices = indices;
    return r;
}

FuzzyResult FuzzyMatch::scorePath(QStringView query, QStringView relativePath)
{
    FuzzyResult r = score(query, relativePath);
    if (r.score < 0) return r;

    const int slash = relativePath.lastIndexOf(u'/');
    if (slash >= 0 && slash + 1 < relativePath.size()) {
        const QStringView fileName = relativePath.mid(slash + 1);
        FuzzyResult fn = score(query, fileName);
        if (fn.score >= 0) {
            // Prefer matches inside the filename; shift indices to path space.
            r.score = qMax(r.score, fn.score + 24);
            r.indices.clear();
            r.indices.reserve(fn.indices.size());
            for (int i : fn.indices) r.indices.append(i + slash + 1);
        }
    }
    // Light penalty for deeper paths so root-level files rank first.
    int depth = 0;
    for (QChar c : relativePath) if (c == u'/') ++depth;
    r.score -= depth * 2;
    return r;
}

QStringList FuzzyMatch::filter(const QString& query, const QStringList& candidates, int limit)
{
    struct Entry { int score; QString text; };
    QVector<Entry> matched;
    matched.reserve(candidates.size());
    for (const QString& c : candidates) {
        FuzzyResult r = score(query, c);
        if (r.score >= 0) matched.append({r.score, c});
    }
    std::stable_sort(matched.begin(), matched.end(),
                     [](const Entry& a, const Entry& b) { return a.score > b.score; });
    QStringList out;
    out.reserve(qMin(limit, matched.size()));
    for (int i = 0; i < matched.size() && i < limit; ++i) out.append(matched.at(i).text);
    return out;
}

}  // namespace cf
