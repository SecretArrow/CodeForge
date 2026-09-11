; ============================================================
;  CodeForge Windows installer (Inno Setup 6)
;  Build: ISCC.exe installer\CodeForge.iss
; ============================================================

#define AppName "CodeForge"
#define AppVersion "1.0.0"
#define AppPublisher "CodeForge Project"
#define AppExe "CodeForge.exe"

[Setup]
AppId={{8E5B7F3A-2C41-4E0D-9A7B-CODEFORGE100}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
OutputDir=Output
OutputBaseFilename=CodeForgeSetup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
WizardStyle=modern
ChangesAssociations=yes

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "portable"; Description: "Portable mode (keep settings next to the program)"; Flags: unchecked
Name: "assoc_cpp"; Description: "Associate .cpp / .h / .hpp files"; Flags: unchecked
Name: "assoc_text"; Description: "Associate .txt / .md / .json files"; Flags: unchecked

[Files]
Source: "..\release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Dirs]
; Portable mode marker folder: settings stored in {app}\portable\data
Name: "{app}\portable"; Tasks: portable

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
Root: HKA; Subdir: "Software\Classes\CodeForge.cpp"; ValueType: string; ValueData: "C++ source (CodeForge)"; Flags: uninsdeletekey; Tasks: assoc_cpp
Root: HKA; Subdir: "Software\Classes\CodeForge.cpp\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: assoc_cpp
Root: HKA; Subdir: "Software\Classes\.cpp"; ValueType: string; ValueData: "CodeForge.cpp"; Flags: uninsdeletevalue; Tasks: assoc_cpp
Root: HKA; Subdir: "Software\Classes\.h"; ValueType: string; ValueData: "CodeForge.cpp"; Flags: uninsdeletevalue; Tasks: assoc_cpp
Root: HKA; Subdir: "Software\Classes\.hpp"; ValueType: string; ValueData: "CodeForge.cpp"; Flags: uninsdeletevalue; Tasks: assoc_cpp
Root: HKA; Subdir: "Software\Classes\CodeForge.text"; ValueType: string; ValueData: "Text (CodeForge)"; Flags: uninsdeletekey; Tasks: assoc_text
Root: HKA; Subdir: "Software\Classes\CodeForge.text\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: assoc_text
Root: HKA; Subdir: "Software\Classes\.txt"; ValueType: string; ValueData: "CodeForge.text"; Flags: uninsdeletevalue; Tasks: assoc_text
Root: HKA; Subdir: "Software\Classes\.md"; ValueType: string; ValueData: "CodeForge.text"; Flags: uninsdeletevalue; Tasks: assoc_text
Root: HKA; Subdir: "Software\Classes\.json"; ValueType: string; ValueData: "CodeForge.text"; Flags: uninsdeletevalue; Tasks: assoc_text

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Leave user data untouched; only remove portable data created by the app.
Type: filesandordirs; Name: "{app}\portable\data"; Tasks: portable
