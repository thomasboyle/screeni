; Screeni — Windows installer (Inno Setup 6)
; https://jrsoftware.org/isinfo.php

#include "ci-version.iss"

#define MyAppName "Screeni"
#define MyAppPublisher "Screeni"
#define MyAppExeName "Screeni.exe"
#ifndef PublishDir
  #define PublishDir "..\artifacts\publish"
#endif

[Setup]
AppId={{8F3C2A91-6B4E-4D2F-9A1C-7E5B0D8F4A21}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\Screeni
DefaultGroupName=Screeni
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\artifacts\installer
OutputBaseFilename=ScreeniSetup-{#MyAppVersion}
SetupIconFile=..\src\Screeni.App\Assets\Screeni.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=no
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "startup"; Description: "Start Screeni when Windows starts"; GroupDescription: "Startup:"

[Files]
Source: "{#PublishDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "Screeni"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: startup

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch Screeni"; Flags: nowait postinstall

[Code]
function ForceCloseScreeni: Boolean;
var
  ResultCode: Integer;
begin
  Result := Exec(
    ExpandConstant('{sys}\taskkill.exe'),
    '/F /T /IM Screeni.exe',
    '',
    SW_HIDE,
    ewWaitUntilTerminated,
    ResultCode);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  try
    WizardForm.StatusLabel.Caption := 'Closing Screeni...';
    ForceCloseScreeni();
  except
    Result := 'Could not stop Screeni: ' + GetExceptionMessage;
  end;
end;

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\Screeni\usage.db-wal"
Type: filesandordirs; Name: "{localappdata}\Screeni\usage.db-shm"