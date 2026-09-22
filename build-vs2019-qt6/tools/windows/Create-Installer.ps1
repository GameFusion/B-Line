[CmdletBinding()]
param(
    [string]$BundleDir,
    [string]$IfwRoot = 'C:\Qt\Tools\QtInstallerFramework\4.10',
    [string]$CrtDir = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Redist\MSVC\14.50.35710\x64\Microsoft.VC145.CRT',
    [string]$OutputDir,
    [string]$ReleaseDate = (Get-Date -Format 'yyyy-MM-dd')
)
$ErrorActionPreference = 'Stop'
if (!$BundleDir) { $BundleDir = Join-Path $PSScriptRoot '..\..\distrib' }
if (!$OutputDir) { $OutputDir = Join-Path $PSScriptRoot '..\..\dist\windows\installer' }
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$bundle = (Resolve-Path -LiteralPath $BundleDir).Path
$exe = Join-Path $bundle 'Boarder.exe'
$version = ((Get-Item -LiteralPath $exe).VersionInfo.ProductVersion -split '\.')[0..2] -join '.'
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'Missing application version' }
if ($ReleaseDate -notmatch '^\d{4}-\d{2}-\d{2}$') { throw 'Invalid release date' }
$creator = Join-Path $IfwRoot 'bin\binarycreator.exe'
if (!(Test-Path -LiteralPath $creator)) { throw 'Qt Installer Framework not found' }
if (!(Test-Path -LiteralPath (Join-Path $CrtDir 'vcruntime140.dll'))) { throw 'MSVC x64 runtime not found' }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDir).Path
$installer = Join-Path $output "B-Line-Setup-$version-$ReleaseDate.exe"
$archive = Join-Path $output "B-Line-Setup-$version-$ReleaseDate.zip"
if ((Test-Path -LiteralPath $installer) -or (Test-Path -LiteralPath $archive)) {
    throw 'This version and date already exist; choose a new version or output directory'
}
$stage = Join-Path $output ('stage-' + [guid]::NewGuid().ToString('N'))
$config = Join-Path $stage 'config'
$package = Join-Path $stage 'packages\com.stargitstudio.bline'
$data = Join-Path $package 'data'
$meta = Join-Path $package 'meta'
New-Item -ItemType Directory -Path $config,$data,$meta -Force | Out-Null

# Stage only runtime files, never logs, settings, credentials or user projects.
foreach ($file in Get-ChildItem -LiteralPath $bundle -Recurse -File) {
    $allowed = $file.Extension -in @('.dll','.svg') -or
        $file.Name -in @('Boarder.exe','ffmpeg.exe','FFmpeg-LICENSE.txt','FFmpeg-source.txt')
    if (!$allowed) { continue }
    $relative = $file.FullName.Substring($bundle.Length).TrimStart('\')
    $destination = Join-Path $data $relative
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $destination
}
Get-ChildItem -LiteralPath $CrtDir -Filter '*.dll' | Copy-Item -Destination $data
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination (Join-Path $data 'B-Line-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'ifw\installscript.qs') -Destination $meta
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'RELEASE-NOTES.txt') -Destination $data

$revision = (git -C $repoRoot rev-parse HEAD).Trim()
$manifest = [ordered]@{
    product = 'B-Line'; version = $version; releaseDate = $ReleaseDate; platform = 'Windows x64'
    sourceCommit = $revision; localWindowsCompatibilityFixes = $true
    sourceWorkingTreeModified = [bool](git -C $repoRoot status --porcelain --untracked-files=no)
    buildConfiguration = 'Release'; compilerOptimization = '/O2'; linkerOptimization = '/OPT:REF /OPT:ICF'
    shotListSourceSha256 = (Get-FileHash -LiteralPath (Join-Path $repoRoot 'ShotListPresentation.cpp') -Algorithm SHA256).Hash.ToLowerInvariant()
    qt = '6.10.2'; compiler = 'MSVC 14.50 (v145)'
    executableSha256 = (Get-FileHash -LiteralPath (Join-Path $data 'Boarder.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    files = @(Get-ChildItem -LiteralPath $data -Recurse -File | ForEach-Object {
        [ordered]@{path=$_.FullName.Substring($data.Length+1).Replace('\','/'); bytes=$_.Length; sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $data 'build-manifest.json') -Encoding UTF8
@"
<?xml version="1.0" encoding="UTF-8"?>
<Installer>
  <Name>B-Line</Name><Version>$version</Version><Title>B-Line Setup</Title>
  <Publisher>Stargit Studio AB</Publisher><ProductUrl>https://syncpipeline.com/tools</ProductUrl>
  <StartMenuDir>B-Line</StartMenuDir><TargetDir>@ApplicationsDirX64@/B-Line</TargetDir>
  <RemoveTargetDir>false</RemoveTargetDir>
</Installer>
"@ | Set-Content -LiteralPath (Join-Path $config 'config.xml') -Encoding UTF8
@"
<?xml version="1.0" encoding="UTF-8"?>
<Package>
  <DisplayName>B-Line</DisplayName><Description>B-Line desktop application and runtime dependencies.</Description>
  <Version>$version</Version><ReleaseDate>$ReleaseDate</ReleaseDate>
  <Default>true</Default><Script>installscript.qs</Script>
</Package>
"@ | Set-Content -LiteralPath (Join-Path $meta 'package.xml') -Encoding UTF8

& $creator --offline-only --compression 5 -c (Join-Path $config 'config.xml') -p (Join-Path $stage 'packages') $installer
if ($LASTEXITCODE -ne 0) { throw "binarycreator failed: $LASTEXITCODE" }
Compress-Archive -LiteralPath $installer -DestinationPath $archive -CompressionLevel Optimal
$sha256 = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$sha256  $([IO.Path]::GetFileName($archive))" | Set-Content -LiteralPath "$archive.sha256" -Encoding ascii
[ordered]@{installer=$installer; archive=$archive; version=$version; sha256=$sha256; stagedBundle=$data} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'release.json') -Encoding UTF8
Write-Output "Installer: $installer"
Write-Output "Archive: $archive"
Write-Output "SHA256: $sha256"
