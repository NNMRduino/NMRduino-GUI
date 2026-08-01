<#
.SYNOPSIS
    Builds NMRduino GUI (Release, MinGW) and packages a self-contained Windows zip.

.DESCRIPTION
    windeployqt on Qt 5.15.2's MinGW open-source package has a known bad debug/release
    heuristic that misidentifies the stock plugin DLLs and refuses to deploy anything
    (fails with "Unable to find the platform plugin", exit code 1, nothing copied).
    This script bypasses it: it builds normally, then walks the real PE import table
    (via objdump) of the built exe and every Qt/MinGW DLL it depends on, recursively,
    and copies exactly what's needed. Platform/imageformat/style plugins aren't found
    by import-table walking (they're loaded at runtime, not linked), so the small set
    this project needs is added explicitly.

.PARAMETER QtDir
    Path to the Qt kit's install dir, e.g. E:\Qt\5.15.2\mingw81_64

.PARAMETER MingwDir
    Path to the matching MinGW toolchain, e.g. E:\Qt\Tools\mingw810_64

.EXAMPLE
    .\tools\windows_deploy.ps1 -QtDir E:\Qt\5.15.2\mingw81_64 -MingwDir E:\Qt\Tools\mingw810_64
#>
param(
    [Parameter(Mandatory=$true)][string]$QtDir,
    [Parameter(Mandatory=$true)][string]$MingwDir
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$ProFile  = Join-Path $RepoRoot "src\NMRduino.pro"
$BuildDir = Join-Path $RepoRoot "build-release"

if (-not (Test-Path $ProFile)) { throw "Can't find $ProFile" }

$proContent = Get-Content $ProFile -Raw
if ($proContent -notmatch '(?m)^\s*VERSION\s*=\s*(\S+)') { throw "No VERSION line in NMRduino.pro" }
$Version = $Matches[1]
if ($proContent -notmatch '(?m)^\s*TARGET\s*=\s*(\S+)') { throw "No TARGET line in NMRduino.pro" }
$Target = $Matches[1]

Write-Output "=== $Target $Version ==="

# --- 1. Build ---
$env:PATH = "$QtDir\bin;$MingwDir\bin;$env:PATH"
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Push-Location $BuildDir
try {
    & qmake $ProFile -spec win32-g++ "CONFIG+=release"
    if ($LASTEXITCODE -ne 0) { throw "qmake failed" }

    & mingw32-make -j4
    if ($LASTEXITCODE -ne 0) { throw "mingw32-make failed" }
} finally {
    Pop-Location
}

$ExePath = Join-Path $BuildDir "release\$Target.exe"
if (-not (Test-Path $ExePath)) { throw "Build did not produce $ExePath" }

# --- 2. Package folder ---
$DistDir = Join-Path $RepoRoot "dist\$Target-$Version-win64"
if (Test-Path $DistDir) { Remove-Item $DistDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

Copy-Item $ExePath $DistDir
foreach ($extra in @("README.md", "LICENCE", "LICENSE")) {
    $p = Join-Path $RepoRoot $extra
    if (Test-Path $p) { Copy-Item $p $DistDir }
}

# --- 3. Resolve real DLL dependencies via objdump, recursively ---
$objdump = Join-Path $MingwDir "bin\objdump.exe"
$searchDirs = @((Join-Path $QtDir "bin"), (Join-Path $MingwDir "bin"))

function Get-DllImports([string]$binaryPath) {
    & $objdump -p $binaryPath 2>$null |
        Select-String "DLL Name:\s*(\S+)" |
        ForEach-Object { $_.Matches[0].Groups[1].Value }
}

function Resolve-Dll([string]$name) {
    foreach ($dir in $searchDirs) {
        $candidate = Join-Path $dir $name
        if (Test-Path $candidate) { return $candidate }
    }
    return $null
}

$copied = @{}
function Copy-DllRecursive([string]$binaryPath, [string]$destDir) {
    foreach ($dep in (Get-DllImports $binaryPath)) {
        if ($copied.ContainsKey($dep.ToLower())) { continue }
        $resolved = Resolve-Dll $dep
        if (-not $resolved) { continue }  # system DLL (KERNEL32, USER32, msvcrt, ...) - ships with Windows
        $copied[$dep.ToLower()] = $true
        Copy-Item $resolved $destDir -Force
        Write-Output "  + $dep"
        Copy-DllRecursive $resolved $destDir
    }
}

Write-Output "Resolving runtime dependencies for $Target.exe ..."
Copy-DllRecursive $ExePath $DistDir

# --- 4. Runtime plugins (not visible to import-table walking - loaded dynamically) ---
function Add-Plugin([string]$relativePath) {
    $src = Join-Path $QtDir "plugins\$relativePath"
    if (-not (Test-Path $src)) { Write-Warning "Plugin not found: $relativePath"; return }
    $destSub = Join-Path $DistDir (Split-Path $relativePath -Parent)
    New-Item -ItemType Directory -Force -Path $destSub | Out-Null
    Copy-Item $src $destSub -Force
    Write-Output "  + [plugin] $relativePath"
    Copy-DllRecursive $src $DistDir
}

Write-Output "Adding required Qt plugins ..."
Add-Plugin "platforms\qwindows.dll"   # required for any Qt Widgets app to start on Windows

$usesSvg = Select-String -Path (Join-Path $RepoRoot "src\qrc\*.qrc") -Pattern "\.svg" -Quiet -ErrorAction SilentlyContinue
if ($usesSvg) { Add-Plugin "imageformats\qsvg.dll" }

Add-Plugin "styles\qwindowsvistastyle.dll"  # native look, small, safe to always include

# --- 5. Zip ---
$ZipPath = Join-Path $RepoRoot "dist\$Target-$Version-win64.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path $DistDir -DestinationPath $ZipPath

Write-Output ""
Write-Output "=== Done ==="
Write-Output "Folder: $DistDir"
Write-Output "Zip:    $ZipPath ($('{0:N1}' -f ((Get-Item $ZipPath).Length / 1MB)) MB)"
Write-Output ""
Write-Output "Sanity check: launching the packaged exe standalone ..."
$proc = Start-Process -FilePath (Join-Path $DistDir "$Target.exe") -PassThru
Start-Sleep -Seconds 3
$found = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
if ($found) {
    Write-Output "OK: running, window title = '$($found.MainWindowTitle)'"
    Stop-Process -Id $proc.Id -Force
} else {
    Write-Warning "Packaged exe did not stay running (exit code $($proc.ExitCode)) - inspect $DistDir manually."
}
