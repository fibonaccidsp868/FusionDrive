[Setup]
AppName=FusionDrive
AppVersion=1.0.0
AppPublisher=FusionDrive Audio
CreateAppDir=no
OutputDir=..\dist
OutputBaseFilename=FusionDrive-1.0.0-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#VstDir}\*"; DestDir: "{commoncf64}\VST3\FusionDrive.vst3"; Flags: recursesubdirs ignoreversion
