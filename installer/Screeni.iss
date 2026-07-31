; Screeni — Windows installer (Inno Setup 6)
; https://jrsoftware.org/isinfo.php

#include "ci-version.iss"

#define MyAppName "Screeni"
#define MyAppPublisher "Screeni"
#define MyAppExeName "Screeni.App.exe"
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
Filename: "{app}\{#MyAppExeName}"; Description: "Launch Screeni"; Flags: nowait postinstall skipifsilent

[Code]
const
  WindowsAppRuntimeUrl = 'https://aka.ms/windowsappsdk/1.6/1.6.250602001/windowsappruntimeinstall-x64.exe';
  WindowsAppRuntimeFile = 'WindowsAppRuntimeInstall-x64.exe';

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Result := '';
  try
    WizardForm.StatusLabel.Caption := 'Downloading Windows App Runtime...';
    DownloadTemporaryFile(
      WindowsAppRuntimeUrl, WindowsAppRuntimeFile,
      'C7CD988425B76EA087E2E1D7B096B585F853E20BB826B8F38D45A5175410A877', nil);
    WizardForm.StatusLabel.Caption := 'Installing Windows App Runtime...';
    if not Exec(
      ExpandConstant('{tmp}') + '\' + WindowsAppRuntimeFile,
      '--quiet', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    begin
      Result := 'Could not start the Windows App Runtime installer.';
      exit;
    end;
    if (ResultCode <> 0) and (ResultCode <> 3010) then
      Result := Format('Windows App Runtime installation failed with exit code %d.', [ResultCode]);
  except
    Result := 'Could not install Windows App Runtime: ' + GetExceptionMessage;
  end;
end;

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\Screeni\usage.db-wal"
Type: filesandordirs; Name: "{localappdata}\Screeni\usage.db-shm"
