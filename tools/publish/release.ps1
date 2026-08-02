<#
.SYNOPSIS
    One-shot release: stage the build output, then sign+publish it.

.DESCRIPTION
    Wraps publish.ps1 so a full release is a single command:
      1. Build a staging folder INSIDE out\Release (release-staging\)
         containing only what should ship: Telegram.exe, Updater.exe,
         modules\. Debug symbols (*.pdb), log.txt, Packer.exe and
         tdata\ (your personal session!) are deliberately left out.
         If the staging folder already exists, files are overwritten
         in place rather than recreated from scratch.
      2. Call publish.ps1 -StagingDir <that folder>, which signs with
         Packer.exe and uploads to all three mirrors.

.PARAMETER SshHost
    Passed through to publish.ps1. See publish.ps1 -SshHost.

.PARAMETER GithubRepo
    Passed through to publish.ps1. See publish.ps1 -GithubRepo.

.PARAMETER DryRun
    Passed through to publish.ps1. Stages the files and signs with
    Packer, but skips every upload.

.EXAMPLE
    .\tools\publish\release.ps1

.EXAMPLE
    .\tools\publish\release.ps1 -DryRun
#>

[CmdletBinding()]
param(
    [string]$SshHost = "customsync-vps",
    [string]$GithubRepo = "Oybek-M/tdesktop-releases",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$RepoRoot    = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$OutRelease  = Join-Path $RepoRoot "out\Release"
$StagingDir  = Join-Path $OutRelease "release-staging"
$PublishPs1  = Join-Path $PSScriptRoot "publish.ps1"

function Fail($message) {
    Write-Host "STAGING FAILED: $message" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $OutRelease)) {
    Fail "Build output not found at $OutRelease. Build Telegram + Updater + Packer first."
}

$telegramExe = Join-Path $OutRelease "Telegram.exe"
$updaterExe  = Join-Path $OutRelease "Updater.exe"
$modulesDir  = Join-Path $OutRelease "modules"

if (-not (Test-Path $telegramExe)) { Fail "Telegram.exe not found in $OutRelease" }
if (-not (Test-Path $updaterExe))  { Fail "Updater.exe not found in $OutRelease" }

Write-Host "Staging into $StagingDir ..." -ForegroundColor Cyan

New-Item -ItemType Directory -Force -Path $StagingDir | Out-Null

Copy-Item $telegramExe (Join-Path $StagingDir "Telegram.exe") -Force
Copy-Item $updaterExe  (Join-Path $StagingDir "Updater.exe") -Force

if (Test-Path $modulesDir) {
    $stagingModules = Join-Path $StagingDir "modules"
    New-Item -ItemType Directory -Force -Path $stagingModules | Out-Null
    Copy-Item (Join-Path $modulesDir "*") $stagingModules -Recurse -Force
}

Write-Host "Staging ready: $StagingDir" -ForegroundColor Green
Write-Host "Handing off to publish.ps1 ..." -ForegroundColor Cyan

$publishArgs = @{
    StagingDir = $StagingDir
    SshHost    = $SshHost
    GithubRepo = $GithubRepo
}
if ($DryRun) { $publishArgs["DryRun"] = $true }

& $PublishPs1 @publishArgs
exit $LASTEXITCODE
