<#
.SYNOPSIS
    customsync-server Releases API client used by publish.ps1.

.DESCRIPTION
    Plan 06, Task 5. Replaces the scp/git fan-out in publish.ps1 with a
    single API call chain:

        POST   /api/v1/releases                      create (idempotent)
        POST   /api/v1/releases/{id}/upload          open an upload session
        PUT    /api/v1/releases/{id}/upload/{sid}    send one chunk
        GET    /api/v1/releases/{id}/upload/{sid}    how much arrived
        POST   /api/v1/releases/{id}/finish          verify sha256, stage it
        POST   /api/v1/releases/{id}/publish[?only=] fan out to mirrors

    Signing is NOT part of this file and never will be. Packer.exe runs
    locally in publish.ps1 and the private keys stay on this machine -
    the server only ever receives an already-signed package. A server
    that could sign could also ship a forged update.

    Publish-ReleaseViaApi returns the same $results shape publish.ps1
    already builds for the scp path (@{ label = @{ ok = $bool; error =
    $string } }), so the verification and summary steps downstream work
    unchanged for both paths.

.NOTES
    Windows PowerShell 5.1 constraints this file works around:

      * Chunk uploads go through System.Net.Http.HttpClient rather than
        Invoke-WebRequest. Content-Range is a content header; setting it
        through -Headers is fragile in 5.1, and Invoke-WebRequest offers
        no way to send a byte range of a file without loading the whole
        thing into memory first. HttpClient gives exact control over both.
      * No ternary / null-coalescing / && - all if-else.
#>

Set-StrictMode -Version Latest

# 4 MB. Big enough that a 52 MB package is ~13 requests, small enough
# that losing one chunk to a timeout costs little.
$script:ChunkSize = 4MB

# Consecutive failed chunk sends before giving up. Reset by any send
# that succeeds, so a long upload with occasional hiccups still gets
# through while a genuinely dead connection stops quickly.
$script:MaxChunkFailures = 3

# ---------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------

function Invoke-WithRetry {
    <#
        Retries a transient failure 3 times with exponential backoff.
        The real incident this plan exists for was a broken pipe partway
        through a 52 MB transfer, so "try again" is the common case, not
        an exceptional one.
    #>
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Action,
        [Parameter(Mandatory = $true)][string]$What,
        [int]$MaxAttempts = 3
    )
    $delay = 1
    for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
        try {
            return & $Action
        } catch {
            if ($attempt -eq $MaxAttempts) {
                throw "$What failed after $MaxAttempts attempts: $_"
            }
            Write-Host "  $What failed (attempt $attempt/$MaxAttempts): $_" -ForegroundColor Yellow
            Write-Host "  retrying in ${delay}s..." -ForegroundColor DarkGray
            Start-Sleep -Seconds $delay
            $delay = $delay * 2
        }
    }
}

function Invoke-ApiJson {
    param(
        [Parameter(Mandatory = $true)][string]$Method,
        [Parameter(Mandatory = $true)][string]$Url,
        [Parameter(Mandatory = $true)][string]$Token,
        $Body = $null
    )
    $headers = @{ Authorization = "Bearer $Token" }
    if ($null -eq $Body) {
        return Invoke-RestMethod -Method $Method -Uri $Url -Headers $headers -ErrorAction Stop
    }
    $json = $Body | ConvertTo-Json -Compress -Depth 5
    return Invoke-RestMethod -Method $Method -Uri $Url -Headers $headers `
        -Body $json -ContentType 'application/json' -ErrorAction Stop
}

function Get-ApiProperty {
    <#
        ConvertFrom-Json returns a PSCustomObject in 5.1, and a missing
        property is a hard error under Set-StrictMode. The server is
        allowed to grow fields, so read them defensively.
    #>
    param($Object, [string]$Name, $Default = $null)
    if ($null -eq $Object) { return $Default }
    $prop = $Object.PSObject.Properties[$Name]
    if ($null -eq $prop) { return $Default }
    if ($null -eq $prop.Value) { return $Default }
    return $prop.Value
}

# ---------------------------------------------------------------------
# Upload
# ---------------------------------------------------------------------

function Send-ApiChunk {
    param(
        [Parameter(Mandatory = $true)]$Client,
        [Parameter(Mandatory = $true)][string]$Url,
        [Parameter(Mandatory = $true)][string]$Token,
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][long]$Offset,
        [Parameter(Mandatory = $true)][int]$Length,
        [Parameter(Mandatory = $true)][long]$Total
    )
    $buffer = New-Object byte[] $Length
    $stream = [System.IO.File]::OpenRead($FilePath)
    try {
        $stream.Position = $Offset
        $read = $stream.Read($buffer, 0, $Length)
    } finally {
        $stream.Dispose()
    }
    if ($read -le 0) {
        throw "read 0 bytes at offset $Offset of $FilePath"
    }

    $content = New-Object System.Net.Http.ByteArrayContent($buffer, 0, $read)
    $content.Headers.ContentType =
        New-Object System.Net.Http.Headers.MediaTypeHeaderValue("application/octet-stream")
    $content.Headers.ContentRange =
        New-Object System.Net.Http.Headers.ContentRangeHeaderValue(
            $Offset, ($Offset + $read - 1), $Total)

    $request = New-Object System.Net.Http.HttpRequestMessage(
        [System.Net.Http.HttpMethod]::Put, $Url)
    $request.Content = $content
    $request.Headers.Authorization =
        New-Object System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", $Token)

    try {
        $response = $Client.SendAsync($request).GetAwaiter().GetResult()
        if (-not $response.IsSuccessStatusCode) {
            $text = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
            throw "HTTP $([int]$response.StatusCode) $($response.ReasonPhrase) - $text"
        }
    } finally {
        $request.Dispose()
    }
    return $read
}

function Send-ApiPackage {
    <#
        Uploads $FilePath in chunks, resuming from whatever the server
        already holds. On a chunk failure the server is asked how much
        it actually received rather than assuming - that answer is the
        only thing that makes resume correct after a half-written chunk.
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Api,
        [Parameter(Mandatory = $true)][string]$Token,
        [Parameter(Mandatory = $true)][string]$ReleaseId,
        [Parameter(Mandatory = $true)][string]$FilePath
    )
    $total = (Get-Item $FilePath).Length

    $session = Invoke-WithRetry -What "open upload session" -Action {
        Invoke-ApiJson -Method Post -Token $Token `
            -Url "$Api/api/v1/releases/$ReleaseId/upload" `
            -Body @{ size = $total }
    }
    $sid = Get-ApiProperty $session 'sid'
    if (-not $sid) { throw "server did not return an upload session id" }

    $statusUrl = "$Api/api/v1/releases/$ReleaseId/upload/$sid"
    $offset = [long](Get-ApiProperty $session 'received' 0)
    if ($offset -gt 0) {
        Write-Host "  resuming at $offset / $total bytes" -ForegroundColor Yellow
    }

    Add-Type -AssemblyName System.Net.Http
    $client = New-Object System.Net.Http.HttpClient
    try {
        # A 4 MB chunk over a slow link can legitimately take a while;
        # the default 100s timeout would abort a working transfer.
        $client.Timeout = [TimeSpan]::FromMinutes(5)

        # A failed chunk must NOT be retried at the same offset. A chunk
        # can be partly stored and still fail - the mock server's
        # simulated broken pipe writes half the bytes, then returns 500 -
        # and re-sending from the old offset then gets a 416 because the
        # server has already moved on. So every iteration, success or
        # failure, ends by asking the server how much it actually holds
        # and continuing from there. The server's count is the only
        # trustworthy resume point.
        $failures = 0
        while ($offset -lt $total) {
            $remaining = $total - $offset
            $length = [int][Math]::Min([long]$script:ChunkSize, $remaining)
            $percent = [math]::Round(100.0 * $offset / $total, 1)
            Write-Host "  chunk at $offset (+$length) - $percent%" -ForegroundColor DarkGray

            $sent = $true
            try {
                Send-ApiChunk -Client $client -Url $statusUrl -Token $Token `
                    -FilePath $FilePath -Offset $offset `
                    -Length $length -Total $total | Out-Null
            } catch {
                $sent = $false
                $failures++
                if ($failures -ge $script:MaxChunkFailures) {
                    throw "chunk at $offset failed $failures times in a row: $_"
                }
                $delay = [int][math]::Pow(2, $failures - 1)
                Write-Host "  chunk failed ($failures/$script:MaxChunkFailures): $_" -ForegroundColor Yellow
                Write-Host "  re-syncing with server, retrying in ${delay}s..." -ForegroundColor DarkGray
                Start-Sleep -Seconds $delay
            }

            $status = Invoke-WithRetry -What "upload status" -Action {
                Invoke-ApiJson -Method Get -Url $statusUrl -Token $Token
            }
            $received = [long](Get-ApiProperty $status 'received' 0)
            if ($received -lt $offset) {
                throw "server reports $received bytes but had $offset - it lost data, aborting"
            }
            if ($received -gt $offset) {
                if (-not $sent) {
                    Write-Host "  server kept $($received - $offset) bytes of the failed chunk - resuming at $received" -ForegroundColor Yellow
                }
                $offset = $received
            } elseif ($sent) {
                # No error, no progress: retrying would spin forever.
                throw "server accepted the chunk but still reports $received bytes - refusing to loop"
            }
            if ($sent) { $failures = 0 }
        }
    } finally {
        $client.Dispose()
    }

    Write-Host "  upload complete ($total bytes)" -ForegroundColor Green
    return $sid
}

# ---------------------------------------------------------------------
# Orchestration
# ---------------------------------------------------------------------

function Publish-ReleaseViaApi {
    <#
        Runs the whole API path and returns the $results hashtable that
        publish.ps1's summary and verification steps already understand.
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Api,
        [Parameter(Mandatory = $true)][string]$Token,
        [Parameter(Mandatory = $true)][int]$Version,
        [Parameter(Mandatory = $true)][string]$PackagePath,
        [Parameter(Mandatory = $true)][string]$PackageName,
        [Parameter(Mandatory = $true)][string]$Sha256,
        [string]$Platform = "win64",
        [string]$Channel = "stable",
        [string]$OnlyMirror = ""
    )
    $Api = $Api.TrimEnd('/')
    $size = (Get-Item $PackagePath).Length
    $results = @{}

    # PowerShell 5.1 still negotiates TLS 1.0 by default on some hosts;
    # most servers have dropped it.
    try {
        [Net.ServicePointManager]::SecurityProtocol =
            [Net.SecurityProtocolType]::Tls12 -bor [Net.SecurityProtocolType]::Tls11
    } catch { }

    Write-Host "Creating release via API ($Api)..." -ForegroundColor Cyan
    $release = Invoke-WithRetry -What "create release" -Action {
        Invoke-ApiJson -Method Post -Url "$Api/api/v1/releases" -Token $Token -Body @{
            platform     = $Platform
            version      = $Version
            channel      = $Channel
            sha256       = $Sha256
            size         = $size
            package_name = $PackageName
        }
    }
    $releaseId = Get-ApiProperty $release 'id'
    if (-not $releaseId) { throw "server did not return a release id" }
    $state = Get-ApiProperty $release 'state' ''
    Write-Host "  release id $releaseId (state: $state)" -ForegroundColor Cyan

    # Re-running the script is a normal event, not an error - that is the
    # whole reason the create call is idempotent. Skip straight to the
    # mirror fan-out when the bytes are already there.
    if ($state -eq 'already_exists') {
        Write-Host "  package already uploaded - skipping upload" -ForegroundColor Green
    } else {
        Write-Host "Uploading package ($size bytes, $($script:ChunkSize / 1MB) MB chunks)..." -ForegroundColor Cyan
        Send-ApiPackage -Api $Api -Token $Token -ReleaseId $releaseId -FilePath $PackagePath | Out-Null

        Write-Host "Finishing (server verifies sha256)..." -ForegroundColor Cyan
        $finish = Invoke-WithRetry -What "finish upload" -Action {
            Invoke-ApiJson -Method Post -Token $Token `
                -Url "$Api/api/v1/releases/$releaseId/finish" `
                -Body @{ sha256 = $Sha256 }
        }
        $finishState = Get-ApiProperty $finish 'state' ''
        if ($finishState -ne 'complete' -and $finishState -ne 'already_exists') {
            throw "finish returned state '$finishState' (expected complete)"
        }
        Write-Host "  checksum accepted" -ForegroundColor Green
    }

    $publishUrl = "$Api/api/v1/releases/$releaseId/publish"
    if ($OnlyMirror) {
        $publishUrl = "$publishUrl`?only=$OnlyMirror"
        Write-Host "Publishing to mirror '$OnlyMirror' only..." -ForegroundColor Cyan
    } else {
        Write-Host "Publishing to mirrors..." -ForegroundColor Cyan
    }

    # No retry wrapper here on purpose: a partial mirror failure is a
    # reportable result, not an error to retry blindly. The caller reruns
    # the failed one with -OnlyMirror, which is exactly what the original
    # incident needed.
    $publish = Invoke-ApiJson -Method Post -Url $publishUrl -Token $Token -Body @{}
    $mirrors = Get-ApiProperty $publish 'mirrors'
    if ($null -eq $mirrors) {
        throw "publish response contained no 'mirrors' array"
    }

    foreach ($m in $mirrors) {
        $name = Get-ApiProperty $m 'mirror' 'unknown'
        $mstate = Get-ApiProperty $m 'state' 'failed'
        if ($mstate -eq 'ok') {
            $results[$name] = @{ ok = $true }
            Write-Host "  $name : OK" -ForegroundColor Green
        } else {
            $err = Get-ApiProperty $m 'error' $mstate
            $results[$name] = @{ ok = $false; error = $err }
            Write-Host "  $name : FAILED - $err" -ForegroundColor Yellow
        }
    }
    return $results
}

function Get-ApiReleaseStatus {
    <#
        GET /api/v1/releases - every release and every mirror's state in
        one request. Replaces logging into the VPS to look around.
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Api,
        [Parameter(Mandatory = $true)][string]$Token
    )
    $Api = $Api.TrimEnd('/')
    return Invoke-ApiJson -Method Get -Url "$Api/api/v1/releases" -Token $Token
}
