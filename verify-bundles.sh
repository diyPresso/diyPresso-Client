#!/bin/bash

echo "==========================================="
echo "Comprehensive Bundle Verification"
echo "==========================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_error() {
    echo -e "${RED}❌ ERROR: $1${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  WARNING: $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

verify_bundle() {
    local bundle_path="$1"
    local expected_arch="$2"
    local bundle_name="$3"
    
    echo ""
    echo "==========================================="
    echo "Verifying $bundle_name ($expected_arch)"
    echo "==========================================="
    
    if [ ! -d "$bundle_path" ]; then
        print_error "Bundle not found: $bundle_path"
        return 1
    fi
    
    local executable="$bundle_path/Contents/MacOS/diypresso-gui"
    
    # 1. Architecture verification
    echo ""
    print_info "1. Architecture Verification"
    if [ -f "$executable" ]; then
        local arch_info=$(file "$executable")
        echo "Binary: $arch_info"
        
        if [[ "$arch_info" == *"$expected_arch"* ]]; then
            print_success "Architecture matches: $expected_arch"
        else
            print_error "Architecture mismatch! Expected: $expected_arch"
        fi
        
        lipo -info "$executable"
    else
        print_error "Executable not found: $executable"
        return 1
    fi
    
    # 2. Dependency analysis
    echo ""
    print_info "2. Dependency Analysis"
    echo "All library dependencies:"
    otool -L "$executable" | grep -E "(dylib|framework)" | while read -r line; do
        if [[ "$line" == *"@executable_path"* ]]; then
            print_success "✓ Bundled: $line"
        elif [[ "$line" == *"/System/Library"* ]] || [[ "$line" == *"/usr/lib"* ]]; then
            print_info "○ System: $line"
        elif [[ "$line" == *"/opt/homebrew"* ]] || [[ "$line" == *"/usr/local"* ]]; then
            print_warning "External dependency: $line"
        else
            print_warning "Unknown: $line"
        fi
    done
    
    # 3. Qt Framework verification
    echo ""
    print_info "3. Qt Framework Verification"
    local frameworks_dir="$bundle_path/Contents/Frameworks"
    if [ -d "$frameworks_dir" ]; then
        echo "Qt frameworks found:"
        ls -1 "$frameworks_dir" | grep "Qt.*framework" | while read -r framework; do
            local framework_binary="$frameworks_dir/$framework/Versions/A/${framework%.framework}"
            if [ -f "$framework_binary" ]; then
                local fw_arch=$(file "$framework_binary" | grep -o "arm64\|x86_64")
                if [ "$fw_arch" = "$expected_arch" ]; then
                    print_success "$framework ($fw_arch)"
                else
                    print_error "$framework (wrong arch: $fw_arch, expected: $expected_arch)"
                fi
            else
                print_error "$framework (binary not found)"
            fi
        done
        
        # Check Qt framework dependencies
        echo ""
        print_info "Qt Framework Dependencies:"
        for framework in "$frameworks_dir"/Qt*.framework; do
            if [ -d "$framework" ]; then
                local fw_name=$(basename "$framework" .framework)
                local fw_binary="$framework/Versions/A/$fw_name"
                if [ -f "$fw_binary" ]; then
                    echo "Dependencies of $fw_name:"
                    otool -L "$fw_binary" | grep "@rpath.*Qt" | while read -r line; do
                        local dep_name=$(echo "$line" | grep -o "Qt[^/]*\.framework" | head -1)
                        if [ -d "$frameworks_dir/$dep_name" ]; then
                            print_success "  ✓ $dep_name (bundled)"
                        else
                            print_error "  ✗ $dep_name (MISSING!)"
                        fi
                    done
                fi
            fi
        done
    else
        print_error "No Frameworks directory found!"
    fi
    
    # 4. External library verification
    echo ""
    print_info "4. External Library Verification"
    if [ -d "$frameworks_dir" ]; then
        echo "Non-Qt libraries:"
        ls -1 "$frameworks_dir" | grep -v "Qt.*framework" | while read -r lib; do
            if [ -f "$frameworks_dir/$lib" ]; then
                local lib_arch=$(file "$frameworks_dir/$lib" | grep -o "arm64\|x86_64")
                if [ "$lib_arch" = "$expected_arch" ]; then
                    print_success "$lib ($lib_arch)"
                else
                    print_error "$lib (wrong arch: $lib_arch)"
                fi
            fi
        done
    fi
    
    # 5. Plugin verification
    echo ""
    print_info "5. Qt Plugin Verification"
    local plugins_dir="$bundle_path/Contents/PlugIns"
    if [ -d "$plugins_dir" ]; then
        echo "Qt plugins found:"
        find "$plugins_dir" -name "*.dylib" | while read -r plugin; do
            local plugin_name=$(basename "$plugin")
            local plugin_arch=$(file "$plugin" | grep -o "arm64\|x86_64")
            if [ "$plugin_arch" = "$expected_arch" ]; then
                print_success "$plugin_name ($plugin_arch)"
            else
                print_error "$plugin_name (wrong arch: $plugin_arch)"
            fi
        done
    else
        print_warning "No PlugIns directory found"
    fi
    
    # 6. Missing Qt frameworks check
    echo ""
    print_info "6. Missing Qt Framework Detection"
    echo "Checking for unresolved Qt dependencies..."
    otool -L "$executable" | grep "@rpath.*Qt" | while read -r line; do
        local dep_name=$(echo "$line" | grep -o "Qt[^/]*\.framework")
        if [ ! -d "$frameworks_dir/$dep_name" ]; then
            print_error "Missing Qt dependency: $dep_name"
        fi
    done
    
    # Check Qt framework internal dependencies
    for framework in "$frameworks_dir"/Qt*.framework; do
        if [ -d "$framework" ]; then
            local fw_name=$(basename "$framework" .framework)
            local fw_binary="$framework/Versions/A/$fw_name"
            if [ -f "$fw_binary" ]; then
                otool -L "$fw_binary" | grep "@rpath.*Qt" | while read -r line; do
                    local dep_name=$(echo "$line" | grep -o "Qt[^/]*\.framework" | head -1)
                    if [ ! -d "$frameworks_dir/$dep_name" ]; then
                        print_error "$fw_name needs missing: $dep_name"
                    fi
                done
            fi
        fi
    done
    
    # 7. Bundle structure verification
    echo ""
    print_info "7. Bundle Structure Verification"
    if [ -f "$bundle_path/Contents/Info.plist" ]; then
        print_success "Info.plist present"
    else
        print_warning "Info.plist missing"
    fi
    
    if [ -f "$bundle_path/Contents/Resources/diypresso.icns" ]; then
        print_success "Icon present"
    else
        print_warning "Icon missing"
    fi
    
    echo ""
    print_info "Bundle verification complete for $bundle_name"
}

# Verify both bundles
verify_bundle "test-build/diypresso-gui.app" "arm64" "ARM64 Bundle"
verify_bundle "test-build-x64/diypresso-gui.app" "x86_64" "x64 Bundle"

echo ""
echo "==========================================="
echo "Verification Summary"
echo "==========================================="
echo "Check the output above for any ❌ errors or ⚠️ warnings"
echo "Both bundles should be self-contained with all Qt dependencies bundled" 