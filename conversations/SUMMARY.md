# LibrePod Project — Technical Summary & Evolution

This document provides a structured, high-level technical overview of all developments, problem solutions, and features added to **LibrePods for Windows** across 54 development turns.

---

## 1. Project Overview & Objectives
**LibrePods** is an open-source companion utility for Apple AirPods on Windows, designed to deliver native-like functionality that Windows does not provide out of the box:
- Battery status for left bud, right bud, case, and headset.
- Noise Control Mode switching (Off, Noise Cancellation, Transparency, Adaptive).
- Listening features: Conversational Awareness, Hearing Aid Mode, One Bud ANC Mode.
- Spatial Audio Head Tracking via AirPods 6-DOF IMU sensor telemetry.
- Seamless co-existence with mobile devices (iOS / Android) without Bluetooth connection hijacking.
- Robust state persistence across restarts and disconnections.

---

## 2. Key Architecture & Components

| Component | File / Location | Description |
| :--- | :--- | :--- |
| **Main App Logic** | `windows/main.cpp` | Core application loop, Win32 MagicAAP driver handling, L2CAP communication, state synchronization. |
| **AirPods Packets** | `windows/airpods_packets.h` | Reverse-engineered Apple AAP (AirPods Accessory Protocol) binary packet formats and parsers. |
| **Spatial Audio** | `windows/spatialaudiotracker.hpp` | Decodes 6-DOF IMU telemetry (Euler angles/quaternions), broadcasts via FreePIE UDP 4242 and OSC 9000. |
| **Device Model** | `windows/deviceinfo.hpp` | QObject-backed state model exposed to QML UI. |
| **Battery Management** | `windows/battery.hpp` | Parses BLE advertisement packets and L2CAP battery telemetry. |
| **QML User Interface** | `windows/Main.qml` | Fluid, responsive dark-themed Qt Quick GUI with auto-scaling and zero horizontal scroll. |
| **Tray Management** | `windows/trayiconmanager.cpp` | System tray icon, notifications, and quick action context menu. |
| **Media Controller** | `windows/media/MediaController.cpp` | Windows media control and A2DP profile activation. |

---

## 3. Major Development Milestones & Problems Solved

### Milestone 1: Initial Setup, Fork & Build Pipeline
- **Problem**: LibrePods Windows original repo was unmaintained and missing automated build and deployment steps on modern toolchains.
- **Solution**:
  - Configured CMake + Ninja + Qt 6.8 (MSVC 2022 x64).
  - Created standalone automated build script `build_librepods.bat`.
  - Configured `windeployqt` for self-contained runtime deployment.

### Milestone 2: MagicAAP Driver Integration & Bluetooth L2CAP
- **Problem**: Windows native Bluetooth stack blocks raw L2CAP access to Apple AirPods accessory endpoints without jailbreaking or specialized drivers.
- **Solution**:
  - Integrated `MagicAAP` Win32 driver interface (`GUID_DEVINTERFACE_MAGICAAP` & `GUID_DEVINTERFACE_AAP_SERVER`).
  - Implemented asynchronous overlapped I/O (`CreateFileW`, `ReadFile`, `WriteFile`) for raw AAP communication.
  - Added non-blocking reader thread for bi-directional packet processing.

### Milestone 3: Reverse-Engineered AirPods Packet Protocols
- **Packets implemented**:
  - Handshake: `00 00 04 00 01 00 02 00 ...`
  - Feature Discovery: `SET_SPECIFIC_FEATURES`, `REQUEST_NOTIFICATIONS`
  - Noise Control Modes: Off, ANC, Transparency, Adaptive
  - Conversational Awareness: `04 00 04 00 09 00 28 01/02 ...`
  - One Bud ANC Mode: `04 00 04 00 09 00 1b 01/02 ...`
  - Hearing Aid Mode: `04 00 04 00 09 00 2d 01/02 ...`
  - Magic Cloud Pairing Keys extraction (`magicAccIRK`, `magicAccEncKey`).

### Milestone 4: Spatial Audio & Real-Time Head Tracking
- **Problem**: Spatial audio head tracking was not available on Windows.
- **Solution**:
  - Reverse-engineered AAP spatial audio IMU sensor telemetry frames.
  - Decoded 16-bit pitch, roll, and yaw values using quaternion-to-Euler conversion.
  - Implemented automatic zero-point drift calibration and user 'Recenter' action.
  - Streamed telemetry over FreePIE UDP port `4242` and OSC port `9000` for integration with spatial audio renderers and head-tracking plugins.
  - Guarded against streaming watchdog loops when disconnected to avoid socket write errors.

### Milestone 5: Audio Quality & Windows Sonic Alignment
- **Problem**: Understanding hardware limitations of AirPods on Windows and spatial audio options.
- **Solution**:
  - Aligned settings with Windows Sonic for Headphones in Windows Sound Control Panel.
  - Documented maximum sample rate/bit depth support (AAC / SBC Bluetooth A2DP stereo, 16-bit/24-bit 48kHz).

### Milestone 6: UI Redesign, Responsive Scaling & Geometry Persistence
- **Problem**: Window width changes caused components to clip or required horizontal scrolling; window dimensions and position were lost on restart.
- **Solution**:
  - Redesigned `Main.qml` with a modern, unified dark theme (`#0f131c` / `#161b26`).
  - Added responsive auto-scaling (`Math.min(1.0, availableWidth / baseWidth)`) so all elements scale cleanly without horizontal scroll.
  - Implemented persistent window geometry in registry (`window/x`, `window/y`, `window/width`, `window/height`).

### Milestone 7: Anti-Hijacking Bluetooth Connection Logic
- **Problem**: When user transferred AirPods connection to their phone, LibrePods on PC aggressively tried to reconnect, hijacking the AirPods back.
- **Solution**:
  - Replaced aggressive reconnection loops with passive Win32 device status checks (`IsAirPodsConnectedWin32()`).
  - Implemented graceful handle release (`closeMagicAap()`) when Windows reports the device is disconnected, allowing phone connections to proceed without interference.

### Milestone 8: Multi-Device Settings Sync & One-Bud ANC Persistence
- **Problem**: Moving between phone and PC caused AirPods to lose user preferences (especially One Bud ANC mode, Noise Control mode, and Conversational Awareness).
- **Solution**:
  - Fixed UI switch binding in `Main.qml` which was previously setting `deviceInfo.oneBudANCMode = checked` instead of calling `airPodsTrayApp.setOneBudANCMode(checked)`.
  - Added One Bud ANC toggle to the main dashboard's "LISTENING FEATURES" card for quick access.
  - Removed early return deadlock (`if (m_deviceInfo->oneBudANCMode() == enabled) return;`) that prevented sending packets to newly reconnected hardware.
  - Added mismatch re-assertion in `parseData`: if AirPods report a state from the phone that differs from the user's saved PC preference, LibrePods automatically detects the difference and re-applies the saved preference.
  - Introduced `m_initialSyncInProgress` synchronization window to prevent incoming hardware notifications from erasing stored preferences.

### Milestone 9: Windows Driver Reverse Engineering & REA Feasibility Analysis
- **Goal**: Reverse engineer the proprietary MagicAAP.sys driver to determine if firmware or drivers can be decompiled/modified without third-party test-signing dependencies.
- **Analysis & Findings**:
  - Binary and symbol analysis revealed MagicAAP.sys is not proprietary Apple firmware, but a direct build of Microsoft's open-source Windows Driver Kit (WDK) sample: bthecho/bthcli (Bluetooth L2CAP client).
  - PDB debug path: C:\Users\steam\Local Storage\Repos\steam3d\MagicAAP\MagicAAP\x64\Release\MagicAAP.pdb.
  - The driver contains **zero Apple proprietary protocol logic**. It operates solely as an L2CAP bridge (BRB_L2CA_ACL_TRANSFER), allowing user-space applications to exchange raw Bluetooth packets via standard Win32 ReadFile / WriteFile.
  - Detailed forensic report created at [docs/DRIVER_ANALYSIS.md](file:///C:/Users/RED/.gemini/antigravity/scratch/librepod/docs/DRIVER_ANALYSIS.md).

### Milestone 10: Clean-Room Open-Source Driver (LibreAAP) Integration
- **Goal**: Provide full, open-source C source code for an independent driver replacement (LibreAAP) to eliminate reliance on pre-compiled binary drivers and enable Microsoft WHQL attestation signing.
- **Implementation**:
  - Integrated 16 C source and header files into [windows/driver/LibreAAP/](file:///C:/Users/RED/.gemini/antigravity/scratch/librepod/windows/driver/LibreAAP/).
  - Authored custom INF (LibreAAP.inf) matching AirPods Hardware ID BTHENUM\{74ec2172-0bad-4d01-8f77-997b2be0722a} and custom Device Interface GUID {74ec2172-0bad-4d01-8f77-997b2be0722a}.
  - Provided automated build script build_driver.cmd and developer documentation README.md.
  - Documented WHQL Attestation Signing path to eliminate Windows Test Signing mode requirements.

### Milestone 11: Windows Media Playback Control & Disconnection Auto-Pause
- **Problem**: When disconnecting AirPods (switching to phone, placing in case, or Bluetooth disconnect) or removing earbuds, media playback continued playing out loud through computer speakers.
- **Root Cause**: Windows media playback control was stubbed out in the original port (`LOG_DEBUG("Media playback pause not fully implemented on Windows yet")`), `PlayerStatusWatcher` was a D-Bus no-op returning empty strings, and `onDeviceDisconnected` never triggered a media pause event.
- **Solution**:
  - Implemented WASAPI active audio session detection (`IAudioSessionManager2` / `IAudioSessionEnumerator`) in [`WindowsAudioController::isAudioPlaying()`](file:///C:/Users/RED/.gemini/antigravity/scratch/librepod/windows/media/windowsaudiocontroller.cpp) to accurately detect when apps (Spotify, YouTube, VLC, browser) are actively rendering audio.
  - Implemented native Windows media playback control via `VK_MEDIA_PLAY_PAUSE` keystroke simulation (`SendInput`) in [`MediaController::pause()`](file:///C:/Users/RED/.gemini/antigravity/scratch/librepod/windows/media/mediacontroller.cpp) and `play()`.
  - Added disconnect event hooks in [`onDeviceDisconnected()`](file:///C:/Users/RED/.gemini/antigravity/scratch/librepod/windows/main.cpp) and [`closeMagicAap()`](file:///C:/Users/RED/.gemini/antigravity/scratch/librepod/windows/main.cpp) so that when AirPods disconnect while media is playing, playback is paused instantly.
  - Connected `handleEarDetection()` so removing earbuds pauses playback and putting them back in resumes playback if previously paused by LibrePods.

---

## 4. Building and Running

### Application (librepods-windows.exe)

```cmd
:: Build the project
cd C:\Users\RED\.gemini\antigravity\scratch\librepod\windows\build
C:\Users\RED\.gemini\antigravity\brain\19b45e6e-470b-43c2-8833-5b1376eb3a89\scratch\build_librepods.bat

:: Run LibrePods
.\librepods-windows.exe
```

---

## 5. Artifacts and References
- Complete Conversation Transcript: `conversations/CONVERSATION_HISTORY.md`
- Raw Log Chunks: `conversations/transcript.jsonl` & `conversations/transcript_full.jsonl`
- Implementation Plan: `conversations/artifacts/implementation_plan.md`
- App Logos: `conversations/artifacts/`
