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
ChangesAssociations=yes
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
chinesesimp.FileAssociation=将 MCnotepad 设为 Markdown 文件的打开方式
chinesesimp.OpenWith=用 MCnotepad 打开
chinesesimp.LaunchNow=启动 MCnotepad
english.DesktopIcon=Create a desktop shortcut
english.ContextMenu=Add "Open with MCnotepad" to the file context menu
english.FileAssociation=Associate MCnotepad with Markdown files
english.OpenWith=Open with MCnotepad
english.LaunchNow=Launch MCnotepad

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopIcon}"; Flags: unchecked
Name: "contextmenu"; Description: "{cm:ContextMenu}"; Flags: unchecked
Name: "fileassociation"; Description: "{cm:FileAssociation}"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\MCnotepad"; Filename: "{app}\MCnotepad.exe"
Name: "{autodesktop}\MCnotepad"; Filename: "{app}\MCnotepad.exe"; Tasks: desktopicon

[Registry]
; Official Markdown file association. The ProgID owns the icon and the open
; command, so Windows can show MCnotepad in Open With / Default apps and pass
; the selected path to the existing command-line file opener.
Root: HKA; Subkey: "Software\Classes\.md"; ValueType: string; ValueName: ""; ValueData: "MCnotepad.Markdown"; Tasks: fileassociation; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.markdown"; ValueType: string; ValueName: ""; ValueData: "MCnotepad.Markdown"; Tasks: fileassociation; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.mdown"; ValueType: string; ValueName: ""; ValueData: "MCnotepad.Markdown"; Tasks: fileassociation; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.mkd"; ValueType: string; ValueName: ""; ValueData: "MCnotepad.Markdown"; Tasks: fileassociation; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\MCnotepad.Markdown"; ValueType: string; ValueName: ""; ValueData: "Markdown document"; Tasks: fileassociation; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\MCnotepad.Markdown\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\MCnotepad.exe,0"; Tasks: fileassociation
Root: HKA; Subkey: "Software\Classes\MCnotepad.Markdown\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\MCnotepad.exe"" ""%1"""; Tasks: fileassociation
; Repair the legacy Applications\MCnotepad.exe entry used by Windows UserChoice
; records from older portable builds. Without this, Windows can keep invoking
; a deleted portable path even after the new ProgID is installed.
Root: HKA; Subkey: "Software\Classes\Applications\MCnotepad.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "MCnotepad"; Tasks: fileassociation; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\MCnotepad.exe\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\MCnotepad.exe,0"; Tasks: fileassociation
Root: HKA; Subkey: "Software\Classes\Applications\MCnotepad.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\MCnotepad.exe"" ""%1"""; Tasks: fileassociation
; Register the application with Windows Default apps. This keeps the entry
; named MCnotepad and points it at the same Markdown ProgID.
Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "MCnotepad"; ValueData: "Software\MCnotepad\Capabilities"; Tasks: fileassociation; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\MCnotepad\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "MCnotepad"; Tasks: fileassociation; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\MCnotepad\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "轻量级文本与 Markdown 编辑器"; Tasks: fileassociation
Root: HKA; Subkey: "Software\MCnotepad\Capabilities"; ValueType: string; ValueName: "ApplicationIcon"; ValueData: "{app}\MCnotepad.exe,0"; Tasks: fileassociation
Root: HKA; Subkey: "Software\MCnotepad\Capabilities\FileAssociations"; ValueType: string; ValueName: ".md"; ValueData: "MCnotepad.Markdown"; Tasks: fileassociation
Root: HKA; Subkey: "Software\MCnotepad\Capabilities\FileAssociations"; ValueType: string; ValueName: ".markdown"; ValueData: "MCnotepad.Markdown"; Tasks: fileassociation
Root: HKA; Subkey: "Software\MCnotepad\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mdown"; ValueData: "MCnotepad.Markdown"; Tasks: fileassociation
Root: HKA; Subkey: "Software\MCnotepad\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mkd"; ValueData: "MCnotepad.Markdown"; Tasks: fileassociation
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
