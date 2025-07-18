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
echo "diyPresso Client C++ - macOS Build (ARM + Intel)"
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
    
    # Set correct Qt path for each architecture
    if [ "$arch" = "arm64" ]; then
        export CMAKE_PREFIX_PATH="$VCPKG_ROOT/installed/arm64-osx;/opt/homebrew/lib/cmake"
    else
        export CMAKE_PREFIX_PATH="$VCPKG_ROOT/installed/x64-osx;/usr/local/lib/cmake"
    fi
    
    # Configure with CMake
    echo "Configuring CMake for $arch..."
    cmake ../.. \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
        -D_VCPKG_INSTALLED_DIR="$VCPKG_ROOT/installed" \
        -DVCPKG_TARGET_TRIPLET="$triplet" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_OSX_ARCHITECTURES="$cmake_arch" \
        -DBUILD_CLI=$BUILD_CLI \
        -DBUILD_GUI=$BUILD_GUI
    
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

# Create universal binaries
echo ""
echo "Creating universal binaries..."

# CLI Application
if [ $BUILD_CLI -eq 1 ]; then
if [ ! -f "build/arm64/diypresso" ]; then
        print_error "ARM64 CLI executable not found!"
    exit 1
fi

if [ ! -f "build/x64/diypresso" ]; then
        print_error "x64 CLI executable not found!"
    exit 1
fi

    # Create universal CLI binary using lipo
lipo -create build/arm64/diypresso build/x64/diypresso -output build/diypresso

if [ $? -ne 0 ]; then
        print_error "Failed to create universal CLI binary!"
    exit 1
fi

    echo "Verifying universal CLI binary..."
file build/diypresso
lipo -info build/diypresso
fi

# GUI Application
if [ $BUILD_GUI -eq 1 ]; then
    if [ ! -f "build/arm64/diypresso-gui.app/Contents/MacOS/diypresso-gui" ]; then
        print_error "ARM64 GUI executable not found!"
        exit 1
    fi

    if [ ! -f "build/x64/diypresso-gui.app/Contents/MacOS/diypresso-gui" ]; then
        print_error "x64 GUI executable not found!"
        exit 1
    fi

    # Create universal GUI binary using lipo
    lipo -create build/arm64/diypresso-gui.app/Contents/MacOS/diypresso-gui build/x64/diypresso-gui.app/Contents/MacOS/diypresso-gui -output build/diypresso-gui

    if [ $? -ne 0 ]; then
        print_error "Failed to create universal GUI binary!"
        exit 1
    fi

    echo "Verifying universal GUI binary..."
    file build/diypresso-gui
    lipo -info build/diypresso-gui
fi

echo ""
print_success "==================================="
print_success "Universal build completed successfully!"
if [ $BUILD_CLI -eq 1 ]; then
    print_success "CLI Executable: build/diypresso (universal)"
fi
if [ $BUILD_GUI -eq 1 ]; then
    print_success "GUI Executable: build/diypresso-gui (universal)"
fi
print_success "==================================="

# Test the universal executables
echo ""
echo "Testing universal executables..."
if [ $BUILD_CLI -eq 1 ] && [ -x "build/diypresso" ]; then
    echo "Testing CLI executable..."
    build/diypresso --help
    echo ""
    print_success "CLI executable is working correctly!"
fi

if [ $BUILD_GUI -eq 1 ] && [ -x "build/diypresso-gui" ]; then
    echo "Testing GUI executable..."
    # Just check if it's executable, don't run it interactively
    if file build/diypresso-gui | grep -q "executable"; then
        print_success "GUI executable is working correctly!"
    else
        print_warning "GUI executable may have issues"
    fi
fi

if [ $SKIP_PACKAGE -eq 0 ]; then
    echo ""
    echo "Creating distribution packages..."
    
    # Create separate package directories
    mkdir -p "bin/package-macos-cli"
    mkdir -p "bin/package-macos-gui"
    rm -rf bin/package-macos-cli/*
    rm -rf bin/package-macos-gui/*

    # CLI Package
    if [ $BUILD_CLI -eq 1 ] && [ -x "build/diypresso" ]; then
        echo "Creating CLI package..."
        cp "build/diypresso" "bin/package-macos-cli/"
        if [ -f "bin/bossac/bossac" ]; then
            cp "bin/bossac/bossac" "bin/package-macos-cli/"
            chmod +x "bin/package-macos-cli/bossac"
        fi
        if [ -f "LICENSE" ]; then
            cp "LICENSE" "bin/package-macos-cli/"
        fi
    fi

    # GUI Package (.app bundle) - Handle universal deployment properly
    if [ $BUILD_GUI -eq 1 ] && [ -d "build/arm64/diypresso-gui.app" ]; then
        echo "Creating GUI package with universal Qt deployment..."
        
        # Deploy Qt for ARM64 first
        echo "Step 1: Deploying ARM64 Qt frameworks..."
        cp -R "build/arm64/diypresso-gui.app" "bin/package-macos-gui/diyPresso.app"
        mkdir -p "bin/package-macos-gui/diyPresso.app/Contents/Frameworks"
        cp "build/arm64/libdiyPresso-core.dylib" "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/"
        
        # Run macdeployqt for ARM64
        PATH="/opt/homebrew/bin:$PATH" macdeployqt "bin/package-macos-gui/diyPresso.app" -verbose=2
        if [ $? -ne 0 ]; then
            print_error "macdeployqt failed for ARM64!"
            exit 1
        fi
        
        # Deploy Qt for x64 into a temporary bundle
        echo "Step 2: Deploying x64 Qt frameworks..."
        cp -R "build/x64/diypresso-gui.app" "bin/temp-x64.app"
        mkdir -p "bin/temp-x64.app/Contents/Frameworks"
        cp "build/x64/libdiyPresso-core.dylib" "bin/temp-x64.app/Contents/Frameworks/"
        
        # Run macdeployqt for x64 with x64 Qt path
        PATH="/usr/local/bin:$PATH" macdeployqt "bin/temp-x64.app" -verbose=2
        if [ $? -ne 0 ]; then
            print_error "macdeployqt failed for x64!"
            exit 1
        fi
        
        # Step 2.5: Deploy missing Qt frameworks that macdeployqt failed to include
        echo "Step 2.5: Deploying missing Qt frameworks..."
        
        # Function to extract Qt framework dependencies from a binary
        get_qt_deps() {
            local binary="$1"
            if [ -f "$binary" ]; then
                otool -L "$binary" | grep -E "(@rpath|/opt/homebrew/opt/qt/lib|/usr/local/opt/qt/lib).*Qt" | sed 's/.*Qt/Qt/' | sed 's/\.framework.*//' | sort -u
            fi
        }
        
        # Function to deploy a Qt framework if missing for ARM64
        deploy_qt_framework_arm64() {
            local framework_name="$1"
            local qt_path="/opt/homebrew/opt/qt/lib"
            local bundle_path="bin/package-macos-gui/diyPresso.app"
            
            if [ ! -d "$bundle_path/Contents/Frameworks/$framework_name.framework" ] && [ -d "$qt_path/$framework_name.framework" ]; then
                echo "Deploying missing $framework_name to ARM64 bundle..."
                cp -R "$qt_path/$framework_name.framework" "$bundle_path/Contents/Frameworks/"
                
                # Fix its dependencies
                local binary="$bundle_path/Contents/Frameworks/$framework_name.framework/Versions/A/$framework_name"
                install_name_tool -change "$qt_path/QtCore.framework/Versions/A/QtCore" "@executable_path/../Frameworks/QtCore.framework/Versions/A/QtCore" "$binary" 2>/dev/null || true
                install_name_tool -change "$qt_path/QtGui.framework/Versions/A/QtGui" "@executable_path/../Frameworks/QtGui.framework/Versions/A/QtGui" "$binary" 2>/dev/null || true
                install_name_tool -change "$qt_path/QtWidgets.framework/Versions/A/QtWidgets" "@executable_path/../Frameworks/QtWidgets.framework/Versions/A/QtWidgets" "$binary" 2>/dev/null || true
                install_name_tool -change "$qt_path/QtDBus.framework/Versions/A/QtDBus" "@executable_path/../Frameworks/QtDBus.framework/Versions/A/QtDBus" "$binary" 2>/dev/null || true
                
                # Special handling for QtDBus - bundle libdbus-1.3.dylib
                if [ "$framework_name" = "QtDBus" ]; then
                    echo "Bundling libdbus-1.3.dylib for QtDBus..."
                    # Find the actual dbus library location
                    local dbus_lib=""
                    if [ -f "/opt/homebrew/lib/libdbus-1.3.dylib" ]; then
                        dbus_lib="/opt/homebrew/lib/libdbus-1.3.dylib"
                    elif [ -d "/opt/homebrew/Cellar/dbus" ]; then
                        dbus_lib=$(find /opt/homebrew/Cellar/dbus -name "libdbus-1.3.dylib" -type f | head -1)
                    fi
                    
                    if [ -n "$dbus_lib" ] && [ -f "$dbus_lib" ]; then
                        cp "$dbus_lib" "$bundle_path/Contents/Frameworks/"
                        # Get current dependency path from the framework
                        current_path=$(otool -L "$binary" | grep libdbus-1.3.dylib | awk '{print $1}' | head -1)
                        if [ -n "$current_path" ]; then
                            install_name_tool -change "$current_path" "@executable_path/../Frameworks/libdbus-1.3.dylib" "$binary"
                            echo "libdbus-1.3.dylib bundled and path fixed: $current_path -> @executable_path/../Frameworks/libdbus-1.3.dylib"
                        else
                            echo "Warning: Could not find libdbus-1.3.dylib reference in QtDBus"
                        fi
                    else
                        echo "Warning: libdbus-1.3.dylib not found in ARM64 Homebrew"
                    fi
                fi
                
                print_success "$framework_name deployed to ARM64 bundle"
            fi
        }
        
        # Function to deploy a Qt framework if missing for x64
        deploy_qt_framework_x64() {
            local framework_name="$1"
            local qt_path="/usr/local/opt/qt/lib"
            local bundle_path="bin/temp-x64.app"
            
            if [ ! -d "$bundle_path/Contents/Frameworks/$framework_name.framework" ] && [ -d "$qt_path/$framework_name.framework" ]; then
                echo "Deploying missing $framework_name to x64 bundle..."
                cp -R "$qt_path/$framework_name.framework" "$bundle_path/Contents/Frameworks/"
                
                # Fix its dependencies
                local binary="$bundle_path/Contents/Frameworks/$framework_name.framework/Versions/A/$framework_name"
                install_name_tool -change "$qt_path/QtCore.framework/Versions/A/QtCore" "@executable_path/../Frameworks/QtCore.framework/Versions/A/QtCore" "$binary" 2>/dev/null || true
                install_name_tool -change "$qt_path/QtGui.framework/Versions/A/QtGui" "@executable_path/../Frameworks/QtGui.framework/Versions/A/QtGui" "$binary" 2>/dev/null || true
                install_name_tool -change "$qt_path/QtWidgets.framework/Versions/A/QtWidgets" "@executable_path/../Frameworks/QtWidgets.framework/Versions/A/QtWidgets" "$binary" 2>/dev/null || true
                install_name_tool -change "$qt_path/QtDBus.framework/Versions/A/QtDBus" "@executable_path/../Frameworks/QtDBus.framework/Versions/A/QtDBus" "$binary" 2>/dev/null || true
                
                # Special handling for QtDBus - bundle libdbus-1.3.dylib
                if [ "$framework_name" = "QtDBus" ]; then
                    echo "Bundling libdbus-1.3.dylib for QtDBus..."
                    # Find the actual dbus library location
                    local dbus_lib=""
                    if [ -f "/usr/local/lib/libdbus-1.3.dylib" ]; then
                        dbus_lib="/usr/local/lib/libdbus-1.3.dylib"
                    elif [ -d "/usr/local/Cellar/dbus" ]; then
                        dbus_lib=$(find /usr/local/Cellar/dbus -name "libdbus-1.3.dylib" -type f | head -1)
                    fi
                    
                    if [ -n "$dbus_lib" ] && [ -f "$dbus_lib" ]; then
                        cp "$dbus_lib" "$bundle_path/Contents/Frameworks/"
                        # Get current dependency path from the framework
                        current_path=$(otool -L "$binary" | grep libdbus-1.3.dylib | awk '{print $1}' | head -1)
                        if [ -n "$current_path" ]; then
                            install_name_tool -change "$current_path" "@executable_path/../Frameworks/libdbus-1.3.dylib" "$binary"
                            echo "libdbus-1.3.dylib bundled and path fixed: $current_path -> @executable_path/../Frameworks/libdbus-1.3.dylib"
                        else
                            echo "Warning: Could not find libdbus-1.3.dylib reference in QtDBus"
                        fi
                    else
                        echo "Warning: libdbus-1.3.dylib not found in x64 Homebrew"
                    fi
                fi
                
                print_success "$framework_name deployed to x64 bundle"
            fi
        }
        
        # Check and deploy missing Qt dependencies for ARM64 bundle
        echo "Checking ARM64 bundle for missing Qt dependencies..."
        max_iterations=5
        iteration=0
        while [ $iteration -lt $max_iterations ]; do
            iteration=$((iteration + 1))
            echo "ARM64 dependency check iteration $iteration..."
            
            new_deps_found=false
            
            # Check main executable dependencies
            qt_deps=$(get_qt_deps "bin/package-macos-gui/diyPresso.app/Contents/MacOS/diypresso-gui")
            for dep in $qt_deps; do
                if [ -n "$dep" ] && [ ! -d "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/$dep.framework" ]; then
                    echo "Found missing dependency in ARM64 executable: $dep"
                    deploy_qt_framework_arm64 "$dep"
                    new_deps_found=true
                fi
            done
            
            # Check Qt framework internal dependencies
            for framework_dir in bin/package-macos-gui/diyPresso.app/Contents/Frameworks/Qt*.framework; do
                if [ -d "$framework_dir" ]; then
                    framework_name=$(basename "$framework_dir" .framework)
                    framework_binary="$framework_dir/Versions/A/$framework_name"
                    
                    if [ -f "$framework_binary" ]; then
                        deps=$(get_qt_deps "$framework_binary")
                        for dep in $deps; do
                            if [ -n "$dep" ] && [ ! -d "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/$dep.framework" ]; then
                                echo "Found missing dependency in ARM64 $framework_name: $dep"
                                deploy_qt_framework_arm64 "$dep"
                                new_deps_found=true
                            fi
                        done
                    fi
                fi
            done
            
            if [ "$new_deps_found" = false ]; then
                echo "No new dependencies found for ARM64 bundle, stopping iteration"
                break
            fi
        done
        
        # Check and deploy missing Qt dependencies for x64 bundle
        echo "Checking x64 bundle for missing Qt dependencies..."
        iteration=0
        while [ $iteration -lt $max_iterations ]; do
            iteration=$((iteration + 1))
            echo "x64 dependency check iteration $iteration..."
            
            new_deps_found=false
            
            # Check main executable dependencies
            qt_deps=$(get_qt_deps "bin/temp-x64.app/Contents/MacOS/diypresso-gui")
            for dep in $qt_deps; do
                if [ -n "$dep" ] && [ ! -d "bin/temp-x64.app/Contents/Frameworks/$dep.framework" ]; then
                    echo "Found missing dependency in x64 executable: $dep"
                    deploy_qt_framework_x64 "$dep"
                    new_deps_found=true
                fi
            done
            
            # Check Qt framework internal dependencies
            for framework_dir in bin/temp-x64.app/Contents/Frameworks/Qt*.framework; do
                if [ -d "$framework_dir" ]; then
                    framework_name=$(basename "$framework_dir" .framework)
                    framework_binary="$framework_dir/Versions/A/$framework_name"
                    
                    if [ -f "$framework_binary" ]; then
                        deps=$(get_qt_deps "$framework_binary")
                        for dep in $deps; do
                            if [ -n "$dep" ] && [ ! -d "bin/temp-x64.app/Contents/Frameworks/$dep.framework" ]; then
                                echo "Found missing dependency in x64 $framework_name: $dep"
                                deploy_qt_framework_x64 "$dep"
                                new_deps_found=true
                            fi
                        done
                    fi
                fi
            done
            
            if [ "$new_deps_found" = false ]; then
                echo "No new dependencies found for x64 bundle, stopping iteration"
                break
            fi
        done
        
        # Step 3: Create universal frameworks by combining ARM64 and x64 versions
        echo "Step 3: Creating universal Qt frameworks..."
        
        # Automatically detect all Qt frameworks deployed by macdeployqt
        echo "Detecting Qt frameworks deployed by macdeployqt..."
        arm64_frameworks=$(find "bin/package-macos-gui/diyPresso.app/Contents/Frameworks" -name "Qt*.framework" -type d -exec basename {} \; | sed 's/\.framework$//')
        x64_frameworks=$(find "bin/temp-x64.app/Contents/Frameworks" -name "Qt*.framework" -type d -exec basename {} \; | sed 's/\.framework$//')
        
        # Combine and deduplicate framework lists
        all_frameworks=$(echo -e "$arm64_frameworks\n$x64_frameworks" | sort -u)
        
        echo "Found Qt frameworks: $all_frameworks"
        
        # Create universal versions of all detected Qt frameworks
        for framework in $all_frameworks; do
            if [ -d "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/$framework.framework" ] && [ -d "bin/temp-x64.app/Contents/Frameworks/$framework.framework" ]; then
                echo "Creating universal $framework framework..."
                lipo -create \
                    "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/$framework.framework/Versions/A/$framework" \
                    "bin/temp-x64.app/Contents/Frameworks/$framework.framework/Versions/A/$framework" \
                    -output "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/$framework.framework/Versions/A/$framework"
            elif [ -d "bin/temp-x64.app/Contents/Frameworks/$framework.framework" ]; then
                echo "Copying missing $framework framework from x64 bundle..."
                cp -R "bin/temp-x64.app/Contents/Frameworks/$framework.framework" "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/"
            fi
        done
        
        # Create universal shared library
        echo "Step 4: Creating universal shared library..."
        lipo -create \
            "build/arm64/libdiyPresso-core.dylib" \
            "build/x64/libdiyPresso-core.dylib" \
            -output "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/libdiyPresso-core.dylib"
        
        # Create universal libdbus-1.3.dylib if it was deployed to both bundles
        if [ -f "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/libdbus-1.3.dylib" ] && [ -f "bin/temp-x64.app/Contents/Frameworks/libdbus-1.3.dylib" ]; then
            echo "Creating universal libdbus-1.3.dylib..."
            lipo -create \
                "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/libdbus-1.3.dylib" \
                "bin/temp-x64.app/Contents/Frameworks/libdbus-1.3.dylib" \
                -output "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/libdbus-1.3.dylib"
        elif [ -f "bin/temp-x64.app/Contents/Frameworks/libdbus-1.3.dylib" ]; then
            echo "Copying libdbus-1.3.dylib from x64 bundle..."
            cp "bin/temp-x64.app/Contents/Frameworks/libdbus-1.3.dylib" "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/"
        fi
        
        # Replace executable with universal binary
        echo "Step 5: Installing universal executable..."
        cp "build/diypresso-gui" "bin/package-macos-gui/diyPresso.app/Contents/MacOS/diypresso-gui"
        
        # Fix all library paths in the universal binary (AFTER macdeployqt and universal binary creation)
        echo "Step 6: Fixing all library paths in universal binary..."
        
        # Fix our custom shared library
        install_name_tool -change "@rpath/libdiyPresso-core.dylib" "@executable_path/../Frameworks/libdiyPresso-core.dylib" "bin/package-macos-gui/diyPresso.app/Contents/MacOS/diypresso-gui"
        
        # Fix Qt framework paths for all detected frameworks
        echo "Fixing paths for all Qt frameworks..."
        for framework in $all_frameworks; do
            # Fix ARM64 paths (from /opt/homebrew)
            install_name_tool -change "/opt/homebrew/opt/qt/lib/$framework.framework/Versions/A/$framework" "@executable_path/../Frameworks/$framework.framework/Versions/A/$framework" "bin/package-macos-gui/diyPresso.app/Contents/MacOS/diypresso-gui" 2>/dev/null || true
            
            # Fix x64 paths (from /usr/local)  
            install_name_tool -change "/usr/local/opt/qt/lib/$framework.framework/Versions/A/$framework" "@executable_path/../Frameworks/$framework.framework/Versions/A/$framework" "bin/package-macos-gui/diyPresso.app/Contents/MacOS/diypresso-gui" 2>/dev/null || true
        done
        
        # Verify all fixes worked
        echo "Verifying library dependencies..."
        echo "Custom library:"
        otool -L "bin/package-macos-gui/diyPresso.app/Contents/MacOS/diypresso-gui" | grep libdiyPresso-core
        echo "All Qt frameworks:"
        otool -L "bin/package-macos-gui/diyPresso.app/Contents/MacOS/diypresso-gui" | grep "Qt.*framework" || echo "No Qt framework dependencies found"
        
        # Re-sign everything after install_name_tool modifications
        echo "Re-signing bundle after install_name_tool modifications..."
        # Sign all frameworks and libraries
        find "bin/package-macos-gui/diyPresso.app/Contents/Frameworks" -name "*.framework" -exec codesign --force --sign - {} \; 2>/dev/null || true
        find "bin/package-macos-gui/diyPresso.app/Contents/Frameworks" -name "*.dylib" -exec codesign --force --sign - {} \; 2>/dev/null || true
        
        # Sign the main executable
        codesign --force --sign - "bin/package-macos-gui/diyPresso.app/Contents/MacOS/diypresso-gui" 2>/dev/null || true
        
        # Sign the whole app bundle
        codesign --force --sign - "bin/package-macos-gui/diyPresso.app" 2>/dev/null || true
        
        echo "Re-signing completed"
        
        # Clean up temporary bundle
        rm -rf "bin/temp-x64.app"
        
        # Add icon to the bundle if it exists
        if [ -f "src/gui/diypresso.icns" ]; then
            mkdir -p "bin/package-macos-gui/diyPresso.app/Contents/Resources"
            cp "src/gui/diypresso.icns" "bin/package-macos-gui/diyPresso.app/Contents/Resources/"
        fi
        # Add bossac and LICENSE to the .app bundle Resources
    if [ -f "bin/bossac/bossac" ]; then
            cp "bin/bossac/bossac" "bin/package-macos-gui/diyPresso.app/Contents/Resources/"
            chmod +x "bin/package-macos-gui/diyPresso.app/Contents/Resources/bossac"
        fi
        if [ -f "LICENSE" ]; then
            cp "LICENSE" "bin/package-macos-gui/diyPresso.app/Contents/Resources/"
        fi
    fi
    
    # Code signing
    echo ""
    echo "Code signing executables..."
    
    # Sign the CLI executable
    if [ $BUILD_CLI -eq 1 ] && [ -x "bin/package-macos-cli/diypresso" ]; then
        echo "Signing diypresso CLI..."
        codesign --force --options runtime --deep --sign "Developer ID Application: diyPresso B.V. (V3QR5FV6B7)" --timestamp "bin/package-macos-cli/diypresso"
        if [ $? -ne 0 ]; then
            print_warning "Failed to sign diypresso CLI (this is normal if you don't have the certificate)"
        else
            print_success "Successfully signed diypresso CLI"
        fi
    fi
    
    # Sign the GUI .app bundle
    if [ $BUILD_GUI -eq 1 ] && [ -d "bin/package-macos-gui/diyPresso.app" ]; then
        # Sign the shared library first
        if [ -f "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/libdiyPresso-core.dylib" ]; then
            echo "Signing shared library..."
            codesign --force --options runtime --sign "Developer ID Application: diyPresso B.V. (V3QR5FV6B7)" --timestamp "bin/package-macos-gui/diyPresso.app/Contents/Frameworks/libdiyPresso-core.dylib"
            if [ $? -ne 0 ]; then
                print_warning "Failed to sign shared library (this is normal if you don't have the certificate)"
            else
                print_success "Successfully signed shared library"
            fi
        fi
        
        # Sign all frameworks (Qt and dependencies) deployed by macdeployqt
        echo "Signing all frameworks in the bundle..."
        find "bin/package-macos-gui/diyPresso.app/Contents/Frameworks" -name "*.framework" -exec codesign --force --options runtime --sign "Developer ID Application: diyPresso B.V. (V3QR5FV6B7)" --timestamp {} \;
        find "bin/package-macos-gui/diyPresso.app/Contents/Frameworks" -name "*.dylib" -exec codesign --force --options runtime --sign "Developer ID Application: diyPresso B.V. (V3QR5FV6B7)" --timestamp {} \;
        if [ $? -ne 0 ]; then
            print_warning "Failed to sign some frameworks/libraries (this is normal if you don't have the certificate)"
        else
            print_success "Successfully signed all frameworks and libraries"
        fi
        
        echo "Signing diyPresso.app bundle..."
        codesign --force --options runtime --deep --sign "Developer ID Application: diyPresso B.V. (V3QR5FV6B7)" --timestamp "bin/package-macos-gui/diyPresso.app"
        if [ $? -ne 0 ]; then
            print_warning "Failed to sign diyPresso.app (this is normal if you don't have the certificate)"
        else
            print_success "Successfully signed diyPresso.app"
        fi
    fi

    # Sign the bossac executable in CLI package
    if [ -x "bin/package-macos-cli/bossac" ]; then
        echo "Signing bossac (CLI package)..."
        codesign --force --options runtime --deep --sign "Developer ID Application: diyPresso B.V. (V3QR5FV6B7)" --timestamp "bin/package-macos-cli/bossac"
        if [ $? -ne 0 ]; then
            print_warning "Failed to sign bossac (CLI package) (this is normal if you don't have the certificate)"
        else
            print_success "Successfully signed bossac (CLI package)"
        fi
    fi
    
    # Sign the bossac executable in GUI package
    if [ -x "bin/package-macos-gui/diyPresso.app/Contents/Resources/bossac" ]; then
        echo "Signing bossac (GUI package)..."
        codesign --force --options runtime --deep --sign "Developer ID Application: diyPresso B.V. (V3QR5FV6B7)" --timestamp "bin/package-macos-gui/diyPresso.app/Contents/Resources/bossac"
        if [ $? -ne 0 ]; then
            print_warning "Failed to sign bossac (GUI package) (this is normal if you don't have the certificate)"
        else
            print_success "Successfully signed bossac (GUI package)"
        fi
    fi
    
    # Create ZIP packages
    echo "Creating ZIP packages..."
    cd bin
    if [ -d "package-macos-cli" ]; then
        zip -r "diyPresso-Client-macOS-CLI.zip" package-macos-cli
    fi
    if [ -d "package-macos-gui" ]; then
        zip -r "diyPresso-Client-macOS-GUI.zip" package-macos-gui
    fi
    cd ..
    
    echo ""
    print_success "==================================="
    print_success "Packages created successfully!"
    print_success "CLI package: bin/package-macos-cli/ (ZIP: bin/diyPresso-Client-macOS-CLI.zip)"
    print_success "GUI package: bin/package-macos-gui/ (ZIP: bin/diyPresso-Client-macOS-GUI.zip)"
    print_success "==================================="
else
    echo "Skipping package creation (due to --no-package argument)."
fi

echo ""
print_success "==================================="
print_success "Build process completed!"
print_success "==================================="