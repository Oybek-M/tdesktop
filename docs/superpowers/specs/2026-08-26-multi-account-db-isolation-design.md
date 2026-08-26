# Ko'p akkauntli bazani ajratish — tashxis va dizayn

**Sana:** 2026-08-26
**Holat:** tashxis TUGADI, implement BOSHLANMAGAN
**Qanday topildi:** foydalanuvchi "hech nima o'chirilmagan chatda o'nlab
[DELETED] belgisi bor" deb shikoyat qildi

---

## 0. Qisqacha

Bitta shikoyat ostidan **uchta mustaqil xato** chiqdi:

| № | Xato | Og'irligi |
|---|---|---|
| 1 | Bazada akkaunt ajratmasi yo'q → boshqa akkauntning yozuvlari arvoh xabar bo'lib chiqadi | 🔴 kritik |
| 2 | Saqlangan media placeholder'ga hech qachon biriktirilmaydi | 🟠 yuqori |
| 3 | Arxiv ildizi ko'chganda `media_path` yangilanmaydi | 🟡 o'rta |

1-xato Track C (`customsync-server`) ga bevosita tegadi — 5-bo'limga qarang.

---

## 1. Xato №1 — akkaunt ajratmasi yo'q

### Alomat

Akam (`peer_id = 1334067829`) chatida 218 ta "o'chirilgan" yozuv bor,
shundan **201 tasi bu akkauntga umuman tegishli emas**. Foydalanuvchi bu
chatda deyarli hech narsa o'chirmagan.

### Dalil — ikkita mustaqil ID ketma-ketligi

`text_cache` dagi 353 qatorni `msg_id` bo'yicha saralaganda ular ikkita
uzilgan klasterga bo'linadi:

| Klaster | Qatorlar | ID diapazoni | Sana diapazoni | Monotonlik buzilishi |
|---|---|---|---|---|
| LOW | 97 | 6 … 12 413 | 2023-10-19 … 2026-08-25 | **0** |
| HIGH | 256 | 390 036 … 396 180 | 2026-07-23 … 2026-08-25 | — |

Telegram'da shaxsiy chat xabar ID'lari **akkaunt bo'yicha monoton o'sadi**.
Bitta akkauntda 2026-08-25 kuni bir vaqtda `id=12413` va `id=396180`
bo'lishi mumkin emas. LOW klasterda 2.5 yil davomida bitta ham buzilish
yo'q — u o'zicha butun va toza ketma-ketlik.

Xulosa: **ikkita turli akkaunt bitta bazaga yozmoqda.**

`tdata` da 12 ta akkaunt bor (`user_data` … `user_data#12`), baza esa bitta:
`ArchiveRoot()/db/actioned_messages.db`.

### Ekrandagi aynan qaysi yozuvlar

2026-07-16 21:35:07–21:35:33 oralig'ida (26 soniya) 189 ta xabar
yuborilgan va 21:36:00–21:36:23 da (50 soniyadan keyin) o'chirilgan.
Ular ikkinchi akkauntning Akam bilan yozishmasida sodir bo'lgan:
ID'lar 8480–8668, `is_out=1`, `sender_id=7815174989`.

Xuddi shu 189 ta xabarning ko'zgusi `peer_id=7815174989` ostida ham bor
(ID'lar 392174–392362, `is_out=0`) — bu asosiy akkauntning nusxasi.

### Ildiz sabab zanjiri

1. **Nega arvoh xabarlar ko'rinadi?**
   `History::loadDeletedMessages()` faqat `peer_id` bo'yicha o'qiydi.
2. **Nega boshqa akkauntning qatorlari o'sha peer'ga tushadi?**
   `actioned_messages` kaliti `(peer_id, msg_id)` — `account_id` YO'Q.
3. **Nega bu zararsiz emas?**
   `owner().message(peer, msgId)` topilmasa, qator o'tkazib yuborilmaydi
   — aksincha yangi lokal xabar **yaratiladi**. ID 8480 asosiy akkauntda
   yo'q, demak har safar arvoh paydo bo'ladi.
4. **Nega mazmuni bo'sh?**
   O'chirish paytida media hech qachon yuklanmagan edi (xato №2 ga qarang).

### Ta'sirlangan jadvallar

Beshalasida ham `account_id` yo'q:

```
actioned_messages : id, peer_id, msg_id, type, original_text, new_text,
                    media_path, is_out, msg_date, timestamp, notes,
                    sender_id, is_media
text_cache        : peer_id, msg_id, text, is_out, msg_date, cached_at,
                    sender_id, is_media, is_archived
media_index       : peer_id, msg_id, kind, file_name, rel_path, size,
                    sha256, msg_date, archived_at, layer, status, reason
activity_history  : id, peer_id, field, old_value, new_value, observed_at
ghost_reads       : peer_id, msg_id, timestamp
```

Aralashgan peer'lardagi qatorlar soni (2026-08-26 holati):

| Jadval | Aralashgan | Jami |
|---|---|---|
| `actioned_messages` | 1362 | 25 434 |
| `text_cache` | 2067 | 11 167 |
| `media_index` | 81 | 1687 |

### Qo'shimcha: sozlamalar ham global

`AntiDeletePerPeer`, `MediaBackupPerPeer`, `PeerWhitelist` registrda
`peer_id` kalitli — ya'ni Akam uchun Anti-Delete'ni asosiy akkauntda
yoqsangiz, u **12 akkauntning hammasida** yonadi. Bu ham shu xatoning
bir qismi, lekin alohida hal qilinadi (6-bo'lim).

---

## 2. Xato №2 — saqlangan media biriktirilmaydi

### 2a. Placeholder mediani hech qachon ko'rsatmaydi

`Telegram/SourceFiles/history/history.cpp` (`loadDeletedMessages()`):

```cpp
const auto item = makeMessage(
    WithLocalFlag(HistoryItemCommonFields{ ... }),
    displayText,
    MTP_messageMediaEmpty());   // <-- msg.mediaPath ISHLATILMAYDI
```

`msg.mediaPath` faqat `hasMedia` bayrog'ini hisoblash uchun o'qiladi.
Natijada fayl diskda turgan bo'lsa ham "(media xabar)" matni chiqadi.
Hozirda diskda mavjud 11 ta fayl aynan shu sababdan ko'rinmayapti.

### 2b. Media deyarli hech qachon saqlanmaydi

2481 ta o'chirilgan yozuvdan **atigi 17 tasida** `media_path` bor:

| Oy | O'chirilgan | Media edi | Media saqlangan |
|---|---|---|---|
| 2026-05 | 26 | 0 | 0 |
| 2026-06 | 130 | 51 | 1 |
| 2026-07 | 751 | 642 | 1 |
| 2026-08 | 1574 | 263 | 12 |

Ikkita sabab:

**(i)** `HistoryItem::setDeletedLocally()` mediani faqat ikki holatda
ko'chiradi — hujjat uchun `doc->filepath(true)` diskda mavjud bo'lsa,
rasm uchun `activeMediaView()->loaded()` bo'lsa. Ommaviy forward qilinib
50 soniyada o'chirilgan 189 ta media'ning hech biri yuklanmagan edi,
shuning uchun ikkala shart ham bajarilmadi. `TryRescueMedia()` (L3) esa
kafolat bermaydi — o'chirilgach `file_reference` tez yaroqsiz bo'ladi.

**(ii)** Xabar xotirada bo'lmasa `setDeletedLocally()` **umuman
chaqirilmaydi**. `Session::processMessagesDeleted()`
(`Telegram/SourceFiles/data/data_session.cpp`) bunday holatda:

```cpp
CustomDB::MarkDeleted(
    messageId.v, peerIdStr,
    QString(),          // <-- media_path DOIM BO'SH
    originalText, msgDate, isOut, senderIdStr, isMedia);
```

L2 (`MaybeDownloadMedia()`) 2026-08-14 da qo'shilgan va faqat
MediaBackup yoqilgan peer'larda ishlaydi — shuning uchun 2026-07 dagi
642 ta media'dan bittasi ham saqlanmagan.

---

## 3. Xato №3 — arxiv ildizi ko'chganda yo'llar buziladi

`actioned_messages.media_path` **absolyut** yo'lni saqlaydi.
Arxiv ildizi 2026-08-15 da `C:/Users/Oybek/customizationMainFolder` dan
`C:/Users/Oybek/Pictures/customizationMainFolder` ga ko'chgan, lekin
bazadagi yo'llar yangilanmagan — 17 ta yozuvdan 6 tasi endi mavjud
bo'lmagan faylga ishora qiladi.

`media_index` bu xatodan xoli, chunki u `rel_path` ishlatadi.

---

## 4. Kelishilgan tuzatish rejasi

Foydalanuvchi 2026-08-26 da tasdiqladi: **to'liq yechim** (account_id
ustuni), va uchala media qismi ham.

### 4.1 Sxema v10 — `account_id`

Beshala jadvalga `account_id INTEGER NOT NULL DEFAULT 0` qo'shiladi.

🔴 **Global o'zgaruvchi ISHLAMAYDI.** tdesktop barcha akkauntlarni bir
vaqtda ishlatadi va **fon akkauntlari ham bazaga yozadi** — aralashuv
aynan shundan kelib chiqqan. Shuning uchun akkaunt ID'si har bir chaqiruv
joyidan **aniq uzatilishi shart**:

| Chaqiruv joyi | Manba |
|---|---|
| `Session::processMessagesDeleted()` | `_session->userId()` |
| `HistoryItem::setDeletedLocally()` | `history()->session().userId()` |
| `HistoryItem::restoreFromCustomDB()` | `history()->session().userId()` |
| `History::loadDeletedMessages()` | `session().userId()` |
| `CustomArchive::*` | item'dan olinadi |

O'qishda filtr: `WHERE account_id IN (0, :current)`.
`0` — eski yozuvlar; ular **o'chirilmaydi**, chunki qaysi akkauntga
tegishli ekanini retroaktiv aniqlab bo'lmaydi.

Indekslar `(peer_id, msg_id)` dan `(account_id, peer_id, msg_id)` ga
o'tadi.

### 4.2 Eski yozuvlar uchun ID-diapazon tekshiruvi

`account_id=0` bo'lgan qatorlar hali ham aralashgan. Ularni ko'rsatishdan
oldin ishonchlilik tekshiruvi qo'yiladi:

> Chatdagi **haqiqiy server xabarlari** ichida eng kichik ID `minReal`
> va uning sanasi `minRealDate` bo'lsin. Agar DB qatorining
> `msg_id < minReal`, lekin `msg_date > minRealDate` bo'lsa — bu qator
> boshqa ID-fazosiga tegishli, demak boshqa akkauntniki. Qo'yilmaydi.

Ma'lumot o'chirilmaydi, faqat ko'rsatilmaydi.

### 4.3 Media — uchala qism

1. **Biriktirish.** `loadDeletedMessages()` `MTP_messageMediaEmpty()`
   o'rniga diskdagi faylni lokal media sifatida qo'yadi (fayl mavjudligi
   tekshirilgach). Diskda turgan 11 ta fayl darhol ko'rinadi.
2. **Nisbiy yo'l.** `media_path` `media_index.rel_path` kabi arxiv
   ildiziga nisbatan saqlanadi; migratsiya eski absolyut yo'llarni
   ildizga nisbatan qayta yozadi.
3. **Qamrov.** `processMessagesDeleted()` xabar xotirada bo'lmaganda ham
   `media_index` dan (`status='present'`) yo'lni topib `MarkDeleted()` ga
   uzatadi — hozirgi doimiy bo'sh `QString()` o'rniga.

---

## 5. 🔴 Track C ga ta'sir — implement'dan OLDIN hal qilinsin

Spec `§0.5` va `record_id` formulasida **akkaunt yo'q**:

```
record_id = SHA256(kind ‖ 0x00 ‖ peer_hash ‖ 0x00 ‖ msg_id ‖ 0x00 ‖ occurred_at)
```

12 ta akkaunt bitta serverga sync qilsa `(peer_hash, msg_id)` juftligi
to'qnashadi va yozuvlar bir-birining ustiga yoziladi — aynan shu xato,
faqat endi serverda va **qaytarib bo'lmaydigan** shaklda.

Hal qilish variantlari (`customsync-server` sessiyasida qaror qilinadi):

- `record_id` ga `account_hash` qo'shish (formulani o'zgartiradi →
  `test-vectors.json` qayta generatsiya qilinadi)
- yoki `peer_hash` ni `HMAC(key, account_id ‖ peer_id)` qilib hisoblash
  (formula o'zgarmaydi, kirish ma'lumoti o'zgaradi)

⚠️ Sxema raqami: v10 ilgari `sync_outbox` + `sync_state` uchun band
edi. Endi v10 = `account_id`, sync jadvallari **v11** ga suriladi.

---

## 6. Keyinga qoldirilgan

- **Per-peer sozlamalar ham global.** `AntiDeletePerPeer`,
  `MediaBackupPerPeer`, `PeerWhitelist`, `GhostModePerPeer` registrda
  faqat `peer_id` kalitli. Akkauntga bog'lash kerak, lekin bu alohida ish
  — baza tuzatilgach ko'riladi.

---

## 7. Tekshirish uchun so'rovlar

Tuzatishdan keyin quyidagilar bajarilishi kerak:

```sql
-- 1. Hech bir peer'da ikkita uzilgan ID-fazosi ko'rinmasin (joriy akkaunt uchun)
SELECT peer_id, SUM(msg_id<100000) lo, SUM(msg_id>=300000) hi
FROM actioned_messages WHERE account_id = :current
GROUP BY peer_id HAVING lo>0 AND hi>0;
-- kutilgan: 0 qator

-- 2. Yangi yozuvlarda account_id to'lgan bo'lsin
SELECT COUNT(*) FROM actioned_messages
WHERE account_id = 0 AND timestamp > '2026-08-26';
-- kutilgan: 0

-- 3. media_path nisbiy bo'lsin
SELECT COUNT(*) FROM actioned_messages
WHERE media_path LIKE 'C:/%' OR media_path LIKE '/%';
-- kutilgan: 0
```

Qo'lda sinov: Akam chatini ochib, 9:35 PM dagi uchta "(media xabar)"
arvohi **yo'qolganini** tasdiqlash.
