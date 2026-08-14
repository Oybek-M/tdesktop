# Katta media backup + indeks va eksport — dizayn

**Sana:** 2026-08-14
**Holat:** dizayn tasdiqlangan, reja hali yozilmagan
**Bog'liq:** A13 (AntiDelete arxivi), A11 (story backup), Track C (customsync-server)

---

## 1. Muammo

2026-08-14 da kritik crash tuzatildi: `DocumentData::save()` ga bo'sh maqsad
nomi berish "faylni xotiraga yukla" degani va `FileLoader` buni 10 MB bilan
cheklaydi (`file_download.cpp:114` `Expects`). Chegaradan oshgan har qanday
fayl ilovani darhol yiqitardi.

Tuzatish `document->size > Storage::kMaxFileInMemory` tekshiruvini qo'shdi —
ya'ni **10 MB dan katta media endi oldindan yuklab olinmaydi**.

Bu spec o'sha bo'shliqni yopadi va shu bilan birga eksport/import tizimini
Track C ga tayyorlaydi.

### 1.1. Hozir nima ISHLAYAPTI (yo'qotilmagan)

Buni aniq bilish muhim, aks holda muammo kattaroq ko'rinadi:

`DocumentData::finishLoad()` (`data_document.cpp:1064`) — har qanday hujjat
yuklab bo'lingach, uni doimiy arxivga nusxalaydi:

```cpp
const auto cachePath = _loader->fileName();
if (!cachePath.isEmpty() && CustomSettings::AntiDelete()) {
    CustomDB::SaveMediaFile(cachePath, "file");
}
```

Ya'ni foydalanuvchi **ochgan / o'ynatgan / yuklagan** media hozir ham
**istalgan hajmda** arxivlanadi. Yo'qolgan yagona narsa — foydalanuvchi
hech qachon ochmagan katta faylni oldindan yuklab qo'yish.

### 1.2. Hozir nima ISHLAMAYAPTI

| № | Kamchilik |
|---|---|
| P1 | >10 MB media, agar foydalanuvchi uni ochmagan bo'lsa, umuman saqlanmaydi |
| P2 | Saqlanmagan media haqida **hech qanday iz qolmaydi** — "bunday fayl bor edi" ma'lumoti ham yo'q |
| P3 | `finishLoad` faqat **global** `AntiDelete()` ni tekshiradi. Global o'chiq + chat White List'da → media saqlanmaydi (`ShouldAntiDelete()` mantig'i bilan ziddiyat) |
| P4 | Eksport media'ni **har doim** o'z ichiga oladi — tanlash imkoni yo'q, natijada eksport o'nlab GB bo'lishi mumkin |
| P5 | A11 story backup real sinovdan o'tmagan (`A11 Task 6` hamon pending) |

---

## 2. Uch qatlamli arxivlash

Qatlamlar bir-birini almashtirmaydi — to'ldiradi.

| Qatlam | Qachon ishga tushadi | Narxi | Ishonchliligi |
|---|---|---|---|
| **L1 — Bazaviy** | Foydalanuvchi media'ni ochganda | 0 (fayl allaqachon yuklangan) | To'liq |
| **L2 — Asosiy** | Ro'yxatdagi chatlarda, oldindan | Disk + trafik | To'liq |
| **L3 — Oxirgi imkoniyat** | O'chirish aniqlanganda, agar arxivda bo'lmasa | ~0 | **Past** |

### L1 — Bazaviy (allaqachon bor, faqat tuzatiladi)

`finishLoad` hook'i saqlanadi. Ikkita tuzatish kiritiladi — 6-bo'limga qarang.

### L2 — Asosiy (yangi)

`CustomArchive::MaybeDownloadMedia()` da: chat ro'yxatda bo'lsa va fayl
chegaradan oshmasa, **haqiqiy maqsad fayl yo'li bilan** yuklab olinadi.

```cpp
// Bo'sh nom EMAS — aynan shu crash'ga olib kelgan edi.
document->save(origin, targetPathInArchive);
```

Maqsad yo'li: `~/customizationMainFolder/medias/<turi>/<peerId>_<msgId>_<nom>`
— mavjud `SaveMediaFile()` papka sxemasi bilan bir xil.

### L3 — Oxirgi imkoniyat (yangi)

O'chirish aniqlanganda (`setDeletedLocally()` yo'li), agar media hali
arxivda bo'lmasa — yuklashga urinib ko'riladi.

**Ochiq e'tirof:** bu ishonchsiz. Xabar o'chirilgach `file_reference` tez
orada yaroqsiz bo'ladi va server `FILE_REFERENCE_EXPIRED` qaytaradi.
Shuning uchun L3 **kafolat emas, bonus** — urinish tekin, muvaffaqiyatsiz
bo'lsa indeksda `status='missing'` bo'lib qoladi (P2 hal bo'ladi).

---

## 3. Qaysi chatlar uchun yoqiladi

```
YOQILADI:     chat White List'da   YOKI   per-chat "Media Backup" toggle
HECH QACHON:  chat Black List'da   YOKI   peer->isSelf()
```

### 3.1. 🔴 `ShouldAntiDelete()` zanjiriga ERGASHMAYDI

Bu spec'dagi eng muhim qoida.

Mavjud zanjir: `Blocklist > Whitelist > per-peer override > global bayroq`.
Oxirgi bo'g'in — **global bayroq** — muammo tug'diradi: 2026-08-14 dagi baza
tozalashida kuzatilayotgan **981 ta peer** aniqlangan edi, ularning
aksariyati botlar va bir martalik kontaktlar. Global AntiDelete yoqilgani
uchun shunday bo'lgan.

AntiDelete uchun bu normal (chat boshiga bir necha KB matn). Media backup
uchun esa halokatli — botlardan kelgan har bir faylni yuklab olardik.

Shuning uchun media backup **faqat aniq belgilangan a'zolikka** qaraydi:
White List (hozir 5 ta chat) yoki per-chat toggle. Global bayroq uni
**yoqmaydi**.

### 3.2. Saved Messages — qat'iy istisno

Sababi shunchaki "o'zimizniki" emas: **uni foydalanuvchidan boshqa hech kim
o'chira olmaydi.** AntiDelete'ning butun tahdid modeli "suhbatdosh o'chirib
yubordi" ustiga qurilgan — Saved Messages'da bunday tahdid yo'q.
Bu vaqtinchalik sozlama emas, `peer->isSelf()` bo'yicha kod darajasidagi
istisno.

### 3.3. UI

Per-chat toggle "Individual sozlamalar (istisnolar)" bo'limiga tushadi —
Ghost Mode / Anti-Delete / Anti-Edit yoniga **4-qator**. Yangi ro'yxat
shakllantirilmaydi (foydalanuvchi qarori).

---

## 4. Sozlamalar

Ikkalasi ham Custom Window'da o'zgartiriladi (faqat kod ichida emas).

| Sozlama | Kalit | Default | Chegaralar |
|---|---|---|---|
| Bitta fayl uchun yuqori chegara | `mediaBackupMaxFileMb` | **100 MB** | 10 MB – 2048 MB |
| Umumiy kvota | `mediaBackupQuotaGb` | **10 GB** | 1 GB – 500 GB |

Pastki chegara 10 MB — undan kichigi hozirgi xatti-harakatdan yomonroq
bo'lardi.

### 4.1. Kvota hisobi

Kvota **faqat** `~/customizationMainFolder/medias/` hajmini o'lchaydi
(baza va sozlamalar hisobga olinmaydi).

- Ishga tushishda bir marta to'liq hisoblanadi
- Har yozuvdan keyin qo'shib boriladi (qayta skanerlash yo'q)
- Indeks jadvalidagi `size` yig'indisi bilan davriy solishtiriladi

### 4.2. Kvota to'lganda

**Hech narsa o'chirilmaydi** (foydalanuvchi qarori — bu qat'iy talab).

| Qatlam | Kvota to'lganda |
|---|---|
| L1 (foydalanuvchi ochgan) | **Davom etadi** — foydalanuvchi faylni ataylab ochgan, uni yo'qotish mumkin emas |
| L2, L3 (oldindan / qutqaruv) | **To'xtaydi** |

Yuklanmagan fayllar indeksga `status='pending'` bilan yoziladi — kvota
kengaytirilgach ular qayta urinib ko'rilishi mumkin.

**Ogohlantirish:** kvota to'lgan holatda ilova **har safar ishga
tushganda** box ko'rsatadi (toast emas — tasdiqlash talab qilinadi).
Box'da: joriy hajm / kvota, "Sozlamalarni ochish" tugmasi. Muammo hal
bo'lmaguncha (hajm kvotadan pastga tushmaguncha yoki kvota
kengaytirilmaguncha) takrorlanaveradi.

---

## 5. Media indeks (yangi)

P2 ni hal qiladi: saqlanmagan media haqida ham iz qoladi.

### 5.1. Nima uchun SQLite jadvali

Indeks `actioned_messages.db` ichida **yangi jadval** bo'ladi, alohida
fayl emas. Sababi: baza eksportga **allaqachon** kiradi — demak "indeks
doim eksport qilinadi" talabi qo'shimcha ishsiz bajariladi.

### 5.2. Sxema (schema v7)

```sql
CREATE TABLE IF NOT EXISTS media_index (
    peer_id     TEXT    NOT NULL,
    msg_id      INTEGER NOT NULL,
    kind        TEXT    NOT NULL,   -- image | video | voice | file
    file_name   TEXT,               -- arxivdagi nom (status='present' bo'lsa)
    rel_path    TEXT,               -- medias/videos/... (arxiv ildizidan)
    size        INTEGER NOT NULL DEFAULT 0,
    sha256      TEXT,               -- faqat present bo'lganda hisoblanadi
    msg_date    INTEGER,
    archived_at INTEGER,
    layer       TEXT,               -- l1 | l2 | l3
    status      TEXT NOT NULL,      -- present | pending | missing
    reason      TEXT,               -- pending/missing sababi (too_large, quota_full, reference_expired ...)
    PRIMARY KEY (peer_id, msg_id)
);
CREATE INDEX IF NOT EXISTS idx_mi_status ON media_index(status);
CREATE INDEX IF NOT EXISTS idx_mi_peer   ON media_index(peer_id);
```

`status` qiymatlari:

| Qiymat | Ma'nosi |
|---|---|
| `present` | Fayl arxivda mavjud |
| `pending` | Fayl mavjud emas, lekin yuklab olish mumkin (kvota, chegara) |
| `missing` | Fayl yo'q va endi olib bo'lmaydi (reference eskirgan) |

`sha256` — dublikatlarni aniqlash va Track C da blob identifikatsiyasi uchun.

---

## 6. Mavjud `finishLoad` hook'ining ikkita tuzatishi

### 6.1. Ikki marta nusxalash tuzog'i (yangi xavf)

L2 faylni **to'g'ridan-to'g'ri** arxiv papkasiga yuklaydi. Shunda
`finishLoad` ishga tushib `_loader->fileName()` ni oladi — bu endi
arxiv yo'lining o'zi — va uni `SaveMediaFile()` bilan **yana** nusxalaydi.
Fayl o'zini o'ziga ko'chiradi.

Yechim: manba yo'li arxiv daraxti ichida bo'lsa hook'ni o'tkazib yuborish.

### 6.2. Per-peer xatosi (P3, mavjud xato)

Hozirgi kod faqat global `AntiDelete()` ni tekshiradi. Koddagi izoh
"no per-peer id available here" deb da'vo qiladi, lekin bu **noto'g'ri** —
`FileLoader::fileOrigin()` mavjud (`file_download.h:98`).

Yechim: `_loader->fileOrigin()` dan `Data::FileOriginMessage` ni ajratib,
peer id ni olish va `ShouldAntiDelete(peerId)` ni ishlatish. Origin
xabarga bog'liq bo'lmasa (masalan avatar) — hozirgidek global bayroqqa
qaytish.

---

## 7. Eksport / import

### 7.1. Hozirgi holat

`ExportFullBackup()` (`custom_db.cpp:1310`) manifest v2 bilan quyidagilarni
ZIP qiladi: baza, **butun** `customizationMainFolder`, `peer_lists.json`,
`branding.json`, `settings.reg` (Windows), `manifest.json`.
Async o'ramlar (`ExportFullBackupAsync`) allaqachon `crl::async` da ishlaydi.

### 7.2. Yangi tuzilma — ikkita alohida arxiv

Foydalanuvchi talabi: indeks doim, media ixtiyoriy, uzatish fonda.

| Arxiv | Mazmuni | Hajmi | Qachon |
|---|---|---|---|
| `CustomModBackup_<stamp>.zip` | Baza (indeks ichida), sozlamalar, registry, manifest | MB'lar | **Doim** |
| `CustomModMedia_<stamp>.zip` | `customizationMainFolder/medias/` | GB'lar | Faqat tanlansa |

**Nima uchun ikkita fayl, bitta emas:** ZIP yaratilgach unga qo'shib
bo'lmaydi. Ikkita alohida arxiv bilan birinchisi soniyalarda tayyor
bo'ladi va darhol ishlatsa bo'ladi, ikkinchisi esa fonda davom etadi.
Bu Track C ga ham to'g'ridan-to'g'ri mos tushadi: indeks har doim
sinxronlanadi, bloblar alohida.

### 7.3. Manifest v3

```json
{
  "version": 3,
  "createdAt": "...", "sourceHost": "...", "os": "...",
  "hasRegistry": true, "hasPeerLists": true, "hasBranding": true,
  "mediaIncluded": false,
  "mediaArchive": "CustomModMedia_20260814_193000.zip",
  "mediaTotalBytes": 12884901888,
  "counts": { "deleted": 0, "edited": 0, "images": 0, "videos": 0,
              "voices": 0, "files": 0,
              "indexPresent": 0, "indexPending": 0, "indexMissing": 0 }
}
```

`mediaIncluded` maydoni **"media yo'q edi"** va **"media ataylab
chiqarib tashlandi"** holatlarini ajratadi — v2 dagi `hasMedia` buni
ajrata olmasdi.

### 7.4. Import

- Faqat asosiy arxiv berilsa → baza + sozlamalar tiklanadi. Indeksdagi
  yozuvlar `status='present'` bo'lsa-yu fayl topilmasa → `missing` ga
  o'tkaziladi (yolg'on ma'lumot qolmasligi uchun)
- Media arxivi ham berilsa → fayllar joyiga qo'yiladi va tegishli
  yozuvlar `present` ga qaytariladi
- Media arxivini **keyinroq alohida** import qilish mumkin bo'lishi kerak
- Mavjud `fullReplace` semantikasi (merge / to'liq almashtirish)
  o'zgarishsiz qoladi

### 7.5. Fon rejimi

Mavjud `ExportFullBackupAsync` / `ImportFullBackupAsync` ishlatiladi.
Media bosqichi alohida vazifa sifatida navbatga qo'yiladi — asosiy
arxiv tayyor bo'lgach foydalanuvchi ishini davom ettiraveradi, media
esa progress ko'rsatkichi bilan fonda ko'chiriladi. Bekor qilish
imkoni bo'lishi kerak (media bosqichi uzoq davom etadi).

---

## 8. Track C (customsync-server) uchun ahamiyati

Bu dizayn Track C ni **bloklamaydi**:

- **Indeks** — kichik, tuzilgan, `sha256` bilan. Qurilmalar orasida
  darhol sinxronlanadi. "Qaysi qurilmada qaysi fayl bor" savoliga javob beradi
- **Bloblar** — alohida masala (server tomonda saqlash, trafik, shifrlash).
  Indeks tayyor bo'lgani uchun bu keyinroq, mustaqil ravishda qo'shiladi

Ya'ni Track C indeks sinxronizatsiyasi bilan boshlanib, blob
sinxronizatsiyasisiz ham foydali bo'ladi.

---

## 9. Scope'dan tashqarida (v1 emas)

| Narsa | Sabab |
|---|---|
| **Chat bo'yicha eksport tanlovi** | v1 da global belgi (media bor / yo'q). Foydalanuvchi javobida "media si bor qismlar media'si bilan" iborasi chat bo'yicha tanlash deb ham o'qilishi mumkin — **tasdiqlash kerak** |
| Kvota to'lganda eski fayllarni o'chirish | Foydalanuvchi aniq rad etdi |
| Blob sinxronizatsiyasi | Track C ning alohida bosqichi |
| Media deduplikatsiyasi (`sha256` bo'yicha) | Indeks buni keyinchalik qo'shishga tayyor qiladi, lekin v1 da emas |

---

## 10. Sinov rejasi

| № | Sinov | Kutilayotgan natija |
|---|---|---|
| T1 | White List'dagi chatda 50 MB video (ochilmagan) | Arxivga tushadi, indeks `present` |
| T2 | Xuddi shu, 500 MB video (chegara 100 MB) | Yuklanmaydi, indeks `pending` / `too_large`, **crash yo'q** |
| T3 | White List'da bo'lmagan chat, global AntiDelete YOQIQ | Media yuklanmaydi (3.1-qoida) |
| T4 | Saved Messages | Hech qachon yuklanmaydi |
| T5 | Black List'dagi chat | Hech qachon yuklanmaydi |
| T6 | Kvota to'ldirilgan holatda qayta ishga tushirish | Box chiqadi, fayllar o'chmaydi |
| T7 | Kvota to'lgan holatda media ochish (L1) | Baribir arxivlanadi |
| T8 | Eksport, media belgisi O'CHIQ | Bitta ZIP, MB'lar, manifest `mediaIncluded: false` |
| T9 | Eksport, media belgisi YOQIQ | Ikkita ZIP, asosiysi tez tayyor |
| T10 | Faqat asosiy arxivni import | Indeks tiklanadi, fayllar yo'q yozuvlar `missing` ga o'tadi |
| T11 | Media arxivini keyinroq import | Yozuvlar `present` ga qaytadi |
| T12 | L2 yuklagan fayl (6.1 tuzog'i) | Fayl ikki marta nusxalanmaydi |
| T13 | Global AntiDelete O'CHIQ, chat White List'da, media ochish | Arxivlanadi (P3 tuzatildi) |
| T14 | **A11 story backup** (P5) | Kichik story arxivlanadi; katta story jimgina o'tkaziladi, indeks `pending` |

---

## 11. Eksport tanlovi — HAL QILINDI (2026-08-14)

Foydalanuvchi qarori: **ikkalasi ham bo'lsin.** Global yoqilgan bo'lsa
hammasi, aks holda chat bo'yicha tanlangani, hech nima tanlanmagan bo'lsa
faqat indeks.

### 11.1. Tanlash qayerda bo'ladi

**"Individual sozlamalar"dan OLINMAYDI.** Sabab: u toggle *arxivlashni*
boshqaradi, *eksportni* emas. 10 ta chatni arxivlab, ulardan 2 tasini
eksport qilish mutlaqo normal — bular ikki xil qaror va ularni bitta
sozlamaga bog'lash noto'g'ri bo'lardi.

**Yechim: eksport paytida tanlash, lekin oddiy chat tanlagich emas.**
Ro'yxat `media_index` dan tuziladi — ya'ni faqat **haqiqatan media'si
bor** chatlar ko'rinadi, har birida hajmi bilan:

```
☑ Hammasi (12.4 GB)
  ☑ Мубина)🎀        2.1 GB    340 fayl
  ☐ Amor Fati❤️‍🔥     8.2 GB   1204 fayl
  ☑ M🐣              1.1 GB     88 fayl
─────────────────────────────
Tanlangan: 3.2 GB
```

Nima uchun bu yaxshiroq:

| Xususiyat | Foyda |
|---|---|
| Faqat media'si bor chatlar | 3-7 qator, yuzlab emas |
| Har qatorda hajm | "Bu 8 GB, keyingi safar" deb qaror qilish mumkin |
| Jonli yig'indi | ZIP hajmi oldindan ma'lum |
| Har eksport uchun alohida | Doimiy holat qo'shilmaydi |
| Oxirgi tanlov eslanadi | Takroriy eksport bir bosishda |

### 11.2. Xatti-harakat jadvali

| Holat | Natija |
|---|---|
| "Hammasi" belgilangan | Barcha media eksport qilinadi |
| Alohida chatlar belgilangan | Faqat o'sha chatlar media'si |
| Hech nima belgilanmagan | Faqat indeks (bitta kichik ZIP) |
