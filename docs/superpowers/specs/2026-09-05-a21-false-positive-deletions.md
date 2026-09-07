# A21 — Soxta "o'chirilgan" yozuvlari (partiyali false-positive)

**Holat:** 🟡 ILDIZ SABAB TOPILDI — tuzatish yo'nalishi bo'yicha qaror kutilmoqda
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

## 3. ILDIZ SABAB (Phase 1 yakunlandi, 2026-09-05)

Ikkita mustaqil mexanizm. Ikkalasi ham haqiqiy.

### 3.1. 🔴 Asosiy: legacy `account_id = 0` barcha akkauntga ko'rinadi

Barcha o'qish so'rovlari `account_id IN (0, ?)` filtridan foydalanadi
(`custom_db.cpp:158` `kAccountFilterSql`, **20 ta joyda**; chatga
kiritish so'rovi — `custom_db.cpp:813`). Nol "noma'lum akkaunt" degani
va **har qanday akkauntga mos keladi**.

Bazada `account_id = 0` bo'lgan **26 753** yozuv bor (jamining ~68%) —
ular v10 (2026-08-28, akkaunt izolyatsiyasi) dan oldin yozilgan.

Foydalanuvchida **8+ akkaunt** bor. `actioned_messages` dagi
`account_id` qiymatlari: `1474449522` (8636), `1334067829` (2533),
`7815174989` (1816), `5451532124` (638), `6700270485` (609),
`6059877171` (594), `6607303857` (357), `6532114959` (66), va h.k.

Peer `1334067829` uchun taqsimot:

| `account_id` | `backup` | `deleted` |
|---|---|---|
| **0 (legacy)** | 228 | **217** |
| 1474449522 | 14 | 10 |
| 5451532124 | — | 11 |
| 1334067829 | — | 11 |
| 7815174989 | — | 8 |
| 6700270485 | — | 1 |

Ya'ni chat qaysi akkauntda ochilishidan qat'i nazar **217 ta legacy
"o'chirilgan" yozuvi kiritiladi** — ular boshqa akkauntda ishlaganda
yozilgan bo'lsa ham. Foydalanuvchi "men buni o'chirmaganman" deydi,
chunki u yozuvni **boshqa akkaunt ostida** ko'rmoqda.

`ORDER BY` msg_date bo'yicha ketgani uchun ular chatning o'z joyiga —
avgust oxiri / sentyabr boshiga — tarqalib tushadi va "yangi paydo
bo'lgan" taassurot beradi.

### 3.2. 🟡 Ikkilamchi: AntiDelete o'chirishni ko'rinmas qiladi

`data_session.cpp:3538` — yozuvdan keyin `item->setDeletedLocally()`
chaqiriladi va xabar **ekranda qoladi**. Ya'ni suhbatdosh haqiqatan
o'chirgan xabar ham "o'chirilmagandek" ko'rinadi.

Shu sababli `msg_id=396918` ("okk") bazada ikki marta turishi —
`backup` (08-30 da kelganda) va `deleted` (08-31 da o'chirilganda) —
**xato emas, kutilgan hayotiy sikl**. Foydalanuvchi uni ekranda ko'rib
"o'chirilmagan" deb hisoblaydi.

Demak "781 yozuv / 35 chat" partiyasining bir qismi haqiqiy: uzoq
oflayn turgandan keyin ulanishda server to'plangan o'chirishlarni
bitta paketda yuboradi. Xabar sanalari `2026-07-24 .. 2026-08-22` —
bir oylik oraliq — aynan shunga mos.

### 3.3. Rad etilgan gipotezalar

| # | Gipoteza | Nega rad etildi |
|---|---|---|
| G1 | "slice'da yo'q" ni "o'chirilgan" deb hisoblash | Yozuv nuqtalari faqat ikkita: `data_session.cpp:3529` va `:3414`. **Ikkalasi ham serverning `updateDeleteMessages` signaliga tayanadi.** Xotira holatiga tayanadigan evristika kodda yo'q. |
| G2 | `account_id` peer id bilan chalkashgan | `custom_peer_key.cpp:19` — `accountId = session.userId().bare`. To'g'ri. `1334067829` ham akkaunt, ham peer sifatida uchraydi, chunki u foydalanuvchining akkauntlaridan biri VA boshqa akkauntdagi suhbatdosh. Chalkashish yo'q. |
| — | Begona `msg_id` oralig'i | `msg_id` uzluksiz `5380..397135` — uzun tarix, xato emas. |

## 4. Tuzatish yo'nalishi (hali kelishilmagan)

**Muammo:** `account_id = 0` ni "hamma akkaunt" deb talqin qilish
26 753 ta legacy yozuvni akkauntlar orasida oqizadi. Lekin uni
shunchaki olib tashlash ham xato — o'shanda legacy yozuvlar **hech
qaysi** akkauntda ko'rinmay qoladi va AntiDelete tarixi yo'qoladi.

Ko'rib chiqilishi kerak bo'lgan variantlar:

1. **Legacy yozuvlarga egalik biriktirish** — har bir `account_id=0`
   yozuvi uchun `peer_id` qaysi akkauntda mavjudligiga qarab egasini
   aniqlash. Bir nechta akkauntda bo'lsa — noaniq, hal qilish kerak.
2. **`account_id=0` ni faqat bitta "asosiy" akkauntga bog'lash.**
   Sodda, lekin boshqa akkauntlarning legacy tarixini yo'qotadi.
3. **UI'da ko'rsatish** — yozuv qaysi akkaunt ostida qayd etilganini
   belgilash, filtrlamasdan. Eng xavfsiz, ma'lumot yo'qolmaydi.

Qaror foydalanuvchiniki. Har uch variant migratsiya talab qiladi.

## 5. Muhim eslatma

Tozalash yoki migratsiya OLDIDAN backup shart. Bugungi nusxa mavjud:
`actioned_messages.db.premigrate-v15-manual-20260905-161948.bak`
(59.8 MB, `integrity ok`).

Soxta deb ko'ringan yozuvlarni ko'r-ko'rona o'chirish **mumkin emas** —
ular orasida chinakam o'chirilgan xabarlar bor (§3.2), va ular ayni
shu jadvalda saqlanadi.
