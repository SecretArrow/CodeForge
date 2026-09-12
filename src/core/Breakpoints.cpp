#include "core/Breakpoints.h"

namespace cf {

BreakpointStore& BreakpointStore::instance()
{
    static BreakpointStore store;
    return store;
}

QVector<Breakpoint>::iterator BreakpointStore::find(const QString& filePath, int line)
{
    return std::find_if(m_bps.begin(), m_bps.end(), [&](const Breakpoint& b) {
        return b.filePath == filePath && b.line == line;
    });
}

bool BreakpointStore::toggle(const QString& filePath, int line)
{
    if (filePath.isEmpty() || line < 0) return false;
    auto it = find(filePath, line);
    if (it != m_bps.end()) {
        const QString path = it->filePath;
        const int ln = it->line;
        m_bps.erase(it);
        emit removed(path, ln);
        emit changed();
        return false;
    }
    Breakpoint bp;
    bp.filePath = filePath;
    bp.line = line;
    m_bps.append(bp);
    emit added(bp);
    emit changed();
    return true;
}

void BreakpointStore::add(const Breakpoint& bp)
{
    if (bp.filePath.isEmpty() || bp.line < 0) return;
    if (find(bp.filePath, bp.line) != m_bps.end()) return;   // already present
    m_bps.append(bp);
    emit added(bp);
    emit changed();
}

void BreakpointStore::remove(const QString& filePath, int line)
{
    auto it = find(filePath, line);
    if (it == m_bps.end()) return;
    m_bps.erase(it);
    emit removed(filePath, line);
    emit changed();
}

void BreakpointStore::clearFile(const QString& filePath)
{
    if (filePath.isEmpty()) return;
    const auto before = m_bps.size();
    m_bps.erase(std::remove_if(m_bps.begin(), m_bps.end(),
                               [&](const Breakpoint& b) { return b.filePath == filePath; }),
                m_bps.end());
    if (m_bps.size() != before) emit changed();
}

void BreakpointStore::clearAll()
{
    if (m_bps.isEmpty()) return;
    m_bps.clear();
    emit changed();
}

void BreakpointStore::setEnabled(const QString& filePath, int line, bool enabled)
{
    auto it = find(filePath, line);
    if (it == m_bps.end() || it->enabled == enabled) return;
    it->enabled = enabled;
    emit changed();
}

void BreakpointStore::setCondition(const QString& filePath, int line, const QString& condition)
{
    auto it = find(filePath, line);
    if (it == m_bps.end() || it->condition == condition) return;
    it->condition = condition;
    emit changed();
}

void BreakpointStore::setDebuggerId(const QString& filePath, int line, int id)
{
    auto it = find(filePath, line);
    if (it == m_bps.end() || it->debuggerId == id) return;
    it->debuggerId = id;
}

bool BreakpointStore::has(const QString& filePath, int line) const
{
    return std::any_of(m_bps.cbegin(), m_bps.cend(), [&](const Breakpoint& b) {
        return b.filePath == filePath && b.line == line;
    });
}

QSet<int> BreakpointStore::linesFor(const QString& filePath) const
{
    QSet<int> lines;
    for (const Breakpoint& b : m_bps) {
        if (b.filePath == filePath)
            lines.insert(b.line);
    }
    return lines;
}

QVector<Breakpoint> BreakpointStore::forFile(const QString& filePath) const
{
    QVector<Breakpoint> out;
    for (const Breakpoint& b : m_bps) {
        if (b.filePath == filePath)
            out.append(b);
    }
    return out;
}

QVector<Breakpoint> BreakpointStore::all() const
{
    return m_bps;
}

}  // namespace cf
