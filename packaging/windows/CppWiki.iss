; Inno Setup script for the CppWiki desktop app installer.
; Built by .github/workflows/package-release.yml on windows-latest.
;
; Expected defines (passed via /D on the ISCC.exe command line):
;   MyAppVersion  - application version, e.g. 0.1.0
;   MySourceDir   - staged install tree (cmake --install output), containing bin\cppwiki.exe
;                   plus windeployqt-deployed Qt runtime DLLs/plugins
;   MyOutputDir   - directory to write the generated installer into

#define MyAppName "CppWiki"
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#define MyAppPublisher "CppWiki"
#define MyAppExeName "cppwiki.exe"

[Setup]
AppId={{6C0B6C7C-8B0B-4E1B-9C1B-CPPWIKI-APP}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir={#MyOutputDir}
OutputBaseFilename=cppwiki-windows-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; Unsigned MVP build: no code-signing certificate is available yet (see issue #13).

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#MySourceDir}\bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\bin\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\bin\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Run]
Filename: "{app}\bin\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
