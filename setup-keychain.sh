#!/bin/bash

# Helper script to manage Keychain passwords for diyPresso notarization

echo "==================================="
echo "diyPresso - Keychain Password Setup"
echo "==================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

print_info() {
    echo -e "${BLUE}INFO: $1${NC}"
}

# Check if Apple ID is provided
if [ -z "$1" ]; then
    print_error "Usage: $0 <your-apple-id@example.com>"
    echo ""
    echo "This script will help you store your app-specific password in the Keychain."
    echo "You can generate an app-specific password at: https://appleid.apple.com/account/manage"
    exit 1
fi

APPLE_ID="$1"
KEYCHAIN_SERVICE="diyPresso Notarization"

echo "Setting up Keychain for Apple ID: $APPLE_ID"
echo ""

# Check if password already exists
print_info "Checking if password already exists in Keychain..."
EXISTING_PASSWORD=$(security find-generic-password -s "$KEYCHAIN_SERVICE" -a "$APPLE_ID" -w 2>/dev/null)

if [ $? -eq 0 ] && [ -n "$EXISTING_PASSWORD" ]; then
    print_warning "A password already exists for this Apple ID"
    read -p "Do you want to replace it? (y/n): " REPLACE_PASSWORD
    
    if [[ "$REPLACE_PASSWORD" =~ ^[Yy]$ ]]; then
        print_info "Removing existing password..."
        security delete-generic-password -s "$KEYCHAIN_SERVICE" -a "$APPLE_ID" 2>/dev/null
        print_success "Existing password removed"
    else
        print_info "Keeping existing password"
        exit 0
    fi
fi

echo ""
print_info "Please enter your app-specific password"
echo "Note: This should be an app-specific password, not your regular Apple ID password"
echo "You can generate one at: https://appleid.apple.com/account/manage"
echo ""

read -s -p "App-specific password: " APP_SPECIFIC_PASSWORD
echo ""

if [ -z "$APP_SPECIFIC_PASSWORD" ]; then
    print_error "Password cannot be empty"
    exit 1
fi

# Store password in Keychain
print_info "Storing password in Keychain..."
echo "$APP_SPECIFIC_PASSWORD" | security add-generic-password -s "$KEYCHAIN_SERVICE" -a "$APPLE_ID" -w

if [ $? -eq 0 ]; then
    print_success "==================================="
    print_success "Password stored successfully!"
    print_success "==================================="
    echo ""
    print_info "You can now run notarization without entering your password:"
    echo "  export APPLE_ID=\"$APPLE_ID\""
    echo "  ./notarize-macos.sh"
    echo ""
    print_info "To remove the password later, run:"
    echo "  security delete-generic-password -s \"$KEYCHAIN_SERVICE\" -a \"$APPLE_ID\""
else
    print_error "Failed to store password in Keychain"
    exit 1
fi 