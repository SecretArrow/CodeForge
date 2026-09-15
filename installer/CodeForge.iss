; ============================================================
;  CodeForge Windows installer (Inno Setup 6)
;  Build from repo root:
;    ISCC.exe /DAppVersion=1.2.0 /DPayloadDir=build\bin installer\CodeForge.iss
;  The payload must already contain windeployqt output.
; ============================================================

#ifndef AppVersion
#define AppVersion "1.2.1"
#endif
#ifndef PayloadDir
#define PayloadDir "..\build\bin"
#endif

#define AppName "CodeForge"
#define AppPublisher "CodeForge Project"
#define AppExe "CodeForge.exe"

[Setup]
AppId={{8E5B7F3A-2C41-4E0D-9A7B-CODEFORGE100}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppVerName={#AppName} {#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
OutputDir=Output
OutputBaseFilename=CodeForgeSetup-{#AppVersion}-x64
Compression=lzma2/max
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
WizardStyle=modern
SetupIconFile=..\src\app\codeforge.ico
LicenseFile=..\LICENSE
ChangesAssociations=yes

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "portable"; Description: "Portable mode (keep settings next to the program)"; Flags: unchecked
Name: "assoc_cpp"; Description: "Associate .cpp / .h / .hpp files"; Flags: unchecked
Name: "assoc_text"; Description: "Associate .txt / .md / .json files"; Flags: unchecked

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\packaging\windows\SMARTSCREEN.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\packaging\windows\README-SMARTSCREEN.txt"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
; Portable mode marker folder: settings stored in {app}\portable\data
Name: "{app}\portable"; Tasks: portable

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\CodeForge.cpp"; ValueType: string; ValueData: "C++ source (CodeForge)"; Flags: uninsdeletekey; Tasks: assoc_cpp
Root: HKA; Subkey: "Software\Classes\CodeForge.cpp\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: assoc_cpp
Root: HKA; Subkey: "Software\Classes\.cpp"; ValueType: string; ValueData: "CodeForge.cpp"; Flags: uninsdeletevalue; Tasks: assoc_cpp
Root: HKA; Subkey: "Software\Classes\.h"; ValueType: string; ValueData: "CodeForge.cpp"; Flags: uninsdeletevalue; Tasks: assoc_cpp
Root: HKA; Subkey: "Software\Classes\.hpp"; ValueType: string; ValueData: "CodeForge.cpp"; Flags: uninsdeletevalue; Tasks: assoc_cpp
Root: HKA; Subkey: "Software\Classes\CodeForge.text"; ValueType: string; ValueData: "Text (CodeForge)"; Flags: uninsdeletekey; Tasks: assoc_text
Root: HKA; Subkey: "Software\Classes\CodeForge.text\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: assoc_text
Root: HKA; Subkey: "Software\Classes\.txt"; ValueType: string; ValueData: "CodeForge.text"; Flags: uninsdeletevalue; Tasks: assoc_text
Root: HKA; Subkey: "Software\Classes\.md"; ValueType: string; ValueData: "CodeForge.text"; Flags: uninsdeletevalue; Tasks: assoc_text
Root: HKA; Subkey: "Software\Classes\.json"; ValueType: string; ValueData: "CodeForge.text"; Flags: uninsdeletevalue; Tasks: assoc_text

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Leave user data untouched; only remove portable data created by the app.
Type: filesandordirs; Name: "{app}\portable\data"; Tasks: portable
