#pragma once
// BreakpointStore: global registry of debugger breakpoints, keyed by file and
// line. Lives in core so both the editor margin (painting + toggling) and the
// debugger service (creation/removal on the debugger side) share one source of
// truth without a dependency between those modules.
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

namespace cf {

struct Breakpoint {
    QString filePath;
    int line = 0;              // 0-based block number
    bool enabled = true;
    QString condition;         // empty = unconditional
    int debuggerId = -1;       // id assigned by the debugger backend (-1 = none)
    int hitCount = 0;
};

class BreakpointStore : public QObject {
    Q_OBJECT
public:
    static BreakpointStore& instance();

    // Toggles the breakpoint at (path, line). Returns true if it now exists.
    bool toggle(const QString& filePath, int line);
    void add(const Breakpoint& bp);
    void remove(const QString& filePath, int line);
    void clearFile(const QString& filePath);
    void clearAll();

    void setEnabled(const QString& filePath, int line, bool enabled);
    void setCondition(const QString& filePath, int line, const QString& condition);
    void setDebuggerId(const QString& filePath, int line, int id);

    bool has(const QString& filePath, int line) const;
    QSet<int> linesFor(const QString& filePath) const;
    QVector<Breakpoint> forFile(const QString& filePath) const;
    QVector<Breakpoint> all() const;
    int count() const { return int(m_bps.size()); }

signals:
    // Emitted after any mutation; editors repaint their margins on this.
    void changed();
    void added(const cf::Breakpoint& bp);
    void removed(const QString& filePath, int line);

private:
    BreakpointStore() = default;
    QVector<Breakpoint>::iterator find(const QString& filePath, int line);

    QVector<Breakpoint> m_bps;
};

}  // namespace cf
