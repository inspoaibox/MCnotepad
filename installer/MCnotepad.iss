; MCnotepad installer (Inno Setup 6). Built by package.ps1, which passes:
;   /DAppVersion=0.1.0  /DBuildId=<git commit>  /DStageDir=<folder with the program files>
;   /DOutputDir=<where setup.exe goes>
; Installs for the current user by default (no administrator rights needed);
; the first page offers "install for all users" instead.

#ifndef AppVersion
  #error Run package.ps1 instead of compiling this file directly.
#endif

[Setup]
AppId={{6E3C2A4B-8F1D-4C7A-9B5E-2D4F6A8C1E73}
AppName=MCnotepad
AppVersion={#AppVersion}
AppVerName=MCnotepad {#AppVersion}
AppPublisher=MCnotepad
VersionInfoVersion={#AppVersion}
VersionInfoProductVersion={#AppVersion}
VersionInfoDescription=MCnotepad {#AppVersion} ({#BuildId}) Setup
DefaultDirName={autopf}\MCnotepad
DefaultGroupName=MCnotepad
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=MCnotepad-{#AppVersion}-setup
SetupIconFile=..\resources\MCnotepad.ico
UninstallDisplayIcon={app}\MCnotepad.exe
UninstallDisplayName=MCnotepad {#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; A running MCnotepad (also one hidden in the tray) is asked to exit before its
; files are replaced; it saves every tab to its recovery snapshots first.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
chinesesimp.DesktopIcon=创建桌面快捷方式
chinesesimp.ContextMenu=在文件右键菜单中添加“用 MCnotepad 打开”
chinesesimp.OpenWith=用 MCnotepad 打开
chinesesimp.LaunchNow=启动 MCnotepad
english.DesktopIcon=Create a desktop shortcut
english.ContextMenu=Add "Open with MCnotepad" to the file context menu
english.OpenWith=Open with MCnotepad
english.LaunchNow=Launch MCnotepad

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopIcon}"; Flags: unchecked
Name: "contextmenu"; Description: "{cm:ContextMenu}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\MCnotepad"; Filename: "{app}\MCnotepad.exe"
Name: "{autodesktop}\MCnotepad"; Filename: "{app}\MCnotepad.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\*\shell\MCnotepad"; ValueType: string; ValueName: ""; ValueData: "{cm:OpenWith}"; Tasks: contextmenu; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\*\shell\MCnotepad"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\MCnotepad.exe"""; Tasks: contextmenu
Root: HKA; Subkey: "Software\Classes\*\shell\MCnotepad\command"; ValueType: string; ValueName: ""; ValueData: """{app}\MCnotepad.exe"" ""%1"""; Tasks: contextmenu

[Run]
Filename: "{app}\MCnotepad.exe"; Description: "{cm:LaunchNow}"; Flags: nowait postinstall skipifsilent

; Sessions and recovery snapshots in %LOCALAPPDATA%\MCnotepad are the user's
; data and are kept on uninstall.

[Code]
// "Start with Windows" is written by the app itself. On uninstall remove it,
// but only when it starts this installed copy (not a portable one).
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Command: String;
begin
  if CurUninstallStep = usPostUninstall then
    if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'MCnotepad', Command) then
      if Pos(Lowercase(ExpandConstant('{app}\MCnotepad.exe')), Lowercase(Command)) > 0 then
        RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'MCnotepad');
end;
