param(
  [string]$GameRoot = "C:\Program Files (x86)\Steam\steamapps\common\Guild Wars 2",
  [switch]$SkipInstall
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$distRoot = Join-Path $scriptRoot "dist"
$dllPath = Join-Path $distRoot "NexusGameWiki.dll"
$legacyPackageDir = Join-Path $distRoot "NexusGameWiki"
$releaseRoot = Join-Path $scriptRoot "release"

New-Item -ItemType Directory -Force $distRoot | Out-Null
if (Test-Path $legacyPackageDir) {
  Remove-Item -Path $legacyPackageDir -Recurse -Force
}

function Get-AddonVersion {
  param([Parameter(Mandatory = $true)][string]$SourcePath)

  $source = Get-Content -Path $SourcePath -Raw

  $major = [regex]::Match($source, 'addonDef\.Version\.Major\s*=\s*(\d+);').Groups[1].Value
  $minor = [regex]::Match($source, 'addonDef\.Version\.Minor\s*=\s*(\d+);').Groups[1].Value
  $build = [regex]::Match($source, 'addonDef\.Version\.Build\s*=\s*(\d+);').Groups[1].Value
  $revision = [regex]::Match($source, 'addonDef\.Version\.Revision\s*=\s*(\d+);').Groups[1].Value

  if (-not $major -or -not $minor -or -not $build -or -not $revision) {
    throw "Failed to parse addon version from $SourcePath"
  }

  return "$major.$minor.$build.$revision"
}

function Get-ZigPath {
  $zig = Get-ChildItem "C:\Users\PCD\AppData\Local\Microsoft\WinGet\Packages" -Recurse -Filter zig.exe -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName

  if (-not $zig) {
    throw "zig.exe was not found. Install zig.zig with winget first."
  }

  return $zig
}

$zig = Get-ZigPath
$includeRoot = Join-Path $scriptRoot "vendor"
$sourcePath = Join-Path $scriptRoot "src\NexusGameWiki.cpp"
$addonVersion = Get-AddonVersion -SourcePath $sourcePath
$releaseZipPath = Join-Path $releaseRoot ("NexusGameWiki-v{0}.zip" -f $addonVersion)

$args = @(
  "c++",
  "-std=c++17",
  "-shared",
  "-O2",
  "-target", "x86_64-windows-gnu",
  "-DWIN32",
  "-D_WINDOWS",
  "-D_USRDLL",
  "-Wno-macro-redefined",
  "-Wno-nontrivial-memcall",
  "-I" + (Join-Path $includeRoot "nexus"),
  "-I" + (Join-Path $includeRoot "mumble"),
  "-I" + (Join-Path $includeRoot "imgui"),
  "-I" + (Join-Path $includeRoot "nlohmann"),
  $sourcePath,
  (Join-Path $scriptRoot "vendor\imgui\imgui.cpp"),
  (Join-Path $scriptRoot "vendor\imgui\imgui_draw.cpp"),
  (Join-Path $scriptRoot "vendor\imgui\imgui_tables.cpp"),
  (Join-Path $scriptRoot "vendor\imgui\imgui_widgets.cpp"),
  "-luser32",
  "-lgdi32",
  "-lole32",
  "-luuid",
  "-lshell32",
  "-lwinhttp",
  "-lwinpthread",
  "-o", $dllPath
)

Write-Host "Building NexusGameWiki.dll with zig..."
& $zig @args
if ($LASTEXITCODE -ne 0) {
  throw "zig failed to build NexusGameWiki.dll"
}

Write-Host ("Built {0}" -f $dllPath)

New-Item -ItemType Directory -Force $releaseRoot | Out-Null
if (Test-Path $releaseZipPath) {
  Remove-Item -Path $releaseZipPath -Force
}

Compress-Archive -Path $dllPath -DestinationPath $releaseZipPath -CompressionLevel Optimal
Write-Host ("Packaged {0}" -f $releaseZipPath)

if (-not $SkipInstall) {
  $addonsRoot = Join-Path $GameRoot "addons"
  New-Item -ItemType Directory -Force $addonsRoot | Out-Null

  $targetDll = Join-Path $addonsRoot "NexusGameWiki.dll"
  $legacyIconPath = Join-Path $addonsRoot "NexusGameWiki\\nexusgamewiki-icon.png"

  Copy-Item -Path $dllPath -Destination $targetDll -Force
  if (Test-Path $legacyIconPath) {
    Remove-Item -Path $legacyIconPath -Force
  }

  Write-Host ("Installed NexusGameWiki to {0}" -f $addonsRoot)
}

Write-Host "NexusGameWiki build complete."
