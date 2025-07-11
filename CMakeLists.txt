cmake_minimum_required(VERSION 3.10)
project(diyPressoClientCpp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure for dynamic linking
set(BUILD_SHARED_LIBS ON)

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
    # For macOS, the triplet should be set by the build script for cross-compilation
    # Only set a default if not already specified
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(VCPKG_TARGET_TRIPLET "arm64-osx" CACHE STRING "")
        else()
            set(VCPKG_TARGET_TRIPLET "x64-osx" CACHE STRING "")
        endif()
    endif()
endif()

# Add source files here as you implement them
set(SOURCES
    src/main.cpp
    src/DpcSerial.cpp
    src/DpcDevice.cpp
    src/DpcSettings.cpp
    src/DpcFirmware.cpp
    src/DpcColors.cpp
    src/DpcDownload.cpp
)

# Find packages from vcpkg
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

add_executable(diypresso ${SOURCES})

# Windows version and other definitions
if(WIN32)
    target_compile_definitions(diypresso PRIVATE 
        _WIN32_WINNT=0x0A00  # Windows 10
        _CRT_SECURE_NO_WARNINGS
    )
endif()

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

# Link libraries
target_link_libraries(diypresso PRIVATE 
    CLI11::CLI11
    nlohmann_json::nlohmann_json 
    ${LIBUSBP_TARGET}
    cpr::cpr
    ${PLATFORM_LIBS}
)

# Compiler-specific settings
if(MSVC)
    # Visual Studio specific settings
    target_compile_options(diypresso PRIVATE /W4)
else()
    # GCC/Clang settings
    target_compile_options(diypresso PRIVATE -Wall -Wextra)
endif()

# Installation rules (optional)
install(TARGETS diypresso DESTINATION bin) 