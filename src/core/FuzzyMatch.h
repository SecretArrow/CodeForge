#pragma once
// Fuzzy subsequence matcher used by Quick Open / Command Palette.
#include <QString>
#include <QStringList>
#include <QVector>

namespace cf {

struct FuzzyResult {
    int score = -1;          // -1 == no match
    QVector<int> indices;    // matched character positions in the candidate
};

class FuzzyMatch {
public:
    // Case-insensitive subsequence match with bonuses for consecutive runs,
    // word boundaries (path separators, camelCase, separators) and start-of-string.
    static FuzzyResult score(QStringView query, QStringView candidate);

    // Convenience: score a relative file path; the filename part gets a bonus.
    static FuzzyResult scorePath(QStringView query, QStringView relativePath);

    // Sort helper: returns candidates filtered + sorted best-first.
    static QStringList filter(const QString& query, const QStringList& candidates, int limit = 50);
};

}  // namespace cf
