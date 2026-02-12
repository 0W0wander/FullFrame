# FullFrame — Lightweight Media Tagging App

**FullFrame** is a fast and lightweight media tagging application built with **Qt6** and **C++**. Inspired by DigiKam, it focuses on **quick, efficient tagging** rather than full-fledged media management.

## Features

- **Tagging Mode**: Browse media one at a time and quickly assign tags using the keyboard.  
- **Tag Hotkeys**: Map your most-used tags to keys for instant tagging.  
- **Tag Albums**: Organize images, videos, and audio files into albums and apply tags across the album.  
- **Prebuilt Classifications**: Use 1–5 hotkeys for ranking media and `F` for marking favorites.  
- **Tag Filtering & Sorting**: Filter media by tags, ranking, or creation date and sort them easily.  
- **Wide File Type Support**: Works with images (JPEG, PNG, GIF, RAW), videos (MP4, MKV, AVI), and audio files (MP3, WAV, FLAC).  



## Getting Started

You can either **run the installer from the latest GitHub release** or **build from source**.

### Building from Source

#### Requirements
- **CMake** 3.16 or newer  
- **Qt 6.x** (Core, Gui, Widgets, Concurrent, Sql)  
- **C++17** compatible compiler  
- **FFmpeg** (optional, for video thumbnails – auto-detected if installed)  

---

### Windows (MSVC)

```powershell
# Configure (adjust Qt path as needed)
cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"

# Build
cmake --build build --config Release

# Run
.\build\Release\FullFrame.exe
```
##### Alternative (Qt built with MinGW)
```powershell
# Configure (auto-detects Qt version, or specify: .\configure-mingw.ps1 6.10.2)
.\configure-mingw.ps1

# Build
cmake --build build

# Run
.\build\FullFrame.exe
```
### Linux / macOS
```bash
# Configure
cmake -B build -S .

# Build
cmake --build build

# Run
./build/FullFrame
```
