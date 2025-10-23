# Karabiner-Elements Fork - Build & Install Guide

**Fork**: https://github.com/kuzmeech/karabiner-fork (private)
**Purpose**: Debug keyboard freeze issues with custom logging

---

## Quick Start

### Build Grabber Only (Recommended)

```bash
cd ~/hobby/karabiner-fork
./build-signed.sh grabber
```

**What this does**:
- Builds `karabiner_grabber` with your self-signed certificate
- Does NOT rebuild DriverKit (uses existing 1.8.0)
- No Apple Developer ID needed

### Install Built Grabber

```bash
cd ~/hobby/karabiner-fork/src/core/grabber
sudo make install
sudo launchctl kickstart -k system/org.pqrs.service.daemon.karabiner_grabber
```

---

## Full Build (Requires Apple Developer ID)

```bash
cd ~/hobby/karabiner-fork
./build-signed.sh full
```

**Requirements**:
- Apple Developer ID ($99/year)
- Set environment variable: `export KARABINER_CODESIGN_IDENTITY='Developer ID Application: ...'`

**What this builds**:
- Complete Karabiner-Elements package
- DriverKit VirtualHIDDevice with debug logging
- Installer .dmg

---

## Code Signing

### Certificate Info
- **Identity**: `me@kuzmeech.com`
- **Hash**: `17F3838F28D599F5DA5E18580FF438735F1B4432`
- **Location**: Login Keychain
- **Shared with**: Language Switcher, other hobby projects

### Verify Certificate

```bash
security find-identity -p codesigning -v | grep "me@kuzmeech"
```

Should show:
```
1) 17F3838F28D599F5DA5E18580FF438735F1B4432 "me@kuzmeech.com"
```

### If Certificate Missing

1. Find `.p12` file (check `~/hobby/codesign/` or AirDrop from another Mac)
2. Double-click to import to Login Keychain
3. In Keychain Access: Right-click → Get Info → Trust → "Always Trust"
4. Re-run build script

---

## What's Modified

### Debug Logging Added

**File**: `vendor/Karabiner-DriverKit-VirtualHIDDevice/src/.../org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard.cpp`

**Changes**:
1. `postReport()` - Measures `handleReport()` duration, logs slow events (>5ms)
2. `setReport()` - Logs LED state updates (caps lock feedback)

**View changes**:
```bash
cd ~/hobby/karabiner-fork
git diff vendor/Karabiner-DriverKit-VirtualHIDDevice/
```

### Why Only in DriverKit?

The keyboard freeze happens in **VirtualHIDKeyboard DriverKit layer**, not in grabber. The critical `handleReport()` call that blocks is in DriverKit.

---

## Alternative: Monitoring Without Rebuild

If you can't build DriverKit (no Apple Developer ID), use monitoring script:

```bash
monitor-karabiner-freeze
```

**What it captures**:
- USB timeout rate
- Karabiner errors
- Process status
- Automatic snapshots when high error rate

**Snapshots saved to**: `/Volumes/ramdisk/karabiner-freeze-snapshots/`

---

## Documentation

### Investigation Docs
- `ARCHITECTURE.md` - Complete Karabiner architecture
- `FREEZE-INVESTIGATION-SUMMARY.md` - Findings, hypotheses, action plan
- `~/hobby/mac/bgc/KEYBOARD-FREEZE-DIAGNOSIS.md` - Initial diagnosis

### Code Signing
- `~/hobby/codesign/README.md` - Certificate management guide

### Monitoring
- `/Users/max/bin/monitor-karabiner-freeze` - Freeze monitoring script

---

## Troubleshooting

### Build Fails with "No profiles"

**Error**: `No profiles for 'org.pqrs.Karabiner-DriverKit-VirtualHIDDevice'`

**Solution**: This is expected for DriverKit. Use grabber-only build:
```bash
./build-signed.sh grabber
```

### Grabber Won't Start After Install

**Check**:
```bash
ps aux | grep karabiner_grabber
launchctl print system/org.pqrs.service.daemon.karabiner_grabber
```

**Restart**:
```bash
sudo launchctl kickstart -k system/org.pqrs.service.daemon.karabiner_grabber
```

**View logs**:
```bash
log show --last 5m --predicate 'process CONTAINS "karabiner_grabber"' --style compact
```

### Permission Denied During Install

**Issue**: `make install` needs root

**Solution**:
```bash
sudo make install
```

Or check file permissions:
```bash
ls -la /Library/Application\ Support/org.pqrs/Karabiner-Elements/bin/
```

---

## Upstream Sync

To sync with upstream Karabiner-Elements:

```bash
cd ~/hobby/karabiner-fork
git fetch upstream
git merge upstream/main

# If conflicts in VirtualHIDKeyboard.cpp, manually re-apply debug logging
```

---

## Next Steps

1. **Immediate**: Use monitoring script without rebuild
2. **Short-term**: Collect freeze snapshots for GitHub issue
3. **Long-term**: Get Apple Developer ID for full build, or report to upstream

See `FREEZE-INVESTIGATION-SUMMARY.md` for complete action plan.
