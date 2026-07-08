param(
    [string]$GradleVersion = "9.1.0"
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Deps = Join-Path $Root "build\deps"
$GradleDir = Join-Path $Deps "gradle-$GradleVersion"
$GradleZip = Join-Path $Deps "gradle-$GradleVersion-bin.zip"
$GradleBat = Join-Path $GradleDir "bin\gradle.bat"

New-Item -ItemType Directory -Force -Path $Deps | Out-Null
if (-not (Test-Path $GradleBat)) {
    if (-not (Test-Path $GradleZip)) {
        Invoke-WebRequest -Uri "https://services.gradle.org/distributions/gradle-$GradleVersion-bin.zip" -OutFile $GradleZip
    }
    Expand-Archive -LiteralPath $GradleZip -DestinationPath $Deps -Force
}

& $GradleBat -p (Join-Path $Root "android") :app:assembleDebug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "APK: $Root\android\app\build\outputs\apk\debug\app-debug.apk"
