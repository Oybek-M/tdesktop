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

## 6. Ochiq savollar — barchasi hal qilindi

### 6.1 `readAutoupdatePrefix()` standart qiymati va runtime sozlash

`localstorage.cpp:543-586`. Standart qiymat kodda **literal**:
`"https://td.telegram.org"` (557-qator). Runtime override ikki bosqichli:

1. `AutoupdatePrefix()` — process-lifetime statik keshdagi qiymat (bo'sh bo'lmasa shuni qaytaradi)
2. `tdata/prefix` fayli (`cWorkingDir() + "tdata/prefix"`) — diskdan o'qiladi

`writeAutoupdatePrefix(prefix)` (560-qator) mavjud — bu qiymatni **kod
o'zgartirmasdan** o'zgartirish imkonini beradi: faylga yozadi va agar
`cAutoUpdate()` yoqilgan bo'lsa, darhol yangi tekshiruvni boshlaydi
(`Core::UpdateChecker().start()`). Ammo bu funksiya UI'dan chaqirilmaydi
hech qayerda hozircha — faqat storage qatlamida mavjud.

**Xulosa:** VPS HTTP mirror manzilini `config.h`'da qattiq yozish shart
emas — `writeAutoupdatePrefix()` orqali runtime'da o'zgartirish mumkin
bo'lgan joy tayyor turibdi. Task 2'da shundan foydalanish mumkin, lekin
birinchi ishga tushirishda baribir default qiymat kerak (chunki fayl
mavjud bo'lmagan holatda `"https://td.telegram.org"` ga qaytadi) — shu
default literal `config.h` yonida almashtiriladigan joyga ko'chiriladi.

### 6.2 `/current` fayl formati (`HttpChecker`)

`update_checker.cpp:751-836`. Ikki format qabul qilinadi (eskisi va
yangisi), ikkalasi ham sinaladi:

**Eski format** (`parseOldResponse`, 786-802): oddiy matn,
`"{version}:{url}"`, regex `^\s*(\d+)\s*:\s*([\x21-\x7f]+)\s*$`.
`url` `"beta_"` bilan boshlansa — beta versiya, `url` dan `"beta_"`
kesib tashlanadi va oxiriga `"_{signature}"` qo'shiladi.

**Yangi format** (`parseResponse` → `ParseCommonMap`, 592-682, JSON):

```json
{
  "linux": {
    "stable": { "released": "5001002:tsetup.5.1.2.tar.xz" },
    "beta":   { "released": "5001003:tbupd5001003" }
  },
  "win": { ... },
  "mac": { ... }
}
```

Struktura: `{platform}.{type}.{released|testing}`. `platform` —
`Platform::AutoUpdateKey()` (masalan `"linux"`, `"win"`, `"win64"`,
`"winarm"`, `"mac"`, `"macarm"`). `type` — `"stable"`/`"beta"`/`"alpha"`
(qaysilari tekshirilishi `cAlphaVersion()`/`cInstallBetaVersion()`ga
bog'liq). Kalit `"released"` yoki `"testing"` — `testing()` flag'iga
qarab (test build'lar `"testing"` maydonini o'qiydi, productionda
`"released"`). Qiymat: `"{version}:{link}"` yoki faqat raqam
(`(*version).isDouble()`). `link` ichida `{version}` va `{signature}`
placeholder'lari almashtiriladi. Yakuniy URL
`Local::readAutoupdatePrefix() + link` (835-qator).

**Xulosa Task 4 uchun:** VPS'ga statik `/current` fayli — yuqoridagi
JSON formatida, `link` maydoni **nisbiy** yo'l (prefiks avtomatik
qo'shiladi).

### 6.3 `MtpChecker` xabar formati va ikki bosqichli indirection

`update_checker.cpp:1051-1099`. Kanal xabarining matni **xuddi shu
`ParseCommonMap` JSON formatini** ishlatadi (6.2 bilan bir xil parser,
`testing()` argumenti bilan). Farqi: `"released"`/`"testing"` qiymati
link emas, balki `"{version}:{username}#{postId}"` — ya'ni xabar
matnidagi JSON **fayl havolasini emas, boshqa Telegram xabarini**
ko'rsatadi (`bestLocation.username`, `bestLocation.postId`).

Bu degani — **ikki bosqichli indirection**:
1. `"tdhbcfeed"` kanalidan **oxirgi xabar** olinadi (`MTPmessages_GetHistory`, limit=1)
2. O'sha xabar matni JSON — u ichida yozilgan `username#postId` boshqa
   (yoki xuddi shu) kanaldagi **fayl o'zi joylashgan xabarni** ko'rsatadi
3. `StartDedicatedLoader` o'sha ikkinchi xabarni oladi va undagi
   dokumentni yuklaydi (`ParseFile`, `dedicated_file_loader.cpp`)

### 6.4 Checker'lar bir vaqtda ishlaydi, ketma-ket emas

`update_checker.cpp:1505-1511`: `HttpChecker` va `MtpChecker`
**parallel** ishga tushiriladi (ikkalasi ham `startImplementation`
bilan bir vaqtda chaqiriladi — birinchi navbatda kutish yo'q).

`tryLoaders()` (1596-1647) ikkalasi ham tugashini kutadi
(`_httpImplementation.checker || _mtpImplementation.checker` bo'lsa
hali kutadi), keyin qaror qabul qiladi:
- Faqat MTP loader topsa → MTP ishlatiladi
- Faqat HTTP loader topsa → HTTP ishlatiladi
- Ikkalasi ham topsa → **navbat bilan almashtiriladi**
  (`_usingMtprotoLoader` flag har chaqiriqda teskarisiga o'giriladi) —
  bittasi yuklab olishda muvaffaqiyatsiz bo'lsa, keyingi urinishda
  avtomatik boshqasiga o'tadi
- Ikkalasi ham muvaffaqiyatsiz bo'lsa → `_failed` fire qilinadi

**Xulosa:** bizning "asosiy = MTP yopiq kanal, zaxira = HTTP" rejamiz
(5.3-bo'lim) kod arxitekturasiga mos — ikkalasi baribir parallel
ishlaydi va bir-birining tabiiy failover'i bo'ladi, qo'shimcha kod
yozish shart emas.

### 6.5 `packer.cpp` argumentlari

`Telegram/SourceFiles/_other/packer.cpp:151-518`. CLI argumentlar:

| Flag | Vazifasi |
|---|---|
| `-path {fayl yoki papka}` | Paketlanadigan manba (bir nechta marta berilishi mumkin) |
| `-version {N}` | Versiya raqami (masalan `5001002` — bu `5.1.2`), `1016` dan katta bo'lishi shart |
| `-target {win64\|winarm}` | Windows arxitekturasi (Windows build'ida) |
| `-arch {x86_64\|arm64}` | macOS arxitekturasi |
| `-beta` | Beta kanal — beta kalit bilan imzolanadi |
| `-alpha {N}` | Alpha versiya raqami — alohida imzolash yo'li |
| `-alphakey` | Faqat alpha kalit faylini yozib chiqadi, paketlamaydi |

**Chiqish fayl nomi** (497-513): platformaga qarab
`tx64upd{version}` / `tarm64upd{version}` / `tupdate{version}` (Windows),
`tmacupd{version}` / `tarmacupd{version}` (macOS),
`tlinuxupd{version}` (Linux). Bular kontrakt hujjatining 3-bo'limidagi
`tupdate` prefiksli fayl nomlari bilan mos keladi.

**Imzolash jarayoni allaqachon o'zida sig'diradi hamma narsani:**
LZMA bilan siqadi → SHA1 hash → RSA imzo (`PrivateKey`/`PrivateBetaKey`,
`packer_private.h` dan, repo'da **yo'q** — alohida saqlanadi) → darhol
o'z ichida `PublicKey`/`PublicBetaKey` bilan tekshirib ko'radi → faylga
yozadi. Ya'ni **bitta buyruq** compress+sign+verify+write bajaradi —
alohida "siqish" va "imzolash" bosqichlariga bo'linmagan.

⚠️ **Muhim:** `packer.cpp`dagi `PublicKey`/`PublicBetaKey` (14-28-qator)
`config.h`dagi `UpdatesPublicKey`/`UpdatesPublicBetaKey` bilan **bir xil
juftlik bo'lishi shart** — packer o'z public key'i bilan tasdiqlaydi,
tdesktop esa `config.h`dagi public key bilan tekshiradi. Ikkalasi
almashtirilganda **ikkalasi ham** yangilanishi kerak, aks holda
imzo tekshiruvi muvaffaqiyatsiz bo'ladi.

### 6.7 Implement qilindi: `ResolveOwnChannel`

`dedicated_file_loader.cpp`ga hardcoded `kFeedChannelId` (yopiq kanal
ID) va `ResolveOwnChannel()` qo'shildi — 5.2-bo'limdagi yechimning aynan
o'zi. **Muhim tuzatish:** `DedicatedLoader::Location`/`ResolveChannel`
`emoji_sets_manager.cpp` va `spellchecker_common.cpp` bilan **birga
ishlatiladigan umumiy mexanizm** ekani aniqlandi — ular haqiqiy public
username bilan chaqiradi. Shu sabab `Location.username`ni olib
tashlamadim (bu ularni sindirar edi), aksincha: `StartDedicatedLoader`
endi `username.isEmpty()` bo'lsa `ResolveOwnChannel`ga, bo'lmasa eski
`ResolveChannel`ga yo'naltiradi — ikkala eski funksionallik ham
o'zgarishsiz qoladi.

`MtpChecker`ning xabar formati soddalashtirildi:
`"{version}:{username}#{postId}"` → `"{version}:{postId}"` (username
kerak emas, kanal doim bir xil). `parseText` bo'sh username bilan
`FileLocation` qaytaradi, bu esa `StartDedicatedLoader`ni avtomatik
`ResolveOwnChannel` yo'liga yo'naltiradi.

### 6.6 Ikki-bosqichli indirection va shared `ResolveChannel` — yangi topilma

Kontraktning oldingi versiyasida "ikkita joy tuzatish kerak" degan
ogohlantirish bor edi (`parseText` + `StartDedicatedLoader`). Kodni
o'qib chiqib **yaxshi xabar** topildi: ikkalasi ham bitta umumiy
funksiyani chaqiradi — `ResolveChannel`
(`dedicated_file_loader.cpp:386-439`):

- `MtpChecker::start()` (1003-qator) — `"tdhbcfeed"` kanalini topish uchun
- `StartDedicatedLoader` (473-qator) — 6.3-bo'limdagi ikkinchi xabarning kanalini topish uchun

**Xulosa:** yopiq kanalga o'tish uchun **faqat bitta joyni** —
`ResolveChannel` funksiyasining o'zini — o'zgartirish kifoya (5.2-bo'limdagi
access_hash yechimi). Ikkala chaqiruv joyi ham avtomatik ravishda yangi
xatti-harakatni oladi. Ammo amaliy natija baribir ikki xabarni talab
qiladi (6.3): agar ikkalasi ham bitta yopiq kanalda bo'lsa (eng oddiy
variant — feed xabari va fayl xabari bitta kanalda), `ResolveChannel`
bir marta channel_id/access_hash'ni hal qiladi va ikkala chaqiruv ham
undan foydalanadi (natija keshlanadi — 406-414-qator, `ResolveCache`).
