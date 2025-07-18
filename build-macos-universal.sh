#!/bin/bash

# Check for --no-package argument
SKIP_PACKAGE=0
BUILD_GUI=1
BUILD_CLI=1

for arg in "$@"; do
    if [ "$arg" == "--no-package" ]; then
        SKIP_PACKAGE=1
    elif [ "$arg" == "--cli-only" ]; then
        BUILD_GUI=0
    elif [ "$arg" == "--gui-only" ]; then
        BUILD_CLI=0
    fi
done

echo "==================================="
echo "diyPresso Client C++ - Qt 6 Universal Build (ARM64 + x86_64)"
echo "==================================="
echo "Build targets:"
if [ $BUILD_CLI -eq 1 ]; then
    echo "  - CLI Application (diypresso)"
fi
if [ $BUILD_GUI -eq 1 ]; then
    echo "  - GUI Application (diypresso-gui)"
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_error() {
    echo -e "${RED}ERROR: $1${NC}"
}

print_success() {
    echo -e "${GREEN}$1${NC}"
}

print_warning() {
    echo -e "${YELLOW}WARNING: $1${NC}"
}

# Check if vcpkg is available
if [ -z "$VCPKG_ROOT" ]; then
    if [ -d "$HOME/vcpkg" ]; then
        export VCPKG_ROOT="$HOME/vcpkg"
        echo "Found vcpkg in home directory: $VCPKG_ROOT"
    else
        print_error "VCPKG_ROOT not set and no vcpkg directory found in home."
        echo "Please set VCPKG_ROOT environment variable or install vcpkg in ~/vcpkg"
        exit 1
    fi
else
    echo "Using VCPKG_ROOT: $VCPKG_ROOT"
fi

# Check for Qt 6
if [ -z "$Qt6_DIR" ]; then
    # Try to find Qt 6 in common locations
    QT_PATHS=(
        "/opt/homebrew/lib/cmake/Qt6"
        "/usr/local/lib/cmake/Qt6"
        "/usr/local/Qt-6.*/lib/cmake/Qt6"
        "$HOME/Qt/6.*/macos/lib/cmake/Qt6"
    )
    
    for qt_path in "${QT_PATHS[@]}"; do
        if [ -d "$qt_path" ]; then
            export Qt6_DIR="$qt_path"
            echo "Found Qt6 at: $Qt6_DIR"
            break
        fi
    done
    
    if [ -z "$Qt6_DIR" ]; then
        print_error "Qt6 not found. Please install Qt 6.9+ or set Qt6_DIR environment variable"
        exit 1
    fi
fi

# Get the Qt bin directory for macdeployqt
QT_BIN_DIR=$(dirname "$Qt6_DIR")/../../bin
if [ ! -d "$QT_BIN_DIR" ]; then
    QT_BIN_DIR=$(dirname "$Qt6_DIR")/../../../bin
fi

if [ ! -f "$QT_BIN_DIR/macdeployqt" ]; then
    print_error "macdeployqt not found at $QT_BIN_DIR/macdeployqt"
    exit 1
fi

echo "Using macdeployqt from: $QT_BIN_DIR/macdeployqt"

# Install vcpkg dependencies for universal build
echo ""
echo "Installing vcpkg dependencies for universal build..."
$VCPKG_ROOT/vcpkg install nlohmann-json:arm64-osx nlohmann-json:x64-osx
$VCPKG_ROOT/vcpkg install cli11:arm64-osx cli11:x64-osx  
$VCPKG_ROOT/vcpkg install libusbp:arm64-osx libusbp:x64-osx
$VCPKG_ROOT/vcpkg install cpr:arm64-osx cpr:x64-osx

if [ $? -ne 0 ]; then
    print_error "Failed to install vcpkg dependencies"
    exit 1
fi

# Create/clean build directory
echo ""
echo "Setting up build directory..."
rm -rf build-universal
mkdir -p build-universal
cd build-universal

# Configure CMake for universal build
echo ""
echo "Configuring CMake for universal build..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_CLI=$BUILD_CLI \
    -DBUILD_GUI=$BUILD_GUI \
    -DCMAKE_PREFIX_PATH="$Qt6_DIR" \
    -DVCPKG_TARGET_TRIPLET="arm64-osx"

if [ $? -ne 0 ]; then
    print_error "CMake configuration failed!"
    exit 1
fi

# Build the project
echo ""
echo "Building universal binary..."
make -j$(sysctl -n hw.ncpu)

if [ $? -ne 0 ]; then
    print_error "Build failed!"
    exit 1
fi

# Verify universal binaries
echo ""
echo "Verifying universal binaries..."
if [ $BUILD_CLI -eq 1 ] && [ -f "diypresso" ]; then
    echo "CLI binary architecture:"
    file diypresso
    lipo -info diypresso
fi

if [ $BUILD_GUI -eq 1 ] && [ -f "diypresso-gui.app/Contents/MacOS/diypresso-gui" ]; then
    echo "GUI binary architecture:"
    file diypresso-gui.app/Contents/MacOS/diypresso-gui
    lipo -info diypresso-gui.app/Contents/MacOS/diypresso-gui
fi

# Check shared library
if [ -f "libdiyPresso-core.dylib" ]; then
    echo "Core library architecture:"
    file libdiyPresso-core.dylib
    lipo -info libdiyPresso-core.dylib
fi

echo ""
print_success "==================================="
print_success "Universal build completed successfully!"
print_success "==================================="

if [ $SKIP_PACKAGE -eq 0 ]; then
    echo ""
    echo "Creating distribution packages..."
    
    # Create package directories
    mkdir -p "../bin/package-macos-universal"
    rm -rf ../bin/package-macos-universal/*
    
    # Package CLI
    if [ $BUILD_CLI -eq 1 ] && [ -f "diypresso" ]; then
        echo "Packaging CLI application..."
        cp diypresso ../bin/package-macos-universal/
        if [ -f "../bin/bossac/bossac" ]; then
            cp ../bin/bossac/bossac ../bin/package-macos-universal/
        fi
        if [ -f "../LICENSE" ]; then
            cp ../LICENSE ../bin/package-macos-universal/
        fi
    fi
    
    # Package GUI with Qt deployment
    if [ $BUILD_GUI -eq 1 ] && [ -d "diypresso-gui.app" ]; then
        echo "Packaging GUI application..."
        cp -R diypresso-gui.app ../bin/package-macos-universal/diyPresso.app
        
        # Copy shared library into the app bundle
        mkdir -p ../bin/package-macos-universal/diyPresso.app/Contents/Frameworks
        cp libdiyPresso-core.dylib ../bin/package-macos-universal/diyPresso.app/Contents/Frameworks/
        
        # Use Qt 6 macdeployqt for universal deployment
        echo "Running macdeployqt..."
        "$QT_BIN_DIR/macdeployqt" ../bin/package-macos-universal/diyPresso.app -verbose=2
        
        if [ $? -ne 0 ]; then
            print_error "macdeployqt failed!"
            exit 1
        fi
        
        # Fix our custom library path
        install_name_tool -change "@rpath/libdiyPresso-core.dylib" "@executable_path/../Frameworks/libdiyPresso-core.dylib" ../bin/package-macos-universal/diyPresso.app/Contents/MacOS/diypresso-gui
        
        # Add additional resources
        if [ -f "../bin/bossac/bossac" ]; then
            cp ../bin/bossac/bossac ../bin/package-macos-universal/diyPresso.app/Contents/Resources/
        fi
        if [ -f "../LICENSE" ]; then
            cp ../LICENSE ../bin/package-macos-universal/diyPresso.app/Contents/Resources/
        fi
        
        # Basic code signing (remove signatures first, then re-sign)
        echo "Re-signing app bundle..."
        codesign --remove-signature ../bin/package-macos-universal/diyPresso.app 2>/dev/null || true
        find ../bin/package-macos-universal/diyPresso.app -name "*.dylib" -exec codesign --force --sign - {} \; 2>/dev/null || true
        find ../bin/package-macos-universal/diyPresso.app -name "*.framework" -exec codesign --force --sign - {} \; 2>/dev/null || true
        codesign --force --sign - ../bin/package-macos-universal/diyPresso.app 2>/dev/null || true
        
        echo "Verifying deployed app..."
        echo "Main executable:"
        otool -L ../bin/package-macos-universal/diyPresso.app/Contents/MacOS/diypresso-gui | head -10
        
        echo "Architecture verification:"
        lipo -info ../bin/package-macos-universal/diyPresso.app/Contents/MacOS/diypresso-gui
        
        # Test that it actually launches
        echo "Testing app launch..."
        timeout 5 ../bin/package-macos-universal/diyPresso.app/Contents/MacOS/diypresso-gui --version 2>/dev/null || true
    fi
    
    # Create zip packages
    echo "Creating ZIP packages..."
    cd ../bin
    if [ -d "package-macos-universal" ]; then
        zip -r "diyPresso-Client-macOS-Universal.zip" package-macos-universal
    fi
    cd ../build-universal
    
    print_success "==================================="
    print_success "Universal packages created successfully!"
    print_success "Package: bin/package-macos-universal/ (ZIP: bin/diyPresso-Client-macOS-Universal.zip)"
    print_success "==================================="
else
    echo "Skipping package creation (--no-package specified)"
fi

cd ..
print_success "==================================="
print_success "Qt 6 Universal build process completed!"
print_success "===================================" 