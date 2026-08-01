# Updater kontrakti — mavjud mexanizm

**Holat:** qisman tugallangan. Quyidagilar kodda **tasdiqlangan**;
"Ochiq savollar" bo'limidagilar hali tekshirilmagan.

Manba: `Telegram/SourceFiles/core/update_checker.cpp`, `config.h`

---

## 1. Ikkita mustaqil tekshiruvchi bor

Bu rejalashtirilganidan farq qiladi va dizaynga ta'sir qiladi.

### 1.1 `MtpChecker` — Telegram kanali orqali

`update_checker.cpp:994-1003`:

```cpp
void MtpChecker::start() {
    ...
    const auto feed = "tdhbcfeed"
    MTP::ResolveChannel(&_mtp, feed, [=](...) { ... });
```

Rasmiy tdesktop yangilanishlarni **Telegram kanalidan** oladi
(`@tdhbcfeed`). Kanal xabarlari ichidan yangilanish havolasi topiladi
(`bestLocation.username`, `postId` — 1084-1086 qatorlar).

### 1.2 `HttpChecker` — HTTP orqali

`update_checker.cpp:718-723`:

```cpp
const auto path = Local::readAutoupdatePrefix()
    + qstr("/current")
    + (updaterVersion > 1 ? QString::number(updaterVersion) : QString());
```

**Muhim:** URL prefiksi kodda literal emas — u
`Local::readAutoupdatePrefix()` orqali **lokal sozlamadan** o'qiladi.
Aynan shu sababli manba kodda `updates.tdesktop.com` satri umuman
topilmaydi.

---

## 2. Kalitlar

`config.h:45` va `config.h:53` — **ikkita** RSA public key:

| Konstanta | Kanal |
|---|---|
| `UpdatesPublicKey` | Barqaror (stable) |
| `UpdatesPublicBetaKey` | Beta |

`update_checker.cpp:374, 390` da qaysi biri ishlatilishi tanlanadi.

**Almashtirish kerak bo'lgan joy aniqlandi:** `config.h`, ikkala
konstanta ham.

---

## 3. Fayl tuzilishi (klient tomonda)

Barchasi `cWorkingDir() + "tupdates/"` ichida:

| Yo'l | Vazifasi |
|---|---|
| `tupdates/` | Yuklab olingan paketlar |
| `tupdates/temp/` | Ochilgan (unpacked) fayllar |
| `tupdates/temp/ready` | "Tayyor" belgisi fayli |
| `tupdates/ready/` | Qo'llashga tayyor holat |
| `tupdates/temp/Updater.exe` | Windows'da almashtirishni bajaruvchi (1801) |
| `tupdates/temp/Updater` | Linux'da (1807) |
| `tupdates/temp/Telegram.app/Contents/Frameworks/Updater` | macOS'da (1804) |

Paket fayl nomlari `tupdate` prefiksi bilan (297-301 qatorlar).

---

## 4. Tekshirish oralig'i

`config.h:27-28`:

```
UpdateDelayConstPart = 8 * 3600   // minimal 8 soat
UpdateDelayRandPart  = 8 * 3600   // ustiga tasodifiy 0..8 soat
```

Ya'ni tekshiruv 8–16 soatda bir marta. Boshqa cheklovlar:
`kUpdaterTimeout = 10s`, `kMaxResponseSize = 1 MB` (67-68).

---

## 5. ⚠️ Dizaynga ta'sir qiladigan topilma

> **Yangilanishni o'z Telegram kanalimizdan tarqatish mumkin.**

`MtpChecker` allaqachon shu ishni qiladi — faqat kanal nomi
qattiq yozilgan (`tdhbcfeed`). Uni o'zimizning **yopiq kanalimizga**
almashtirsak:

| | Natija |
|---|---|
| Hosting | **Umuman kerak emas** — VPS ham, GitHub ham |
| Maxfiylik | Yopiq kanal — faqat siz va aka a'zo |
| Ishonchlilik | Telegram infratuzilmasi |
| Trafik | Telegram hisobidan |
| api_id xavfi | **Yo'q** — paket ochiq joyda turmaydi |

Bu rejadagi uchta mirror muammosini butunlay yo'q qiladi va sizning
"private qilish" savolingizga eng toza javob bo'lishi mumkin.

**Lekin tekshirilishi kerak** (Ochiq savollar, 6-bo'lim) — kanal
nomini almashtirish yetarlimi yoki boshqa bog'liqliklar bormi.

Agar ishlasa, mirror strategiyasi shunday bo'lishi mumkin:

```
1. O'z Telegram kanalimiz (MtpChecker)  ← asosiy, hosting kerak emas
2. VPS HTTP (HttpChecker)               ← zaxira
3. GitHub private repo                  ← oxirgi chora
```

---

## 6. Ochiq savollar (hali tekshirilmagan)

Bularni implement boshlashdan **oldin** aniqlash kerak. Taxmin
qilinmadi — kodda topilmagan narsa shu yerda ochiq qoldirildi.

1. `Local::readAutoupdatePrefix()` standart qiymatni qayerdan oladi?
   `writeAutoupdatePrefix()` bormi — ya'ni prefiksni **kod
   o'zgartirmasdan** sozlash mumkinmi?
2. `HttpChecker` so'raydigan `/current` faylining **formati** qanday?
   (`gotResponse()`, 736-qator va undan keyin)
3. `MtpChecker` kanal xabaridan havolani qanday ajratadi? Xabar
   qanday formatda bo'lishi kerak?
4. Ikkala checker birga ishlaydimi yoki biri ikkinchisining
   zaxirasimi? Qaysi biri birinchi?
5. `packer.cpp` qanday argumentlar qabul qiladi va chiqish fayli
   nomini qanday hosil qiladi?
6. **MtpChecker uchun kanalni almashtirish yetarlimi?** Kanal ochiq
   bo'lishi shartmi (`ResolveChannel` username orqali ishlaydi —
   yopiq kanalda username bo'lmaydi)?

> 6-savol eng muhimi: yopiq kanalda username yo'q, `ResolveChannel`
> esa username bilan ishlaydi. Agar shunday bo'lsa, "yopiq kanal"
> g'oyasi ishlamaydi va HTTP mirror'larga qaytish kerak bo'ladi.
> Buni birinchi navbatda tekshirish kerak.
