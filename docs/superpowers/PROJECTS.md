# Loyiha yo'nalishlari — holat jadvali

Bu fayl qaysi ish **hozir faol**, qaysi biri **to'xtatib qo'yilgan** va
qaysi biri **hali muhokama bosqichida** ekanini ko'rsatadi. Yangi
sessiya boshlanganda birinchi shu yerga qarang.

Oxirgi yangilanish: 2026-08-01

---

## A — tdesktop CustomMod ⚡ FAOL

Hozirgi asosiy ish. Boshqa yo'nalishlar bunga aralashmasligi kerak.

**Repo:** `Telegram/tdesktop`, branch `Oybek`
**Holat:** **v7.0.7** ga sync qilingan, build o'tgan, qo'lda sinovdan o'tgan

**Ochiq vazifalar:**

| № | Vazifa | Holat |
|---|---|---|
| A1 | To'liq qo'lda tekshiruv | ✅ 2026-08-01, kritik muammo yo'q |
| A2 | Upstream v7.0.5 → v7.0.7 sync | ✅ 2026-08-01, `2e61fdcbc2` (162 commit, 2 konflikt) |
| A3 | Log shovqinini kamaytirish (`API Warning: not loaded minimal channel applied.`) | Past ustuvorlik, tekshirilmagan |

**v7.0.7 bilan kelgan yangi bog'liqliklar:** `Telegram/ThirdParty/libcbor`
va `libfido2` (FIDO2/passkey). Build muammosiz o'tdi.

---

## B — Self-update mexanizmi ⚡ KEYINGI ISH

Har rebuild'dan keyin ilovani fleshka/cloud orqali qo'lda tarqatish
muammosini hal qiladi. Bir nechta desktop qurilma va boshqa
foydalanuvchilar (masalan aka) paydo bo'lganda bu muammo keskinlashadi.

**Holat:** tahlil qilingan, spec yozilmagan. Foydalanuvchi ruxsatini
kutmoqda.

**Asosiy topilma:** tdesktop'ning **butun updater mexanizmi allaqachon
kodda mavjud** — u shunchaki `DESKTOP_APP_DISABLE_AUTOUPDATE=ON` bilan
o'chirilgan. Noldan yozish kerak emas.

**Muhim xavf:** updater'ni yoqishdan **oldin** URL va public key
almashtirilishi SHART. Aks holda "yangilanishni tekshirish" rasmiy
Telegram build'ini yuklab olib, bizning fork'ni **butunlay bosib
ketadi** va barcha customization yo'qoladi.

**A yo'nalishiga bog'liqligi:** yo'q. Update server oddiy statik fayl
(nginx) — B yo'nalishiga ham bog'liq emas, mustaqil bajarilishi mumkin.

---

## C — Multi-device sync ekotizimi (5 app) ⏸️ TO'XTATIB QO'YILGAN

Spec va planlar **to'liq yozilgan**, implement boshlanmagan.
Foydalanuvchi aniq aytdi: boshlash vaqtini u belgilaydi.

**Hujjatlar:**
- Spec: [`specs/2026-07-29-multi-device-sync-backend-design.md`](specs/2026-07-29-multi-device-sync-backend-design.md)
- Planlar: [`plans/2026-07-29-multi-device-sync-00-index.md`](plans/2026-07-29-multi-device-sync-00-index.md) va undan keyingi 6 ta fayl

**Muhim:** bu ish **tdesktop repozitoriysida bajarilmaydi.** Backend,
web app va capture service **alohida papkada** yaratiladi.

> ⚠️ **Ishni boshlashdan oldin foydalanuvchidan qaysi papkadan
> foydalanishni so'rash SHART.** O'zim tanlamayman.

**Boshlash tartibi (foydalanuvchi ruxsatidan keyin):**
1. `01a` — backend poydevori
2. `01b` — backend sync yadrosi
3. `02` — tdesktop sync agenti (bu qism tdesktop repo'sida)
4. `03` — web controller
5. `04` — storage lifecycle
6. `05` — always-on TDLib capture service

---

## D — Mobil klientlar 🕓 KEYINGA QOLDIRILGAN

`tmobile-android` (exteraGram fork) va `tmobile-ios` (Telegram-iOS fork).

**Holat:** hali muhokama qilinmagan. Foydalanuvchi aniq aytdi —
tafsilotlarni keyin batafsil muhokama qilamiz.

**Allaqachon ma'lum bo'lgan cheklovlar** (C yo'nalishi muhokamasida
aniqlangan, muhokama boshlanganda esga olinsin):
- iOS ilovani to'xtatadi va Telegram o'chirish hodisasi uchun push
  yubormaydi → iOS'da capture tabiatan to'liq bo'lmaydi, u amalda
  asosan **viewer** bo'lib qoladi
- Samsung Note 10+ batareya boshqaruvi Android fon xizmatini o'ldiradi
- iOS uchun macOS + Xcode kerak (foydalanuvchida bor)

---

## Doimiy qoidalar

- Build ishga tushirishdan **oldin doim so'rash** — ~34 daqiqa oladi va
  boshqa og'ir ilovalar bilan raqobatlashadi
- Commit + push faqat `origin/Oybek` ga; `upstream` ga **hech qachon**
- Commit'larda `Co-Authored-By` trailer **ishlatilmaydi**
