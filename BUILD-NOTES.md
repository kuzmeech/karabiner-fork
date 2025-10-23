# Karabiner Fork Build Notes

## Session: 2025-10-18

### What We Built

✅ **Successfully built** `karabiner_grabber` component
- Size: 16MB universal binary (x86_64 + arm64)
- Build script: `./build-signed.sh grabber`
- Code signing: Uses `me@kuzmeech.com` certificate

### Installation Issue

⚠️ **Grabber daemon not starting after installation**

**Symptoms**:
- console_user_server: ✅ RUNNING
- session_monitor: ✅ RUNNING
- grabber: ❌ NOT RUNNING

**Cause**: macOS Service Management requires approval for daemon changes

**Solution**: After reinstalling Karabiner:
1. Open System Settings > General > Login Items & Extensions
2. Enable "Karabiner-VirtualHIDDevice-Daemon"
3. Or: Reboot Mac to reset Service Management state

### Key Learnings

1. **Build Works**: Our custom build compiles successfully
2. **Service Management**: Replacing system daemons requires macOS approval
3. **Testing Limitation**: Can't easily test custom grabber without full package

### TODO for Next Build

#### 1. Change Product Name for Easy Identification

**Problem**: Custom build shows same name in System Settings as official build

**Solution**: Modify product name in build to show it's custom:

**File to Edit**: `src/core/grabber/project.yml`
```yaml
# Change product name from "karabiner_grabber" to "karabiner_grabber_debug"
# This will show as different binary in System Settings
```

**Or in Xcode project settings**:
- Product Name: `karabiner_grabber_debug`
- Bundle Identifier: `org.pqrs.karabiner.grabber.debug`

This way it's clear which version is running.

#### 2. Alternative Approach: Build Full Package

Instead of replacing just grabber, build complete `.dmg` package:

```bash
cd ~/hobby/karabiner-fork

# Need Apple Developer ID for this
./build-signed.sh full

# Or if we get Developer ID:
export KARABINER_CODESIGN_IDENTITY='Developer ID Application: ...'
make package
```

This creates proper installer that handles Service Management correctly.

#### 3. Or: Use Monitoring Script

**Recommended for now** - no rebuild needed:

```bash
monitor-karabiner-freeze
```

Captures freeze events with snapshots to `/Volumes/ramdisk/karabiner-freeze-snapshots/`

---

## Debug Logging Added (in fork)

**File**: `vendor/Karabiner-DriverKit-VirtualHIDDevice/src/.../org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard.cpp`

**Changes**:
1. `postReport()` - Logs slow events (>5ms), failures
2. `setReport()` - Logs LED state updates

**Note**: These changes are in DriverKit, which requires:
- Apple Developer ID to build
- Or: Full package build with proper signing

**Current grabber-only build does NOT include these debug logs**

---

## Build Commands Reference

### Build Grabber Only (Works Without Apple Developer ID)

```bash
cd ~/hobby/karabiner-fork
./build-signed.sh grabber
```

### Install Built Grabber

```bash
cd ~/hobby/karabiner-fork/src/core/grabber

# Backup current
sudo cp "/Library/Application Support/org.pqrs/Karabiner-Elements/bin/karabiner_grabber" \
   "/Library/Application Support/org.pqrs/Karabiner-Elements/bin/karabiner_grabber.backup-$(date +%Y%m%d)"

# Install
sudo make install

# Restart (may require System Settings approval)
sudo launchctl kickstart -k system/org.pqrs.service.daemon.karabiner_grabber
```

### Restore Original

```bash
# Reinstall via Homebrew (easiest)
brew reinstall --cask karabiner-elements

# Or restore backup
sudo cp "/Library/Application Support/org.pqrs/Karabiner-Elements/bin/karabiner_grabber.backup-YYYYMMDD" \
   "/Library/Application Support/org.pqrs/Karabiner-Elements/bin/karabiner_grabber"
sudo launchctl kickstart -k system/org.pqrs.service.daemon.karabiner_grabber
```

---

## Investigation Results

### Keyboard Freeze Analysis

**Findings**:
- USB timeouts from Razer mouse correlate with freezes
- VirtualHIDKeyboard `handleReport()` blocks synchronously
- Both USB and Bluetooth keyboards freeze (same Virtual Keyboard)

**Documentation**:
- `ARCHITECTURE.md` - Complete architecture
- `FREEZE-INVESTIGATION-SUMMARY.md` - Detailed findings
- `~/hobby/mac/bgc/KEYBOARD-FREEZE-DIAGNOSIS.md` - Initial diagnosis

### Next Steps for Investigation

1. ✅ Collect freeze snapshots with monitoring script
2. ⏳ Apply Razer Control polling patch (reduce USB load)
3. ⏳ Monitor over 3-5 days
4. ⏳ Report to Karabiner GitHub if continues

---

## Files Created This Session

### In Fork
- `~/hobby/karabiner-fork/build-signed.sh` - Build script
- `~/hobby/karabiner-fork/ARCHITECTURE.md` - Architecture docs
- `~/hobby/karabiner-fork/FREEZE-INVESTIGATION-SUMMARY.md` - Findings
- `~/hobby/karabiner-fork/README-BUILD.md` - Build guide
- `~/hobby/karabiner-fork/BUILD-NOTES.md` - This file

### Monitoring
- `/Users/max/bin/monitor-karabiner-freeze` - Freeze monitoring script

### Documentation Updates
- `~/hobby/codesign/README.md` - Added Karabiner section

---

**Status**: Build infrastructure ready, monitoring in place
**Recommendation**: Use monitoring script for now, revisit custom build when we get Apple Developer ID or after collecting more freeze data

**Last Updated**: 2025-10-18 23:23
