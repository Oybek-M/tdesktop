# Always-On Capture Service Implementation Plan (05)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** VPS'da 24/7 ishlaydigan TDLib asosidagi xizmat — o'chirilgan va tahrirlangan xabarlarni ilova yopiq bo'lganda ham ushlab qolish. **Muammo A shu bilan hal bo'ladi.**

**Architecture:** Backend'dan **alohida jarayon**. U hech qanday maxsus imtiyozga ega emas — backend nuqtai nazaridan bu shunchaki yana bitta qurilma: o'z `device_id` si bilan ro'yxatdan o'tadi va xuddi tdesktop kabi `/api/v1/sync/push` qiladi. Backend'da birorta ham maxsus holat kodi yo'q.

**Tech Stack:** .NET 8 Worker Service, TDLib (tdjson, to'g'ridan-to'g'ri P/Invoke), SQLite (o'z lokal keshi uchun).

**Kirish sharti:** [01b](2026-07-29-multi-device-sync-01b-backend-sync.md) va [04](2026-07-29-multi-device-sync-04-storage-lifecycle.md) tugagan. **04 majburiy** — bu xizmat doimiy ma'lumot oqimi hosil qiladi va xotira boshqaruvisiz disk tez to'ladi.

**Umumiy qoidalar:** [00-index](2026-07-29-multi-device-sync-00-index.md) dagi K1–K7.

---

> ## ⚠️ REVIZIYA 2026-08-25 — bu planga tegishli o'zgarishlar
>
> To'liq ro'yxat: spec **§0**.
>
> Capture service ham **oddiy klient** — ya'ni boshqa klientlar
> uchun amal qiladigan qoidalar unga ham tegishli:
>
> | Qoida | Manba |
> |---|---|
> | `sha256` — ochiq matn ustidan, shifrlashdan OLDIN | §0.5 |
> | Manfiy `msg_id` (avatar/story) ishorasi saqlanadi | §0.6 |
> | Pull'da retention filtri — aks holda cheksiz sikl | §0.3 |
> | `tombstone` qabul qilinadi | §0.3 |
> | Media yo'llari kodda saqlanmaydi | §0.8 |
>
> ### Task 8 (media siyosati) — `media_index` bilan
> Service media yuklaganda `media_index` yozuvi ham push qilinadi
> (§0.4). Bu uning eng katta qiymati: service 24/7 ulangan, ya'ni
> u **hech kim ko'rmagan** media'ni ham ushlab qoladi va boshqa
> qurilmalar uni o'sha yerdan oladi.
>
> ### Task 9 (xotira) — kvota
> Service o'z kvotasiga ega bo'lishi kerak (§0.9). To'lganda
> `pending/quota_full` yoziladi — ma'lumot yo'qolmaydi, faqat fayl
> keyinroq olinadi.

---

## Nima uchun alohida jarayon

| Sabab | Foyda |
|---|---|
| TDLib xotira sarfi izolyatsiya qilinadi | `systemd` uni alohida o'lchaydi va `MemoryMax=` bilan cheklaydi |
| TDLib crash bo'lsa API o'lmaydi | Web app va tdesktop sync ishlashda davom etadi |
| Alohida restart va monitoring | Muammo bo'lsa faqat shu qismni qayta ishga tushirasiz |
| Backend'da maxsus kod yo'q | Xizmat oddiy sync klienti — kontrakt bir xil |

---

## Oldindan bilinishi kerak bo'lgan cheklovlar

**1. O'chirilgan xabar matni oldindan keshlangan bo'lishi SHART.**
Telegram `updateDeleteMessages` da faqat ID yuboradi. Shuning uchun bu
xizmat **har bir kelgan xabarni** o'z lokal keshiga yozadi — aynan
tdesktop'ning `text_cache` jadvali kabi. Kesh bo'lmasa, o'chirish
hodisasi kelganda matn allaqachon yo'q bo'ladi.

**2. Media alohida masala.** O'chirilgan rasm/videoni saqlash uchun uni
o'chirilishidan **oldin** yuklab olgan bo'lish kerak. Hamma chatdagi
hamma media'ni yuklab olish diskni juda tez to'ldiradi, shuning uchun
media yuklash **faqat aniq ro'yxatdagi peerlar uchun** yoqiladi
(Task 8).

**3. TDLib build'i qimmat.** `libtdjson` ni kompilyatsiya qilish ~8 GB
RAM talab qiladi. Contabo VPS'da RAM yetmasligi mumkin — u holda
boshqa mashinada build qilib, faqat `.so` faylini ko'chirish kerak
(Task 1).

**4. `api_id` / `api_hash`** — my.telegram.org dan olinadi. Bu shaxsiy
qiymatlar va repozitoriyga **commit qilinmaydi**.

---

## File Structure

```
src/CustomSync.Capture/
├── Program.cs
├── appsettings.json
├── Tdlib/
│   ├── TdJsonInterop.cs        # tdjson ga P/Invoke
│   ├── TdClient.cs             # send/receive sikli, so'rov-javob moslashtirish
│   └── TdAuthenticator.cs      # telefon → kod → 2FA oqimi
├── Capture/
│   ├── MessageCache.cs         # LOKAL SQLite kesh — o'chirish uchun zarur
│   ├── UpdateHandler.cs        # TDLib update → Record
│   └── ScopeEvaluator.cs       # qaysi chatlar kuzatiladi
├── Sync/
│   └── CaptureSyncClient.cs    # backend API'ga push
└── Maintenance/
    └── StorageMaintenance.cs   # TDLib optimizeStorage, kesh tozalash

deploy/customsync-capture.service
```

---

## Task 1: TDLib'ni tayyorlash va interop

**Files:**
- Create: `src/CustomSync.Capture/` loyihasi
- Create: `src/CustomSync.Capture/Tdlib/TdJsonInterop.cs`
- Create: `src/CustomSync.Capture/Tdlib/TdClient.cs`

- [ ] **Step 1: `libtdjson` ni olish**

```bash
# ≥8 GB RAM li mashinada (VPS'da emas, agar RAM yetmasa):
git clone https://github.com/tdlib/td.git && cd td
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --target tdjson -- -j2
```

Natija: `libtdjson.so` (Linux) yoki `tdjson.dll` (Windows).

**RAM yetmasa:** `-j1` bilan build qiling va swap qo'shing, yoki boshqa
mashinada build qilib faylni ko'chiring. Bu bir martalik ish.

- [ ] **Step 2: Loyihani yaratish**

```bash
dotnet new worker -n CustomSync.Capture -o src/CustomSync.Capture
dotnet sln add src/CustomSync.Capture
dotnet add src/CustomSync.Capture reference src/CustomSync.Core
dotnet add src/CustomSync.Capture package Microsoft.Data.Sqlite
```

`CustomSync.Core` ga havola muhim: `RecordId` va yozuv kontrakti
backend bilan **aynan bir xil** bo'lishi kerak.

- [ ] **Step 3: Interop qatlamini yozish**

```csharp
using System.Runtime.InteropServices;

namespace CustomSync.Capture.Tdlib;

/// <summary>
/// tdjson ning JSON interfeysiga to'g'ridan-to'g'ri P/Invoke.
///
/// Rasmiy .NET binding o'rniga shu tanlandi: JSON interfeysi barqaror
/// va versiyaga kam bog'liq, binding esa TDLib versiyasi bilan aniq
/// mos kelishi kerak va har yangilanishda qayta generatsiya talab qiladi.
/// </summary>
internal static class TdJsonInterop
{
    private const string Library = "tdjson";

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    public static extern int td_create_client_id();

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    public static extern void td_send(int clientId, IntPtr request);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr td_receive(double timeout);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr td_execute(IntPtr request);
}
```

- [ ] **Step 4: Klient siklini yozish**

`TdClient.cs` — bitta fon oqimida `td_receive` siklini yuritadi, kelgan
JSON'ni `@extra` maydoni bo'yicha kutayotgan so'rovga moslashtiradi,
qolganini update sifatida tarqatadi.

**Muhim:** `td_receive` **faqat bitta oqimdan** chaqirilishi kerak.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "chore: add TDLib JSON interop and client loop

Direct P/Invoke to the JSON interface rather than the generated .NET
binding: the JSON layer is stable across TDLib versions, while the binding
has to be regenerated and version-matched on every upgrade."
```

---

## Task 2: Autentifikatsiya

**Files:**
- Create: `src/CustomSync.Capture/Tdlib/TdAuthenticator.cs`

- [ ] **Step 1: Sozlamalarni tayyorlash**

`appsettings.json` (maxfiy qiymatlarsiz — ular
`appsettings.Production.json` da va u `.gitignore` da):

```json
{
  "Telegram": {
    "ApiId": 0,
    "ApiHash": "",
    "DatabaseDirectory": "/var/lib/customsync-capture/tdlib",
    "FilesDirectory": "/var/lib/customsync-capture/files"
  },
  "Backend": {
    "BaseUrl": "https://sync.example.uz"
  }
}
```

`.gitignore` ga: `appsettings.Production.json`

- [ ] **Step 2: Autentifikatsiya oqimini yozish**

TDLib `updateAuthorizationState` bo'yicha holat mashinasi:

```
waitTdlibParameters  → setTdlibParameters
waitPhoneNumber      → setAuthenticationPhoneNumber
waitCode             → checkAuthenticationCode      (interaktiv)
waitPassword         → checkAuthenticationPassword  (interaktiv)
ready                → tayyor
```

Kod va 2FA paroli **bir martalik interaktiv** o'rnatishda kiritiladi:

```bash
dotnet run --project src/CustomSync.Capture -- --login
```

Bu rejim konsoldan kod so'raydi. Muvaffaqiyatdan keyin sessiya
`DatabaseDirectory` da saqlanadi va keyingi ishga tushishlarda
qayta so'ralmaydi.

**`TdlibParameters` da:**

```csharp
use_message_database   = true,   // o'chirish paytida getMessage uchun
use_file_database      = true,
use_chat_info_database = true,
use_secret_chats       = false,  // qurilmaga bog'liq, sync qilinmaydi
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: add interactive one-time TDLib login

Login runs as an explicit --login mode rather than prompting inside the
service: a systemd unit has no console to read a code from, and the
session persists in the TDLib database afterwards anyway.

Secret chats are disabled -- they are device-bound by design and cannot
be synced."
```

---

## Task 3: Lokal xabar keshi

Bu xizmatning ishlashi uchun **majburiy** qism.

**Files:**
- Create: `src/CustomSync.Capture/Capture/MessageCache.cs`

- [ ] **Step 1: Keshni yozish**

```csharp
using Microsoft.Data.Sqlite;

namespace CustomSync.Capture.Capture;

/// <summary>
/// Kelgan xabarlarning lokal keshi.
///
/// NIMA UCHUN KERAK: Telegram updateDeleteMessages da faqat ID yuboradi,
/// matnni yubormaydi. Xabar o'chirilgandan keyin uni hech qayerdan
/// olib bo'lmaydi. Shuning uchun har bir kelgan xabar OLDINDAN shu
/// yerga yoziladi — o'chirish hodisasi kelganda matn shu keshdan olinadi.
///
/// Bu tdesktop'dagi text_cache jadvalining aynan shu maqsaddagi
/// ekvivalenti.
/// </summary>
public class MessageCache(string databasePath)
{
    public void Initialize()
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = """
            CREATE TABLE IF NOT EXISTS message_cache (
              chat_id    INTEGER NOT NULL,
              message_id INTEGER NOT NULL,
              text       TEXT,
              sender_id  TEXT,
              is_out     INTEGER NOT NULL DEFAULT 0,
              is_media   INTEGER NOT NULL DEFAULT 0,
              media_id   TEXT,
              date       INTEGER NOT NULL,
              cached_at  INTEGER NOT NULL,
              PRIMARY KEY (chat_id, message_id)
            );
            CREATE INDEX IF NOT EXISTS idx_cache_age
              ON message_cache(cached_at);
            """;
        command.ExecuteNonQuery();
    }

    public void Put(CachedMessage message) { /* INSERT OR REPLACE */ }

    public CachedMessage? Get(long chatId, long messageId) { /* SELECT */ return null; }

    /// <summary>
    /// Eski yozuvlarni tozalash. Kesh cheksiz o'smasligi kerak —
    /// u faqat "yaqinda kelgan xabar o'chirilsa matni topilsin" uchun.
    /// Standart 30 kun; sozlanadi.
    /// </summary>
    public int Prune(int olderThanDays) { /* DELETE */ return 0; }

    private SqliteConnection Open()
    {
        var connection = new SqliteConnection($"Data Source={databasePath}");
        connection.Open();
        return connection;
    }
}

public sealed record CachedMessage(
    long ChatId, long MessageId, string? Text, string? SenderId,
    bool IsOut, bool IsMedia, string? MediaId, long Date);
```

- [ ] **Step 2: Commit**

```bash
git add -A
git commit -m "feat: add local message cache for the capture service

Without this the service cannot do anything useful: updateDeleteMessages
carries only ids, so a message whose text was not stored before it was
deleted is gone permanently. The cache is pruned on a timer because it
only ever needs to cover the recent past."
```

---

## Task 4: Update handler'lari

**Files:**
- Create: `src/CustomSync.Capture/Capture/UpdateHandler.cs`

- [ ] **Step 1: Kuzatiladigan update'lar**

| TDLib update | Nima qilinadi |
|---|---|
| `updateNewMessage` | Keshga yoziladi (qamrovga kirsa) |
| `updateMessageContent` | Keshdagi eski matn bilan solishtiriladi → `edited` yozuvi |
| `updateDeleteMessages` | Keshdan matn olinadi → `deleted` yozuvi |
| `updateUserStatus` | `activity` yozuvi (status o'zgarishi) |
| `updateUser` | Ism/username o'zgarishi → `activity` yozuvi |

- [ ] **Step 2: O'chirish handler'ini yozish**

```csharp
/// <summary>
/// O'chirish hodisasi.
///
/// MUHIM ikkita bayroq:
///   is_permanent = false → xabar haqiqatan o'chirilmagan, faqat shu
///                          sessiyada ko'rinmay qolgan. E'tiborsiz.
///   from_cache   = true  → TDLib o'z keshidan chiqargan, Telegram'da
///                          xabar hali turibdi. E'tiborsiz.
///
/// Bu bayroqlarni tekshirmaslik yolg'on "o'chirildi" yozuvlarini
/// keltirib chiqaradi va arxivni ishonchsiz qiladi.
/// </summary>
private async Task HandleDeleteAsync(JsonElement update)
{
    if (!update.GetProperty("is_permanent").GetBoolean()) return;
    if (update.GetProperty("from_cache").GetBoolean()) return;

    var chatId = update.GetProperty("chat_id").GetInt64();
    if (!await _scope.ShouldCaptureAsync(chatId)) return;

    foreach (var element in update.GetProperty("message_ids").EnumerateArray())
    {
        var messageId = element.GetInt64();
        var cached = _cache.Get(chatId, messageId);
        if (cached is null) continue;   // kesh davridan oldingi xabar

        await _sync.EnqueueDeletedAsync(cached);
    }
}
```

- [ ] **Step 3: Tahrirlash handler'ini yozish**

`updateMessageContent` yangi matnni beradi; eski matn keshdan olinadi.
Eski matn topilmasa yozuv **yaratilmaydi** — "oldingi holati noma'lum"
degan yozuv foydasiz.

Yozuvdan keyin kesh yangi matn bilan yangilanadi (keyingi tahrir uchun).

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat: map TDLib updates to sync records

updateDeleteMessages is only acted on when is_permanent is true and
from_cache is false. Skipping those checks produces phantom 'deleted'
records for messages that are still in the chat, which would make the
archive untrustworthy in exactly the situation it exists for."
```

---

## Task 5: Qamrov (scope) baholash

Foydalanuvchi tanlovi: **sync bo'lgan White/Block list + serverning
o'z qo'shimcha ro'yxati.**

**Files:**
- Create: `src/CustomSync.Capture/Capture/ScopeEvaluator.cs`

- [ ] **Step 1: Ustuvorlik tartibini yozish**

Mavjud `custom_settings.h` dagi `ShouldAntiDelete` naqshiga ergashadi —
shunda xatti-harakat tanish va bashorat qilinadigan bo'ladi.

```csharp
/// <summary>
/// Qaysi chatlar kuzatiladi.
///
/// Ustuvorlik (yuqoridan pastga, birinchi mos kelgani g'olib):
///   1. Server Blocklist   → hech qachon kuzatilmaydi
///   2. Server Whitelist   → doim kuzatiladi
///   3. Sync Blocklist     → kuzatilmaydi
///   4. Sync Whitelist     → kuzatiladi
///   5. Global standart    → sozlamadagi qiymat
///
/// Server ro'yxatlari sync qilinganlaridan ustun: ular aynan shu
/// mashinaga tegishli qaror (masalan disk cheklovi tufayli faqat
/// bir nechta chatni kuzatish).
/// </summary>
public async Task<bool> ShouldCaptureAsync(long chatId)
{
    var peerHash = await _peerHasher.HashAsync(chatId);

    if (_serverBlocklist.Contains(peerHash)) return false;
    if (_serverWhitelist.Contains(peerHash)) return true;
    if (_syncedBlocklist.Contains(peerHash)) return false;
    if (_syncedWhitelist.Contains(peerHash)) return true;

    return await _settings.GetBoolAsync("capture.default_enabled");
}
```

- [ ] **Step 2: Sync qilingan ro'yxatlarni o'qish**

Sync bo'lgan `setting` turidagi yozuvlardan Whitelist/Blocklist
o'qiladi. Bu xizmat ham oddiy sync klienti bo'lgani uchun ularni pull
orqali oladi.

**`capture.default_enabled` standart `false`** — xizmat aniq ro'yxat
berilmaguncha hech narsa yozmaydi.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: add capture scope evaluation with server overrides

Server-local lists outrank the synced ones because they express a
decision about this machine specifically -- typically narrowing capture
to a handful of chats when disk is the constraint. Default is off, so
the service captures nothing until a list says otherwise."
```

---

## Task 6: Sync klienti

**Files:**
- Create: `src/CustomSync.Capture/Sync/CaptureSyncClient.cs`

- [ ] **Step 1: Klientni yozish**

Bu qism **tdesktop agenti bilan bir xil kontraktni** bajaradi:

- Enrollment kodi bilan ro'yxatdan o'tadi (`platform: "service"`)
- `CustomSync.Core.RecordId` bilan `record_id` hisoblaydi
- Payload'ni AES-256-GCM bilan shifrlaydi
- `/api/v1/sync/push` ga to'plamlar bilan yuboradi
- `/api/v1/sync/pull` orqali sozlamalar va ro'yxatlarni oladi

**Master kalit qayerdan keladi:** xizmat headless, parol so'ray olmaydi.
Kalit bir marta `--set-key` rejimida kiritiladi va OS darajasida
himoyalangan faylda saqlanadi (`chmod 600`, faqat xizmat foydalanuvchisi
o'qiy oladi).

```bash
dotnet run --project src/CustomSync.Capture -- --set-key
```

⚠️ **Bu ongli murosа:** kalit diskda turadi, chunki 24/7 avtomatik
ishlaydigan xizmat uchun boshqa yo'l yo'q. VPS'ga root kirish huquqiga
ega bo'lgan kishi uni o'qiy oladi. Buni qabul qilmasangiz — bu xizmatni
ishlatib bo'lmaydi va Muammo A hal qilinmay qoladi. Muqobil: kalit
faqat xotirada, lekin har restartdan keyin qo'lda kiritish kerak.

- [ ] **Step 2: Commit**

```bash
git add -A
git commit -m "feat: add capture sync client using the shared contracts

The service registers as an ordinary device and pushes through the same
endpoint as every other client, so the backend needs no awareness of it.

The master key is stored in a 0600 file: a headless 24/7 service cannot
prompt for a passphrase. This is a deliberate trade -- anyone with root on
the VPS can read it -- and the alternative is re-entering the key by hand
after every restart."
```

---

## Task 7: Onlayn holat va maxfiylik

**Files:**
- Modify: `src/CustomSync.Capture/Tdlib/TdClient.cs`

- [ ] **Step 1: Doimiy "onlayn" ko'rinishni oldini olish**

24/7 ulangan sessiya sizni boshqalarga doimo onlayn ko'rsatishi mumkin —
bu CustomMod'ning Ghost Mode falsafasiga to'g'ridan-to'g'ri zid.

Ulangandan **darhol keyin**:

```csharp
// Ulanish saqlanadi, lekin Telegram bizni "onlayn" deb ko'rsatmaydi.
// Bu qadam tashlab ketilsa, xizmat yoqilganidan keyin siz doimo
// onlayn ko'rinasiz — Ghost Mode ni ishlatadigan foydalanuvchi uchun
// bu jiddiy regressiya.
await _client.SendAsync(new
{
    @type = "setOption",
    name = "online",
    value = new { @type = "optionValueBoolean", value = false }
});
```

- [ ] **Step 2: Xabarlarni o'qilgan deb belgilamaslik**

TDLib avtomatik `viewMessages` yubormaydi, lekin buni tasodifan
chaqirmaslik uchun kod ko'rib chiqiladi. Xizmat **hech qachon**
`viewMessages`, `openChat` yoki `readAllChatMentions` chaqirmasligi kerak.

- [ ] **Step 3: Qo'lda tekshirish**

Xizmatni ishga tushiring va boshqa akkauntdan o'z profilingizga qarang:

| Tekshiruv | Kutilgan |
|---|---|
| Onlayn ko'rinasizmi | **Yo'q** |
| Kelgan xabarlar o'qilgan bo'ladimi | **Yo'q** |
| "Yozmoqda…" ko'rinadimi | **Yo'q** |

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat: keep the capture session invisible

Sets the TDLib online option to false immediately after connecting. A
24/7 session would otherwise report the account as permanently online,
which directly contradicts the Ghost Mode behaviour the rest of CustomMod
is built around."
```

---

## Task 8: Media siyosati

**Files:**
- Create: media yuklash mantiqini `UpdateHandler.cs` ga

- [ ] **Step 1: Standart holat — media yuklanmaydi**

Sozlamalar:

```
capture.download_media          bool   false   Media yuklab olinsinmi
capture.media_peer_list         string ""      Faqat shu peerlar uchun
capture.media_max_bytes         int    10485760  Bitta fayl chegarasi
```

**Nima uchun standart o'chiq:** hamma chatdagi hamma media'ni yuklab
olish diskni juda tez to'ldiradi. O'chirilgan **matn** deyarli har doim
eng qimmatli qism; media esa hajmning 95% ini egallaydi.

- [ ] **Step 2: Tanlangan peerlar uchun yuklash**

`capture.media_peer_list` dagi peerlar uchun `updateNewMessage` kelganda
`downloadFile` chaqiriladi, fayl `media_max_bytes` dan kichik bo'lsa.
Yuklangan fayl shifrlanadi va backend'ga media blob sifatida yuboriladi.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: add opt-in media capture

Media downloading is off by default: it accounts for the overwhelming
majority of storage while deleted text is almost always the part worth
keeping. Enabling it per-peer keeps the disk cost proportional to how much
a chat actually matters."
```

---

## Task 9: Xotira va disk boshqaruvi

Foydalanuvchining aniq talabi: "xotirani aqlli boshqarish va doimiy
nazorat qilish".

**Files:**
- Create: `src/CustomSync.Capture/Maintenance/StorageMaintenance.cs`
- Create: `deploy/customsync-capture.service`

- [ ] **Step 1: Davriy tozalash**

`BackgroundService` sifatida, sozlangan oraliqda:

1. `MessageCache.Prune(capture.cache_days)` — eski kesh yozuvlari
2. TDLib `optimizeStorage` — TDLib'ning o'z fayl keshi
3. Xotira sarfini o'lchash va backend'ga metrik sifatida yuborish

```csharp
// TDLib'ning o'z fayl keshi cheksiz o'sadi. optimizeStorage uni
// belgilangan chegaraga tushiradi.
await _client.SendAsync(new
{
    @type = "optimizeStorage",
    size = maxBytes,
    ttl = ttlSeconds,
    count = 0,
    immunity_delay = 0,
    return_deleted_file_statistics = true
});
```

- [ ] **Step 2: systemd cheklovlari**

`deploy/customsync-capture.service`:

```ini
[Unit]
Description=CustomSync capture service
After=network.target customsync.service

[Service]
Type=simple
User=customsync
WorkingDirectory=/var/www/customsync-capture
ExecStart=/usr/bin/dotnet /var/www/customsync-capture/CustomSync.Capture.dll
Restart=always
RestartSec=15
Environment=ASPNETCORE_ENVIRONMENT=Production
Environment=LD_LIBRARY_PATH=/var/www/customsync-capture

# TDLib xotirani ko'p ishlatishi mumkin. Qattiq chegara qo'yamiz —
# oshsa systemd jarayonni o'ldiradi va qayta ishga tushiradi.
# Bu VPS ni butunlay muzlatib qo'yishdan ancha yaxshi.
MemoryMax=1200M
MemoryHigh=900M
CPUQuota=100%

NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/customsync-capture

[Install]
WantedBy=multi-user.target
```

- [ ] **Step 3: Xotira metrikasini backend'ga yuborish**

Xizmat o'z RSS'ini davriy o'lchab, `setting` turidagi yozuv sifatida
emas, alohida health endpoint orqali backend'ga bildiradi. Web app'da
"Capture service: 640 MB / 1200 MB" ko'rinadi.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat: add storage maintenance and systemd resource limits

MemoryMax is a hard ceiling rather than a target: TDLib can grow
unpredictably with large chats, and a killed-and-restarted service is far
better than a VPS that becomes unresponsive and takes the API down with
it. optimizeStorage bounds TDLib's own file cache, which otherwise grows
without limit."
```

---

## Task 10: Uchi-uchiga tekshirish

- [ ] **Step 1: Asosiy ssenariy — Muammo A hal bo'lganini isbotlash**

Bu butun loyihaning maqsadi. Qadamlar:

1. tdesktop'ni **butunlay yoping** (tray'dan ham chiqing)
2. Boshqa akkauntdan sizga xabar yuboring
3. 30 soniya kuting
4. O'sha xabarni **o'chiring**
5. tdesktop'ni oching

**Kutilgan natija:** o'chirilgan xabar matni bilan arxivda ko'rinadi.

Sync'dan oldin bu holatda xabar butunlay yo'qolardi.

- [ ] **Step 2: Qamrov tekshiruvi**

| Holat | Kutilgan |
|---|---|
| Ro'yxatga kirmagan chat | Yozuv yaratilmaydi |
| Server blocklist'idagi chat | Yozuv yaratilmaydi (sync whitelist'da bo'lsa ham) |
| Server whitelist'idagi chat | Yozuv yaratiladi |

- [ ] **Step 3: Dedup tekshiruvi**

tdesktop **va** capture service ikkalasi ham ochiq bo'lganda bir xil
xabarni o'chiring.

**Kutilgan:** serverda **bitta** yozuv (ikkita emas), va u
`observed_at` kichigi bo'lgan manbanikidir.

- [ ] **Step 4: Barqarorlik tekshiruvi**

Xizmatni 48 soat ishlatib qo'ying va tekshiring:

| Metrik | Kutilgan |
|---|---|
| Xotira sarfi | Barqaror, doimiy o'smaydi |
| Restart soni | 0 |
| Disk o'sishi | Prognozga mos |
| Onlayn ko'rinish | Yo'q |

- [ ] **Step 5: Natijani hujjatlashtirish**

```bash
git commit --allow-empty -m "test: verify capture works with the desktop client closed

The scenario the whole project exists for: with tdesktop fully shut down,
a message received and then deleted still reaches the archive with its
text intact. Also verified that a message deleted while both the desktop
client and the service are running produces exactly one record, not two."
```

---

## Qabul qilish mezonlari (05)

1. tdesktop **yopiq** bo'lganda o'chirilgan xabar matni bilan saqlanadi.
2. Xizmat sizni **onlayn ko'rsatmaydi** va xabarlarni o'qilgan deb
   belgilamaydi.
3. `is_permanent=false` yoki `from_cache=true` bo'lgan o'chirishlar
   e'tiborsiz qoldiriladi (yolg'on yozuv yo'q).
4. Qamrov ustuvorligi to'g'ri ishlaydi; standart holatda hech narsa
   yozilmaydi.
5. tdesktop va xizmat bir vaqtda ishlaganda **dublikat yozuv yo'q**.
6. Xotira `MemoryMax` ichida qoladi; 48 soatlik sinovda o'smaydi.
7. Media standart holatda yuklanmaydi.
8. Backend'da bu xizmat uchun **birorta ham maxsus holat kodi yo'q**.

---

## Seriya yakuni

Beshala plan tugagach:

- **Muammo A** hal bo'ladi — capture ilova yopiq bo'lganda ham ishlaydi
- **Muammo B** hal bo'ladi — ma'lumot barcha qurilmalarda mavjud
- Ekotizim mobil klientlarni qabul qilishga tayyor (Plan 06/07), chunki
  backend boshidanoq client-agnostic qurilgan
