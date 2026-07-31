# Backend Sync Core Implementation Plan (1b)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Poydevor ustiga sync qatlamini qurish — cursor ajratish, dedup bilan push, cursor bo'yicha pull, media saqlash, kalit o'ramlari, keyset pagination, statistika, WebSocket bildirishnoma va `.cmx` almashuv formati.

**Architecture:** Sync hot-path'i raw SQL orqali ishlaydi (`NpgsqlCommand`), chunki `ON CONFLICT ... WHERE` shartli upsert va bitta tranzaksiya ichida cursor ajratish EF Core'ning LINQ qatlamida ifodalab bo'lmaydigan narsalar. Qolgan hamma joyda EF Core.

**Tech Stack:** 1a dagi bilan bir xil + `System.IO.Compression` (`.cmx` uchun), ASP.NET Core WebSockets.

**Kirish sharti:** [01a](2026-07-29-multi-device-sync-01a-backend-foundation.md) tugagan va uning qabul qilish mezonlari bajarilgan.

**Umumiy qoidalar:** [00-index](2026-07-29-multi-device-sync-00-index.md) dagi K1–K7.

---

## File Structure

```
src/CustomSync.Core/
├── Contracts/
│   ├── PushRequest.cs          # push so'rovi va natijasi DTO
│   ├── PullResponse.cs
│   └── PushOutcome.cs          # created | duplicate | superseded | error
└── Interchange/
    ├── CmxManifest.cs
    ├── CmxWriter.cs            # .cmx yozish — server ham, capture service ham ishlatadi
    └── CmxReader.cs

src/CustomSync.Services/
├── SyncService.cs              # push + pull. Loyihaning yuragi.
├── MediaService.cs             # kontent-adresli blob saqlash
├── KeyWrapService.cs
├── RecordQueryService.cs       # keyset pagination
├── StatsService.cs
└── InterchangeService.cs       # .cmx eksport/import — SyncService ni qayta ishlatadi

src/CustomSync.Api/
├── Realtime/NotifyHub.cs
└── Endpoints/
    ├── SyncEndpoints.cs
    ├── MediaEndpoints.cs
    ├── KeyEndpoints.cs
    ├── RecordEndpoints.cs
    ├── StatsEndpoints.cs
    └── InterchangeEndpoints.cs

tests/CustomSync.Tests/
├── SeqMonotonicityTests.cs     # eng muhim test — quyida sababi
├── SyncServiceTests.cs
├── PaginationTests.cs
├── MediaServiceTests.cs
└── InterchangeTests.cs

tools/
└── GenerateTestVectors/        # platformalararo test vektorlarini chiqaradi
```

---

## Task 1: Cursor ajratish va monotonlik kafolati

Bu plandagi eng nozik qism. Xato qilinsa, **yozuvlar jimgina yo'qoladi** va
buni haftalar o'tib payqashadi.

**Muammo:** `BIGSERIAL` yetarli emas. Ikki parallel tranzaksiya `seq` oladi
(A=5, B=6), lekin B avval commit qiladi. Shu paytda klient `since=6` bilan
pull qilsa, A hali commit qilinmagan — keyin A commit bo'ladi, lekin klient
cursor'i allaqachon 6 da. **Yozuv 5 hech qachon olinmaydi.**

**Yechim:** `seq` ni bitta qator ustidagi lock bilan berish. Lock commit'gacha
ushlanadi, shuning uchun `seq` tartibi = commit tartibi.

**Files:**
- Create: `tests/CustomSync.Tests/SeqMonotonicityTests.cs`
- Create: `src/CustomSync.Services/SyncService.cs`

- [ ] **Step 1: Yiqiladigan testni yozish**

`tests/CustomSync.Tests/SeqMonotonicityTests.cs`:

```csharp
using CustomSync.Core;
using CustomSync.Core.Contracts;
using CustomSync.Services;
using CustomSync.Tests.Fixtures;
using Xunit;

namespace CustomSync.Tests;

public class SeqMonotonicityTests : IClassFixture<DatabaseFixture>
{
    private readonly DatabaseFixture _fixture;

    public SeqMonotonicityTests(DatabaseFixture fixture) => _fixture = fixture;

    private static SyncRecord Make(int index) => new()
    {
        RecordId   = RecordId.Compute(RecordKind.Deleted, "peer01", index, 1753800000),
        Kind       = RecordKind.Deleted,
        PeerHash   = "peer01",
        MsgId      = index,
        OccurredAt = 1753800000,
        ObservedAt = 1753800000 + index,
        DeviceId   = "test-device",
        Nonce      = new byte[12],
        Payload    = [1, 2, 3]
    };

    /// <summary>
    /// Regressiya testi: BIGSERIAL ishlatilsa bu test yiqiladi.
    /// Parallel push'lar davomida uzluksiz pull qilamiz va OXIRIDA
    /// hech bir yozuv o'tkazib yuborilmaganini tekshiramiz.
    /// </summary>
    [Fact]
    public async Task Concurrent_pushes_are_never_skipped_by_a_polling_reader()
    {
        const int total = 60;
        var seen = new HashSet<string>();
        var cursor = 0L;
        using var stop = new CancellationTokenSource();

        var reader = Task.Run(async () =>
        {
            while (!stop.IsCancellationRequested)
            {
                await using var db = _fixture.CreateContext();
                var page = await new SyncService(db).PullAsync(cursor, 500);
                foreach (var r in page.Records) seen.Add(r.RecordId);
                if (page.Records.Count > 0) cursor = page.NextSince;
                await Task.Delay(5);
            }
        });

        var writers = Enumerable.Range(0, total).Select(i => Task.Run(async () =>
        {
            await using var db = _fixture.CreateContext();
            await new SyncService(db).PushAsync("test-device", [Make(i)]);
        }));
        await Task.WhenAll(writers);

        // Yozuvchilar tugagach reader oxirgi bo'lakni ham olishi uchun kutamiz.
        await Task.Delay(300);
        stop.Cancel();
        await reader;

        await using var final = _fixture.CreateContext();
        var tail = await new SyncService(final).PullAsync(cursor, 500);
        foreach (var r in tail.Records) seen.Add(r.RecordId);

        Assert.Equal(total, seen.Count);
    }

    [Fact]
    public async Task Seq_values_are_unique_and_increasing()
    {
        await using var db = _fixture.CreateContext();
        var service = new SyncService(db);

        await service.PushAsync("test-device",
            Enumerable.Range(100, 20).Select(Make).ToList());

        var page = await service.PullAsync(0, 500);
        var seqs = page.Records.Select(r => r.Seq).ToList();

        Assert.Equal(seqs.Count, seqs.Distinct().Count());
        Assert.Equal(seqs.OrderBy(s => s), seqs);
    }
}
```

- [ ] **Step 2: Kontraktlarni yozish**

`src/CustomSync.Core/Contracts/PushOutcome.cs`:

```csharp
namespace CustomSync.Core.Contracts;

public static class PushOutcome
{
    /// <summary>Yangi yozuv qo'shildi.</summary>
    public const string Created = "created";

    /// <summary>Allaqachon mavjud va mavjudi yaxshiroq — o'zgarish yo'q.</summary>
    public const string Duplicate = "duplicate";

    /// <summary>Mavjud yozuv yaxshiroq kuzatuv bilan almashtirildi.</summary>
    public const string Superseded = "superseded";

    /// <summary>Rad etildi — klient outbox'da ushlab qolishi va qayta urinishi kerak.</summary>
    public const string Error = "error";
}

public sealed record PushResult(
    string RecordId, string Status, long? Seq = null, string? Message = null);
```

`src/CustomSync.Core/Contracts/PullResponse.cs`:

```csharp
namespace CustomSync.Core.Contracts;

public sealed record StoredRecord
{
    public required long   Seq         { get; init; }
    public required string RecordId    { get; init; }
    public required string Kind        { get; init; }
    public required string PeerHash    { get; init; }
    public required long   MsgId       { get; init; }
    public required long   OccurredAt  { get; init; }
    public required long   ObservedAt  { get; init; }
    public required string DeviceId    { get; init; }
    public required byte[] Nonce       { get; init; }
    public required byte[] Payload     { get; init; }
    public IReadOnlyList<string> MediaHashes { get; init; } = Array.Empty<string>();
}

public sealed record PullResponse
{
    public required IReadOnlyList<StoredRecord> Records { get; init; }
    public required long NextSince { get; init; }
    public required bool HasMore   { get; init; }
}
```

- [ ] **Step 3: Testni ishga tushirib, yiqilishini ko'rish**

```bash
dotnet test --filter SeqMonotonicityTests
```

Kutilgan: FAIL — `SyncService` mavjud emas.

- [ ] **Step 4: `SyncService` ni yozish**

`src/CustomSync.Services/SyncService.cs`:

```csharp
using CustomSync.Core.Contracts;
using CustomSync.Data;
using Microsoft.EntityFrameworkCore;
using Npgsql;
using NpgsqlTypes;

namespace CustomSync.Services;

/// <summary>
/// Sync yadrosi. Raw SQL ishlatiladi, chunki bu yerdagi ikki narsani
/// EF Core'ning LINQ qatlamida ifodalab bo'lmaydi: bitta tranzaksiya
/// ichida cursor ajratish va shartli upsert (ON CONFLICT ... WHERE).
/// </summary>
public class SyncService(SyncDbContext db)
{
    /// <summary>
    /// MUHIM: cursor BIGSERIAL bilan berilmaydi.
    ///
    /// BIGSERIAL da ikki parallel tranzaksiya seq oladi (A=5, B=6) va B
    /// avval commit qilishi mumkin. Shu payt pull qilgan klient cursor'ini
    /// 6 ga surib qo'yadi, keyin A commit bo'ladi — va 5-yozuv hech qachon
    /// olinmaydi. Yozuv jimgina yo'qoladi.
    ///
    /// sync_counter qatorini UPDATE qilish qator lockini commit'gacha
    /// ushlaydi, shuning uchun seq tartibi = commit tartibi.
    ///
    /// Dublikat push'da seq behuda sarflanadi — bu ataylab qabul qilingan.
    /// Cursor faqat monotonlikni talab qiladi, zichlikni emas, shuning
    /// uchun bo'shliqlar zararsiz.
    /// </summary>
    private const string UpsertSql = """
        WITH allocated AS (
          UPDATE sync_counter SET value = value + 1 WHERE id = 1 RETURNING value
        )
        INSERT INTO records (
            record_id, seq, kind, peer_hash, msg_id, occurred_at,
            observed_at, device_id, nonce, payload, payload_size, received_at)
        SELECT @record_id, allocated.value, @kind, @peer_hash, @msg_id,
               @occurred_at, @observed_at, @device_id, @nonce, @payload,
               @payload_size, now()
        FROM allocated
        ON CONFLICT (record_id) DO UPDATE SET
            seq          = EXCLUDED.seq,
            observed_at  = EXCLUDED.observed_at,
            device_id    = EXCLUDED.device_id,
            nonce        = EXCLUDED.nonce,
            payload      = EXCLUDED.payload,
            payload_size = EXCLUDED.payload_size,
            received_at  = EXCLUDED.received_at
        WHERE records.observed_at > EXCLUDED.observed_at
           OR (records.observed_at = EXCLUDED.observed_at
               AND records.device_id > EXCLUDED.device_id)
        RETURNING seq, (xmax::text = '0') AS was_insert;
        """;

    public async Task<IReadOnlyList<PushResult>> PushAsync(
        string deviceId,
        IReadOnlyList<SyncRecord> records,
        CancellationToken ct = default)
    {
        var results = new List<PushResult>(records.Count);
        var connection = (NpgsqlConnection)db.Database.GetDbConnection();
        if (connection.State != System.Data.ConnectionState.Open)
            await connection.OpenAsync(ct);

        foreach (var record in records)
        {
            if (!RecordKind.IsValid(record.Kind))
            {
                results.Add(new PushResult(
                    record.RecordId, PushOutcome.Error, Message: "unknown_kind"));
                continue;
            }

            var expected = Core.RecordId.Compute(
                record.Kind, record.PeerHash, record.MsgId, record.OccurredAt);
            if (!string.Equals(expected, record.RecordId, StringComparison.Ordinal))
            {
                // Klient record_id ni noto'g'ri hisoblagan — bu interop bug'i.
                // Qabul qilsak dedup butunlay buziladi, shuning uchun rad etamiz.
                results.Add(new PushResult(
                    record.RecordId, PushOutcome.Error, Message: "record_id_mismatch"));
                continue;
            }

            await using var cmd = new NpgsqlCommand(UpsertSql, connection);
            cmd.Parameters.AddWithValue("record_id",   record.RecordId);
            cmd.Parameters.AddWithValue("kind",        record.Kind);
            cmd.Parameters.AddWithValue("peer_hash",   record.PeerHash);
            cmd.Parameters.AddWithValue("msg_id",      record.MsgId);
            cmd.Parameters.AddWithValue("occurred_at", record.OccurredAt);
            cmd.Parameters.AddWithValue("observed_at", record.ObservedAt);
            cmd.Parameters.AddWithValue("device_id",   deviceId);
            cmd.Parameters.Add("nonce",   NpgsqlDbType.Bytea).Value = record.Nonce;
            cmd.Parameters.Add("payload", NpgsqlDbType.Bytea).Value = record.Payload;
            // Hajm serverda hisoblanadi — klientga ishonilmaydi.
            cmd.Parameters.AddWithValue("payload_size", record.Payload.Length);

            await using var reader = await cmd.ExecuteReaderAsync(ct);
            if (await reader.ReadAsync(ct))
            {
                var seq = reader.GetInt64(0);
                var wasInsert = reader.GetBoolean(1);
                results.Add(new PushResult(
                    record.RecordId,
                    wasInsert ? PushOutcome.Created : PushOutcome.Superseded,
                    seq));
            }
            else
            {
                // Qator qaytmadi = ON CONFLICT DO UPDATE ning WHERE sharti
                // bajarilmadi = mavjud yozuv yaxshiroq. Klient buni
                // muvaffaqiyat deb hisoblaydi va outbox'dan o'chiradi.
                results.Add(new PushResult(record.RecordId, PushOutcome.Duplicate));
            }
        }

        return results;
    }

    public async Task<PullResponse> PullAsync(
        long since, int limit, CancellationToken ct = default)
    {
        var rows = await db.Records.AsNoTracking()
            .Where(r => r.Seq > since)
            .OrderBy(r => r.Seq)
            .Take(limit)
            .Select(r => new StoredRecord
            {
                Seq        = r.Seq,
                RecordId   = r.RecordId,
                Kind       = r.Kind,
                PeerHash   = r.PeerHash,
                MsgId      = r.MsgId,
                OccurredAt = r.OccurredAt,
                ObservedAt = r.ObservedAt,
                DeviceId   = r.DeviceId,
                Nonce      = r.Nonce,
                Payload    = r.Payload
            })
            .ToListAsync(ct);

        if (rows.Count == 0)
            return new PullResponse { Records = rows, NextSince = since, HasMore = false };

        var ids = rows.Select(r => r.RecordId).ToList();
        var links = await db.RecordMedia.AsNoTracking()
            .Where(m => ids.Contains(m.RecordId))
            .ToListAsync(ct);

        var withMedia = rows.Select(r => r with
        {
            MediaHashes = links.Where(l => l.RecordId == r.RecordId)
                               .Select(l => l.Hash).ToList()
        }).ToList();

        var next = withMedia[^1].Seq;
        return new PullResponse
        {
            Records   = withMedia,
            NextSince = next,
            HasMore   = withMedia.Count == limit
        };
    }
}
```

`CustomSync.Services` loyihasiga Npgsql paketi kerak:

```bash
dotnet add src/CustomSync.Services package Npgsql
```

- [ ] **Step 5: Testni qayta ishga tushirish**

```bash
dotnet test --filter SeqMonotonicityTests
```

Kutilgan: PASS — 2 ta test o'tdi.

- [ ] **Step 6: Yechim haqiqatan kerakligini isbotlash**

`UpsertSql` dagi `WITH allocated AS (...)` ni vaqtincha `nextval` bilan
almashtiring (`seq BIGSERIAL` sifatida) va testni qayta ishga tushiring —
`Concurrent_pushes_are_never_skipped_by_a_polling_reader` **yiqilishi
kerak**. Bu bug haqiqiy ekanini tasdiqlaydi. So'ng o'zgarishni qaytaring.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat: add sync push/pull with commit-ordered cursor allocation

seq comes from a locked counter row rather than BIGSERIAL. With BIGSERIAL
two concurrent transactions can take 5 and 6 and commit out of order, so a
reader that polls in between advances its cursor past 5 and never sees it
-- the record is lost silently. Locking the counter row until commit makes
seq order equal commit order.

Push also rejects a record whose record_id does not match the recomputed
value: accepting it would break dedup for every future observation of that
same event."
```

---

## Task 2: Push va pull endpoint'lari

**Files:**
- Create: `src/CustomSync.Api/Endpoints/SyncEndpoints.cs`
- Modify: `src/CustomSync.Api/Program.cs`

- [ ] **Step 1: Endpoint'larni yozish**

`src/CustomSync.Api/Endpoints/SyncEndpoints.cs`:

```csharp
using System.Security.Claims;
using CustomSync.Api.Realtime;
using CustomSync.Core.Contracts;
using CustomSync.Services;

namespace CustomSync.Api.Endpoints;

public static class SyncEndpoints
{
    public record PushRequest(IReadOnlyList<SyncRecord> Records);

    public static void MapSyncEndpoints(this WebApplication app)
    {
        var group = app.MapGroup("/api/v1/sync").RequireAuthorization();

        group.MapPost("/push", async (
            PushRequest request,
            ClaimsPrincipal user,
            SyncService sync,
            SettingsService settings,
            NotifyHub hub) =>
        {
            var deviceId = user.FindFirstValue(ClaimTypes.NameIdentifier)!;

            var maxBatch = await settings.GetIntAsync("sync.push_batch_size");
            if (request.Records.Count > maxBatch)
                return Results.BadRequest(new
                {
                    error = "batch_too_large",
                    max = maxBatch,
                    received = request.Records.Count
                });

            var results = await sync.PushAsync(deviceId, request.Records);

            // Faqat haqiqatan o'zgarish bo'lsa xabar beramiz — dublikatlar
            // boshqa qurilmalarni behuda uyg'otmasligi kerak.
            var applied = results
                .Where(r => r.Seq.HasValue)
                .Select(r => r.Seq!.Value)
                .DefaultIfEmpty(0)
                .Max();
            if (applied > 0)
                await hub.NotifyOthersAsync(deviceId, applied);

            return Results.Ok(new { results });
        });

        group.MapGet("/pull", async (
            long since,
            int? limit,
            SyncService sync,
            SettingsService settings) =>
        {
            var configured = await settings.GetIntAsync("sync.pull_batch_size");
            var effective = Math.Clamp(limit ?? configured, 1, configured);
            return Results.Ok(await sync.PullAsync(since, effective));
        });
    }
}
```

`Program.cs` ga qo'shing:

```csharp
builder.Services.AddScoped<SyncService>();
builder.Services.AddSingleton<NotifyHub>();
// ...
app.MapSyncEndpoints();
```

- [ ] **Step 2: Kompilyatsiyani tekshirish**

`NotifyHub` hali yozilmagan (Task 6). Vaqtincha stub yarating —
`src/CustomSync.Api/Realtime/NotifyHub.cs`:

```csharp
namespace CustomSync.Api.Realtime;

public class NotifyHub
{
    public Task NotifyOthersAsync(string originDeviceId, long seq)
        => Task.CompletedTask;
}
```

```bash
dotnet build
```

Kutilgan: `Build succeeded. 0 Error(s)`.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: expose sync push and pull endpoints

Push only notifies other devices when something actually changed, so a
device re-sending records that turned out to be duplicates does not wake
every other client for nothing."
```

---

## Task 3: Media saqlash

**Files:**
- Create: `src/CustomSync.Services/MediaService.cs`
- Create: `src/CustomSync.Api/Endpoints/MediaEndpoints.cs`
- Create: `tests/CustomSync.Tests/MediaServiceTests.cs`
- Modify: `src/CustomSync.Api/appsettings.json`

- [ ] **Step 1: Yiqiladigan testni yozish**

`tests/CustomSync.Tests/MediaServiceTests.cs`:

```csharp
using CustomSync.Services;
using CustomSync.Tests.Fixtures;
using Xunit;

namespace CustomSync.Tests;

public class MediaServiceTests : IClassFixture<DatabaseFixture>, IDisposable
{
    private readonly DatabaseFixture _fixture;
    private readonly string _root =
        Path.Combine(Path.GetTempPath(), $"cs-media-{Guid.NewGuid():N}");

    public MediaServiceTests(DatabaseFixture fixture) => _fixture = fixture;

    public void Dispose()
    {
        if (Directory.Exists(_root)) Directory.Delete(_root, recursive: true);
    }

    [Fact]
    public async Task Stored_blob_can_be_read_back_byte_for_byte()
    {
        await using var db = _fixture.CreateContext();
        var service = new MediaService(db, _root);
        var content = new byte[] { 9, 8, 7, 6, 5 };

        await service.StoreAsync("hash-aaa", content, new byte[12]);
        var read = await service.ReadAsync("hash-aaa");

        Assert.Equal(content, read);
    }

    [Fact]
    public async Task Storing_the_same_hash_twice_is_a_no_op()
    {
        await using var db = _fixture.CreateContext();
        var service = new MediaService(db, _root);

        await service.StoreAsync("hash-bbb", [1, 2, 3], new byte[12]);
        await service.StoreAsync("hash-bbb", [1, 2, 3], new byte[12]);

        Assert.True(await service.ExistsAsync("hash-bbb"));
        Assert.Equal(new byte[] { 1, 2, 3 }, await service.ReadAsync("hash-bbb"));
    }

    [Fact]
    public async Task Missing_blob_reads_as_null_rather_than_throwing()
    {
        await using var db = _fixture.CreateContext();
        var service = new MediaService(db, _root);

        Assert.Null(await service.ReadAsync("nope"));
        Assert.False(await service.ExistsAsync("nope"));
    }
}
```

- [ ] **Step 2: Testni ishga tushirib, yiqilishini ko'rish**

```bash
dotnet test --filter MediaServiceTests
```

Kutilgan: FAIL — `MediaService` mavjud emas.

- [ ] **Step 3: `MediaService` ni yozish**

`src/CustomSync.Services/MediaService.cs`:

```csharp
using CustomSync.Data;
using CustomSync.Data.Entities;
using Microsoft.EntityFrameworkCore;

namespace CustomSync.Services;

/// <summary>
/// Kontent-adresli shifrlangan blob saqlash. Blob'lar diskda, metadata
/// bazada — katta ikkilik ma'lumotni PostgreSQL ichida saqlash zaxira
/// olishni ham, so'rovlarni ham sekinlashtiradi.
/// </summary>
public class MediaService(SyncDbContext db, string storageRoot)
{
    public async Task<bool> ExistsAsync(string hash, CancellationToken ct = default)
        => await db.MediaBlobs.AnyAsync(m => m.Hash == hash, ct);

    public async Task StoreAsync(
        string hash, byte[] encryptedContent, byte[] nonce,
        CancellationToken ct = default)
    {
        if (await ExistsAsync(hash, ct)) return;

        var path = PathFor(hash);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        await File.WriteAllBytesAsync(path, encryptedContent, ct);

        db.MediaBlobs.Add(new MediaBlobEntity
        {
            Hash        = hash,
            Size        = encryptedContent.LongLength,
            Nonce       = nonce,
            StoragePath = path,
            UploadedAt  = DateTime.UtcNow
        });
        await db.SaveChangesAsync(ct);
    }

    public async Task<byte[]?> ReadAsync(string hash, CancellationToken ct = default)
    {
        var blob = await db.MediaBlobs.AsNoTracking()
            .FirstOrDefaultAsync(m => m.Hash == hash, ct);
        if (blob is null || !File.Exists(blob.StoragePath)) return null;
        return await File.ReadAllBytesAsync(blob.StoragePath, ct);
    }

    /// <summary>
    /// Bitta katalogda o'n minglab fayl to'planmasligi uchun hash'ning
    /// birinchi ikki belgisi bo'yicha sharding.
    /// </summary>
    private string PathFor(string hash)
    {
        var prefix = hash.Length >= 2 ? hash[..2] : "00";
        return Path.Combine(storageRoot, prefix, hash);
    }
}
```

- [ ] **Step 4: Testni qayta ishga tushirish**

```bash
dotnet test --filter MediaServiceTests
```

Kutilgan: PASS — 3 ta test o'tdi.

- [ ] **Step 5: Endpoint'larni yozish**

`appsettings.json` ga qo'shing:

```json
"Storage": { "MediaRoot": "/var/lib/customsync/media" }
```

`src/CustomSync.Api/Endpoints/MediaEndpoints.cs`:

```csharp
using CustomSync.Services;

namespace CustomSync.Api.Endpoints;

public static class MediaEndpoints
{
    public static void MapMediaEndpoints(this WebApplication app)
    {
        var group = app.MapGroup("/api/v1/media").RequireAuthorization();

        // Klient yuklashdan oldin shu bilan tekshiradi — boshqa qurilma
        // allaqachon yuklagan bo'lsa trafik behuda sarflanmaydi.
        group.MapMethods("/{hash}", ["HEAD"], async (
            string hash, MediaService media) =>
            await media.ExistsAsync(hash) ? Results.Ok() : Results.NotFound());

        group.MapPut("/{hash}", async (
            string hash, HttpRequest request,
            MediaService media, SettingsService settings) =>
        {
            var max = await settings.GetIntAsync("media.max_upload_bytes");
            using var buffer = new MemoryStream();
            await request.Body.CopyToAsync(buffer);
            if (buffer.Length > max)
                return Results.StatusCode(StatusCodes.Status413PayloadTooLarge);

            var nonceHeader = request.Headers["X-Nonce"].ToString();
            var nonce = string.IsNullOrEmpty(nonceHeader)
                ? new byte[12]
                : Convert.FromBase64String(nonceHeader);

            await media.StoreAsync(hash, buffer.ToArray(), nonce);
            return Results.Ok();
        }).DisableAntiforgery();

        group.MapGet("/{hash}", async (string hash, MediaService media) =>
        {
            var content = await media.ReadAsync(hash);
            return content is null
                ? Results.NotFound()
                : Results.File(content, "application/octet-stream");
        });
    }
}
```

`Program.cs` da:

```csharp
builder.Services.AddScoped(sp => new MediaService(
    sp.GetRequiredService<SyncDbContext>(),
    builder.Configuration["Storage:MediaRoot"]!));
// ...
app.MapMediaEndpoints();
```

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat: add content-addressed encrypted media storage

Blobs live on disk with only metadata in Postgres -- large binaries inside
the database would slow both backups and queries. HEAD exists so a client
can skip uploading a blob another device already sent."
```

---

## Task 4: Kalit o'ramlari

**Files:**
- Create: `src/CustomSync.Services/KeyWrapService.cs`
- Create: `src/CustomSync.Api/Endpoints/KeyEndpoints.cs`

- [ ] **Step 1: `KeyWrapService` ni yozish**

`src/CustomSync.Services/KeyWrapService.cs`:

```csharp
using CustomSync.Data;
using CustomSync.Data.Entities;
using Microsoft.EntityFrameworkCore;

namespace CustomSync.Services;

public sealed record WrapSummary(
    string WrapId, string WrapType, string Label,
    DateTime CreatedAt, DateTime? LastUsedAt);

/// <summary>
/// Master kalitning o'ralgan nusxalari. Server o'ralgan blob'ni saqlaydi,
/// lekin uni hech qachon ocha olmaydi — KEK klientda hosil qilinadi.
///
/// Qurilma o'ramlari bu yerda saqlanmaydi: ular OS keystore ichida
/// qoladi. Bu jadval faqat KO'CHMA tiklash yo'llari uchun.
/// </summary>
public class KeyWrapService(SyncDbContext db, AuditService audit)
{
    public async Task<IReadOnlyList<WrapSummary>> ListAsync(
        CancellationToken ct = default)
        => await db.KeyWraps.AsNoTracking()
            .OrderBy(w => w.CreatedAt)
            .Select(w => new WrapSummary(
                w.WrapId, w.WrapType, w.Label, w.CreatedAt, w.LastUsedAt))
            .ToListAsync(ct);

    public async Task<KeyWrapEntity?> GetAsync(
        string wrapId, CancellationToken ct = default)
    {
        var wrap = await db.KeyWraps.FirstOrDefaultAsync(w => w.WrapId == wrapId, ct);
        if (wrap is null) return null;

        wrap.LastUsedAt = DateTime.UtcNow;
        await db.SaveChangesAsync(ct);
        await audit.WriteAsync("keywrap.retrieved", detail: new { wrapId, wrap.WrapType }, ct: ct);
        return wrap;
    }

    public async Task<string> CreateAsync(
        string wrapType, string label, byte[] salt, byte[] nonce,
        byte[] wrappedKey, int iterations, CancellationToken ct = default)
    {
        var wrapId = Guid.NewGuid().ToString("N");
        db.KeyWraps.Add(new KeyWrapEntity
        {
            WrapId     = wrapId,
            WrapType   = wrapType,
            Label      = label,
            Salt       = salt,
            Nonce      = nonce,
            WrappedKey = wrappedKey,
            Iterations = iterations,
            CreatedAt  = DateTime.UtcNow
        });
        await db.SaveChangesAsync(ct);
        await audit.WriteAsync("keywrap.created", detail: new { wrapId, wrapType, label }, ct: ct);
        return wrapId;
    }

    public async Task DeleteAsync(string wrapId, CancellationToken ct = default)
    {
        var removed = await db.KeyWraps
            .Where(w => w.WrapId == wrapId)
            .ExecuteDeleteAsync(ct);
        if (removed > 0)
            await audit.WriteAsync("keywrap.deleted", detail: new { wrapId }, ct: ct);
    }
}
```

- [ ] **Step 2: Endpoint'larni yozish (rate limiting bilan)**

`Program.cs` ga rate limiter qo'shing (`builder.Build()` dan oldin):

```csharp
builder.Services.AddRateLimiter(options =>
{
    options.AddFixedWindowLimiter("keywrap", o =>
    {
        // Spec 4.4.1: email escrow o'ramini offline hujumdan himoya qilish
        // uchun yuklab olish urinishlari cheklanadi.
        o.PermitLimit = 5;
        o.Window = TimeSpan.FromHours(1);
        o.QueueLimit = 0;
    });
    options.RejectionStatusCode = StatusCodes.Status429TooManyRequests;
});
```

`app.UseAuthentication()` dan oldin: `app.UseRateLimiter();`

`src/CustomSync.Api/Endpoints/KeyEndpoints.cs`:

```csharp
using CustomSync.Services;

namespace CustomSync.Api.Endpoints;

public static class KeyEndpoints
{
    public record CreateWrapRequest(
        string WrapType, string Label, string Salt,
        string Nonce, string WrappedKey, int Iterations);

    public static void MapKeyEndpoints(this WebApplication app)
    {
        var group = app.MapGroup("/api/v1/keys/wraps").RequireAuthorization();

        group.MapGet("/", async (KeyWrapService keys) =>
            Results.Ok(await keys.ListAsync()));

        group.MapGet("/{wrapId}", async (string wrapId, KeyWrapService keys) =>
        {
            var wrap = await keys.GetAsync(wrapId);
            return wrap is null ? Results.NotFound() : Results.Ok(new
            {
                wrap.WrapId,
                wrap.WrapType,
                wrap.Label,
                Salt       = Convert.ToBase64String(wrap.Salt),
                Nonce      = Convert.ToBase64String(wrap.Nonce),
                WrappedKey = Convert.ToBase64String(wrap.WrappedKey),
                wrap.Iterations
            });
        }).RequireRateLimiting("keywrap");

        group.MapPost("/", async (CreateWrapRequest request, KeyWrapService keys) =>
        {
            var wrapId = await keys.CreateAsync(
                request.WrapType, request.Label,
                Convert.FromBase64String(request.Salt),
                Convert.FromBase64String(request.Nonce),
                Convert.FromBase64String(request.WrappedKey),
                request.Iterations);
            return Results.Ok(new { wrapId });
        });

        group.MapDelete("/{wrapId}", async (string wrapId, KeyWrapService keys) =>
        {
            await keys.DeleteAsync(wrapId);
            return Results.NoContent();
        });
    }
}
```

`Program.cs` da `builder.Services.AddScoped<KeyWrapService>();` va
`app.MapKeyEndpoints();`.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: add key wrap storage with rate-limited retrieval

Wrap retrieval is rate limited because an attacker who obtains the emailed
half of an escrow secret can otherwise pull the wrap and brute-force the
PIN entirely offline. Device wraps are deliberately absent: they stay in
the OS keystore, and this table holds only portable recovery paths."
```

---

## Task 5: Keyset pagination va statistika

**Files:**
- Create: `src/CustomSync.Services/RecordQueryService.cs`
- Create: `src/CustomSync.Services/StatsService.cs`
- Create: `src/CustomSync.Api/Endpoints/RecordEndpoints.cs`
- Create: `src/CustomSync.Api/Endpoints/StatsEndpoints.cs`
- Create: `tests/CustomSync.Tests/PaginationTests.cs`

- [ ] **Step 1: Yiqiladigan testni yozish**

Bu test aynan foydalanuvchi so'ragan xatti-harakatni tekshiradi:
sahifalash davomida yangi ma'lumot kelsa **dublikat ham, tushib qolgan
qator ham bo'lmasligi** kerak.

`tests/CustomSync.Tests/PaginationTests.cs`:

```csharp
using CustomSync.Core;
using CustomSync.Core.Contracts;
using CustomSync.Services;
using CustomSync.Tests.Fixtures;
using Xunit;

namespace CustomSync.Tests;

public class PaginationTests : IClassFixture<DatabaseFixture>
{
    private readonly DatabaseFixture _fixture;

    public PaginationTests(DatabaseFixture fixture) => _fixture = fixture;

    private static SyncRecord Make(int index, long occurredAt) => new()
    {
        RecordId   = RecordId.Compute(RecordKind.Deleted, "peerP", index, occurredAt),
        Kind       = RecordKind.Deleted,
        PeerHash   = "peerP",
        MsgId      = index,
        OccurredAt = occurredAt,
        ObservedAt = occurredAt,
        DeviceId   = "test-device",
        Nonce      = new byte[12],
        Payload    = [1]
    };

    [Fact]
    public async Task Paging_under_concurrent_inserts_yields_no_duplicates_and_no_gaps()
    {
        await using var db = _fixture.CreateContext();
        var sync = new SyncService(db);
        var query = new RecordQueryService(db);

        await sync.PushAsync("test-device",
            Enumerable.Range(0, 40).Select(i => Make(i, 1_700_000_000 + i)).ToList());

        var snapshot = await query.CurrentSnapshotAsync();
        var seen = new List<string>();
        long? afterKey = null;
        long? afterSeq = null;

        while (true)
        {
            var page = await query.QueryAsync(new RecordQuery
            {
                Snapshot  = snapshot,
                Limit     = 10,
                Descending = true,
                AfterKey  = afterKey,
                AfterSeq  = afterSeq
            });
            if (page.Count == 0) break;

            seen.AddRange(page.Select(r => r.RecordId));
            afterKey = page[^1].OccurredAt;
            afterSeq = page[^1].Seq;

            // Sahifalar orasida yangi ma'lumot keladi — snapshot uni
            // ushbu sahifalashdan tashqarida ushlab turishi kerak.
            await sync.PushAsync("test-device",
                [Make(1000 + seen.Count, 1_800_000_000 + seen.Count)]);
        }

        Assert.Equal(40, seen.Count);
        Assert.Equal(40, seen.Distinct().Count());
    }

    [Fact]
    public async Task Ascending_and_descending_return_exact_reverse_order()
    {
        await using var db = _fixture.CreateContext();
        var sync = new SyncService(db);
        var query = new RecordQueryService(db);

        await sync.PushAsync("test-device",
            Enumerable.Range(0, 15).Select(i => Make(i, 1_600_000_000 + i)).ToList());
        var snapshot = await query.CurrentSnapshotAsync();

        var desc = await query.QueryAsync(new RecordQuery
        { Snapshot = snapshot, Limit = 100, Descending = true });
        var asc = await query.QueryAsync(new RecordQuery
        { Snapshot = snapshot, Limit = 100, Descending = false });

        Assert.Equal(desc.Select(r => r.RecordId),
                     asc.Select(r => r.RecordId).Reverse());
    }
}
```

- [ ] **Step 2: Testni ishga tushirib, yiqilishini ko'rish**

```bash
dotnet test --filter PaginationTests
```

Kutilgan: FAIL — `RecordQueryService` mavjud emas.

- [ ] **Step 3: `RecordQueryService` ni yozish**

`src/CustomSync.Services/RecordQueryService.cs`:

```csharp
using CustomSync.Data;
using Microsoft.EntityFrameworkCore;

namespace CustomSync.Services;

public sealed record RecordQuery
{
    /// <summary>
    /// So'rov boshlanganda olingan max(seq). Barcha sahifalar shu
    /// nuqtadan oldingi holatni ko'radi, shuning uchun sahifalash
    /// davomida kelgan yangi yozuvlar qatorlarni surib yubormaydi.
    /// </summary>
    public required long Snapshot   { get; init; }
    public int   Limit              { get; init; } = 50;
    public bool  Descending         { get; init; } = true;
    public long? AfterKey           { get; init; }
    public long? AfterSeq           { get; init; }
    public string? PeerHash         { get; init; }
    public string? Kind             { get; init; }
    public long? FromOccurredAt     { get; init; }
    public long? ToOccurredAt       { get; init; }
}

public sealed record RecordSummary
{
    public required long   Seq         { get; init; }
    public required string RecordId    { get; init; }
    public required string Kind        { get; init; }
    public required string PeerHash    { get; init; }
    public required long   MsgId       { get; init; }
    public required long   OccurredAt  { get; init; }
    public required long   ObservedAt  { get; init; }
    public required string DeviceId    { get; init; }
    public required int    PayloadSize { get; init; }
}

/// <summary>
/// Web app uchun so'rovlar. OFFSET ishlatilmaydi (qoida K3): u sync
/// paytida qatorlarni surib yuboradi va dublikat yoki tushib qolgan
/// qator beradi.
/// </summary>
public class RecordQueryService(SyncDbContext db)
{
    public async Task<long> CurrentSnapshotAsync(CancellationToken ct = default)
        => await db.Records.AnyAsync(ct)
            ? await db.Records.MaxAsync(r => r.Seq, ct)
            : 0;

    public async Task<IReadOnlyList<RecordSummary>> QueryAsync(
        RecordQuery query, CancellationToken ct = default)
    {
        var q = db.Records.AsNoTracking().Where(r => r.Seq <= query.Snapshot);

        if (query.PeerHash is not null)     q = q.Where(r => r.PeerHash == query.PeerHash);
        if (query.Kind is not null)         q = q.Where(r => r.Kind == query.Kind);
        if (query.FromOccurredAt is not null) q = q.Where(r => r.OccurredAt >= query.FromOccurredAt);
        if (query.ToOccurredAt is not null)   q = q.Where(r => r.OccurredAt <= query.ToOccurredAt);

        // Keyset: (occurred_at, seq) juftligi bo'yicha qat'iy taqqoslash.
        // seq tiebreaker sifatida zarur — bir xil occurred_at li qatorlar
        // aks holda cheksiz siklga yoki tushib qolishga olib keladi.
        if (query.AfterKey is not null && query.AfterSeq is not null)
        {
            var key = query.AfterKey.Value;
            var seq = query.AfterSeq.Value;
            q = query.Descending
                ? q.Where(r => r.OccurredAt < key
                            || (r.OccurredAt == key && r.Seq < seq))
                : q.Where(r => r.OccurredAt > key
                            || (r.OccurredAt == key && r.Seq > seq));
        }

        q = query.Descending
            ? q.OrderByDescending(r => r.OccurredAt).ThenByDescending(r => r.Seq)
            : q.OrderBy(r => r.OccurredAt).ThenBy(r => r.Seq);

        return await q.Take(query.Limit)
            .Select(r => new RecordSummary
            {
                Seq         = r.Seq,
                RecordId    = r.RecordId,
                Kind        = r.Kind,
                PeerHash    = r.PeerHash,
                MsgId       = r.MsgId,
                OccurredAt  = r.OccurredAt,
                ObservedAt  = r.ObservedAt,
                DeviceId    = r.DeviceId,
                PayloadSize = r.PayloadSize
            })
            .ToListAsync(ct);
    }

    public async Task<byte[]?> GetPayloadAsync(
        string recordId, CancellationToken ct = default)
        => await db.Records.AsNoTracking()
            .Where(r => r.RecordId == recordId)
            .Select(r => r.Payload)
            .FirstOrDefaultAsync(ct);
}
```

- [ ] **Step 4: Testni qayta ishga tushirish**

```bash
dotnet test --filter PaginationTests
```

Kutilgan: PASS — 2 ta test o'tdi.

- [ ] **Step 5: `StatsService` ni yozish**

Bu foydalanuvchi so'ragan "kim bilan ko'p yozilgan", "kim ko'p joy olgan"
statistikasi. `peer_hash` HMAC bo'lgani uchun ham server buni bemalol
hisoblaydi — u faqat ismni bilmaydi, uni web app qo'shadi.

`src/CustomSync.Services/StatsService.cs`:

```csharp
using CustomSync.Data;
using Microsoft.EntityFrameworkCore;

namespace CustomSync.Services;

public sealed record PeerStat(
    string PeerHash, int RecordCount, long TotalBytes, long LastOccurredAt);

public sealed record StorageStat(
    long RecordCount, long RecordBytes, long MediaCount, long MediaBytes);

public class StatsService(SyncDbContext db)
{
    /// <summary>
    /// peer_hash deterministik HMAC bo'lgani uchun guruhlash serverda
    /// ishlaydi — ism kerak emas. Web app natijaga ismlarni o'zi qo'shadi.
    /// </summary>
    public async Task<IReadOnlyList<PeerStat>> PeersAsync(
        string sort = "bytes", int limit = 100, CancellationToken ct = default)
    {
        var grouped = db.Records.AsNoTracking()
            .GroupBy(r => r.PeerHash)
            .Select(g => new PeerStat(
                g.Key,
                g.Count(),
                g.Sum(r => (long)r.PayloadSize),
                g.Max(r => r.OccurredAt)));

        grouped = sort switch
        {
            "count"  => grouped.OrderByDescending(p => p.RecordCount),
            "recent" => grouped.OrderByDescending(p => p.LastOccurredAt),
            _        => grouped.OrderByDescending(p => p.TotalBytes)
        };

        return await grouped.Take(limit).ToListAsync(ct);
    }

    public async Task<StorageStat> StorageAsync(CancellationToken ct = default)
    {
        var recordCount = await db.Records.LongCountAsync(ct);
        var recordBytes = recordCount == 0
            ? 0
            : await db.Records.SumAsync(r => (long)r.PayloadSize, ct);
        var mediaCount  = await db.MediaBlobs.LongCountAsync(ct);
        var mediaBytes  = mediaCount == 0
            ? 0
            : await db.MediaBlobs.SumAsync(m => m.Size, ct);

        return new StorageStat(recordCount, recordBytes, mediaCount, mediaBytes);
    }
}
```

- [ ] **Step 6: Endpoint'larni yozish**

`src/CustomSync.Api/Endpoints/RecordEndpoints.cs`:

```csharp
using CustomSync.Services;

namespace CustomSync.Api.Endpoints;

public static class RecordEndpoints
{
    public static void MapRecordEndpoints(this WebApplication app)
    {
        var group = app.MapGroup("/api/v1/records").RequireAuthorization();

        group.MapGet("/snapshot", async (RecordQueryService query) =>
            Results.Ok(new { snapshot = await query.CurrentSnapshotAsync() }));

        group.MapGet("/", async (
            long snapshot, int? limit, bool? desc,
            long? afterKey, long? afterSeq,
            string? peerHash, string? kind, long? from, long? to,
            RecordQueryService query, SettingsService settings) =>
        {
            var defaultSize = await settings.GetIntAsync("api.default_page_size");
            var maxSize     = await settings.GetIntAsync("api.max_page_size");

            var rows = await query.QueryAsync(new RecordQuery
            {
                Snapshot       = snapshot,
                Limit          = Math.Clamp(limit ?? defaultSize, 1, maxSize),
                Descending     = desc ?? true,
                AfterKey       = afterKey,
                AfterSeq       = afterSeq,
                PeerHash       = peerHash,
                Kind           = kind,
                FromOccurredAt = from,
                ToOccurredAt   = to
            });

            return Results.Ok(new
            {
                records = rows,
                nextAfterKey = rows.Count > 0 ? rows[^1].OccurredAt : (long?)null,
                nextAfterSeq = rows.Count > 0 ? rows[^1].Seq : (long?)null
            });
        });

        group.MapGet("/{recordId}/payload", async (
            string recordId, RecordQueryService query) =>
        {
            var payload = await query.GetPayloadAsync(recordId);
            return payload is null
                ? Results.NotFound()
                : Results.File(payload, "application/octet-stream");
        });
    }
}
```

`src/CustomSync.Api/Endpoints/StatsEndpoints.cs`:

```csharp
using CustomSync.Services;

namespace CustomSync.Api.Endpoints;

public static class StatsEndpoints
{
    public static void MapStatsEndpoints(this WebApplication app)
    {
        var group = app.MapGroup("/api/v1/stats").RequireAuthorization();

        group.MapGet("/peers", async (
            string? sort, int? limit, StatsService stats) =>
            Results.Ok(await stats.PeersAsync(sort ?? "bytes", limit ?? 100)));

        group.MapGet("/storage", async (StatsService stats) =>
            Results.Ok(await stats.StorageAsync()));
    }
}
```

`Program.cs` da servislarni va endpoint'larni ro'yxatdan o'tkazing.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat: add keyset-paginated record queries and peer statistics

Pagination pins every page to a seq snapshot taken when the query started,
so records arriving mid-scroll cannot shift rows under the reader and
produce duplicates or gaps -- the failing case is covered by a test that
inserts between pages. The (occurred_at, seq) tuple comparison needs seq as
a tiebreaker; without it, rows sharing a timestamp are skipped or repeated.

Peer statistics group by the HMAC peer_hash, which stays useful for
COUNT/SUM/ORDER BY even though the server cannot resolve it to a name."
```

---

## Task 6: WebSocket bildirishnoma

**Files:**
- Modify: `src/CustomSync.Api/Realtime/NotifyHub.cs` (stub'ni almashtirish)
- Modify: `src/CustomSync.Api/Program.cs`

- [ ] **Step 1: `NotifyHub` ni yozish**

`src/CustomSync.Api/Realtime/NotifyHub.cs` ni to'liq almashtiring:

```csharp
using System.Collections.Concurrent;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;

namespace CustomSync.Api.Realtime;

/// <summary>
/// Jonli bildirishnoma. Ma'lumot bu kanal orqali YURMAYDI — yuboriladigan
/// yagona narsa "yangilik bor, pull qil" signali. Shu sababli SignalR emas,
/// oddiy WebSocket: payload trivial, va C++ hamda mobil klientlarda
/// QWebSocket / OkHttp / URLSession bilan qo'shimcha kutubxonasiz ishlaydi.
///
/// Bu kanal uzilsa tizim to'g'ri ishlashda davom etadi — periodik pull
/// asosiy yo'l bo'lib qoladi.
/// </summary>
public class NotifyHub
{
    private readonly ConcurrentDictionary<string, ConcurrentBag<WebSocket>> _sockets = new();

    public void Register(string deviceId, WebSocket socket)
        => _sockets.GetOrAdd(deviceId, _ => []).Add(socket);

    public async Task NotifyOthersAsync(string originDeviceId, long seq)
    {
        var message = JsonSerializer.Serialize(new { type = "changes", seq });
        var bytes = Encoding.UTF8.GetBytes(message);

        foreach (var (deviceId, sockets) in _sockets)
        {
            if (deviceId == originDeviceId) continue;
            foreach (var socket in sockets)
            {
                if (socket.State != WebSocketState.Open) continue;
                try
                {
                    await socket.SendAsync(bytes, WebSocketMessageType.Text,
                        endOfMessage: true, CancellationToken.None);
                }
                catch (WebSocketException)
                {
                    // Uzilgan ulanish — e'tiborsiz qoldiramiz. Klient qayta
                    // ulanadi va oradagi o'zgarishlarni pull orqali oladi.
                }
            }
        }
    }

    public void Prune()
    {
        foreach (var (deviceId, sockets) in _sockets)
        {
            var alive = sockets.Where(s => s.State == WebSocketState.Open).ToList();
            _sockets[deviceId] = new ConcurrentBag<WebSocket>(alive);
        }
    }
}
```

- [ ] **Step 2: Endpoint va query-string autentifikatsiyasi**

Brauzerlar WebSocket so'roviga header qo'sha olmaydi, shuning uchun JWT
query string orqali beriladi. `Program.cs` dagi `AddJwtBearer` ichiga:

```csharp
o.Events = new JwtBearerEvents
{
    OnMessageReceived = context =>
    {
        var token = context.Request.Query["access_token"];
        if (!string.IsNullOrEmpty(token) &&
            context.HttpContext.Request.Path.StartsWithSegments("/ws"))
        {
            context.Token = token;
        }
        return Task.CompletedTask;
    }
};
```

`app.UseAuthentication()` dan oldin `app.UseWebSockets();`, so'ng:

```csharp
app.Map("/ws/notify", async (HttpContext context, NotifyHub hub) =>
{
    if (!context.WebSockets.IsWebSocketRequest)
    {
        context.Response.StatusCode = StatusCodes.Status400BadRequest;
        return;
    }

    var deviceId = context.User.FindFirst(
        System.Security.Claims.ClaimTypes.NameIdentifier)?.Value;
    if (deviceId is null)
    {
        context.Response.StatusCode = StatusCodes.Status401Unauthorized;
        return;
    }

    using var socket = await context.WebSockets.AcceptWebSocketAsync();
    hub.Register(deviceId, socket);

    // Klient hech narsa yubormaydi; ulanish yopilguncha ushlab turamiz.
    var buffer = new byte[256];
    try
    {
        while (socket.State == WebSocketState.Open)
        {
            var result = await socket.ReceiveAsync(buffer, CancellationToken.None);
            if (result.MessageType == WebSocketMessageType.Close) break;
        }
    }
    catch (WebSocketException) { /* uzilish — normal holat */ }

    hub.Prune();
}).RequireAuthorization();
```

- [ ] **Step 3: Qo'lda tekshirish**

Serverni ishga tushiring. Ikkita qurilma ulang (ikkita `accessToken`).
Birinchisi bilan WebSocket'ga ulaning:

```bash
npx wscat -c "ws://localhost:5000/ws/notify?access_token=BIRINCHI_TOKEN"
```

Ikkinchi token bilan push qiling:

```bash
curl -s -X POST http://localhost:5000/api/v1/sync/push \
  -H "Authorization: Bearer IKKINCHI_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"records":[]}'
```

Bo'sh push signal bermaydi (kutilgan). Haqiqiy yozuv bilan push qilinganda
`wscat` oynasida `{"type":"changes","seq":N}` ko'rinishi kerak.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat: add WebSocket change notifications

Plain WebSocket rather than SignalR: the payload is a single {seq} hint to
pull now, and there is no maintained C++ SignalR client for the desktop
agent. Tokens arrive via query string because browsers cannot set headers
on a WebSocket handshake."
```

---

## Task 7: `.cmx` almashuv formati

**Files:**
- Create: `src/CustomSync.Core/Interchange/CmxManifest.cs`
- Create: `src/CustomSync.Core/Interchange/CmxWriter.cs`
- Create: `src/CustomSync.Core/Interchange/CmxReader.cs`
- Create: `src/CustomSync.Services/InterchangeService.cs`
- Create: `src/CustomSync.Api/Endpoints/InterchangeEndpoints.cs`
- Create: `tests/CustomSync.Tests/InterchangeTests.cs`

- [ ] **Step 1: Yiqiladigan testni yozish**

Muhim xossa: import **push bilan bir xil natija** berishi kerak.

`tests/CustomSync.Tests/InterchangeTests.cs`:

```csharp
using CustomSync.Core;
using CustomSync.Core.Contracts;
using CustomSync.Core.Interchange;
using CustomSync.Services;
using CustomSync.Tests.Fixtures;
using Xunit;

namespace CustomSync.Tests;

public class InterchangeTests : IClassFixture<DatabaseFixture>
{
    private readonly DatabaseFixture _fixture;

    public InterchangeTests(DatabaseFixture fixture) => _fixture = fixture;

    private static SyncRecord Make(int index) => new()
    {
        RecordId   = RecordId.Compute(RecordKind.Deleted, "peerX", index, 1753900000),
        Kind       = RecordKind.Deleted,
        PeerHash   = "peerX",
        MsgId      = index,
        OccurredAt = 1753900000,
        ObservedAt = 1753900001,
        DeviceId   = "exporter",
        Nonce      = new byte[12],
        Payload    = [7, 7, 7]
    };

    [Fact]
    public async Task Roundtrip_preserves_every_record_field()
    {
        var original = Enumerable.Range(0, 5).Select(Make).ToList();
        using var stream = new MemoryStream();

        await CmxWriter.WriteAsync(stream, new CmxManifest
        {
            FormatVersion = 1,
            SourceApp     = "test",
            DeviceId      = "exporter",
            CreatedAt     = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
            RecordCount   = original.Count,
            Encrypted     = true,
            KeyFingerprint = "abcd1234"
        }, original, media: []);

        stream.Position = 0;
        var (manifest, records, _) = await CmxReader.ReadAsync(stream);

        Assert.Equal(5, manifest.RecordCount);
        Assert.Equal(original.Select(r => r.RecordId), records.Select(r => r.RecordId));
        Assert.Equal(original[0].Payload, records[0].Payload);
        Assert.Equal(original[0].ObservedAt, records[0].ObservedAt);
    }

    [Fact]
    public async Task Import_produces_the_same_state_as_push()
    {
        var records = Enumerable.Range(10, 5).Select(Make).ToList();

        await using var pushDb = _fixture.CreateContext();
        await new SyncService(pushDb).PushAsync("exporter", records);
        var afterPush = await new RecordQueryService(pushDb).QueryAsync(
            new RecordQuery { Snapshot = long.MaxValue, Limit = 100 });

        // Xuddi shu yozuvlarni ikkinchi marta, endi .cmx orqali kiritamiz.
        using var stream = new MemoryStream();
        await CmxWriter.WriteAsync(stream, new CmxManifest
        {
            FormatVersion = 1, SourceApp = "test", DeviceId = "exporter",
            CreatedAt = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
            RecordCount = records.Count, Encrypted = true,
            KeyFingerprint = "abcd1234"
        }, records, media: []);
        stream.Position = 0;

        await using var importDb = _fixture.CreateContext();
        var imported = await new InterchangeService(
            new SyncService(importDb),
            new MediaService(importDb, Path.GetTempPath()))
            .ImportAsync(stream, "importer");

        await using var checkDb = _fixture.CreateContext();
        var afterImport = await new RecordQueryService(checkDb).QueryAsync(
            new RecordQuery { Snapshot = long.MaxValue, Limit = 100 });

        Assert.Equal(records.Count, imported.Count);
        Assert.Equal(afterPush.Count, afterImport.Count);
        Assert.All(imported, r => Assert.Equal(PushOutcome.Duplicate, r.Status));
    }
}
```

- [ ] **Step 2: Testni ishga tushirib, yiqilishini ko'rish**

```bash
dotnet test --filter InterchangeTests
```

Kutilgan: FAIL — `CmxWriter` mavjud emas.

- [ ] **Step 3: Manifest va writer/reader'ni yozish**

`src/CustomSync.Core/Interchange/CmxManifest.cs`:

```csharp
namespace CustomSync.Core.Interchange;

public sealed record CmxManifest
{
    public required int    FormatVersion  { get; init; }
    public required string SourceApp      { get; init; }
    public required string DeviceId       { get; init; }
    public required long   CreatedAt      { get; init; }
    public required int    RecordCount    { get; init; }
    public required bool   Encrypted      { get; init; }

    /// <summary>
    /// SHA256("customsync-fingerprint-v1" ‖ master_key)[0..8], hex.
    /// Import qilayotgan qurilma kalit mos kelishini OLDINDAN tekshiradi —
    /// aks holda foydalanuvchi yuzlab deshifrlash xatosini ko'radi.
    /// </summary>
    public required string KeyFingerprint { get; init; }

    public long?  ScopeSince { get; init; }
    public long?  ScopeUntil { get; init; }
    public string? ScopePeerHash { get; init; }
}
```

`src/CustomSync.Core/Interchange/CmxWriter.cs`:

```csharp
using System.IO.Compression;
using System.Text;
using System.Text.Json;
using CustomSync.Core.Contracts;

namespace CustomSync.Core.Interchange;

/// <summary>
/// .cmx — oddiy ZIP. records.jsonl satrlari HTTP sync payload bilan
/// AYNAN bir xil shaklga ega, shuning uchun import push bilan bir xil
/// kodni bosadi va alohida sinovdan o'tkazilishi shart emas.
/// </summary>
public static class CmxWriter
{
    private static readonly JsonSerializerOptions Json = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public static async Task WriteAsync(
        Stream output,
        CmxManifest manifest,
        IReadOnlyList<SyncRecord> records,
        IReadOnlyDictionary<string, byte[]> media,
        CancellationToken ct = default)
    {
        using var zip = new ZipArchive(output, ZipArchiveMode.Create, leaveOpen: true);

        var manifestEntry = zip.CreateEntry("manifest.json");
        await using (var stream = manifestEntry.Open())
            await JsonSerializer.SerializeAsync(stream, manifest, Json, ct);

        var recordsEntry = zip.CreateEntry("records.jsonl");
        await using (var stream = recordsEntry.Open())
        await using (var writer = new StreamWriter(stream, Encoding.UTF8))
        {
            foreach (var record in records)
                await writer.WriteLineAsync(JsonSerializer.Serialize(record, Json));
        }

        foreach (var (hash, content) in media)
        {
            var entry = zip.CreateEntry($"media/{hash}");
            await using var stream = entry.Open();
            await stream.WriteAsync(content, ct);
        }
    }
}
```

`src/CustomSync.Core/Interchange/CmxReader.cs`:

```csharp
using System.IO.Compression;
using System.Text.Json;
using CustomSync.Core.Contracts;

namespace CustomSync.Core.Interchange;

public static class CmxReader
{
    private static readonly JsonSerializerOptions Json = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public const int SupportedFormatVersion = 1;

    public static async Task<(
        CmxManifest Manifest,
        IReadOnlyList<SyncRecord> Records,
        IReadOnlyDictionary<string, byte[]> Media)> ReadAsync(
            Stream input, CancellationToken ct = default)
    {
        using var zip = new ZipArchive(input, ZipArchiveMode.Read, leaveOpen: true);

        var manifestEntry = zip.GetEntry("manifest.json")
            ?? throw new InvalidDataException("manifest.json topilmadi");

        CmxManifest manifest;
        await using (var stream = manifestEntry.Open())
            manifest = await JsonSerializer.DeserializeAsync<CmxManifest>(stream, Json, ct)
                ?? throw new InvalidDataException("manifest.json o'qib bo'lmadi");

        if (manifest.FormatVersion > SupportedFormatVersion)
            throw new InvalidDataException(
                $"Format versiyasi {manifest.FormatVersion} qo'llab-quvvatlanmaydi " +
                $"(maksimal {SupportedFormatVersion}). Serverni yangilang.");

        var records = new List<SyncRecord>(manifest.RecordCount);
        var recordsEntry = zip.GetEntry("records.jsonl")
            ?? throw new InvalidDataException("records.jsonl topilmadi");

        await using (var stream = recordsEntry.Open())
        using (var reader = new StreamReader(stream))
        {
            while (await reader.ReadLineAsync(ct) is { } line)
            {
                if (string.IsNullOrWhiteSpace(line)) continue;
                var record = JsonSerializer.Deserialize<SyncRecord>(line, Json);
                if (record is not null) records.Add(record);
            }
        }

        var media = new Dictionary<string, byte[]>();
        foreach (var entry in zip.Entries.Where(e => e.FullName.StartsWith("media/")))
        {
            await using var stream = entry.Open();
            using var buffer = new MemoryStream();
            await stream.CopyToAsync(buffer, ct);
            media[entry.Name] = buffer.ToArray();
        }

        return (manifest, records, media);
    }
}
```

- [ ] **Step 4: `InterchangeService` ni yozish**

`src/CustomSync.Services/InterchangeService.cs`:

```csharp
using CustomSync.Core.Contracts;
using CustomSync.Core.Interchange;

namespace CustomSync.Services;

/// <summary>
/// Import ATAYLAB SyncService.PushAsync ni qayta ishlatadi. Bu yerda
/// alohida merge logikasi yo'q — bo'lsa, u sync yo'lidan chetga chiqib
/// ketishi va ikkalasi turlicha xatti-harakat qilishi muqarrar edi.
/// </summary>
public class InterchangeService(SyncService sync, MediaService media)
{
    public async Task<IReadOnlyList<PushResult>> ImportAsync(
        Stream cmx, string importingDeviceId, CancellationToken ct = default)
    {
        var (manifest, records, blobs) = await CmxReader.ReadAsync(cmx, ct);

        // Media avval — yozuvlar unga havola qiladi.
        foreach (var (hash, content) in blobs)
            await media.StoreAsync(hash, content, new byte[12], ct);

        // Yozuvning o'z device_id si emas, import qilayotgan qurilma
        // yoziladi: kim import qilgani audit uchun muhim.
        return await sync.PushAsync(importingDeviceId, records, ct);
    }
}
```

- [ ] **Step 5: Testni qayta ishga tushirish**

```bash
dotnet test --filter InterchangeTests
```

Kutilgan: PASS — 2 ta test o'tdi.

- [ ] **Step 6: Eksport endpoint'ini yozish**

`src/CustomSync.Api/Endpoints/InterchangeEndpoints.cs`:

```csharp
using System.Security.Claims;
using CustomSync.Core.Contracts;
using CustomSync.Core.Interchange;
using CustomSync.Services;

namespace CustomSync.Api.Endpoints;

public static class InterchangeEndpoints
{
    public static void MapInterchangeEndpoints(this WebApplication app)
    {
        var group = app.MapGroup("/api/v1").RequireAuthorization();

        group.MapPost("/import", async (
            HttpRequest request, ClaimsPrincipal user, InterchangeService interchange) =>
        {
            var deviceId = user.FindFirstValue(ClaimTypes.NameIdentifier)!;
            using var buffer = new MemoryStream();
            await request.Body.CopyToAsync(buffer);
            buffer.Position = 0;

            try
            {
                var results = await interchange.ImportAsync(buffer, deviceId);
                return Results.Ok(new
                {
                    imported   = results.Count,
                    created    = results.Count(r => r.Status == PushOutcome.Created),
                    duplicates = results.Count(r => r.Status == PushOutcome.Duplicate),
                    errors     = results.Count(r => r.Status == PushOutcome.Error)
                });
            }
            catch (InvalidDataException ex)
            {
                return Results.BadRequest(new { error = "invalid_cmx", message = ex.Message });
            }
        }).DisableAntiforgery();

        group.MapGet("/export", async (
            long? since, long? until, string? peerHash,
            ClaimsPrincipal user, RecordQueryService query, SyncService sync) =>
        {
            var deviceId = user.FindFirstValue(ClaimTypes.NameIdentifier)!;
            var snapshot = await query.CurrentSnapshotAsync();

            // Eksport pull bilan bir xil yo'ldan boradi: bitta manba,
            // bitta xatti-harakat.
            var page = await sync.PullAsync(0, int.MaxValue);
            var records = page.Records
                .Where(r => since is null || r.OccurredAt >= since)
                .Where(r => until is null || r.OccurredAt <= until)
                .Where(r => peerHash is null || r.PeerHash == peerHash)
                .Select(r => new SyncRecord
                {
                    RecordId   = r.RecordId,
                    Kind       = r.Kind,
                    PeerHash   = r.PeerHash,
                    MsgId      = r.MsgId,
                    OccurredAt = r.OccurredAt,
                    ObservedAt = r.ObservedAt,
                    DeviceId   = r.DeviceId,
                    Nonce      = r.Nonce,
                    Payload    = r.Payload
                })
                .ToList();

            var output = new MemoryStream();
            await CmxWriter.WriteAsync(output, new CmxManifest
            {
                FormatVersion  = CmxReader.SupportedFormatVersion,
                SourceApp      = "server-backend",
                DeviceId       = deviceId,
                CreatedAt      = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
                RecordCount    = records.Count,
                Encrypted      = true,
                KeyFingerprint = "server-unknown",
                ScopeSince     = since,
                ScopeUntil     = until,
                ScopePeerHash  = peerHash
            }, records, media: new Dictionary<string, byte[]>());

            output.Position = 0;
            var name = $"customsync-{DateTime.UtcNow:yyyyMMdd-HHmmss}.cmx";
            return Results.File(output, "application/zip", name);
        });
    }
}
```

**Eslatma:** `KeyFingerprint = "server-unknown"` — server master kalitni
bilmaydi, shuning uchun barmoq izini hisoblay olmaydi. Import qilayotgan
klient buni ko'rib, tekshiruvni o'tkazib yuboradi va deshifrlashda
xatolik chiqsa aniq xabar beradi. Klient tomondan qilingan eksportlarda
esa haqiqiy barmoq izi bo'ladi (Plan 02).

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat: add .cmx interchange import and export

Import deliberately routes through SyncService.PushAsync rather than
having its own merge path: a second implementation would inevitably drift
from the sync one, and the two would start behaving differently on the
same input. The test asserts import lands in exactly the state push does."
```

---

## Task 8: Platformalararo test vektorlari

Beshala platforma bir xil natija berishini kafolatlaydigan yagona narsa.
Interop buzilishi jimgina bo'ladi va uni topish juda qiyin — shuning
uchun bu birinchi kundan mavjud bo'lishi kerak.

**Files:**
- Create: `tools/GenerateTestVectors/Program.cs`
- Create: `docs/sync-protocol/test-vectors.json` (generatsiya qilinadi)
- Create: `tests/CustomSync.Tests/TestVectorTests.cs`

- [ ] **Step 1: Generatorni yozish**

```bash
dotnet new console -n GenerateTestVectors -o tools/GenerateTestVectors
dotnet sln add tools/GenerateTestVectors
dotnet add tools/GenerateTestVectors reference src/CustomSync.Core
```

`tools/GenerateTestVectors/Program.cs`:

```csharp
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using CustomSync.Core;

// Referens qiymatlarni C# implementatsiyasidan chiqaradi. Qolgan
// platformalar (C++, TypeScript, Kotlin, Swift) shu faylga qarab
// o'zini tekshiradi — hech kim qiymatlarni qo'lda yozmaydi.

var recordIdCases = new[]
{
    new { kind = "deleted",  peer_hash = "a3f9c2", msg_id = 12345L, occurred_at = 1753800000L },
    new { kind = "edited",   peer_hash = "a3f9c2", msg_id = 12345L, occurred_at = 1753800000L },
    new { kind = "activity", peer_hash = "0011ff", msg_id = 0L,     occurred_at = 1753800000L },
    new { kind = "deleted",  peer_hash = "",       msg_id = 0L,     occurred_at = 0L },
    new { kind = "ab",       peer_hash = "c",      msg_id = 1L,     occurred_at = 1L },
    new { kind = "a",        peer_hash = "bc",     msg_id = 1L,     occurred_at = 1L },
};

var recordIds = recordIdCases.Select(c => new
{
    input = c,
    expected = RecordId.Compute(c.kind, c.peer_hash, c.msg_id, c.occurred_at)
}).ToArray();

var pbkdf2Cases = new[]
{
    new { password = "correct horse battery staple", salt_hex = "000102030405060708090a0b0c0d0e0f", iterations = 600_000 },
    new { password = "P@ssw0rd-8chars!",             salt_hex = "0f0e0d0c0b0a09080706050403020100", iterations = 2_000_000 },
};

var pbkdf2 = pbkdf2Cases.Select(c => new
{
    input = c,
    expected_key_hex = Convert.ToHexString(
        Rfc2898DeriveBytes.Pbkdf2(
            Encoding.UTF8.GetBytes(c.password),
            Convert.FromHexString(c.salt_hex),
            c.iterations,
            HashAlgorithmName.SHA256,
            32)).ToLowerInvariant()
}).ToArray();

var hkdfCases = new[] { "customsync-content-v1", "customsync-media-v1", "customsync-peer-v1" };
var masterKey = Convert.FromHexString(
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");

var hkdf = hkdfCases.Select(info => new
{
    input = new { master_key_hex = Convert.ToHexString(masterKey).ToLowerInvariant(), info },
    expected_key_hex = Convert.ToHexString(
        HKDF.DeriveKey(HashAlgorithmName.SHA256, masterKey, 32,
            salt: null, info: Encoding.UTF8.GetBytes(info))).ToLowerInvariant()
}).ToArray();

var document = new
{
    version = 1,
    note = "Generated by tools/GenerateTestVectors. Every platform must reproduce these exactly.",
    record_id = recordIds,
    pbkdf2_hmac_sha256 = pbkdf2,
    hkdf_sha256 = hkdf
};

var path = Path.Combine("docs", "sync-protocol", "test-vectors.json");
Directory.CreateDirectory(Path.GetDirectoryName(path)!);
await File.WriteAllTextAsync(path,
    JsonSerializer.Serialize(document, new JsonSerializerOptions { WriteIndented = true }));

Console.WriteLine($"Wrote {path}");
```

- [ ] **Step 2: Vektorlarni generatsiya qilish**

```bash
dotnet run --project tools/GenerateTestVectors
```

Kutilgan: `Wrote docs/sync-protocol/test-vectors.json`

**Diqqat:** 2 000 000 iteratsiyali PBKDF2 bir necha soniya olishi mumkin —
bu normal.

- [ ] **Step 3: Vektorlarga qarshi testni yozish**

`tests/CustomSync.Tests/TestVectorTests.cs`:

```csharp
using System.Text.Json;
using CustomSync.Core;
using Xunit;

namespace CustomSync.Tests;

public class TestVectorTests
{
    private static JsonDocument Load()
    {
        var path = Path.Combine(
            AppContext.BaseDirectory, "..", "..", "..", "..", "..",
            "docs", "sync-protocol", "test-vectors.json");
        return JsonDocument.Parse(File.ReadAllText(Path.GetFullPath(path)));
    }

    [Fact]
    public void RecordId_matches_every_published_vector()
    {
        using var doc = Load();

        foreach (var entry in doc.RootElement.GetProperty("record_id").EnumerateArray())
        {
            var input = entry.GetProperty("input");
            var actual = RecordId.Compute(
                input.GetProperty("kind").GetString()!,
                input.GetProperty("peer_hash").GetString()!,
                input.GetProperty("msg_id").GetInt64(),
                input.GetProperty("occurred_at").GetInt64());

            Assert.Equal(entry.GetProperty("expected").GetString(), actual);
        }
    }
}
```

- [ ] **Step 4: Testni ishga tushirish**

```bash
dotnet test --filter TestVectorTests
```

Kutilgan: PASS.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat: publish cross-platform crypto and record-id test vectors

Interop breakage between five platforms fails silently -- a different
base64 padding or byte order produces valid-looking data that simply never
dedups. The vectors are generated from the C# reference implementation and
committed, so C++, TypeScript, Kotlin and Swift each assert against the
same fixed expectations rather than against prose."
```

---

## Task 9: Deployment

**Files:**
- Create: `deploy/customsync.service`
- Create: `deploy/nginx-customsync.conf`
- Create: `deploy/README.md`

- [ ] **Step 1: systemd unit**

`deploy/customsync.service`:

```ini
[Unit]
Description=CustomSync backend
After=network.target postgresql.service
Requires=postgresql.service

[Service]
Type=notify
User=customsync
Group=customsync
WorkingDirectory=/var/www/customsync
ExecStart=/usr/bin/dotnet /var/www/customsync/CustomSync.Api.dll
Restart=always
RestartSec=5
Environment=ASPNETCORE_ENVIRONMENT=Production
Environment=ASPNETCORE_URLS=http://127.0.0.1:5080

# Resurs cheklovlari — bitta jarayon butun VPS ni yeb qo'ymasligi uchun.
MemoryMax=1G
CPUQuota=150%

# Qattiqlashtirish
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/customsync /var/www/customsync/logs

[Install]
WantedBy=multi-user.target
```

- [ ] **Step 2: Nginx konfiguratsiyasi**

`deploy/nginx-customsync.conf`:

```nginx
server {
    listen 443 ssl http2;
    server_name sync.example.uz;

    ssl_certificate     /etc/letsencrypt/live/sync.example.uz/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/sync.example.uz/privkey.pem;

    # Media yuklash uchun (media.max_upload_bytes bilan mos bo'lishi kerak).
    client_max_body_size 50m;

    location / {
        proxy_pass http://127.0.0.1:5080;
        proxy_http_version 1.1;
        proxy_set_header Host              $host;
        proxy_set_header X-Real-IP         $remote_addr;
        proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # WebSocket upgrade — busiz /ws/notify ishlamaydi.
        proxy_set_header Upgrade    $http_upgrade;
        proxy_set_header Connection "upgrade";

        # WebSocket ulanishi uzoq turadi; standart 60s uni uzib qo'yadi.
        proxy_read_timeout 3600s;
    }
}

server {
    listen 80;
    server_name sync.example.uz;
    return 301 https://$host$request_uri;
}
```

- [ ] **Step 3: Deploy hujjatini yozish**

`deploy/README.md` ga quyidagi qadamlarni yozing:

```markdown
# Deployment

## Birinchi o'rnatish

1. Foydalanuvchi va kataloglar:
   sudo useradd -r -s /bin/false customsync
   sudo mkdir -p /var/www/customsync /var/lib/customsync/media
   sudo chown -R customsync:customsync /var/lib/customsync

2. PostgreSQL:
   sudo -u postgres psql -c "CREATE USER customsync WITH PASSWORD '<parol>';"
   sudo -u postgres psql -c "CREATE DATABASE customsync OWNER customsync;"

3. Publish va nusxalash:
   dotnet publish src/CustomSync.Api -c Release -o ./publish
   rsync -av ./publish/ user@vps:/var/www/customsync/

4. Sozlash: /var/www/customsync/appsettings.Production.json da
   connection string va Jwt:SigningKey ni o'rnating (kalit kamida
   32 bayt tasodifiy bo'lsin).

5. Xizmatlar:
   sudo cp deploy/customsync.service /etc/systemd/system/
   sudo systemctl daemon-reload && sudo systemctl enable --now customsync
   sudo cp deploy/nginx-customsync.conf /etc/nginx/sites-available/customsync
   sudo ln -s /etc/nginx/sites-available/customsync /etc/nginx/sites-enabled/
   sudo certbot --nginx -d sync.example.uz
   sudo nginx -t && sudo systemctl reload nginx

6. Firewall:
   sudo ufw allow 22,80,443/tcp && sudo ufw enable

7. Birinchi qurilma kodi:
   sudo -u customsync dotnet /var/www/customsync/CustomSync.Api.dll --create-enrollment-code

## Zaxira

Kechasi 03:00 da (cron):
   pg_dump -U customsync customsync | gzip > /backup/db-$(date +%F).sql.gz
   rsync -a /var/lib/customsync/media/ /backup/media/

MUHIM: /backup VPS ning O'ZIDA bo'lmasligi kerak — VPS yo'qolsa zaxira
ham yo'qoladi. Uni boshqa xostga ko'chiring (Plan 04 buni avtomatlashtiradi).
```

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "chore: add systemd, nginx and deployment documentation

The systemd unit sets MemoryMax and CPUQuota so a runaway backend cannot
starve the rest of the VPS, and nginx needs an explicit long read timeout
or it severs the notification WebSocket after 60 seconds."
```

---

## Qabul qilish mezonlari (1b)

1. `dotnet test` — barcha testlar o'tadi (1a dagi 14 + bu yerdagi 11 = 25).
2. `Concurrent_pushes_are_never_skipped_by_a_polling_reader` testi
   `BIGSERIAL` ga qaytarilganda **yiqiladi** (bug haqiqiyligi isbotlangan).
3. Push idempotent: bir xil to'plamni ikki marta yuborish — birinchisida
   `created`, ikkinchisida `duplicate`, jadval hajmi o'zgarmaydi.
4. Yaxshiroq kuzatuv (kichikroq `observed_at`) mavjud yozuvni almashtiradi
   va unga **yangi `seq`** beriladi.
5. Sahifalash davomida yangi yozuv kelsa — dublikat ham, tushib qolgan
   qator ham yo'q.
6. `.cmx` eksport qilinadi → import qilinadi → barcha yozuvlar
   `duplicate` bo'ladi (holat o'zgarmaydi).
7. Ikkinchi qurilma push qilganda birinchi qurilmaning WebSocket'iga
   `{"type":"changes","seq":N}` keladi.
8. `docs/sync-protocol/test-vectors.json` mavjud va `TestVectorTests` o'tadi.

---

## Keyingi qadam

Plan 02 — tdesktop sync agenti. U shu yerda qurilgan API'ga ulanadi va
`test-vectors.json` ga qarshi o'zining crypto implementatsiyasini
tekshiradi.
