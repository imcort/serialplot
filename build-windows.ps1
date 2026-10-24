#Requires -Version 5.1
<#
    Build SerialPlot for Windows with MSVC (x64) and produce an NSIS installer.

    Requirements
      * Visual Studio 2022/2026 (Build Tools or IDE) with the
        "Desktop development with C++" workload: MSVC x64 toolset + Windows SDK
      * Qt 6 "msvc2022_64" kit with the SerialPort and Bluetooth modules
        (MinGW kits do NOT have a working Bluetooth backend on Windows)
      * NSIS 3.x on PATH (or in its default install directory)
      * Qwt 6.3.0 source is downloaded and built automatically when missing

    Examples
      .\build-windows.ps1
      .\build-windows.ps1 -QtDir 'C:\Qt\6.8.3\msvc2022_64' -SkipInstaller
#>
[CmdletBinding()]
param(
    [string]$QtDir     = 'C:\Qt\6.8.3\msvc2022_64',
    [string]$QwtRoot   = 'C:\Qwt-6.3.0',
    [string]$BuildDir  = (Join-Path $PSScriptRoot 'build-msvc'),
    [string]$BuildType = 'Release',
    [switch]$SkipQwt,
    [switch]$SkipInstaller
)

$ErrorActionPreference = 'Stop'
$QwtVersion = '6.3.0'
$QwtUrl     = "https://downloads.sourceforge.net/project/qwt/qwt/$QwtVersion/qwt-$QwtVersion.tar.bz2"

function Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Die($msg)  { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }
function Need($cond, $msg) { if (-not $cond) { Die $msg } }

# ---------------------------------------------------------------- MSVC + tools
Step 'Preparing the MSVC x64 environment'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
Need (Test-Path $vswhere) 'vswhere.exe not found, install Visual Studio Build Tools with the C++ workload.'

$vsPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Need $vsPath 'No Visual Studio instance with the MSVC x64 toolset was found.'

$devShell = Join-Path $vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
Need (Test-Path $devShell) "Missing DevShell module: $devShell"
Import-Module $devShell
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation `
    -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

# cmake/cpack/ninja ship with Visual Studio but are not on PATH by default
$vsCmakeBin = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$vsNinjaDir = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
foreach ($dir in @($vsCmakeBin, $vsNinjaDir)) {
    if ((Test-Path $dir) -and ($env:PATH -notlike "*$dir*")) { $env:PATH = "$dir;$env:PATH" }
}
Need (Get-Command cmake -ErrorAction SilentlyContinue) 'cmake was not found (install the "C++ CMake tools" component).'
Need (Get-Command ninja -ErrorAction SilentlyContinue) 'ninja was not found (install the "C++ CMake tools" component).'

# git is only used to embed the revision hash, but it should be found when present
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    foreach ($cand in 'C:\Program Files\Git\cmd\git.exe',
                      'C:\Program Files\Git\bin\git.exe') {
        if (Test-Path $cand) { $env:PATH = (Split-Path $cand) + ';' + $env:PATH; break }
    }
}

# NSIS is not part of Visual Studio
if (-not (Get-Command makensis -ErrorAction SilentlyContinue)) {
    $nsis = 'C:\Program Files (x86)\NSIS'
    if (Test-Path (Join-Path $nsis 'makensis.exe')) { $env:PATH = "$nsis;$env:PATH" }
}
if (-not $SkipInstaller -and -not (Get-Command makensis -ErrorAction SilentlyContinue)) {
    Write-Warning 'makensis not found, the NSIS installer cannot be produced. Install NSIS 3.x or pass -SkipInstaller.'
}

# ------------------------------------------------------------------------ Qt
Step 'Checking Qt'
$QtDir = (Resolve-Path $QtDir).Path
foreach ($f in 'bin\qmake.exe', 'bin\windeployqt.exe', 'bin\Qt6Core.dll',
               'bin\Qt6SerialPort.dll', 'bin\Qt6Bluetooth.dll') {
    Need (Test-Path (Join-Path $QtDir $f)) `
        "Qt is incomplete: $f missing under $QtDir. Use the msvc2022_64 kit and install the SerialPort + Bluetooth modules."
}
$env:PATH = (Join-Path $QtDir 'bin') + ';' + $env:PATH

# ----------------------------------------------------------------------- Qwt
$qwtHeader = Join-Path $QwtRoot 'include\qwt_global.h'
$qwtLib    = Join-Path $QwtRoot 'lib\qwt.lib'
$qwtDll    = Join-Path $QwtRoot 'lib\qwt.dll'
$qwtStatic = (Test-Path $qwtHeader) -and (Test-Path $qwtLib) -and -not (Test-Path $qwtDll)
if (-not $SkipQwt -and -not $qwtStatic) {
    Step "Downloading and building Qwt $QwtVersion (static, MSVC)"
    if (Test-Path $qwtDll) { Remove-Item -Force $qwtDll }   # stale shared build
    $work = Join-Path $env:TEMP "serialplot-qwt-$QwtVersion"
    if (Test-Path $work) { Remove-Item -Recurse -Force $work }
    New-Item -ItemType Directory -Path $work | Out-Null

    $archive = Join-Path $work "qwt-$QwtVersion.tar.bz2"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
        # sourceforge.net answers with an HTML interstitial page, the redirect
        # from downloads.sourceforge.net already points at the real mirror file
        $head   = & curl.exe -sS -I --max-time 60 $QwtUrl 2>&1
        $mirror = ($head | Select-String -Pattern '^location:\s*(.+)$' -AllMatches).Matches |
                  Select-Object -First 1
        if ($mirror) {
            $target = $mirror.Groups[1].Value.Trim()
        } else {
            $target = $QwtUrl
        }
        & curl.exe -L --silent --show-error --max-time 600 -o $archive $target
    } else {
        Invoke-WebRequest -Uri $QwtUrl -OutFile $archive -UseBasicParsing
    }
    Need ((Test-Path $archive) -and ((Get-Item $archive).Length -gt 1MB)) 'Downloading Qwt failed.'
    Need ([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($archive)[0..2]) -eq 'BZh') `
        'The Qwt download is not a bzip2 file (SourceForge served an HTML page). Download qwt-6.3.0.tar.bz2 manually, extract it and pass -QwtRoot <installed-qwt> -SkipQwt.'
    & tar -xjf $archive -C $work
    Need ($LASTEXITCODE -eq 0) 'Extracting the Qwt archive failed (tar.exe is part of Windows 10/11).'

    $src = Join-Path $work "qwt-$QwtVersion"

    # static library: disable the optional components that need extra modules
    $cfgPath = Join-Path $src 'qwtconfig.pri'
    $cfg = [IO.File]::ReadAllText($cfgPath)
    foreach ($opt in 'QwtDll', 'QwtPolar', 'QwtWidgets', 'QwtSvg', 'QwtOpenGL',
                     'QwtDesigner', 'QwtDesignerSelfContained', 'QwtExamples',
                     'QwtPlayground', 'QwtTests') {
        $cfg = [regex]::Replace($cfg, "(?m)^[ \t]*QWT_CONFIG[ \t]*\+=[ \t]*$opt\b",
                                "# QWT_CONFIG += $opt")
    }
    $cfg = [regex]::Replace($cfg, '(?m)^[ \t]*QWT_INSTALL_PREFIX[ \t]*=.*$',
                            'QWT_INSTALL_PREFIX = ' + ($QwtRoot -replace '\\', '/'))
    # belt and braces: a later assignment always wins over the ones above
    $cfg += "`r`nQWT_CONFIG -= QwtDll`r`n"
    [IO.File]::WriteAllText($cfgPath, $cfg, [Text.Encoding]::ASCII)
    Need (([IO.File]::ReadAllText($cfgPath)) -notmatch '(?m)^[ \t]*QWT_CONFIG[ \t]*\+=[ \t]*QwtDll\b') `
        'Could not switch Qwt to a static build (qwtconfig.pri).'

    # release only
    $bldPath = Join-Path $src 'qwtbuild.pri'
    $bld = [IO.File]::ReadAllText($bldPath)
    $bld = [regex]::Replace($bld, '(?m)^[ \t]*CONFIG[ \t]*\+=[ \t]*debug_and_release.*$', 'CONFIG += release')
    $bld = [regex]::Replace($bld, '(?m)^[ \t]*CONFIG[ \t]*\+=[ \t]*build_all.*$', '# CONFIG += build_all')
    [IO.File]::WriteAllText($bldPath, $bld, [Text.Encoding]::ASCII)

    # build only the library: qwt.pro would also build designer, examples and
    # playground, src.pro installs the headers and the library on its own
    Push-Location (Join-Path $src 'src')
    try {
        & (Join-Path $QtDir 'bin\qmake.exe') src.pro
        Need ($LASTEXITCODE -eq 0) 'qmake failed for Qwt.'
        & nmake /nologo
        Need ($LASTEXITCODE -eq 0) 'Building Qwt failed.'
        & nmake /nologo install
        Need ($LASTEXITCODE -eq 0) 'Installing Qwt failed.'
    } finally {
        Pop-Location
    }
}
Need ((Test-Path $qwtHeader) -and (Test-Path $qwtLib)) `
    "Qwt was not found at $QwtRoot (expected include\qwt_global.h and lib\qwt.lib)."
if (Test-Path $qwtDll) {
    # a leftover qwt.dll would be picked up instead of the static library
    Remove-Item -Force $qwtDll
    Write-Warning "Removed stale $qwtDll, the build expects a static Qwt."
}

# -------------------------------------------------------------------- build
Step 'Configuring serialplot'
& cmake -S $PSScriptRoot -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DBUILD_QWT=OFF" "-DQWT_ROOT=$QwtRoot" "-DCMAKE_PREFIX_PATH=$QtDir"
Need ($LASTEXITCODE -eq 0) 'CMake configuration failed.'

Step 'Building'
& cmake --build $BuildDir
Need ($LASTEXITCODE -eq 0) 'Build failed.'

# --------------------------------------------------------------- deploy/check
Step 'Installing into a staging directory (runs windeployqt)'
$stage = Join-Path $BuildDir 'stage'
& cmake --install $BuildDir --prefix $stage
Need ($LASTEXITCODE -eq 0) 'Install/staging failed.'

$missing = @()
foreach ($f in 'bin\serialplot.exe', 'bin\Qt6Core.dll', 'bin\Qt6SerialPort.dll',
               'bin\Qt6Bluetooth.dll', 'bin\Qt6Svg.dll', 'bin\platforms\qwindows.dll') {
    if (-not (Test-Path (Join-Path $stage $f))) { $missing += $f }
}
if ($missing.Count -gt 0) { Write-Warning ("Staged tree is missing: " + ($missing -join ', ')) }
else { Write-Host "Staging directory looks complete: $stage" -ForegroundColor Green }

# ------------------------------------------------------------------ installer
if (-not $SkipInstaller -and (Get-Command makensis -ErrorAction SilentlyContinue)) {
    Step 'Creating the NSIS installer'
    & cpack --config (Join-Path $BuildDir 'CPackConfig.cmake') -G NSIS -B $BuildDir
    Need ($LASTEXITCODE -eq 0) 'CPack/NSIS failed.'
    Get-ChildItem $BuildDir -Filter '*win64.exe' | ForEach-Object { Write-Host "Installer: $($_.FullName)" -ForegroundColor Green }
}
