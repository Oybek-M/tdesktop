# Protokol o'zgarishlari

> Protokolga **tegadigan** har o'zgarish shu yerga bir qator bilan
> yoziladi: sana, nima, nima uchun, qaysi loyihalar ta'sirlanadi.
>
> Boshqa loyiha sessiyasi ishni shu fayldan boshlaydi.

Format: `## YYYY-MM-DD — sarlavha`

---

## 2026-09-05 — payload ochiq `account_id` va `peer_id` ni olib yuradi

**Nima:** har kind'ning shifrlanadigan payload'iga ikkita majburiy
maydon qo'shildi: `account_id` va `peer_id`, ikkalasi ham o'nlik satr —
`account_hash` / `peer_hash` uchun ishlatilgan aynan pre-image. Spec §0.14.

**Nima uchun:** `peer_hash` — HMAC, teskarilab bo'lmaydi. Usiz qabul
qiluvchi yozuvni qaysi lokal chatga yozishni aniqlay olmaydi. Mahalliy
peer'lar ustidan teskari jadval qurish esa aynan kerakli holatni —
boshqa qurilma biladigan, bu qurilma bilmaydigan peer'ni — qamramaydi.

Qo'shimcha: pre-image mavjud bo'lgani uchun qabul qiluvchi
`HMAC(peer_key, peer_id) == peer_hash` ni tekshira oladi (kalit
nomuvofiqligini arzon ushlaydi).

**Ta'sirlanadi:** faqat klientlar. Payload E2E shifrlangan, server uni
ochmaydi va `byte[]` sifatida saqlaydi — server tomonida o'zgarish yo'q.
`test-vectors.json` o'zgarmaydi (vektorlar payload tarkibini
belgilamaydi).

---

## 2026-08-27 — `tombstone` nishoni ochiq maydonda

**Nima:** `tombstone` yozuvi `target_record_id` ni ochiq maydonda ham
yuboradi; `records` ga shu nomli nullable ustun qo'shiladi. Spec §0.13.

**Nima uchun:** §0.3 serverdan asl yozuvni o'chirishni talab qiladi,
lekin nishonni shifrlangan `payload` ichida belgilagan edi — server uni
o'qiy olmaydi. Maxfiylik buzilmaydi: `record_id` allaqachon ochiq
(PRIMARY KEY va har `pull` javobida qaytadi).

**Ta'sirlanadi:** hammasi. tdesktop (plan 02) tombstone push qilganda
maydonni to'ldirishi shart.

---

## 2026-08-26 — ✅ `record_id` akkaunt ajratmasi HAL QILINDI

**Qaror:** faqat (a) variant. `peer_hash` o'zgarmaydi. `record_id`
`account_hash` maydonini oladi, **`activity` kind uchun bo'sh satr**
(§5.1 dagi birlashish talabi saqlanadi). Yangi kalit
`customsync-account-v1`. Spec §0.12 to'liq formula bilan.

`test-vectors.json` qayta generatsiya qilindi: `account_hash` (3),
`record_id` (7→11, jumladan birlashish va ajratma isbotlovchi 4 ta
yangi holat). `peer_hash` o'zgarmadi.

**Ta'sirlanadi:** hammasi, lekin hech kim sync kodini hali
implement qilmagan — eng arzon payt edi.

---

## 2026-08-26 — 🔴 `record_id` da akkaunt yo'q (HAL QILINMAGAN, ↑ ga qarang)

**Nima:** tdesktop'da ko'p akkauntli aralashuv xatosi topildi — baza
`(peer_id, msg_id)` bilan kalitlangan, `account_id` yo'q. 12 ta akkaunt
bitta bazaga yozadi va bir akkauntning o'chirilgan xabarlari boshqasining
chatida arvoh bo'lib chiqadi.

**Nima uchun protokolga tegishli:** `record_id` formulasi ham xuddi shu
kamchilikka ega:

```
record_id = SHA256(kind ‖ 0x00 ‖ peer_hash ‖ 0x00 ‖ msg_id ‖ 0x00 ‖ occurred_at)
```

12 akkaunt bitta serverga sync qilsa `(peer_hash, msg_id)` to'qnashadi va
yozuvlar bir-birining ustiga yoziladi — lokal xatoning aynan o'zi, faqat
serverda va **qaytarib bo'lmaydigan** shaklda.

**Variantlar:** (a) `record_id` ga `account_hash` qo'shish — formula
o'zgaradi, `test-vectors.json` qayta generatsiya qilinadi; (b) `peer_hash`
ni `HMAC(key, account_id ‖ peer_id)` qilish — formula o'zgarmaydi.

⚠️ **Sxema raqami suriladi:** v10 endi `account_id`,
`sync_outbox` + `sync_state` esa **v11**.

**Ta'sirlanadi:** hammasi. `customsync-server` implement'idan OLDIN
qaror qilinishi shart.

🔗 **MUHIM istisno — `activity` kind ajratilmaydi.** Ajratish faqat
*bizning* ma'lumotimizga tegishli (o'chirilgan/tahrirlangan xabarlar,
matn keshi, media indeksi). `activity_history` esa **kuzatilayotgan
odam** haqidagi obyektiv fakt — kim kuzatganiga bog'liq emas. Uni
akkauntlarga bo'lib tashlash last-seen bypass qamrovini buzadi;
aksincha, har akkaunt o'z kuzatuv oynasini beradi va birlashganda
surat **aniqroq** bo'ladi.

Bu variant tanlashga bevosita ta'sir qiladi: (a) `record_id` ga
`account_hash` qo'shish — `activity` uchun bo'sh qoldirish mumkin ✅;
(b) `peer_hash` ni `HMAC(key, account_id ‖ peer_id)` qilish —
`activity` ni ham majburan ajratadi ❌. Shu sababli **(a) afzal**.

To'liq tashxis: [`../superpowers/specs/2026-08-26-multi-account-db-isolation-design.md`](../superpowers/specs/2026-08-26-multi-account-db-isolation-design.md) §4.1 va §5.1

---

## 2026-08-25 — `test-vectors.json` yaratildi

**Nima:** Platformalararo test vektorlari birinchi marta
generatsiya qilindi va o'z-o'zini tekshiruvdan o'tkazildi.

Qamrov: HKDF-SHA256 (3 ta kalit), HMAC peer_hash (3 holat),
`record_id` (7 holat — **ikkitasi manfiy `msg_id`**),
AES-256-GCM (3 holat — bo'sh matn, JSON, UTF-8+emoji),
PBKDF2 (3 holat — 600k va 2M iteratsiya).

**Nima uchun:** Spec §11.1 buni "interop buzilishining eng katta
manbai" deb belgilagan. Beshala platforma turli kripto
kutubxonalardan foydalanadi.

**Ta'sirlanadi:** hammasi. Har platforma implement qilinganda
birinchi ish — shu vektorlarni qayta hosil qilish.

---

## 2026-08-25 — Spec §0 REVIZIYA: 11 ta qaror

**Nima:** Spec 2026-07-29 da yozilgan edi; bir oy ichida tdesktop
ancha o'zgardi. 11 ta qaror `§0` bo'limiga yozildi va u asosiy
matndan **ustun turadi**.

Protokolga bevosita tegadiganlari:

| § | Qaror |
|---|---|
| 0.3 | **Retention assimetriyasi** — mijozda qabul filtri, serverda uzunroq saqlash, `tombstone` yangi kind |
| 0.4 | **`media_index`** yangi kind sifatida sync'ga kiritildi |
| 0.5 | **`sha256` majburiy** va **ochiq matn** ustidan hisoblanadi |
| 0.6 | **Manfiy `msg_id`** rasmiylashtirildi (avatar/story) |
| 0.7 | Eksport formati birlashtirildi — barcha klientlar bitta `.cmx` |
| 0.10 | `peer_directory` va `PeerNameCache` birlashtirildi |

**Nima uchun 0.3 muhim:** busiz sync **cheksiz siklga** tushadi —
mijoz 30 kundan eski yozuvni o'chiradi, server qaytaradi, mijoz
yana o'chiradi. Har 30 soniyada.

**Ta'sirlanadi:** hammasi.

---

## 2026-08-25 — `qtwebsockets` moduli tdesktop uchun qurildi

**Nima:** Modul mavjud Qt 6.11.1 ustiga alohida qurildi
(butun Qt qayta qurilmadi). `QWebSocket` endi ishlatilishi mumkin.

**Nima uchun:** Spec §8.5 "yangi kutubxona kerak emas" degan edi,
amalda esa modul bizning Qt'da qurilmagan edi.

**Ta'sirlanadi:** faqat tdesktop. Protokol o'zgarmadi.

Tafsilot: [`../self-update/qtwebsockets-module.md`](../self-update/qtwebsockets-module.md)

---

## 2026-07-29 — Boshlang'ich protokol dizayni

Spec yozildi: kanonik yozuv modeli, `record_id` deterministik
dedup, `observed_at` konflikt qoidasi, `seq` monoton cursor,
kalit ierarxiyasi va key-wrapping, keyset pagination, `.cmx`.

**Ta'sirlanadi:** hammasi.
