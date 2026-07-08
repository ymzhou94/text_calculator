@echo off
setlocal

set "BUILD_DIR=%~1"
if "%BUILD_DIR%"=="" set "BUILD_DIR=build-msvc"

set "ARCH=%~2"
if "%ARCH%"=="" set "ARCH=x64"

set "GENERATOR=%~3"
if "%GENERATOR%"=="" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-msvc.ps1" -BuildDir "%BUILD_DIR%" -Architecture "%ARCH%"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-msvc.ps1" -BuildDir "%BUILD_DIR%" -Architecture "%ARCH%" -Generator "%GENERATOR%"
)
exit /b %ERRORLEVEL%
