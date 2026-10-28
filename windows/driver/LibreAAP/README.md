# LibreAAP — Open-Source Windows Bluetooth Driver for Apple AirPods

`LibreAAP` is a clean, 100% open-source Kernel-Mode Driver Framework (KMDF 1.15) bridge driver that enables raw Bluetooth L2CAP communication with Apple AirPods on Windows.

It replaces the closed-source, third-party `MagicAAP.sys` binary with fully auditable, MIT-licensed code derived directly from Microsoft's official Windows Driver Kit (WDK) sample.

---

## 1. Provenance & License
- **Base Codebase**: [Microsoft Windows Driver Samples — `bluetooth/bthecho`](https://github.com/microsoft/Windows-driver-samples/tree/main/bluetooth/bthecho)
- **License**: MIT License (Microsoft Corporation)
- **Subsystem**: Native Windows KMDF Driver (`SERVICE_KERNEL_DRIVER`)

---

## 2. Directory Structure

```text
LibreAAP/
├── LibreAAP.inf          ; PnP installation INF matching BTHENUM\{74ec2172-0bad-4d01-8f77-997b2be0722a}
├── LibreAAP.inx          ; Driver template source
├── LibreAAP.vcxproj      ; Visual Studio 2022 + WDK MSBuild project
├── build_driver.cmd      ; Automated compilation and catalog generation script
├── inc/
│   ├── client.h          ; Client context and device structures
│   ├── clisrv.h          ; Common client/server definitions
│   ├── connection.h      ; Bluetooth L2CAP connection state machine
│   ├── device.h          ; WDFDEVICE context and callbacks
│   ├── driver.h          ; DriverEntry and initialization
│   ├── public.h          ; GUID_DEVINTERFACE_MAGICAAP ({74ec2172-0bad-4d01-8f77-997b2be0722a})
│   ├── queue.h           ; EvtIoRead / EvtIoWrite queue handler definitions
│   └── trace.h           ; WPP software tracing macros
└── src/
    ├── client.c          ; L2CAP client initialization and teardown
    ├── clisrv.c          ; Common connection utilities
    ├── connection.c      ; BRB_L2CA_ACL_TRANSFER submission routines
    ├── device.c          ; EvtDriverDeviceAdd and device interface publishing
    ├── driver.c          ; DriverEntry and WDF driver creation
    └── queue.c           ; Win32 ReadFile / WriteFile dispatch to L2CAP BRBs
```

---

## 3. How It Works
1. When AirPods pair to Windows, Windows enumerates the Apple Accessory Protocol (AAP) endpoint as:
   ```
   BTHENUM\{74ec2172-0bad-4d01-8f77-997b2be0722a}
   ```
2. `LibreAAP.inf` matches this hardware ID and loads `LibreAAP.sys`.
3. In `device.c`, the driver registers the device interface:
   ```c
   DEFINE_GUID(GUID_DEVINTERFACE_MAGICAAP, 0x74ec2172, 0x0bad, 0x4d01, 0x8f, 0x77, 0x99, 0x7b, 0x2b, 0xe0, 0x72, 0x2a);
   ```
4. User-space applications (such as **LibrePods**) open a handle via `CreateFileW` and issue standard asynchronous `ReadFile()` and `WriteFile()` calls.
5. In `queue.c`, `EvtIoWrite` and `EvtIoRead` package the buffers into Bluetooth Request Blocks (`BRB_L2CA_ACL_TRANSFER`) and submit them to `bthport.sys`.

---

## 4. How to Compile

### Requirements:
1. Visual Studio 2022 (Community or Professional) with the **Desktop development with C++** workload.
2. [Windows Driver Kit (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk) for Windows 10/11.

### Build Steps:
1. Run `build_driver.cmd` or open `LibreAAP.vcxproj` in Visual Studio 2022.
2. Select **Release** and **x64**.
3. Build Solution (`Ctrl + Shift + B`).
4. The output will be generated in `x64\Release\LibreAAP.sys`.

---

## 5. Signing & Running Without Windows Test Mode

### A. Development (Test Mode)
To test locally without an EV certificate:
```cmd
bcdedit /set testsigning on
makecert -r -pe -ss PrivateCertStore -n "CN=LibreAAP" LibreAAP.cer
signtool sign /v /s PrivateCertStore /n "LibreAAP" /t http://timestamp.digicert.com LibreAAP.sys
```

### B. Production (Retail Windows Without Test Mode)
To distribute `LibreAAP.sys` so standard users can run it on retail Windows without enabling Test Mode:
1. Create a [Microsoft Hardware Developer Center](https://developer.microsoft.com/en-us/dashboard/hardware) account.
2. Package `LibreAAP.sys` + `LibreAAP.inf` + `LibreAAP.cat` into a `.cab` archive.
3. Submit the archive for **WHQL Driver Attestation Signing**.
4. Microsoft automatically returns a production-signed driver package valid for all retail Windows 10 and 11 installations.
