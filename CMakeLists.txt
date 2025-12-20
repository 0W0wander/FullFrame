cmake_minimum_required(VERSION 3.16)
project(FullFrame VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Sql)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/core/thumbnailcache.cpp
    src/core/thumbnailloadthread.cpp
    src/core/thumbnailcreator.cpp
    src/models/imagethumbnailmodel.cpp
    src/views/imagegridview.cpp
    src/views/thumbnaildelegate.cpp
)

set(HEADERS
    src/mainwindow.h
    src/core/thumbnailcache.h
    src/core/thumbnailloadthread.h
    src/core/thumbnailcreator.h
    src/models/imagethumbnailmodel.h
    src/views/imagegridview.h
    src/views/thumbnaildelegate.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/models
    ${CMAKE_SOURCE_DIR}/src/views
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Sql
)
