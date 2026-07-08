@echo off
setlocal

set "BUILD_DIR=%~1"
if "%BUILD_DIR%"=="" set "BUILD_DIR=build-mingw"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-mingw.ps1" -BuildDir "%BUILD_DIR%"
exit /b %ERRORLEVEL%
