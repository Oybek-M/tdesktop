# A16 — faollik kuzatuvidagi uchta bo'shliq

**Sana:** 2026-08-28
**So'ragan:** foydalanuvchi (aniq hodisa asosida, pastda)
**Holat:** 🔴 OCHIQ — tez implement qilinishi kerak
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

### Ko'rib chiqish uchun variantlar (TANLANMAGAN — user hal qiladi)

1. **Chat ro'yxatida tez tugma** — o'ng tugma → "Faollikni kuzatish".
   Eng arzon, lekin baribir bir necha soniya ketadi.
2. **Qisqa muddatli bufer** — BARCHA foydalanuvchilar uchun oxirgi
   N soatlik status o'zgarishlari xotirada saqlanadi; odam ro'yxatga
   qo'shilganda bufer bazaga ko'chiriladi. "Kech qo'shsam ham
   yo'qolmaydi" degani. Xotira sarfi baholanishi kerak.
3. **Hammasini yozish** — `ShouldTrackActivity()` darvozasi olib
   tashlanadi, ro'yxat faqat KO'RSATISHNI filtrlaydi. Eng to'liq,
   lekin baza tez o'sadi (hozir 30 kunlik saqlash bilan ~260k yozuv).

Variant 2 ehtimol eng mos: yo'qotishni bartaraf qiladi, baza
o'sishini esa cheklab turadi.

---

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
