#!/bin/bash

echo "==================================="
echo "Test: x64-only app bundle creation"
echo "==================================="

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

# Check VCPKG_ROOT
if [ -z "$VCPKG_ROOT" ]; then
    if [ -d "$HOME/vcpkg" ]; then
        export VCPKG_ROOT="$HOME/vcpkg"
    else
        print_error "VCPKG_ROOT not set"
        exit 1
    fi
fi

echo "Using VCPKG_ROOT: $VCPKG_ROOT"

# Clean and create test build directory
echo ""
echo "Setting up test build directory..."
rm -rf test-build-x64
mkdir -p test-build-x64
cd test-build-x64

# Configure for x64 only
echo ""
echo "Configuring CMake for x64..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_PREFIX_PATH="/usr/local/lib/cmake" \
    -DVCPKG_TARGET_TRIPLET="x64-osx" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="x86_64" \
    -DBUILD_CLI=OFF \
    -DBUILD_GUI=ON

if [ $? -ne 0 ]; then
    print_error "CMake configuration failed!"
    exit 1
fi

# Build the project
echo ""
echo "Building x64 GUI..."
make -j$(sysctl -n hw.ncpu)

if [ $? -ne 0 ]; then
    print_error "Build failed!"
    exit 1
fi

# Check what we built
echo ""
echo "Build results:"
ls -la *.app/Contents/MacOS/

# Test before macdeployqt
echo ""
echo "Testing before macdeployqt..."
echo "Dependencies before deployment:"
otool -L diypresso-gui.app/Contents/MacOS/diypresso-gui

# Copy our shared library manually first
echo ""
echo "Adding our shared library..."
mkdir -p diypresso-gui.app/Contents/Frameworks
cp libdiyPresso-core.dylib diypresso-gui.app/Contents/Frameworks/
install_name_tool -change "@rpath/libdiyPresso-core.dylib" "@executable_path/../Frameworks/libdiyPresso-core.dylib" diypresso-gui.app/Contents/MacOS/diypresso-gui

# Run macdeployqt (use x64 Qt path)
echo ""
echo "Running macdeployqt with x64 Qt..."
PATH="/usr/local/bin:$PATH" macdeployqt diypresso-gui.app -verbose=2

if [ $? -ne 0 ]; then
    print_error "macdeployqt failed!"
    exit 1
fi

# Manually deploy missing Qt frameworks that macdeployqt failed to include
echo ""
echo "Manually deploying missing Qt frameworks..."
QT_PATH="/usr/local/opt/qt/lib"

# Function to deploy a Qt framework if missing
deploy_qt_framework() {
    local framework_name="$1"
    if [ ! -d "diypresso-gui.app/Contents/Frameworks/$framework_name.framework" ] && [ -d "$QT_PATH/$framework_name.framework" ]; then
        echo "Deploying missing $framework_name..."
        cp -R "$QT_PATH/$framework_name.framework" diypresso-gui.app/Contents/Frameworks/
        
        # Fix its dependencies
        local binary="diypresso-gui.app/Contents/Frameworks/$framework_name.framework/Versions/A/$framework_name"
        install_name_tool -change "$QT_PATH/QtCore.framework/Versions/A/QtCore" "@executable_path/../Frameworks/QtCore.framework/Versions/A/QtCore" "$binary" 2>/dev/null || true
        install_name_tool -change "$QT_PATH/QtGui.framework/Versions/A/QtGui" "@executable_path/../Frameworks/QtGui.framework/Versions/A/QtGui" "$binary" 2>/dev/null || true
        install_name_tool -change "$QT_PATH/QtWidgets.framework/Versions/A/QtWidgets" "@executable_path/../Frameworks/QtWidgets.framework/Versions/A/QtWidgets" "$binary" 2>/dev/null || true
        install_name_tool -change "$QT_PATH/QtDBus.framework/Versions/A/QtDBus" "@executable_path/../Frameworks/QtDBus.framework/Versions/A/QtDBus" "$binary" 2>/dev/null || true
        
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
                cp "$dbus_lib" "diypresso-gui.app/Contents/Frameworks/"
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
        
        print_success "$framework_name deployed and fixed"
    fi
}

# Check for missing Qt dependencies and deploy them
echo "Checking for missing Qt dependencies..."

# Function to extract Qt framework dependencies from a binary
get_qt_deps() {
    local binary="$1"
    if [ -f "$binary" ]; then
        otool -L "$binary" | grep -E "(@rpath|/opt/homebrew/opt/qt/lib|/usr/local/opt/qt/lib).*Qt" | sed 's/.*Qt/Qt/' | sed 's/\.framework.*//' | sort -u
    fi
}

# Check main executable dependencies
echo "Checking main executable dependencies..."
qt_deps=$(get_qt_deps "diypresso-gui.app/Contents/MacOS/diypresso-gui")
for dep in $qt_deps; do
    if [ -n "$dep" ]; then
        deploy_qt_framework "$dep"
    fi
done

# Check Qt framework internal dependencies (iterate until no new dependencies found)
echo "Checking Qt framework internal dependencies..."
max_iterations=5
iteration=0
while [ $iteration -lt $max_iterations ]; do
    iteration=$((iteration + 1))
    echo "Dependency check iteration $iteration..."
    
    new_deps_found=false
    for framework_dir in diypresso-gui.app/Contents/Frameworks/Qt*.framework; do
        if [ -d "$framework_dir" ]; then
            framework_name=$(basename "$framework_dir" .framework)
            framework_binary="$framework_dir/Versions/A/$framework_name"
            
            if [ -f "$framework_binary" ]; then
                deps=$(get_qt_deps "$framework_binary")
                for dep in $deps; do
                    if [ -n "$dep" ] && [ ! -d "diypresso-gui.app/Contents/Frameworks/$dep.framework" ]; then
                        echo "Found missing dependency: $dep"
                        deploy_qt_framework "$dep"
                        new_deps_found=true
                    fi
                done
            fi
        fi
    done
    
    if [ "$new_deps_found" = false ]; then
        echo "No new dependencies found, stopping iteration"
        break
    fi
done

# Re-sign everything after modifications
echo ""
echo "Re-signing after modifications..."
# Sign all frameworks and libraries
find diypresso-gui.app/Contents/Frameworks -name "*.framework" -exec codesign --force --sign - {} \; 2>/dev/null
find diypresso-gui.app/Contents/Frameworks -name "*.dylib" -exec codesign --force --sign - {} \; 2>/dev/null

# Sign the main executable
codesign --force --sign - diypresso-gui.app/Contents/MacOS/diypresso-gui

# Sign the whole app bundle
codesign --force --sign - diypresso-gui.app

echo "Re-signing completed"

# Check what frameworks were deployed
echo ""
echo "Frameworks deployed by macdeployqt:"
ls -la diypresso-gui.app/Contents/Frameworks/

echo ""
echo "Qt frameworks specifically:"
ls -la diypresso-gui.app/Contents/Frameworks/ | grep "Qt.*framework" || echo "No Qt frameworks found"

# Check dependencies after macdeployqt
echo ""
echo "Dependencies after macdeployqt:"
otool -L diypresso-gui.app/Contents/MacOS/diypresso-gui

echo ""
echo "Dependencies of QtGui framework:"
if [ -f "diypresso-gui.app/Contents/Frameworks/QtGui.framework/Versions/A/QtGui" ]; then
    otool -L diypresso-gui.app/Contents/Frameworks/QtGui.framework/Versions/A/QtGui
else
    echo "QtGui framework not found in bundle"
fi

# Check architecture of built binary
echo ""
echo "Architecture verification:"
file diypresso-gui.app/Contents/MacOS/diypresso-gui
lipo -info diypresso-gui.app/Contents/MacOS/diypresso-gui

# Verify frameworks are x64
echo ""
echo "Qt framework architectures:"
if [ -f "diypresso-gui.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore" ]; then
    echo "QtCore:"
    file diypresso-gui.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore
fi
if [ -f "diypresso-gui.app/Contents/Frameworks/QtGui.framework/Versions/A/QtGui" ]; then
    echo "QtGui:"
    file diypresso-gui.app/Contents/Frameworks/QtGui.framework/Versions/A/QtGui
fi

# Test the app
echo ""
echo "Testing the app bundle..."
print_success "Test x64 app bundle created: test-build-x64/diypresso-gui.app"
print_success "You can test it with: open test-build-x64/diypresso-gui.app"

cd .. 