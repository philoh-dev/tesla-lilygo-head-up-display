@echo off
setlocal

if not "%~2"=="" goto usage
where idf.py >nul 2>&1
if errorlevel 1 goto idf_missing

set "PORT_OPTION="
if not "%~1"=="" set PORT_OPTION=-p "%~1"

pushd "%~dp0firmware"
if errorlevel 1 goto firmware_missing

echo Building and flashing DevKitC N32R16V memory-only bring-up...
echo Leave the display disconnected. A dark screen is expected. BLE is disabled.
call idf.py -B build-newhaven-memory %PORT_OPTION% -DIDF_TARGET=esp32s3 -DTESLA_HEADUP_BOARD_PROFILE=devkitc_n32r16v -DSDKCONFIG=sdkconfig.devkitc_n32r16v.memory -DTESLA_HEADUP_DASHBOARD_LAYOUT=minimal -DTESLA_HEADUP_FIRMWARE_MODE=board_bring_up build flash monitor
set "RESULT=%ERRORLEVEL%"

popd
exit /b %RESULT%

:usage
echo Usage: %~nx0 [COM3]
exit /b 2

:idf_missing
echo ERROR: idf.py is not available. Run this script from an ESP-IDF 5.4.4 command prompt.
exit /b 1

:firmware_missing
echo ERROR: The firmware directory could not be opened.
exit /b 1
