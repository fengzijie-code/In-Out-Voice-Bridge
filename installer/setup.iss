[Setup]
AppName=In Out Voice Bridge
AppVersion=1.0.0
AppVerName=In Out Voice Bridge 1.0.0
AppPublisher=FengZijie
AppPublisherURL=https://github.com/fengzijie
DefaultDirName={autopf}\InOutVoiceBridge
DefaultGroupName=In Out Voice Bridge
OutputDir=Output
OutputBaseFilename=InOutVoiceBridgeSetup
SetupIconFile=..\src\InOutVoiceBridge.App\Assets\AppIcon.ico
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\InOutVoiceBridge.exe
WizardStyle=modern
VersionInfoVersion=1.0.0
VersionInfoCompany=FengZijie
VersionInfoDescription=In Out Voice Bridge Setup
VersionInfoProductName=In Out Voice Bridge
VersionInfoProductVersion=1.0.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
Source: "..\publish\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\In Out Voice Bridge"; Filename: "{app}\InOutVoiceBridge.exe"
Name: "{group}\Uninstall In Out Voice Bridge"; Filename: "{uninstallexe}"
Name: "{autodesktop}\In Out Voice Bridge"; Filename: "{app}\InOutVoiceBridge.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\InOutVoiceBridge.exe"; Description: "Launch In Out Voice Bridge"; Flags: nowait postinstall skipifsilent
