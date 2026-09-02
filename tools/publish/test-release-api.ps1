# release-api.ps1 ni SOXTA server bilan sinash - backend kerak emas.
#
#   .\tools\publish\test-release-api.ps1                 # toza yuklash
#   .\tools\publish\test-release-api.ps1 -FailOnChunk 2  # uzilish + davom etish
#
# Kontrakt: docs/superpowers/plans/2026-08-25-multi-device-sync-06-release-management.md
param([int]$Port = 18642, [int]$FailOnChunk = 0, [string]$Label = 'sinov')

$ErrorActionPreference = 'Stop'

$state = Join-Path $env:TEMP "mock-release-api-$Port"
if (Test-Path $state) { Remove-Item $state -Recurse -Force }
New-Item -ItemType Directory -Force -Path $state | Out-Null

# 10 MB soxta paket -> 4 MB bo'laklarda 3 ta so'rov
$pkg = Join-Path $state 'tx64upd7001000'
$rand = New-Object byte[] (10 * 1024 * 1024)
(New-Object Random 42).NextBytes($rand)
[System.IO.File]::WriteAllBytes($pkg, $rand)
$sha = (Get-FileHash $pkg -Algorithm SHA256).Hash

Write-Host "=== $Label ===" -ForegroundColor Magenta
Write-Host "paket: $([math]::Round($rand.Length/1MB,1)) MB, sha256 $($sha.Substring(0,16))..."

$job = Start-Job -FilePath (Join-Path $PSScriptRoot 'mock-release-api.ps1') `
    -ArgumentList $Port, $FailOnChunk, $state
Start-Sleep -Milliseconds 1200

try {
    . (Join-Path $PSScriptRoot 'release-api.ps1')

    $results = Publish-ReleaseViaApi `
        -Api "http://localhost:$Port" -Token 'test-token' -Version 7001000 `
        -PackagePath $pkg -PackageName 'tx64upd7001000' -Sha256 $sha

    Write-Host ""
    Write-Host "NATIJA:" -ForegroundColor Cyan
    foreach ($k in ($results.Keys | Sort-Object)) {
        if ($results[$k].ok) { "  $k : OK" }
        else { "  $k : FAILED - $($results[$k].error)" }
    }

    # Serverdagi fayl haqiqatan to'g'ri yig'ilganmi?
    $uploaded = Get-ChildItem $state -Filter 'upload-*.bin' | Select-Object -First 1
    $upHash = (Get-FileHash $uploaded.FullName -Algorithm SHA256).Hash
    Write-Host ""
    Write-Host "SERVERDAGI FAYL:" -ForegroundColor Cyan
    "  hajm : $($uploaded.Length) (kutilgan $($rand.Length))"
    if ($upHash -eq $sha) { Write-Host "  sha256: MOS" -ForegroundColor Green }
    else { Write-Host "  sha256: MOS EMAS ($upHash)" -ForegroundColor Red }

    # Idempotentlik: aynan shu relizni qayta yuborish
    Write-Host ""
    Write-Host "--- qayta yuborish (idempotentlik) ---" -ForegroundColor Magenta
    $results2 = Publish-ReleaseViaApi `
        -Api "http://localhost:$Port" -Token 'test-token' -Version 7001000 `
        -PackagePath $pkg -PackageName 'tx64upd7001000' -Sha256 $sha

    # Faqat bitta mirror'ni qayta urinish
    Write-Host ""
    Write-Host "--- ?only=vps-secure ---" -ForegroundColor Magenta
    $results3 = Publish-ReleaseViaApi `
        -Api "http://localhost:$Port" -Token 'test-token' -Version 7001000 `
        -PackagePath $pkg -PackageName 'tx64upd7001000' -Sha256 $sha `
        -OnlyMirror 'vps-secure'
    Write-Host "  qaytgan mirror soni: $($results3.Count) (kutilgan 1)"
} finally {
    try { Invoke-RestMethod "http://localhost:$Port/__shutdown" -TimeoutSec 3 | Out-Null } catch {}
    Start-Sleep -Milliseconds 400
    Stop-Job $job -ErrorAction SilentlyContinue
    Remove-Job $job -Force -ErrorAction SilentlyContinue
    Write-Host ""
    Write-Host "--- server jurnali ---" -ForegroundColor DarkGray
    Get-Content (Join-Path $state 'mock.log') -ErrorAction SilentlyContinue |
        Select-Object -First 40 | ForEach-Object { "  $_" }
}
