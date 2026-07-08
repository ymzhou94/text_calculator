param(
    [string]$BuildDir = "build-msvc",
    [string]$Architecture = "x64",
    [string]$Generator = ""
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildPath = Join-Path $Root $BuildDir

if ([string]::IsNullOrWhiteSpace($Generator)) {
    $VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $VsWhere) {
        $Version = & $VsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationVersion
        if ($Version -match "^18\.") {
            $Generator = "Visual Studio 18 2026"
        } elseif ($Version -match "^17\.") {
            $Generator = "Visual Studio 17 2022"
        }
    }
}

if ([string]::IsNullOrWhiteSpace($Generator)) {
    Write-Error "MSVC Build Tools were not found. Pass -Generator explicitly after installing Visual Studio Build Tools."
}

cmake -S $Root -B $BuildPath -G $Generator -A $Architecture
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $BuildPath --config Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir $BuildPath -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "EXE: $BuildPath\Release\text_calculator.exe"
