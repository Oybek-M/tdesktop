# Multi-Device Sync Backend — Design Spec

**Sana:** 2026-07-29
**Bo'lak:** 1-bo'lak (5 bo'lakli ekotizimning birinchisi)
**Holat:** User review kutilmoqda

---

## 1. Maqsad va kontekst

### Hal qilinayotgan 2 ta muammo

**Muammo A — vaqt bo'yicha uzilish.** CustomMod capture logikasi (`MarkDeleted`,
`CacheMessageText`, `RecordBackgroundEdit`, activity `peerUpdates` kuzatuvi)
faqat tdesktop MTProto sessiyasi jonli bo'lganda ishlaydi. Ilova yopiq bo'lsa
hech narsa yozilmaydi va o'sha davrdagi ma'lumot **qaytarib bo'lmas darajada**
yo'qoladi — Telegram `updateDeleteMessages` faqat msgId yuboradi, kontent
yubormaydi, shuning uchun keyinroq ulanib "tiklab olish" imkonsiz.

**Muammo B — joy bo'yicha uzilish.** Barcha ma'lumot bitta qurilmadagi
`custom_mod.db` + media papkasida qamalgan.

### Yakuniy ko'rinish (5 ta app)

| App | Stack | Holat |
|---|---|---|
| `tdesktop` | C++/Qt | Mavjud — bu bo'lakda sync agenti qo'shiladi |
| `tmobile-android` | Kotlin (exteraGram fork) | Keyingi bo'lak |
| `tmobile-ios` | Swift (Telegram-iOS fork) | Keyingi bo'lak |
| `server-backend` | .NET 8 + PostgreSQL | **Bu bo'lak** |
| `server-controller` | Vue 3 + Vite SPA | **Bu bo'lak** |

### Bu bo'lak scope'i

**Kiradi:** `server-backend`, `server-controller`, tdesktop sync agenti,
almashuv (import/export) formati, kalit boshqaruvi va tiklash.

**Kirmaydi (keyingi bo'laklar):** always-on TDLib capture service, Android
fork, iOS fork. Lekin backend boshidanoq **client-agnostic** loyihalanadi —
mobil klientlar hech qanday server o'zgarishisiz ulanadi.

### Non-goals

- Telegram protokolining o'zini o'zgartirish yoki reverse engineering
- Boshqa foydalanuvchilar ma'lumotiga kirish — faqat o'z akkaunt, o'z qurilmalar
- Store'ga (App Store / Play Store) chiqarish
- Real-time chat funksiyalari — sinxronlanadigan narsa faqat CustomMod arxivi

---

## 2. Asosiy arxitektura tamoyili

> **Local-first, server — konvergentsiya nuqtasi.**

Hech bir klient serverga bog'liq emas. Har bir klient o'zining lokal
SQLite'iga mustaqil yozadi va to'liq offline ishlaydi. Server barcha
qurilmalar kuzatuvlarining **birlashmasi** yashaydigan joy.

Amaliy natijalari:

1. Server o'chsa, internet yo'qolsa, kalit qulflangan bo'lsa — CustomMod
   **hozirgidek to'liq ishlaydi**. Sync butunlay qo'shimcha qatlam.
2. Lokal SQLite **saqlanadi va o'zgarmaydi**. PostgreSQL faqat serverda.
   ("SQLite sekin" muammosi query pattern muammosi edi va u
   `EnsureActivityCacheLoaded()` bilan hal qilingan — PostgreSQL uni
   yaxshilamas, network round-trip qo'shib battar qilardi.)
3. Yozuvlar ko'p manbadan keladi (desktop + telefon + keyinchalik server
   service) — shuning uchun dedup va konflikt qoidasi majburiy.

```
┌─ Capture klientlar (local-first, mustaqil) ──────────────┐
│  tdesktop (C++/Qt)   [+android, +ios — keyingi bo'laklar]│
│    ├─ custom_db.cpp   → lokal SQLite (O'ZGARMAYDI)       │
│    └─ custom_sync.cpp → YANGI: outbox + shifrlash + HTTP │
└──────────────────────────┬───────────────────────────────┘
              HTTPS (REST) │ ▲ WS: {"seq":N} = "pull qil"
                           ▼ │
┌─ server-backend (.NET 8) ────────────────────────────────┐
│  REST API  │  WebSocket notify  │  JWT auth (per-device) │
│  PostgreSQL: metadata + shifrlangan payload              │
│  Disk: shifrlangan media blob'lar                        │
└──────────────────────────┬───────────────────────────────┘
                           │ HTTPS
┌─ server-controller (Vue 3 SPA) ──────────────────────────┐
│  Control plane: qurilmalar, sync holati, disk, loglar    │
│  Data plane: arxiv — Web Worker'da deshifrlanadi,        │
│              IndexedDB lokal indeks, kalit serverga      │
│              hech qachon yuborilmaydi                    │
└──────────────────────────────────────────────────────────┘
```

---

## 3. Kanonik yozuv modeli

Bitta yozuv shakli **uch joyda** ishlatiladi: HTTP sync payload, `.cmx`
almashuv fayli, PostgreSQL qatori. Ikkita alohida kod yo'li yozilmaydi.

```json
{
  "record_id":   "a3f9…64 hex belgi",
  "kind":        "deleted",
  "peer_hash":   "…32 hex belgi",
  "msg_id":      123456,
  "occurred_at": 1753800000,
  "observed_at": 1753800005,
  "device_id":   "desktop-pc-01",
  "nonce":       "<base64, 12 bayt>",
  "payload":     "<base64 AES-256-GCM ciphertext+tag>",
  "media":       [{"hash": "…", "size": 10240, "nonce": "…"}]
}
```

### 3.1 `record_id` — deterministik dedup

```
record_id = hex( SHA256( kind ‖ 0x00 ‖ peer_hash ‖ 0x00 ‖
                         msg_id_decimal ‖ 0x00 ‖ occurred_at_decimal ) )
```

`‖` — konkatenatsiya, `0x00` — ajratuvchi bayt (ambiguity oldini oladi).
`msg_id` bo'lmagan kind'lar uchun (`setting`, `peer_directory`) `msg_id = 0`
va o'rniga kalit satri `occurred_at` bilan birga ishlatiladi (3.2 ga qarang).

**Xossasi:** ikki xil qurilma bir xil hodisani ko'rsa — bir xil `record_id`
hosil qiladi. Dedup hech qanday muvofiqlashtirishsiz ishlaydi. Server uni
PRIMARY KEY sifatida ishlatadi, shuning uchun push **idempotent** — qayta
yuborish xavfsiz.

### 3.2 Kind'lar va payload tarkibi

Payload — shifrlanishdan oldingi JSON obyekti.

| kind | msg_id | Payload (shifrlanadi) |
|---|---|---|
| `deleted` | xabar id | `{text, sender_id, is_out, is_media}` |
| `edited` | xabar id | `{old_text, new_text, is_out}` |
| `activity` | 0 | `{field, old_value, has_old_value, new_value}` |
| `ghost_read` | o'qilgan max id | `{}` (metadata yetarli) |
| `setting` | 0 | `{key, value}` |
| `peer_directory` | 0 | `{entries: [{peer_hash, name, username, type}]}` |

`activity` uchun `record_id` hisoblashda `msg_id` o'rniga
`SHA256(field)` ning birinchi 8 bayti ishlatiladi — chunki bitta peer
uchun bir soniyada turli maydonlar o'zgarishi mumkin.

`setting` va `peer_directory` — **LWW (last-write-wins)** semantikasiga ega:
bir xil kalit uchun eng katta `occurred_at` g'olib. Qolgan kind'lar
append-only.

### 3.3 Konflikt qoidasi

Bir xil `record_id` ikki marta kelsa: **`observed_at` kichigi g'olib.**

Sababi: eng erta kuzatgan qurilma matnni eng to'liq ushlagan bo'ladi (masalan
desktop xabarni o'chirilishidan oldin ko'rgan, telefon esa faqat keyin ulangan).
Qoida yetib kelish tartibiga bog'liq emas — deterministik, shuning uchun
barcha qurilmalar bir xil natijaga keladi.

`observed_at` teng bo'lsa — `device_id` leksikografik kichigi g'olib
(sof determinizm uchun; amalda deyarli uchramaydi).

---

## 4. Shifrlash va kalit boshqaruvi

### 4.1 Kalit ierarxiyasi

**Master kalit — tasodifiy 32 bayt** (CSPRNG), bir marta yaratiladi va
**hech qachon o'zgarmaydi**. Paroldan hosil qilinmaydi.

```
Master kalit (32 bayt, tasodifiy)
  ├─ HKDF-SHA256(master, "customsync-content-v1") → payload AES-256-GCM kaliti
  ├─ HKDF-SHA256(master, "customsync-media-v1")   → media AES-256-GCM kaliti
  └─ HKDF-SHA256(master, "customsync-peer-v1")    → peer HMAC kaliti
```

**Nima uchun tasodifiy, paroldan emas:** parol o'zgarganda kalit o'zgarmasin.
Aks holda parolni almashtirish butun arxivni qayta shifrlashni talab qilardi
va bir vaqtda bir nechta qulf ochish usuli mumkin bo'lmasdi.

### 4.2 Algoritmlar

| Maqsad | Algoritm | Parametrlar |
|---|---|---|
| Kontent shifrlash | AES-256-GCM | 12 baytli tasodifiy nonce, 16 baytli tag |
| Kalit hosil qilish | HKDF-SHA256 | Yuqoridagi info satrlari |
| Paroldan KEK | PBKDF2-HMAC-SHA256 | 600 000 iteratsiya, 16 baytli salt |
| Peer yashirish | HMAC-SHA256 → 16 bayt → hex | Deterministik |

**PBKDF2, Argon2 emas** — chunki Web Crypto API Argon2'ni qo'llab-quvvatlamaydi
va brauzerda WASM kutubxona kerak bo'lardi. PBKDF2-HMAC-SHA256 beshala
platformada ham nativ mavjud: OpenSSL (tdesktop allaqachon linklaydi),
.NET `Rfc2898DeriveBytes`, Web Crypto `deriveBits`, Android/iOS standart
kutubxonalari. 600 000 iteratsiya — OWASP 2023 tavsiyasi.

### 4.3 Peer yashirish

```
peer_hash = hex( HMAC-SHA256(peer_key, peer_id)[0..16] )
```

Deterministik bo'lgani uchun server `peer_hash` bo'yicha **GROUP BY, COUNT,
SUM, ORDER BY, WHERE** qila oladi — ya'ni "kim bilan eng ko'p yozilgan",
"kimning xabari eng ko'p joy olgan", chat bo'yicha filtr — hammasi server
tomonda ishlaydi. Server faqat **ismni** bilmaydi.

Ismlarni web app `peer_directory` yozuvidan oladi (shifrlangan blob) va
UI'da mos qo'yadi.

### 4.4 Qulf ochish usullari (key wrapping)

Har bir usul master kalitning mustaqil **o'ralgan nusxasini** saqlaydi.
Istalgan bittasi kalitni ochadi.

| O'ram | Qayerda saqlanadi | KEK manbasi |
|---|---|---|
| Custom parol | Server (`key_wraps`) | PBKDF2(parol, salt) |
| Har bir qurilma | **Lokal** OS keystore | OS himoyasi (biometrika/PIN) |
| Tiklash kodi | Server (`key_wraps`) | PBKDF2(kod, salt) |
| Email escrow | Server (`key_wraps`) | PBKDF2(email_qismi ‖ PIN, salt) |

**Muhim:** qurilma o'rami serverda saqlanmaydi — master kalit to'g'ridan-to'g'ri
OS keystore ichida yotadi va uni OS o'zi himoya qiladi. Serverdagi o'ramlar
faqat **ko'chma** (portable) tiklash yo'llari uchun.

### 4.5 Platformalar bo'yicha OS keystore

| App | Saqlash | Biometrika |
|---|---|---|
| tdesktop (Windows) | DPAPI | Windows Hello |
| tdesktop (macOS) | Keychain | Touch ID |
| tdesktop (Linux) | Secret Service (libsecret) | — |
| tmobile-android | Android Keystore | BiometricPrompt |
| tmobile-ios | Keychain + Secure Enclave | Face ID / Touch ID |
| web app | — | WebAuthn (mavjud bo'lsa), aks holda parol |

### 4.6 Sozlamalar (har qurilmada mustaqil)

- Custom parol bilan ochish — yoq/o'chir
- OS parol / biometrika bilan ochish — yoq/o'chir
- **Qulflab turmaslik** (standart): kalit OS keystore'da, ilova ochilganda
  hech narsa so'ralmaydi, sync jimgina ishlaydi
- **Har ishga tushganda so'rash**: kalit faqat xotirada
- N daqiqa harakatsizlikdan keyin avtomatik qulflash

### 4.7 Qulf va sync munosabati

Capture kodi lokal SQLite'ga **ochiq matnda** yozadi (bugungidek). Shifrlash
faqat serverga yuborishdan oldin bo'ladi.

Demak kalit qulflangan bo'lsa: yozuvlar `sync_outbox`da to'planadi, hech
narsa yo'qolmaydi, CustomMod to'liq ishlaydi. Qulf ochilganda navbat
shifrlanib jo'natiladi. **Qulf — pauza, yo'qotish emas.**

### 4.8 Tiklash

1. **Bitta ishlayotgan qurilma qolgan bo'lsa** — u master kalitga ega,
   undan yangi tiklash kodi yoki yangi parol o'rami chiqariladi.
2. **Parol esda bo'lsa** — server `key_wraps`dan parol o'ramini beradi.
3. **Tiklash kodi bo'lsa** — xuddi shunday.
4. **Faqat email bo'lsa** — email qismi + eslab qolingan PIN birgalikda
   KEK hosil qiladi. Email'ning o'zi yetarli emas, PIN'ning o'zi ham yetarli emas.

**Ochiq aytilgan cheklov:** umumiy xavfsizlik darajasi **eng zaif yoqilgan
o'ramga** teng. Email escrow yoqiq bo'lsa, arxiv himoyasi "email + PIN"
darajasida bo'ladi. Har bir o'ram sozlamalarda alohida o'chiriladi va
o'chirish bir zumda (qayta shifrlash yo'q).

**Barcha usullar yo'qolsa** — arxiv tiklanmaydi. Bu E2E shifrlashning
matematik oqibati, bug emas.

---

## 5. Sync protokoli

### 5.1 Endpoint'lar

```
POST   /api/v1/devices/enroll          bir martalik kod → device_id + tokenlar
POST   /api/v1/devices/refresh         refresh token → yangi JWT
GET    /api/v1/devices                 ro'yxat (control plane)
DELETE /api/v1/devices/{id}            revoke

POST   /api/v1/sync/push               yozuvlar to'plami
GET    /api/v1/sync/pull?since=&limit= cursor bo'yicha o'zgarishlar

HEAD   /api/v1/media/{hash}            bormi? (trafik tejash)
PUT    /api/v1/media/{hash}            shifrlangan blob yuklash
GET    /api/v1/media/{hash}            yuklab olish

GET    /api/v1/keys/wraps              o'ramlar ro'yxati
POST   /api/v1/keys/wraps              yangi o'ram qo'shish
DELETE /api/v1/keys/wraps/{id}         o'ramni o'chirish

GET    /api/v1/records                 keyset paginated (web app)
GET    /api/v1/stats/peers             GROUP BY peer_hash agregatlari
GET    /api/v1/stats/storage           disk taqsimoti

POST   /api/v1/import                  .cmx yuklash
GET    /api/v1/export?since=&until=&peer=   .cmx yuklab olish

WS     /ws/notify                      {"type":"changes","seq":N}
```

### 5.2 `seq` — monoton cursor

`records.seq` — cursor manbasi. **BIGSERIAL yetarli emas**: parallel
tranzaksiyalar seq'ni olib, boshqa tartibda commit qilishi mumkin, natijada
`since=N` bilan so'ragan klient oradagi qatorni **butunlay o'tkazib
yuborishi** mumkin.

Yechim: seq hisoblagichi bitta qator ustidagi lock bilan beriladi —

```sql
UPDATE sync_counter SET value = value + 1 WHERE id = 1 RETURNING value;
```

Qator locki commit'gacha ushlanadi, shuning uchun **seq tartibi = commit
tartibi**. 5 ta qurilmali shaxsiy tizim uchun kontensiya ahamiyatsiz.

### 5.3 Push

Klient `sync_outbox`dan to'plam oladi (max 500 yozuv yoki 5 MB), shifrlaydi,
POST qiladi. Javob **har bir yozuv uchun alohida holat** qaytaradi:

```json
{"results": [
  {"record_id": "a3f9…", "status": "created", "seq": 10501},
  {"record_id": "b7c2…", "status": "duplicate"},
  {"record_id": "c1d8…", "status": "superseded"},
  {"record_id": "d4e6…", "status": "error", "message": "media hash missing"}
]}
```

`created`/`duplicate`/`superseded` — outbox'dan o'chiriladi.
`error` — outbox'da qoladi, backoff bilan qayta uriniladi.

Media avval yuklanadi: `HEAD /api/v1/media/{hash}` → 404 bo'lsa `PUT`.
Server'da allaqachon bo'lsa (boshqa qurilma yuklagan) — o'tkazib yuboriladi.

### 5.4 Pull

```
GET /api/v1/sync/pull?since=10500&limit=500
```

`seq > since` bo'lgan yozuvlar `seq ASC` tartibida. Javobda `next_since` va
`has_more` bo'ladi. Klient har to'plamni merge qilib, `sync_state.cursor`ni
yangilaydi — **to'plam muvaffaqiyatli merge bo'lgandan keyin**, shuning uchun
uzilish bo'lsa o'sha to'plam qayta olinadi (at-least-once, dedup tufayli
xavfsiz).

Klient o'zi yuborgan yozuvlarni ham qaytarib oladi (`device_id` o'ziniki) —
ular merge'da no-op bo'ladi. Bu maxsus filtrlashdan ko'ra soddaroq va
xatoga kamroq moyil.

### 5.5 WebSocket

`/ws/notify` — JWT bilan autentifikatsiya. Server yangi yozuv qabul qilganda
**boshqa** ulangan qurilmalarga yuboradi:

```json
{"type": "changes", "seq": 10501}
```

Ma'lumot WS orqali **yurmaydi** — bu faqat "hoziroq pull qil" signali.
Uzilsa eksponensial backoff bilan qayta ulanadi (1s, 2s, 4s… max 60s).
WS umuman ishlamasa tizim to'g'ri ishlashda davom etadi — periodik pull
(standart 30s) asosiy yo'l bo'lib qoladi.

### 5.6 Keyset pagination (web app uchun)

Offset pagination **ishlatilmaydi** — sync paytida yangi qator kelsa
dublikat/tushib qolish beradi.

```
GET /api/v1/records?sort=occurred_at&dir=desc
                   &after=1753800000,10499
                   &limit=50
                   &snapshot=10500
```

- `after=<sort_key>,<seq>` — oxirgi ko'rilgan qator (tiebreaker `seq` bilan)
- `snapshot=<seq>` — so'rov boshlanganda olingan `max(seq)`; barcha sahifalar
  `WHERE seq <= snapshot` bilan keladi, shuning uchun qatorlar oyoq ostidan
  surilmaydi
- Yangi ma'lumot kelsa UI tepada "N ta yangi yozuv — yangilash" banneri
  ko'rsatadi (Gmail modeli)

SQL:
```sql
WHERE seq <= :snapshot
  AND (occurred_at, seq) < (:after_key, :after_seq)   -- desc
ORDER BY occurred_at DESC, seq DESC
LIMIT :limit
```

`dir=asc` uchun `<` → `>` va `DESC` → `ASC`.

### 5.7 Autentifikatsiya

- **Enrollment:** web app control plane'da bir martalik kod generatsiya
  qilinadi (10 daqiqa amal qiladi). Qurilma uni `/devices/enroll` ga yuborib
  `device_id` + refresh token + JWT oladi.
- **JWT:** 1 soat amal qiladi, refresh token bilan yangilanadi.
- **Revoke:** `devices.revoked_at` o'rnatiladi → refresh rad etiladi, mavjud
  JWT max 1 soatda o'ladi, WS ulanish darhol uziladi.
- Bularning **hech biriga master kalit kerak emas** — auth va shifrlash
  butunlay mustaqil qatlamlar.

---

## 6. PostgreSQL sxemasi

```sql
CREATE TABLE devices (
  device_id     TEXT PRIMARY KEY,
  name          TEXT NOT NULL,
  platform      TEXT NOT NULL,          -- 'desktop-win','android','ios','service'
  enrolled_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
  last_seen_at  TIMESTAMPTZ,
  last_cursor   BIGINT NOT NULL DEFAULT 0,
  revoked_at    TIMESTAMPTZ,
  refresh_hash  TEXT NOT NULL
);

CREATE TABLE sync_counter (
  id     INT PRIMARY KEY CHECK (id = 1),
  value  BIGINT NOT NULL DEFAULT 0
);

CREATE TABLE records (
  seq          BIGINT PRIMARY KEY,       -- sync_counter dan
  record_id    TEXT UNIQUE NOT NULL,
  kind         TEXT NOT NULL,
  peer_hash    TEXT NOT NULL,
  msg_id       BIGINT NOT NULL DEFAULT 0,
  occurred_at  BIGINT NOT NULL,
  observed_at  BIGINT NOT NULL,
  device_id    TEXT NOT NULL REFERENCES devices(device_id),
  nonce        BYTEA NOT NULL,
  payload      BYTEA NOT NULL,
  payload_size INT NOT NULL,
  received_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_records_peer   ON records (peer_hash, occurred_at DESC, seq DESC);
CREATE INDEX idx_records_kind   ON records (kind, occurred_at DESC, seq DESC);
CREATE INDEX idx_records_occur  ON records (occurred_at DESC, seq DESC);

CREATE TABLE media_blobs (
  hash         TEXT PRIMARY KEY,         -- SHA256(ochiq matn)
  size         BIGINT NOT NULL,
  nonce        BYTEA NOT NULL,
  storage_path TEXT NOT NULL,
  uploaded_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE record_media (
  record_id TEXT NOT NULL REFERENCES records(record_id) ON DELETE CASCADE,
  hash      TEXT NOT NULL REFERENCES media_blobs(hash),
  PRIMARY KEY (record_id, hash)
);

CREATE TABLE key_wraps (
  wrap_id      TEXT PRIMARY KEY,
  wrap_type    TEXT NOT NULL,            -- 'passphrase','recovery','email'
  label        TEXT NOT NULL,
  salt         BYTEA NOT NULL,
  nonce        BYTEA NOT NULL,
  wrapped_key  BYTEA NOT NULL,
  iterations   INT NOT NULL,
  created_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
  last_used_at TIMESTAMPTZ
);

CREATE TABLE audit_log (
  id         BIGSERIAL PRIMARY KEY,
  at         TIMESTAMPTZ NOT NULL DEFAULT now(),
  device_id  TEXT,
  action     TEXT NOT NULL,
  detail     JSONB
);
```

**`peer_hash` ochiq emas** — u HMAC natijasi, haqiqiy peer_id emas. Server
uni faqat opaque guruhlash kaliti sifatida ko'radi.

**Media diskda:** `/var/lib/customsync/media/<hash[0:2]>/<hash>` — sharding
bitta katalogda juda ko'p fayl to'planmasligi uchun.

---

## 7. Almashuv formati (`.cmx`)

Oddiy ZIP arxiv. Maqsad: qurilma uzoq uzilib qolganda yoki server bilan
muammo bo'lganda qo'lda ma'lumot ko'chirish.

```
manifest.json     format_version, source_app, device_id, created_at,
                  scope {since, until, peer_hash|null}, record_count,
                  encrypted: true|false, key_fingerprint
records.jsonl     har satrda bitta yozuv — 3-bo'limdagi shakl bilan AYNAN bir xil
media/<hash>      shifrlangan media blob'lar
```

**Muhim dizayn qarori:** `records.jsonl` satrlari sync payload bilan **bir xil**.
Shuning uchun:

- Eksport = "men push qilgan bo'lardigan yozuvlarni faylga yozish"
- Import = "bu yozuvlarni pull merge yo'lidan o'tkazish"
- Server `/api/v1/import` = `/api/v1/sync/push` bilan **bir xil** dedup/merge kodi

Ikkita alohida kod yo'li yozilmaydi va import yo'li avtomatik ravishda sync
testlari bilan qamrab olinadi.

**`key_fingerprint`** = `SHA256(master_key)[0:8]` — import qilayotgan qurilma
kalit mos kelishini oldindan tekshiradi va noto'g'ri kalit bo'lsa aniq xato
beradi (yuzlab deshifrlash xatosi o'rniga).

**Shifrsiz eksport** — alohida, aniq belgilangan opsiya (`encrypted: false`).
UI'da ogohlantirish bilan: fayl himoyalanmagan bo'ladi. Faqat shaxsiy
offline arxivlash uchun.

**Mavjud `ExportFullBackup`/`ImportFullBackup` saqlanadi** — u boshqa maqsad
(qurilmani to'liq tiklash, lokal-only ma'lumot bilan birga). UI'da ikkalasi
aniq ajratiladi: *"To'liq zaxira"* va *"Almashuv eksporti"*.

---

## 8. tdesktop sync agenti

### 8.1 Yangi modul

`Telegram/SourceFiles/custom_sync.h` / `.cpp` — mavjud `custom_db` /
`custom_settings` pattern'iga mos. `Telegram/CMakeLists.txt` ga qo'shiladi.

### 8.2 Sxema v6 migratsiyasi

`CustomDB::kCurrentSchemaVersion` 5 → 6. Mavjud `RunMigrations()` mexanizmi
orqali **2 ta yangi jadval** qo'shiladi. Mavjud jadvallarga ustun
qo'shilmaydi, mavjud funksiyalar imzosi o'zgarmaydi.

```sql
CREATE TABLE IF NOT EXISTS sync_outbox (
  record_id    TEXT PRIMARY KEY,
  kind         TEXT NOT NULL,
  peer_id      TEXT NOT NULL,        -- lokal: ochiq (HMAC push paytida)
  msg_id       INTEGER NOT NULL DEFAULT 0,
  occurred_at  INTEGER NOT NULL,
  observed_at  INTEGER NOT NULL,
  attempts     INTEGER NOT NULL DEFAULT 0,
  last_error   TEXT,
  next_retry_at INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_outbox_retry ON sync_outbox(next_retry_at);

CREATE TABLE IF NOT EXISTS sync_state (
  key   TEXT PRIMARY KEY,            -- 'cursor','device_id','last_success_at'
  value TEXT NOT NULL
);
```

Outbox faqat **identifikatsiya** maydonlarini saqlaydi — payload push paytida
mavjud jadvallardan o'qib olinadi. Bu ma'lumot dublikatini oldini oladi.

### 8.3 Outbox qanday to'ladi

**Aniq chaqiruv** (SQLite TRIGGER emas — debug qilinadigan va kodbaza
uslubiga mos bo'lishi uchun). Jami 4 ta nuqta:

| Fayl | Funksiya | Qo'shiladigan |
|---|---|---|
| `custom_db.cpp` | `MarkDeleted()` | `CustomSync::Enqueue(kDeleted, …)` |
| `custom_db.cpp` | `SaveActionedMessage()` (type=="edited") | `CustomSync::Enqueue(kEdited, …)` |
| `custom_db.cpp` | `SaveActivityHistoryEntry()` | `CustomSync::Enqueue(kActivity, …)` |
| `custom_db.cpp` | `SaveGhostRead()` | `CustomSync::Enqueue(kGhostRead, …)` |

Har biri bitta satr. Capture logikasining o'zi umuman o'zgarmaydi.

`CustomSync::Enqueue()` sync o'chiq bo'lsa darhol qaytadi (no-op).

### 8.4 Ish oqimi

Agent `crl::async` fon oqimida ishlaydi (`ExportFullBackupAsync` allaqachon
shu naqshni ishlatadi). UI oqimi hech qachon bloklanmaydi.

- **Timer:** har 30 soniyada push+pull sikli
- **WS signali:** `{seq}` kelsa darhol pull
- **Push:** outbox → shifrlash (OpenSSL AES-256-GCM) → media HEAD/PUT →
  `POST /sync/push` → natijaga qarab outbox tozalanadi
- **Pull:** `GET /sync/pull?since=cursor` → deshifrlash → merge → cursor
- **Backoff:** 1s, 2s, 4s, 8s… max 5 daqiqa. `next_retry_at` diskda
  saqlanadi, shuning uchun ilova qayta ishga tushsa backoff yo'qolmaydi

### 8.5 Bog'liqliklar

Yangi tashqi kutubxona **kerak emas**:

| Ehtiyoj | Mavjud |
|---|---|
| HTTP | `QNetworkAccessManager` (Qt Network) |
| WebSocket | `QWebSocket` (Qt WebSockets) |
| AES-256-GCM, PBKDF2, HKDF, HMAC, SHA-256 | OpenSSL (MTProto uchun allaqachon linklangan) |
| JSON | `QJsonDocument` |
| OS keystore (Windows) | DPAPI (`Crypt32.lib`) |

### 8.6 UI

`custom_mod_window.cpp` ga yangi **"☁️ Sinxronizatsiya"** bo'limi:

- Server URL, enrollment kodi kiritish, ulanish holati
- Sync yoqiq/o'chiq toggle
- Oxirgi muvaffaqiyatli sync vaqti, outbox'dagi yozuvlar soni
- Qulf holati va qulf ochish usullari sozlamalari (4.6)
- "Hozir sinxronlash" tugmasi
- Almashuv eksporti / importi (scope tanlash bilan)

---

## 9. Web app (`server-controller`)

### 9.1 Ikki rejim

**Control plane** — master kalitsiz to'liq ishlaydi:
qurilmalar ro'yxati va revoke, sync holati va cursor'lar, disk taqsimoti,
retention sozlamalari, audit log, server salomatligi, `.cmx` yuklash/olish.

**Data plane** — kalit ochilgandan keyin:
arxiv brauzeri, qidiruv, filtr, saralash, statistika va grafiklar.

### 9.2 Freez bo'lmasligi kontrakti

| Muammo | Yechim |
|---|---|
| Deshifrlash asosiy oqimni bloklaydi | **Web Worker** — barcha crypto va JSON parsing worker'da |
| Minglab qator render qilinadi | **Virtual scrolling** — faqat ko'rinadigan qatorlar |
| Qidiruvda har harfda so'rov | **Debounce 250ms + AbortController** — eski so'rovlar bekor qilinadi |
| Pagination sync bilan konflikt | **Keyset + snapshot** (5.6) |
| Matn qidiruvi sekin | **IndexedDB lokal indeks** — birinchi yuklashdan keyin darhol |

### 9.3 Bitta so'rov — bitta manba

Aralashtirish **taqiqlanadi**:

- Metadata so'rovi (peer/sana/tur filtri, sana/hajm saralash, statistika)
  → **butunlay serverda**, keyset pagination bilan
- Matn ichidan qidiruv → **butunlay brauzerda**, IndexedDB ustida, o'sha
  keyset pattern bilan

Ikkalasi bir xil pagination shartnomasini bajaradi, shuning uchun UI
komponenti rejimni bilishi shart emas.

### 9.4 Ma'lum UX cheklovi

Peer **ismi** bo'yicha alifbo saralash faqat "peer bo'yicha jamlanma"
ko'rinishida mavjud (ro'yxat kichik — bir necha yuz element, brauzerda
bir zumda). Xom yozuvlar ro'yxatida bu yo'q, chunki server ismlarni bilmaydi.

---

## 10. Xatolarni boshqarish

| Holat | Xatti-harakat |
|---|---|
| Server yetib bo'lmaydi | Outbox to'planadi, backoff bilan qayta urinish. CustomMod normal ishlaydi. UI'da "offline" indikatori |
| Kalit qulflangan | Sync pauza qiladi, capture davom etadi. Ogohlantirish ko'rsatiladi |
| Qisman push xatosi | Har yozuv alohida holat oladi (5.3); faqat xatolilar outbox'da qoladi |
| Deshifrlab bo'lmaydigan yozuv | Log + `corrupt` deb belgilanadi, ko'rsatilmaydi. Crash **yo'q**. Web app'da alohida ro'yxatda ko'rinadi |
| Media serverda yo'q | Yozuv baribir ko'rsatiladi, media "mavjud emas" deb belgilanadi |
| Soat farqi (clock skew) | Server `received_at` ni ham yozadi. Farq > 5 daqiqa bo'lsa audit log'ga ogohlantirish |
| Import: kalit mos emas | `key_fingerprint` oldindan tekshiriladi → aniq xato xabari |
| Import: format versiyasi yangi | Rad etiladi, aniq xabar bilan |
| Disk to'lgan | Push 507 qaytaradi, klient outbox'da ushlab turadi, web app'da ogohlantirish |

---

## 11. Testing strategiyasi

### 11.1 Umumiy crypto test vektorlari (eng muhim)

`docs/sync-protocol/test-vectors.json` — qat'iy belgilangan
(kalit, nonce, ochiq matn, shifrlangan matn) to'plami va
(parol, salt, iteratsiya, KEK) to'plami.

**Beshala app ham shu vektorlarni aynan qayta hosil qila olishi shart.**
5 platformali loyihada interop buzilishining eng katta manbai shu — bir
platformada base64 padding yoki nonce tartibi boshqacha bo'lsa, hamma
narsa jimgina buziladi. Vektorlar buni birinchi kunda ushlaydi.

Shu bilan birga `record_id` hisoblash vektorlari ham (kirish maydonlari →
kutilgan hex).

### 11.2 Backend (.NET, xUnit)

- Dedup: bir xil `record_id` ni 2 marta push → 1 ta qator, `duplicate` javobi
- Konflikt: kichik `observed_at` g'olib bo'lishi, kelish tartibidan qat'i nazar
- **`seq` monotonligi:** 50 parallel push → `pull` hech qanday qatorni
  o'tkazib yubormasligi (5.2 dagi bug uchun regressiya testi)
- Keyset pagination: sahifalash davomida yangi yozuv insert qilinsa —
  dublikat ham, tushib qolgan qator ham bo'lmasligi
- Auth: revoke qilingan qurilma refresh qila olmasligi, WS uzilishi
- Import: `.cmx` → push bilan bir xil natija berishi

### 11.3 tdesktop (qo'lda + maqsadli)

- Outbox ilova qayta ishga tushganda saqlanishi
- Offline → online o'tishda navbat to'liq jo'natilishi
- Merge: serverdan kelgan yozuv lokal SQLite'ga to'g'ri tushishi va
  mavjud UI (Deleted Archive, Activity History) uni ko'rsatishi
- Sync o'chiq bo'lganda **hech qanday regressiya yo'qligi** — bu eng
  muhim tekshiruv, chunki asosiy funksionallik buzilmasligi kerak

### 11.4 Web app (Vitest + Playwright)

- Worker deshifrlash to'g'riligi (test vektorlari bilan)
- Virtual ro'yxat 10 000 qatorda ham 60fps
- Qidiruv debounce va bekor qilish race condition'siz
- Pagination jonli insert ostida barqaror

---

## 12. Deployment

**Server:** Contabo VPS (Ubuntu), mavjud iBOS CRM naqshi bilan bir xil.

```
/var/www/customsync/          .NET 8 publish natijasi
/var/lib/customsync/media/    shifrlangan media blob'lar
/etc/systemd/system/customsync.service
/etc/nginx/sites-available/customsync
```

- **systemd** unit: `Restart=always`, `User=customsync` (root emas)
- **Nginx** reverse proxy: TLS (Let's Encrypt), WebSocket upgrade uchun
  `proxy_set_header Upgrade/Connection`, request body limiti 50 MB (media)
- **PostgreSQL** faqat `localhost` da tinglaydi, tashqariga ochilmaydi
- **Firewall (ufw):** faqat 22, 80, 443
- **Zaxira:** har kecha `pg_dump` + media `rsync` — **boshqa joyga**
  (VPS'ning o'zida emas, aks holda VPS yo'qolsa zaxira ham yo'qoladi)
- **Migratsiyalar:** EF Core migrations, deploy skriptida avtomatik
- **Loglar:** `journalctl`, audit log alohida PostgreSQL jadvalida

**Xavfsizlik qattiqlashtirish:** parol bilan SSH o'chirilgan (faqat kalit),
fail2ban, avtomatik xavfsizlik yangilanishlari, `/api/v1/devices/enroll`
uchun rate limiting.

---

## 13. Keyingi bo'laklar uchun ochiq qoldirilgan

Bu spec **ataylab** qamramaydi (har biri o'z spec'ini oladi):

1. **Always-on capture service** (TDLib, VPS) — Muammo A ni to'liq hal
   qiladi. Diqqat qilinadigan nuqta: 24/7 ulangan sessiya sizni doimo
   "online" ko'rsatmasligi uchun TDLib `online` opsiyasi `false` bo'lishi kerak.
2. **`tmobile-android`** (exteraGram fork) — Samsung batareya boshqaruvi
   fon xizmatini o'ldirishi alohida hal qilinadi.
3. **`tmobile-ios`** (Telegram-iOS fork) — **oldindan ma'lum cheklov:**
   iOS ilovalarni to'xtatadi va Telegram o'chirish hodisasi uchun push
   yubormaydi, shuning uchun iOS'da capture tabiatan to'liq bo'lmaydi.
   iOS klienti amalda asosan **viewer** bo'lib qoladi.
4. **Jonli push kengaytmasi** — kerak bo'lsa WS payload'iga ko'proq
   ma'lumot qo'shish (hozircha faqat `{seq}`).

---

## 14. Qabul qilish mezonlari (1-bo'lak)

1. tdesktop'da o'chirilgan xabar ushlanadi → 60 soniya ichida serverda
   ko'rinadi → web app'da parol kiritilgach o'qiladi.
2. Server o'chirilgan holatda tdesktop **hech qanday regressiyasiz**
   ishlaydi; server qaytgach outbox to'liq jo'natiladi.
3. Web app'da 10 000+ yozuvda qidiruv, filtr, asc/desc saralash va
   pagination **freez bo'lmasdan** va dublikat/tushib qolishsiz ishlaydi.
4. `.cmx` eksport qilinadi → boshqa qurilmada import qilinadi → yozuvlar
   dublikatsiz merge bo'ladi.
5. Parol, tiklash kodi, email+PIN va OS keystore — to'rttalasi ham
   mustaqil ravishda master kalitni ocha oladi.
6. Qurilma revoke qilinadi → u boshqa push/pull qila olmaydi.
7. Test vektorlari backend va tdesktop'da bir xil natija beradi.
