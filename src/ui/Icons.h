#pragma once
// Vector icon painter: crisp, theme-colored icons without external assets.
#include <QColor>
#include <QHash>
#include <QIcon>
#include <QString>

namespace cf {

class Icons {
public:
    enum class Name {
        File, Folder, FolderOpen, Search, GitBranch, Play, Puzzle, Gear,
        Terminal, Close, DotModified, Pin, SplitRight, Plus, Refresh,
        CollapseAll, Warning, Error, Info, Save, Book, Lock, NewFile,
        NewFolder, Filter, Outline, ChevronRight, ChevronDown, Copy, Trash, Checklist
    };

    static QIcon icon(Name name, const QColor& color);
    // Convenience with current palette foreground.
    static QIcon icon(Name name);

private:
    static void paint(Name name, QPainter& p, const QColor& color, int size);
};

}  // namespace cf
