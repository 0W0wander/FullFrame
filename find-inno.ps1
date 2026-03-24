# Helper script to find Inno Setup installation
Write-Host "Searching for Inno Setup..." -ForegroundColor Cyan

# Try Get-Command first (if in PATH)
try {
    $cmd = Get-Command iscc -ErrorAction Stop
    Write-Host "Found in PATH: $($cmd.Source)" -ForegroundColor Green
    exit 0
} catch {
    Write-Host "Not found in PATH" -ForegroundColor Yellow
}

# Check common installation locations
$searchPaths = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles(x86)}\Inno Setup 5\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 5\ISCC.exe"
)

foreach ($path in $searchPaths) {
    if (Test-Path $path) {
        Write-Host "Found: $path" -ForegroundColor Green
        Write-Host ""
        Write-Host "Use this path:" -ForegroundColor Yellow
        Write-Host "  .\create-installer.ps1 -InnoSetupPath `"$path`"" -ForegroundColor Cyan
        exit 0
    }
}

Write-Host "Inno Setup not found in common locations." -ForegroundColor Red
Write-Host ""
Write-Host "Try:" -ForegroundColor Yellow
Write-Host "1. Close and reopen PowerShell (to refresh PATH)" -ForegroundColor Cyan
Write-Host "2. Or manually find ISCC.exe and specify the path:" -ForegroundColor Cyan
Write-Host "   .\create-installer.ps1 -InnoSetupPath `"C:\Path\To\ISCC.exe`"" -ForegroundColor Cyan
