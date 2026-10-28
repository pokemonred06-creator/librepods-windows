# Walkthrough - Upstream Updates & Windows Autostart Configuration

We have checked the latest upstream repositories, integrated the newest model mappings (AirPods 5 & AirPods 5 with Wireless Charging Case), rebuilt LibrePods, and configured it to automatically run on Windows startup in tray mode (`--hide`).

---

## 🚀 What Was Updated & Integrated

### 1. New Model Mappings from Upstream ([enums.h](file:///C:/Users/RED/.gemini/antigravity/scratch/librepods-tblob/windows/enums.h))
- Upstream added support for Apple's upcoming hardware entries:
  - **AirPods 5**: `A3531`, `A3532`, `A3533`
  - **AirPods 5 (Wireless Charging Case)**: `A3439`, `A3440`, `A3441`
- Integrated these identifiers into `enums.h` with display names, model mapping, and iconography.

### 2. Windows Startup Autostart Configuration
LibrePods is now configured to start automatically on Windows boot via two complementary mechanisms:
1. **Windows Registry Run Entry**:
   - **Path**: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
   - **Key**: `librepods-windows`
   - **Command**: `"C:\Users\RED\.gemini\antigravity\scratch\librepods-tblob\windows\build\librepods-windows.exe" --hide`
2. **Startup Folder Shortcut**:
   - **Location**: `C:\Users\RED\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup\LibrePods.lnk`
   - **Arguments**: `--hide` (launches minimized to the system tray without opening the main window).

### 3. Native Path Separator Fix ([autostartmanager.hpp](file:///C:/Users/RED/.gemini/antigravity/scratch/librepods-tblob/windows/autostartmanager.hpp))
- Updated `createAutoStartEntry()` to always convert path separators to native Windows backslashes (`\`) using `QDir::toNativeSeparators()` and wrap paths in double quotes, ensuring Windows Explorer executes it cleanly on login.

---

## 🔍 Verification & Test Results

1. **Rebuild Status**:
   - `librepods-windows.exe` and `librepods-ctl.exe` compiled cleanly with 0 errors.
   - Hardlink `librepods.exe` created in the build folder.
2. **Autostart Verification**:
   - Verified Registry entry: `"C:\Users\RED\.gemini\antigravity\scratch\librepods-tblob\windows\build\librepods-windows.exe" --hide`.
   - Verified Startup shortcut: `LibrePods.lnk` in `shell:startup` pointing to `librepods-windows.exe` with `--hide`.
3. **Driver & Hardware Communication**:
   - Active communication verified with your AirPods Pro (`A3063`):
     - Transparency & Noise Cancellation mode packets successfully written over `MagicAAP`.
     - Live Conversational Awareness state received.
     - CLI remote control via `librepods-ctl.exe noise:anc` verified.
