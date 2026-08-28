# 7.4 O'TMADI — eski (`account_id = 0`) arvohlar hamon ko'rinmoqda

**Sana:** 2026-08-27
**Holat:** ✅ YOPILDI (2026-08-28) — pastdagi 6-bo'limga qarang
**Bog'liq:** `2026-08-26-multi-account-db-isolation-design.md`,
`2026-08-26-account-isolation-v10-plan.md` (Vazifa 7.4)

---

## 1. Qo'lda sinov natijasi

| Qadam | Natija |
|---|---|
| 7.4 — Akam chatida yangi arvoh chiqmasligi | 🔴 **O'TMADI** — arvohlar hamon ko'rinmoqda |
| 7.5 — Faollik tarixi ikkala akkauntda bir xil to'liq | ✅ **O'TDI** — ma'lumotlar qaytdi |

Ya'ni akkauntlar ajratmasining **yozish** tomoni ishlaydi, **ko'rsatish**
tomoni esa eski yozuvlarni hamon filtrlay olmayapti.

## 2. Bazadagi holat (2026-08-27, migratsiyadan keyin)

```
type='deleted' account_id bo'yicha:  { 0: 2490 }      <- HAMMASI 0
eng ko'p peer'lar (account_id=0):
    7053823996  614
    7815174989  454
    7815103103  247
    1334067829  218
    8720525440  160
mazmuni butunlay bo'sh (matn ham, media ham yo'q):  817
```

**Muhim:** `type='deleted'` yozuvlarning **100 %** i `account_id = 0`.
Bu kutilgan holat — `account_id` retroaktiv aniqlanmaydi, migratsiya
eski qatorlarni 0 deb qoldiradi. Demak yangi filtr ularga **umuman
ta'sir qilmaydi**: o'qish so'rovlari `account_id IN (0, ?)` bo'lgani
uchun 0 li qatorlar HAR BIR akkauntda ko'rinaveradi.

## 3. Nima uchun ID-diapazon evristikasi yetarli bo'lmadi

`history.cpp` dagi Vazifa 4 evristikasi `account_id == 0` yozuvni
faqat shu shartda yashiradi:

```
msg_id < minReal  VA  date > minRealDate
```

ya'ni "ID kichik, lekin sanasi yangi" — boshqa akkauntdan kelgan
yozuvning klassik izi. Bu shart juda TOR:

- Agar chat yangi ochilgan bo'lsa `minReal` juda kichik bo'ladi va
  hech bir arvoh shartga tushmaydi.
- `date` (`msg_date`) eski yozuvlarda 0 bo'lishi mumkin — u holda
  `date > minRealDate` hech qachon bajarilmaydi.
- Bir xil odam bilan IKKALA akkauntda yozishilgan bo'lsa ID'lar
  o'zaro qoplanadi va evristika ajrata olmaydi.

## 4. Ko'rib chiqish uchun yondashuvlar (hali TANLANMAGAN)

Implement qilishdan OLDIN foydalanuvchi bilan kelishilsin.

1. **Bo'sh mazmunli yozuvlarni umuman ko'rsatmaslik.**
   817 ta yozuvda na matn, na media bor — ular foydalanuvchiga hech
   qanday ma'lumot bermaydi, faqat shovqin. Eng arzon va eng katta
   samarali qadam. (Hozir `loadDeletedMessages()` da shunga o'xshash
   tekshiruv bor, lekin `isMedia` bayrog'i yoki `mediaPath` bo'lsa
   o'tkazib yuboradi — fayl DISKDA bor-yo'qligi tekshirilmaydi.)

2. **Bir martalik "egasini aniqlash" migratsiyasi.**
   Har bir `account_id = 0` yozuv uchun: o'sha `peer_id` bilan shu
   akkauntda REAL suhbat bormi (dialoglar ro'yxatida)? Yo'q bo'lsa —
   yozuv boshqa akkauntniki. Sekin, lekin bir marta ishlaydi.

3. **Foydalanuvchiga tanlov berish.**
   Custom Window ichida "eski, egasi noma'lum yozuvlarni ko'rsatish"
   bayrog'i (standart: O'CHIQ). Eng xavfsiz variant — hech narsa
   yo'qolmaydi, lekin ko'z oldida turmaydi.

4. **Karantin jadvali.** 2026-08-26 da 794 qator uchun qilinganidek:
   shubhali yozuvlarni alohida jadvalga ko'chirish. Qaytarib olish
   mumkin, chatda ko'rinmaydi.

## 5. Keyingi qadam

Foydalanuvchi 1–4 dan qaysi biri (yoki qanday kombinatsiya)
kerakligini aytgach implement qilinadi. **Ruxsatsiz boshlanmasin.**


---

## 6. YECHIM (2026-08-28) — ✅ tasdiqlangan

Ikki qatlamli tuzatish `history.cpp::loadDeletedMessages()` da.
Ikkalasi ham FAQAT ko'rsatish bosqichida — bazaga yozilmaydi,
yashirilgan yozuvlar butunligicha qoladi.

### 6.1 Mediasiz "(media xabar)" yozuvlari

**Ildiz sabab:** media yo'li ko'rsatish qaroridan KEYIN hal qilinardi.
Shart `msg.isMedia || !mediaPath.isEmpty()` bo'lgani uchun `is_media=1`
bayrog'i yolg'iz o'zi yetardi — fayl umuman bo'lmasa ham yozuv
chizilaverardi.

**Tuzatish:** tartib teskari qilindi. Yozuv ko'rsatiladi faqat matn
bor YOKI media fayli HAQIQATAN diskda mavjud bo'lsa.

**Ta'sir:** 2490 -> 1670 (820 yozuv yashirildi).

### 6.2 Monotonlik qoidasi (begona akkaunt yozuvlari)

Bitta akkaunt ichida `msg_id` va `msg_date` birga o'sadi. Langarlar
shu akkauntning HAQIQIY server xabarlaridan olinadi
(`isRegular() && !isLocal()`), demak ta'rifan ishonchli.

Noto'g'ri yashirishga qarshi 4 himoya (har qanday shubhada
"ko'rsatiladi"):

1. Kamida 5 ta langar
2. `msg_date > 0`
3. Sana langarlar oralig'ida bo'lsin
4. Chetlanish >= 4x oraliq, minimum 1000 ID

### Natija (qo'lda sinov, 2026-08-28)

| | Oldin | Keyin |
|---|---|---|
| Akam chati | 218 | **15** |

Foydalanuvchi tasdig'i: "arvohlar yo'qoldi, faqat 15 ta haqiqiy yozuv
qoldi". Bashorat 13 ta haqiqiy edi — ya'ni noto'g'ri yashirish
kuzatilmadi, begonalarning deyarli hammasi tutildi.

**Shu bilan v10 (Vazifa 7.4) ham yopiladi.**
