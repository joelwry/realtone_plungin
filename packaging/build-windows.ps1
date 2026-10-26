<#
.SYNOPSIS
  Builds RealTone Studio for Windows x64 with Visual Studio 2022, runs the tests and produces the
  installer, the portable zip and SHA-256 checksums in <repo>\dist.

.EXAMPLE
  pwsh packaging\build-windows.ps1 -Version 0.1.0

Requirements: Visual Studio 2022 with the "Desktop development with C++" workload, CMake 3.22+,
Git, and NSIS 3 (makensis) for the installer. JUCE is fetched by CMake at the pinned tag unless
-JuceDir points at an existing checkout.
#>
param(
    [string]$Version = "0.1.0",
    [string]$Config = "Release",
    [string]$BuildDir = "build-win",
    [string]$JuceDir = "",
    [switch]$SkipTests,
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$build = Join-Path $root $BuildDir
$dist = Join-Path $root "dist"
$stage = Join-Path $dist "stage"

function Step($text) { Write-Host "`n=== $text" -ForegroundColor Cyan }
function Run($exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe failed with exit code $LASTEXITCODE" }
}

Step "Configure"
$cfg = @("-S", $root, "-B", $build, "-G", "Visual Studio 17 2022", "-A", "x64", "-DREALTONE_BUILD_TESTS=ON")
if ($JuceDir) { $cfg += "-DREALTONE_JUCE_DIR=$JuceDir" }
Run cmake $cfg

Step "Build ($Config)"
Run cmake @("--build", $build, "--config", $Config, "--parallel")

if (-not $SkipTests) {
    Step "Core tests"
    Run (Join-Path $build "tests\$Config\realtone_tests.exe") @()
    Step "Studio tests (processor, state, presets, editor layout, standalone workflows)"
    $env:REALTONE_TEST_OUTPUT = Join-Path $dist "test-output"
    Run (Join-Path $build "tests_studio\realtone_studio_tests_artefacts\$Config\realtone_studio_tests.exe") @()
}

Step "Stage"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path "$stage\app\docs", "$stage\app\examples", "$stage\vst3" | Out-Null

$appExe = Join-Path $build "app\RealToneStudio_artefacts\$Config\RealTone Studio.exe"
$cliExe = Join-Path $build "cli\$Config\realtone-cli.exe"
$vst3 = Join-Path $build "plugin\RealToneInstrument_artefacts\$Config\VST3\RealTone Studio.vst3"
foreach ($p in @($appExe, $cliExe, $vst3)) { if (-not (Test-Path $p)) { throw "Missing build output: $p" } }

Copy-Item $appExe "$stage\app\"
Copy-Item $cliExe "$stage\app\"
Copy-Item (Join-Path $root "packaging\README-installed.txt") "$stage\app\README.txt"
Copy-Item (Join-Path $root "CHANGELOG.md") "$stage\app\"
Copy-Item (Join-Path $root "THIRD_PARTY_NOTICES.txt") "$stage\app\"
Copy-Item (Join-Path $root "docs\USER_GUIDE.md"), (Join-Path $root "docs\INSTALL_AND_TROUBLESHOOTING.md"), (Join-Path $root "docs\LIBRARIES.md") "$stage\app\docs\"
Copy-Item (Join-Path $root "examples\demo_phrase.mid") "$stage\app\examples\"

# Only the files that make up the plug-in go in: the DLL and the module manifest.
$bundle = "$stage\vst3\RealTone Studio.vst3"
New-Item -ItemType Directory -Force -Path "$bundle\Contents\x86_64-win", "$bundle\Contents\Resources" | Out-Null
Copy-Item "$vst3\Contents\x86_64-win\RealTone Studio.vst3" "$bundle\Contents\x86_64-win\"
Copy-Item "$vst3\Contents\Resources\moduleinfo.json" "$bundle\Contents\Resources\"

# Code signing runs only when a certificate is supplied (SIGN_PFX_BASE64 and SIGN_PFX_PASSWORD).
if ($env:SIGN_PFX_BASE64 -and $env:SIGN_PFX_PASSWORD) {
    Step "Sign binaries"
    $pfx = Join-Path $dist "sign.pfx"
    [IO.File]::WriteAllBytes($pfx, [Convert]::FromBase64String($env:SIGN_PFX_BASE64))
    $signtool = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" | Sort-Object FullName | Select-Object -Last 1).FullName
    foreach ($f in @("$stage\app\RealTone Studio.exe", "$stage\app\realtone-cli.exe", "$bundle\Contents\x86_64-win\RealTone Studio.vst3")) {
        Run $signtool @("sign", "/f", $pfx, "/p", $env:SIGN_PFX_PASSWORD, "/fd", "SHA256", "/tr", "http://timestamp.digicert.com", "/td", "SHA256", $f)
    }
    $signed = $true
} else {
    Write-Host "No signing certificate supplied: binaries are NOT code signed." -ForegroundColor Yellow
    $signed = $false
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
$setup = Join-Path $dist "RealToneStudio-$Version-win64-setup.exe"
$zip = Join-Path $dist "RealToneStudio-$Version-win64-portable.zip"

if (-not $SkipInstaller) {
    Step "Installer"
    $makensis = (Get-Command makensis -ErrorAction SilentlyContinue)
    if ($makensis) { $makensis = $makensis.Source } else { $makensis = "C:\Program Files (x86)\NSIS\makensis.exe" }
    if (-not (Test-Path $makensis)) { throw "NSIS (makensis) not found. Install it from https://nsis.sourceforge.io or with 'choco install nsis'." }
    Run $makensis @("/V2", "/DVERSION=$Version", "/DSTAGE=$stage", "/DOUTFILE=$setup", (Join-Path $root "packaging\installer.nsi"))
    if ($signed) {
        Run $signtool @("sign", "/f", (Join-Path $dist "sign.pfx"), "/fd", "SHA256", $setup)
    }
}

if ($signed) { Remove-Item (Join-Path $dist "sign.pfx") -Force -ErrorAction SilentlyContinue }

Step "Portable zip"
$portable = Join-Path $dist "portable"
if (Test-Path $portable) { Remove-Item $portable -Recurse -Force }
New-Item -ItemType Directory -Force -Path "$portable\RealToneStudio" | Out-Null
Copy-Item "$stage\app\*" "$portable\RealToneStudio\" -Recurse
Copy-Item "$stage\vst3\RealTone Studio.vst3" "$portable\RealToneStudio\" -Recurse
Copy-Item (Join-Path $root "packaging\README-portable.txt") "$portable\RealToneStudio\README-PORTABLE.txt"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path "$portable\RealToneStudio" -DestinationPath $zip

Step "Checksums"
$sums = @()
foreach ($f in @($setup, $zip)) {
    if (Test-Path $f) {
        $h = (Get-FileHash $f -Algorithm SHA256).Hash.ToLower()
        $sums += "$h  $(Split-Path $f -Leaf)"
    }
}
$sums | Set-Content -Encoding ascii (Join-Path $dist "SHA256SUMS.txt")
Get-Content (Join-Path $dist "SHA256SUMS.txt")
Write-Host "`nDone. Artifacts are in $dist" -ForegroundColor Green
