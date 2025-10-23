#!/bin/bash
# Build Karabiner-Elements with custom code signing
# Uses self-signed certificate: me@kuzmeech.com
#
# Usage:
#   cd ~/hobby/karabiner-fork
#   ./build-signed.sh [grabber|full]
#
# Options:
#   grabber - Build only grabber component (default, works without Apple Developer ID)
#   full    - Build full package including DriverKit (requires Apple Developer ID)

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CODESIGN_IDENTITY="me@kuzmeech.com"
CODESIGN_IDENTITY_HASH="17F3838F28D599F5DA5E18580FF438735F1B4432"

echo "🔨 Building Karabiner-Elements with custom code signing"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Directory: $SCRIPT_DIR"
echo "Identity:  $CODESIGN_IDENTITY"
echo "Hash:      $CODESIGN_IDENTITY_HASH"
echo ""

# Verify identity exists
if ! security find-identity -p codesigning -v | grep -q "$CODESIGN_IDENTITY_HASH"; then
    echo "❌ ERROR: Code signing identity not found in keychain"
    echo ""
    echo "Expected: $CODESIGN_IDENTITY ($CODESIGN_IDENTITY_HASH)"
    echo ""
    echo "To import the certificate:"
    echo "  1. Find the .p12 file (check ~/hobby/codesign/ or AirDrop from another Mac)"
    echo "  2. Double-click to import to login keychain"
    echo "  3. In Keychain Access, mark as 'Always Trust'"
    exit 1
fi

echo "✅ Code signing identity verified"
echo ""

# Set environment variable for Xcode
export CODE_SIGN_IDENTITY="$CODESIGN_IDENTITY_HASH"
export CODE_SIGN_STYLE="Manual"

# For DriverKit, we need to disable provisioning profile checks
export OTHER_CODE_SIGN_FLAGS="--deep --force --options runtime"

echo "📦 Building DriverKit VirtualHIDDevice..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

cd "$SCRIPT_DIR/vendor/Karabiner-DriverKit-VirtualHIDDevice"

# Build DriverKit with ad-hoc signing first, then re-sign
echo "Building DriverKit components..."

# For DriverKit, we need to use ad-hoc signing during build, then resign
# This is because DriverKit requires specific entitlements and provisioning
# that we can't easily override without Apple Developer ID

# Try building with manual signing disabled
make clean 2>/dev/null || true

BUILD_MODE="${1:-grabber}"

if [[ "$BUILD_MODE" == "full" ]]; then
    echo ""
    echo "🚨 FULL BUILD MODE"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "⚠️  This requires Apple Developer ID"
    echo ""
    echo "If you have Developer ID, set these environment variables:"
    echo "  export KARABINER_CODESIGN_IDENTITY='Developer ID Application: Your Name (TEAM_ID)'"
    echo ""
    read -p "Do you have Apple Developer ID configured? [y/N] " -n 1 -r
    echo

    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "❌ Full build requires Apple Developer ID"
        echo ""
        echo "Use './build-signed.sh grabber' for grabber-only build"
        exit 1
    fi

    echo "Building full package with DriverKit..."
    make package

    echo ""
    echo "✅ Full package built successfully"
    echo ""
    echo "Package location: ./dist/"
    echo "To install: Follow DEVELOPMENT.md instructions"
else
    echo ""
    echo "📦 GRABBER-ONLY BUILD MODE"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "ℹ️  Building only karabiner_grabber component"
    echo "ℹ️  DriverKit VirtualHIDDevice will NOT be rebuilt"
    echo "ℹ️  Existing DriverKit (1.8.0) will continue to work"
    echo ""

    cd "$SCRIPT_DIR"

    # Build grabber
    echo "Building karabiner_grabber..."
    cd src/core/grabber
    make clean 2>/dev/null || true
    make || {
        echo "❌ Grabber build failed"
        exit 1
    }

    echo ""
    echo "✅ karabiner_grabber built successfully"
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📥 INSTALLATION"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "To install the new grabber:"
    echo ""
    echo "  cd $SCRIPT_DIR/src/core/grabber"
    echo "  sudo make install"
    echo ""
    echo "This will replace:"
    echo "  /Library/Application Support/org.pqrs/Karabiner-Elements/bin/karabiner_grabber"
    echo ""
    echo "Then restart Karabiner:"
    echo "  sudo launchctl kickstart -k system/org.pqrs.service.daemon.karabiner_grabber"
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "⚠️  NOTE ABOUT DEBUG LOGGING"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "The debug logging we added is in DriverKit VirtualHIDKeyboard."
    echo "Grabber-only build does NOT include these debug logs."
    echo ""
    echo "Options:"
    echo "  1. Use monitoring script: monitor-karabiner-freeze"
    echo "  2. Get Apple Developer ID and run: ./build-signed.sh full"
    echo "  3. Report findings to Karabiner GitHub with existing logs"
    echo ""
fi
