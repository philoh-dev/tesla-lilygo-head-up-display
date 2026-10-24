@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\flash.ps1" %*
exit /b %ERRORLEVEL%
