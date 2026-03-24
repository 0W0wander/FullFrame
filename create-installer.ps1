# PowerShell script to build FullFrame and create a Windows installer
# This script automates the build and installer creation process

param(
    [string]$QtVersion = "",
    [string]$Config = "Release",
    [switch]$SkipBuild = $false,
    [string]$InnoSetupPath = ""
)

$ErrorActionPreference = "Stop"

Write-Host "=== FullFrame Installer Builder ===" -ForegroundColor Cyan
Write-Host ""

# Check if Inno Setup is installed
if ($InnoSetupPath -and (Test-Path $InnoSetupPath)) {
    # User specified path
    $innoSetupPath = $InnoSetupPath
    Write-Host "Using Inno Setup from: $innoSetupPath" -ForegroundColor Green
} else {
    # Try to find it automatically
    $innoSetupPath = $null
    
    # First, try to find it in PATH (works for winget installations after PATH refresh)
    try {
        $innoSetupCmd = Get-Command iscc -ErrorAction Stop
        $innoSetupPath = $innoSetupCmd.Source
    } catch {
        # Not in PATH, check common installation locations
        $searchPaths = @(
            "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
            "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
            "${env:ProgramFiles(x86)}\Inno Setup 5\ISCC.exe",
            "${env:ProgramFiles}\Inno Setup 5\ISCC.exe"
        )
        
        foreach ($path in $searchPaths) {
            if (Test-Path $path) {
                $innoSetupPath = $path
                break
            }
        }
        
        # Check winget installation location (common paths)
        if (-not $innoSetupPath) {
            $wingetPaths = @(
                "${env:LOCALAPPDATA}\Microsoft\WinGet\Links\ISCC.exe",
                "${env:ProgramFiles}\WindowsApps\*InnoSetup*\ISCC.exe"
            )
            foreach ($pattern in $wingetPaths) {
                $found = Get-ChildItem -Path (Split-Path $pattern -Parent) -Filter (Split-Path $pattern -Leaf) -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
                if ($found) {
                    $innoSetupPath = $found.FullName
                    break
                }
            }
        }
    }
}

if (-not $innoSetupPath) {
    Write-Host "ERROR: Inno Setup not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install Inno Setup:" -ForegroundColor Yellow
    Write-Host "  winget install JRSoftware.InnoSetup" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Or download from: https://jrsoftware.org/isdl.php" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "If Inno Setup is installed but not found, you can specify the path:" -ForegroundColor Yellow
    Write-Host "  .\create-installer.ps1 -InnoSetupPath `"C:\Path\To\ISCC.exe`"" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Note: After installing with winget, you may need to:" -ForegroundColor Yellow
    Write-Host "  1. Close and reopen PowerShell/terminal" -ForegroundColor Yellow
    Write-Host "  2. Or manually add Inno Setup to your PATH" -ForegroundColor Yellow
    exit 1
}

Write-Host "Found Inno Setup at: $innoSetupPath" -ForegroundColor Green

# Build the project if not skipped
if (-not $SkipBuild) {
    Write-Host ""
    Write-Host "=== Building FullFrame ===" -ForegroundColor Cyan
    
    # Check if executable already exists (already built)
    $exeExists = (Test-Path "build\FullFrame.exe") -or (Test-Path "build\Release\FullFrame.exe")
    
    # Check if build directory exists and has been configured
    if (-not (Test-Path "build\CMakeCache.txt")) {
        Write-Host "Build directory not configured. Running configure script..." -ForegroundColor Yellow
        
        if ($QtVersion) {
            & .\configure-mingw.ps1 $QtVersion
        } else {
            & .\configure-mingw.ps1
        }
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: Configuration failed!" -ForegroundColor Red
            exit 1
        }
    }
    
    # Build the project (only if executable doesn't exist or user wants to rebuild)
    if (-not $exeExists) {
        Write-Host "Building project..." -ForegroundColor Yellow
        cmake --build build --config $Config
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: Build failed!" -ForegroundColor Red
            exit 1
        }
        
        Write-Host "Build completed successfully!" -ForegroundColor Green
    } else {
        Write-Host "Executable already exists. Skipping build." -ForegroundColor Green
        Write-Host "Use --SkipBuild to skip this check and force a rebuild." -ForegroundColor Gray
    }
} else {
    Write-Host "Skipping build (--SkipBuild flag set)" -ForegroundColor Yellow
}

# Determine the build output directory
$buildOutputDir = "build"
if ($Config -eq "Release" -and (Test-Path "build\Release\FullFrame.exe")) {
    $buildOutputDir = "build\Release"
} elseif (-not (Test-Path "build\FullFrame.exe")) {
    Write-Host "ERROR: FullFrame.exe not found!" -ForegroundColor Red
    Write-Host "Checked: build\FullFrame.exe and build\Release\FullFrame.exe" -ForegroundColor Yellow
    Write-Host "Please build the project first." -ForegroundColor Yellow
    exit 1
}

$exePath = Join-Path $buildOutputDir "FullFrame.exe"
Write-Host "Using build output directory: $buildOutputDir" -ForegroundColor Green

# Check if windeployqt has been run (check for Qt DLLs)
$qtDllPath = Join-Path $buildOutputDir "Qt6Core.dll"
if (-not (Test-Path $qtDllPath)) {
    Write-Host "WARNING: Qt DLLs not found. Running windeployqt..." -ForegroundColor Yellow
    
    # Try to find windeployqt
    $windeployqt = @(
        "C:\Qt\6.10.1\mingw_64\bin\windeployqt.exe",
        "C:\Qt\6.9.0\mingw_64\bin\windeployqt.exe",
        "C:\Qt\6.8.0\mingw_64\bin\windeployqt.exe",
        "C:\Qt\6.10.1\msvc2022_64\bin\windeployqt.exe",
        "C:\Qt\6.9.0\msvc2022_64\bin\windeployqt.exe",
        "C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    
    if ($windeployqt) {
        Write-Host "Running windeployqt..." -ForegroundColor Yellow
        & $windeployqt $exePath
    } else {
        Write-Host "WARNING: windeployqt not found. Qt DLLs may be missing from the installer." -ForegroundColor Yellow
    }
}

# Create installer directory if it doesn't exist
if (-not (Test-Path "installer")) {
    New-Item -ItemType Directory -Path "installer" | Out-Null
}

# Create the installer
Write-Host ""
Write-Host "=== Creating Installer ===" -ForegroundColor Cyan
Write-Host "Running Inno Setup Compiler..." -ForegroundColor Yellow

# Pass the build directory to Inno Setup
$buildDirForInno = $buildOutputDir.Replace('\', '\\')
& $innoSetupPath "/DBuildDir=$buildDirForInno" "installer.iss"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Installer creation failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=== Success! ===" -ForegroundColor Green
$installerPath = Get-ChildItem "installer\FullFrame-Setup-*.exe" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($installerPath) {
    Write-Host "Installer created: $($installerPath.FullName)" -ForegroundColor Green
    $size = [math]::Round($installerPath.Length / 1MB, 2)
    Write-Host "Size: $size MB" -ForegroundColor Green
} else {
    Write-Host "Installer may have been created, but could not find the output file." -ForegroundColor Yellow
}
