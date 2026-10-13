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

.PARAMETER Api
    Base URL of customsync-server. When given, the mirror fan-out goes
    through its Releases API (resumable chunked upload, idempotent
    re-runs, per-mirror status) instead of scp + git push. Without it
    the original scp path runs unchanged - deliberately kept as the
    fallback for when the backend is down.

.PARAMETER Token
    Bearer token for -Api. Keep it in an environment variable rather
    than shell history: -Token $env:CUSTOMSYNC_TOKEN.

.PARAMETER OnlyMirror
    API mode only. Re-publish to a single mirror by name, e.g.
    -OnlyMirror vps-secure. This is the case the API exists for: a run
    where two mirrors succeeded and one did not should not re-upload
    52 MB to fix the third.

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

    [Parameter(Mandatory = $false)]
    [string]$Api = "",

    [Parameter(Mandatory = $false)]
    [string]$Token = "",

    [Parameter(Mandatory = $false)]
    [string]$OnlyMirror = "",

    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$RepoRoot     = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$VersionFile  = Join-Path $RepoRoot "Telegram\SourceFiles\core\version.h"
# A7: reliz kanali va tartib raqami (klient ham aynan shu fayldan oladi).
$ReleaseFile  = Join-Path $RepoRoot "Telegram\SourceFiles\custom_release.h"
$PackerExe    = Join-Path $RepoRoot "out\Release\Packer.exe"
$DesktopPrivateDir = Join-Path $RepoRoot "..\DesktopPrivate"
# A27 (7.3+): paket V2 (Ed25519) bilan imzolanadi. Maxfiy imzolovchi kalit
# faqat DesktopPrivate'da; ochiq qismlar (ildiz kaliti, manifest va uning
# imzosi) repo'dagi Resources/update'da -- Packer ularni -keys-loc dan o'qiydi.
$V2SigningKey = Join-Path $DesktopPrivateDir "update-v2\signing-oy-2026a-private.pem"
$V2KeyId      = "oy-2026a"
$V2KeysLoc    = Join-Path $RepoRoot "Telegram\Resources\update"
# Windows'da Platform::AutoUpdateVersion() = 6, klient "<prefix>/current6"
# ni so'raydi (eski "current4" 7.x klientlar uchun 404 berardi).
$FeedName     = "current6"
# 7.3+ klientning standart prefiksi ".../secure/v2" (localstorage.cpp):
# v1 va V2 paketlar hech qachon bitta lentada bo'lmasligi uchun.
$FeedSubdir   = "v2"
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

if (-not (Test-Path $V2SigningKey)) {
    Fail "V2 signing key not found at $V2SigningKey. Without it, Packer.exe cannot sign anything - check docs/self-update/a27-v2-signing.md."
}
foreach ($name in @("root-public.pem", "manifest.min.json", "manifest.sig")) {
    if (-not (Test-Path (Join-Path $V2KeysLoc $name))) {
        Fail "Missing $name in $V2KeysLoc - check docs/self-update/a27-v2-signing.md."
    }
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
if ($OnlyMirror -and -not $Api) {
    Fail "-OnlyMirror only applies to the API path. Pass -Api as well, or drop -OnlyMirror."
}
if ($Api -and -not $Token) {
    Fail "-Api needs -Token. Keep the token in an environment variable: -Token `$env:CUSTOMSYNC_TOKEN"
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

# A7: asosiy versiya upstream bilan bir xil qoladi, o'z relizlarimiz
# "canary-public" kanalida "<versiya> canary #N" bo'lib chiqadi
# (docs/self-update/a7-canary-releases.md). Paket nomi, Packer argumentlari
# va feed yozuvi shunga qarab tanlanadi.
$releaseContent = Get-Content $ReleaseFile -Raw
if ($releaseContent -match 'kUpdateChannel = Core::Updates::Channel::CanaryPublic;') {
    if ($releaseContent -notmatch 'constexpr auto kCounter = (\d+);') {
        Fail "Could not parse kCounter out of $ReleaseFile"
    }
    $counter = [int]$Matches[1]
    if ($counter -le 0) {
        Fail "kCounter must be positive for the canary-public channel ($ReleaseFile)"
    }
    if ($Api) {
        # Releases API feed'ga faqat "released" yozadi, "counter" ni
        # bilmaydi -- canary relizi hozircha faqat scp yo'lidan chiqadi.
        Fail "-Api does not support canary releases yet; run without -Api (scp path)."
    }
    $packerChannelArgs = @("-channel", "canary-public", "-counter", "$counter")
    $packageName = "td-update-win-x64-$version-canary-$counter"
    $feedEntry = @{ canary = @{
        released = $version
        counter  = $counter
        link     = "/win/$packageName"
    } }
    $releaseLabel = "$version canary #$counter"
    Write-Host "Channel: canary-public #$counter" -ForegroundColor Cyan
} elseif ($releaseContent -match 'kUpdateChannel = Core::Updates::Channel::Stable;') {
    $packerChannelArgs = @("-channel", "stable")
    $packageName = "td-update-win-x64-$version"
    $feedEntry = @{ stable = @{
        released = $version
        link     = "/win/$packageName"
    } }
    $releaseLabel = "$version"
    Write-Host "Channel: stable" -ForegroundColor Cyan
} else {
    Fail "Unsupported kUpdateChannel in $ReleaseFile (expected Stable or CanaryPublic)"
}

# --- Step 2: sign with Packer ----------------------------------------------
#
# Packer computes the archive-relative name of each file by stripping a
# "remove" prefix equal to QFileInfo(firstPathArg).canonicalPath() - for a
# directory argument that's the directory's PARENT, not the directory
# itself (Qt treats the last path segment as a "file name" regardless of
# whether it's actually a folder). Passing -path $StagingDir therefore
# archived every file as "release-staging\<name>" instead of "<name>",
# so Updater.exe copied the update into a nested release-staging\ folder
# instead of over the real installation (found 2026-08-08 debugging why
# a real update download+restart cycle left the version unchanged).
# Fix: pass each top-level entry inside $StagingDir as its own -path, so
# the prefix Packer strips is $StagingDir itself.
Write-Host "Signing with Packer.exe..." -ForegroundColor Cyan
$stagingEntries = Get-ChildItem -Path $StagingDir -Force
if (-not $stagingEntries) {
    Fail "Staging directory is empty: $StagingDir"
}
$packerPathArgs = foreach ($entry in $stagingEntries) { "-path"; $entry.FullName }
Push-Location $WorkDir
try {
    # Packer V2 paketni yozishdan oldin o'zi tekshiradi (VerifyPackedV2).
    & $PackerExe @packerPathArgs -version $version -target win64 `
        @packerChannelArgs -keys-loc $V2KeysLoc `
        -local-key $V2SigningKey -local-key-id $V2KeyId
    if ($LASTEXITCODE -ne 0) {
        Fail "Packer.exe exited with code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

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
    win64 = $feedEntry
} | ConvertTo-Json -Compress -Depth 5

$manifestPath = Join-Path $WorkDir $FeedName
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
        & scp $manifestPath "${SshHost}:$remotePath/$FeedName"
        if ($LASTEXITCODE -ne 0) { throw "scp manifest failed" }
        $results[$label] = @{ ok = $true }
    } catch {
        Write-Host "  Warning: $label upload failed - $_" -ForegroundColor Yellow
        $results[$label] = @{ ok = $false; error = $_.ToString() }
    }
}

# API path (plan 06). Publish-ReleaseViaApi returns the same $results
# shape this script already builds, and the server reports its mirrors
# under the same names used below ("vps-secure", "vps-pub", "github"),
# so Steps 5 and 6 work for both paths without a second code path.
if ($Api) {
    . (Join-Path $PSScriptRoot "release-api.ps1")
    try {
        $results = Publish-ReleaseViaApi `
            -Api $Api `
            -Token $Token `
            -Version $version `
            -PackagePath $packagePath `
            -PackageName $packageName `
            -Sha256 $localHash `
            -OnlyMirror $OnlyMirror
    } catch {
        Fail "API publish failed: $_"
    }
} else {

Upload-Vps "vps-secure" "/var/www/updates/secure/$FeedSubdir"
Upload-Vps "vps-pub"    "/var/www/updates/$($Secrets.pub_prefix)/$FeedSubdir"

Write-Host "Uploading to GitHub ($GithubRepo)..." -ForegroundColor Cyan
try {
    $ghWorkDir = Join-Path $WorkDir "gh-repo"
    & gh repo clone $GithubRepo $ghWorkDir -- --depth 1
    if ($LASTEXITCODE -ne 0) { throw "gh repo clone failed" }
    New-Item -ItemType Directory -Force -Path (Join-Path $ghWorkDir "$FeedSubdir\win") | Out-Null
    Copy-Item $packagePath (Join-Path $ghWorkDir "$FeedSubdir\win\$packageName")
    Copy-Item $manifestPath (Join-Path $ghWorkDir "$FeedSubdir\$FeedName")
    Push-Location $ghWorkDir
    try {
        & git add -A
        & git commit -m "release: version $releaseLabel"
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

} # end of the scp path

$anySucceeded = ($results.Values | Where-Object { $_.ok }).Count -gt 0
if (-not $anySucceeded) {
    Fail "Every mirror failed. No release was published."
}

# --- Step 5: re-download and verify checksums -------------------------------
# Mandatory, not optional: a mirror that reports success but silently
# stores a truncated file blocks updates for everyone pointed at it,
# and nobody finds out until someone reports the app never updating.

function Verify-Url($label, $url, $headers = @{}) {
    if (-not $results.ContainsKey($label) -or -not $results[$label].ok) {
        return  # already failed to upload, nothing to verify
    }
    Write-Host "Verifying $label ($url)..." -ForegroundColor Cyan
    $checkPath = Join-Path $WorkDir "verify-$label"
    try {
        Invoke-WebRequest -Uri $url -Headers $headers -OutFile $checkPath -ErrorAction Stop
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

# NOTE: credentials go in an Authorization header, not embedded as
# user:pass@host in the URL - Invoke-WebRequest on Windows PowerShell
# 5.1 silently drops URL-embedded userinfo instead of sending it as
# Basic auth, which made every verify attempt come back 401 even with
# the correct password (found 2026-08-02 against the live VPS: curl -u
# succeeded with the same credentials while Invoke-WebRequest with an
# embedded-credential URL failed).
$secureAuthHeader = @{
    Authorization = "Basic " + [Convert]::ToBase64String(
        [Text.Encoding]::ASCII.GetBytes("$($Secrets.secure_user):$($Secrets.secure_pass)")
    )
}
Verify-Url "vps-secure" "https://updates.2007.uz/secure/$FeedSubdir/win/$packageName" $secureAuthHeader
Verify-Url "vps-pub"    "https://updates.2007.uz/$($Secrets.pub_prefix)/$FeedSubdir/win/$packageName"

# --- Step 6: summary ---------------------------------------------------

Write-Host ""
Write-Host "=== Release $releaseLabel summary ===" -ForegroundColor Cyan
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
