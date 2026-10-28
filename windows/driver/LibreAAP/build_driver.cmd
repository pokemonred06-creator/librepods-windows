@echo off
setlocal EnableDelayedExpansion

echo =======================================================
echo          LibreAAP Open-Source Driver Builder
echo =======================================================

:: Locate VS MSBuild
set MSBUILD_CMD=""
if exist "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_CMD="C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_CMD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
)

if %MSBUILD_CMD%=="" (
    echo [ERROR] MSBuild.exe was not found. Please ensure Visual Studio 2022 with C++ is installed.
    exit /b 1
)

echo [*] Found MSBuild at %MSBUILD_CMD%
echo [*] Checking for Windows Driver Kit (WDK)...

:: Check for WDK headers
set WDK_FOUND=0
if exist "C:\Program Files (x86)\Windows Kits\10\Include\wdf" set WDK_FOUND=1
for /d %%D in ("C:\Program Files (x86)\Windows Kits\10\Include\10.*") do (
    if exist "%%D\km" set WDK_FOUND=1
)

if %WDK_FOUND%==0 (
    echo.
    echo [NOTE] Windows Driver Kit (WDK) is required to compile .sys kernel binaries.
    echo [NOTE] To install WDK:
    echo        1. Open Visual Studio Installer -> Modify -> Individual Components
    echo        2. Check "Windows Driver Kit" or download from:
    echo           https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
    echo.
    echo In the meantime, the pre-tested driver is ready to use in windows\driver\MagicAAP.sys.
    exit /b 0
)

echo [*] Building LibreAAP.sys (Release x64)...
cd /d "%~dp0"
%MSBUILD_CMD% LibreAAP.vcxproj /p:Configuration=Release /p:Platform=x64
if %errorlevel% neq 0 (
    echo [ERROR] Driver build failed.
    exit /b %errorlevel%
)

echo [*] Creating catalog file (Inf2Cat)...
for /d %%K in ("C:\Program Files (x86)\Windows Kits\10\bin\10.*") do (
    if exist "%%K\x64\Inf2Cat.exe" (
        "%%K\x64\Inf2Cat.exe" /driver:"%~dp0x64\Release" /os:10_X64,11_X64
    )
)

echo [*] Build complete! Output located at %~dp0x64\Release\LibreAAP.sys
