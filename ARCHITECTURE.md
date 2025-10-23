# Karabiner-Elements Architecture Overview

**Fork Location**: `~/hobby/karabiner-fork`
**Upstream**: https://github.com/pqrs-org/Karabiner-Elements
**Private Fork**: https://github.com/kuzmeech/karabiner-fork

---

## Purpose of This Fork

Investigate keyboard freeze issue where Karabiner Virtual Keyboard deadlocks, causing both USB and Bluetooth keyboards to stop working. The deadlock appears to be triggered by USB congestion from Razer mouse timeouts.

**Related Documentation**:
- `~/hobby/mac/bgc/KEYBOARD-FREEZE-DIAGNOSIS.md` - Complete keyboard freeze diagnosis
- `~/hobby/razer-control/REDUCE-POLLING-PATCH.md` - Razer Control polling reduction patch

---

## Core Architecture

### Process Flow

```
Physical Keyboard (USB/Bluetooth)
         ↓
   IOHIDDevice (macOS kernel)
         ↓
karabiner_grabber (root privileges)
   - Seizes input devices via IOKit
   - Applies key remapping rules
   - Posts to VirtualHIDDevice
         ↓
Karabiner-DriverKit-VirtualHIDDevice (kernel extension)
   - Receives HID reports
   - Creates virtual keyboard
         ↓
Virtual HID Keyboard (IOHIDDevice)
         ↓
   macOS System (receives events)
```

### Three Core Processes

1. **`karabiner_grabber`** (Root privilege, LaunchDaemon)
   - **Location**: `src/core/grabber/`
   - **Purpose**: Seizes physical keyboards using IOKit, modifies events, posts to Virtual HID
   - **Socket**: Provides Unix domain socket for `karabiner_console_user_server`
   - **Key Code**: `src/core/grabber/include/grabber/device_grabber.hpp`

2. **`karabiner_session_monitor`** (Root privilege, LaunchAgent)
   - **Location**: `src/core/session_monitor/`
   - **Purpose**: Detects console user changes (important for Screen Sharing scenarios)
   - **API**: Uses `CGSessionCopyCurrentDictionary` (most reliable method)
   - **Notifies**: `karabiner_grabber` of current console user

3. **`karabiner_console_user_server`** (User privilege, LaunchAgent)
   - **Location**: `src/core/console_user_server/`
   - **Purpose**: Connects to grabber, enables event processing, executes shell commands
   - **Notifies**: Active application, input source to grabber for filtering

### Critical Component: VirtualHIDDevice DriverKit

**Location**: `vendor/Karabiner-DriverKit-VirtualHIDDevice/`
**Separate Repo**: https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice
**Version**: 1.8.0 (as of 2025-10-18)

**Key File**: `src/DriverKit/Karabiner-DriverKit-VirtualHIDDevice/org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard.cpp`

#### VirtualHIDKeyboard Structure

```cpp
struct org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard_IVars {
  org_pqrs_Karabiner_DriverKit_VirtualHIDDeviceUserClient* provider;
  bool ready;
  uint8_t lastLedState;
};
```

**Critical Methods**:
- `handleStart()` - Initializes virtual keyboard, sets `ivars->ready = true`
- `postReport()` - Posts HID report to virtual keyboard (line 336-352)
- `reset()` - Clears all pressed keys (line 354-392)
- `setReport()` - Handles LED state (caps lock, num lock) feedback (line 272-334)

#### Report Descriptor

The keyboard uses **7 report types** (Report IDs 1-7):
1. **Report ID 1**: Standard keyboard (modifiers + 32 keys)
2. **Report ID 2**: Consumer controls (media keys)
3. **Report ID 3**: Apple Vendor TopCase
4. **Report ID 4**: Apple Vendor Keyboard
5. **Report ID 5**: LED output (caps/num lock)
6. **Report ID 6**: LED input (feedback from macOS)
7. **Report ID 7**: Generic Desktop controls

**Key Constraints**:
- Report Count: 32 keys maximum per report (line 38, 53, etc.)
- Usage Maximum: Limited to 255 to avoid high CPU usage on macOS Monterey (line 16-17)

---

## Potential Deadlock Points

Based on keyboard freeze diagnosis, the deadlock likely occurs in:

### 1. Event Queue Overflow

**Hypothesis**: USB congestion → delayed IOHIDDevice events → DriverKit queue overflow

**Relevant Code**:
- `vendor/Karabiner-DriverKit-VirtualHIDDevice/src/DriverKit/.../org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard.cpp:336-352`
- `postReport()` calls `handleReport()` which is inherited from IOHIDDevice

**Debug Approach**:
- Add logging to `postReport()` to measure queue depth
- Track time between event arrival and `handleReport()` completion
- Log when `kIOReturnSuccess` is NOT returned

### 2. DriverKit Watchdog Timeout

**Hypothesis**: If `handleReport()` takes too long, DriverKit may freeze the device

**Evidence**: macOS DriverKit has strict timing requirements for kernel extensions

**Debug Approach**:
- Add `mach_absolute_time()` before/after `handleReport()` (already used at line 347)
- Log events that take > 10ms to process
- Check for correlation with USB timeout timestamps

### 3. LED State Deadlock

**Hypothesis**: `setReport()` (caps lock LED feedback) might block on USB congestion

**Relevant Code**:
```cpp
kern_return_t VirtualHIDKeyboard::setReport(IOMemoryDescriptor* report, ...) {
  // Line 272-334
  // Posts LED report back via postReport()
  postReport(memory);  // Line 329 - could block?
}
```

**Debug Approach**:
- Add logging when `setReport()` is called
- Track if `setReport()` happens during USB timeout bursts
- Measure time spent in `report->Map()` (line 283)

---

## Build Process

### Requirements

- macOS 15+ (Sequoia or newer)
- Xcode 26+ (specified in README, though Xcode 26 doesn't exist yet - likely meant Xcode 16)
- Command Line Tools: `xcode-select --install`
- Homebrew packages:
  ```bash
  brew install xz xcodegen cmake
  ```

### Build Steps

```bash
cd ~/hobby/karabiner-fork

# Already done: git submodule update --init --recursive

# Build entire project
make

# Or build specific components for faster iteration:

# Build and install grabber only
cd src/core/grabber
make install

# Build and install console_user_server only
cd src/core/console_user_server
make install

# Build and install session_monitor only
cd src/core/session_monitor
make install
```

### Code Signing (Optional)

If you have a Developer ID:

```bash
# Find your identity
security find-identity -p codesigning -v | grep 'Developer ID Application'

# Set environment variable (example)
export KARABINER_CODESIGN_IDENTITY="8D660191481C98F5C56630847A6C39D95C166F22"
```

---

## Debug Instrumentation Plan

### Phase 1: Add Logging to VirtualHIDKeyboard

**File**: `vendor/Karabiner-DriverKit-VirtualHIDDevice/src/DriverKit/.../org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard.cpp`

**Add to `postReport()`**:
```cpp
kern_return_t IMPL(org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard, postReport) {
  uint64_t start_time = mach_absolute_time();

  if (!report) {
    os_log(OS_LOG_DEFAULT, LOG_PREFIX " postReport: null report");
    return kIOReturnBadArgument;
  }

  uint64_t reportLength;
  auto kr = report->GetLength(&reportLength);
  if (kr != kIOReturnSuccess) {
    os_log(OS_LOG_DEFAULT, LOG_PREFIX " postReport: GetLength failed: 0x%x", kr);
    return kr;
  }

  kr = handleReport(mach_absolute_time(),
                    report,
                    static_cast<uint32_t>(reportLength),
                    kIOHIDReportTypeInput,
                    0);

  uint64_t end_time = mach_absolute_time();
  uint64_t duration_ns = end_time - start_time;

  // Log slow events (> 5ms = 5,000,000 ns)
  if (duration_ns > 5000000) {
    os_log(OS_LOG_DEFAULT, LOG_PREFIX " postReport SLOW: %llu ns, length: %llu, kr: 0x%x",
           duration_ns, reportLength, kr);
  }

  if (kr != kIOReturnSuccess) {
    os_log(OS_LOG_DEFAULT, LOG_PREFIX " postReport: handleReport FAILED: 0x%x", kr);
  }

  return kr;
}
```

**Add to `setReport()`**:
```cpp
kern_return_t org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard::setReport(...) {
  os_log(OS_LOG_DEFAULT, LOG_PREFIX " setReport: LED update");

  // ... existing code ...

  // Before line 329
  os_log(OS_LOG_DEFAULT, LOG_PREFIX " setReport: posting LED report (state: 0x%x)", state);
  postReport(memory);
  os_log(OS_LOG_DEFAULT, LOG_PREFIX " setReport: posted LED report");

  // ... rest of code ...
}
```

### Phase 2: Check Logs After Freeze

After keyboard freeze incident:

```bash
# Show DriverKit logs during freeze (adjust time)
log show --last 5m --predicate 'process CONTAINS "Karabiner" AND subsystem CONTAINS "DriverKit"' --style compact

# Show slow postReport events
log show --last 5m --predicate 'eventMessage CONTAINS "postReport SLOW"' --style compact

# Show handleReport failures
log show --last 5m --predicate 'eventMessage CONTAINS "handleReport FAILED"' --style compact

# Correlate with USB timeouts
log show --last 5m --predicate '(eventMessage CONTAINS "Karabiner" OR eventMessage CONTAINS "USB") AND eventMessage CONTAINS "timeout"' --style compact
```

### Phase 3: Check IOKit Service State

When keyboard is frozen:

```bash
# Check if Virtual Keyboard is still registered
ioreg -c IOHIDDevice -r -l | grep -i "karabiner" -A 20

# Check grabber process
ps aux | grep karabiner_grabber

# Check if grabber is stuck
sudo lsof -p $(pgrep karabiner_grabber) | grep -i usb

# Check DriverKit extension status
systemextensionsctl list | grep -i karabiner
```

---

## Testing Approach

### 1. Monitor USB Load

Before and after adding debug logging:

```bash
# Terminal 1: Monitor USB timeouts
watch -n 1 'log show --last 30s --predicate "eventMessage CONTAINS \"USB\" AND eventMessage CONTAINS \"timeout\"" 2>&1 | grep -c timeout'

# Terminal 2: Monitor Karabiner logs
log stream --predicate 'process CONTAINS "karabiner"' --style compact

# Terminal 3: Type on keyboard
# Watch for correlation between USB timeouts and slow postReport events
```

### 2. Reproduce Freeze

**Trigger Conditions**:
- Razer mouse plugged in (generates USB timeouts)
- Razer Control running (adds USB polling)
- Extended typing session
- Display sleep/wake cycles

**Monitor**:
```bash
# Run before freeze
~/bin/fix-keyboard  # Shows current USB error rate

# When keyboard freezes:
# 1. Note exact time
# 2. DO NOT quit Karabiner yet
# 3. Run diagnostics:
ioreg -c IOHIDDevice -r -l | grep -i "karabiner" > /tmp/karabiner-frozen-state.txt
log show --last 2m --predicate 'process CONTAINS "karabiner"' > /tmp/karabiner-freeze-logs.txt

# 4. Then quit Karabiner to restore keyboard
```

---

## Hypotheses to Test

### Hypothesis 1: Event Queue Exhaustion

**Test**: Add counter to track queued events in `postReport()`

**Expected**: If queue fills up, `handleReport()` returns error and Virtual Keyboard stops accepting events

**Fix**: Increase queue size or add queue overflow recovery

### Hypothesis 2: USB Transaction Timeout Propagation

**Test**: Check if USB timeout from Razer mouse affects IOKit's global USB transaction queue

**Expected**: If macOS IOKit shares USB transaction context, Razer timeouts might delay all USB/HID events

**Fix**: This would require macOS kernel patch (unlikely) or hardware fix (replace mouse)

### Hypothesis 3: DriverKit Resource Exhaustion

**Test**: Monitor memory/CPU of DriverKit extension during high USB load

```bash
# Monitor DriverKit extension resources
sudo dtruss -p $(pgrep karabiner_grabber) 2>&1 | grep -i "mach_msg\|IOKit"
```

**Expected**: If DriverKit runs out of mach ports or memory, it might deadlock

**Fix**: Add resource limits, periodic reset, or queue flush

---

## Next Steps

1. **Add debug logging to VirtualHIDKeyboard.cpp** (Phase 1 above)
2. **Build instrumented version**: `cd ~/hobby/karabiner-fork && make`
3. **Install custom build**: Follow DEVELOPMENT.md replacement instructions
4. **Monitor for next freeze**: Collect logs as specified in Phase 2
5. **Analyze correlation**: USB timeout timestamps vs. postReport slow events
6. **File GitHub issue**: With detailed logs and analysis
7. **Propose fix**: Based on findings (queue size, timeout handling, etc.)

---

## Useful Commands

```bash
# Check current Karabiner version
/Applications/Karabiner-Elements.app/Contents/MacOS/Karabiner-Elements --version

# Check running processes
ps aux | grep karabiner | grep -v grep

# Check LaunchDaemon/Agent status
launchctl print system/org.pqrs.service.daemon.karabiner_grabber
launchctl print gui/$(id -u)/org.pqrs.service.agent.karabiner_console_user_server

# Restart grabber after rebuild
sudo launchctl kickstart -k system/org.pqrs.service.daemon.karabiner_grabber

# View DriverKit logs live
log stream --predicate 'process CONTAINS "karabiner"' --style compact
```

---

## Files of Interest

### DriverKit Virtual HID
- `vendor/Karabiner-DriverKit-VirtualHIDDevice/src/DriverKit/.../org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard.cpp` - Main virtual keyboard implementation
- `vendor/Karabiner-DriverKit-VirtualHIDDevice/include/pqrs/karabiner/driverkit/virtual_hid_device_driver.hpp` - HID report structures

### Grabber
- `src/core/grabber/include/grabber/device_grabber.hpp` - Device seizure logic
- `src/core/grabber/` - Main event processing loop

### Build System
- `Makefile` - Top-level build
- `make-package.sh` - Creates distributable .dmg

---

**Last Updated**: 2025-10-18
**Status**: Fork created, ready for debug instrumentation
