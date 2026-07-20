# Activity History Log — Design

**Sana:** 2026-07-20
**Qamrov:** Kontaktlarning ism/username/rasm/last-seen o'zgarishlarini vaqt bilan saqlash va ikkala UI joyida (Custom Window + Profil sahifasi) ko'rsatish.
**Maqsad:** Faqat ilova qonuniy ravishda (joriy maxfiylik sozlamalari asosida) qabul qilayotgan ma'lumotlarni saqlab, keyinchalik tahlil qilish imkonini berish — hech qanday maxfiylik cheklovini aylanib o'tish yo'q.

---

## 1. Umumiy tamoyillar

- Ilova hech qachon hech kimning maxfiylik sozlamasini aylanib o'tmaydi. Faqat allaqachon ekranda ko'rsatilayotgan/serverdan legal ravishda kelayotgan ma'lumot saqlanadi.
- Saqlash — mavjud SQLite infratuzilmasi (`custom_db.cpp`) ustiga, xuddi shu pattern bilan (yangi jadval, mavjud `execSql`/`sqlite3_prepare_v2` funksiyalari).
- Kuzatish qamrovi — mavjud White/Black List priority modeliga to'liq mos (`Blocklist(false) > Whitelist(true) > standart holat`), lekin **alohida, yangi ro'yxat juftligi** sifatida (mavjud Ghost/AntiDelete/AntiEdit uchun ishlatiladigan White/Black List bilan aralashtirmaslik uchun — bu butunlay boshqa maqsad).
- O'zgarishlarni ushlash — mavjud, markazlashgan `session().changes().peerUpdates(...)` signal oqimiga bitta joydan obuna bo'lish orqali (har bir alohida joyni — `data_session.cpp`dagi setName/setPhoto va h.k. — alohida o'zgartirish shart emas).

---

## 2. Ma'lumotlar bazasi

### 2.1 Yangi SQLite jadvali: `activity_history`

```sql
CREATE TABLE IF NOT EXISTS activity_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    peer_id TEXT NOT NULL,
    field TEXT NOT NULL,      -- "name" | "username" | "photo" | "status"
    old_value TEXT,           -- oldingi qiymat (matn yoki status-JSON)
    new_value TEXT,           -- yangi qiymat
    observed_at INTEGER NOT NULL  -- unix timestamp, biz shu o'zgarishni qachon ko'rganimiz
);
CREATE INDEX IF NOT EXISTS idx_activity_history_peer
    ON activity_history(peer_id, observed_at DESC);
```

- `field = "name"` / `"username"`: `old_value`/`new_value` — oddiy matn.
- `field = "photo"`: `new_value` — mavjud `SaveMediaFile()` orqali saqlangan lokal fayl yo'li (agar rasm ko'rinadigan bo'lsa) yoki bo'sh (agar `userProfilePhotoEmpty` kelsa — ya'ni rasm yashirin/olib tashlangan).
- `field = "status"`: `new_value` — xom holat JSON sifatida saqlanadi: `{"type":"online","expires":T}` / `{"type":"offline","was_online":T}` / `{"type":"recently"}` / `{"type":"lastWeek"}` / `{"type":"lastMonth"}` / `{"type":"empty"}`. Hech qanday oldindan hisoblangan "sessiya" jadvali yo'q — "online bo'lgan davrlar" faqat ko'rish vaqtida shu xom yozuvlar ketma-ketligidan hisoblanadi (pastga qarang, 4-bo'lim).

### 2.2 Migratsiya

Mavjud `schema_version` mexanizmi orqali (`custom_db.cpp`dagi `kCurrentSchemaVersion` va migratsiya bloki) — yangi jadval qo'shish uchun versiya oshiriladi, xuddi mavjud jadvallar (`ghost_reads`, `text_cache` va h.k.) qanday qo'shilgan bo'lsa, xuddi shunday.

---

## 3. Kuzatish qamrovi (Include/Exclude ro'yxatlari)

### 3.1 Ustuvorlik modeli

`ShouldTrackActivity(peerId)`:
1. **Exclude List**da bo'lsa → kuzatilmaydi (eng yuqori ustuvorlik).
2. **Include List**da bo'lsa → kuzatiladi (standart holatdan qat'iy nazar — standart qamrov o'chiq bo'lsa ham).
3. Aks holda → **"Barcha Contact'larni kuzatish" toggle** yoniq **VA** `peer->isContact()` → kuzatiladi.
4. Aks holda → kuzatilmaydi.

Bu — mavjud `CustomSettings::ShouldGhost()`/`ShouldAntiDelete()` funksiyalari bilan bir xil pattern, faqat "standart holat" bu yerda flat global bool emas, balki `(globalToggle && isContact())` kombinatsiyasi.

### 3.2 Ma'lumotlar (`custom_settings.h`/`.cpp`)

- Yangi bool: `activityHistoryTrackAllContacts` (standart: `true`).
- Yangi ikki ro'yxat: `activityHistoryIncludeList` va `activityHistoryExcludeList` — mavjud `Whitelist`/`Blocklist` bilan **bir xil funksiya to'plami** (`AddToActivityInclude`/`RemoveFromActivityInclude`/`GetActivityInclude`/`IsInActivityInclude`, va Exclude uchun mos ravishda), lekin butunlay mustaqil `QMap`larda saqlanadi (mavjud `gWhitelist`/`gBlocklist`dan alohida).
- Yangi funksiya: `bool ShouldTrackActivity(const QString &peerId)` — 3.1-bo'limdagi mantiqni implement qiladi, unified "should" helper'lar qatoriga qo'shiladi.

---

## 4. Last-seen "online bo'lgan davrlar" ni qayta tiklash

Faqat ko'rish vaqtida hisoblanadi (oldindan hisoblangan jadval yo'q):

- `activity_history`dan `field="status"` bo'lgan yozuvlar `observed_at` bo'yicha tartiblanadi.
- `type="online"` yozuvi ko'rilgan vaqt **T1** sifatida qabul qilinadi.
- Undan keyingi `type="offline"` yozuvidagi `was_online` qiymati **T2** sifatida olinadi.
- Natija: "T1 dan T2 gacha online bo'lgan" deb ko'rsatiladi.
- Agar ketma-ket ikkita `online` yozuvi kelsa (masalan T1, T1'), oxirgisi ustun oladi (foydalanuvchi doim online bo'lib qolgan deb hisoblanadi, T1' gacha).

**Joriy + oldingi holat:** UI har doim eng so'nggi 2 ta xom `status` yozuvini ko'rsatadi — "Hozirgi holat: yashiringan" + "Oxirgi marta ko'ra olgan vaqtim: 3 soat oldin, offline edi".

---

## 5. O'zgarishlarni ushlash

Yangi, kichik modul (masalan `custom_activity_history.h`/`.cpp`) quyidagini qiladi:

1. Session tayyor bo'lganda, bitta marta obuna bo'ladi:
   ```cpp
   session->changes().peerUpdates(
       Data::PeerUpdate::Flag::Name
       | Data::PeerUpdate::Flag::Username
       | Data::PeerUpdate::Flag::Photo
       | Data::PeerUpdate::Flag::OnlineStatus
   ) | rpl::on_next([=](const Data::PeerUpdate &update) { ... });
   ```
2. Har bir kelgan update uchun: `update.peer->asUser()` orqali User ekanini tekshiradi, `CustomSettings::ShouldTrackActivity(peerId)` orqali kuzatilishi kerakligini tekshiradi, va tegishli maydon(lar)ni (`Flags`ga qarab: Name/Username/Photo/OnlineStatus) `custom_db`ga yozadi (eski qiymat bilan solishtirib, faqat haqiqiy o'zgarish bo'lsa yozadi — takroriy bir xil qiymatni qayta yozmaslik uchun). Har bir peer/maydon juftligi uchun **birinchi marta** kuzatilganda (ya'ni o'sha peer uchun `activity_history`da hali hech qanday yozuv yo'q) — `old_value = NULL` bilan yoziladi (bu "o'zgarish" emas, "kuzatish boshlangan sanadagi boshlang'ich holat" sifatida ko'rsatiladi).
3. Rasm o'zgarganda (`Flag::Photo`), agar yangi rasm mavjud bo'lsa, mavjud `CustomDB::SaveMediaFile()` orqali lokal nusxa saqlanadi (mavjud infratuzilma, media fayllarni saqlash uchun allaqachon bor).

---

## 6. UI

### 6.1 Custom Window → Peers tab → "🕒 Activity History" bo'limi

Individual sozlamalar (istisnolar) bo'limidan **pastda**, alohida blok:

- "Barcha Contact'larni kuzatish" toggle.
- Include List (Chat tanlash / ID orqali qo'shish / ro'yxat / o'chirish — mavjud White/Black List UI pattern'i bilan bir xil).
- Exclude List (xuddi shunday).
- **Kuzatilayotganlar ro'yxati**: hozir amalda kuzatilayotgan barcha peer'lar (3.1-bo'lim mantig'i bo'yicha hisoblangan), har birining yonida "📜 Tarixni ko'rish" tugmasi.

### 6.2 Profil sahifasi (`info_profile`)

Har bir User uchun profil sahifasida "📜 Faollik tarixi" tugmasi (faqat `ShouldTrackActivity(peerId)` true bo'lganda ko'rinadi).

### 6.3 Umumiy "Tarix ko'ruvchi" oynasi (ikkala joydan ham ochiladi, bitta implementatsiya)

Ikkala joy ham **bitta umumiy Box** ochadi (kod takrorlanmasligi uchun), tarkibi:

1. **Joriy holat** — hozirgi ism, username, rasm, last-seen (yoki "yashirilgan").
2. **So'nggi ko'ra olgan holatim** (agar hozir yashirilgan bo'lsa) — oxirgi legal ko'rilgan qiymat + vaqti.
3. **Online bo'lgan davrlar** — "14:32 – 15:10, 38 daqiqa" formatidagi ro'yxat (4-bo'lim algoritmi).
4. **To'liq o'zgarishlar jurnali** — vaqt bo'yicha teskari tartiblangan: "Ism: 'Ali' → 'Aliyor' (2026-07-15 14:20)", "Rasm yangilandi", "Username o'zgardi" va h.k.

---

## 7. Qamrovdan tashqari (bu safar)

- Guruh/kanal a'zolari uchun kuzatish — faqat User (shaxsiy chat) peerlar uchun.
- Avtomatik eskirgan yozuvlarni tozalash (pruning) — hozircha cheklovsiz saqlanadi; agar kelajakda DB hajmi muammo bo'lsa, alohida so'rov bilan qo'shiladi.
- Bio/"about" maydoni o'zgarishlari — faqat ism/username/rasm/last-seen so'ralgan, bio kiritilmagan.
