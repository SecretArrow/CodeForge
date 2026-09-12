#pragma once
// Filesystem utility helpers (atomic writes, shell/desktop integration).
#include <QString>
#include <QStringList>

class QByteArray;

namespace cf::fs {

// Reads entire file. Returns false and fills error on failure.
bool readAll(const QString& path, QByteArray& out, QString* error = nullptr);

// Atomic write (temp file + rename via QSaveFile).
bool writeAllAtomic(const QString& path, const QByteArray& data, QString* error = nullptr);

// Trash if possible, otherwise permanent delete after caller confirmation.
bool removePath(const QString& path, bool permanent, QString* error = nullptr);

bool duplicateEntry(const QString& path, QString* error = nullptr);

QString uniqueSiblingName(const QString& path, bool isDir);

bool ensureParentDir(const QString& path);

// Open a terminal window in the given directory (Windows Terminal / PowerShell
// / CMD on Windows; $TERM variants on other platforms).
bool openTerminalAt(const QString& dir);

// Reveal path in system file manager.
void revealInFileManager(const QString& path);

// Open with the system default application.
void openWithDefaultApp(const QString& path);

bool isTextFileByHead(const QString& path, QString* error = nullptr);

}  // namespace cf::fs
