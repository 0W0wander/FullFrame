# Configure script for MinGW builds
# This script sets up the MinGW environment and runs CMake

# Find Qt's MinGW installation
$mingwPaths = @(
    "C:/Qt/Tools/mingw1310_64",
    "C:/Qt/Tools/mingw1120_64",
    "C:/Qt/Tools/mingw1121_64",
    "C:/Qt/Tools/mingw1122_64",
    "C:/Qt/Tools/mingw1123_64"
)

$mingwPath = $null
foreach ($path in $mingwPaths) {
    if (Test-Path "$path/bin/gcc.exe") {
        $mingwPath = $path
        break
    }
}

if (-not $mingwPath) {
    Write-Host "ERROR: Could not find Qt MinGW installation." -ForegroundColor Red
    Write-Host "Please install Qt with MinGW support or update the paths in this script." -ForegroundColor Yellow
    exit 1
}

Write-Host "Found MinGW at: $mingwPath" -ForegroundColor Green

# Add MinGW to PATH
$env:PATH = "$mingwPath/bin;$env:PATH"

# Get Qt version from user or use default
$qtVersion = $args[0]
if (-not $qtVersion) {
    # Try to auto-detect
    $qtDirs = Get-ChildItem "C:/Qt" -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } | Sort-Object Name -Descending
    if ($qtDirs.Count -gt 0) {
        $qtVersion = $qtDirs[0].Name
        Write-Host "Auto-detected Qt version: $qtVersion" -ForegroundColor Green
    } else {
        Write-Host "ERROR: Could not auto-detect Qt version." -ForegroundColor Red
        Write-Host "Usage: .\configure-mingw.ps1 [QtVersion]" -ForegroundColor Yellow
        Write-Host "Example: .\configure-mingw.ps1 6.10.2" -ForegroundColor Yellow
        exit 1
    }
}

$qtMingwPath = "C:/Qt/$qtVersion/mingw_64"
if (-not (Test-Path $qtMingwPath)) {
    Write-Host "ERROR: Qt MinGW installation not found at: $qtMingwPath" -ForegroundColor Red
    exit 1
}

Write-Host "Configuring with Qt: $qtMingwPath" -ForegroundColor Green

# Run CMake
cmake -G "MinGW Makefiles" -B build -S . -DCMAKE_PREFIX_PATH="$qtMingwPath"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nConfiguration successful! Now run: cmake --build build" -ForegroundColor Green
} else {
    Write-Host "`nConfiguration failed!" -ForegroundColor Red
    exit 1
}
