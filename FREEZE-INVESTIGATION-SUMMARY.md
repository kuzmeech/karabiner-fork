# Karabiner Keyboard Freeze - Investigation Summary

**Date**: 2025-10-18
**Fork**: https://github.com/kuzmeech/karabiner-fork (private)
**Local**: `~/hobby/karabiner-fork/`

---

## Problem Summary

Periodic keyboard freeze affecting **both USB and Bluetooth keyboards** simultaneously:
- Mouse continues working
- Keyboard completely unresponsive
- Only fix: Quit Karabiner via menu
- Frequency: ~2 times in 5 hours today (2025-10-18)
- Also reported: typing lag/delays before full freeze

---

## Root Cause Analysis

### Confirmed Facts

1. **Both USB and Bluetooth freeze** → Problem is NOT USB-specific
2. **Mouse works during freeze** → Problem is keyboard-specific
3. **Quit Karabiner fixes instantly** → Problem is in Karabiner layer
4. **USB timeouts correlate with freezes** → USB congestion triggers issue

### Architecture Discovery

```
Physical Keyboard (USB/BT)
         ↓
   IOHIDDevice
         ↓
karabiner_grabber (PID 543)
   - Seizes keyboards via IOKit
   - Applies remapping rules
         ↓
Karabiner-DriverKit-VirtualHIDKeyboard (PID 585)
   - postReport() → handleReport() [SYNCHRONOUS]
   - No timeout, no async queue
         ↓
Virtual HID Keyboard
         ↓
   macOS System
```

**Critical Code Path** (`VirtualHIDKeyboard.cpp:336-372`):
```cpp
kern_return_t postReport() {
  // ... validation ...

  return handleReport(mach_absolute_time(),  // BLOCKS here!
                      report,
                      kIOHIDReportTypeInput,
                      0);
}
```

If `handleReport()` blocks → entire event pipeline stops → deadlock.

### Evidence from Logs

**First freeze (16:35-16:42)**:
- USB timeouts `0xe00002d6` every 5-8 seconds
- karabiner_grabber logging: `kIOHIDLibUserClientPostElementValues:e00002d6`
- ~25 USB timeout errors in 2 minutes

**Second freeze (21:58)**:
- No USB errors in logs (!)
- Keyboard froze anyway
- Bluetooth switch didn't help until Karabiner quit

**Conclusion**: USB timeouts are ONE trigger, but not the ONLY trigger.

---

## Hypotheses

### Hypothesis 1: Event Queue Overflow (MOST LIKELY)

**Mechanism**:
1. USB congestion delays IOHIDDevice events
2. Events accumulate in DriverKit queue
3. `handleReport()` blocks waiting for queue space
4. New events can't be posted → deadlock

**Evidence**:
- Typing lag before freeze (queue filling)
- Synchronous `handleReport()` call (no timeout)
- Both USB and BT affected (same queue)

**Test**: Add logging to measure `handleReport()` duration

### Hypothesis 2: USB Transaction Timeout Propagation

**Mechanism**:
1. Razer mouse generates USB timeout (0xe00002d6)
2. macOS IOKit shares USB transaction context
3. All USB/HID operations delayed globally
4. VirtualHIDKeyboard `handleReport()` times out → deadlock

**Evidence**:
- USB timeouts from Razer correlate with freeze
- Error code 0xe00002d6 = USB transaction timeout
- ~12 timeouts/minute from Razer mouse

**Test**: Disconnect Razer mouse and monitor for freezes

### Hypothesis 3: DriverKit Resource Exhaustion

**Mechanism**:
1. High event rate exhausts DriverKit resources
2. Mach ports / memory / kernel buffers full
3. DriverKit watchdog kills Virtual Keyboard
4. Requires restart to recover

**Evidence**:
- Quit Karabiner fixes (cleans up resources)
- No automatic recovery
- DriverKit PID 585 unchanged (not restarted)

**Test**: Monitor DriverKit memory/CPU during high load

---

## Triggers Identified

### Primary Trigger: USB Congestion

**Source**: Razer Lancehead Tournament Edition mouse
- **12 USB timeouts/minute** (hardware issue)
- **60 USB polls/minute** from Razer Control (2s interval)
- **Combined**: 72 USB events/minute

**USB Error Code**: `0xe00002d6` = kIOUSBTransactionTimeout

### Secondary Trigger: Unknown

Second freeze (21:58) had NO USB errors in logs but still froze. Possible causes:
- High typing rate overwhelming queue
- LED state updates (caps lock) causing race
- DriverKit watchdog timeout
- macOS system issue

---

## Debug Instrumentation Added

### Code Changes (in fork)

**File**: `vendor/Karabiner-DriverKit-VirtualHIDDevice/src/.../org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard.cpp`

**postReport() enhancement**:
```cpp
kern_return_t postReport() {
  uint64_t start_time = mach_absolute_time();

  // ... existing code ...

  kr = handleReport(...);

  uint64_t duration_ns = mach_absolute_time() - start_time;

  if (duration_ns > 5000000) {  // > 5ms
    os_log(OS_LOG_DEFAULT, LOG_PREFIX " postReport SLOW: %llu ns", duration_ns);
  }

  if (kr != kIOReturnSuccess) {
    os_log(OS_LOG_DEFAULT, LOG_PREFIX " handleReport FAILED: 0x%x", kr);
  }
}
```

**setReport() enhancement** (LED feedback):
```cpp
os_log(OS_LOG_DEFAULT, LOG_PREFIX " setReport: posting LED (state: 0x%x)", state);
postReport(memory);
os_log(OS_LOG_DEFAULT, LOG_PREFIX " setReport: posted LED");
```

### Build Script Created

**Script**: `~/hobby/karabiner-fork/build-signed.sh`

**Usage**:
```bash
cd ~/hobby/karabiner-fork

# Build grabber only (no Apple Developer ID needed)
./build-signed.sh grabber

# Build full package with DriverKit (requires Apple Developer ID)
./build-signed.sh full
```

**Code Signing**:
- Uses existing self-signed certificate: `me@kuzmeech.com`
- Identity hash: `17F3838F28D599F5DA5E18580FF438735F1B4432`
- Located in keychain, shared with Language Switcher

**Limitation**:
- DriverKit System Extension requires Apple Developer ID ($99/year)
- Grabber-only build works but doesn't include debug logging
- Debug logging is in DriverKit VirtualHIDKeyboard (requires full build)

**Workaround**: Use monitoring script instead (see below)

---

## Monitoring Solution

### Script Created: `/Users/max/bin/monitor-karabiner-freeze`

**What it does**:
- Monitors USB timeout rate every 30 seconds
- Monitors Karabiner grabber errors
- Creates snapshot when error rate > 5/30s
- Saves to `/Volumes/ramdisk/karabiner-freeze-snapshots/`

**Snapshot includes**:
- USB error count
- Karabiner process status
- System extension status
- Recent error logs
- IOKit HID device state
- USB device tree

**Usage**:
```bash
monitor-karabiner-freeze
# Runs in foreground, Ctrl+C to stop
# Creates snapshots automatically when errors detected
```

---

## Immediate Actions

### For Next Freeze

1. **Don't quit Karabiner immediately**
2. Check if you can run commands (Terminal via mouse)
3. Run diagnostics:
   ```bash
   # Create manual snapshot
   date >> /tmp/freeze-time.txt
   ps aux | grep karabiner > /tmp/freeze-ps.txt
   ioreg -c IOHIDDevice -r -l | grep -i karabiner > /tmp/freeze-ioreg.txt
   ```
4. **Then** quit Karabiner to restore keyboard

### Monitor for Patterns

Run monitor script in background:
```bash
# Terminal 1: Monitor
monitor-karabiner-freeze

# Terminal 2: Check snapshots when freeze happens
ls -lt /Volumes/ramdisk/karabiner-freeze-snapshots/
cat /Volumes/ramdisk/karabiner-freeze-snapshots/freeze-*.log
```

---

## Potential Fixes (Not Implemented Yet)

### Short-term: Reduce USB Load

**Apply Razer Control polling patch** (see `~/hobby/razer-control/REDUCE-POLLING-PATCH.md`):
- Change DPI polling from 2s → 5s
- Change idle detection from 2s → 5s
- **Impact**: 60% reduction in USB requests (60/min → 24/min)

### Medium-term: Fix Razer Mouse

- Try different USB port
- Replace USB cable
- Update firmware (if available)
- Or: Replace with different mouse (Logitech, etc.)

### Long-term: Karabiner Fix

**Option A**: Report to Karabiner-Elements GitHub
- Provide logs from freeze snapshots
- Propose fix: Add timeout to `handleReport()` call
- Or: Use async event queue instead of synchronous

**Option B**: Build custom Karabiner with fix
- Requires Apple Developer ID ($99/year)
- Add timeout and retry logic
- Add event queue with overflow handling

**Option C**: Alternative to Karabiner
- Hammerspoon (Lua-based, no DriverKit)
- BetterTouchTool (different architecture)
- macOS built-in remapping (limited)

---

## Files Created

### In Fork
- `~/hobby/karabiner-fork/ARCHITECTURE.md` - Complete architecture documentation
- `~/hobby/karabiner-fork/FREEZE-INVESTIGATION-SUMMARY.md` - This file
- `~/hobby/karabiner-fork/vendor/.../org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard.cpp` - Modified with debug logging

### Monitoring Tools
- `/Users/max/bin/monitor-karabiner-freeze` - Freeze monitoring script

### Previous Documentation
- `~/hobby/mac/bgc/KEYBOARD-FREEZE-DIAGNOSIS.md` - Initial diagnosis
- `~/hobby/razer-control/REDUCE-POLLING-PATCH.md` - Razer USB reduction patch

---

## Next Steps

### Immediate (This Week)

1. ✅ Run `monitor-karabiner-freeze` in Terminal session
2. ✅ Document next freeze event with snapshots
3. ⏳ Apply Razer Control polling patch
4. ⏳ Monitor freeze frequency over 3-5 days

### If Freezes Continue

1. Disconnect Razer mouse temporarily
2. Test if freezes stop → confirms USB trigger
3. If stops: Replace Razer with different mouse
4. If continues: Investigate other triggers

### Long-term

1. File detailed GitHub issue on Karabiner-Elements
2. Include all snapshots and logs
3. Propose fix with code (from fork)
4. Monitor for official fix or workaround

---

## Useful Commands

```bash
# Check current Karabiner status
ps aux | grep karabiner | grep -v grep
systemextensionsctl list | grep -i karabiner

# Check USB errors
log show --last 5m --predicate 'eventMessage CONTAINS "USB" AND eventMessage CONTAINS "timeout"' 2>&1 | grep -c timeout

# Check Karabiner errors
log show --last 5m --predicate 'process CONTAINS "karabiner_grabber" AND eventMessage CONTAINS "e00002d6"' 2>&1 | grep -c e00002d6

# Manual snapshot
/Users/max/bin/monitor-karabiner-freeze

# View snapshots
ls -lt /Volumes/ramdisk/karabiner-freeze-snapshots/
```

---

**Status**: Investigation complete, monitoring in place
**Next Freeze**: Capture snapshot before quitting Karabiner
**Rebuild**: Pending Apple Developer ID or alternative approach
