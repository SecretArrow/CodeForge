#pragma once
// Central data-directory resolution (portable-mode aware).
//
// Portable mode: if a folder named "portable" exists next to the executable,
// all user data (settings, themes, recovery, logs) is stored inside it.
// SECURITY: files under dataRoot() must never store editor plaintext;
// anything derived from document content is encrypted (see security/).
#include <QString>

namespace cf::paths {

// Next to the executable; empty string when not portable.
QString portableDir();

bool isPortable();

// Base directory for all user data.
QString dataRoot();

// Sub-paths
QString settingsFile();
QString recentsFile();
QString sessionFile();
QString themesDir();
QString extensionsDir();
QString recoveryDir();
QString keysDir();
QString logsDir();

// Per-workspace settings location.
QString workspaceSettingsFile(const QString& workspaceRoot);

}  // namespace cf::paths
