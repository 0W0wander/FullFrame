cmake_minimum_required(VERSION 3.16)
project(FullFrame VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Try to find Qt6 automatically in common locations
if(NOT DEFINED CMAKE_PREFIX_PATH)
    # Common Qt installation paths on Windows
    # Check for MSVC builds first (most common on Windows)
    if(EXISTS "C:/Qt/6.10.1/msvc2022_64")
        set(CMAKE_PREFIX_PATH "C:/Qt/6.10.1/msvc2022_64" ${CMAKE_PREFIX_PATH})
    elseif(EXISTS "C:/Qt/6.9.0/msvc2022_64")
        set(CMAKE_PREFIX_PATH "C:/Qt/6.9.0/msvc2022_64" ${CMAKE_PREFIX_PATH})
    elseif(EXISTS "C:/Qt/6.8.0/msvc2022_64")
        set(CMAKE_PREFIX_PATH "C:/Qt/6.8.0/msvc2022_64" ${CMAKE_PREFIX_PATH})
    elseif(EXISTS "C:/Qt/6.9.0/mingw_64")
        set(CMAKE_PREFIX_PATH "C:/Qt/6.9.0/mingw_64" ${CMAKE_PREFIX_PATH})
    elseif(EXISTS "C:/Qt/6.8.0/mingw_64")
        set(CMAKE_PREFIX_PATH "C:/Qt/6.8.0/mingw_64" ${CMAKE_PREFIX_PATH})
    endif()
endif()

# Find Qt6 core components (required)
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
    Sql
)

if(NOT Qt6_FOUND)
    message(FATAL_ERROR 
        "Qt6 not found!\n"
        "Please set CMAKE_PREFIX_PATH to your Qt6 installation:\n"
        "  cmake -B build -S . -DCMAKE_PREFIX_PATH=\"C:/Qt/6.x.x/mingw_64\"\n"
        "Or set Qt6_DIR:\n"
        "  cmake -B build -S . -DQt6_DIR=\"C:/Qt/6.x.x/mingw_64/lib/cmake/Qt6\""
    )
endif()

# Optional: Qt Multimedia for video/audio playback
find_package(Qt6Multimedia QUIET)
find_package(Qt6MultimediaWidgets QUIET)
if(Qt6Multimedia_FOUND AND Qt6MultimediaWidgets_FOUND)
    message(STATUS "Qt Multimedia found - video/audio playback enabled")
    add_compile_definitions(HAVE_QT_MULTIMEDIA)
else()
    message(STATUS "Qt Multimedia not found - video/audio playback will show placeholders only")
    message(STATUS "To enable video playback, install Qt Multimedia using Qt Maintenance Tool")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/core/thumbnailcache.cpp
    src/core/thumbnailloadthread.cpp
    src/core/thumbnailcreator.cpp
    src/core/tagmanager.cpp
    src/models/imagethumbnailmodel.cpp
    src/views/imagegridview.cpp
    src/views/thumbnaildelegate.cpp
    src/views/taggingmodewidget.cpp
    src/widgets/tagsidebar.cpp
)

set(HEADERS
    src/mainwindow.h
    src/core/thumbnailcache.h
    src/core/thumbnailloadthread.h
    src/core/thumbnailcreator.h
    src/core/tagmanager.h
    src/models/imagethumbnailmodel.h
    src/views/imagegridview.h
    src/views/thumbnaildelegate.h
    src/views/taggingmodewidget.h
    src/widgets/tagsidebar.h
)

# Windows application icon (for .exe file icon)
if(WIN32)
    set(APP_ICON_RESOURCE "${CMAKE_SOURCE_DIR}/Assets/app.rc")
endif()

# Qt resources (for in-app icon in taskbar/window title)
set(APP_RESOURCES "${CMAKE_SOURCE_DIR}/Assets/resources.qrc")

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${APP_ICON_RESOURCE} ${APP_RESOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/models
    ${CMAKE_SOURCE_DIR}/src/views
    ${CMAKE_SOURCE_DIR}/src/widgets
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Sql
)

# Conditionally link Qt Multimedia if available
if(Qt6Multimedia_FOUND AND Qt6MultimediaWidgets_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Multimedia
        Qt6::MultimediaWidgets
    )
endif()

# Platform-specific settings
# Temporarily disabled WIN32_EXECUTABLE for debugging
# if(WIN32)
#     set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
# endif()

