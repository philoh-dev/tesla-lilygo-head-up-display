@echo off
setlocal

where idf.py >nul 2>&1
if errorlevel 1 goto idf_missing

set "PORT_OPTION="
if not "%~1"=="" set "PORT_OPTION=-p %~1"

pushd "%~dp0firmware"
if errorlevel 1 goto firmware_missing

echo Building and flashing the mirrored Tesla Head-Up Display display self-test...
idf.py -B build-display-self-test %PORT_OPTION% -DIDF_TARGET=esp32s3 -DSDKCONFIG=sdkconfig.display-self-test -DTESLA_HEADUP_DASHBOARD_LAYOUT=road_scene -DTESLA_HEADUP_FIRMWARE_MODE=display_self_test build flash monitor
set "RESULT=%ERRORLEVEL%"

popd
exit /b %RESULT%

:idf_missing
echo ERROR: idf.py is not available. Run this script from an ESP-IDF command prompt.
exit /b 1

:firmware_missing
echo ERROR: The firmware directory could not be opened.
exit /b 1
