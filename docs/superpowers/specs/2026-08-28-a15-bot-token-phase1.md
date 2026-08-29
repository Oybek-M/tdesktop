# A15 — bot token orqali kirish, 1-BOSQICH

**Sana:** 2026-08-28
**Holat:** 🟡 Kod tayyor, build va SINOV kutilmoqda
**Bog'liq:** `docs/superpowers/PROJECTS.md` → A15

---

## 1. Bu bosqichning maqsadi

Bu **to'liq funksiya emas, balki tekshiruv (probe)**.

MTProto darajasida botlar user-only metodlarga
(`messages.getDialogs`, `contacts.getContacts`,
`account.updateStatus`) `BOT_METHOD_INVALID` yoki
`USER_BOT_INVALID` qaytarishi **kutilmoqda**. Agar shunday bo'lsa,
"bot token bilan to'liq klient" g'oyasi amalga oshmaydi — chat
ro'yxati bo'sh qoladi.

1-bosqich shu taxminni **haqiqiy token bilan** tekshiradi:

- login ekrani ishlaydi,
- user-only so'rovlar xatosi yutiladi (ilova qulamaydi, cheksiz
  qayta so'rov tsikliga tushmaydi),
- har bir so'rov natijasi `[BOTPROBE]` prefiksi bilan logga yoziladi.

2-bosqich (bot konsoli UI'si yoki g'oyani yopish) **shu sinov
natijasiga qarab** rejalashtiriladi.

## 2. O'zgargan fayllar

| Fayl | Holat | Nima qilingan |
|---|---|---|
| `Telegram/SourceFiles/intro/intro_bot_token.h` | yangi | `BotTokenWidget` deklaratsiyasi |
| `Telegram/SourceFiles/intro/intro_bot_token.cpp` | yangi | Token maydoni, ogohlantirish, `auth.importBotAuthorization` so'rovi, `finish()` |
| `Telegram/SourceFiles/intro/intro_qr.{h,cpp}` | o'zgardi | QR ekraniga "Bot token orqali kirish" havolasi |
| `Telegram/CMakeLists.txt` | o'zgardi | Yangi fayllar ro'yxatga qo'shildi |
| `Telegram/SourceFiles/apiwrap.cpp` | o'zgardi | `requestContacts()` va `requestMoreDialogs()` — probe logi + xato tsiklini to'xtatish |
| `Telegram/SourceFiles/api/api_updates.cpp` | o'zgardi | `account.updateStatus` — probe logi |

### Xato tsiklini to'xtatish

- `requestContacts()`: bot xatosida `contactsLoaded() = true` —
  kontaktlar hech qachon kelmasligi ma'lum, qayta so'ralmaydi.
- `requestMoreDialogs()`: bot xatosida `state->listReceived = true`
  va `dialogsLoadFinish(folder)` — aks holda ilova cheksiz
  `messages.getDialogs` yuborardi.

Probe loglari `isBot()` sharti bilan o'ralgan, ya'ni oddiy
foydalanuvchi akkauntida logga hech narsa yozilmaydi.

## 3. Sinov yo'riqnomasi

1. [@BotFather](https://t.me/BotFather) dan bot token oling
   (`/newbot` yoki mavjud botdan `/token`).
2. Ilovada yangi akkaunt qo'shing → QR ekrani ochiladi →
   pastdagi **"Bot token orqali kirish"** havolasini bosing.
3. Tokenni kiriting va davom eting.
4. Kirish muvaffaqiyatli bo'lsa — chat ro'yxati **bo'sh** bo'lishi
   kutiladi. Bu nuqson emas, aynan tekshirilayotgan narsa.
5. Log faylini oching:

   ```
   C:\TBuild\tdesktop\out\Release\log.txt
   ```

   (Ilova portativ rejimda ishlaydi — log `tdata` papkasi yonida,
   ya'ni `Telegram.exe` turgan papkada joylashadi. Manba:
   `logs.cpp` → `_logsFilePath()` → `cWorkingDir()`.)

6. `BOTPROBE` so'zini qidiring va **har bir qatorni** qayd eting.
   Kutilayotgan uch qator:

   ```
   [BOTPROBE] contacts.getContacts  -> OK yoki FAIL: <xato_turi>
   [BOTPROBE] messages.getDialogs   -> OK yoki FAIL: <xato_turi>
   [BOTPROBE] account.updateStatus  -> OK yoki FAIL: <xato_turi>
   ```

### Natijani qanday o'qish kerak

| `messages.getDialogs` | Xulosa |
|---|---|
| `FAIL: BOT_METHOD_INVALID` | Kutilgan holat. To'liq klient **mumkin emas** → 2-bosqich = bot konsoli yoki g'oyani yopish |
| `OK` | Kutilmagan va yaxshi xabar. To'liq klient yo'nalishi ochiq qoladi |
| Boshqa xato | Alohida tahlil kerak — xato turini to'liq yozib qo'ying |

Agar login **umuman** o'tmasa (ya'ni `auth.importBotAuthorization`
xato bersa), ekranda xato turi ko'rsatiladi — o'shani ham qayd eting.

---

## 4. SINOV NATIJASI (2026-08-29) — HAL QILUVCHI

Haqiqiy bot token bilan kirildi. `log.txt` dan:

```
[2026.08.29 16:12:28] [BOTPROBE] messages.getDialogs -> FAIL: BOT_METHOD_INVALID
[2026.08.29 16:12:28] RPC Error: request 1389 got fail with code 400, error BOT_METHOD_INVALID
[2026.08.29 16:12:28] [BOTPROBE] account.updateStatus -> FAIL: BOT_METHOD_INVALID
```

### Xulosa: "bot token bilan to'liq klient" MUMKIN EMAS

Taxmin tasdiqlandi. Server bot akkauntga `messages.getDialogs` ni
bermaydi — bot uchun "dialoglar ro'yxati" tushunchasi umuman yo'q.
Login ishlaydi, lekin ilova doimiy "Loading..." holatida qoladi va
chat ro'yxati hech qachon to'lmaydi.

`account.updateStatus` ham rad etiladi — ya'ni Ghost Mode kabi
funksiyalar bot akkauntda ma'nosiz.

### 1-bosqichning qiymati

Kod behuda ketmadi: `BOTPROBE` diagnostikasi va xato tsiklini
to'xtatish mantig'i savolni **taxmin bilan emas, dalil bilan**
yopdi. 250 satr evaziga noto'g'ri yo'nalishga oylab ketmaslik.

### Qaror kerak (foydalanuvchi hal qiladi)

| Variant | Izoh |
|---|---|
| **A. Yopish** | Login ekrani va probe kodi olib tashlanadi. Eng toza. |
| **B. Saqlab qo'yish** | Kod qoladi, lekin ekranda ochiq ogohlantirish: "bot rejimi to'liq ishlamaydi". Kelajakda bot konsoli qurilsa asos bo'ladi. |
| **C. Bot konsoli** | Chat ro'yxati o'rniga boshqa UI: `messages.getMessages` (ID bo'yicha xabar) + `updates` orqali real vaqtda kelgan xabarlar. Bu **yangi, alohida loyiha**, kichik ish emas. |

Hozircha hech biri tanlanmagan.
