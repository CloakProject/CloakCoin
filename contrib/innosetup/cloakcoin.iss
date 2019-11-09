; Inno Setup script to create the installer

#include <idp.iss>

[Setup]
AppName=CloakCoin
AppVersion=2.0.2.1.defender
VersionInfoVersion=2.0.2.1.defender
DefaultDirName={pf}\CloakCoin
DefaultGroupName=CloakCoin
UninstallDisplayIcon={app}\cloakcoin-qt.exe
Compression=lzma2
SolidCompression=yes
OutputDir=windows-setup
OutputBaseFilename=cloakcoin-windows-setup
WizardImageFile=logo-164x314.bmp
WizardSmallImageFile=logo-55x58.bmp
DisableWelcomePage=no

[Tasks]
Name: "desktopicon"; Description: "Create a desktop icon"; GroupDescription: "Additional Tasks"; Flags: checkedonce

[Files]
Source: "cloakcoin-qt.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "libcurl.dll"; DestDir: "{app}"
Source: "libeay32.dll"; DestDir: "{app}"
Source: "libidn-11.dll"; DestDir: "{app}"
Source: "librtmp.dll"; DestDir: "{app}"
Source: "libssh2.dll"; DestDir: "{app}"
Source: "ssleay32.dll"; DestDir: "{app}"
Source: "zlib1.dll"; DestDir: "{app}"

[Icons]
Name: "{group}\CloakCoin"; Filename: "{app}\cloakcoin-qt.exe"
Name: "{userdesktop}\CloakCoin"; Filename: "{app}\cloakcoin-qt.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\cloakcoin-qt.exe"; Description: "Launch CloakCoin now"; Flags: postinstall nowait skipifsilent

;[Code]
;procedure CurPageChanged(CurPageID: Integer);
;begin
;  if (CurPageID=wpReady) and IsTaskSelected('installblockchain') then begin
;    idpAddFile('https://cloakcoin.com/downloads/blockchain_win.zip', ExpandConstant('{tmp}\blockchain_win.zip'));
;    idpDownloadAfter(wpReady);
;  end;
;end;
