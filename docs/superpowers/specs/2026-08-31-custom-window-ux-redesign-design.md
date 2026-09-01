# Custom Window UX qayta dizayni

**Sana:** 2026-08-31
**Holat:** 🟡 SPEC — foydalanuvchi tasdig'i kutilmoqda
**Fayl:** `Telegram/SourceFiles/custom_mod_window.cpp` (3383 qator)

---

## 1. Muammo

Foydalanuvchi to'rt narsani tasdiqladi:

1. General tab juda uzun va tartibsiz
2. Tushuntirish matnlari uzun
3. Bo'limlar ochiq holatda chiqadi
4. **Funksiyalar noto'g'ri tablarga tarqalgan**

### O'lchangan holat (2026-08-31)

| Ko'rsatkich | Qiymat |
|---|---|
| Fayl hajmi | 3383 qator |
| Satr literallari | 409 |
| 120+ belgili matn | 13 |
| 200+ belgili matn | 2 (eng uzuni 326) |
| `FlatLabel` | 80 |
| `SlideWrap` | 16 |
| `AddDivider` | 10 |
| **`AddSubsectionTitle`** | **0** ← bo'lim sarlavhasi umuman yo'q |

> ⚠️ Eski hujjatda "75 ta matn qisqartiriladi" deb yozilgan edi —
> bu noto'g'ri. Haqiqiy raqam: 60+ belgili 36 ta, 120+ belgili 13 ta.

### Hozirgi tablar

| Tab | Qator | Ichida |
|---|---|---|
| **About** | **750** 🔴 | zaxira, arxiv papkasi, media backup, kvota, indekslash, eksport, tiklash, arxiv boshqaruvi, bomb-media, **XAVFLI HUDUD** |
| General | 486 | qurilma soxtalashtirish, **Privacy & Custom Mods tugmalari**, branding, mutual-contact emoji |
| ↳ upstream | 221 | rasmiy versiya tekshiruvi (General ichida chaqiriladi) |
| Peers | ~800 | Include/Exclude ro'yxatlari, per-chat istisnolar, faollik tarixi |
| ↳ activity | 378 | faollik tarixi (Peers ichida) |
| Archive | 197 | o'chirilgan/tahrirlangan xabarlar ko'rgichi |

**Eng katta nuqson:** "About" tabi aslida About emas — u ombor,
zaxira va xavfli amallar tabi. Kvota sozlamalari va "BARCHA arxivni
tozalash" tugmasi "Dastur haqida" ostida turishi mantiqsiz.

---

## 2. Yechim: funksiya bo'yicha 7 tab

Foydalanuvchi 6 tabni tanladi. Lekin Arxiv 950 qatorga chiqadi —
"hech bir tab 400 qatordan oshmasin" maqsadiga zid. Shuning uchun
Arxiv ikkiga bo'linadi: **ko'rish** va **boshqarish**.

| # | Tab | Ichida | Manba |
|---|---|---|---|
| 1 | **Yashirinlik** | Ghost Mode, Anti-Delete, Anti-Edit, Bypass, Story anonim ko'rish, Offline DB | General ichidagi "Privacy & Custom Mods" |
| 2 | **Ko'rinish** | Qurilma soxtalashtirish (nom/versiya/tur), Branding (chat nomi, icon), Mutual-contact emoji (4 joy) | General'ning qolgani |
| 3 | **Chatlar** | Include ro'yxati, Exclude ro'yxati, Per-chat istisnolar | Peers |
| 4 | **Faollik** | Faollik tarixi, vaqtinchalik bufer, qo'lda yozuv, Include/Exclude | Peers'dan ajratiladi |
| 5 | **Arxiv** | O'chirilgan va tahrirlangan xabarlar ko'rgichi | Archive (o'zgarishsiz) |
| 6 | **Ombor** | Arxiv papkasi, media backup, kvota, indekslash, eksport/import, bomb-media | About'dan |
| 7 | **Tizim** | Versiya tekshiruvi, dastur haqida, ⚠️ Xavfli hudud | General + About'dan |

**"About" tabi yo'qoladi.** Uning mazmuni Ombor va Tizim ga bo'linadi.

### Nima uchun aynan shunday

- **Yashirinlik alohida** — bular mahsulotning asosiy funksiyalari,
  qurilma soxtalashtirish sozlamalari ichida yashirinib turmasligi kerak
- **Faollik alohida** — 378 qator, mustaqil mavzu, Chatlar bilan
  faqat "peer ro'yxati" jihatidan o'xshash
- **Ombor va Arxiv ajratiladi** — biri ma'lumotni KO'RISH, ikkinchisi
  DISK va FAYLLARni boshqarish. Turli maqsad, turli xavf darajasi
- **Xavfli hudud Tizim'da** — kamdan-kam ochiladigan tab, tasodifan
  bosish ehtimoli past

---

## 3. Har tab uchun umumiy qoidalar

### 3.1 Bo'lim sarlavhalari

Hozir **0 ta** sarlavha bor, faqat 10 ta ajratgich. Har mantiqiy
bo'lim `Ui::AddSubsectionTitle` bilan sarlavhalanadi.

### 3.2 Yopiq bo'limlar

Barcha bo'limlar `SlideWrap` ichida, **standart holatda YOPIQ**.
Sarlavha bosilganda ochiladi.

Istisno: har tabning BIRINCHI bo'limi ochiq bo'lsin — aks holda
tab bo'sh ko'rinadi.

### 3.3 Matnlarni qisqartirish

13 ta matn 120+ belgi. Qoida:

- Tugma tagidagi izoh — **eng ko'pi 2 qator** (~100 belgi)
- Uzunroq tushuntirish kerak bo'lsa — sarlavha yonidagi `?`
  tugmasi ostiga, alohida oynaga

Misol (hozir 191 belgi):
> "Online holatini, yozmoqda belgisini va xabar o'qildi
> bildirishnomasini yashiradi.\n\nYoqilgach, to'liq kuchga kirishi
> uchun 1-2 daqiqa ketishi mumkin."

Qisqartirilgani:
> "Online, yozmoqda va o'qildi belgilarini yashiradi."

Ikkinchi jumla (`1-2 daqiqa`) — `?` ostiga.

### 3.4 Fayl bo'linishi

3383 qator bitta faylda. Har tab alohida faylga chiqarilsin:

```
custom_mod_window.cpp        — oyna, tab boshqaruvi (~400)
custom_tab_privacy.cpp       — 1-tab
custom_tab_appearance.cpp    — 2-tab
custom_tab_chats.cpp         — 3-tab
custom_tab_activity.cpp      — 4-tab
custom_tab_archive.cpp       — 5-tab
custom_tab_storage.cpp       — 6-tab
custom_tab_system.cpp        — 7-tab
```

Umumiy yordamchilar (`AddAvatarPeerRow`, `ChoosePeerBox`,
`addToggle`, `addSection`) — `custom_tab_common.{h,cpp}`.

> ⚠️ Yangi fayllar `Telegram/CMakeLists.txt` ga qo'shilishi shart.

---

## 4. Bosqichma-bosqich bajarish

Bitta katta o'zgarish emas — har bosqich alohida build va sinovdan
o'tadi. Har bosqich mustaqil qiymat beradi.

| Bosqich | Ish | Xavf |
|---|---|---|
| **1** | Yordamchilarni `custom_tab_common` ga ajratish, mavjud 4 tab alohida fayllarga (mazmun O'ZGARMAYDI) | past — sof ko'chirish |
| **2** | 7 tabga qayta bo'lish, tab paneli yangilanishi | o'rta |
| **3** | Bo'lim sarlavhalari + hamma `SlideWrap` yopiq | past |
| **4** | Matnlarni qisqartirish + `?` tugmalari | past |

**1-bosqich alohida commit va build bo'lishi SHART** — sof
ko'chirishni mazmun o'zgarishidan ajratmasa, xato qidirish
imkonsiz bo'ladi.

---

## 5. v1 ga KIRMAYDIGAN narsalar (YAGNI)

- Yangi funksiya qo'shilmaydi — faqat mavjudlari qayta joylashadi
- Ranglar va stil o'zgarmaydi
- Qidiruv maydoni (sozlama qidirish) — keyingi ish
- `custom_mod_settings.cpp` — **o'lik kod** (CMake'da yo'q, hech kim
  chaqirmaydi). Bu ishda TEGILMAYDI, lekin alohida o'chirilishi kerak

---

## 6. Ochiq savol

Tab soni 7 ga chiqdi. Agar 7 ta ko'p bo'lsa, muqobil: **Ombor**ni
**Arxiv** ichiga qaytarib, ikkalasini bitta tabda ikki bo'lim qilish
(u holda 6 tab, lekin Arxiv ~950 qator).

Tavsiyam — 7 tab.

---

## 7. Bajarildi va TUZATILDI (2026-08-31 / 09-01)

🔴 **Diqqat:** Gemini bu bo'limga dastlab NOTO'G'RI hisobot
yozgan edi (masalan Chatlar tabida "🌐 Global rejim" bo'limi bor
deb). O'sha bo'lim MAVJUD BO'LMAGAN API'lardan foydalanardi,
kompilyatsiya bo'lmasdi va olib tashlandi. Quyidagi jadval
**build'dan o'tgan haqiqiy holat** (2026-09-01, o'lchangan).

| Tab | Bo'lim | Qator | Izoh |
|---|---|---|---|
| Yashirinlik | 1 | 122 | ochiq |
| Ko'rinish | 3 | 398 | 1-si ochiq |
| Chatlar | 3 | 761 | Include (ochiq), Exclude, Per-chat |
| Faollik | 3 | 372 | 1-si ochiq |
| **Arxiv** | **0** | 207 | ⚠️ bo'limlarga o'ralmagan — pastga qarang |
| Ombor | 6 | 539 | 1-si ochiq |
| Tizim | 3 | 332 | `⚠️ Xavfli hudud` HAR DOIM yopiq |

### Nima uchun Arxiv bo'limlarsiz qoldi

Gemini uni `CustomDB::EditRecord` ning MAVJUD BO'LMAGAN maydonlari
bilan qayta yozgan edi (`date`, `editDate`, `isOut`, `oldText` —
haqiqiylari: `msgDate`, `editedAt`, `originalText`). Fayl
1-bosqichdagi ishlaydigan holatga qaytarildi. Bo'limlarga o'rash
keyingi ishga qoldirildi — ishlaydigan kod muhimroq edi.

### Gemini keltirib chiqargan boshqa nuqsonlar (tuzatilgan)

Barchasi 2-bosqichda (`da1ea72460`) paydo bo'lgan — u "sof
ko'chirish" bo'lishi kerak edi, lekin Gemini kod O'YLAB TOPGAN:

| Fayl | O'ylab topilgani | Tuzatish |
|---|---|---|
| `chats` | `PeerListMode`, `WhiteList`, `BlackList`, `Ui::Radiobutton`; `fillPeerSection` 4 → 12 argument | 1-bosqich kodidan qayta qurildi |
| `storage` | 6 ta media tugmasi (`mediaBackupPhoto`, `mediaQuotaAutoClean`...) | 62 qator olib tashlandi |
| `archive` | `EditRecord` ning yo'q maydonlari | 1-bosqichga qaytarildi |
| `activity` | `CustomActivityHistoryBox` klassi | `MakeHistoryBox()` ga to'g'irlandi |
| `system` | "Oxirgi tekshiruv" bloki YO'QOLGAN | tiklandi |
| `common` | Ortiqcha `}` (anonim namespace ochilmasdan yopilgan) | ~15 sintaksis xatosining sababi edi |
| `common` | `gInstance` — `CustomModWindow` to'liq turi tab fayllarda yo'q | `ShowCustomBox()` funksiyasiga o'tkazildi |
| `common` | `AddSubsectionTitle` `FlatLabel` qaytaradi, bosilmaydi | sarlavha `SettingsButton` ga almashtirildi |

### Saboq

**Diffni o'qish yetarli emas.** `storage` da 786 qator o'zgargan
edi — yo'qotishni ko'rish imkonsiz. Ishlagan usul: har fayldagi
foydalanuvchiga ko'rinadigan **satrlar to'plamini** eski/yangi
holatda solishtirish. Aynan shu bilan "Oxirgi tekshiruv" blokining
yo'qolgani topildi.

### Foydalanuvchi bahosi (2026-09-01, sinovdan keyin)

"UX redesign yaxshilangan, hozircha yetadi" — keyinroq yana
qayta ko'riladi. **Faollik tarixi oynasi alohida redesign talab
qiladi** (o'qish noqulay).
