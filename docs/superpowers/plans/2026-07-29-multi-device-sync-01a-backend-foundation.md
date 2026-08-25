# Backend Foundation Implementation Plan (1a)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `customsync-server` repozitoriysini yaratish va unda ishlaydigan .NET 8 API poydevorini qurish — PostgreSQL sxemasi, runtime konfiguratsiya, qurilma ro'yxatdan o'tkazish va JWT autentifikatsiya.

**Architecture:** ASP.NET Core 8 Minimal API, endpoint'lar funksiya bo'yicha alohida modul fayllarga bo'lingan. EF Core 8 + Npgsql migratsiyalar va oddiy so'rovlar uchun; sync hot-path'da aniq nazorat kerak bo'lgani uchun raw SQL `NpgsqlCommand` orqali (1b planida). Barcha sozlanadigan qiymatlar `server_settings` jadvalida (qoida K1).

**Tech Stack:** .NET 8, ASP.NET Core Minimal API, EF Core 8, Npgsql, PostgreSQL 16, xUnit, `Microsoft.AspNetCore.Mvc.Testing`, Serilog.

**Kirish sharti:** Ishlab chiqish mashinasida PostgreSQL 16 va .NET 8 SDK o'rnatilgan bo'lishi kerak.

**Umumiy qoidalar:** [00-index](2026-07-29-multi-device-sync-00-index.md) dagi K1–K7 qoidalari bu plandagi har bir task uchun kuchda.

---

> ## ⚠️ REVIZIYA 2026-08-25 — bu planga tegishli o'zgarishlar
>
> To'liq ro'yxat: spec **§0**. Quyida faqat SHU planga tegishlilari.
>
> ### Task 2 (`RecordId`) — manfiy `msg_id`
> `msg_id` **manfiy bo'lishi mumkin** (avatar `-photo_id`, story
> `-story_id`, skaner topgan fayl `-qHash(rel_path)-1`).
> `record_id` hisoblashda ishora **saqlanishi shart** — `-42` va `42`
> turli yozuvlar. Test vektorlariga manfiy holat ham qo'shiladi.
>
> ### Task 3 (PostgreSQL sxemasi)
> `records.msg_id` allaqachon `BIGINT` — manfiy qiymat muammosiz.
> Qo'shimcha jadval **kerak emas**: `tombstone` ham oddiy `records`
> qatori (spec §0.3). Server tombstone qabul qilganda `payload`
> ichidagi `target_record_id` bo'yicha asl qatorni o'chiradi va
> tombstone'ning o'zini saqlaydi.
>
> ### Task 4 (`SettingsService`) — yangi kalitlar
> | Kalit | Standart | Nima uchun |
> |---|---|---|
> | `retention.activity_days` | 90 | Mijozda 30, serverda UZUNROQ (spec §0.3) |
> | `retention.<kind>_days` | 0 = cheksiz | Kind bo'yicha alohida |
> | `storage.quota_total_mb` | 0 = cheksiz | Spec §0.9 |
> | `storage.quota_per_device_mb` | 0 = cheksiz | |
>
> 🔴 Serverdagi retention **hech qachon tombstone yaratmaydi** —
> u lokal tozalash, global o'chirish emas.

---

## File Structure

```
customsync-server/
├── CustomSync.sln
├── .gitignore
├── src/
│   ├── CustomSync.Core/               # Platformalararo kontraktlar. Hech nimaga bog'liq emas.
│   │   ├── Contracts/
│   │   │   ├── RecordKind.cs          # kind konstantalari
│   │   │   └── SyncRecord.cs          # kanonik yozuv DTO
│   │   └── RecordId.cs                # deterministik id — referens implementatsiya
│   ├── CustomSync.Data/               # Faqat sxema va DbContext.
│   │   ├── SyncDbContext.cs
│   │   ├── Entities/
│   │   │   ├── DeviceEntity.cs
│   │   │   ├── RecordEntity.cs
│   │   │   ├── MediaBlobEntity.cs
│   │   │   ├── RecordMediaEntity.cs
│   │   │   ├── KeyWrapEntity.cs
│   │   │   ├── ServerSettingEntity.cs
│   │   │   ├── EnrollmentCodeEntity.cs
│   │   │   └── AuditLogEntity.cs
│   │   └── Migrations/                # dotnet ef tomonidan generatsiya qilinadi
│   ├── CustomSync.Services/           # Biznes logika. Endpoint'lardan mustaqil.
│   │   ├── SettingsService.cs
│   │   └── DeviceService.cs
│   └── CustomSync.Api/
│       ├── Program.cs
│       ├── appsettings.json
│       ├── Auth/
│       │   └── JwtIssuer.cs
│       └── Endpoints/
│           ├── HealthEndpoints.cs
│           ├── SettingsEndpoints.cs
│           └── DeviceEndpoints.cs
└── tests/
    └── CustomSync.Tests/
        ├── Fixtures/DatabaseFixture.cs
        ├── RecordIdTests.cs
        ├── SettingsServiceTests.cs
        └── DeviceAuthTests.cs
```

**Loyihaviy bog'liqliklar (bir tomonlama):**
`Api → Services → Data → Core`. `Core` hech nimaga bog'liq emas — shu sababli
uni keyinchalik capture service ham, test-vektor generatori ham ishlatadi.

---

## Task 1: Solution va loyiha skeleti

**Files:**
- Create: `CustomSync.sln`, `.gitignore`
- Create: `src/CustomSync.Core/CustomSync.Core.csproj` (va qolgan 3 ta loyiha)
- Create: `tests/CustomSync.Tests/CustomSync.Tests.csproj`
- Create: `src/CustomSync.Api/Endpoints/HealthEndpoints.cs`
- Modify: `src/CustomSync.Api/Program.cs`

- [ ] **Step 1: Repozitoriy va loyihalarni yaratish**

```bash
mkdir customsync-server && cd customsync-server && git init
dotnet new gitignore
dotnet new sln -n CustomSync
dotnet new classlib -n CustomSync.Core     -o src/CustomSync.Core
dotnet new classlib -n CustomSync.Data     -o src/CustomSync.Data
dotnet new classlib -n CustomSync.Services -o src/CustomSync.Services
dotnet new webapi   -n CustomSync.Api      -o src/CustomSync.Api
dotnet new xunit    -n CustomSync.Tests    -o tests/CustomSync.Tests
dotnet sln add src/CustomSync.Core src/CustomSync.Data src/CustomSync.Services src/CustomSync.Api tests/CustomSync.Tests
dotnet add src/CustomSync.Data     reference src/CustomSync.Core
dotnet add src/CustomSync.Services reference src/CustomSync.Data
dotnet add src/CustomSync.Api      reference src/CustomSync.Services
dotnet add tests/CustomSync.Tests  reference src/CustomSync.Api
dotnet add tests/CustomSync.Tests  package Microsoft.AspNetCore.Mvc.Testing
```

- [ ] **Step 2: `Program.cs` ni testlanadigan qilish**

`WebApplicationFactory<Program>` ishlashi uchun `Program` klassi ochiq
bo'lishi shart. `src/CustomSync.Api/Program.cs` ni to'liq almashtiring:

```csharp
using CustomSync.Api.Endpoints;

var builder = WebApplication.CreateBuilder(args);

var app = builder.Build();

app.MapHealthEndpoints();

app.Run();

// Integration testlar uchun (WebApplicationFactory<Program>).
public partial class Program { }
```

- [ ] **Step 3: Yiqiladigan testni yozish**

`tests/CustomSync.Tests/HealthTests.cs`:

```csharp
using System.Net;
using System.Net.Http.Json;
using Microsoft.AspNetCore.Mvc.Testing;
using Xunit;

namespace CustomSync.Tests;

public class HealthTests : IClassFixture<WebApplicationFactory<Program>>
{
    private readonly WebApplicationFactory<Program> _factory;

    public HealthTests(WebApplicationFactory<Program> factory) => _factory = factory;

    [Fact]
    public async Task Health_returns_ok_with_version()
    {
        var client = _factory.CreateClient();

        var response = await client.GetAsync("/api/v1/health");

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        var body = await response.Content.ReadFromJsonAsync<HealthBody>();
        Assert.Equal("ok", body!.Status);
        Assert.False(string.IsNullOrWhiteSpace(body.Version));
    }

    private sealed record HealthBody(string Status, string Version);
}
```

- [ ] **Step 4: Testni ishga tushirib, yiqilishini ko'rish**

```bash
dotnet test
```

Kutilgan: FAIL — `/api/v1/health` mavjud emas (404), yoki
`MapHealthEndpoints` topilmagani uchun kompilyatsiya xatosi.

- [ ] **Step 5: Health endpoint'ini yozish**

`src/CustomSync.Api/Endpoints/HealthEndpoints.cs`:

```csharp
using System.Reflection;

namespace CustomSync.Api.Endpoints;

public static class HealthEndpoints
{
    public static void MapHealthEndpoints(this WebApplication app)
    {
        app.MapGet("/api/v1/health", () =>
        {
            var version = Assembly.GetExecutingAssembly()
                .GetCustomAttribute<AssemblyInformationalVersionAttribute>()
                ?.InformationalVersion ?? "0.0.0";

            return Results.Ok(new { status = "ok", version });
        });
    }
}
```

- [ ] **Step 6: Testni qayta ishga tushirish**

```bash
dotnet test
```

Kutilgan: PASS — 1 ta test o'tdi.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "chore: scaffold CustomSync solution with health endpoint

Four projects with a one-way dependency chain (Api -> Services -> Data ->
Core) so that Core stays free of infrastructure and can later be reused by
the capture service and the cross-platform test-vector generator."
```

---

## Task 2: Kanonik yozuv kontrakti va `RecordId`

`RecordId` — butun tizimning eng muhim primitivi. Beshala platforma uni
**bayt-ma-bayt bir xil** hisoblashi shart, aks holda dedup buziladi.

**Files:**
- Create: `src/CustomSync.Core/Contracts/RecordKind.cs`
- Create: `src/CustomSync.Core/Contracts/SyncRecord.cs`
- Create: `src/CustomSync.Core/RecordId.cs`
- Create: `tests/CustomSync.Tests/RecordIdTests.cs`

- [ ] **Step 1: Yiqiladigan testni yozish**

`tests/CustomSync.Tests/RecordIdTests.cs`:

```csharp
using CustomSync.Core;
using Xunit;

namespace CustomSync.Tests;

public class RecordIdTests
{
    [Fact]
    public void Compute_is_deterministic()
    {
        var a = RecordId.Compute("deleted", "a3f9c2", 12345, 1753800000);
        var b = RecordId.Compute("deleted", "a3f9c2", 12345, 1753800000);

        Assert.Equal(a, b);
    }

    [Fact]
    public void Compute_returns_lowercase_hex_of_64_chars()
    {
        var id = RecordId.Compute("deleted", "a3f9c2", 12345, 1753800000);

        Assert.Equal(64, id.Length);
        Assert.Matches("^[0-9a-f]{64}$", id);
    }

    [Theory]
    [InlineData("edited",  "a3f9c2", 12345, 1753800000)]
    [InlineData("deleted", "b7c210", 12345, 1753800000)]
    [InlineData("deleted", "a3f9c2", 12346, 1753800000)]
    [InlineData("deleted", "a3f9c2", 12345, 1753800001)]
    public void Compute_changes_when_any_field_changes(
        string kind, string peerHash, long msgId, long occurredAt)
    {
        var baseline = RecordId.Compute("deleted", "a3f9c2", 12345, 1753800000);

        var other = RecordId.Compute(kind, peerHash, msgId, occurredAt);

        Assert.NotEqual(baseline, other);
    }

    [Fact]
    public void Separator_prevents_field_boundary_ambiguity()
    {
        // Ajratuvchi bo'lmasa "ab"+"c" va "a"+"bc" bir xil natija berardi.
        var first  = RecordId.Compute("ab", "c", 1, 1);
        var second = RecordId.Compute("a", "bc", 1, 1);

        Assert.NotEqual(first, second);
    }
}
```

- [ ] **Step 2: Testni ishga tushirib, yiqilishini ko'rish**

```bash
dotnet test --filter RecordIdTests
```

Kutilgan: FAIL — `RecordId` klassi mavjud emas (kompilyatsiya xatosi).

- [ ] **Step 3: Kontraktlarni yozish**

`src/CustomSync.Core/Contracts/RecordKind.cs`:

```csharp
namespace CustomSync.Core.Contracts;

/// <summary>
/// Yozuv turlari. Bu satrlar simli protokolning bir qismi —
/// o'zgartirilsa barcha klientlar buziladi.
/// </summary>
public static class RecordKind
{
    public const string Deleted       = "deleted";
    public const string Edited        = "edited";
    public const string Activity      = "activity";
    public const string GhostRead     = "ghost_read";
    public const string Setting       = "setting";
    public const string PeerDirectory = "peer_directory";

    public static readonly IReadOnlySet<string> All = new HashSet<string>
    {
        Deleted, Edited, Activity, GhostRead, Setting, PeerDirectory
    };

    public static bool IsValid(string kind) => All.Contains(kind);
}
```

`src/CustomSync.Core/Contracts/SyncRecord.cs`:

```csharp
namespace CustomSync.Core.Contracts;

/// <summary>
/// Kanonik yozuv. Aynan shu shakl uch joyda ishlatiladi:
/// HTTP sync payload, .cmx faylining records.jsonl satri, DB qatori.
/// </summary>
public sealed record SyncRecord
{
    public required string RecordId    { get; init; }
    public required string Kind        { get; init; }
    public required string PeerHash    { get; init; }
    public long            MsgId       { get; init; }
    public required long   OccurredAt  { get; init; }
    public required long   ObservedAt  { get; init; }
    public required string DeviceId    { get; init; }
    public required byte[] Nonce       { get; init; }
    public required byte[] Payload     { get; init; }
    public IReadOnlyList<MediaRef> Media { get; init; } = Array.Empty<MediaRef>();
}

public sealed record MediaRef
{
    public required string Hash  { get; init; }
    public required long   Size  { get; init; }
    public required byte[] Nonce { get; init; }
}
```

- [ ] **Step 4: `RecordId` ni yozish**

`src/CustomSync.Core/RecordId.cs`:

```csharp
using System.Globalization;
using System.Security.Cryptography;
using System.Text;

namespace CustomSync.Core;

/// <summary>
/// Yozuvning deterministik identifikatori — referens implementatsiya.
///
/// Formula:
///   record_id = hex(SHA256(kind ‖ 0x00 ‖ peerHash ‖ 0x00 ‖
///                          msgId ‖ 0x00 ‖ occurredAt))
///
/// Barcha satrlar UTF-8, sonlar InvariantCulture o'nlik ko'rinishda.
/// 0x00 ajratuvchi maydon chegarasi noaniqligini yo'q qiladi.
///
/// MUHIM: bu funksiya beshala platformada bayt-ma-bayt bir xil natija
/// berishi shart. O'zgartirish oldingi barcha yozuvlarni yaroqsiz qiladi.
/// </summary>
public static class RecordId
{
    public static string Compute(
        string kind, string peerHash, long msgId, long occurredAt)
    {
        var buffer = new ArrayBufferWriter<byte>(128);
        Append(buffer, kind);
        Append(buffer, "\0");
        Append(buffer, peerHash);
        Append(buffer, "\0");
        Append(buffer, msgId.ToString(CultureInfo.InvariantCulture));
        Append(buffer, "\0");
        Append(buffer, occurredAt.ToString(CultureInfo.InvariantCulture));

        var hash = SHA256.HashData(buffer.WrittenSpan);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    private static void Append(ArrayBufferWriter<byte> writer, string value)
    {
        var count = Encoding.UTF8.GetByteCount(value);
        var span = writer.GetSpan(count);
        Encoding.UTF8.GetBytes(value, span);
        writer.Advance(count);
    }
}
```

`ArrayBufferWriter` uchun fayl boshiga `using System.Buffers;` qo'shing.

- [ ] **Step 5: Testni qayta ishga tushirish**

```bash
dotnet test --filter RecordIdTests
```

Kutilgan: PASS — 7 ta test o'tdi (2 + 4 theory + 1).

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat: add canonical record contract and deterministic RecordId

RecordId is the primitive the whole dedup design rests on: two devices
observing the same event must independently produce the same id, with no
coordination. The 0x00 separator is load-bearing -- without it ('ab','c')
and ('a','bc') would hash identically, silently merging unrelated events."
```

---

## Task 3: PostgreSQL sxemasi va DbContext

**Files:**
- Create: `src/CustomSync.Data/Entities/*.cs` (8 ta fayl)
- Create: `src/CustomSync.Data/SyncDbContext.cs`
- Modify: `src/CustomSync.Api/appsettings.json`
- Modify: `src/CustomSync.Api/Program.cs`

- [ ] **Step 1: Paketlarni qo'shish**

```bash
dotnet add src/CustomSync.Data package Npgsql.EntityFrameworkCore.PostgreSQL
dotnet add src/CustomSync.Api  package Npgsql.EntityFrameworkCore.PostgreSQL
dotnet add src/CustomSync.Api  package Microsoft.EntityFrameworkCore.Design
dotnet tool install --global dotnet-ef
```

- [ ] **Step 2: Entity'larni yozish**

`src/CustomSync.Data/Entities/DeviceEntity.cs`:

```csharp
namespace CustomSync.Data.Entities;

public class DeviceEntity
{
    public string    DeviceId     { get; set; } = null!;
    public string    Name         { get; set; } = null!;
    public string    Platform     { get; set; } = null!;
    public DateTime  EnrolledAt   { get; set; }
    public DateTime? LastSeenAt   { get; set; }
    public long      LastCursor   { get; set; }
    public DateTime? RevokedAt    { get; set; }
    public string    RefreshHash  { get; set; } = null!;
}
```

`src/CustomSync.Data/Entities/RecordEntity.cs`:

```csharp
namespace CustomSync.Data.Entities;

public class RecordEntity
{
    /// <summary>Deterministik, hech qachon o'zgarmaydi. PRIMARY KEY.</summary>
    public string   RecordId    { get; set; } = null!;

    /// <summary>
    /// Cursor manbasi. sync_counter dan olinadi va yozuv yaxshiroq
    /// kuzatuv bilan almashtirilganda YANGILANADI — shuning uchun bu
    /// PRIMARY KEY emas.
    /// </summary>
    public long     Seq         { get; set; }

    public string   Kind        { get; set; } = null!;
    public string   PeerHash    { get; set; } = null!;
    public long     MsgId       { get; set; }
    public long     OccurredAt  { get; set; }
    public long     ObservedAt  { get; set; }
    public string   DeviceId    { get; set; } = null!;
    public byte[]   Nonce       { get; set; } = null!;
    public byte[]   Payload     { get; set; } = null!;
    public int      PayloadSize { get; set; }
    public DateTime ReceivedAt  { get; set; }
}
```

`src/CustomSync.Data/Entities/MediaBlobEntity.cs`:

```csharp
namespace CustomSync.Data.Entities;

public class MediaBlobEntity
{
    public string   Hash        { get; set; } = null!;
    public long     Size        { get; set; }
    public byte[]   Nonce       { get; set; } = null!;
    public string   StoragePath { get; set; } = null!;
    public DateTime UploadedAt  { get; set; }
}
```

`src/CustomSync.Data/Entities/RecordMediaEntity.cs`:

```csharp
namespace CustomSync.Data.Entities;

public class RecordMediaEntity
{
    public string RecordId { get; set; } = null!;
    public string Hash     { get; set; } = null!;
}
```

`src/CustomSync.Data/Entities/KeyWrapEntity.cs`:

```csharp
namespace CustomSync.Data.Entities;

public class KeyWrapEntity
{
    public string    WrapId     { get; set; } = null!;
    /// <summary>"passphrase" | "recovery" | "email"</summary>
    public string    WrapType   { get; set; } = null!;
    public string    Label      { get; set; } = null!;
    public byte[]    Salt       { get; set; } = null!;
    public byte[]    Nonce      { get; set; } = null!;
    public byte[]    WrappedKey { get; set; } = null!;
    public int       Iterations { get; set; }
    public DateTime  CreatedAt  { get; set; }
    public DateTime? LastUsedAt { get; set; }
}
```

`src/CustomSync.Data/Entities/ServerSettingEntity.cs`:

```csharp
namespace CustomSync.Data.Entities;

/// <summary>
/// Runtime konfiguratsiya (qoida K1). Sozlanadigan har qanday qiymat
/// shu yerda yashaydi — kodda yoki appsettings.json da emas.
/// </summary>
public class ServerSettingEntity
{
    public string   Key         { get; set; } = null!;
    public string   Value       { get; set; } = null!;
    /// <summary>"int" | "bool" | "string" | "duration" — UI validatsiyasi uchun.</summary>
    public string   ValueType   { get; set; } = null!;
    public string   Category    { get; set; } = null!;
    public string   Description { get; set; } = null!;
    public DateTime UpdatedAt   { get; set; }
}
```

`src/CustomSync.Data/Entities/EnrollmentCodeEntity.cs`:

```csharp
namespace CustomSync.Data.Entities;

public class EnrollmentCodeEntity
{
    public string    CodeHash  { get; set; } = null!;
    public DateTime  CreatedAt { get; set; }
    public DateTime  ExpiresAt { get; set; }
    public DateTime? UsedAt    { get; set; }
    public string?   UsedBy    { get; set; }
}
```

`src/CustomSync.Data/Entities/AuditLogEntity.cs`:

```csharp
namespace CustomSync.Data.Entities;

public class AuditLogEntity
{
    public long     Id       { get; set; }
    public DateTime At       { get; set; }
    public string?  DeviceId { get; set; }
    public string   Action   { get; set; } = null!;
    public string?  Detail   { get; set; }
}
```

- [ ] **Step 3: DbContext'ni yozish**

`src/CustomSync.Data/SyncDbContext.cs`:

```csharp
using CustomSync.Data.Entities;
using Microsoft.EntityFrameworkCore;

namespace CustomSync.Data;

public class SyncDbContext(DbContextOptions<SyncDbContext> options)
    : DbContext(options)
{
    public DbSet<DeviceEntity>         Devices         => Set<DeviceEntity>();
    public DbSet<RecordEntity>         Records         => Set<RecordEntity>();
    public DbSet<MediaBlobEntity>      MediaBlobs      => Set<MediaBlobEntity>();
    public DbSet<RecordMediaEntity>    RecordMedia     => Set<RecordMediaEntity>();
    public DbSet<KeyWrapEntity>        KeyWraps        => Set<KeyWrapEntity>();
    public DbSet<ServerSettingEntity>  ServerSettings  => Set<ServerSettingEntity>();
    public DbSet<EnrollmentCodeEntity> EnrollmentCodes => Set<EnrollmentCodeEntity>();
    public DbSet<AuditLogEntity>       AuditLogs       => Set<AuditLogEntity>();

    protected override void OnModelCreating(ModelBuilder b)
    {
        b.Entity<DeviceEntity>(e =>
        {
            e.ToTable("devices");
            e.HasKey(x => x.DeviceId);
        });

        b.Entity<RecordEntity>(e =>
        {
            e.ToTable("records");
            e.HasKey(x => x.RecordId);
            e.HasIndex(x => x.Seq).IsUnique();
            e.HasIndex(x => new { x.PeerHash, x.OccurredAt, x.Seq })
             .HasDatabaseName("idx_records_peer");
            e.HasIndex(x => new { x.Kind, x.OccurredAt, x.Seq })
             .HasDatabaseName("idx_records_kind");
            e.HasIndex(x => new { x.OccurredAt, x.Seq })
             .HasDatabaseName("idx_records_occur");
        });

        b.Entity<MediaBlobEntity>(e =>
        {
            e.ToTable("media_blobs");
            e.HasKey(x => x.Hash);
        });

        b.Entity<RecordMediaEntity>(e =>
        {
            e.ToTable("record_media");
            e.HasKey(x => new { x.RecordId, x.Hash });
        });

        b.Entity<KeyWrapEntity>(e =>
        {
            e.ToTable("key_wraps");
            e.HasKey(x => x.WrapId);
        });

        b.Entity<ServerSettingEntity>(e =>
        {
            e.ToTable("server_settings");
            e.HasKey(x => x.Key);
        });

        b.Entity<EnrollmentCodeEntity>(e =>
        {
            e.ToTable("enrollment_codes");
            e.HasKey(x => x.CodeHash);
        });

        b.Entity<AuditLogEntity>(e =>
        {
            e.ToTable("audit_log");
            e.HasKey(x => x.Id);
            e.HasIndex(x => x.At);
        });
    }
}
```

**Eslatma:** `sync_counter` jadvali EF entity sifatida modellashtirilmaydi —
u faqat raw SQL orqali ishlatiladi (1b, Task 2). U keyingi qadamdagi
migratsiyaga qo'lda qo'shiladi.

- [ ] **Step 4: DI va connection string**

`src/CustomSync.Api/appsettings.json` ni almashtiring:

```json
{
  "ConnectionStrings": {
    "Postgres": "Host=localhost;Port=5432;Database=customsync;Username=customsync;Password=CHANGE_ME"
  },
  "Logging": {
    "LogLevel": { "Default": "Information", "Microsoft.AspNetCore": "Warning" }
  },
  "AllowedHosts": "*"
}
```

`Program.cs` da `var app = builder.Build();` dan **oldin**:

```csharp
builder.Services.AddDbContext<SyncDbContext>(o =>
    o.UseNpgsql(builder.Configuration.GetConnectionString("Postgres")));
```

`using CustomSync.Data;` va `using Microsoft.EntityFrameworkCore;` qo'shing.

- [ ] **Step 5: Migratsiyani generatsiya qilish**

```bash
dotnet ef migrations add InitialSchema \
  --project src/CustomSync.Data \
  --startup-project src/CustomSync.Api
```

Kutilgan: `src/CustomSync.Data/Migrations/` da 3 ta fayl paydo bo'ladi.

- [ ] **Step 6: `sync_counter` ni migratsiyaga qo'lda qo'shish**

Generatsiya qilingan migratsiya faylini oching va `Up(MigrationBuilder)`
metodining **oxiriga** qo'shing:

```csharp
migrationBuilder.Sql("""
    CREATE TABLE sync_counter (
      id    INT PRIMARY KEY CHECK (id = 1),
      value BIGINT NOT NULL DEFAULT 0
    );
    INSERT INTO sync_counter (id, value) VALUES (1, 0);
""");
```

Va `Down(MigrationBuilder)` metodining **boshiga**:

```csharp
migrationBuilder.Sql("DROP TABLE IF EXISTS sync_counter;");
```

- [ ] **Step 7: Bazani yaratish va migratsiyani qo'llash**

```bash
psql -U postgres -c "CREATE USER customsync WITH PASSWORD 'CHANGE_ME';"
psql -U postgres -c "CREATE DATABASE customsync OWNER customsync;"
dotnet ef database update --project src/CustomSync.Data --startup-project src/CustomSync.Api
```

Kutilgan: `Done.` va bazada 9 ta jadval (8 entity + `sync_counter`
+ `__EFMigrationsHistory`).

- [ ] **Step 8: Tekshirish**

```bash
psql -U customsync -d customsync -c "\dt"
```

Kutilgan chiqish: `records`, `devices`, `media_blobs`, `record_media`,
`key_wraps`, `server_settings`, `enrollment_codes`, `audit_log`,
`sync_counter`, `__EFMigrationsHistory`.

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "feat: add PostgreSQL schema and DbContext

records uses record_id as the primary key rather than seq, because seq is
reassigned when a better observation supersedes a record -- other devices
have to re-pull the corrected copy, and a mutable primary key would be
wrong. sync_counter is raw SQL only; it is never touched through EF."
```

---

## Task 4: `SettingsService` — runtime konfiguratsiya

Qoida K1 ning amaliy asosi. Bundan keyingi **barcha** planlar sozlanadigan
qiymatlarni shu servisdan oladi.

**Files:**
- Create: `src/CustomSync.Services/SettingsService.cs`
- Create: `tests/CustomSync.Tests/Fixtures/DatabaseFixture.cs`
- Create: `tests/CustomSync.Tests/SettingsServiceTests.cs`

- [ ] **Step 1: Test fixture'ini yozish**

Har test ishga tushganda alohida baza yaratiladi — testlar bir-biriga
ta'sir qilmasligi uchun.

`tests/CustomSync.Tests/Fixtures/DatabaseFixture.cs`:

```csharp
using CustomSync.Data;
using Microsoft.EntityFrameworkCore;
using Npgsql;
using Xunit;

namespace CustomSync.Tests.Fixtures;

public class DatabaseFixture : IAsyncLifetime
{
    private const string AdminConnection =
        "Host=localhost;Port=5432;Database=postgres;Username=customsync;Password=CHANGE_ME";

    public string DatabaseName { get; } = $"customsync_test_{Guid.NewGuid():N}";
    public string ConnectionString => AdminConnection.Replace("Database=postgres", $"Database={DatabaseName}");

    public async Task InitializeAsync()
    {
        await using (var admin = new NpgsqlConnection(AdminConnection))
        {
            await admin.OpenAsync();
            await using var cmd = new NpgsqlCommand($"CREATE DATABASE \"{DatabaseName}\"", admin);
            await cmd.ExecuteNonQueryAsync();
        }

        await using var db = CreateContext();
        await db.Database.MigrateAsync();
    }

    public SyncDbContext CreateContext()
    {
        var options = new DbContextOptionsBuilder<SyncDbContext>()
            .UseNpgsql(ConnectionString)
            .Options;
        return new SyncDbContext(options);
    }

    public async Task DisposeAsync()
    {
        NpgsqlConnection.ClearAllPools();
        await using var admin = new NpgsqlConnection(AdminConnection);
        await admin.OpenAsync();
        await using var cmd = new NpgsqlCommand(
            $"DROP DATABASE IF EXISTS \"{DatabaseName}\" WITH (FORCE)", admin);
        await cmd.ExecuteNonQueryAsync();
    }
}
```

- [ ] **Step 2: Yiqiladigan testni yozish**

`tests/CustomSync.Tests/SettingsServiceTests.cs`:

```csharp
using CustomSync.Services;
using CustomSync.Tests.Fixtures;
using Xunit;

namespace CustomSync.Tests;

public class SettingsServiceTests : IClassFixture<DatabaseFixture>
{
    private readonly DatabaseFixture _fixture;

    public SettingsServiceTests(DatabaseFixture fixture) => _fixture = fixture;

    [Fact]
    public async Task Seeds_defaults_on_first_run()
    {
        await using var db = _fixture.CreateContext();
        var service = new SettingsService(db);

        await service.EnsureDefaultsAsync();

        Assert.Equal(500, await service.GetIntAsync("sync.push_batch_size"));
        Assert.Equal(50,  await service.GetIntAsync("api.default_page_size"));
    }

    [Fact]
    public async Task Set_then_get_returns_new_value_without_restart()
    {
        await using var db = _fixture.CreateContext();
        var service = new SettingsService(db);
        await service.EnsureDefaultsAsync();

        await service.SetAsync("sync.push_batch_size", "250");

        Assert.Equal(250, await service.GetIntAsync("sync.push_batch_size"));
    }

    [Fact]
    public async Task Unknown_key_throws_rather_than_returning_a_silent_default()
    {
        await using var db = _fixture.CreateContext();
        var service = new SettingsService(db);
        await service.EnsureDefaultsAsync();

        await Assert.ThrowsAsync<KeyNotFoundException>(
            () => service.GetIntAsync("does.not.exist"));
    }
}
```

- [ ] **Step 3: Testni ishga tushirib, yiqilishini ko'rish**

```bash
dotnet test --filter SettingsServiceTests
```

Kutilgan: FAIL — `SettingsService` mavjud emas.

- [ ] **Step 4: `SettingsService` ni yozish**

`src/CustomSync.Services/SettingsService.cs`:

```csharp
using System.Collections.Concurrent;
using System.Globalization;
using CustomSync.Data;
using CustomSync.Data.Entities;
using Microsoft.EntityFrameworkCore;

namespace CustomSync.Services;

/// <summary>
/// Runtime konfiguratsiya (qoida K1). Sozlanadigan qiymatlar kodda
/// literal bo'lmaydi — ular shu yerda yashaydi va web app'dan
/// qayta deploy qilmasdan o'zgartiriladi.
/// </summary>
public class SettingsService(SyncDbContext db)
{
    private static readonly ConcurrentDictionary<string, string> Cache = new();

    /// <summary>
    /// Standart qiymatlar. Yangi sozlama qo'shish = shu ro'yxatga bitta
    /// satr qo'shish; migratsiya kerak emas.
    /// </summary>
    public static readonly IReadOnlyList<ServerSettingEntity> Defaults =
    [
        New("sync.push_batch_size",       "500",  "int",      "sync",    "Bitta push so'rovidagi maksimal yozuvlar soni"),
        New("sync.push_max_bytes",        "5242880", "int",   "sync",    "Bitta push so'rovining maksimal hajmi (bayt)"),
        New("sync.pull_batch_size",       "500",  "int",      "sync",    "Bitta pull javobidagi maksimal yozuvlar soni"),
        New("sync.client_poll_seconds",   "30",   "int",      "sync",    "Klientlar necha soniyada bir pull qilishi"),
        New("api.default_page_size",      "50",   "int",      "api",     "Ro'yxatlar uchun standart sahifa hajmi"),
        New("api.max_page_size",          "200",  "int",      "api",     "So'ralishi mumkin bo'lgan maksimal sahifa hajmi"),
        New("auth.jwt_lifetime_minutes",  "60",   "int",      "auth",    "JWT amal qilish muddati"),
        New("auth.enroll_code_minutes",   "10",   "int",      "auth",    "Ro'yxatdan o'tkazish kodining amal qilish muddati"),
        New("auth.wrap_rate_per_hour",    "5",    "int",      "auth",    "Kalit o'ramini yuklab olish urinishlari (soatiga, IP bo'yicha)"),
        New("media.max_upload_bytes",     "52428800", "int",  "media",   "Bitta media faylning maksimal hajmi"),
    ];

    private static ServerSettingEntity New(
        string key, string value, string type, string category, string description)
        => new()
        {
            Key = key, Value = value, ValueType = type,
            Category = category, Description = description,
            UpdatedAt = DateTime.UtcNow
        };

    public async Task EnsureDefaultsAsync(CancellationToken ct = default)
    {
        var existing = await db.ServerSettings
            .Select(s => s.Key)
            .ToListAsync(ct);

        var missing = Defaults.Where(d => !existing.Contains(d.Key)).ToList();
        if (missing.Count > 0)
        {
            db.ServerSettings.AddRange(missing);
            await db.SaveChangesAsync(ct);
        }

        await ReloadCacheAsync(ct);
    }

    public async Task ReloadCacheAsync(CancellationToken ct = default)
    {
        var all = await db.ServerSettings.AsNoTracking().ToListAsync(ct);
        Cache.Clear();
        foreach (var s in all) Cache[s.Key] = s.Value;
    }

    public async Task SetAsync(string key, string value, CancellationToken ct = default)
    {
        var entity = await db.ServerSettings.FirstOrDefaultAsync(s => s.Key == key, ct)
            ?? throw new KeyNotFoundException($"Noma'lum sozlama: {key}");

        entity.Value = value;
        entity.UpdatedAt = DateTime.UtcNow;
        await db.SaveChangesAsync(ct);
        Cache[key] = value;
    }

    public Task<string> GetStringAsync(string key, CancellationToken ct = default)
    {
        if (Cache.TryGetValue(key, out var cached)) return Task.FromResult(cached);
        throw new KeyNotFoundException($"Noma'lum sozlama: {key}");
    }

    public async Task<int> GetIntAsync(string key, CancellationToken ct = default)
        => int.Parse(await GetStringAsync(key, ct), CultureInfo.InvariantCulture);

    public async Task<bool> GetBoolAsync(string key, CancellationToken ct = default)
        => bool.Parse(await GetStringAsync(key, ct));

    public async Task<IReadOnlyList<ServerSettingEntity>> ListAsync(
        CancellationToken ct = default)
        => await db.ServerSettings
            .AsNoTracking()
            .OrderBy(s => s.Category).ThenBy(s => s.Key)
            .ToListAsync(ct);
}
```

- [ ] **Step 5: Testni qayta ishga tushirish**

```bash
dotnet test --filter SettingsServiceTests
```

Kutilgan: PASS — 3 ta test o'tdi.

- [ ] **Step 6: Ishga tushishda avtomatik chaqirish**

`Program.cs` da `app.Run();` dan **oldin**:

```csharp
builder.Services.AddScoped<SettingsService>();

// ...builder.Build() dan keyin:
using (var scope = app.Services.CreateScope())
{
    var db = scope.ServiceProvider.GetRequiredService<SyncDbContext>();
    await db.Database.MigrateAsync();
    await scope.ServiceProvider.GetRequiredService<SettingsService>()
        .EnsureDefaultsAsync();
}
```

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat: add runtime settings service backed by the database

Every tunable value lives in server_settings and is editable from the web
app, so operational changes -- batch sizes, intervals, thresholds -- never
require a code edit or a redeploy. Unknown keys throw instead of returning
a silent default, so a typo surfaces immediately rather than quietly
running on the wrong value."
```

---

## Task 5: Qurilma ro'yxatdan o'tkazish va JWT

**Files:**
- Create: `src/CustomSync.Api/Auth/JwtIssuer.cs`
- Create: `src/CustomSync.Services/DeviceService.cs`
- Create: `src/CustomSync.Api/Endpoints/DeviceEndpoints.cs`
- Create: `tests/CustomSync.Tests/DeviceAuthTests.cs`
- Modify: `src/CustomSync.Api/Program.cs`, `appsettings.json`

- [ ] **Step 1: Paket qo'shish**

```bash
dotnet add src/CustomSync.Api package Microsoft.AspNetCore.Authentication.JwtBearer
```

- [ ] **Step 2: Yiqiladigan testni yozish**

`tests/CustomSync.Tests/DeviceAuthTests.cs`:

```csharp
using CustomSync.Services;
using CustomSync.Tests.Fixtures;
using Xunit;

namespace CustomSync.Tests;

public class DeviceAuthTests : IClassFixture<DatabaseFixture>
{
    private readonly DatabaseFixture _fixture;

    public DeviceAuthTests(DatabaseFixture fixture) => _fixture = fixture;

    private async Task<DeviceService> CreateServiceAsync(Data.SyncDbContext db)
    {
        var settings = new SettingsService(db);
        await settings.EnsureDefaultsAsync();
        return new DeviceService(db, settings);
    }

    [Fact]
    public async Task Enrollment_code_can_be_redeemed_once()
    {
        await using var db = _fixture.CreateContext();
        var service = await CreateServiceAsync(db);
        var code = await service.CreateEnrollmentCodeAsync();

        var first = await service.RedeemAsync(code, "laptop", "desktop-win");
        var second = await service.RedeemAsync(code, "phone", "android");

        Assert.NotNull(first);
        Assert.Null(second);
    }

    [Fact]
    public async Task Expired_code_is_rejected()
    {
        await using var db = _fixture.CreateContext();
        var service = await CreateServiceAsync(db);
        var code = await service.CreateEnrollmentCodeAsync();

        await service.ExpireAllCodesAsync();

        Assert.Null(await service.RedeemAsync(code, "laptop", "desktop-win"));
    }

    [Fact]
    public async Task Revoked_device_cannot_refresh()
    {
        await using var db = _fixture.CreateContext();
        var service = await CreateServiceAsync(db);
        var code = await service.CreateEnrollmentCodeAsync();
        var enrolled = await service.RedeemAsync(code, "laptop", "desktop-win");

        await service.RevokeAsync(enrolled!.DeviceId);

        Assert.Null(await service.RefreshAsync(enrolled.DeviceId, enrolled.RefreshToken));
    }

    [Fact]
    public async Task Refresh_rotates_the_token()
    {
        await using var db = _fixture.CreateContext();
        var service = await CreateServiceAsync(db);
        var code = await service.CreateEnrollmentCodeAsync();
        var enrolled = await service.RedeemAsync(code, "laptop", "desktop-win");

        var refreshed = await service.RefreshAsync(enrolled!.DeviceId, enrolled.RefreshToken);

        Assert.NotNull(refreshed);
        Assert.NotEqual(enrolled.RefreshToken, refreshed!.RefreshToken);
        Assert.Null(await service.RefreshAsync(enrolled.DeviceId, enrolled.RefreshToken));
    }
}
```

- [ ] **Step 3: Testni ishga tushirib, yiqilishini ko'rish**

```bash
dotnet test --filter DeviceAuthTests
```

Kutilgan: FAIL — `DeviceService` mavjud emas.

- [ ] **Step 4: `DeviceService` ni yozish**

`src/CustomSync.Services/DeviceService.cs`:

```csharp
using System.Security.Cryptography;
using System.Text;
using CustomSync.Data;
using CustomSync.Data.Entities;
using Microsoft.EntityFrameworkCore;

namespace CustomSync.Services;

public sealed record EnrolledDevice(
    string DeviceId, string RefreshToken, string Name, string Platform);

public class DeviceService(SyncDbContext db, SettingsService settings)
{
    /// <summary>
    /// Bir martalik ro'yxatdan o'tkazish kodi. Web app'da ko'rsatiladi,
    /// qurilmaga qo'lda kiritiladi. Bazada faqat hash saqlanadi.
    /// </summary>
    public async Task<string> CreateEnrollmentCodeAsync(CancellationToken ct = default)
    {
        var code = Base32(RandomNumberGenerator.GetBytes(10)); // 80 bit
        var minutes = await settings.GetIntAsync("auth.enroll_code_minutes", ct);

        db.EnrollmentCodes.Add(new EnrollmentCodeEntity
        {
            CodeHash  = Sha256Hex(code),
            CreatedAt = DateTime.UtcNow,
            ExpiresAt = DateTime.UtcNow.AddMinutes(minutes)
        });
        await db.SaveChangesAsync(ct);
        return code;
    }

    public async Task<EnrolledDevice?> RedeemAsync(
        string code, string name, string platform, CancellationToken ct = default)
    {
        var hash = Sha256Hex(code);
        var entry = await db.EnrollmentCodes
            .FirstOrDefaultAsync(c => c.CodeHash == hash, ct);

        if (entry is null || entry.UsedAt is not null || entry.ExpiresAt < DateTime.UtcNow)
            return null;

        var deviceId    = $"{platform}-{Guid.NewGuid():N}"[..24];
        var refreshToken = Base32(RandomNumberGenerator.GetBytes(32));

        db.Devices.Add(new DeviceEntity
        {
            DeviceId    = deviceId,
            Name        = name,
            Platform    = platform,
            EnrolledAt  = DateTime.UtcNow,
            LastCursor  = 0,
            RefreshHash = Sha256Hex(refreshToken)
        });

        entry.UsedAt = DateTime.UtcNow;
        entry.UsedBy = deviceId;
        await db.SaveChangesAsync(ct);

        return new EnrolledDevice(deviceId, refreshToken, name, platform);
    }

    /// <summary>
    /// Refresh token har ishlatilganda almashtiriladi (rotation) — o'g'irlangan
    /// eski token qayta ishlatilmasligi uchun.
    /// </summary>
    public async Task<EnrolledDevice?> RefreshAsync(
        string deviceId, string refreshToken, CancellationToken ct = default)
    {
        var device = await db.Devices.FirstOrDefaultAsync(d => d.DeviceId == deviceId, ct);
        if (device is null || device.RevokedAt is not null) return null;
        if (!FixedTimeEquals(device.RefreshHash, Sha256Hex(refreshToken))) return null;

        var rotated = Base32(RandomNumberGenerator.GetBytes(32));
        device.RefreshHash = Sha256Hex(rotated);
        device.LastSeenAt  = DateTime.UtcNow;
        await db.SaveChangesAsync(ct);

        return new EnrolledDevice(device.DeviceId, rotated, device.Name, device.Platform);
    }

    public async Task RevokeAsync(string deviceId, CancellationToken ct = default)
    {
        var device = await db.Devices.FirstOrDefaultAsync(d => d.DeviceId == deviceId, ct);
        if (device is null) return;
        device.RevokedAt = DateTime.UtcNow;
        await db.SaveChangesAsync(ct);
    }

    public async Task<bool> IsActiveAsync(string deviceId, CancellationToken ct = default)
        => await db.Devices.AnyAsync(
            d => d.DeviceId == deviceId && d.RevokedAt == null, ct);

    /// <summary>Test yordamchisi: barcha amaldagi kodlarni muddati o'tgan qiladi.</summary>
    public async Task ExpireAllCodesAsync(CancellationToken ct = default)
    {
        await db.EnrollmentCodes
            .Where(c => c.UsedAt == null)
            .ExecuteUpdateAsync(s => s.SetProperty(
                c => c.ExpiresAt, DateTime.UtcNow.AddMinutes(-1)), ct);
    }

    public async Task<IReadOnlyList<DeviceEntity>> ListAsync(CancellationToken ct = default)
        => await db.Devices.AsNoTracking().OrderBy(d => d.EnrolledAt).ToListAsync(ct);

    private static string Sha256Hex(string value)
        => Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(value)))
                  .ToLowerInvariant();

    private static bool FixedTimeEquals(string a, string b)
        => CryptographicOperations.FixedTimeEquals(
            Encoding.UTF8.GetBytes(a), Encoding.UTF8.GetBytes(b));

    /// <summary>Chalkashadigan belgilarsiz (0/O, 1/I) — qo'lda kiritish uchun.</summary>
    private static string Base32(byte[] bytes)
    {
        const string alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        var sb = new StringBuilder();
        foreach (var b in bytes)
        {
            sb.Append(alphabet[b >> 3]);
            if (sb.Length % 5 == 4) sb.Append('-');
        }
        return sb.ToString().TrimEnd('-');
    }
}
```

- [ ] **Step 5: Testni qayta ishga tushirish**

```bash
dotnet test --filter DeviceAuthTests
```

Kutilgan: PASS — 4 ta test o'tdi.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat: add device enrollment with rotating refresh tokens

Enrollment codes and refresh tokens are stored only as hashes, and refresh
rotates the token on every use so a captured token is single-use. Codes are
base32 without visually ambiguous characters because they are typed by hand
on the enrolling device."
```

---

## Task 6: JWT chiqarish va endpoint'lar

**Files:**
- Create: `src/CustomSync.Api/Auth/JwtIssuer.cs`
- Create: `src/CustomSync.Api/Endpoints/DeviceEndpoints.cs`
- Create: `src/CustomSync.Api/Endpoints/SettingsEndpoints.cs`
- Modify: `src/CustomSync.Api/Program.cs`, `appsettings.json`

- [ ] **Step 1: JWT kalitini konfiguratsiyaga qo'shish**

`appsettings.json` ga qo'shing (bootstrap qiymat — K1 dan istisno, chunki
uni o'zgartirish barcha tokenlarni bekor qiladi):

```json
"Jwt": {
  "Issuer": "customsync",
  "Audience": "customsync-clients",
  "SigningKey": "CHANGE_ME_AT_LEAST_32_BYTES_LONG_RANDOM"
}
```

- [ ] **Step 2: `JwtIssuer` ni yozish**

`src/CustomSync.Api/Auth/JwtIssuer.cs`:

```csharp
using System.IdentityModel.Tokens.Jwt;
using System.Security.Claims;
using System.Text;
using CustomSync.Services;
using Microsoft.IdentityModel.Tokens;

namespace CustomSync.Api.Auth;

public class JwtIssuer(IConfiguration config, SettingsService settings)
{
    public async Task<(string Token, DateTime ExpiresAt)> IssueAsync(
        string deviceId, CancellationToken ct = default)
    {
        var minutes  = await settings.GetIntAsync("auth.jwt_lifetime_minutes", ct);
        var expires  = DateTime.UtcNow.AddMinutes(minutes);
        var key      = new SymmetricSecurityKey(
            Encoding.UTF8.GetBytes(config["Jwt:SigningKey"]!));

        var token = new JwtSecurityToken(
            issuer:             config["Jwt:Issuer"],
            audience:           config["Jwt:Audience"],
            claims:             [new Claim(ClaimTypes.NameIdentifier, deviceId)],
            expires:            expires,
            signingCredentials: new SigningCredentials(key, SecurityAlgorithms.HmacSha256));

        return (new JwtSecurityTokenHandler().WriteToken(token), expires);
    }
}
```

- [ ] **Step 3: Autentifikatsiyani sozlash**

`Program.cs` da `builder.Build()` dan oldin:

```csharp
builder.Services.AddScoped<DeviceService>();
builder.Services.AddScoped<JwtIssuer>();

builder.Services.AddAuthentication(JwtBearerDefaults.AuthenticationScheme)
    .AddJwtBearer(o =>
    {
        o.TokenValidationParameters = new TokenValidationParameters
        {
            ValidIssuer      = builder.Configuration["Jwt:Issuer"],
            ValidAudience    = builder.Configuration["Jwt:Audience"],
            IssuerSigningKey = new SymmetricSecurityKey(
                Encoding.UTF8.GetBytes(builder.Configuration["Jwt:SigningKey"]!)),
            ValidateIssuerSigningKey = true,
            ClockSkew = TimeSpan.FromSeconds(30)
        };
    });
builder.Services.AddAuthorization();
```

`app.Run()` dan oldin, endpoint'lardan **avval**:

```csharp
app.UseAuthentication();
app.UseAuthorization();
```

- [ ] **Step 4: Endpoint'larni yozish**

`src/CustomSync.Api/Endpoints/DeviceEndpoints.cs`:

```csharp
using System.Security.Claims;
using CustomSync.Api.Auth;
using CustomSync.Services;

namespace CustomSync.Api.Endpoints;

public static class DeviceEndpoints
{
    public record RedeemRequest(string Code, string Name, string Platform);
    public record RefreshRequest(string DeviceId, string RefreshToken);

    public static void MapDeviceEndpoints(this WebApplication app)
    {
        var group = app.MapGroup("/api/v1/devices");

        // Ochiq: qurilma hali tokenga ega emas. Kodning o'zi maxfiy.
        group.MapPost("/enroll", async (
            RedeemRequest request, DeviceService devices, JwtIssuer jwt) =>
        {
            var enrolled = await devices.RedeemAsync(
                request.Code, request.Name, request.Platform);
            if (enrolled is null)
                return Results.BadRequest(new { error = "invalid_or_used_code" });

            var (token, expiresAt) = await jwt.IssueAsync(enrolled.DeviceId);
            return Results.Ok(new
            {
                deviceId     = enrolled.DeviceId,
                refreshToken = enrolled.RefreshToken,
                accessToken  = token,
                expiresAt
            });
        });

        group.MapPost("/refresh", async (
            RefreshRequest request, DeviceService devices, JwtIssuer jwt) =>
        {
            var refreshed = await devices.RefreshAsync(
                request.DeviceId, request.RefreshToken);
            if (refreshed is null)
                return Results.Unauthorized();

            var (token, expiresAt) = await jwt.IssueAsync(refreshed.DeviceId);
            return Results.Ok(new
            {
                refreshToken = refreshed.RefreshToken,
                accessToken  = token,
                expiresAt
            });
        });

        group.MapGet("/", async (DeviceService devices) =>
            Results.Ok(await devices.ListAsync()))
            .RequireAuthorization();

        group.MapPost("/codes", async (DeviceService devices) =>
            Results.Ok(new { code = await devices.CreateEnrollmentCodeAsync() }))
            .RequireAuthorization();

        group.MapDelete("/{deviceId}", async (
            string deviceId, DeviceService devices, ClaimsPrincipal user) =>
        {
            await devices.RevokeAsync(deviceId);
            return Results.NoContent();
        }).RequireAuthorization();
    }
}
```

`src/CustomSync.Api/Endpoints/SettingsEndpoints.cs`:

```csharp
using CustomSync.Services;

namespace CustomSync.Api.Endpoints;

public static class SettingsEndpoints
{
    public record UpdateRequest(string Value);

    public static void MapSettingsEndpoints(this WebApplication app)
    {
        var group = app.MapGroup("/api/v1/settings").RequireAuthorization();

        group.MapGet("/", async (SettingsService settings) =>
            Results.Ok(await settings.ListAsync()));

        group.MapPut("/{key}", async (
            string key, UpdateRequest request, SettingsService settings) =>
        {
            try
            {
                await settings.SetAsync(key, request.Value);
                return Results.NoContent();
            }
            catch (KeyNotFoundException)
            {
                return Results.NotFound(new { error = "unknown_setting", key });
            }
        });
    }
}
```

`Program.cs` da ro'yxatdan o'tkazing:

```csharp
app.MapDeviceEndpoints();
app.MapSettingsEndpoints();
```

- [ ] **Step 5: Qo'lda tekshirish**

```bash
dotnet run --project src/CustomSync.Api
```

Boshqa terminalda — birinchi kod bazadan qo'lda olinadi (hali
autentifikatsiyalangan klient yo'q):

```bash
psql -U customsync -d customsync -c "SELECT code_hash FROM enrollment_codes;"
```

Kutilgan: bo'sh jadval. Bootstrap kodi keyingi qadamda hal qilinadi.

- [ ] **Step 6: Bootstrap kodi uchun CLI argumenti qo'shish**

Birinchi qurilmani ulash uchun kod kerak, lekin kod yaratish
autentifikatsiya talab qiladi. `Program.cs` da migratsiyadan keyin:

```csharp
if (args.Contains("--create-enrollment-code"))
{
    using var bootstrapScope = app.Services.CreateScope();
    var devices = bootstrapScope.ServiceProvider.GetRequiredService<DeviceService>();
    Console.WriteLine($"Enrollment code: {await devices.CreateEnrollmentCodeAsync()}");
    return;
}
```

- [ ] **Step 7: To'liq oqimni tekshirish**

```bash
dotnet run --project src/CustomSync.Api -- --create-enrollment-code
```

Kutilgan: `Enrollment code: ABCD-EFGH-...`

Serverni ishga tushiring va kodni ishlating:

```bash
curl -s -X POST http://localhost:5000/api/v1/devices/enroll \
  -H "Content-Type: application/json" \
  -d '{"code":"KODNI-BU-YERGA","name":"dev-laptop","platform":"desktop-win"}'
```

Kutilgan: `deviceId`, `refreshToken`, `accessToken`, `expiresAt` bo'lgan JSON.

Xuddi shu kodni ikkinchi marta ishlatib ko'ring — kutilgan:
`{"error":"invalid_or_used_code"}`.

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "feat: add JWT issuance and device/settings endpoints

Adds a --create-enrollment-code CLI path to break the bootstrap cycle:
creating a code normally requires authentication, but the first device has
no token yet."
```

---

## Task 7: Serilog va audit log

**Files:**
- Create: `src/CustomSync.Services/AuditService.cs`
- Modify: `src/CustomSync.Api/Program.cs`, `appsettings.json`

- [ ] **Step 1: Paketlarni qo'shish**

```bash
dotnet add src/CustomSync.Api package Serilog.AspNetCore
dotnet add src/CustomSync.Api package Serilog.Sinks.File
```

- [ ] **Step 2: `AuditService` ni yozish**

`src/CustomSync.Services/AuditService.cs`:

```csharp
using System.Text.Json;
using CustomSync.Data;
using CustomSync.Data.Entities;
using Microsoft.EntityFrameworkCore;

namespace CustomSync.Services;

/// <summary>
/// Xavfsizlik ahamiyatiga ega hodisalar jurnali — qurilma ulanishi,
/// bekor qilinishi, kalit o'rami o'zgarishi, ma'lumot o'chirilishi.
/// Fayl loglaridan farqi: bu jadval hech qachon rotatsiya qilinmaydi
/// va web app'dan ko'riladi.
/// </summary>
public class AuditService(SyncDbContext db)
{
    public async Task WriteAsync(
        string action, string? deviceId = null, object? detail = null,
        CancellationToken ct = default)
    {
        db.AuditLogs.Add(new AuditLogEntity
        {
            At       = DateTime.UtcNow,
            DeviceId = deviceId,
            Action   = action,
            Detail   = detail is null ? null : JsonSerializer.Serialize(detail)
        });
        await db.SaveChangesAsync(ct);
    }

    public async Task<IReadOnlyList<AuditLogEntity>> RecentAsync(
        int limit, CancellationToken ct = default)
        => await db.AuditLogs.AsNoTracking()
            .OrderByDescending(a => a.At)
            .Take(limit)
            .ToListAsync(ct);
}
```

- [ ] **Step 3: Serilog'ni ulash**

`Program.cs` ning **birinchi** satrlariga:

```csharp
using Serilog;

Log.Logger = new LoggerConfiguration()
    .WriteTo.Console()
    .WriteTo.File("logs/customsync-.log", rollingInterval: RollingInterval.Day,
                  retainedFileCountLimit: 14)
    .CreateLogger();
```

`builder` yaratilgandan keyin:

```csharp
builder.Host.UseSerilog();
builder.Services.AddScoped<AuditService>();
```

`app.UseAuthentication()` dan oldin:

```csharp
app.UseSerilogRequestLogging();
```

- [ ] **Step 4: Audit yozuvlarini qo'shish**

`DeviceEndpoints.cs` da `enroll` muvaffaqiyatli bo'lganda va `revoke`
chaqirilganda `AuditService` ga yozing. `enroll` ichida, `Results.Ok`
dan oldin:

```csharp
await audit.WriteAsync("device.enrolled", enrolled.DeviceId,
    new { enrolled.Name, enrolled.Platform });
```

`AuditService audit` parametrini handler imzosiga qo'shing. Xuddi shunday
`revoke` uchun `"device.revoked"`.

- [ ] **Step 5: Tekshirish**

Serverni ishga tushirib, yangi qurilma ulang, so'ng:

```bash
psql -U customsync -d customsync -c "SELECT at, action, device_id FROM audit_log ORDER BY at DESC LIMIT 5;"
```

Kutilgan: `device.enrolled` qatori ko'rinadi.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat: add structured logging and a security audit trail

The audit table is separate from the rolling file logs because it must
survive log rotation and be queryable from the web app -- device
enrolments, revocations and later key-wrap and purge operations all need
a permanent record."
```

---

## Qabul qilish mezonlari (1a)

1. `dotnet test` — barcha testlar o'tadi (14 ta test).
2. `dotnet run --project src/CustomSync.Api -- --create-enrollment-code`
   ishlaydigan kod chiqaradi.
3. Kod bilan qurilma ulanadi; xuddi shu kod ikkinchi marta rad etiladi.
4. Bekor qilingan qurilma `refresh` qila olmaydi.
5. `PUT /api/v1/settings/{key}` qiymatni o'zgartiradi va o'zgarish
   **qayta ishga tushirmasdan** kuchga kiradi.
6. `audit_log` da qurilma ulanishi va bekor qilinishi qayd etilgan.

---

## Keyingi qadam

[01b — Sync yadrosi](2026-07-29-multi-device-sync-01b-backend-sync.md):
`seq` ajratish, push/pull, media, kalit o'ramlari, keyset pagination,
statistika, WebSocket va `.cmx` import/eksport.
