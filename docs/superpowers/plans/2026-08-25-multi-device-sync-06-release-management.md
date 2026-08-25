# Release Management Implementation Plan (06)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reliz paketlarini SSH/scp o'rniga backend API orqali yuklash va tarqatish; boshqaruv panelidan qaysi qurilma qaysi versiyada ekanini ko'rish.

**Architecture:** Backend yangi `releases` moduli oladi — bo'laklab (resumable) yuklash, checksum bo'yicha idempotentlik, mirror tarqatish. Imzolash **lokalda qoladi** va hech qachon serverga ko'chmaydi. Mavjud statik nginx mirror'lar **saqlanadi** va zaxira bo'lib qoladi.

**Tech Stack:** .NET 8 (mavjud backend), PostgreSQL, Vue 3 (mavjud web app), PowerShell (mavjud `release.ps1`).

**Kirish sharti:** [01a](2026-07-29-multi-device-sync-01a-backend-foundation.md) va [01b](2026-07-29-multi-device-sync-01b-backend-sync.md) tugagan — auth, `SettingsService` va audit log ishlaydi.

**Umumiy qoidalar:** [00-index](2026-07-29-multi-device-sync-00-index.md) dagi K1–K7.

---

## Nima uchun bu plan kerak — real hodisa

2026-08-24 da bitta reliz chiqarish uchun skript **ikki marta** ishga tushirildi:

```
1-yurish: vps-pub OK, github OK, vps-secure FAILED
          Connection reset ... scp: Couldn't send packet: Broken pipe
2-yurish: vps-secure OK, vps-pub OK, github FAILED (clone uzildi)
```

Ikkalasi birga to'liq qamradi — **lekin bu tasodif**. Skript har yurishda faqat o'sha yurishni biladi, umumiy holatni bilmaydi va allaqachon to'g'ri bo'lgan mirror uchun ham "FAILED" deb yozadi.

| Hozirgi kamchilik | API bilan |
|---|---|
| 52 MB bitta scp seansida; uzilsa HAMMASI boshidan | Bo'laklab yuklash, uzilgan joydan davom |
| SSH kaliti kerak — faqat bitta odam chiqara oladi | Token bilan, ruxsat berilganlar ham |
| "broken pipe" — sabab noaniq | Aniq HTTP status + xato matni |
| Mirror holati qo'lda tekshiriladi | `GET /releases` — bir so'rovda |
| Uchta mirror = uchta alohida mantiq | Bitta API, tarqatishni server bajaradi |
| Idempotentlik yo'q | Checksum bo'yicha "allaqachon bor" javobi |

---

## Xavfsizlik chegarasi — buzilmaydigan qoida

🔴 **Imzolash LOKALDA qoladi.**

`Packer.exe` va maxfiy kalitlar (`packer_private.h`, `alpha_private.h`, `customsync-updates-private.pem`) **hech qachon serverga chiqmaydi**. Server faqat tayyor, imzolangan paketni qabul qiladi va tarqatadi.

Sababi: serverni buzgan odam soxta yangilanish tarqata olmasligi kerak. Klient imzoni o'zi tekshiradi va noto'g'ri imzoni rad etadi — server ishonchli tomon EMAS.

---

## File Structure

**Backend (yangi):**
- `Modules/Releases/ReleaseController.cs` — API endpoint'lari
- `Modules/Releases/ReleaseService.cs` — biznes mantiq, mirror tarqatish
- `Modules/Releases/UploadSessionStore.cs` — bo'lakli yuklash holati
- `Data/Entities/Release.cs`, `ReleaseMirror.cs`, `UploadSession.cs`
- `Migrations/…_AddReleases.cs`

**Web app (yangi):**
- `src/views/Releases.vue` — ro'yxat, yuklash, mirror holati
- `src/api/releases.ts`

**tdesktop tomonda (mavjud fayl o'zgaradi):**
- `tools/publish/publish.ps1` — scp o'rniga API chaqiruvi

---

## Task 1: Sxema va entity'lar

**Files:**
- Create: `Data/Entities/Release.cs`, `ReleaseMirror.cs`, `UploadSession.cs`
- Modify: `Data/AppDbContext.cs`
- Create: `Migrations/…_AddReleases.cs`

- [ ] **Step 1: Jadvallarni yozish**

```sql
CREATE TABLE releases (
  release_id   TEXT PRIMARY KEY,          -- 'win64-7001001'
  platform     TEXT NOT NULL,             -- 'win64','linux','macos'
  version      BIGINT NOT NULL,           -- AppVersion (7001001)
  channel      TEXT NOT NULL,             -- 'stable','alpha','beta'
  file_name    TEXT NOT NULL,             -- 'tx64upd7001001'
  size         BIGINT NOT NULL,
  sha256       TEXT NOT NULL,
  storage_path TEXT NOT NULL,
  uploaded_by  TEXT NOT NULL,             -- device_id
  uploaded_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  published_at TIMESTAMPTZ,               -- NULL = hali tarqatilmagan
  UNIQUE (platform, version, channel)
);

CREATE TABLE release_mirrors (
  release_id  TEXT NOT NULL REFERENCES releases(release_id) ON DELETE CASCADE,
  mirror      TEXT NOT NULL,              -- 'vps-secure','vps-pub','github'
  state       TEXT NOT NULL,              -- 'pending','ok','failed'
  last_error  TEXT,
  verified_at TIMESTAMPTZ,
  PRIMARY KEY (release_id, mirror)
);

CREATE TABLE upload_sessions (
  session_id   TEXT PRIMARY KEY,
  release_id   TEXT NOT NULL,
  total_size   BIGINT NOT NULL,
  received     BIGINT NOT NULL DEFAULT 0,
  temp_path    TEXT NOT NULL,
  created_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
  expires_at   TIMESTAMPTZ NOT NULL
);
```

- [ ] **Step 2: EF Core entity'lari va DbContext'ga qo'shish**

Mavjud entity naqshiga amal qiling (01a Task 3).

- [ ] **Step 3: Migratsiya yaratish va qo'llash**

```bash
dotnet ef migrations add AddReleases
dotnet ef database update
```

- [ ] **Step 4: Commit**

```bash
git add . && git commit -m "feat(releases): sxema va entity'lar"
```

---

## Task 2: Bo'lakli (resumable) yuklash

**Files:**
- Create: `Modules/Releases/UploadSessionStore.cs`
- Create: `Modules/Releases/ReleaseController.cs`

- [ ] **Step 1: Xatolikni takrorlaydigan test yozish**

```csharp
[Fact]
public async Task Upload_ResumesAfterInterruption()
{
    var session = await StartUpload(totalSize: 10_000_000);
    await UploadChunk(session, offset: 0, size: 4_000_000);
    // uzilish simulyatsiyasi — hech narsa qilinmaydi
    var status = await GetUploadStatus(session);
    Assert.Equal(4_000_000, status.Received);

    await UploadChunk(session, offset: 4_000_000, size: 6_000_000);
    var result = await FinishUpload(session);
    Assert.Equal("complete", result.State);
}
```

- [ ] **Step 2: Testni ishga tushirib, yiqilishini ko'rish**

Run: `dotnet test --filter Upload_ResumesAfterInterruption`
Expected: FAIL — endpoint hali yo'q

- [ ] **Step 3: Endpoint'larni yozish**

```
POST /api/v1/releases                    reliz yaratish (metadata)
POST /api/v1/releases/{id}/upload        yuklash seansini boshlash
PUT  /api/v1/releases/{id}/upload/{sid}  bo'lak yuborish (Content-Range)
GET  /api/v1/releases/{id}/upload/{sid}  qancha qabul qilingani
POST /api/v1/releases/{id}/finish        yakunlash + sha256 tekshirish
```

`PUT` `Content-Range: bytes 0-4194303/54950589` sarlavhasini o'qiydi va
`temp_path` faylining o'sha offsetiga yozadi. Standart bo'lak — 4 MB.

- [ ] **Step 4: Idempotentlik**

🔴 `POST /api/v1/releases` bir xil `(platform, version, channel)` bilan
qayta kelsa **409 emas, 200** qaytaradi va mavjud `release_id` ni beradi.
Agar `sha256` ham bir xil bo'lsa — `{"state":"already_exists"}`.

Sababi: skript qayta ishga tushirilishi **normal holat** (yuqoridagi
real hodisa). Qayta yurish xatoga olib kelmasligi kerak.

- [ ] **Step 5: Test o'tishini tekshirish**

Run: `dotnet test --filter Upload_`
Expected: PASS

- [ ] **Step 6: Commit**

---

## Task 3: Yakunlash va checksum tekshiruvi

**Files:**
- Modify: `Modules/Releases/ReleaseService.cs`

- [ ] **Step 1: Test — noto'g'ri checksum rad etilishi**

```csharp
[Fact]
public async Task Finish_RejectsWrongChecksum()
{
    var session = await UploadFull(bytes: TestData, declaredSha: "0000…");
    var ex = await Assert.ThrowsAsync<ApiException>(() => FinishUpload(session));
    Assert.Equal(422, ex.StatusCode);
}
```

- [ ] **Step 2: Testni ishga tushirish** — FAIL kutiladi

- [ ] **Step 3: Implement**

`finish` chaqirilganda:
1. Vaqtinchalik fayl sha256 hisoblanadi
2. `releases.sha256` bilan solishtiriladi
3. Mos kelmasa **422** va vaqtinchalik fayl o'chiriladi
4. Mos kelsa `storage_path` ga ko'chiriladi va `release_mirrors`
   qatorlari `pending` holatida yaratiladi

⚠️ Server **imzoni tekshirmaydi** — u kalitga ega emas va bo'lishi
ham kerak emas. Imzo klient tomonda tekshiriladi.

- [ ] **Step 4: Test o'tishini tekshirish** · **Step 5: Commit**

---

## Task 4: Mirror tarqatish va holat

**Files:**
- Modify: `Modules/Releases/ReleaseService.cs`
- Modify: `SettingsService` — mirror ro'yxati

- [ ] **Step 1: Mirror konfiguratsiyasi `server_settings` da**

K1 qoidasi: konfiguratsiya kodda bo'lmaydi.

| Kalit | Misol |
|---|---|
| `releases.mirrors` | `[{"name":"vps-pub","type":"local","path":"/var/www/updates/pub"}, …]` |
| `releases.verify_after_publish` | `true` |

- [ ] **Step 2: `publish` endpoint**

```
POST /api/v1/releases/{id}/publish
```

Har mirror uchun ketma-ket: fayl ko'chiriladi, manifest yangilanadi,
`verify_after_publish` bo'lsa HTTP GET bilan checksum tekshiriladi.
Natija `release_mirrors` ga yoziladi.

🔴 **Bitta mirror yiqilsa qolganlari to'xtamaydi.** Javob har mirror
uchun alohida holat qaytaradi — 01b Task 2 dagi push javobi naqshi
bilan bir xil:

```json
{"mirrors":[
  {"mirror":"vps-pub","state":"ok"},
  {"mirror":"vps-secure","state":"failed","error":"disk full"},
  {"mirror":"github","state":"ok"}
]}
```

- [ ] **Step 3: Qayta urinish**

```
POST /api/v1/releases/{id}/publish?only=vps-secure
```

Faqat yiqilgan mirror'ni qayta urinadi. Bu real hodisadagi asosiy
ehtiyoj edi — ikkinchi yurish boshqa mirror'ni buzgandi.

- [ ] **Step 4: `GET /api/v1/releases`**

Ro'yxat + har birining mirror holati. Bitta so'rovda umumiy manzara.

- [ ] **Step 5: Commit**

---

## Task 5: `publish.ps1` ni API ga o'tkazish

**Files:**
- Modify: `tools/publish/publish.ps1` (tdesktop repo'sida)

- [ ] **Step 1: Yangi rejim qo'shish**

```powershell
.\tools\publish\release.ps1 -Api https://sync.example.com -Token $env:CUSTOMSYNC_TOKEN
```

`-Api` berilmasa — **eski scp yo'li ishlaydi**. Bu ataylab: API
ishlamay qolsa qaytish yo'li qoladi.

- [ ] **Step 2: Bo'laklab yuklash**

4 MB bo'laklar, uzilishda `GET …/upload/{sid}` bilan qancha
qabul qilinganini so'rab, o'sha joydan davom etadi. 3 marta
qayta urinish, eksponensial backoff.

- [ ] **Step 3: Imzolash tartibi o'zgarmaydi**

Packer avvalgidek **lokalda** ishlaydi. API ga faqat tayyor paket
va uning sha256'si yuboriladi.

- [ ] **Step 4: Qo'lda sinov**

| Sinov | Kutilgan |
|---|---|
| Toza yuklash | 3 mirror OK |
| O'rtada uzish (tarmoqni o'chirish) | qolgan joydan davom etadi |
| Bir xil relizni qayta yuborish | `already_exists`, qayta yuklanmaydi |
| Bitta mirror yiqilsa | qolganlari OK, `?only=` bilan qayta urinish ishlaydi |

- [ ] **Step 5: Commit**

---

## Task 6: Web app — reliz boshqaruvi

**Files:**
- Create: `src/views/Releases.vue`, `src/api/releases.ts`

- [ ] **Step 1: Ro'yxat ko'rinishi**

Har reliz uchun: versiya, platforma, kanal, hajm, sana va **mirror
holati chiroqlari**. Yiqilgan mirror yonida "Qayta urinish" tugmasi
(`?only=` chaqiruvi).

- [ ] **Step 2: Qurilma versiyalari**

`devices` jadvalida `last_seen_at` bor. Qurilma sync qilganda
o'z versiyasini ham yuborsin (JWT claim yoki push metadata) —
shunda panelda "kim qaysi versiyada" ko'rinadi.

Bu A9 (rasmiy versiya tekshiruvi) bilan birga to'liq manzara beradi:
rasmiy versiya · bizning oxirgi reliz · har qurilmadagi versiya.

- [ ] **Step 3: Commit**

---

## Qabul qilish mezonlari (06)

1. 52 MB paket API orqali yuklanadi; o'rtada tarmoq uzilsa
   qolgan joydan davom etadi va **boshidan boshlamaydi**.
2. Bir xil reliz qayta yuborilsa qayta yuklanmaydi (`already_exists`).
3. Bitta mirror yiqilsa qolganlari muvaffaqiyatli bo'ladi va
   `?only=` bilan faqat o'shani qayta urinish mumkin.
4. `GET /api/v1/releases` barcha mirror holatini bitta so'rovda beradi.
5. Maxfiy kalitlar serverga **umuman yuborilmaydi** — imzolash
   lokalda qoladi.
6. `-Api` bayrog'isiz eski scp yo'li avvalgidek ishlaydi.

---

## Eslatmalar

- **Statik mirror'lar saqlanadi.** Backend yana bitta mirror bo'lib
  qo'shiladi, ularni almashtirmaydi. Backend o'chsa self-update
  statik nginx orqali ishlashda davom etadi.
- **Manifest formati bir xil** bo'lishi shart (self-update plani
  §3.5) — shunda ko'chish faqat URL almashtirish bo'ladi.
- GitHub mirror 50 MB dan katta fayl uchun ogohlantirish beradi
  (LFS tavsiyasi). Hozircha push o'tadi, lekin paket kattalashsa
  qayta ko'rib chiqish kerak.
