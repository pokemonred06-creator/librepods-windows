# Implementation Plan - Absorb LibrePods Linux Updates into Windows

We can selectively port and absorb the valuable features, bug fixes, and improvements from upstream LibrePods (Linux) into your Windows version while keeping all Windows-specific functionality intact (MagicAAP driver integration, overlapped asynchronous I/O, AirPods Pro 2 USB-C model detection, and Windows audio control).

## Proposed Updates from Linux to Absorb

1. **Conversational Awareness Protocol Fix**:
   - **Upstream improvement**: Decodes all state packets (`0x01` voice started, `0x08` feature disabled, `0x09` feature enabled, `0x06`/default voice ended) so volume is dynamically ducked to 20% when you speak and restored when you stop speaking, without falsely setting the feature to "disabled".
   - **Target**: `windows/media/mediacontroller.cpp`.

2. **Robust Single-Instance IPC (`QLocalServer`)**:
   - **Upstream improvement**: Fixes the stale `QSharedMemory` bug where terminating the process left orphaned memory blocks, causing false "Already Running" errors and premature exits.
   - **Target**: `windows/main.cpp`.

3. **IPC CLI Remote Control & `librepods-ctl.exe` Tool**:
   - **Upstream improvement**: Extends the local IPC socket server to receive remote commands (`noise:off`, `noise:anc`, `noise:transparency`, `noise:adaptive`) and builds a lightweight CLI tool (`librepods-ctl.exe`).
   - **Benefits for Windows**: You can toggle ANC/Transparency via PowerShell, Windows Terminal, keyboard shortcuts, or Stream Deck/AutoHotkey without opening the GUI.
   - **Target**: `windows/main.cpp`, `windows/librepods-ctl.cpp`, `windows/CMakeLists.txt`.

4. **Preserved Windows Capabilities**:
   - `MagicAAP.sys` direct kernel driver interface over L2CAP PSM `0x1001`.
   - Asynchronous overlapped I/O avoiding GUI thread deadlocks.
   - Model identification for `A3063`, `A3064`, `A3065`, and case `A2968`.
   - Silicone-tip iconography and custom device title header.

---

## Proposed Changes

### Core Logic & IPC

#### [MODIFY] [mediacontroller.cpp](file:///C:/Users/RED/.gemini/antigravity/scratch/librepods-tblob/windows/media/mediacontroller.cpp)
- Update `handleConversationalAwareness(const QByteArray &data)` to match upstream's switch-case on `data[9]` (`0x01`, `0x08`, `0x09`, default).
- Ensure Windows audio volume returns to `initialVolume` smoothly once voice has ended.

#### [MODIFY] [main.cpp](file:///C:/Users/RED/.gemini/antigravity/scratch/librepods-tblob/windows/main.cpp)
- Clean up single-instance startup logic using `QLocalServer::removeServer("app_server")` before listening.
- Add handlers in `readyRead` for IPC commands:
  - `noise:off` -> `trayApp->setNoiseControlModeInt(0)`
  - `noise:anc` -> `trayApp->setNoiseControlModeInt(1)`
  - `noise:transparency` -> `trayApp->setNoiseControlModeInt(2)`
  - `noise:adaptive` -> `trayApp->setNoiseControlModeInt(3)`

#### [NEW] [librepods-ctl.cpp](file:///C:/Users/RED/.gemini/antigravity/scratch/librepods-tblob/windows/librepods-ctl.cpp)
- Port the lightweight CLI utility that sends command arguments to `app_server` over `QLocalSocket`.

#### [MODIFY] [CMakeLists.txt](file:///C:/Users/RED/.gemini/antigravity/scratch/librepods-tblob/windows/CMakeLists.txt)
- Add executable target for `librepods-ctl.exe` linked with `Qt6::Core` and `Qt6::Network`.

---

## Verification Plan

### Automated Build & Compilation
- Run `build_librepods.bat` to build both `librepods-windows.exe` and `librepods-ctl.exe`.
- Verify compilation succeeds with 0 errors.

### Manual Verification
1. Launch `librepods-windows.exe` and confirm:
   - Device and model name `Red’s AirPods Pro` / `AirPods Pro 2 (USB-C) (A3063)` display properly.
   - Mode switching works via GUI pill.
2. Test CLI IPC tool:
   - Run `librepods-ctl.exe noise:anc` -> verify AirPods switch physically to Noise Cancellation.
   - Run `librepods-ctl.exe noise:transparency` -> verify AirPods switch physically to Transparency.
   - Run `librepods-ctl.exe noise:off` -> verify AirPods switch physically to Off.
3. Test Single-Instance behavior:
   - Running `librepods-windows.exe` a second time brings the existing window to the front without error.
