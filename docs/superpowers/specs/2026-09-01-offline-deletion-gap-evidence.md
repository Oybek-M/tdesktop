# Oflayn o'chirish bo'shlig'i — amaliy dalil

**Sana:** 2026-09-01
**Turi:** tashxis hujjati (kod o'zgarishi YO'Q)
**Ahamiyati:** `customsync-server` plan 05 (always-on capture) uchun
birinchi HAQIQIY dalil

---

## 1. Hodisa

Foydalanuvchi `Zulayxo | A | 2` (`8560257649`) chatidagi o'chirilgan
xabarlarni NovaGram (telefon) da to'liq ko'rdi, lekin CustomMod
(desktop) da faqat bir qismi saqlanib qolgan edi.

NovaGram ekran suratlaridan 13 ta xabar aniqlandi. Bazadagi holat:

| Sana | NovaGram'da | CustomMod'da saqlangan |
|---|---|---|
| 25-avgust | 7 ta | **1 ta** |
| 30-avgust | 6 ta | **6 ta** ✅ |

## 2. Tekshirilgan va RAD ETILGAN uchta nazariya

### 2.1 "AntiDelete o'chiq edi"

Foydalanuvchi 2026-09-01 da Custom Window'ni ochganda AntiDelete va
AntiEdit **o'chiq** turganini ko'rdi. Tabiiy taxmin — sozlama
o'chiq bo'lgani uchun saqlanmagan.

**Rad etildi.** Kod to'g'ri:

- `custom_settings.h:23` — `bool antiDelete = true` (standart YOQIQ)
- `custom_settings.cpp:258` — `settings.value("antiDelete", true)`
- `custom_tab_privacy.cpp` — tugmada `rpl::skip(1)` bor, ya'ni
  boshlang'ich qiymat registrga qayta yozilmaydi
- `Set()` → `QSettings::setValue` — yozish yo'li ham benuqson

Bundan tashqari 30-avgust xabarlari **to'liq** saqlangan — demak
o'sha paytda AntiDelete yoqiq edi.

> ⚠️ Sozlamaning qachon va nima uchun `false` bo'lib qolgani ALOHIDA
> savol bo'lib qolmoqda. Kodda sabab topilmadi. Kuzatib borish kerak.

### 2.2 "Akkaunt mos kelmagan" (v10 izolyatsiyasi)

**Rad etildi va teskarisi chiqdi.** `text_cache` da yo'qolgan
xabarlarning HAR BIRI uchun **ikkita** qator bor edi
(`account_id = 0` va `account_id = 1474449522`), ya'ni
`account_id IN (0, ?)` filtri ularni albatta topardi.

Aksincha, faqat `account_id = 0` bo'lgan `396096` **saqlangan**.

### 2.3 "`msg_date = 0` bo'lgan"

`data_session.cpp:3457` da darvoza bor: `if (msgDate > 0)` — sana
aniqlanmasa yozuv **jimgina tashlab yuboriladi**. Bu jiddiy nomzod
edi.

**Rad etildi.** Yo'qolgan oltala xabarning ham `text_cache` da
to'g'ri `msg_date` qiymati bor:

```
396090  1787626328    396093  1787626731
396091  1787626391    396094  1787626869
396092  1787626703    396095  1787626973
```

## 3. Haqiqiy sabab

Yo'qolgan xabarlarning **matni, sanasi va jo'natuvchisi**
`text_cache` da to'liq turgan edi. Ya'ni **arxivlash qismimiz
benuqson ishlagan**.

Lekin `actioned_messages` da ular uchun `deleted` yozuvi yo'q, va
mavjud barcha `deleted` yozuvlar **bitta lahzada** yaratilgan:

```
2026-08-30T21:51:37  →  396938
2026-08-30T21:51:46  →  396096, 396931, 396934, 396935, 396936, 396937
```

**Xulosa:** o'sha lahzada Telegram klientimizga faqat 7 ta ID uchun
`updateDeleteMessages` yubordi. Qolgan 6 tasi uchun o'chirish
hodisasi **umuman yetib kelmadi**.

Sabab: ular ilova **oflayn turgan paytda** o'chirilgan. Telegram
oflayn davrdagi individual o'chirishlarni qayta yubormaydi —
qayta ulanganda `updates.getDifference` faqat YAKUNIY holatni
sinxronlaydi, har bir o'chirish uchun alohida update bermaydi.

> **Bu bizning kodimizdagi xato EMAS.** Bu klient-asosli
> yondashuvning tub cheklovi: ilova yopiq bo'lsa, o'chirish
> hodisasini hech qanday mahalliy mantiq ushlab qololmaydi.

## 4. Nima uchun bu plan 05 uchun muhim

`plans/2026-07-29-multi-device-sync-05-capture-service.md` — VPS'da
24/7 ishlaydigan TDLib xizmati. Uning maqsadi so'zma-so'z:
"o'chirilgan va tahrirlangan xabarlarni **ilova yopiq bo'lganda ham**
ushlab qolish".

Bugungi hodisa shu ehtiyojning **birinchi hujjatlashtirilgan
amaliy dalili**:

- Desktop klient oflayn edi → 6 ta xabar yo'qoldi
- NovaGram (telefonda, doimiy ulangan) → hammasini ushlab qoldi

Ya'ni muammo nazariy emas. Doimiy ulangan bitta manba bo'lsa,
bu 6 ta xabar ham saqlanib qolardi.

## 5. Qilingan ish

13 ta xabar NovaGram ekran suratlaridan bazaga qo'lda import
qilindi:

- `msg_id` lar mavjud yozuvlar orasidagi bo'sh joylarga qo'yildi
  (396090, 396091, 396093, 396095 — 396092/396094/396096 haqiqiy)
- `msg_date` avval rasmdagi vaqtdan olingan, so'ng `text_cache`
  dagi **haqiqiy** qiymatlarga to'g'irlandi (8–50 soniya farq bor edi)
- `notes = 'NovaGram screenshot import 2026-09-01'` — kelajakda
  ajratish uchun
- Zaxira: `actioned_messages.db.zulayxo-import-20260901-020542.bak`

## 6. Ochiq savol

**AntiDelete sozlamasi qachon va nega `false` bo'lib qolgan?**

Kodda sabab topilmadi (2.1 ga qarang). Ehtimollar:

1. Foydalanuvchi o'zi o'chirgan va esidan chiqqan
2. Registrga tegadigan boshqa yo'l bor (topilmadi)
3. Bir vaqtlar eski `anti_delete` (snake_case) kaliti bilan
   bog'liq migratsiya qoldig'i

Hozircha kuzatib borish kerak: agar sozlama yana o'z-o'zidan
o'chsa — bu haqiqiy nuqson va alohida tashxis talab qiladi.
