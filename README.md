# FullFrame - High-Performance Media Tagging Application

A fast, modern media tagging app built with C++ and Qt6. Inspired by DigiKam's efficient architecture, but built from the ground up to handle massive collections without slowing down.

## Features

### Media Support
- **Images**: JPG, PNG, GIF, WebP, BMP, TIFF, SVG, and more
- **Videos**: MP4, MKV, WebM, MOV, AVI, WMV, 3GP, and 30+ other formats
- **Audio**: MP3, M4A, WAV, FLAC, OGG, AAC, WMA
- **Video Thumbnails**: Extracts actual frames from videos (requires FFmpeg)
- **Audio Placeholders**: Nice looking placeholders for audio files

### Performance
- **Blazing Fast**: Load thousands of thumbnails without lag
- **Multi-threaded**: Uses all your CPU cores for thumbnail generation
- **Smart Caching**: LRU cache keeps frequently viewed items in memory
- **Disk Caching**: FreeDesktop standard thumbnail cache (persists between sessions)
- **Lazy Loading**: Only loads what you can see
- **Preloading**: Automatically loads thumbnails just outside the viewport

### Tagging System
- **Create Tags**: Add custom tags with colors
- **Tag Hotkeys**: Assign keyboard shortcuts to tags (press the key to tag!)
- **Tag Filtering**: Click any tag to filter your collection
- **Show Untagged**: Filter to see only files without tags
- **Multi-select**: Tag multiple files at once
- **Tag Autocomplete**: Type-ahead when adding tags

### User Interface
- **Dark Theme**: Easy on the eyes
- **Zoom**: Ctrl+Wheel to zoom thumbnails in/out
- **Drag & Drop**: Drop folders onto the window to open them
- **Media Preview**: Click any file to see a larger preview
- **Video Playback**: Play videos right in the app (if Qt Multimedia is installed)
- **Audio Playback**: Play audio files in the app (if Qt Multimedia is installed)

## Building

### Requirements

- **CMake** 3.16 or newer
- **Qt 6.x** (Core, Gui, Widgets, Concurrent, Sql)
- **C++17** compatible compiler
- **FFmpeg** (optional, for video thumbnails - will auto-detect if installed)

### Windows (MSVC)

**Important:** When using MSVC Qt builds, you must specify the Visual Studio generator:

```powershell
# Configure (adjust Qt path as needed - replace 6.10.2 with your version)
cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"

# Build
cmake --build build --config Release

# Run
.\build\Release\FullFrame.exe
```

**Alternative:** If you have Qt built with MinGW:

**Easiest method - use the configure script:**

```powershell
# Configure (auto-detects Qt version, or specify: .\configure-mingw.ps1 6.10.2)
.\configure-mingw.ps1

# Build
cmake --build build

# Run
.\build\FullFrame.exe
```

**Manual method:**

```powershell
# Add Qt's MinGW to PATH (adjust path if your Qt MinGW is in a different location)
$env:PATH = "C:/Qt/Tools/mingw1310_64/bin;$env:PATH"

# Configure (adjust Qt path as needed - replace 6.x.x with your version)
cmake -G "MinGW Makefiles" -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/mingw_64"

# Build
cmake --build build

# Run
.\build\FullFrame.exe
```

**Note:** 
- You need to add MinGW to PATH so CMake can find the compiler DLLs. Qt's MinGW is typically at `C:/Qt/Tools/mingw1310_64/bin` (or similar version number).
- After building, Qt DLLs are automatically deployed using `windeployqt`. If you get DLL errors, run `windeployqt build/FullFrame.exe` manually.

### Linux/macOS

```bash
# Configure
cmake -B build -S .

# Build
cmake --build build

# Run
./build/FullFrame
```

## Usage

1. **Open a Folder**: Click "Open Folder" or drag-drop a folder onto the window
2. **Browse**: Scroll through your media. Use Ctrl+Wheel to zoom thumbnails
3. **Tag Files**: 
   - Select one or more files in the grid
   - Type a tag name in the sidebar and click "+" (or press Enter)
   - Or click an existing tag to apply it
4. **Filter**: Click any tag in the sidebar to show only files with that tag
5. **Show Untagged**: Click "⊘ Untagged" to see files without any tags

## How It Works (Technical Stuff)

The app uses several tricks to stay fast:

1. **Thread Pool**: Parallel thumbnail generation across all CPU cores
2. **LRU Cache**: Keeps 500 images + 200 pixmaps in memory by default
3. **Disk Cache**: Thumbnails saved to `~/.cache/thumbnails` (FreeDesktop standard)
4. **Batched Layout**: Processes 50 items at a time for smooth scrolling
5. **Debounced Preloading**: 50ms delay prevents loading spam during fast scrolling
6. **EXIF Thumbnails**: Uses embedded JPEG thumbnails when available (instant!)
7. **FFmpeg Integration**: Extracts video frames for thumbnails (finds FFmpeg automatically)

### Architecture

- **ThumbnailCache**: Thread-safe image cache + main-thread pixmap cache
- **ThumbnailLoadThread**: Background thread pool for parallel loading
- **ThumbnailCreator**: Generates thumbnails (images, videos, audio)
- **ImageThumbnailModel**: Efficient data model with tag filtering
- **ImageGridView**: Lazy-loading grid view
- **TagManager**: SQLite-based tag storage

## Video Thumbnails

FullFrame can extract actual frames from videos for thumbnails! It uses FFmpeg if available. The app will automatically find FFmpeg if it's:
- In your PATH
- Installed via WinGet
- In common installation locations (C:\ffmpeg, Scoop, Chocolatey, etc.)

If FFmpeg isn't found, videos will show a nice placeholder with a play button.

## Coming Soon

- **Tagging Mode**: A dedicated interface for bulk tagging operations
- More features as we build them!

## License

MIT License - do whatever you want with it!
