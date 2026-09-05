# A21 — Soxta "o'chirilgan" yozuvlari (partiyali false-positive)

**Holat:** 🔴 OCHIQ — ildiz sabab topilmagan, kod yozilmagan
**Aniqlangan:** 2026-09-05, foydalanuvchi hisoboti + baza tahlili
**Xavf darajasi:** yuqori — AntiDelete'ning ishonchliligini yo'q qiladi

---

## 1. Kuzatilgan holat

Foydalanuvchi "Akam" chatini (peer `1334067829`) ochganda `[DELETED] — saved
by Anti-Delete` belgilari **hech qachon o'chirilmagan** xabarlarda chiqdi.
Xabarlar 2026-08-29, 08-30 va 09-02 sanalariga tegishli; foydalanuvchi bu
davrda o'sha chatda hech nima o'chirmagan va suhbatdosh ham o'chirmagan.

Foydalanuvchi buni "oxirgi build'dan keyin paydo bo'ldi" deb qabul qildi.

## 2. Nima aniqlandi (dalillar)

Baza: `<ArchiveRoot>/db/actioned_messages.db`, read-only so'rovlar.

### 2.1. Yozuvlar build'dan OLDIN yozilgan

`actioned_messages` da peer `1334067829` uchun eng yangi `timestamp` —
**2026-09-03T08:50:14**. Bugungi build (2026-09-05 16:30) hech qanday yangi
yozuv yaratmagan. Ya'ni build sababchi EMAS; o'zgargan narsa —
**ko'rinuvchanlik**, yozuv emas.

### 2.2. Yozuvlar PARTIYALAB yaratilgan — asosiy dalil

Bir daqiqada yozilgan `type='deleted'` yozuvlari:

| `timestamp` | Yozuv | Chat | Xabar sanalari |
|---|---|---|---|
| 2026-08-23T13:32 | **781** | **35** | — |
| 2026-07-16T21:36 | 378 | 2 | 2026-07-16 |
| 2026-08-23T23:12 | 152 | 1 | — |
| 2026-08-11T14:01 | 105 | 1 | — |
| 2026-09-02T13:01 | 11 | 1 | 2026-08-29 .. 08-31 |

**35 ta chatda bir daqiqada 781 ta xabar o'chirilishi mumkin emas.** Bu
global hodisa (ilova ishga tushishi, akkaunt almashish, qayta login,
tarix qayta yuklanishi) paytida ishlaydigan soxta aniqlash.

Ikkinchi belgi: `2026-09-02T13:01` da yozilgan 11 ta yozuv **08-29..08-31**
sanali xabarlarga tegishli — ya'ni o'sha paytda kelgan yangi xabarlar emas,
balki allaqachon mavjud eski xabarlar to'satdan "o'chirilgan" deb
belgilangan.

### 2.3. Bir xil xabar ham `backup`, ham `deleted`

Peer `1334067829` da **3 ta** `msg_id` ikkala turda ham bor. Masalan
`msg_id=396918` ("okk"):

```
backup   msg_date=2026-08-30 17:56   yozilgan 2026-08-30T18:02
deleted  msg_date=2026-08-30 17:56   yozilgan 2026-08-31T10:03
```

Xabar arxivlangan, keyin ertasi kuni "o'chirilgan" deb belgilangan.
Ekranda esa u hamon turibdi — demak o'chirilmagan.

### 2.4. Nisbat mantiqsiz

Peer `1334067829`: `backup` 242, `deleted` **258**.
Butun baza: `backup` 36 229, `deleted` 2 874.

Bitta chatda xabarlarning yarmidan ko'pi o'chirilgan bo'lishi haqiqatga
to'g'ri kelmaydi. Boshqa chatlarda nisbat ~8% — demak muammo hamma joyda
emas, ma'lum sharoitda yuzaga keladi.

### 2.5. TEKSHIRILDI VA RAD ETILDI: begona `msg_id`

Dastlab `msg_id` 5380..15109 kabi kichik qiymatlar boshqa chatdan
adashib kelgan deb gumon qilindi. Rad etildi: peer'da `msg_id` uzluksiz
`5380..397135` oralig'ida — bu shunchaki uzun tarix, xato emas.

## 3. Gipotezalar (tekshirilmagan)

**G1 — "slice'da yo'q" ni "o'chirilgan" deb hisoblash.** Eng ehtimoliy.
Tarix qayta yuklanganda yoki keshdan chiqarilganda xabar xotirada
qolmaydi; agar aniqlash mantig'i "ilgari ko'rgan, hozir yo'q" tamoyiliga
tayansa, butun bir slice soxta o'chirilgan deb belgilanadi. 2026-08-23T13:32
dagi 35 chatlik partiya aynan shunga o'xshaydi.

**G2 — akkaunt izolyatsiyasi bilan bog'liq.** v10 da `account_id`
qo'shilgan. Agar aniqlash `account_id` bo'yicha filtrlanmagan ro'yxat bilan
solishtirsa, boshqa akkauntda ochilgan chat "yo'qolgan" ko'rinadi.
2026-08-23 sanasi v10 ishlari bilan yaqin — tekshirishga arziydi.

**G3 — ko'rinuvchanlik alohida masala.** Yozuvlar 09-02/09-03 da yaratilgan,
lekin foydalanuvchi ularni bugun ko'rdi. Nima uchun bugun ko'rindi —
alohida savol (chat birinchi marta ochildimi, yoki `loadDeletedMessages()`
chaqirilish sharti o'zgardimi).

## 4. Keyingi qadamlar

1. `type='deleted'` YOZUVCHI barcha nuqtalarni topish
   (`grep -rn "u\"deleted\"_q\|'deleted'" Telegram/SourceFiles/`), va har
   biri qanday shart bilan ishga tushishini aniqlash.
2. Aniqlash mantig'i "xabar xotirada yo'q" ga tayanadimi, yoki serverdan
   kelgan `updateDeleteMessages` ga tayanadimi — shuni ajratish.
   Faqat ikkinchisi ishonchli.
3. G2 uchun: 2026-08-23T13:32 partiyasidagi 35 chatni `account_id` bo'yicha
   guruhlash — hammasi bitta akkauntga tegishlimi?
4. Ildiz sabab topilgach: mavjud soxta yozuvlarni tozalash so'rovi
   (avval `.bak` olinadi — bugungi
   `actioned_messages.db.premigrate-v15-manual-20260905-161948.bak` bor).

## 5. Muhim eslatma

Tozalashdan OLDIN ildiz sabab topilishi shart. Aks holda soxta yozuvlar
qaytadan paydo bo'ladi, va tozalash chinakam o'chirilgan xabarlarni ham
yo'q qilib yuborishi mumkin — ular ayni shu jadvalda saqlanadi.
