# Releasing FullFrame

## Bump the version

Edit **`VERSION`** first (this is the source of truth), then update the same number in these files:

| File | What to change |
|------|----------------|
| `VERSION` | The version string, e.g. `1.2.0` |
| `installer.iss` | `#define MyAppVersion "1.2.0"` |
| `CMakeLists.txt` | `project(FullFrame VERSION 1.2.0 ...)` |
| `src/main.cpp` | `app.setApplicationVersion("1.2.0")` |
| `src/mainwindow.cpp` | About dialog: `Version 1.2.0` |

Use a three-part version (`major.minor.patch`) so the Windows installer filename stays consistent: `FullFrame-Setup-1.2.0.exe`.

## Build the installer

Requirements: **Inno Setup 6** (install with `winget install JRSoftware.InnoSetup` if needed).

From the repo root in PowerShell:

```powershell
# Build Release + package (runs configure/build if needed)
.\create-installer.ps1

# Or skip rebuild if build\Release\FullFrame.exe is already up to date
.\create-installer.ps1 -SkipBuild
```

Output: `installer\FullFrame-Setup-<version>.exe`

## Publish to GitHub

Commit your changes, push, then create a release and attach the installer:

```powershell
git add -A
git commit -m "Release v1.2.0"
git push origin main

gh release create v1.2.0 `
  --title "FullFrame 1.2.0" `
  --notes "See commit history for changes." `
  "installer/FullFrame-Setup-1.2.0.exe"
```

Replace `v1.2.0` and the installer filename with your new version each time.
