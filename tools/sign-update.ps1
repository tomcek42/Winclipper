<#
.SYNOPSIS
Creates the signed update manifest for a release (winclipper-update.txt + .sig).

.DESCRIPTION
Hashes the two MSIs, writes the manifest that Winclipper's updater downloads from
the latest GitHub release, and signs it with the release signing key (ECDSA P-256,
IEEE P1363 signature). The private key is DPAPI-protected for the project owner at
%USERPROFILE%\.winclipper\update-signing-key.dpapi and never enters the repository.
Upload both output files as release assets next to the MSIs.

.EXAMPLE
pwsh tools/sign-update.ps1 -Version 1.6.0.0 -MsiDir dist
#>
param(
    [Parameter(Mandatory)][ValidatePattern('^\d+\.\d+\.\d+\.\d+$')][string]$Version,
    [string]$MsiDir = 'dist',
    [string]$OutDir = 'dist',
    [string]$KeyFile = "$env:USERPROFILE\.winclipper\update-signing-key.dpapi"
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Security

$tag = 'v' + ($Version -replace '\.0$', '')
$base = "https://github.com/tomcek42/Winclipper/releases/download/$tag"

$lines = @('winclipper-update 1', "version $Version")
foreach ($arch in 'x64', 'x86') {
    $msi = Join-Path $MsiDir "WinclipperSetup-$arch.msi"
    $hash = (Get-FileHash -Algorithm SHA256 $msi).Hash.ToLowerInvariant()
    $lines += "$arch $hash $base/WinclipperSetup-$arch.msi"
}
$manifest = [Text.Encoding]::ASCII.GetBytes(($lines -join "`n") + "`n")

$pkcs8 = [Security.Cryptography.ProtectedData]::Unprotect(
    [IO.File]::ReadAllBytes($KeyFile), $null, 'CurrentUser')
$key = [Security.Cryptography.ECDsa]::Create()
try {
    $key.ImportPkcs8PrivateKey($pkcs8, [ref]$null)
    $signature = $key.SignData($manifest, [Security.Cryptography.HashAlgorithmName]::SHA256,
        [Security.Cryptography.DSASignatureFormat]::IeeeP1363FixedFieldConcatenation)
    if (-not $key.VerifyData($manifest, $signature, [Security.Cryptography.HashAlgorithmName]::SHA256,
        [Security.Cryptography.DSASignatureFormat]::IeeeP1363FixedFieldConcatenation)) {
        throw 'Signature self-check failed.'
    }
} finally {
    [Array]::Clear($pkcs8, 0, $pkcs8.Length)
    $key.Dispose()
}

[IO.File]::WriteAllBytes((Join-Path $OutDir 'winclipper-update.txt'), $manifest)
[IO.File]::WriteAllBytes((Join-Path $OutDir 'winclipper-update.sig'), $signature)
Write-Output ([Text.Encoding]::ASCII.GetString($manifest))
