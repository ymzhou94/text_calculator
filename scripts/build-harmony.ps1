param(
    [string]$DevEcoSdkHome = "C:\Program Files\Huawei\DevEco Studio\sdk",
    [string]$DevEcoToolsHome = "C:\Program Files\Huawei\DevEco Studio\tools",
    [string]$HvigorUserHome = "$env:USERPROFILE\.hvigor_textcalc"
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$HarmonyRoot = Join-Path $Root "harmony"

$env:DEVECO_SDK_HOME = $DevEcoSdkHome
$env:HVIGOR_USER_HOME = $HvigorUserHome
$env:PATH = (Join-Path $DevEcoToolsHome "node") + ";" + $env:PATH

$Ohpm = Join-Path $DevEcoToolsHome "ohpm\bin\ohpm.bat"
$Hvigor = Join-Path $DevEcoToolsHome "hvigor\bin\hvigorw.bat"

Push-Location $HarmonyRoot
try {
    & $Ohpm install
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $Hvigor assembleHap --mode module -p module=entry@default -p product=default
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}

Write-Host "HAP: $HarmonyRoot\entry\build\default\outputs\default\entry-default-unsigned.hap"
