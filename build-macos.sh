#!/bin/bash

# Check for --no-package argument
SKIP_PACKAGE=0
for arg in "$@"; do
    if [ "$arg" == "--no-package" ]; then
        SKIP_PACKAGE=1
    fi
done

echo "==================================="
echo "diyPresso Client C++ - macOS Build"
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

# Determine target triplet based on architecture
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    TARGET_TRIPLET="arm64-osx"
else
    TARGET_TRIPLET="x64-osx"
fi

echo "Detected architecture: $ARCH"
echo "Using target triplet: $TARGET_TRIPLET"

# Install dependencies via vcpkg
echo ""
echo "Installing dependencies via vcpkg..."

# Use the vcpkg binary (either from VCPKG_ROOT or PATH)
if [ -f "$VCPKG_ROOT/vcpkg" ]; then
    VCPKG_CMD="$VCPKG_ROOT/vcpkg"
elif command -v vcpkg >/dev/null 2>&1; then
    VCPKG_CMD="vcpkg"
else
    print_error "vcpkg executable not found"
    exit 1
fi

$VCPKG_CMD install nlohmann-json:$TARGET_TRIPLET  
$VCPKG_CMD install cli11:$TARGET_TRIPLET
$VCPKG_CMD install libusbp:$TARGET_TRIPLET

if [ $? -ne 0 ]; then
    print_error "Failed to install dependencies via vcpkg"
    exit 1
fi

# Create/clean build directory
echo ""
echo "Setting up build directory..."
if [ -d "build" ]; then
    echo "Cleaning existing build directory..."
    rm -rf build
fi
mkdir build
cd build

# Configure with CMake using the parameters that worked
echo ""
echo "Configuring with CMake..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_PREFIX_PATH="$VCPKG_ROOT/installed/$TARGET_TRIPLET" \
    -D_VCPKG_INSTALLED_DIR="$VCPKG_ROOT/installed" \
    -DVCPKG_TARGET_TRIPLET="$TARGET_TRIPLET" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if [ $? -ne 0 ]; then
    print_error "CMake configuration failed!"
    exit 1
fi

# Build the project
echo ""
echo "Building project..."
make -j$(sysctl -n hw.ncpu)

if [ $? -ne 0 ]; then
    print_error "Build failed!"
    exit 1
fi

echo ""
print_success "==================================="
print_success "Build completed successfully!"
print_success "Executable: build/diypresso"
print_success "==================================="

# Test the executable
echo ""
echo "Testing executable..."
if [ -x "./diypresso" ]; then
    ./diypresso --help
    echo ""
    print_success "Executable is working correctly!"
else
    print_warning "Executable not found or not executable"
    cd ..
    exit 1 # Exit the script if the executable is not found or not executable
fi

if [ $SKIP_PACKAGE -eq 0 ]; then
    echo ""
    echo "Creating distribution package..."
    
    # Create package directory
    mkdir -p "../bin/package-macos"
    
    # Clean existing package
    rm -f ../bin/package-macos/*
    
    # Copy executable
    if [ -x "./diypresso" ]; then
        echo "Copying diypresso..."
        cp "./diypresso" "../bin/package-macos/"
    else
        print_error "diypresso not found!"
        cd ..
        exit 1
    fi
    
    # Copy bossac executable
    if [ -f "../bin/bossac/bossac" ]; then
        echo "Copying bossac..."
        cp "../bin/bossac/bossac" "../bin/package-macos/"
        chmod +x "../bin/package-macos/bossac"
    else
        print_warning "bossac not found!"
        cd ..
        exit 1
    fi
    
    # Copy firmware.bin
    if [ -f "../bin/firmware/firmware.bin" ]; then
        echo "Copying firmware.bin..."
        cp "../bin/firmware/firmware.bin" "../bin/package-macos/"
    else
        print_warning "firmware.bin not found!"
        cd ..
        exit 1
    fi
    
    # Copy LICENSE
    if [ -f "../LICENSE" ]; then
        echo "Copying LICENSE..."
        cp "../LICENSE" "../bin/package-macos/"
    else
        print_warning "LICENSE not found!"
        cd ..
        exit 1
    fi
    
    # Create ZIP package
    echo "Creating ZIP package..."
    cd ../bin
    if command -v zip >/dev/null 2>&1; then
        zip -r "diyPresso-Client-macOS.zip" package-macos/
        if [ $? -eq 0 ]; then
            print_success "Successfully created diyPresso-Client-macOS.zip"
        else
            print_warning "Failed to create ZIP package"
            cd ../..
            exit 1
        fi
    else
        print_warning "zip command not found, skipping ZIP creation"
        cd ../..
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

# Return to project root
cd ..

echo ""
print_success "==================================="
print_success "Build process completed!"
print_success "==================================="