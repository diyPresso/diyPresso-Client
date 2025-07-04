#!/bin/bash

# Notarization script for diyPresso Client macOS package
# This script notarizes the distribution ZIP (no stapling)

echo "==================================="
echo "diyPresso Client - macOS Notarization"
echo "==================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

print_info() {
    echo -e "${BLUE}INFO: $1${NC}"
}

# Check if package exists
PACKAGE_DIR="bin/package-macos"
ZIP_FILE="bin/diyPresso-Client-macOS.zip"

if [ ! -d "$PACKAGE_DIR" ]; then
    print_error "Package directory not found: $PACKAGE_DIR"
    echo "Please run the build script first: ./build-macos.sh"
    exit 1
fi

if [ ! -f "$ZIP_FILE" ]; then
    print_error "ZIP package not found: $ZIP_FILE"
    echo "Please run the build script first: ./build-macos.sh"
    exit 1
fi

# Check if executables are signed
print_info "Checking code signatures..."

if ! codesign --verify "$PACKAGE_DIR/diypresso" >/dev/null 2>&1; then
    print_error "diypresso is not properly signed!"
    exit 1
fi

if ! codesign --verify "$PACKAGE_DIR/bossac" >/dev/null 2>&1; then
    print_error "bossac is not properly signed!"
    exit 1
fi

print_success "Both executables are properly signed"

# Get Apple ID and password from Keychain
if [ -z "$APPLE_ID" ]; then
    print_warning "Apple ID not set in environment variable"
    echo "Please set: export APPLE_ID=\"your-apple-id@example.com\""
    echo ""
    
    # Prompt for Apple ID if not set
    read -p "Enter your Apple ID: " APPLE_ID
    
    if [ -z "$APPLE_ID" ]; then
        print_error "Apple ID is required for notarization"
        exit 1
    fi
fi

# Try to get password from Keychain
print_info "Looking for app-specific password in Keychain..."
APPLE_ID_PASSWORD=$(security find-generic-password -s "diyPresso Notarization" -a "$APPLE_ID" -w 2>/dev/null)

if [ $? -ne 0 ] || [ -z "$APPLE_ID_PASSWORD" ]; then
    print_error "App-specific password not found in Keychain. Run ./setup-keychain.sh first."
    exit 1
else
    print_success "Found app-specific password in Keychain"
fi

# Check if xcrun is available
if ! command -v xcrun >/dev/null 2>&1; then
    print_error "xcrun not found. Please install Xcode Command Line Tools"
    exit 1
fi

# Notarize the distribution ZIP
print_info "Submitting $ZIP_FILE for notarization..."
xcrun notarytool submit "$ZIP_FILE" \
    --apple-id "$APPLE_ID" \
    --password "$APPLE_ID_PASSWORD" \
    --team-id "V3QR5FV6B7" \
    --wait \
    --output-format json > notarization.log 2>&1

SUBMIT_RESULT=$?
if [ $SUBMIT_RESULT -ne 0 ]; then
    print_error "Notarization submission failed!"
    cat notarization.log
    exit 1
fi

NOTARIZATION_STATUS=$(grep -o '"status":"[^"]*"' notarization.log | cut -d'"' -f4)
if [ "$NOTARIZATION_STATUS" != "Accepted" ]; then
    print_error "Notarization failed! Status: $NOTARIZATION_STATUS"
    cat notarization.log
    exit 1
fi

print_success "ZIP package notarized successfully!"

# Verify notarization
print_info "Verifying notarization..."
SUBMISSION_ID=$(grep -o '"id":"[^"]*"' notarization.log | cut -d'"' -f4)
if [ -n "$SUBMISSION_ID" ]; then
    print_info "Submission ID: $SUBMISSION_ID"
    
    # Get detailed notarization info
    DETAILED_INFO=$(xcrun notarytool info "$SUBMISSION_ID" \
        --apple-id "$APPLE_ID" \
        --password "$APPLE_ID_PASSWORD" \
        --team-id "V3QR5FV6B7" \
        --output-format json)
    
    echo "Notarization details:"
    echo "$DETAILED_INFO"
    
    # Verify code signatures (this is what matters for ZIP distribution)
    print_info "Verifying code signatures..."
    TEMP_DIR=$(mktemp -d)
    unzip -q "$ZIP_FILE" -d "$TEMP_DIR"
    
    for exe in diypresso bossac; do
        EXE_PATH="$TEMP_DIR/package-macos/$exe"
        if [ -x "$EXE_PATH" ]; then
            print_info "Verifying $exe signature..."
            if codesign --verify --verbose=4 "$EXE_PATH" >/dev/null 2>&1; then
                print_success "$exe has valid code signature"
            else
                print_error "$exe has invalid code signature"
            fi
        fi
    done
    
    rm -rf "$TEMP_DIR"
    
    # Note about Gatekeeper behavior
    print_info "Note: When users extract this ZIP, Gatekeeper will allow execution"
    print_info "because the ZIP archive is notarized and the executables are properly signed."
else
    print_warning "Could not extract submission ID for verification"
fi

print_success "==================================="
print_success "Notarization process completed!"
print_success "Package: $ZIP_FILE"
print_success "==================================="