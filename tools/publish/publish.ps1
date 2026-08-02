<#
.SYNOPSIS
    Sign a CustomMod tdesktop release and publish it to all mirrors.

.DESCRIPTION
    One command covers the whole release flow:
      1. Read the version from Telegram/SourceFiles/core/version.h
      2. Sign the staged build with Packer.exe
      3. Upload the package + manifest to the VPS (secure + pub) and to
         the private GitHub releases repo
      4. Re-download from every mirror and compare SHA-256 against the
         local file - an upload that "succeeded" but stored a
         truncated file is worse than no upload at all, because it
         silently blocks every client pointed at that mirror.
      5. Print a summary

    No encryption step: that layer was deliberately dropped (see
    docs/superpowers/plans/2026-08-01-self-update-plan.md, section
    3.2) - the AES key would ship in the same binary that decrypts
    with it, so it added attack surface without real protection.

.PARAMETER StagingDir
    Directory containing the built release files to package (the
    contents that should replace the running installation - typically
    a copy of out/Release with logs/tdata stripped out).

.PARAMETER SshHost
    SSH host alias for the VPS, as defined in your ~/.ssh/config.
    Must use key-based auth - this script never handles the VPS
    password. See "SSH KEY SETUP" below if not configured yet.

.PARAMETER GithubRepo
    owner/repo for the private releases mirror, e.g. Oybek-M/tdesktop-releases.

.PARAMETER DryRun
    Sign and build the manifest, but skip every upload. Use this to
    verify Packer + the key still work without touching any mirror.

.EXAMPLE
    .\publish.ps1 -StagingDir C:\release-staging -SshHost customsync-vps -GithubRepo Oybek-M/tdesktop-releases

.NOTES
    SSH KEY SETUP (one-time, not done by this script):
      1. ssh-keygen -t ed25519 -f $env:USERPROFILE\.ssh\customsync-release
      2. ssh-copy-id -i $env:USERPROFILE\.ssh\customsync-release.pub root@<vps-ip>
         (or manually append the .pub content to /root/.ssh/authorized_keys)
      3. Add to ~/.ssh/config:
             Host customsync-vps
                 HostName <vps-ip>
                 User root
                 IdentityFile ~/.ssh/customsync-release
    A dedicated key (not your everyday one) makes it easy to revoke
    release access later without touching anything else.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$StagingDir,

    [Parameter(Mandatory = $false)]
    [string]$SshHost = "customsync-vps",

    [Parameter(Mandatory = $false)]
    [string]$GithubRepo = "Oybek-M/tdesktop-releases",

    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$RepoRoot     = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$VersionFile  = Join-Path $RepoRoot "Telegram\SourceFiles\core\version.h"
$PackerExe    = Join-Path $RepoRoot "out\Release\Packer.exe"
$DesktopPrivateDir = Join-Path $RepoRoot "..\DesktopPrivate"
$PrivateKey   = Join-Path $DesktopPrivateDir "customsync-updates-private.pem"
$SecretsFile  = Join-Path $DesktopPrivateDir "vps-mirror-secrets.txt"
$WorkDir      = Join-Path $env:TEMP "customsync-release-$([guid]::NewGuid().ToString('N').Substring(0,8))"

function Fail($message) {
    Write-Host "RELEASE FAILED: $message" -ForegroundColor Red
    if (Test-Path $WorkDir) { Remove-Item -Recurse -Force $WorkDir -ErrorAction SilentlyContinue }
    exit 1
}

# --- Step 0: preflight -------------------------------------------------
#
# The VPS Basic-auth password and the pub-mirror's secret path segment
# are read from DesktopPrivate/vps-mirror-secrets.txt (outside this
# repo, never committed) rather than hardcoded here - this script gets
# committed to the public tdesktop fork, and the pub mirror's only
# protection IS that path segment being unguessable. Hardcoding it
# here would publish it to anyone who reads this file on GitHub.

if (-not (Test-Path $PrivateKey)) {
    Fail "Private key not found at $PrivateKey. Without it, Packer.exe cannot sign anything - check docs/self-update/key-management.md."
}
if (-not (Test-Path $PackerExe)) {
    Fail "Packer.exe not found at $PackerExe. Build it first: cmake --build out --target Packer --config Release"
}
if (-not (Test-Path $StagingDir)) {
    Fail "Staging directory not found: $StagingDir"
}
if (-not (Test-Path $SecretsFile)) {
    Fail "Secrets file not found at $SecretsFile (expected secure_user=, secure_pass=, pub_prefix= lines)."
}

$Secrets = @{}
Get-Content $SecretsFile | ForEach-Object {
    if ($_ -match '^(\w+)=(.*)$') {
        $Secrets[$Matches[1]] = $Matches[2]
    }
}
foreach ($key in @("secure_user", "secure_pass", "pub_prefix")) {
    if (-not $Secrets.ContainsKey($key)) {
        Fail "Secrets file is missing '$key='"
    }
}

New-Item -ItemType Directory -Path $WorkDir | Out-Null

# --- Step 1: version ------------------------------------------------------

$versionContent = Get-Content $VersionFile -Raw
if ($versionContent -notmatch 'constexpr auto AppVersion = (\d+);') {
    Fail "Could not parse AppVersion out of $VersionFile"
}
$version = [int]$Matches[1]
Write-Host "Version: $version" -ForegroundColor Cyan

# --- Step 2: sign with Packer ----------------------------------------------

Write-Host "Signing with Packer.exe..." -ForegroundColor Cyan
Push-Location $WorkDir
try {
    & $PackerExe -path $StagingDir -version $version -target win64
    if ($LASTEXITCODE -ne 0) {
        Fail "Packer.exe exited with code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

$packageName = "tx64upd$version"
$packagePath = Join-Path $WorkDir $packageName
if (-not (Test-Path $packagePath)) {
    Fail "Packer did not produce the expected file: $packageName"
}

$localHash = (Get-FileHash $packagePath -Algorithm SHA256).Hash
Write-Host "Package: $packageName ($((Get-Item $packagePath).Length) bytes, sha256 $localHash)" -ForegroundColor Cyan

# --- Step 3: build the manifest --------------------------------------------
# NOTE: "released" and "link" are sibling keys, and "link" must start
# with "/" - the client does prefix+link with no separator in between.
# Both details were only discovered by testing against the live client
# (see the Task 3 notes in the plan doc) - get either wrong and the
# client silently rejects or 404s the manifest.

$manifest = @{
    win64 = @{
        stable = @{
            released = $version
            link     = "/win/$packageName"
        }
    }
} | ConvertTo-Json -Compress -Depth 5

$manifestPath = Join-Path $WorkDir "current4"
Set-Content -Path $manifestPath -Value $manifest -NoNewline -Encoding utf8

Write-Host "Manifest: $manifest" -ForegroundColor Cyan

if ($DryRun) {
    Write-Host "DRY RUN - stopping before upload. Package and manifest are in $WorkDir" -ForegroundColor Yellow
    exit 0
}

# --- Step 4: upload to mirrors ----------------------------------------------

$results = @{}

function Upload-Vps($label, $remotePath) {
    Write-Host "Uploading to VPS ($label)..." -ForegroundColor Cyan
    try {
        & ssh $SshHost "mkdir -p $remotePath/win"
        if ($LASTEXITCODE -ne 0) { throw "ssh mkdir failed" }
        & scp $packagePath "${SshHost}:$remotePath/win/$packageName"
        if ($LASTEXITCODE -ne 0) { throw "scp package failed" }
        & scp $manifestPath "${SshHost}:$remotePath/current4"
        if ($LASTEXITCODE -ne 0) { throw "scp manifest failed" }
        $results[$label] = @{ ok = $true }
    } catch {
        Write-Host "  Warning: $label upload failed - $_" -ForegroundColor Yellow
        $results[$label] = @{ ok = $false; error = $_.ToString() }
    }
}

Upload-Vps "vps-secure" "/var/www/updates/secure"
Upload-Vps "vps-pub"    "/var/www/updates/$($Secrets.pub_prefix)"

Write-Host "Uploading to GitHub ($GithubRepo)..." -ForegroundColor Cyan
try {
    $ghWorkDir = Join-Path $WorkDir "gh-repo"
    & gh repo clone $GithubRepo $ghWorkDir -- --depth 1
    if ($LASTEXITCODE -ne 0) { throw "gh repo clone failed" }
    New-Item -ItemType Directory -Force -Path (Join-Path $ghWorkDir "win") | Out-Null
    Copy-Item $packagePath (Join-Path $ghWorkDir "win\$packageName")
    Copy-Item $manifestPath (Join-Path $ghWorkDir "current4")
    Push-Location $ghWorkDir
    try {
        & git add -A
        & git commit -m "release: version $version"
        if ($LASTEXITCODE -ne 0) { throw "git commit failed (maybe nothing changed?)" }
        & git push
        if ($LASTEXITCODE -ne 0) { throw "git push failed" }
        $results["github"] = @{ ok = $true }
    } finally {
        Pop-Location
    }
} catch {
    Write-Host "  Warning: GitHub upload failed - $_" -ForegroundColor Yellow
    $results["github"] = @{ ok = $false; error = $_.ToString() }
}

$anySucceeded = ($results.Values | Where-Object { $_.ok }).Count -gt 0
if (-not $anySucceeded) {
    Fail "Every mirror failed. No release was published."
}

# --- Step 5: re-download and verify checksums -------------------------------
# Mandatory, not optional: a mirror that reports success but silently
# stores a truncated file blocks updates for everyone pointed at it,
# and nobody finds out until someone reports the app never updating.

function Verify-Url($label, $url) {
    if (-not $results.ContainsKey($label) -or -not $results[$label].ok) {
        return  # already failed to upload, nothing to verify
    }
    Write-Host "Verifying $label ($url)..." -ForegroundColor Cyan
    $checkPath = Join-Path $WorkDir "verify-$label"
    try {
        Invoke-WebRequest -Uri $url -OutFile $checkPath -ErrorAction Stop
        $remoteHash = (Get-FileHash $checkPath -Algorithm SHA256).Hash
        if ($remoteHash -ne $localHash) {
            $results[$label] = @{ ok = $false; error = "checksum mismatch: local=$localHash remote=$remoteHash" }
            Write-Host "  MISMATCH - $label is serving a corrupted file" -ForegroundColor Red
        } else {
            Write-Host "  OK - checksum matches" -ForegroundColor Green
        }
    } catch {
        $results[$label] = @{ ok = $false; error = "re-download failed: $_" }
        Write-Host "  Re-download failed: $_" -ForegroundColor Red
    }
}

Verify-Url "vps-secure" "https://$($Secrets.secure_user):$($Secrets.secure_pass)@updates.2007.uz/secure/win/$packageName"
Verify-Url "vps-pub"    "https://updates.2007.uz/$($Secrets.pub_prefix)/win/$packageName"

# --- Step 6: summary ---------------------------------------------------

Write-Host ""
Write-Host "=== Release $version summary ===" -ForegroundColor Cyan
foreach ($label in $results.Keys) {
    $r = $results[$label]
    if ($r.ok) {
        Write-Host "  $label : OK" -ForegroundColor Green
    } else {
        Write-Host "  $label : FAILED - $($r.error)" -ForegroundColor Red
    }
}

Remove-Item -Recurse -Force $WorkDir -ErrorAction SilentlyContinue

$failedCount = ($results.Values | Where-Object { -not $_.ok }).Count
if ($failedCount -gt 0) {
    Write-Host "$failedCount mirror(s) failed - see above." -ForegroundColor Yellow
    exit 2
}
Write-Host "All mirrors verified." -ForegroundColor Green
