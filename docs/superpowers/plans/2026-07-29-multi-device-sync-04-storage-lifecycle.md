# Storage Lifecycle Manager Implementation Plan (04)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Serverdagi ma'lumot hajmini doimiy nazorat qilish, sozlanadigan retention siyosatlari bo'yicha eskirgan ma'lumotni xavfsiz joyga arxivlash va faqat **tasdiqlangan zaxiradan keyin** VPS'dan o'chirish. Avtomatik ham, qo'lda ham.

**Architecture:** Pluggable `IArchiveTarget` interfeysi — to'rtta implementatsiya (qo'lda yuklab olish, S3-mos, SFTP, Telegram bot). O'chirish **ikki fazali**: avval arxivlanadi va tekshiriladi, keyin o'chiriladi. Tekshiruvsiz o'chirish umuman mumkin emas.

**Tech Stack:** .NET 8, `AWSSDK.S3` (S3-mos uchun), `SSH.NET` (SFTP uchun), Telegram Bot API (HTTP).

**Kirish sharti:** [01b](2026-07-29-multi-device-sync-01b-backend-sync.md) va [03](2026-07-29-multi-device-sync-03-web-controller.md) tugagan.

**Umumiy qoidalar:** [00-index](2026-07-29-multi-device-sync-00-index.md) dagi K1–K7.

---

> ## ⚠️ REVIZIYA 2026-08-25 — bu planga tegishli o'zgarishlar
>
> To'liq ro'yxat: spec **§0**.
>
> ### Task 2 (retention siyosatlari) — MIJOZ BILAN MOSLASHTIRISH
>
> 🔴 Bu planning eng muhim o'zgarishi.
>
> 2026-08-24 da tdesktop'ga **30 kunlik** activity tozalash qo'shildi.
> Serverda retention undan **UZUNROQ** bo'lishi shart (standart 90
> kun) — server markaziy arxiv, mijoz esa faqat yaqin tarixni
> ushlaydi.
>
> Agar server mijozdan QISQAROQ saqlasa, mijozdagi ma'lumot
> serverdan yo'qolib, keyin boshqa qurilmaga ham yetib bormaydi.
>
> 🔴 **Serverdagi retention hech qachon `tombstone` YARATMAYDI.**
> Tombstone — foydalanuvchining ataylab o'chirishi (global).
> Retention — lokal xotira boshqaruvi. Ikkalasi aralashtirilsa,
> serverdagi tozalash barcha qurilmalardagi arxivni kesib tashlardi.
>
> Sozlamalar (`SettingsService`, plan 01a Task 4):
> `retention.activity_days` = 90, `retention.<kind>_days` = 0 (cheksiz).
>
> ### Task 1 va 6 — kvota bilan bog'lanish
> Spec §0.9: kvota (`storage.quota_*`) va retention birga ishlaydi.
> Kvota to'lganda avval retention ishlaydi, keyin arxivlash
> (Task 3-5), oxirida `PUT /media` 507 qaytaradi.
>
> ### `media_index` va xotira prognozi (Task 1)
> Endi har media fayl uchun indeks yozuvi bor (spec §0.4), ya'ni
> prognoz aniqroq bo'ladi: `size` yig'indisi `status='present'`
> bo'yicha. `pending` yozuvlar — kelajakda kelishi mumkin bo'lgan
> hajm, ular alohida ko'rsatiladi.

---

## Nima uchun bu plan capture service'dan OLDIN

Always-on capture service (Plan 05) 24/7 hamma narsani yozadi. Bu
ma'lumot oqimini ochishdan **oldin** xotira boshqaruvi tayyor turishi
kerak — aks holda disk birinchi haftadayoq to'lib qolishi mumkin va
muammoni shoshilinch hal qilishga to'g'ri keladi.

---

## Xavfsizlik tamoyili

> **Tekshirilmagan zaxiradan keyin hech qachon o'chirilmaydi.**

O'chirish oqimi qat'iy:

```
1. Qamrov tanlanadi        →  dry-run: nima o'chadi, qancha joy bo'shaydi
2. Arxiv yaratiladi        →  .cmx.enc (shifrlangan)
3. Target'ga yuboriladi    →  S3 / SFTP / Telegram / yuklab olish
4. TEKSHIRILADI            →  qayta o'qib SHA-256 solishtiriladi
5. Faqat shundan keyin     →  bazadan va diskdan o'chiriladi
6. Audit log'ga yoziladi   →  qayerga arxivlangani bilan
```

4-qadam muvaffaqiyatsiz bo'lsa — **hech narsa o'chirilmaydi** va xato
web app'da ko'rsatiladi.

---

## File Structure

```
src/CustomSync.Services/Storage/
├── StorageMetricsService.cs      # hajm, o'sish tezligi, prognoz
├── RetentionPolicy.cs            # siyosat modeli va baholash
├── PurgeService.cs               # ikki fazali xavfsiz o'chirish
├── ArchiveJobService.cs          # rejalashtirilgan ishlar
└── Targets/
    ├── IArchiveTarget.cs
    ├── ManualDownloadTarget.cs
    ├── S3ArchiveTarget.cs
    ├── SftpArchiveTarget.cs
    └── TelegramBotArchiveTarget.cs

src/CustomSync.Api/Endpoints/StorageEndpoints.cs
src/CustomSync.Data/Entities/
├── ArchiveRunEntity.cs           # har bir arxivlash urinishi tarixi
└── RetentionPolicyEntity.cs

web/src/views/StorageView.vue     # kengaytiriladi
web/src/views/RetentionView.vue   # yangi
```

---

## Task 1: Xotira metrikasi va o'sish prognozi

**Files:**
- Create: `src/CustomSync.Services/Storage/StorageMetricsService.cs`
- Create: `tests/CustomSync.Tests/StorageMetricsTests.cs`

- [ ] **Step 1: Yiqiladigan testni yozish**

```csharp
using CustomSync.Services.Storage;
using CustomSync.Tests.Fixtures;
using Xunit;

namespace CustomSync.Tests;

public class StorageMetricsTests : IClassFixture<DatabaseFixture>
{
    private readonly DatabaseFixture _fixture;

    public StorageMetricsTests(DatabaseFixture fixture) => _fixture = fixture;

    [Fact]
    public async Task Empty_database_reports_zero_without_dividing_by_zero()
    {
        await using var db = _fixture.CreateContext();
        var metrics = new StorageMetricsService(db);

        var summary = await metrics.SummaryAsync();

        Assert.Equal(0, summary.TotalBytes);
        Assert.Equal(0, summary.DailyGrowthBytes);
        Assert.Null(summary.DaysUntilFull);
    }

    [Fact]
    public async Task Growth_projection_is_null_when_growth_is_zero()
    {
        await using var db = _fixture.CreateContext();
        var metrics = new StorageMetricsService(db);

        // O'sish nol bo'lganda "necha kunda to'ladi" savolining javobi yo'q,
        // 0 yoki cheksizlik emas — null. Aks holda UI "0 kun qoldi" deb
        // noto'g'ri ogohlantiradi.
        var summary = await metrics.SummaryAsync(diskCapacityBytes: 1_000_000);

        Assert.Null(summary.DaysUntilFull);
    }
}
```

- [ ] **Step 2: Testni ishga tushirib, yiqilishini ko'rish**

```bash
dotnet test --filter StorageMetricsTests
```

Kutilgan: FAIL.

- [ ] **Step 3: Servisni yozish**

```csharp
using CustomSync.Data;
using Microsoft.EntityFrameworkCore;

namespace CustomSync.Services.Storage;

public sealed record StorageSummary
{
    public required long TotalBytes        { get; init; }
    public required long RecordBytes       { get; init; }
    public required long MediaBytes        { get; init; }
    public required long RecordCount       { get; init; }
    public required long MediaCount        { get; init; }
    public required long DailyGrowthBytes  { get; init; }

    /// <summary>
    /// Disk to'lgunicha necha kun qolgani. O'sish nol bo'lsa yoki disk
    /// hajmi berilmagan bo'lsa null — "0 kun" emas, chunki bu UI'da
    /// shoshilinch ogohlantirish sifatida ko'rinardi.
    /// </summary>
    public int? DaysUntilFull { get; init; }
}

public class StorageMetricsService(SyncDbContext db)
{
    public async Task<StorageSummary> SummaryAsync(
        long? diskCapacityBytes = null, CancellationToken ct = default)
    {
        var recordCount = await db.Records.LongCountAsync(ct);
        var recordBytes = recordCount == 0 ? 0
            : await db.Records.SumAsync(r => (long)r.PayloadSize, ct);
        var mediaCount = await db.MediaBlobs.LongCountAsync(ct);
        var mediaBytes = mediaCount == 0 ? 0
            : await db.MediaBlobs.SumAsync(m => m.Size, ct);

        var growth = await DailyGrowthAsync(ct);
        var total = recordBytes + mediaBytes;

        int? daysLeft = null;
        if (diskCapacityBytes is > 0 && growth > 0)
        {
            var remaining = diskCapacityBytes.Value - total;
            daysLeft = remaining <= 0 ? 0 : (int)(remaining / growth);
        }

        return new StorageSummary
        {
            TotalBytes       = total,
            RecordBytes      = recordBytes,
            MediaBytes       = mediaBytes,
            RecordCount      = recordCount,
            MediaCount       = mediaCount,
            DailyGrowthBytes = growth,
            DaysUntilFull    = daysLeft
        };
    }

    /// <summary>
    /// So'nggi 7 kunning o'rtacha kunlik o'sishi. Qisqaroq oyna
    /// tasodifiy kunlarga juda sezgir, uzunroq oyna esa yaqinda
    /// boshlangan tez o'sishni yashiradi.
    /// </summary>
    private async Task<long> DailyGrowthAsync(CancellationToken ct)
    {
        var cutoff = DateTime.UtcNow.AddDays(-7);
        var recent = await db.Records
            .Where(r => r.ReceivedAt >= cutoff)
            .SumAsync(r => (long?)r.PayloadSize, ct) ?? 0;
        return recent / 7;
    }

    public async Task<IReadOnlyList<(string PeerHash, long Bytes, int Count)>>
        ByPeerAsync(int limit = 50, CancellationToken ct = default)
        => await db.Records
            .GroupBy(r => r.PeerHash)
            .Select(g => new ValueTuple<string, long, int>(
                g.Key, g.Sum(r => (long)r.PayloadSize), g.Count()))
            .OrderByDescending(t => t.Item2)
            .Take(limit)
            .ToListAsync(ct);
}
```

- [ ] **Step 4: Testni qayta ishga tushirish**

```bash
dotnet test --filter StorageMetricsTests
```

Kutilgan: PASS.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat: add storage metrics with growth projection

DaysUntilFull is null rather than zero when growth is zero: zero would
render in the panel as an urgent 'disk full today' warning on a server
that is not growing at all."
```

---

## Task 2: Retention siyosatlari

**Files:**
- Create: `src/CustomSync.Data/Entities/RetentionPolicyEntity.cs`
- Create: `src/CustomSync.Services/Storage/RetentionPolicy.cs`

- [ ] **Step 1: Siyosat modelini yozish**

```csharp
namespace CustomSync.Data.Entities;

/// <summary>
/// Retention siyosati — qanday ma'lumot qancha vaqtdan keyin nima
/// bo'lishi. Web app'dan to'liq tahrirlanadi (qoida K1): yangi siyosat
/// qo'shish uchun kodga tegilmaydi.
/// </summary>
public class RetentionPolicyEntity
{
    public string  PolicyId    { get; set; } = null!;
    public string  Name        { get; set; } = null!;
    public bool    Enabled     { get; set; }

    /// <summary>Qaysi yozuvlarga tegishli: kind, yoki bo'sh = hammasi.</summary>
    public string? Kind        { get; set; }

    /// <summary>Faqat shu peer uchun; bo'sh = hammasi.</summary>
    public string? PeerHash    { get; set; }

    /// <summary>Faqat media biriktirilgan yozuvlar uchunmi.</summary>
    public bool    MediaOnly   { get; set; }

    /// <summary>Necha kundan eski yozuvlarga qo'llanadi.</summary>
    public int     OlderThanDays { get; set; }

    /// <summary>"archive_then_delete" | "delete_only" | "never_delete"</summary>
    public string  Action      { get; set; } = null!;

    /// <summary>archive_then_delete uchun target id.</summary>
    public string? TargetId    { get; set; }

    public int     Priority    { get; set; }
    public DateTime UpdatedAt  { get; set; }
}
```

- [ ] **Step 2: Baholash tartibini yozish**

```csharp
namespace CustomSync.Services.Storage;

public static class RetentionActions
{
    public const string ArchiveThenDelete = "archive_then_delete";
    public const string DeleteOnly        = "delete_only";

    /// <summary>
    /// Himoya siyosati — boshqa hamma narsadan ustun turadi. Muhim
    /// chatlarni tasodifan o'chirib yubormaslik uchun.
    /// </summary>
    public const string NeverDelete       = "never_delete";
}
```

Baholash qoidasi (mavjud `custom_settings.h` dagi
`ShouldAntiDelete` ustuvorlik naqshiga ergashadi):

```
1. never_delete mos keladigan siyosat bormi?  → hech narsa qilinmaydi
2. Eng yuqori Priority li mos siyosat          → uning Action i qo'llanadi
3. Mos siyosat yo'q                            → hech narsa qilinmaydi
```

**Standart holat — hech narsa o'chirilmaydi.** Foydalanuvchi aniq
siyosat yaratmaguncha ma'lumot tegilmaydi.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: add retention policy model with a protective default

Nothing is deleted until a policy explicitly says so, and never_delete
outranks every other policy regardless of priority so an important chat
cannot be caught by a broad rule added later."
```

---

## Task 3: Arxiv target interfeysi va qo'lda yuklab olish

**Files:**
- Create: `src/CustomSync.Services/Storage/Targets/IArchiveTarget.cs`
- Create: `src/CustomSync.Services/Storage/Targets/ManualDownloadTarget.cs`

- [ ] **Step 1: Interfeysni yozish**

```csharp
namespace CustomSync.Services.Storage.Targets;

public sealed record ArchiveUploadResult(
    bool Success,
    string? Location,      // target ichidagi manzil (kalit, yo'l, message_id)
    string? Sha256,        // target o'qib tasdiqlagan checksum
    string? Error);

/// <summary>
/// Arxiv manzili. Har bir implementatsiya ikki narsani bajara olishi
/// SHART: yozish va QAYTA O'QISH. Qayta o'qish tekshiruv uchun zarur —
/// tasdiqlanmagan zaxiradan keyin hech narsa o'chirilmaydi.
/// </summary>
public interface IArchiveTarget
{
    string TargetId { get; }
    string DisplayName { get; }

    /// <summary>Sozlamalar to'g'ri va manzil yetib bo'ladiganmi.</summary>
    Task<bool> HealthCheckAsync(CancellationToken ct = default);

    Task<ArchiveUploadResult> UploadAsync(
        string archiveName, Stream content, CancellationToken ct = default);

    /// <summary>
    /// Tekshiruv uchun qayta o'qish. null qaytarsa — tekshiruv
    /// muvaffaqiyatsiz va o'chirish BEKOR QILINADI.
    /// </summary>
    Task<Stream?> DownloadAsync(string location, CancellationToken ct = default);
}
```

- [ ] **Step 2: Qo'lda yuklab olish target'ini yozish**

Bu eng xavfsiz variant: fayl VPS'da vaqtinchalik saqlanadi, foydalanuvchi
uni yuklab oladi va web app'da **"Xavfsiz saqladim"** tugmasini bosadi —
faqat shundan keyin o'chirish ruxsat etiladi.

```csharp
namespace CustomSync.Services.Storage.Targets;

/// <summary>
/// Foydalanuvchi arxivni o'zi yuklab oladi va tasdiqlaydi.
///
/// Bu yerdagi "tekshiruv" — inson tasdig'i. Avtomatik target'larda
/// tekshiruv checksum solishtirish orqali bo'ladi; bu yerda esa
/// foydalanuvchi faylni haqiqatan xavfsiz joyga qo'yganini faqat o'zi
/// biladi, shuning uchun oshkora tasdiq talab qilinadi.
/// </summary>
public class ManualDownloadTarget(string stagingRoot) : IArchiveTarget
{
    public string TargetId => "manual";
    public string DisplayName => "Qo'lda yuklab olish";

    public Task<bool> HealthCheckAsync(CancellationToken ct = default)
    {
        Directory.CreateDirectory(stagingRoot);
        return Task.FromResult(true);
    }

    public async Task<ArchiveUploadResult> UploadAsync(
        string archiveName, Stream content, CancellationToken ct = default)
    {
        Directory.CreateDirectory(stagingRoot);
        var path = Path.Combine(stagingRoot, archiveName);

        await using (var file = File.Create(path))
            await content.CopyToAsync(file, ct);

        await using var reading = File.OpenRead(path);
        var hash = Convert.ToHexString(
            await System.Security.Cryptography.SHA256.HashDataAsync(reading, ct))
            .ToLowerInvariant();

        return new ArchiveUploadResult(true, path, hash, null);
    }

    public Task<Stream?> DownloadAsync(
        string location, CancellationToken ct = default)
        => Task.FromResult<Stream?>(
            File.Exists(location) ? File.OpenRead(location) : null);
}
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: add archive target interface and manual download target

Every target must be able to read back what it wrote, because the purge
step verifies the archive before deleting anything. For the manual target
that verification is an explicit human confirmation -- only the user knows
whether the downloaded file actually reached somewhere safe."
```

---

## Task 4: S3-mos va SFTP target'lari

**Files:**
- Create: `src/CustomSync.Services/Storage/Targets/S3ArchiveTarget.cs`
- Create: `src/CustomSync.Services/Storage/Targets/SftpArchiveTarget.cs`

- [ ] **Step 1: Paketlarni qo'shish**

```bash
dotnet add src/CustomSync.Services package AWSSDK.S3
dotnet add src/CustomSync.Services package SSH.NET
```

- [ ] **Step 2: S3 target'ini yozish**

Backblaze B2, Wasabi va boshqa S3-mos xizmatlar uchun. Sozlamalar
`server_settings` da: endpoint, bucket, region, access key, secret.

Yuklashdan keyin `GetObjectMetadata` bilan ETag olinadi va hajm
tekshiriladi; to'liq tekshiruv esa `DownloadAsync` orqali `PurgeService`
tomonidan qilinadi.

- [ ] **Step 3: SFTP target'ini yozish**

`SSH.NET` bilan. Sozlamalar: host, port, foydalanuvchi, kalit yo'li
yoki parol, masofaviy katalog.

**Muhim:** kalit bilan autentifikatsiya afzal ko'riladi; parol bilan
ulanish ishlaydi, lekin UI'da ogohlantirish ko'rsatiladi.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat: add S3-compatible and SFTP archive targets

Both are configured entirely from server_settings so credentials and
destinations change without a redeploy."
```

---

## Task 5: Telegram bot target'i

**Files:**
- Create: `src/CustomSync.Services/Storage/Targets/TelegramBotArchiveTarget.cs`

- [ ] **Step 1: Bot API cheklovlarini hisobga olish**

Bu yerda **muhim assimetriya** bor va u implementatsiyani belgilaydi:

| Amal | Cheklov |
|---|---|
| `sendDocument` (yuborish) | 50 MB |
| `getFile` (yuklab olish) | **20 MB** |

Ya'ni bot 50 MB fayl **yubora oladi, lekin qaytarib ola olmaydi**. Bizga
esa tekshiruv uchun **qaytarib olish shart**.

Shuning uchun arxiv **15 MB lik bo'laklarga** bo'linadi — bu ikkala
chegaradan ham xavfsiz pastda.

- [ ] **Step 2: Target'ni yozish**

```csharp
namespace CustomSync.Services.Storage.Targets;

/// <summary>
/// Arxivni Telegram'ga saqlash (bot orqali, xususiy kanal yoki chatga).
///
/// Bo'lak hajmi 15 MB — ATAYLAB kichik. Bot API 50 MB gacha yubora
/// oladi, lekin getFile orqali faqat 20 MB gacha qaytarib ola oladi.
/// Tekshiruvsiz o'chirish mumkin emas, shuning uchun bo'laklar
/// qaytarib olinadigan hajmda bo'lishi SHART.
///
/// Arxiv bu yerga kelishidan oldin allaqachon shifrlangan (.cmx.enc),
/// shuning uchun Telegram serverlarida faqat shifrlangan blob turadi.
/// </summary>
public class TelegramBotArchiveTarget(
    HttpClient http, string botToken, string chatId) : IArchiveTarget
{
    private const int ChunkBytes = 15 * 1024 * 1024;

    public string TargetId => "telegram";
    public string DisplayName => "Telegram bot";

    public async Task<bool> HealthCheckAsync(CancellationToken ct = default)
    {
        var response = await http.GetAsync(
            $"https://api.telegram.org/bot{botToken}/getMe", ct);
        return response.IsSuccessStatusCode;
    }

    public async Task<ArchiveUploadResult> UploadAsync(
        string archiveName, Stream content, CancellationToken ct = default)
    {
        var messageIds = new List<long>();
        var index = 0;
        var buffer = new byte[ChunkBytes];

        while (true)
        {
            var read = await ReadFullyAsync(content, buffer, ct);
            if (read == 0) break;

            using var form = new MultipartFormDataContent();
            form.Add(new StringContent(chatId), "chat_id");
            form.Add(new ByteArrayContent(buffer, 0, read),
                "document", $"{archiveName}.part{index:D4}");

            var response = await http.PostAsync(
                $"https://api.telegram.org/bot{botToken}/sendDocument", form, ct);
            if (!response.IsSuccessStatusCode)
                return new ArchiveUploadResult(false, null, null,
                    $"Telegram xatosi bo'lak {index}: {response.StatusCode}");

            messageIds.Add(await ExtractFileIdAsync(response, ct));
            ++index;
            if (read < ChunkBytes) break;
        }

        // Location — bo'laklar ro'yxati; qaytarib olishda shu tartibda
        // birlashtiriladi.
        return new ArchiveUploadResult(
            true, string.Join(',', messageIds), null, null);
    }

    public async Task<Stream?> DownloadAsync(
        string location, CancellationToken ct = default)
    {
        var output = new MemoryStream();
        foreach (var fileId in location.Split(','))
        {
            var path = await ResolveFilePathAsync(fileId, ct);
            if (path is null) return null;

            var chunk = await http.GetByteArrayAsync(
                $"https://api.telegram.org/file/bot{botToken}/{path}", ct);
            await output.WriteAsync(chunk, ct);
        }
        output.Position = 0;
        return output;
    }

    // ReadFullyAsync, ExtractFileIdAsync, ResolveFilePathAsync —
    // standart yordamchilar.
}
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: add Telegram bot archive target with 15MB chunking

The chunk size is driven by an asymmetry in the Bot API: sendDocument
accepts 50MB but getFile only returns 20MB. Since nothing is deleted
without reading the archive back, chunks have to stay inside the smaller
download limit, not the larger upload one."
```

---

## Task 6: Ikki fazali xavfsiz o'chirish

Bu planning yuragi.

**Files:**
- Create: `src/CustomSync.Services/Storage/PurgeService.cs`
- Create: `src/CustomSync.Data/Entities/ArchiveRunEntity.cs`
- Create: `tests/CustomSync.Tests/PurgeSafetyTests.cs`

- [ ] **Step 1: Yiqiladigan xavfsizlik testlarini yozish**

Bular eng muhim testlar — ular ma'lumot yo'qolishini oldini oladi.

```csharp
using CustomSync.Services.Storage;
using CustomSync.Tests.Fixtures;
using Xunit;

namespace CustomSync.Tests;

public class PurgeSafetyTests : IClassFixture<DatabaseFixture>
{
    private readonly DatabaseFixture _fixture;

    public PurgeSafetyTests(DatabaseFixture fixture) => _fixture = fixture;

    [Fact]
    public async Task Nothing_is_deleted_when_upload_fails()
    {
        // Target yuklashda xato qaytaradi → yozuvlar joyida qolishi kerak.
        var before = await CountRecordsAsync();
        var result = await RunPurgeAsync(new FailingUploadTarget());

        Assert.False(result.Success);
        Assert.Equal(before, await CountRecordsAsync());
    }

    [Fact]
    public async Task Nothing_is_deleted_when_verification_fails()
    {
        // Yuklash "muvaffaqiyatli", lekin qaytarib o'qilgan ma'lumot
        // boshqacha → o'chirish BEKOR QILINADI.
        var before = await CountRecordsAsync();
        var result = await RunPurgeAsync(new CorruptingTarget());

        Assert.False(result.Success);
        Assert.Contains("verification", result.Error!, StringComparison.OrdinalIgnoreCase);
        Assert.Equal(before, await CountRecordsAsync());
    }

    [Fact]
    public async Task Dry_run_never_deletes_anything()
    {
        var before = await CountRecordsAsync();
        var preview = await PreviewPurgeAsync();

        Assert.True(preview.MatchedCount > 0);
        Assert.Equal(before, await CountRecordsAsync());
    }

    [Fact]
    public async Task Records_are_deleted_only_after_verified_archive()
    {
        var result = await RunPurgeAsync(new WorkingTarget());

        Assert.True(result.Success);
        Assert.True(result.DeletedCount > 0);
    }
}
```

- [ ] **Step 2: Testni ishga tushirib, yiqilishini ko'rish**

```bash
dotnet test --filter PurgeSafetyTests
```

Kutilgan: FAIL — `PurgeService` mavjud emas.

- [ ] **Step 3: `PurgeService` ni yozish**

```csharp
using System.Security.Cryptography;
using CustomSync.Core.Interchange;
using CustomSync.Data;
using CustomSync.Services.Storage.Targets;
using Microsoft.EntityFrameworkCore;

namespace CustomSync.Services.Storage;

public sealed record PurgePreview(
    int MatchedCount, long MatchedBytes, long FreedBytes,
    long OldestOccurredAt, long NewestOccurredAt);

public sealed record PurgeResult(
    bool Success, int DeletedCount, long FreedBytes,
    string? ArchiveLocation, string? Error);

/// <summary>
/// Ikki fazali o'chirish. Qat'iy tartib:
///   arxivlash → yuborish → QAYTA O'QIB TEKSHIRISH → o'chirish
///
/// Tekshiruvdan o'tmagan bo'lsa hech narsa o'chirilmaydi. Bu qoida
/// muhokama qilinmaydi: bu yerda yo'qolgan ma'lumot qaytarib bo'lmaydi,
/// chunki Telegram serverida ham u allaqachon yo'q.
/// </summary>
public class PurgeService(
    SyncDbContext db,
    SyncService sync,
    MediaService media,
    AuditService audit)
{
    public async Task<PurgePreview> PreviewAsync(
        RetentionScope scope, CancellationToken ct = default)
    {
        var query = BuildQuery(scope);
        var matched = await query.CountAsync(ct);
        var bytes = matched == 0 ? 0
            : await query.SumAsync(r => (long)r.PayloadSize, ct);

        return new PurgePreview(
            matched, bytes, bytes,
            matched == 0 ? 0 : await query.MinAsync(r => r.OccurredAt, ct),
            matched == 0 ? 0 : await query.MaxAsync(r => r.OccurredAt, ct));
    }

    public async Task<PurgeResult> ExecuteAsync(
        RetentionScope scope,
        IArchiveTarget target,
        CancellationToken ct = default)
    {
        var query = BuildQuery(scope);
        var records = await query.ToListAsync(ct);
        if (records.Count == 0)
            return new PurgeResult(true, 0, 0, null, null);

        // ── Faza 1: arxiv yaratish ────────────────────────────────
        var archiveName = $"customsync-purge-{DateTime.UtcNow:yyyyMMdd-HHmmss}.cmx";
        using var archive = new MemoryStream();
        await WriteArchiveAsync(archive, records, ct);
        archive.Position = 0;

        var expectedHash = Convert.ToHexString(
            await SHA256.HashDataAsync(archive, ct)).ToLowerInvariant();
        archive.Position = 0;

        // ── Faza 2: yuborish ──────────────────────────────────────
        var upload = await target.UploadAsync(archiveName, archive, ct);
        if (!upload.Success)
        {
            await audit.WriteAsync("purge.upload_failed",
                detail: new { target.TargetId, upload.Error }, ct: ct);
            return new PurgeResult(false, 0, 0, null,
                $"Arxivni yuborib bo'lmadi: {upload.Error}");
        }

        // ── Faza 3: TEKSHIRISH ────────────────────────────────────
        await using var readBack = await target.DownloadAsync(upload.Location!, ct);
        if (readBack is null)
        {
            await audit.WriteAsync("purge.verification_failed",
                detail: new { target.TargetId, reason = "download_returned_null" }, ct: ct);
            return new PurgeResult(false, 0, 0, upload.Location,
                "Verification failed: arxivni qaytarib o'qib bo'lmadi. " +
                "Hech narsa o'chirilmadi.");
        }

        var actualHash = Convert.ToHexString(
            await SHA256.HashDataAsync(readBack, ct)).ToLowerInvariant();
        if (actualHash != expectedHash)
        {
            await audit.WriteAsync("purge.verification_failed",
                detail: new { target.TargetId, expectedHash, actualHash }, ct: ct);
            return new PurgeResult(false, 0, 0, upload.Location,
                "Verification failed: checksum mos kelmadi. Hech narsa o'chirilmadi.");
        }

        // ── Faza 4: faqat endi o'chirish ──────────────────────────
        var ids = records.Select(r => r.RecordId).ToList();
        var freed = records.Sum(r => (long)r.PayloadSize);

        await db.RecordMedia.Where(m => ids.Contains(m.RecordId))
            .ExecuteDeleteAsync(ct);
        await db.Records.Where(r => ids.Contains(r.RecordId))
            .ExecuteDeleteAsync(ct);

        var orphaned = await DeleteOrphanedMediaAsync(ct);

        await audit.WriteAsync("purge.completed", detail: new
        {
            target.TargetId,
            location = upload.Location,
            deleted = ids.Count,
            orphanedMedia = orphaned,
            freed,
            sha256 = expectedHash
        }, ct: ct);

        return new PurgeResult(true, ids.Count, freed, upload.Location, null);
    }

    /// <summary>
    /// Hech qanday yozuv havola qilmaydigan media'ni o'chiradi.
    /// Yozuvlar o'chgandan KEYIN chaqiriladi, aks holda hali kerak
    /// bo'lgan blob o'chib ketishi mumkin.
    /// </summary>
    private async Task<int> DeleteOrphanedMediaAsync(CancellationToken ct)
    {
        var referenced = db.RecordMedia.Select(m => m.Hash);
        var orphans = await db.MediaBlobs
            .Where(b => !referenced.Contains(b.Hash))
            .ToListAsync(ct);

        foreach (var blob in orphans)
        {
            if (File.Exists(blob.StoragePath)) File.Delete(blob.StoragePath);
        }
        db.MediaBlobs.RemoveRange(orphans);
        await db.SaveChangesAsync(ct);
        return orphans.Count;
    }

    // BuildQuery va WriteArchiveAsync — scope bo'yicha filtr va
    // CmxWriter chaqiruvi.
}
```

- [ ] **Step 4: Testni qayta ishga tushirish**

```bash
dotnet test --filter PurgeSafetyTests
```

Kutilgan: PASS — 4 ta test o'tdi.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat: add two-phase purge that verifies before deleting

The archive is uploaded, read back, and checksummed before a single row is
removed. If the target cannot return what it was given, the purge aborts
and says so -- data deleted here is unrecoverable, so a failed
verification has to mean 'stop', not 'probably fine'.

Orphaned media is collected only after records are deleted; doing it
before would remove blobs that are still referenced."
```

---

## Task 7: Rejalashtirilgan ishlar va dry-run

**Files:**
- Create: `src/CustomSync.Services/Storage/ArchiveJobService.cs`
- Modify: `src/CustomSync.Api/Program.cs`

- [ ] **Step 1: Fon xizmatini yozish**

`BackgroundService` sifatida, sozlamada belgilangan vaqtda (standart
kechasi 03:30) ishlaydi:

1. Yoqilgan siyosatlarni `Priority` bo'yicha o'qiydi
2. Har biri uchun `PreviewAsync` chaqiradi
3. Agar mos yozuv bo'lsa — `ExecuteAsync`
4. Natijani `archive_runs` jadvaliga va audit log'ga yozadi
5. Xato bo'lsa — keyingi siyosatga o'tadi, to'xtamaydi

Sozlamalar (`server_settings` ga qo'shiladi):

```
storage.jobs_enabled          bool   false   Rejalashtirilgan arxivlash yoqilganmi
storage.jobs_hour             int    3       Ishga tushish soati (UTC)
storage.jobs_minute           int    30      Daqiqa
storage.disk_capacity_bytes   int    0       Disk hajmi (0 = avtomatik aniqlash)
storage.warn_percent          int    80      Ogohlantirish chegarasi
storage.critical_percent      int    92      Kritik chegara
```

**`storage.jobs_enabled` standart `false`** — avtomatik o'chirish
foydalanuvchi aniq yoqmaguncha ishlamaydi.

- [ ] **Step 2: Chegara ogohlantirishlari**

Har ishga tushganda disk foizi tekshiriladi. Chegaradan oshsa audit
log'ga yoziladi va web app'da banner ko'rsatiladi.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: add scheduled archive jobs with thresholds

Scheduled purging is off by default. A job that fails on one policy moves
to the next instead of aborting the run, so one unreachable target does
not block every other policy that night."
```

---

## Task 8: Web UI

**Files:**
- Modify: `web/src/views/StorageView.vue`
- Create: `web/src/views/RetentionView.vue`

- [ ] **Step 1: Xotira ekranini kengaytirish**

- Umumiy hajm, disk foizi (rangli progress: yashil/sariq/qizil)
- Kunlik o'sish va "disk N kunda to'ladi" prognozi
- Peer bo'yicha eng katta 20 talik (ismlar bilan)
- Tur bo'yicha taqsimot
- Media va yozuvlar nisbati

- [ ] **Step 2: Retention ekranini yozish**

Siyosatlar ro'yxati va tahrirlash. Har bir siyosat uchun **"Ko'rib
chiqish (dry-run)"** tugmasi — u nima o'chishini va qancha joy
bo'shashini ko'rsatadi, hech narsa o'chirmaydi.

- [ ] **Step 3: Qo'lda arxivlash oqimi**

Sehrgar (wizard) ko'rinishida, chunki bu xavfli amal:

```
1-qadam: Qamrov  → sana oralig'i, peer, tur
2-qadam: Ko'rib chiqish → "1 240 yozuv, 3.2 GB o'chadi"
3-qadam: Manzil  → target tanlash + health check
4-qadam: Bajarish → progress: arxivlash → yuborish → tekshirish → o'chirish
5-qadam: Natija  → arxiv manzili, o'chirilgan hajm
```

**3-qadamda tanlangan target avval health check'dan o'tadi** — sozlamasi
noto'g'ri target bilan boshlanib, oxirida yiqilish yomon tajriba.

**"Qo'lda yuklab olish" target'ida** 4-qadam to'xtaydi va foydalanuvchidan
oshkora tasdiq so'raydi: *"Faylni yuklab oldingiz va xavfsiz joyga
saqladingizmi? Tasdiqlasangiz, bu ma'lumot serverdan butunlay
o'chiriladi."*

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(web): add storage and retention management screens

Manual purging is a wizard with a mandatory dry-run step and a target
health check before execution, because discovering a misconfigured
destination after the archive is built is the worst moment to find out."
```

---

## Qabul qilish mezonlari (04)

1. Yuklash muvaffaqiyatsiz bo'lsa — **hech narsa o'chirilmaydi**.
2. Tekshiruv muvaffaqiyatsiz bo'lsa — **hech narsa o'chirilmaydi**, xato
   aniq ko'rsatiladi.
3. Dry-run hech qachon hech narsa o'chirmaydi.
4. To'rtala target ham yozadi **va qaytarib o'qiydi**.
5. Telegram target'i 15 MB dan katta arxivni bo'laklab yuboradi va
   to'liq qaytarib oladi.
6. `never_delete` siyosati boshqa barcha siyosatlardan ustun turadi.
7. Yetim media yozuvlar o'chirilgandan keyin tozalanadi.
8. Avtomatik ishlar standart holatda **o'chiq**.
9. Har bir arxivlash audit log'da manzili va checksum'i bilan qayd
   etilgan.

---

## Keyingi qadam

Plan 05 — Always-on TDLib capture service. Endi xotira boshqaruvi
tayyor bo'lgani uchun 24/7 ma'lumot oqimini xavfsiz yoqish mumkin.
