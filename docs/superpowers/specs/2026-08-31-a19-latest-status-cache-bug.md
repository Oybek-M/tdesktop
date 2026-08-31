# A19 — "Joriy holat" eski qiymatni ko'rsatadi

**Sana:** 2026-08-31
**Topgan:** foydalanuvchi (ekran surati bilan)
**Holat:** 🔴 OCHIQ — **YUQORI USTUVORLIK**
**Sabab:** A16 §1 da men qo'ygan shartning yon ta'siri

---

## 1. Belgi

Faollik tarixi oynasida ziddiyat:

| Manba | Ko'rsatgan sana |
|---|---|
| Telegram'ning o'zi (profil) | 30.08.2026 23:21 |
| Jurnaldagi eng yangi yozuv | 30.08.2026 23:21 ✅ |
| **"Joriy holat"** | **24.08.2026 15:02** ❌ |

Peer: `5631101362`.

## 2. Ildiz sabab

Bazadagi yozuvlar TO'G'RI. Eng yangi qator:

```
id=364046  offline:1788114111  observed_at=2026-08-31 14:19:39  source='buffer'
```

Lekin "Joriy holat" xotira keshidan (`gActivityLatestCache`) o'qiydi, va
`SaveActivityHistoryEntry` keshni faqat shu shartda yangilaydi
(`custom_db.cpp:3021` atrofi):

```cpp
if (source == u"observed"_q) {
    gActivityLatestCache[key.peerId][field] = newValue;
}
```

`source='buffer'` bo'lgani uchun kesh yangilanmagan va u hamon
24.08 dagi `observed` qiymatni saqlab turibdi.

### Nima uchun bu shart qo'yilgan edi

A16 §1 da story nuqtalari qo'shilganda: retroaktiv yozuv (o'tmishdagi
vaqt) "joriy holat" ni buzmasligi kerak edi. Fikr to'g'ri edi.

### Nima uchun shart XATO

Shart **manbaga** qarab hukm qiladi, aslida **vaqtga** qarab hukm
qilishi kerak:

| Manba | Retroaktivmi? | Kesh yangilanishi kerakmi? |
|---|---|---|
| `observed` | yo'q | ✅ ha |
| `buffer` | **yo'q** — yaqinda kuzatilgan, faqat kech yozilgan | ✅ **ha** (hozir YO'Q — nuqson shu) |
| `story` | ha | ❌ yo'q |
| `manual` | ha (odatda) | ❌ yo'q |

`buffer` yozuvlari retroaktiv emas — ular bir necha daqiqa oldin
kuzatilgan va odam ro'yxatga qo'shilganda bazaga ko'chirilgan.

## 3. Yechim

Kesh qiymat bilan birga **`observed_at`** ni ham saqlasin va faqat
yozuv keshdagidan **yangiroq** bo'lganda yangilansin.

```cpp
struct CachedLatest {
    QString value;
    qint64 observedAt = 0;
};
static QHash<QString, QHash<QString, CachedLatest>> gActivityLatestCache;
```

`SaveActivityHistoryEntry` da `source` sharti o'rniga:

```cpp
auto &slot = gActivityLatestCache[key.peerId][field];
if (observedAt >= slot.observedAt) {
    slot.value = newValue;
    slot.observedAt = observedAt;
}
```

Shunda to'rt manba ham o'z-o'zidan to'g'ri ishlaydi — manbani
tekshirish umuman kerak emas.

### Preload so'rovi ham xato

`custom_db.cpp:3097` atrofida kesh `MAX(id)` bo'yicha yuklanadi:

```sql
WHERE id IN (SELECT MAX(id) FROM activity_history GROUP BY peer_id, field)
```

`id` — yozilish tartibi, `observed_at` — hodisa vaqti. Retroaktiv
yozuvlar (story backfill) yuqori `id` va eski `observed_at` bilan
kiradi, ya'ni `MAX(id)` noto'g'ri qatorni tanlashi mumkin.

`observed_at` bo'yicha eng kattasini olish kerak, teng bo'lsa `id`
bo'yicha:

```sql
SELECT peer_id, field, new_value, observed_at
FROM activity_history a
WHERE a.id = (
    SELECT b.id FROM activity_history b
    WHERE b.peer_id = a.peer_id AND b.field = a.field
    ORDER BY b.observed_at DESC, b.id DESC
    LIMIT 1)
```

Preload endi `observed_at` ni ham o'qib, keshga yozishi kerak.

### `GetLatestActivityHistoryValue` imzosi

Chaqiruvchilar faqat qiymatni ishlatadi, shuning uchun imzo
o'zgarmasligi mumkin — ichkarida `slot.value` qaytariladi.

## 4. Ta'sir doirasi

`GetLatestActivityHistoryValue` ni `RecordField` ham ishlatadi
(`hadPrevious` ni aniqlash uchun). Kesh eski bo'lsa, u "oldingi
qiymat yo'q" deb noto'g'ri xulosa chiqarib, keraksiz "kuzatish
boshlandi" yozuvi qo'shishi mumkin — ekran suratidagi
"(kuzatish boshlandi)" yozuvi ehtimol aynan shundan.

Ya'ni bu nuqson faqat ko'rsatishni emas, **yozishni ham** buzadi.

## 5. Sinov

Tuzatishdan keyin bazadan tekshirish:

```sql
-- har peer uchun kesh nima ko'rsatishi kerak
SELECT peer_id, new_value, datetime(observed_at,'unixepoch','+5 hours')
FROM activity_history a
WHERE field='status' AND a.id = (
  SELECT b.id FROM activity_history b
  WHERE b.peer_id=a.peer_id AND b.field='status'
  ORDER BY b.observed_at DESC, b.id DESC LIMIT 1);
```

Oynadagi "Joriy holat" shu qiymatga MOS kelishi kerak.
Sxema o'zgarishi **kerak emas** — faqat kod.

---

## 6. Qo'shimcha topilma (2026-08-31): "Joriy holat" yorlig'i noto'g'ri

Oynadagi tushuntirish matni shunday deydi:

> "...yuqoridagi 'Joriy holat' esa Telegram'dan olingan alohida qiymat"

Aslida u **bazadan** (`GetLatestActivityHistoryValue`) o'qiladi.
Dalil: `Guli` (5882234960) uchun oyna 30.08 19:01 ko'rsatgan, Telegram
profilida esa "last seen recently" turgan.

**QAROR (2026-08-31, foydalanuvchi aniqlashtirgandan keyin): B varianti**

Foydalanuvchi story nuqtasi "Joriy holat" ga ham tushishini kutadi —
ya'ni bu qator BYPASS natijasini ko'rsatishi kerak, Telegram'ning
o'z qiymatini emas.

Sabab: Telegram'ning qiymati profilda allaqachon ko'rinadi
("last seen recently"). Bu oynaning butun qiymati — Telegram
yashirgan narsani ko'rsatishda. Agar "Joriy holat" ham "recently"
desa, qator befoyda bo'lib qoladi.

**Bajariladi:**

1. Matn "Joriy holat" → **"Eng so'nggi aniqlangan holat"**
2. Tushuntirishdagi "Telegram'dan olingan alohida qiymat" jumlasi
   olib tashlanadi — u yolg'on
3. 2-5 bo'limlardagi kesh tuzatishi (vaqt bo'yicha) — shundan keyin
   story nuqtasi eng yangi bo'lgani uchun o'z-o'zidan bu qatorga
   chiqadi

**Rad etilgan variant (A):** jonli `user->lastseen()` dan olish —
Telegram bilan mos bo'lardi, lekin bypass natijasini yashirardi.
