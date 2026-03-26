param(
  [string]$GameRoot = "C:\Program Files (x86)\Steam\steamapps\common\Guild Wars 2",
  [switch]$SkipInstall
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$distRoot = Join-Path $scriptRoot "dist"
$packageDir = Join-Path $distRoot "NexusGameWiki"
$iconPath = Join-Path $packageDir "nexusgamewiki-icon.png"
$dllPath = Join-Path $distRoot "NexusGameWiki.dll"
$cacheSearchDir = Join-Path $packageDir "cache\search"
$cachePageDir = Join-Path $packageDir "cache\pages"

New-Item -ItemType Directory -Force $distRoot, $packageDir, $cacheSearchDir, $cachePageDir | Out-Null

function Get-ZigPath {
  $zig = Get-ChildItem "C:\Users\PCD\AppData\Local\Microsoft\WinGet\Packages" -Recurse -Filter zig.exe -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName

  if (-not $zig) {
    throw "zig.exe was not found. Install zig.zig with winget first."
  }

  return $zig
}

function Write-NexusGameWikiIcon {
  param([Parameter(Mandatory = $true)][string]$OutputPath)

  $iconBase64 = "iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAGuSURBVFhH7ZUtTwNBEIYrUQSJIUGS1CAJqj8BgcCQYEBDSEhQQK9YoGRnt6kpKbS7twupRCAQKAz8AASGgEQ2BAGZwt4tc5/lQ3FPsmkz884772XFlkoFBQU5UIrPKM0XfAPbUrMlaUTlO5qhaLVaI0rzVWX4k2/4Gz1K82fl85rSsJmmwUC93v4Y9c9EGX4eMYwsEtGlEQ0euMcPojsSkVJMOoa3SsHcl74RFaXYjdVIwx/iNL7ml6FmiCtxA8QNAnizJ5ItWk1H15tYo7osn0ToYP14a5Q3dteZ8M5AeI/4e9Q+WLOadmev6/ZQizPUh+5JhA6yZrWMpu5XdrtiPtBoYLaOmkHYZrVMfYIFWaQNojF+HQ2ANey52jSfVOjgxxXUdqDhXYOoXeAiGgBr2EMNan/9CpjwlgG8CauhAWwdNaj9syuwJAVwyeMTS57B/xUg7qGhAeI01IfuScQd9A30w//hkQpenACvtE9nfxAgPPEPTT7NUAEGT/GnUfJjBFeBuQ93cZrwMYK+1mzc7WfSOT2cwnuldRep+YrSsEHrLlqzaTy0XlBQUGB5B5CD+PHf0u1IAAAAAElFTkSuQmCC"
  [System.IO.File]::WriteAllBytes($OutputPath, [Convert]::FromBase64String($iconBase64))
}

Write-NexusGameWikiIcon -OutputPath $iconPath

$zig = Get-ZigPath
$includeRoot = Join-Path $scriptRoot "vendor"

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
  (Join-Path $scriptRoot "src\NexusGameWiki.cpp"),
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

if (-not $SkipInstall) {
  $addonsRoot = Join-Path $GameRoot "addons"
  if (-not (Test-Path $addonsRoot)) {
    throw "Guild Wars 2 addons folder not found at $addonsRoot"
  }

  $targetDll = Join-Path $addonsRoot "NexusGameWiki.dll"
  $targetDir = Join-Path $addonsRoot "NexusGameWiki"

  New-Item -ItemType Directory -Force $targetDir, (Join-Path $targetDir "cache\search"), (Join-Path $targetDir "cache\pages") | Out-Null
  Copy-Item -Path $dllPath -Destination $targetDll -Force
  Copy-Item -Path (Join-Path $packageDir "*") -Destination $targetDir -Recurse -Force

  Write-Host ("Installed NexusGameWiki to {0}" -f $addonsRoot)
}

Write-Host "NexusGameWiki build complete."
