@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo   LibrePods / NTPods - AirPods AAP Bluetooth Driver Setup
echo =======================================================
echo.

:: Check for administrative rights
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Administrator privileges are required.
    echo Please right-click this script and select 'Run as administrator'.
    pause
    exit /b 1
)

cd /d "%~dp0"

echo [1/3] Installing Driver Certificate into Trusted Root Store...
if exist "MagicAAP.cer" (
    certutil -addstore -f "Root" "MagicAAP.cer" >nul
    certutil -addstore -f "TrustedPublisher" "MagicAAP.cer" >nul
    echo       Certificate installed successfully.
) else (
    echo       [WARNING] MagicAAP.cer not found. Skipping certificate import.
)

echo [2/3] Adding Driver Package to Windows Driver Store via pnputil...
pnputil /add-driver MagicAAP.inf /install
if %errorlevel% neq 0 (
    echo       [WARNING] pnputil returned code %errorlevel%.
) else (
    echo       Driver package staged successfully.
)

echo [3/3] Checking Windows Test Mode Status...
for /f "tokens=2*" %%a in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control" /v SystemStartOptions 2^>nul ^| findstr /i "SystemStartOptions"') do set BOOT_OPTIONS=%%b

echo Current Boot Options: !BOOT_OPTIONS!
echo !BOOT_OPTIONS! | findstr /i "TESTSIGNING" >nul
if %errorlevel% equ 0 (
    echo [OK] Windows Test Mode (TESTSIGNING) is currently ENABLED.
    echo      The driver is active and ready for LibrePods.
) else (
    echo.
    echo [IMPORTANT] Windows 64-bit Kernel Driver Signature Enforcement is ACTIVE.
    echo Unofficial community Bluetooth drivers require Test Mode to load.
    echo.
    echo Would you like to enable Test Mode now? (Requires reboot and Secure Boot disabled)
    set /p ENABLE_TEST="Enable Test Mode? (Y/N): "
    if /i "!ENABLE_TEST!"=="Y" (
        bcdedit /set testsigning on
        echo.
        echo [DONE] Test Mode enabled. Please restart your PC for changes to take effect.
        echo Note: Ensure 'Secure Boot' is disabled in your UEFI/BIOS settings.
    ) else (
        echo.
        echo Driver staged, but it will only load after Test Mode is enabled:
        echo Run: bcdedit /set testsigning on (in Admin CMD) and reboot.
    )
)

echo.
pause
