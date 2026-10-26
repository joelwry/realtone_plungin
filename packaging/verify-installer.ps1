<#
.SYNOPSIS
  Installs the installer silently, checks what it put where, validates the installed VST3 with
  pluginval, checks the window behaviour of the installed application, uninstalls it, and checks that
  nothing but the program's own files was removed. Needs administrator rights (the installer is per-machine).

  pwsh packaging\verify-installer.ps1 -Setup dist\RealToneStudio-0.1.0-win64-setup.exe -Pluginval C:\tools\pluginval.exe
#>
param(
    [Parameter(Mandatory = $true)][string]$Setup,
    [string]$Pluginval = "",
    [string]$ExpectedVersion = ""
)

$ErrorActionPreference = "Stop"
$failures = @()
function Check($name, $ok, $detail = "") {
    if ($ok) { Write-Host ("  PASS  {0} {1}" -f $name, $detail) }
    else { Write-Host ("  FAIL  {0} {1}" -f $name, $detail) -ForegroundColor Red; $script:failures += $name }
}
function Step($t) { Write-Host "`n=== $t" -ForegroundColor Cyan }

$setup = (Resolve-Path $Setup).Path
$progDir = Join-Path $env:ProgramFiles "RealTone Studio"
$vstRoot = Join-Path ${env:CommonProgramFiles} "VST3"
$bundle = Join-Path $vstRoot "RealTone Studio.vst3"
$dll = Join-Path $bundle "Contents\x86_64-win\RealTone Studio.vst3"
$userData = Join-Path $env:LOCALAPPDATA "RealToneStudio"
$uninstKey = "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\RealToneStudio"

Step "Prepare unrelated files that must survive"
New-Item -ItemType Directory -Force -Path (Join-Path $vstRoot "Other Vendor.vst3\Contents") | Out-Null
$otherFile = Join-Path $vstRoot "Other Vendor.vst3\Contents\marker.txt"
"not ours" | Set-Content $otherFile
$otherHash = (Get-FileHash $otherFile).Hash
New-Item -ItemType Directory -Force -Path (Join-Path $userData "Libraries\vsco2ce"), (Join-Path $userData "Presets") | Out-Null
$sampleStandIn = Join-Path $userData "Libraries\vsco2ce\Harp.sfz"
"<region> sample=x.wav" | Set-Content $sampleStandIn
$presetFile = Join-Path $userData "Presets\Mine.rtpreset"
"<RealTonePreset/>" | Set-Content $presetFile

Step "Install silently"
$p = Start-Process -FilePath $setup -ArgumentList "/S" -Wait -PassThru
Check "installer exit code 0" ($p.ExitCode -eq 0) "(exit $($p.ExitCode))"
Check "application installed" (Test-Path (Join-Path $progDir "RealTone Studio.exe"))
Check "command line tool installed" (Test-Path (Join-Path $progDir "realtone-cli.exe"))
Check "licence notices installed" (Test-Path (Join-Path $progDir "THIRD_PARTY_NOTICES.txt"))
Check "user guide installed" (Test-Path (Join-Path $progDir "docs\USER_GUIDE.md"))
Check "VST3 installed in the standard folder" (Test-Path $dll) $dll
Check "VST3 module manifest installed" (Test-Path (Join-Path $bundle "Contents\Resources\moduleinfo.json"))
Check "uninstall entry registered" (Test-Path $uninstKey)
if (Test-Path $uninstKey) {
    $ver = (Get-ItemProperty $uninstKey).DisplayVersion
    if ($ExpectedVersion) { Check "registered version" ($ver -eq $ExpectedVersion) "($ver)" }
}
Check "start menu shortcut" (Test-Path (Join-Path $env:ProgramData "Microsoft\Windows\Start Menu\Programs\RealTone Studio\RealTone Studio.lnk"))
Check "unrelated plug-in untouched" ((Test-Path $otherFile) -and ((Get-FileHash $otherFile).Hash -eq $otherHash))

if ($Pluginval) {
    Step "pluginval on the installed VST3"
    # The Windows pluginval build is a GUI-subsystem program; read the verdict from its output.
    $out = (& $Pluginval --strictness-level 5 --skip-gui-tests --validate-in-process --timeout-ms 120000 $bundle 2>&1 | Out-String)
    Write-Host $out
    Check "pluginval strictness 5 (without GUI tests)" ($out -match "(?m)^SUCCESS\s*$")
}

Step "Installed application: native window behaviour"
& pwsh -NoProfile -File (Join-Path $PSScriptRoot "smoke-test-window.ps1") -Exe (Join-Path $progDir "RealTone Studio.exe")
Check "window behaviour checks" ($LASTEXITCODE -eq 0)

Step "Reinstall over the top"
$p = Start-Process -FilePath $setup -ArgumentList "/S" -Wait -PassThru
Check "second install succeeds" ($p.ExitCode -eq 0)

Step "Uninstall silently"
$p = Start-Process -FilePath (Join-Path $progDir "Uninstall.exe") -ArgumentList "/S", "_?=$progDir" -Wait -PassThru
Check "uninstaller exit code 0" ($p.ExitCode -eq 0) "(exit $($p.ExitCode))"
Remove-Item (Join-Path $progDir "Uninstall.exe") -Force -ErrorAction SilentlyContinue   # an in-place uninstaller cannot delete itself
if ((Test-Path $progDir) -and -not (Get-ChildItem $progDir -Force)) { Remove-Item $progDir -Force }
Check "application removed" (-not (Test-Path (Join-Path $progDir "RealTone Studio.exe")))
Check "program folder removed" (-not (Test-Path $progDir))
Check "VST3 binary removed" (-not (Test-Path $dll))
Check "VST3 folder removed" (-not (Test-Path $bundle))
Check "uninstall entry removed" (-not (Test-Path $uninstKey))
Check "unrelated plug-in still there" ((Test-Path $otherFile) -and ((Get-FileHash $otherFile).Hash -eq $otherHash))
Check "user's sample library untouched" (Test-Path $sampleStandIn)
Check "user's presets untouched" (Test-Path $presetFile)

Remove-Item (Join-Path $vstRoot "Other Vendor.vst3") -Recurse -Force -ErrorAction SilentlyContinue

if ($failures.Count -gt 0) {
    Write-Host ("`n{0} check(s) failed: {1}" -f $failures.Count, ($failures -join ", ")) -ForegroundColor Red
    exit 1
}
Write-Host "`nInstaller checks passed." -ForegroundColor Green
