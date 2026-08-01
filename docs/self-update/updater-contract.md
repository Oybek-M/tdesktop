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

### 5.1 Tekshirildi: kanal nomini almashtirish YETARLI EMAS

`dedicated_file_loader.cpp:434`:

```cpp
mtp->send(MTPcontacts_ResolveUsername(
    MTP_flags(0),
    MTP_string(username),
    MTP_string()
), doneHandler, failHandler);
```

`ResolveChannel` **faqat username orqali** ishlaydi
(`contacts.resolveUsername`). Yopiq kanalda username yo'q → resolve
muvaffaqiyatsiz bo'ladi. Ya'ni `"tdhbcfeed"` o'rniga o'z kanalimiz
nomini yozib qo'yish **ishlamaydi**.

### 5.2 Lekin g'oya baribir amalga oshadi — kichik o'zgarish bilan

`ResolveChannel` ning yagona vazifasi — `MTPInputChannel` hosil qilish,
u esa shunchaki `MTP_inputChannel(channel_id, access_hash)`.

Agar akkaunt kanalning **a'zosi** bo'lsa, bu ikkala qiymat lokal
sessiyada allaqachon mavjud — resolve umuman kerak emas.

**Yechim:**

```cpp
// ResolveChannel(...) o'rniga:
// 1. Kanal ID si kodda qattiq yoziladi (u hamma uchun bir xil)
// 2. access_hash lokal sessiyadan olinadi
//    (session->data().channelLoaded(channelId)->access_hash)
// 3. MTP_inputChannel(MTP_long(id), MTP_long(accessHash)) quriladi
```

⚠️ **`access_hash` ni kodga yozib bo'lmaydi** — u har bir akkaunt
uchun har xil. Uni ish vaqtida lokal sessiyadan olish shart.

**Yon foyda:** akkaunt kanal a'zosi bo'lmasa, `channelLoaded` null
qaytaradi va tekshiruv jimgina muvaffaqiyatsiz bo'ladi — ya'ni
**yangilanishni faqat kanal a'zolari oladi.** Bu bizga aynan kerak
bo'lgan kirish nazorati, qo'shimcha mexanizmsiz.

Taxminiy hajm: `MtpChecker::start()` da ~15-20 satr.

### 5.3 Yakuniy mirror strategiyasi

```
1. O'z YOPIQ Telegram kanalimiz (MtpChecker)  ← asosiy, hosting kerak emas
2. VPS HTTP (HttpChecker)                     ← zaxira
3. GitHub private repo                        ← oxirgi chora
```

---

## 6. Ochiq savollar

Eng muhimi (yopiq kanal masalasi) **hal qilindi** — 5.1/5.2 ga qarang.
Qolganlari implement paytida aniqlanadi; ular dizaynni o'zgartirmaydi,
faqat tafsilotlarni to'ldiradi.

1. `Local::readAutoupdatePrefix()` standart qiymatni qayerdan oladi?
   `writeAutoupdatePrefix()` bormi — ya'ni prefiksni **kod
   o'zgartirmasdan** sozlash mumkinmi?
2. `HttpChecker` so'raydigan `/current` faylining **formati** qanday?
   (`gotResponse()`, 736-qator va undan keyin)
3. `MtpChecker` kanal xabaridan havolani qanday ajratadi?
   (`parseText`, ~1084-1086 — `username` va `postId` ajratiladi.)
   Xabar qanday formatda yozilishi kerak?
4. Ikkala checker birga ishlaydimi yoki biri ikkinchisining
   zaxirasimi? Qaysi biri birinchi?
5. `packer.cpp` qanday argumentlar qabul qiladi va chiqish fayli
   nomini qanday hosil qiladi?

⚠️ **3-savolda kutilmagan bog'liqlik bor:** `parseText` xabardan
`username` ajratadi va keyin `StartDedicatedLoader` uni yana resolve
qiladi (`dedicated_file_loader.cpp:473`). Ya'ni yopiq kanalga o'tishda
**ikkita** joyni tuzatish kerak bo'lishi mumkin: kanalning o'zini
topish (5.2) va xabar ichidagi havolani ochish. Implement paytida
`parseText` va `StartDedicatedLoader` ni birga ko'rib chiqing.
