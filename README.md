# CodeForge

**Editor kode/text native untuk Windows 10/11 (64-bit) berbasis C++20 dan Qt 6** — lightweight, cepat, offline-first, tanpa telemetri. Dirancang sebagai alternatif ringan dengan pengalaman ala Visual Studio Code, dibangun dengan arsitektur modular yang jelas.

> ⚠️ Aplikasi ini **tidak mengirim data apa pun ke internet**. Tidak ada telemetri. Satu-satunya request jaringan opsional adalah cek pembaruan manual / opt-in (`Settings → Updates`) ke GitHub Releases API — **tidak pernah mengunduh apa pun secara otomatis** (selain `git` yang Anda jalankan sendiri).

---

## Ringkasan Fitur yang Diimplementasikan

### Baru di v1.1.0
- **Multi-cursor editing** (nyata, bukan dekorasi): `Alt+Click` tambah/hapus kursor, `Ctrl+D` pilih kemunculan berikutnya, `Ctrl+K Ctrl+D` skip, `Ctrl+Alt+↑/↓` kursor atas/bawah, `Esc` kembali satu kursor — ketik/Backspace/Delete/Enter/Tab diterapkan ke semua kursor dalam satu langkah undo
- **LSP client asli** (JSON-RPC 2.0 over stdio): konfigurasi per bahasa di `Settings → Extensions` (mis. `cpp=clangd`, `python=pyright-langserver --stdio`), **publishDiagnostics → Problems + squiggly underline**, hover (`Ctrl+K Ctrl+I`), **Go to Definition** (`F12`)
- **Markdown preview** live (`Ctrl+Shift+V`) — konverter markdown sendiri (heading, fenced code, tabel, blockquote, list, task list, link/gambar, tanpa dependensi eksternal)
- **.editorconfig** — parser spesifikasi (glob `*`/`**`/`{a,b}`, walk-up sampai `root=true`): indent style/size, tab_width, end_of_line, trim_trailing_whitespace, insert_final_newline
- **TODO panel** — pemindaian workspace (TODO/FIXME/HACK/XXX) via search engine background, dikelompokkan per file
- **Bookmark** (`Ctrl+F2` toggle, `F2`/`Shift+F2` navigasi) dengan penanda di margin
- **Bracket pair colorization** (3 warna per kedalaman, sadar string/komentar) + **Sticky scroll** (header scope menempel di atas editor)
- **Pratinjau gambar** sebagai tab editor (png/jpg/bmp/gif/svg/webp/ico, zoom Ctrl+roda, klik-ganda fit)
- **Zen mode** (`Ctrl+K Z`), **Update checker** opt-in (GitHub Releases API)

### Inti Editor (semuanya berfungsi nyata — bukan mock)
- Buka file/folder sebagai **workspace**, buffer per tab yang benar-benar independen
- **Multi-tab**: close/middle-click/Ctrl+W, Ctrl+Tab berpindah tab, **pin tab**, **preview tab** (italic, diganti saat membuka file lain), indicator file termodifikasi (dot), drag tab antar grup editor
- **Split editor** kanan/bawah dengan splitter tree nested ala VS Code, drag antar grup, auto-collapse grup kosong
- **Undo/redo**, auto-indent, **auto-closing brackets/quotes** (termasuk wrap seleksi), Tab/Shift+Tab indent blok
- **Bracket matching** + highlight, highlight baris aktif, **indent guides**, visualisasi whitespace, word wrap toggle
- **Code folding** (brace untuk C-family/JSON/CSS; indentasi untuk Python/YAML), unfold otomatis saat kursor masuk area tersembunyi
- **Find/Replace** in-editor (case/word/regex, hitung jumlah match, replace all)
- **Minimap** pratinjau struktur dokumen + navigasi klik/drag
- **Breadcrumbs** path + simbol terdekat pada kursor
- **Outline panel** + simbol dokumen (regex scanner per bahasa)
- Status bar interaktif: Ln/Col, seleksi, **encoding (klik untuk ganti)**, **EOL LF/CRLF/CR (klik untuk konversi)**, **bahasa**, ukuran file, branch git, read-only

### Search
- **Quick Open (Ctrl+P)** — fuzzy search atas indeks file workspace (background thread, real-time)
- **Global Search (Ctrl+Shift+F)** — literal/regex/whole-word/case, include/exclude glob, hasil dikelompokkan per file, klik → buka di baris tsb
- **Replace across workspace** — buffer terbuka diganti in-place (undo tetap utuh), file tertutup ditulis aman (atomic write) dengan encoding asli dipertahankan
- Mode prefix di Quick Open: `>` command palette, `:N` go-to-line, `@` simbol

### Command Palette & Shortcut
- **Ctrl+Shift+P** — command palette dengan fuzzy matching (~40 perintah terdaftar)
- Shortcut manager di Settings → Keyboard: ubah binding per perintah, **deteksi konflik**, override tersimpan di settings
- Shortcut utama: Ctrl+S/Shift+S (Save/As), Ctrl+P, Ctrl+Shift+P, Ctrl+F/H, Ctrl+G, Ctrl+B (sidebar), Ctrl+J (panel), Ctrl+` (terminal), Ctrl+\ (split), Ctrl+K Ctrl+T (theme), Ctrl+, (settings)

### Syntax Highlighting (20+ bahasa)
C, C++, C#, Java, JavaScript, TypeScript, Python, Rust, Go, PHP, HTML, CSS/SCSS, JSON, XML, YAML, Markdown, SQL, Shell, PowerShell, CMake, INI, TOML — dengan dukungan **multi-line comment/docstring** (block state) dan deteksi via ekstensi/filename (`.h`→C++, `CMakeLists.txt`→CMake, dst).

### Themes
- 6 tema bawaan: **Dark+, Light+, Dracula-like, Monokai-like, High Contrast Dark/Light**
- Theme engine berbasis token JSON (UI + editor + syntax); **tema kustom = drop file JSON** ke folder themes (lihat lokasi di bawah), tanpa re-compile
- Palet QPalette + stylesheet global dihasilkan dari tema (high-DPI aman)

### Filesystem & Workspace
- **Explorer** dengan lazy-loading per direktori (tidak memuat seluruh tree), hidden toggle, filter, drag & drop (pindah file/folder), multi-select
- Context menu lengkap: New File/Folder, Rename (F2), Delete (ke Recycle Bin bila bisa), Copy/Cut/Paste, Duplicate, Copy Path/Relative Path, Reveal in File Explorer, Open in Terminal, Properties
- **File watcher** (debounced): perubahan workspace → refresh tree; file terbuka berubah di luar → dialog **Reload / Keep Changes / Compare** (buka versi disk read-only bersebelahan)
- **Autosave**: off / afterDelay / onFocusChange / onWindowBlur
- **Large file**: dialog "Open normally / Open read-only / Cancel" dengan ambang bisa diatur
- **Encoding**: UTF-8, UTF-8 BOM, UTF-16 LE/BE, Windows-1252/Latin-1 — deteksi otomatis + konversi
- **Recent projects/files** dengan pin, welcome page

### Terminal, Build, Git
- **Terminal terintegrasi** (QProcess): PowerShell/pwsh/CMD/Git Bash (Windows), multi-tab, restart/kill/clear, riwayat perintah, cwd mengikuti workspace
- **CMake integration**: deteksi `CMakeLists.txt`, Configure/Build/Rebuild/Clean/Run — **semua async** (UI tidak pernah freeze), parsing output MSVC & GCC/Ninja menjadi **Problems** (klik → lompat ke baris), ringkasan `N errors, M warnings`
- **Git integration modular** (tanpa git, app tetap jalan): status real (staged/unstaged/untracked), dekorasi warna di Explorer, commit dengan pesan, stage/unstage/discard, pull/push, checkout branch, view diff, clone repository dari Welcome
- **Extensions**: native plugin via `QPluginLoader` (interface `cf::IExtension`), drop DLL ke folder extensions; tema JSON adalah extension surface pertama
- **LSP abstraction**: `lsp/LanguageService.h` — interface diagnostics/goToDefinition/hover siap dipasangkan dengan language server di fase berikutnya

### Keamanan (Sesuai Spesifikasi)
- **AES-256-GCM** authenticated encryption:
  - Windows: **CNG/BCrypt** (OS-provided)
  - Linux build: OpenSSL EVP
- **Key storage**: kunci acak dilindungi **DPAPI** (Windows); file 0600 (Linux). Tidak ada kunci hard-coded.
- **Crash recovery snapshots terenkripsi** — buffer yang belum disimpan di-snapshot berkala + saat shutdown; dipulihkan via dialog "Recovered Files" (Restore/Discard). **Tidak pernah ada plaintext isi editor di disk** — jika enkripsi tak tersedia, snapshot di-skip (bukan ditulis plaintext).
- **SecureBuffer**: memori terkunci (VirtualLock/mlock) + zeroize non-optimizable, non-copyable — untuk kunci & plaintext staging sebelum enkripsi
- **Clipboard security**: opsi auto-clear clipboard 15/30/60 detik (hanya jika konten belum berganti), tanpa clipboard history permanen
- Logging rotating (5×2 MB) yang **tidak pernah** menulis isi file/keys/clipboard
- **Portable mode**: buat folder `portable` di sebelah exe → semua data (settings, tema, recovery, log) tersimpan di `portable\data`

### Session
- Restore workspace, tab per grup, layout split, aktif tab, sidebar/panel state, geometry window
- Untitled docs yang belum tersimpan dipulihkan dari recovery terenkripsi

---

## Yang Secara Sadar Ditunda (Fase Berikutnya) — Tidak Ada yang Palsu

Sesuai aturan "jangan membuat fake/mock implementation", fitur berikut **belum** ada dan tidak dipalsukan:

| Fitur | Status | Catatan |
|---|---|---|
| Debugger (DAP) | Fase berikutnya | butuh integrasi adapter per toolchain |
| VT100 terminal emulation | Pakai line-based console | output + input line, history |
| Column selection (Alt+Shift) | Fase berikutnya | multi-kursor vertikal sudah ada via Ctrl+Alt+Up/Down |
| Marketplace extensions | Fase berikutnya | loader native plugin sudah jalan |
| Completion/renaming LSP | Fase berikutnya | klien LSP v1.1 sudah jalan (diagnostics/hover/definition) |

Keterbatasan teknis jujur lainnya:
- File raksasa (> ~500 MB) dibaca read-only ke buffer — pemakaian memori ~2× ukuran file (UTF-16)
- Folding di-reset saat dokumen berubah (dihitung ulang manual) — kebijakan sederhana yang aman
- Minimap berbasis sampling (struktur baris), bukan render glyph penuh

---

## Build (Windows)

### Prasyarat
1. **Qt 6.5+** (MSVC 2019/2022 64-bit) — mis. `C:\Qt\6.7.3\msvc2019_64`
2. **Visual Studio 2022** (workload Desktop C++) atau Build Tools
3. **CMake 3.21+** dan **Ninja** (bundled dengan VS)

### Langkah
```bat
:: 1. arahkan ke Qt
set QT6_DIR=C:\Qt\6.7.3\msvc2019_64

:: 2. build release
build-release.bat

:: 3. deploy Qt DLL + (opsional) installer Inno Setup
package.bat
```

Hasil:
```
build\windows-release\bin\CodeForge.exe   ← binary
release\                                  ← folder portable self-contained (hasil windeployqt)
installer\Output\CodeForgeSetup.exe       ← installer (bila Inno Setup 6 terpasang)
```

### Debug / IDE
```bat
build.bat                       :: configure + build debug
cmake --open build\windows-release   :: buka di Visual Studio (opsional)
```

### Linux (untuk pengembangan/testing)
```bash
cmake --preset linux-release
cmake --build build/linux -j
ctest --test-dir build/linux         # 6 suite unit test
```

---

## Instalasi & Portable Mode

- **Installer** (`CodeForgeSetup.exe`): Start Menu + desktop shortcut (opsional), uninstaller, file association opsional (`.cpp/.h/.hpp`, `.txt/.md/.json`), opsi **portable mode**
- **Portable manual**: letakkan `release\` di mana saja, buat folder kosong `portable` di sebelah `CodeForge.exe` → settings/recovery/log pindah ke `portable\data`
- Lokasi data default (non-portable): `%LOCALAPPDATA%\CodeForgeProject\CodeForge`
  - `settings.json`, `recents.json`, `session.json`
  - `themes\` — drop tema JSON kustom di sini
  - `extensions\` — drop plugin DLL di sini
  - `recovery\` — snapshot terenkripsi; `keys\` — kunci DPAPI; `logs\` — log rotating

## Testing

```bash
ctest --test-dir build/<preset>      # menjalankan 10 suite:
```
| Suite | Cakupan |
|---|---|
| TestFuzzyMatch | subsequence, boundary bonus, path scoring |
| TestEncoding | BOM, UTF-16 round-trip, fallback 1252, EOL detection |
| TestTextDocument | load/save round-trip CRLF, external change, untitled |
| TestCrypto | AES-256-GCM round-trip, **tamper rejection**, zeroize |
| TestSettingsTheme | schema defaults, set/get, parse tema, 6 tema bawaan |
| TestSearch | literal/regex/whole-word, glob `**` |
| TestMultiCursor | insert/backspace/newline ke banyak kursor, occurrence, normalize |
| TestMarkdown | heading, style inline, code, link/gambar, list, tabel, escaping |
| TestEditorConfig | glob matcher, properti, resolve dari disk, indent_size=tab |
| TestJsonRpc | framing Content-Length (split/chunk/malformed), URI round-trip |

## Struktur Kode

```
src/
├── app/          Application bootstrap, single-instance, crash marker
├── core/         TextDocument, DocumentManager, Encoding, FuzzyMatch,
│                 CommandRegistry, Logger, FileUtils, AppPaths
├── editor/       CodeEditor, TabBar, EditorGroup, EditorArea (split),
│                 FindReplaceBar, Minimap, Breadcrumbs
├── filesystem/   FileTreeModel (lazy), FileWatcher
├── project/      Workspace + async index, RecentManager, SessionManager
├── search/       SearchEngine (threaded), SearchPanel
├── terminal/     TerminalPane (QProcess shell)
├── git/          GitClient (async CLI), GitPanel
├── buildsys/     BuildManager (CMake async), BuildPanel
├── security/     SecureBuffer, Crypto (CNG/OpenSSL), KeyStore (DPAPI), ClipboardGuard
├── settings/     SettingsManager (+schema), SettingsDialog, KeybindManager
├── themes/       Theme, ThemeManager, 6 tema bawaan (JSON)
├── extensions/   IExtension, ExtensionHost (QPluginLoader)
├── lsp/          LanguageService abstraction
└── ui/           MainWindow, StatusBar, BottomPanel, Explorer, QuickOpen,
                  CommandPalette, WelcomePage, Outline, Icons (vector)
```

Dependency boundary: `ui` → semua layer; `core` tidak meng-include `ui` (prompt UI disuntikkan sebagai callback); antar-modul berkomunikasi lewat signal/slot atau interface.

## Lisensi
MIT — lihat `LICENSE`.
