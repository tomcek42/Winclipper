<#
.SYNOPSIS
Creates the signed update manifest for a release (winclipper-update.txt + .minisig).

.DESCRIPTION
Hashes the two MSIs, writes the manifest that Winclipper's updater downloads from
the latest GitHub release, and signs it with minisign (Ed25519, the same scheme the
Tauri updater uses). The secret key is a password-protected minisign key file kept
by the project owner outside the repository; it is not bound to a Windows profile,
so a backup copy works on any machine. minisign asks for the key password.
Upload both output files as release assets next to the MSIs.

.EXAMPLE
pwsh tools/sign-update.ps1 -Version 1.6.0.0 -MsiDir dist
#>
param(
    [Parameter(Mandatory)][ValidatePattern('^\d+\.\d+\.\d+\.\d+$')][string]$Version,
    [string]$MsiDir = 'dist',
    [string]$OutDir = 'dist',
    [string]$KeyFile = "$env:USERPROFILE\.winclipper\winclipper-update.key"
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot
$publicKeyFile = Join-Path $PSScriptRoot 'winclipper-update.pub'

if (-not (Get-Command minisign -ErrorAction SilentlyContinue)) {
    throw 'minisign not found. Install it with: winget install jedisct1.minisign'
}

# The EXE embeds the public key; signing with a key it does not know would ship
# a release that no installed copy accepts.
$publicKey = (Get-Content $publicKeyFile)[1].Trim()
if (-not (Select-String -Quiet -SimpleMatch "`"$publicKey`"" (Join-Path $repo 'Winclipper\Updater.cpp'))) {
    throw "Public key in $publicKeyFile does not match SIGNING_PUBLIC_KEY in Winclipper\Updater.cpp."
}

$tag = 'v' + ($Version -replace '\.0$', '')
$base = "https://github.com/tomcek42/Winclipper/releases/download/$tag"

$lines = @('winclipper-update 1', "version $Version")
foreach ($arch in 'x64', 'x86') {
    $msi = Join-Path $MsiDir "WinclipperSetup-$arch.msi"
    $hash = (Get-FileHash -Algorithm SHA256 $msi).Hash.ToLowerInvariant()
    $lines += "$arch $hash $base/WinclipperSetup-$arch.msi"
}
$manifest = Join-Path $OutDir 'winclipper-update.txt'
[IO.File]::WriteAllBytes($manifest, [Text.Encoding]::ASCII.GetBytes(($lines -join "`n") + "`n"))

# In CI the password comes from MINISIGN_PASSWORD; minisign reads it from stdin.
if ($env:MINISIGN_PASSWORD) {
    $env:MINISIGN_PASSWORD | minisign -S -s $KeyFile -m $manifest -t "winclipper-update $Version"
} else {
    minisign -S -s $KeyFile -m $manifest -t "winclipper-update $Version"
}
if ($LASTEXITCODE -ne 0) { throw 'Signing failed.' }
minisign -V -p $publicKeyFile -m $manifest
if ($LASTEXITCODE -ne 0) { throw 'Signature self-check failed.' }

Get-Content $manifest
