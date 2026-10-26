<#
.SYNOPSIS
  Downloads one small CC0 instrument (Glockenspiel) exactly as the application's library installer
  would, verifies every file against the manifest's SHA-256 with realtone-cli, and renders the demo
  MIDI file through it. Proves the real sample path on Windows.

  pwsh packaging\check-real-library.ps1 -Cli path\to\realtone-cli.exe
#>
param(
    [Parameter(Mandatory = $true)][string]$Cli,
    [string]$InstrumentId = "vsco2ce.glockenspiel"
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$manifestPath = Join-Path $root "libraries\manifests\vsco2ce.json"
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$inst = $manifest.instruments | Where-Object { $_.id -eq $InstrumentId }
if (-not $inst) { throw "Instrument $InstrumentId is not in the manifest." }

$lib = Join-Path ([IO.Path]::GetTempPath()) ("rt-lib-" + [guid]::NewGuid().ToString("N"))
$packDir = Join-Path $lib $manifest.id
New-Item -ItemType Directory -Force -Path $packDir | Out-Null

$base = $manifest.source.baseUrl
$count = 0
foreach ($rel in $inst.files) {
    $dest = Join-Path $packDir ($rel -replace "/", "\")
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    $url = $base + (($rel -split "/" | ForEach-Object { [uri]::EscapeDataString($_) }) -join "/")
    Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
    $count++
}
Write-Host "Downloaded $count files for $InstrumentId"

& $Cli verify --manifest $manifestPath --library $lib --instrument $InstrumentId
if ($LASTEXITCODE -ne 0) { throw "Checksum verification failed." }

$sfz = Join-Path $packDir $inst.sfz
$out = Join-Path $lib "demo.wav"
& $Cli render --instrument $sfz --midi (Join-Path $root "examples\demo_phrase.mid") --out $out --tracks 1
if ($LASTEXITCODE -ne 0) { throw "Rendering failed." }
$size = (Get-Item $out).Length
if ($size -lt 100000) { throw "Rendered file is suspiciously small ($size bytes)." }
Write-Host "Rendered $out ($size bytes)" -ForegroundColor Green

# A damaged file must be detected.
$first = Join-Path $packDir (($inst.files | Where-Object { $_ -like "*.wav" } | Select-Object -First 1) -replace "/", "\")
$bytes = [IO.File]::ReadAllBytes($first); $bytes[100] = $bytes[100] -bxor 0xFF; [IO.File]::WriteAllBytes($first, $bytes)
& $Cli verify --manifest $manifestPath --library $lib --instrument $InstrumentId
if ($LASTEXITCODE -eq 0) { throw "A corrupted sample was not detected." }
Write-Host "Corruption detected as expected." -ForegroundColor Green
Remove-Item $lib -Recurse -Force
