# MinGW Toolchain file for Qt's MinGW installation
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Find Qt's MinGW installation
set(MINGW_ROOT "C:/Qt/Tools/mingw1310_64")
if(NOT EXISTS "${MINGW_ROOT}")
    # Try alternative paths
    set(MINGW_PATHS
        "C:/Qt/Tools/mingw1120_64"
        "C:/Qt/Tools/mingw1121_64"
        "C:/Qt/Tools/mingw1122_64"
        "C:/Qt/Tools/mingw1123_64"
    )
    foreach(PATH ${MINGW_PATHS})
        if(EXISTS "${PATH}")
            set(MINGW_ROOT "${PATH}")
            break()
        endif()
    endforeach()
endif()

if(EXISTS "${MINGW_ROOT}")
    set(CMAKE_C_COMPILER "${MINGW_ROOT}/bin/gcc.exe" CACHE FILEPATH "C compiler")
    set(CMAKE_CXX_COMPILER "${MINGW_ROOT}/bin/g++.exe" CACHE FILEPATH "C++ compiler")
    set(CMAKE_MAKE_PROGRAM "${MINGW_ROOT}/bin/mingw32-make.exe" CACHE FILEPATH "Make program")
    set(CMAKE_RC_COMPILER "${MINGW_ROOT}/bin/windres.exe" CACHE FILEPATH "Resource compiler")
    
    # Set CMAKE_PROGRAM_PATH so CMake can find MinGW tools
    set(CMAKE_PROGRAM_PATH "${MINGW_ROOT}/bin" ${CMAKE_PROGRAM_PATH})
    
    # Add MinGW bin to PATH for DLL resolution (this affects subprocess execution)
    set(ENV{PATH} "${MINGW_ROOT}/bin;$ENV{PATH}")
    
    message(STATUS "Using MinGW from: ${MINGW_ROOT}")
else()
    message(FATAL_ERROR "Could not find Qt MinGW installation. Please install MinGW or specify MINGW_ROOT.")
endif()
