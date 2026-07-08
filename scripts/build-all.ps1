$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "build-mingw.ps1")
& (Join-Path $PSScriptRoot "build-android.ps1")
& (Join-Path $PSScriptRoot "build-harmony.ps1")
