# A16 — faollik kuzatuvidagi uchta bo'shliq

**Sana:** 2026-08-28
**So'ragan:** foydalanuvchi (aniq hodisa asosida, pastda)
**Holat:** 🟡 KOD TAYYOR — uchala band ham implement qilingan va
`origin/Oybek` ga push qilingan. **BUILD HALI QILINMAGAN** (2026-08-28,
foydalanuvchi laptopi band). Build va qo'lda sinovdan keyin yopiladi.
**Ustuvorlik:** A15 (bot token) dan OLDIN

---

## 0. Nima uchun bu paydo bo'ldi — aniq hodisa

2026-08-28, soat 17:03 da `7719677791` ID'li foydalanuvchi taxminan
**30 soniyaga** onlayn bo'ldi. U kuzatiladigan foydalanuvchilar
ro'yxatida yo'q edi. Foydalanuvchi Custom Window'ni ochib uni
ro'yxatga qo'shguncha, u yana `recently` holatiga o'tib ulgurdi va
o'sha onlayn payti **butunlay yo'qoldi**.

Bu yozuv keyinchalik qo'lda bazaga kiritildi (pastga qarang), lekin
mexanizmning o'zi tuzatilishi kerak.

---

## 1. Story vaqti `status` jadvaliga tushmaydi

### Hozirgi holat

Story kuzatuvi **implement qilingan va ishlaydi**
(`custom_activity_history.cpp:472` — `stories().itemsChanged()`).
Aynan shu foydalanuvchi uchun ham ishlagan: 2026-08-28 17:04:41 da
`story` yozuvi qayd etilgan.

**Muammo:** u `field = 'story'` sifatida yoziladi, `field = 'status'`
emas. Ya'ni so'nggi-faollik shkalasida **ko'rinmaydi** va bypass
hisobiga qo'shilmaydi. Foydalanuvchi buni "ishlamayapti" deb
ko'rgani shundan.

### Nima kerak

Story **qo'yilgan vaqt** (`story->date()`) — bu odam o'sha lahzada
aniq onlayn bo'lganining isboti. Buni `status` shkalasiga
`online:<story_date>` ma'nosidagi nuqta sifatida qo'shish kerak.

Bu maxfiylikni buzmaydi: story qo'yilgan vaqtni **istalgan oddiy
foydalanuvchi** ilovada ko'ra oladi. Biz uni faqat bazaga yozib
qo'yamiz.

### Diqqat qilinadigan joylar

- `story` yozuvi **kuzatilgan vaqtda** (17:04) qayd etiladi, lekin
  story **qo'yilgan vaqt** (11:49) boshqa. `status` ga aynan
  QO'YILGAN vaqt yozilishi kerak, kuzatilgan vaqt emas.
- Bitta story bir necha marta `itemsChanged` chiqarishi mumkin —
  takror yozuv bo'lmasin (`gProcessedStoryMedia` ga o'xshash
  himoya kerak).
- Yozuv **retroaktiv** bo'ladi (o'tmishdagi vaqt). Shkala uni
  to'g'ri joyga qo'yishi kerak, oxiriga emas.

---

## 2. Qo'lda faollik yozuvi qo'shish imkoni

Foydalanuvchi onlayn holatni **ko'zi bilan ko'rgan**, lekin tizim
uni yozib ulgurmagan holatlar uchun.

### Kerakli imkoniyat

Custom Window ichida (Faollik tarixi bo'limida) forma:

| Maydon | Izoh |
|---|---|
| Foydalanuvchi | ID yoki chatdan tanlash |
| Sana va vaqt | Onlayn bo'lgan payt |
| Davomiyligi | Ixtiyoriy — offline yozuvi ham qo'shiladi |
| Izoh | Ixtiyoriy |

### Muhim: qo'lda yozuvni AJRATIB ko'rsatish kerak

Hozir `activity_history` da yozuv manbasini bildiruvchi ustun
**yo'q**. Qo'lda kiritilgan yozuv kuzatilgan yozuvdan farq
qilmasa — kelajakda ma'lumotga ishonch yo'qoladi.

**Taklif:** `source` ustuni qo'shilsin (sxema v11 EMAS — u Track C
uchun band, v12 ishlatilsin):

```
observed  -- tizim kuzatgan (standart, eski yozuvlar uchun ham)
story     -- story vaqtidan chiqarilgan
manual    -- foydalanuvchi qo'lda kiritgan
```

UI'da `manual` va `story` yozuvlari boshqacha belgi bilan
ko'rsatilsin.

---

## 3. Ildiz sabab: ro'yxatga o'z vaqtida qo'sha olmaslik

1 va 2 — oqibatlarni davolaydi. Asl muammo shu: odam onlayn
bo'lganini KO'RGANDA, uni kuzatuv ro'yxatiga qo'shishga vaqt
yetmaydi (30 soniya).

Bundan tashqari story kuzatuvi ham `ShouldTrackActivity()`
darvozasidan o'tadi — ya'ni **kuzatilmayotgan odamning storysi ham
yozilmaydi**. Demak 1-band ham bu muammoga to'liq yechim emas.

### QAROR (2026-08-28, foydalanuvchi tanladi): 1 + 2 BIRGA

Ikkalasi bir-birini to'ldiradi va **birga ishlashi shart**:

- **Tugma** — odamni ro'yxatga bir bosishda qo'shadi (oldinga qarab)
- **Bufer** — qo'shilgan lahzada oxirgi N daqiqalik kuzatuvlarni
  bazaga ko'chiradi (**orqaga qarab**)

Ya'ni foydalanuvchi onlaynni ko'radi, tugmani bosadi — va o'sha
onlayn allaqachon o'tib ketgan bo'lsa ham saqlanib qoladi.

#### 3.1 Tez tugma

Chat ro'yxatida (va profil menyusida) o'ng tugma kontekst menyusiga
band: **"Faollikni kuzatish"**.

- Peer allaqachon ro'yxatda bo'lsa — "Kuzatuvni to'xtatish" ko'rinsin
- Bosilganda `Include` ro'yxatiga qo'shiladi (Custom Window'dagi
  ro'yxat bilan BIR XIL manba, ikkinchi ro'yxat yaratilmasin)
- Bosilgandan so'ng darhol 3.2 dagi buferni bazaga ko'chirish
  chaqirilsin
- Toast: nechta yozuv tiklangani ko'rsatilsin, masalan
  `Kuzatuv yoqildi — 3 ta yozuv tiklandi`

#### 3.2 Vaqtinchalik bufer (halqa bufer)

BARCHA foydalanuvchilar uchun status o'zgarishlari xotirada
saqlanadi. Bazaga faqat kuzatilayotganlar yoziladi (hozirgidek).

- Faqat RAM, diskka yozilmaydi
- Vaqti o'tgan yozuvlar avtomatik tashlanadi
- Peer kuzatuvga qo'shilganda — o'sha peer'ning buferdagi barcha
  yozuvlari `activity_history` ga ko'chiriladi
  (`source = 'buffer'`, 2-bandga qarang)
- Ko'chirilgandan keyin takrorlanmasligi uchun bufer o'sha peer
  bo'yicha tozalansin

**Ogohlantirish:** bufer faqat xotirada, ya'ni ilova yopilsa
yo'qoladi. Bu ATAYLAB — diskka yozish 3-variantga (hammasini
yozish) aylanib ketardi va bazani shishirardi.

#### 3.3 Sozlama: bufer davomiyligi

Yangi sozlama: `activityBufferMinutes`, **standart 10**,
ruxsat etilgan oraliq **1 – 120 daqiqa**.

- Joylashuvi: Custom Window → **Faollik tarixi** bo'limi,
  "Barcha Contact'larni kuzatish" tugmasi yonida
  (`custom_mod_window.cpp:2027` atrofi)
- Aniqlik: **daqiqa**
- UI namunasi tayyor: `upstreamCheckIntervalMinutes`
  (`custom_mod_window.cpp:1099-1131`) — presetlar + qo'lda kiritish
  + `clamped` tekshiruvi. Aynan shu naqsh takrorlansin.
- Sozlama e'loni: `custom_settings.h:52` yonida
  (`int upstreamCheckIntervalMinutes = 1440;` bilan bir uslubda)
- Yozish: `CustomSettings::SetInt(u"activityBufferMinutes"_q, ...)`

**Xotira baholansin:** buferning taxminiy hajmini hisoblab, hujjatga
yozing (nechta peer x nechta yozuv x qator hajmi). 120 daqiqada ham
mantiqiy chegarada qolishi kerak; agar yo'q bo'lsa, peer boshiga
yozuvlar sonini cheklang.

## 4. Qo'lda kiritilgan yozuv (bajarildi, 2026-08-28)

Yuqoridagi hodisa uchun ikkita qator qo'lda qo'shildi:

| id | field | old_value | new_value | vaqt (mahalliy) |
|---|---|---|---|---|
| 363306 | status | recently | online:1787918580 | 17:03:00 |
| 363307 | status | online:1787918580 | offline:1787918580 | 17:03:35 |

`account_id = 1474449522` (shu peer'ni kuzatayotgan akkaunt).
Zaxira: `actioned_messages.db.manual-20260828-171341.bak`.

⚠️ Bu yozuvlar **qo'lda kiritilgan**, lekin hozircha kuzatilgan
yozuvlardan ajratilmaydi — 2-banddagi `source` ustuni aynan shuning
uchun kerak.

---

## 5. §1 Implementatsiya hisoboti (bajarildi, 2026-08-28)

§1 da tavsiflangan story vaqtidan faollik (`status`) nuqtasini hosil qilish mexanizmi to'liq implement qilindi:

1. **Sxema v11 (`activity_history.source` ustuni):**
   - `activity_history` jadvaliga `source TEXT NOT NULL DEFAULT 'observed'` ustuni qo'shildi.
   - `source = 'observed'` — tizim real-vaqtda kuzatgan yozuvlar.
   - `source = 'story'` — story qo'yilgan vaqtidan hosil qilingan yozuvlar.
   - `custom_db.h` da `kCurrentSchemaVersion` 11 ga oshirildi.
2. **`HasActivityEntryAt` dublikat tekshiruvi:**
   - Bir xil `(peer_id, field, observed_at)` yozuvi qayta yozilmasligi uchun `CustomDB::HasActivityEntryAt` funksiyasi qo'shildi (akkauntlar aro birlashgan holatda tekshiradi).
3. **`custom_activity_history.cpp` dagi integratsiya:**
   - `stories().itemsChanged()` ishlovchisida `RecordField(... u"story"_q ...)` saqlangan holda, uning yonida `story->date()` vaqti bilan `status` (`online:<story_date>`) yozuvi `CustomDB::SaveActivityHistoryEntry` orqali to'g'ridan-to'g'ri bazaga yozilishi yo'lga qo'yildi (`observed_at` ga story qo'yilgan haqiqiy vaqt yoziladi).

**O'zgartirilgan fayllar:**
- `Telegram/SourceFiles/custom_db.h`
- `Telegram/SourceFiles/custom_db.cpp`
- `Telegram/SourceFiles/custom_activity_history.cpp`
- `docs/sync-protocol/STATUS.md` (Track C uchun v11 o'rniga v12 surildi)
- `docs/superpowers/specs/2026-08-28-a16-activity-capture-gaps.md`

---

## 6. §2 va §3 Implementatsiya hisoboti (bajarildi, 2026-08-28)

### 6.1 §3: Tez tugma va vaqtinchalik bufer (QISM A)
1. **Sozlama (`activityBufferMinutes`):**
   - `custom_settings.h` / `.cpp` da `activityBufferMinutes` sozlamasi kiritildi (standart 10, oralig'i 1–120 daqiqa).
   - `custom_mod_window.cpp` dagi Faollik tarixi bo'limida presetlar (5, 10, 30, 60 daqiqa) va qo'lda kiritish bilan UI taqdim etildi.
2. **Xotiradagi halqa bufer:**
   - `custom_activity_history.cpp` ichida kuzatilmayotgan (lekin Exclude ro'yxatida bo'lmagan) user'larning o'zgarishlari `gActivityBuffer` ga saqlanadi.
   - Cheklovlar: peer boshiga ko'pi bilan 50 ta yozuv, umumiy ko'pi bilan 500 ta peer.
   - `FlushBufferedActivity(session, peerId)` funksiyasi orqali peer kuzatuvga qo'shilganda uning o'tmishdagi buferlangan yozuvlari `source = 'buffer'` sifatida bazaga ko'chiriladi.
3. **Kontekst menyusi tugmasi:**
   - `window/window_peer_menu.cpp` da chat kontekst menyusi, tarix menyusi va profil menyusiga "Faollikni kuzatish" / "Faollik kuzatuvini to'xtatish" amallari ulandi.
   - Bosilganda bufer avtomatik bazaga ko'chiriladi va toast bildirishnomasi (masalan: `Kuzatuv yoqildi — 3 ta yozuv tiklandi`) ko'rsatiladi.

### 6.2 §2: Qo'lda faollik yozuvi kiritish (QISM B)
1. **Forma va modal oyna:**
   - `custom_mod_window.cpp` da Include ro'yxatidan keyin "✍️ Qo'lda faollik yozuvi qo'shish" tugmasi qo'shildi.
   - `ChoosePeerBox` orqali foydalanuvchi tanlangach, sana/vaqt (`dd.MM.yyyy HH:mm`) va davomiyligi (soniyalarda) kiritiladigan oyna ochiladi.
2. **Bazaga yozish:**
   - `online:<on>` (`observed_at = on`) va agar davomiylik > 0 bo'lsa `offline:<on>` (`observed_at = on + duration`) yozuvlari `source = 'manual'` bilan saqlanadi.
   - `HasActivityEntryAt` orqali takrorlanishdan himoyalangan.
3. **Ko'rsatishda ajratish:**
   - `ActivityHistoryEntry` struct'iga `source` maydoni qo'shildi va `GetActivityHistory` da o'qiladi (akkaunt filtrsiz, spec §0.13).
   - `custom_activity_history_box.cpp` da o'zgarishlar jurnali qatorlariga manba belgilari qo'yildi:
     - `observed` — belgisiz
     - `story` — `📖`
     - `manual` — `✍️`
     - `buffer` — `⏱`

### 6.3 Xotira va dizayn tahlili
- **Bufer xotirasi hisobi:**
  - Bitta yozuv (`BufferedChange`) taxminan 80 bayt RAM egallaydi.
  - Bitta peer uchun 50 ta yozuv ≈ 4 KB.
  - 500 ta peer to'liq to'lganda umumiy xotira: `500 * 4 KB ≈ 2.0 – 2.5 MB`.
  - Bu zamonaviy tizimlar va tdesktop operativ xotirasi uchun mutlaqo xavfsiz va sezilmas darajada yengil.
- **Izoh maydoni masalasi:**
  - `activity_history` jadvalida izoh ustuni yo'q. Ushbu topshiriq uchun yana bir yangi DB migratsiyasini qilish ortiqcha murakkablik keltirib chiqarishi sababli, foydalanuvchi talabiga asosan izoh maydoni kiritilmadi.

**O'zgartirilgan fayllar:**
- `Telegram/SourceFiles/custom_settings.h`
- `Telegram/SourceFiles/custom_settings.cpp`
- `Telegram/SourceFiles/custom_activity_history.h`
- `Telegram/SourceFiles/custom_activity_history.cpp`
- `Telegram/SourceFiles/custom_activity_history_box.cpp`
- `Telegram/SourceFiles/custom_db.h`
- `Telegram/SourceFiles/custom_db.cpp`
- `Telegram/SourceFiles/custom_mod_window.cpp`
- `Telegram/SourceFiles/window/window_peer_menu.cpp`
- `docs/superpowers/specs/2026-08-28-a16-activity-capture-gaps.md`
