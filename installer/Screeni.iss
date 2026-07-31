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
const
  WindowsAppRuntimeUrl = 'https://aka.ms/windowsappsdk/1.6/1.6.250602001/windowsappruntimeinstall-x64.exe';
  WindowsAppRuntimeFile = 'WindowsAppRuntimeInstall-x64.exe';
  ShutdownRequestEventName = 'Local\Screeni.ShutdownRequest';
  ShutdownCompleteEventName = 'Local\Screeni.ShutdownComplete';
  EventModifyState = $0002;
  EventSynchronize = $00100000;
  WaitObjectSignaled = 0;
  ShutdownTimeoutMs = 15000;
  LegacyFlushGraceMs = 6000;

function OpenEvent(
  DesiredAccess: DWORD;
  InheritHandle: Boolean;
  EventName: string): THandle;
  external 'OpenEventW@kernel32.dll stdcall';

function SetEvent(EventHandle: THandle): Boolean;
  external 'SetEvent@kernel32.dll stdcall';

function WaitForSingleObject(EventHandle: THandle; Timeout: DWORD): DWORD;
  external 'WaitForSingleObject@kernel32.dll stdcall';

function CloseHandle(Handle: THandle): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

function SignalRunningScreeni: Boolean;
var
  EventHandle: THandle;
begin
  Result := False;
  EventHandle := OpenEvent(EventModifyState, False, ShutdownRequestEventName);
  if EventHandle = 0 then
    exit;
  try
    Result := SetEvent(EventHandle);
  finally
    CloseHandle(EventHandle);
  end;
end;

function WaitForScreeniShutdown: Boolean;
var
  EventHandle: THandle;
begin
  Result := False;
  EventHandle := OpenEvent(EventSynchronize, False, ShutdownCompleteEventName);
  if EventHandle = 0 then
    exit;
  try
    Result := WaitForSingleObject(EventHandle, ShutdownTimeoutMs) = WaitObjectSignaled;
  finally
    CloseHandle(EventHandle);
  end;
end;

function ForceCloseScreeni: Boolean;
var
  ResultCode: Integer;
begin
  Result := Exec(
    ExpandConstant('{sys}\taskkill.exe'),
    '/F /T /IM Screeni.App.exe',
    '',
    SW_HIDE,
    ewWaitUntilTerminated,
    ResultCode);
end;

function AskScreeniToClose: Boolean;
var
  ResultCode: Integer;
begin
  Result := Exec(
    ExpandConstant('{sys}\taskkill.exe'),
    '/T /IM Screeni.App.exe',
    '',
    SW_HIDE,
    ewWaitUntilTerminated,
    ResultCode);
  Result := Result and (ResultCode = 0);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
  GracefulCloseRequested: Boolean;
begin
  Result := '';
  try
    WizardForm.StatusLabel.Caption := 'Closing Screeni...';
    if SignalRunningScreeni() then
    begin
      if not WaitForScreeniShutdown() then
        ForceCloseScreeni();
    end
    else
    begin
      GracefulCloseRequested := AskScreeniToClose();
      if GracefulCloseRequested then
        Sleep(LegacyFlushGraceMs);
      ForceCloseScreeni();
    end;

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
