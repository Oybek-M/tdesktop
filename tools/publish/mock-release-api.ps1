# Plan 06 Releases API'sining soxta implementatsiyasi.
# Faqat release-api.ps1 ni sinash uchun. Kontrakt plan 06 Task 2-4 dan.
param(
    [int]$Port = 18642,
    # N-chi bo'lakda ataylab uziladi (resume yo'lini sinash uchun).
    # 0 = hech qachon uzilmaydi.
    [int]$FailOnChunk = 0,
    [string]$StateDir
)

$ErrorActionPreference = 'Stop'
if (-not $StateDir) { $StateDir = Join-Path $env:TEMP "mock-release-api" }
New-Item -ItemType Directory -Force -Path $StateDir | Out-Null

$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:$Port/")
$listener.Start()
"MOCK: listening on http://localhost:$Port/" | Out-File (Join-Path $StateDir 'mock.log') -Append

$releases = @{}      # key -> @{ id; sha256; size; package_name; complete }
$sessions = @{}      # sid -> @{ releaseId; path; received }
$chunkCount = 0

function Write-Json($ctx, $obj, [int]$status = 200) {
    $json = $obj | ConvertTo-Json -Compress -Depth 6
    $bytes = [Text.Encoding]::UTF8.GetBytes($json)
    $ctx.Response.StatusCode = $status
    $ctx.Response.ContentType = 'application/json'
    $ctx.Response.ContentLength64 = $bytes.Length
    $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
    $ctx.Response.Close()
}

function Log($msg) {
    $msg | Out-File (Join-Path $StateDir 'mock.log') -Append
}

try {
    while ($listener.IsListening) {
        $ctx = $listener.GetContext()
        $path = $ctx.Request.Url.AbsolutePath
        $method = $ctx.Request.HttpMethod
        $query = $ctx.Request.Url.Query
        Log "$method $path$query"

        if ($path -eq '/__shutdown') { Write-Json $ctx @{ ok = $true }; break }

        # ---- POST /api/v1/releases -------------------------------------
        if ($method -eq 'POST' -and $path -eq '/api/v1/releases') {
            $reader = New-Object System.IO.StreamReader($ctx.Request.InputStream)
            $body = $reader.ReadToEnd() | ConvertFrom-Json
            $key = "$($body.platform)|$($body.version)|$($body.channel)"
            if ($releases.ContainsKey($key)) {
                $r = $releases[$key]
                if ($r.sha256 -eq $body.sha256 -and $r.complete) {
                    Write-Json $ctx @{ id = $r.id; state = 'already_exists' }
                    continue
                }
                Write-Json $ctx @{ id = $r.id; state = 'created' }
                continue
            }
            $id = [guid]::NewGuid().ToString('N').Substring(0, 8)
            $releases[$key] = @{
                id = $id; sha256 = $body.sha256; size = $body.size
                package_name = $body.package_name; complete = $false
            }
            Write-Json $ctx @{ id = $id; state = 'created' }
            continue
        }

        # ---- POST /api/v1/releases/{id}/upload -------------------------
        if ($method -eq 'POST' -and $path -match '^/api/v1/releases/([^/]+)/upload$') {
            $rid = $Matches[1]
            $existing = $sessions.GetEnumerator() |
                Where-Object { $_.Value.releaseId -eq $rid } | Select-Object -First 1
            if ($existing) {
                Write-Json $ctx @{ sid = $existing.Key; received = $existing.Value.received }
                continue
            }
            $sid = [guid]::NewGuid().ToString('N').Substring(0, 8)
            $sessions[$sid] = @{
                releaseId = $rid
                path = (Join-Path $StateDir "upload-$sid.bin")
                received = 0
            }
            [System.IO.File]::WriteAllBytes($sessions[$sid].path, @())
            Write-Json $ctx @{ sid = $sid; received = 0 }
            continue
        }

        # ---- PUT/GET /api/v1/releases/{id}/upload/{sid} ----------------
        if ($path -match '^/api/v1/releases/([^/]+)/upload/([^/]+)$') {
            $sid = $Matches[2]
            if (-not $sessions.ContainsKey($sid)) {
                Write-Json $ctx @{ error = 'no such session' } 404; continue
            }
            $s = $sessions[$sid]

            if ($method -eq 'GET') {
                Write-Json $ctx @{ sid = $sid; received = $s.received }
                continue
            }
            if ($method -eq 'PUT') {
                $chunkCount++
                $range = $ctx.Request.Headers['Content-Range']
                if (-not $range) {
                    Write-Json $ctx @{ error = 'Content-Range required' } 400; continue
                }
                if ($range -notmatch '^bytes (\d+)-(\d+)/(\d+)$') {
                    Write-Json $ctx @{ error = "bad Content-Range: $range" } 400; continue
                }
                $from = [long]$Matches[1]; $to = [long]$Matches[2]; $total = [long]$Matches[3]

                $ms = New-Object System.IO.MemoryStream
                $ctx.Request.InputStream.CopyTo($ms)
                $bytes = $ms.ToArray()

                if ($from -ne $s.received) {
                    Write-Json $ctx @{ error = "expected offset $($s.received), got $from" } 416
                    continue
                }

                # Uzilish simulyatsiyasi: yarmini YOZIB, keyin xato qaytaradi.
                # Bu eng yomon holat - server qisman qabul qilgan, klient esa
                # buni bilmaydi. Klient GET bilan haqiqiy holatni so'rashi shart.
                if ($FailOnChunk -gt 0 -and $chunkCount -eq $FailOnChunk) {
                    $half = [int]($bytes.Length / 2)
                    $fs = [System.IO.File]::Open($s.path, 'Append')
                    $fs.Write($bytes, 0, $half); $fs.Close()
                    $s.received = $s.received + $half
                    Log "MOCK: chunk $chunkCount ataylab uzildi (yarmi yozildi: $half)"
                    Write-Json $ctx @{ error = 'simulated broken pipe' } 500
                    continue
                }

                $fs = [System.IO.File]::Open($s.path, 'Append')
                $fs.Write($bytes, 0, $bytes.Length); $fs.Close()
                $s.received = $s.received + $bytes.Length
                Write-Json $ctx @{ received = $s.received }
                continue
            }
        }

        # ---- POST /api/v1/releases/{id}/finish -------------------------
        if ($method -eq 'POST' -and $path -match '^/api/v1/releases/([^/]+)/finish$') {
            $rid = $Matches[1]
            $reader = New-Object System.IO.StreamReader($ctx.Request.InputStream)
            $body = $reader.ReadToEnd() | ConvertFrom-Json
            $sess = $sessions.GetEnumerator() |
                Where-Object { $_.Value.releaseId -eq $rid } | Select-Object -First 1
            $hash = (Get-FileHash $sess.Value.path -Algorithm SHA256).Hash
            if ($hash -ne $body.sha256) {
                Log "MOCK: checksum mos emas: kutilgan $($body.sha256), olingan $hash"
                Write-Json $ctx @{ error = 'checksum mismatch' } 422
                continue
            }
            foreach ($k in @($releases.Keys)) {
                if ($releases[$k].id -eq $rid) { $releases[$k].complete = $true }
            }
            Write-Json $ctx @{ state = 'complete' }
            continue
        }

        # ---- POST /api/v1/releases/{id}/publish ------------------------
        if ($method -eq 'POST' -and $path -match '^/api/v1/releases/([^/]+)/publish$') {
            $only = $null
            if ($query -match 'only=([^&]+)') { $only = $Matches[1] }
            $all = @('vps-secure', 'vps-pub', 'github')
            $out = @()
            foreach ($m in $all) {
                if ($only -and $m -ne $only) { continue }
                if ($m -eq 'vps-secure' -and -not $only) {
                    $out += @{ mirror = $m; state = 'failed'; error = 'disk full' }
                } else {
                    $out += @{ mirror = $m; state = 'ok' }
                }
            }
            Write-Json $ctx @{ mirrors = $out }
            continue
        }

        Write-Json $ctx @{ error = "unhandled $method $path" } 404
    }
} finally {
    $listener.Stop()
    $listener.Close()
}
