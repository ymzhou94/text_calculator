param(
    [string]$BuildDir = "build-mingw"
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildPath = Join-Path $Root $BuildDir

cmake -S $Root -B $BuildPath -G "MinGW Makefiles"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $BuildPath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir $BuildPath --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "EXE: $BuildPath\text_calculator.exe"
