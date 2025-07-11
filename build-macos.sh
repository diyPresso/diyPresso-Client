#!/bin/bash

# Check for --no-package argument
SKIP_PACKAGE=0
for arg in "$@"; do
    if [ "$arg" == "--no-package" ]; then
        SKIP_PACKAGE=1
    fi
done

echo "==================================="
echo "diyPresso Client C++ - macOS Build (ARM + Intel)"
echo "==================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
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
    # Try to find vcpkg in common locations
    if [ -d "$HOME/vcpkg" ]; then
        export VCPKG_ROOT="$HOME/vcpkg"
        echo "Found vcpkg in home directory: $VCPKG_ROOT"
    elif command -v vcpkg >/dev/null 2>&1; then
        # vcpkg is in PATH, try to find the root
        VCPKG_PATH=$(which vcpkg)
        if [ -L "$VCPKG_PATH" ]; then
            # If it's a symlink (like Homebrew), try to find the actual installation
            if [ -d "$HOME/vcpkg" ]; then
                export VCPKG_ROOT="$HOME/vcpkg"
                echo "Using vcpkg from home directory: $VCPKG_ROOT"
            else
                print_error "vcpkg found in PATH but VCPKG_ROOT not set and no vcpkg directory found in home."
                echo "Please either:"
                echo "1. Set VCPKG_ROOT environment variable to point to your vcpkg installation"
                echo "2. Install vcpkg in your home directory: git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg"
                exit 1
            fi
        fi
    else
        print_error "vcpkg not found and VCPKG_ROOT environment variable not set."
        echo "Please either:"
        echo "1. Install vcpkg and set VCPKG_ROOT to its installation directory"
        echo "2. Install vcpkg via Homebrew: brew install vcpkg"
        echo "3. Clone vcpkg: git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg"
        exit 1
    fi
else
    echo "Using VCPKG_ROOT: $VCPKG_ROOT"
fi

# Use the vcpkg binary (either from VCPKG_ROOT or PATH)
if [ -f "$VCPKG_ROOT/vcpkg" ]; then
    VCPKG_CMD="$VCPKG_ROOT/vcpkg"
elif command -v vcpkg >/dev/null 2>&1; then
    VCPKG_CMD="vcpkg"
else
    print_error "vcpkg executable not found"
    exit 1
fi

# Install dependencies for both architectures
echo ""
echo "Installing dependencies for both architectures..."

# Install for ARM64
echo "Installing ARM64 dependencies..."
$VCPKG_CMD install nlohmann-json:arm64-osx
$VCPKG_CMD install cli11:arm64-osx
$VCPKG_CMD install libusbp:arm64-osx
$VCPKG_CMD install cpr:arm64-osx

# Install for x64
echo "Installing x64 dependencies..."
$VCPKG_CMD install nlohmann-json:x64-osx
$VCPKG_CMD install cli11:x64-osx
$VCPKG_CMD install libusbp:x64-osx
$VCPKG_CMD install cpr:x64-osx

if [ $? -ne 0 ]; then
    print_error "Failed to install dependencies via vcpkg"
    exit 1
fi

# Create/clean build directories
echo ""
echo "Setting up build directories..."
if [ -d "build" ]; then
    echo "Cleaning existing build directory..."
    rm -rf build
fi
mkdir -p build/arm64 build/x64

# Function to build for a specific architecture
build_for_arch() {
    local arch=$1
    local triplet=$2
    local build_dir="build/$arch"
    
    echo ""
    echo "Building for $arch architecture..."
    echo "Using triplet: $triplet"
    
    cd "$build_dir"
    
    # Map architecture names for CMAKE_OSX_ARCHITECTURES
    local cmake_arch="$arch"
    if [ "$arch" = "x64" ]; then
        cmake_arch="x86_64"
    fi
    
    # Configure with CMake
    echo "Configuring CMake for $arch..."
    cmake ../.. \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -DCMAKE_PREFIX_PATH="$VCPKG_ROOT/installed/$triplet" \
        -D_VCPKG_INSTALLED_DIR="$VCPKG_ROOT/installed" \
        -DVCPKG_TARGET_TRIPLET="$triplet" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_OSX_ARCHITECTURES="$cmake_arch"
    
    if [ $? -ne 0 ]; then
        print_error "CMake configuration failed for $arch!"
        cd ../..
        exit 1
    fi
    
    # Build the project
    echo "Building project for $arch..."
    make -j$(sysctl -n hw.ncpu)
    
    if [ $? -ne 0 ]; then
        print_error "Build failed for $arch!"
        cd ../..
        exit 1
    fi
    
    echo "Build completed for $arch"
    cd ../..
}

# Build for both architectures
build_for_arch "arm64" "arm64-osx"
build_for_arch "x64" "x64-osx"

# Create universal binary
echo ""
echo "Creating universal binary..."

# Check if both executables exist
if [ ! -f "build/arm64/diypresso" ]; then
    print_error "ARM64 executable not found!"
    exit 1
fi

if [ ! -f "build/x64/diypresso" ]; then
    print_error "x64 executable not found!"
    exit 1
fi

# Create universal binary using lipo
lipo -create build/arm64/diypresso build/x64/diypresso -output build/diypresso

if [ $? -ne 0 ]; then
    print_error "Failed to create universal binary!"
    exit 1
fi

# Verify the universal binary
echo "Verifying universal binary..."
file build/diypresso
lipo -info build/diypresso

echo ""
print_success "==================================="
print_success "Universal build completed successfully!"
print_success "Executable: build/diypresso (universal)"
print_success "==================================="

# Test the universal executable
echo ""
echo "Testing universal executable..."
if [ -x "build/diypresso" ]; then
    build/diypresso --help
    echo ""
    print_success "Universal executable is working correctly!"
else
    print_warning "Universal executable not found or not executable"
    exit 1
fi

if [ $SKIP_PACKAGE -eq 0 ]; then
    echo ""
    echo "Creating distribution package..."
    
    # Create package directory
    mkdir -p "bin/package-macos"
    
    # Clean existing package
    rm -f bin/package-macos/*
    
    # Copy executable
    if [ -x "build/diypresso" ]; then
        echo "Copying diypresso..."
        cp "build/diypresso" "bin/package-macos/"
    else
        print_error "diypresso not found!"
        exit 1
    fi
    
    # Copy bossac executable
    if [ -f "bin/bossac/bossac" ]; then
        echo "Copying bossac..."
        cp "bin/bossac/bossac" "bin/package-macos/"
        chmod +x "bin/package-macos/bossac"
    else
        print_error "bossac not found at bin/bossac/bossac"
        echo "The macOS package requires bossac for firmware uploads."
        echo "Please ensure bossac is available in bin/bossac/ directory."
        exit 1
    fi
    
    # Code signing
    echo ""
    echo "Code signing executables..."
    
    # Sign the main executable
    if [ -x "bin/package-macos/diypresso" ]; then
        echo "Signing diypresso..."
        codesign --force --options runtime --deep --sign "Developer ID Application: diyPresso B.V. (V3QR5FV6B7)" --timestamp "bin/package-macos/diypresso"
        if [ $? -ne 0 ]; then
            print_warning "Failed to sign diypresso (this is normal if you don't have the certificate)"
        else
            print_success "Successfully signed diypresso"
        fi
    else
        print_warning "diypresso not found for signing!"
    fi
    
    # Sign the bossac executable
    if [ -x "bin/package-macos/bossac" ]; then
        echo "Signing bossac..."
        codesign --force --options runtime --deep --sign "Developer ID Application: diyPresso B.V. (V3QR5FV6B7)" --timestamp "bin/package-macos/bossac"
        if [ $? -ne 0 ]; then
            print_warning "Failed to sign bossac (this is normal if you don't have the certificate)"
        else
            print_success "Successfully signed bossac"
        fi
    else
        print_warning "bossac not found for signing!"
    fi
    
    # Copy LICENSE
    if [ -f "LICENSE" ]; then
        echo "Copying LICENSE..."
        cp "LICENSE" "bin/package-macos/"
    else
        print_warning "LICENSE not found!"
        exit 1
    fi
    
    # Create ZIP package
    echo "Creating ZIP package..."
    cd bin
    
    # Remove existing ZIP file to ensure clean creation
    if [ -f "diyPresso-Client-macOS.zip" ]; then
        echo "Removing existing ZIP file..."
        rm -f "diyPresso-Client-macOS.zip"
    fi
    
    if command -v zip >/dev/null 2>&1; then
        cd package-macos
        zip -r "../diyPresso-Client-macOS.zip" .
        cd ..
        if [ $? -eq 0 ]; then
            print_success "Successfully created diyPresso-Client-macOS.zip"
        else
            print_warning "Failed to create ZIP package"
            cd ..
            exit 1
        fi
    else
        print_warning "zip command not found, skipping ZIP creation"
        cd ..
        exit 1
    fi
    cd ..
    
    echo ""
    print_success "==================================="
    print_success "Package created successfully!"
    print_success "Location: bin/package-macos/"
    print_success "ZIP file: bin/diyPresso-Client-macOS.zip"
    print_success "==================================="
else
    echo "Skipping package creation (due to --no-package argument)."
fi

echo ""
print_success "==================================="
print_success "Build process completed!"
print_success "==================================="