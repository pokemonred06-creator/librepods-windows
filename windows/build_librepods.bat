@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
set PATH=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64;C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x64;C:\Users\RED\.gemini\antigravity\scratch;%PATH%
set LIB=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.28000.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.28000.0\um\x64;%LIB%
set INCLUDE=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.28000.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.28000.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.28000.0\shared;%INCLUDE%

if not exist "%~dp0build" mkdir "%~dp0build"
cd /d "%~dp0build"

echo === Running CMake Configure ===
cmake -G Ninja -DCMAKE_PREFIX_PATH="C:/Users/RED/.gemini/antigravity/scratch/Qt/6.8.0/msvc2022_64" ..
if %errorlevel% neq 0 exit /b %errorlevel%

echo === Running CMake Build ===
cmake --build .
if %errorlevel% neq 0 exit /b %errorlevel%

echo === Deploying Qt Dependencies ===
"C:\Users\RED\.gemini\antigravity\scratch\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe" --qmldir .. librepods-windows.exe

echo === Bundling Driver Assets ===
if not exist "driver" mkdir driver
copy /y "..\driver\*" "driver\" >nul
echo Driver assets bundled to build\driver\
