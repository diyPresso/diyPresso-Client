cmake_minimum_required(VERSION 3.16) # Qt 6 requires CMake 3.16+
project(diyPressoClientCpp VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable compile_commands.json generation for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure for dynamic linking
set(BUILD_SHARED_LIBS ON)

# Build options
option(BUILD_GUI "Build Qt GUI application" ON)
option(BUILD_CLI "Build command line interface" ON)

# Use vcpkg toolchain if available
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# Set platform-specific vcpkg triplets for dynamic linking
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "")
    else()
        set(VCPKG_TARGET_TRIPLET "x86-windows" CACHE STRING "")
    endif()
elseif(APPLE)
    # For macOS, the triplet should be set by the build script
    # For universal builds, we'll use arm64-osx as the primary triplet
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64.*x86_64|x86_64.*arm64")
            # Universal build - use arm64-osx as primary triplet
            set(VCPKG_TARGET_TRIPLET "arm64-osx" CACHE STRING "")
            message(STATUS "Using arm64-osx triplet for universal build")
        elseif(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
            set(VCPKG_TARGET_TRIPLET "arm64-osx" CACHE STRING "")
        elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
            set(VCPKG_TARGET_TRIPLET "x64-osx" CACHE STRING "")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(VCPKG_TARGET_TRIPLET "arm64-osx" CACHE STRING "")
        else()
            set(VCPKG_TARGET_TRIPLET "x64-osx" CACHE STRING "")
        endif()
    endif()
endif()

# Find Qt6 if GUI is enabled
if(BUILD_GUI)
    # For macOS universal builds, CMAKE_PREFIX_PATH should be set by the build script
    # This simplifies the architecture-specific logic
    if(APPLE AND NOT DEFINED CMAKE_PREFIX_PATH)
        # Fallback: try to find Qt6 in common locations
        set(QT_SEARCH_PATHS
            "/opt/homebrew/lib/cmake"
            "/usr/local/lib/cmake"
            "/usr/local/Qt-6.*/lib/cmake"
            "$ENV{HOME}/Qt/6.*/macos/lib/cmake"
        )
        
        foreach(qt_path IN LISTS QT_SEARCH_PATHS)
            if(EXISTS "${qt_path}/Qt6")
                list(APPEND CMAKE_PREFIX_PATH "${qt_path}")
                message(STATUS "Found Qt6 at: ${qt_path}")
                break()
            endif()
        endforeach()
    endif()
    
    find_package(Qt6 6.9 REQUIRED COMPONENTS Core Widgets)
    
    # Enable Qt's automatic MOC, UIC, and RCC processing
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    
    # For universal builds, ensure Qt is also universal
    if(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES "arm64" AND CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
        message(STATUS "Building universal binary for architectures: ${CMAKE_OSX_ARCHITECTURES}")
    endif()
endif()

# Find required packages
find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)

# Find libusbp using multiple approaches
find_package(unofficial-libusbp CONFIG QUIET)
if(NOT unofficial-libusbp_FOUND)
    find_package(libusbp CONFIG QUIET)
    if(NOT libusbp_FOUND)
        # Fallback to pkg-config
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LIBUSBP libusbp)
        endif()
        
        if(NOT LIBUSBP_FOUND)
            message(FATAL_ERROR "libusbp not found. Please install it via vcpkg: vcpkg install libusbp")
        endif()
    endif()
endif()

# Platform-specific system libraries
if(WIN32)
    # Windows-specific libraries that libusbp might need
    set(PLATFORM_LIBS setupapi winmm)
elseif(APPLE)
    # macOS frameworks required by libusbp
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(IOKIT_FRAMEWORK IOKit)
    set(PLATFORM_LIBS ${COREFOUNDATION_FRAMEWORK} ${IOKIT_FRAMEWORK})
elseif(UNIX)
    # Linux libraries that might be needed
    set(PLATFORM_LIBS)
endif()

# Core library sources (shared between CLI and GUI)
set(CORE_SOURCES
    src/core/DpcDevice.cpp
    src/core/DpcSerial.cpp
    src/core/DpcSettings.cpp
    src/core/DpcFirmware.cpp
    src/core/DpcColors.cpp
    src/core/DpcDownload.cpp
)

set(CORE_HEADERS
    src/core/DpcDevice.h
    src/core/DpcSerial.h
    src/core/DpcSettings.h
    src/core/DpcFirmware.h
    src/core/DpcColors.h
    src/core/DpcDownload.h
)

# Determine which libusbp target to use
if(TARGET unofficial::libusbp::libusbp)
    set(LIBUSBP_TARGET unofficial::libusbp::libusbp)
elseif(TARGET libusbp::libusbp)
    set(LIBUSBP_TARGET libusbp::libusbp)
elseif(LIBUSBP_FOUND)
    set(LIBUSBP_TARGET ${LIBUSBP_LIBRARIES})
else()
    message(FATAL_ERROR "No suitable libusbp target found")
endif()

# Create shared core library
add_library(diyPresso-core SHARED ${CORE_SOURCES} ${CORE_HEADERS})

# Windows version and other definitions
if(WIN32)
    target_compile_definitions(diyPresso-core PRIVATE 
        _WIN32_WINNT=0x0A00  # Windows 10
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Link libraries to core
target_link_libraries(diyPresso-core PUBLIC 
    nlohmann_json::nlohmann_json 
    ${LIBUSBP_TARGET}
    cpr::cpr
    ${PLATFORM_LIBS}
)

# Set include directories for core library
target_include_directories(diyPresso-core PUBLIC 
    src/core
)

# CLI Application Target
if(BUILD_CLI)
    add_executable(diypresso src/cli/main.cpp)
    
    target_link_libraries(diypresso PRIVATE 
        diyPresso-core
        CLI11::CLI11
    )
    
    # Set the output name
    set_target_properties(diypresso PROPERTIES OUTPUT_NAME "diypresso")
endif()

# GUI Application Target
if(BUILD_GUI)
    # GUI source files (will be created next)
    set(GUI_SOURCES
        src/gui/main.cpp
        src/gui/MainWindow.cpp
    )
    
    set(GUI_HEADERS
        src/gui/MainWindow.h
    )
    
    set(GUI_UI_FILES
        src/gui/MainWindow.ui
    )
    
    # Create GUI executable
    add_executable(diypresso-gui ${GUI_SOURCES} ${GUI_HEADERS} ${GUI_UI_FILES})
    
    # Add icon file to the target if it exists
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/gui/diypresso.icns")
        target_sources(diypresso-gui PRIVATE "${CMAKE_SOURCE_DIR}/src/gui/diypresso.icns")
    endif()
    
    # Link Qt libraries and core library
    target_link_libraries(diypresso-gui PRIVATE 
        diyPresso-core
        Qt6::Core
        Qt6::Widgets
    )
    
    # Set the output name
    set_target_properties(diypresso-gui PROPERTIES OUTPUT_NAME "diypresso-gui")
    
    # Windows-specific GUI settings
    if(WIN32)
        set_target_properties(diypresso-gui PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
    
    # macOS-specific GUI settings
    if(APPLE)
        set_target_properties(diypresso-gui PROPERTIES MACOSX_BUNDLE TRUE)
        
        # Set the icon file
        set(MACOSX_BUNDLE_ICON_FILE diypresso.icns)
        
        # Add the icon as a resource
        if(EXISTS "${CMAKE_SOURCE_DIR}/src/gui/diypresso.icns")
            set_source_files_properties(${CMAKE_SOURCE_DIR}/src/gui/diypresso.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
            set_target_properties(diypresso-gui PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/src/gui/Info.plist
                RESOURCE "${CMAKE_SOURCE_DIR}/src/gui/diypresso.icns"
            )
            message(STATUS "Icon file found and will be included in bundle")
        else()
            message(WARNING "Icon file not found: ${CMAKE_SOURCE_DIR}/src/gui/diypresso.icns")
        endif()
    endif()
endif()

# Compiler-specific settings for all targets
if(MSVC)
    # Visual Studio specific settings
    if(TARGET diyPresso-core)
        target_compile_options(diyPresso-core PRIVATE /W4)
    endif()
    if(TARGET diypresso)
    target_compile_options(diypresso PRIVATE /W4)
    endif()
    if(TARGET diypresso-gui)
        target_compile_options(diypresso-gui PRIVATE /W4)
    endif()
else()
    # GCC/Clang settings
    if(TARGET diyPresso-core)
        target_compile_options(diyPresso-core PRIVATE -Wall -Wextra)
    endif()
    if(TARGET diypresso)
    target_compile_options(diypresso PRIVATE -Wall -Wextra)
    endif()
    if(TARGET diypresso-gui)
        target_compile_options(diypresso-gui PRIVATE -Wall -Wextra)
    endif()
endif()

# Installation rules
if(BUILD_CLI)
install(TARGETS diypresso DESTINATION bin) 
endif()

if(BUILD_GUI)
    install(TARGETS diypresso-gui DESTINATION bin)
endif()

install(TARGETS diyPresso-core DESTINATION lib)

# Print build configuration
message(STATUS "diyPresso Build Configuration:")
message(STATUS "  CLI Application: ${BUILD_CLI}")
message(STATUS "  GUI Application: ${BUILD_GUI}")
if(BUILD_GUI)
    message(STATUS "  Qt Version: ${Qt6_VERSION}")
    if(APPLE)
        message(STATUS "  Qt Location: ${QT_DIR}")
    endif()
endif()
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}") 