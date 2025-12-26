# FullFrame - High-Performance Image Tagging Application

A C++ Qt6-based image tagging application inspired by DigiKam.

## Features

- Fast thumbnail loading for large image collections
- Multi-threaded thumbnail generation
- Smart caching (LRU memory cache + disk cache)
- Tag-based organization with SQLite storage
- Modern dark UI
- Ctrl+Wheel zoom

## Building

### Requirements

- CMake 3.16+
- Qt 6.x (Core, Gui, Widgets, Concurrent, Sql)
- C++17 compatible compiler

### Build Steps

```bash
cmake -B build -S .
cmake --build build
./build/FullFrame
```

## Usage

1. Open a folder with images
2. Browse thumbnails
3. Create and assign tags
4. Filter by tags
