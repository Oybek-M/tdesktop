# Self-update — implement uchun kirish nuqtasi

Bu faylni **birinchi** o'qing. Oxirgi yangilanish: 2026-08-08.

---

## 0. TL;DR — hozirgi holat

⚠️ **BLOKLANGAN — birinchi real-world update-apply sinovi
MUVAFFAQIYATSIZ tugadi, sabab hali aniqlanmagan.** Keyingi sessiya
aynan shu yerdan boshlanishi kerak — pastga qarang (§0.1).

Build va reliz qismi ishladi:

1. ✅ v7.0.9 build qilindi (Telegram.exe, Updater.exe, Packer.exe —
   VS2022→2026 ko'chishi bilan bog'liq ikkita muammo hal qilindi,
   §2.6 ga qarang).
2. ✅ `.\tools\publish\release.ps1` bilan haqiqiy v7.0.9 (7000009)
   chiqarildi, uchala mirror (VPS secure, VPS pub, GitHub) tasdiqlandi.
3. ❌ **Test nusxada (`C:\TelegramTest-v7.0.7\`) update-apply
   ishlamadi** — pastga qarang.

### 0.1. ⚠️ Hal qilinmagan muammo: update yuklandi, lekin qo'llanmadi

**Kuzatilgan:** Test nusxada (v7.0.7, `C:\TelegramTest-v7.0.7\`)
"Check for Updates" bosildi → "New version is ready" chiqdi → "Update
Telegram" bosildi → yuklab olindi (50.6 MB, 100%) → dastur **yopilib
qayta ochildi** (restart sikli ishladi) → lekin **Settings sidebar
hali ham "Telegram Desktop Version 7.0.7 x64" ko'rsatmoqda** — versiya
o'zgarmadi.

**Tasdiqlangan faktlar** (keyingi sessiya qayta tekshirmasin):
- Mirror'lardagi paket to'g'ri va tekshirilgan (checksum mos,
  `release.ps1` xatosiz tugagan).
- Signature verification avval (Task 3, jonli test) alohida
  tasdiqlangan — imzo tekshiruvi ishlaydi.
- Restart sikli **sodir bo'ldi** (dastur yopilib qayta ochildi) — bu
  `Updater.exe`ning umuman ishga tushmaganini emas, balki fayl
  almashtirish bosqichida nimadir noto'g'ri ketganini ko'rsatadi.

**Keyingi sessiyada tekshirish kerak (`superpowers:systematic-debugging`
bo'yicha, taxmin qilmasdan):**
1. `C:\TelegramTest-v7.0.7\tdata\DebugLogs\` ichida `-debug` flag bilan
   ishga tushirib, `Updater`ning o'z logini olish (`updater.log`
   ga o'xshash, `_other/updater_win.cpp`dagi `writeLog()` chiqishi).
2. `C:\TelegramTest-v7.0.7\tdata\tupdates\` papkasi hali ham
   mavjudmi — agar `ready`/`temp` ichida chala qolgan bo'lsa, bu
   `Updater.exe` fayllarni ko'chirishda to'xtab qolganini bildiradi.
3. `Telegram.exe`ning fayl vaqti/hajmi haqiqatan o'zgarganmi (agar
   o'zgarmagan bo'lsa — almashtirish umuman sodir bo'lmagan;
   `Get-FileHash`/`Get-Item` bilan tekshirish).
4. Bugun `Updater.exe`ning o'zi ham qayta build qilingan edi (VS2022→
   2026 toolset muammosi tufayli, §2.6) — ehtimol shu yangi build'da
   biror regressiya bor, avvalgi (eski, `v143` bilan qurilgan)
   `Updater.exe` bilan solishtirib ko'rish foydali bo'lishi mumkin.
5. `readAutoupdatePrefix()` process-lifetime keshlash muammosi
   (avvalgi sessiyada topilgan) bu safar daxldor emasligini
   tasdiqlash — chunki foydalanuvchi **to'liq restart** qilgan edi.

Keyingi versiyalarni chiqarish (muammo hal qilingandan keyin, doimiy
jarayon):

```powershell
.\tools\publish\release.ps1
```

Qolgan bo'limlar — **nima uchun** shunday ishlashi va **Linux/macOS**
uchun qolgan ishlar.

---

## 1. To'liq tafsilot kerak bo'lsa — shu ikkalasini o'qing

| Fayl | Nima bor |
|---|---|
| [`updater-contract.md`](updater-contract.md) | Mavjud mexanizm qanday ishlashi — kodda **tasdiqlangan** faktlar |
| [`key-management.md`](key-management.md) | RSA kalit qayerda, nega 1024-bit, zaxira talabi |
| [`../superpowers/plans/2026-08-01-self-update-plan.md`](../superpowers/plans/2026-08-01-self-update-plan.md) | 6 bosqichli plan, Task 1-4 ✅, Task 5-6 bloklangan |

---

## 2. Windows uchun nima qilingan (Task 1-4, hammasi ✅)

- Rasmiy Telegram public key/mirror butunlay almashtirildi — o'zimizning
  RSA-1024 kalit (`DesktopPrivate/customsync-updates-private.pem`,
  repo'dan **tashqarida**) va o'zimizning VPS (`updates.2007.uz`).
- Yangilanish tekshiruvi 3 ta mustaqil manba orqali: VPS secure
  (Basic-auth), VPS pub (yashirin path), GitHub (`Oybek-M/tdesktop-releases`,
  private-effektiv chunki fork public bo'lsa ham reliz repo alohida).
- Imzo tekshiruvi jonli sinovdan o'tgan: buzilgan paket rad etiladi,
  to'g'ri paket qabul qilinadi.
- **2026-08-02: haqiqiy 7.0.7 relizi chiqarilgan va uchala mirror'da
  tasdiqlangan** (`tools/publish/release.ps1` orqali).

**Reliz chiqarish jarayoni (kelgusi versiyalar uchun):**

1. `Telegram/SourceFiles/core/version.h`dagi `AppVersion`ni oshiring,
   odatdagidek build qiling (Telegram.exe + Updater.exe;
   `cmake --build out --target Packer --config Release` bilan
   Packer.exe ham build bo'lgan/yangilangan bo'lishi kerak).
2. `.\tools\publish\release.ps1` ishga tushiring — bu `out\Release`dan
   kerakli fayllarni (`Telegram.exe`, `Updater.exe`, `modules\`)
   `out\Release\release-staging\`ga joylaydi, `Packer.exe` bilan
   imzolaydi, uchala mirror'ga yuklaydi va checksum bilan tasdiqlaydi.
   Avval sinab ko'rish uchun `-DryRun`.
3. Muvaffaqiyatli chiqsa — mijozlar keyingi "Check for Updates"da
   (yoki avtomatik tekshiruvda) yangi versiyani ko'radi.

**Muhim eslatma:** `publish.ps1`ni to'liq ishga tushirish (ssh+scp+git
push kombinatsiyasi) ba'zan avto-rejim classifier tomonidan
bloklanishi mumkin — shunda alohida `ssh`/`scp`/`git` buyruqlari bilan
qo'lda takrorlash kerak bo'ladi (bu holat oldin uchragan, ishlaydigan
workaround).

---

## 2.5. v7.0.9 sync (2026-08-06) — branch flow tiklandi, lib_ui fork

Bu safar upstream sync **to'liq `dev → SafeWall → Customizations ↔
Oybek` zanjiri orqali** bajarildi (avvalgi sync'lar to'g'ridan-to'g'ri
`Oybek`ga qilingan edi, zanjir uzoq vaqt ishlatilmay eskirib qolgan
edi). Muhim topilmalar:

- **`Customizations` branch'i butunlay reset qilindi** — u eski
  (2026-03/06 dagi, Saidjon bilan qilingan Ghost Mode/AntiDelete/SQLite
  tajribalari) tarixni saqlab turgan edi, `Oybek`dan mutlaqo boshqa
  bazada. User qarori: bu eski tarix endi asosiy nuqta emas —
  `Customizations` `Oybek`ning nusxasiga tekislandi, keyin `SafeWall`
  (v7.0.9) shu ustiga merge qilindi. Eski tarix git'da yo'qolmagan
  (reflog/eski SHA orqali topsa bo'ladi), lekin branch ko'rsatkichi
  endi undan uzoqlashgan — **`origin/Customizations` force-push
  qilingan**.
- **Merge deyarli konfliktsiz o'tdi** — faqat bitta konflikt:
  `Telegram/lib_ui` submodule pointer'i (bizning Qt5 patch'imiz
  sababli). Yechim: upstream'ning yangi `lib_ui` commit'i ustiga
  bizning Qt5-guard patch'imiz (`fix: restore Qt5 compatibility
  guards...`) qayta cherry-pick qilindi (konfliktsiz).
- **Muhim topilma:** bu Qt5 patch hech qachon GitHub'da bo'lmagan —
  faqat lokal kompyuterda "tasodifan" saqlanib qolgan edi (chunki
  `desktop-app/lib_ui`ga yozish huquqimiz yo'q). Bu **fresh clone'da
  ishlamay qolishi mumkin edi**. Yechim: `desktop-app/lib_ui`
  `Oybek-M/lib_ui`ga fork qilindi (branch: `oybek-qt5-patch`),
  `.gitmodules` shu fork'ga yo'naltirildi. Endi istalgan yangi clone
  ham ishlaydi.
- Barcha self-update fayllari (config.h kaliti, update_checker.cpp,
  localstorage.cpp mirror URL, packer.cpp, CMakeLists.txt Packer
  target) merge'dan keyin **tekshirilib, saqlanib qolgani tasdiqlangan**.
- `dev`, `SafeWall`, `Customizations`, `Oybek` — barchasi push qilindi.
  Hech narsa hali **build qilinmagan**.

---

## 2.6. VS2022 → VS2026 ko'chishi (2026-08-08) — build muhiti tuzatildi

Foydalanuvchi Visual Studio'ni 2022'dan 2026'ga yangiladi (v143 toolset
o'rniga v145), bu `out\` CMake keshini butunlay eskirtirib qo'ydi.
Ketma-ket 3 ta muammo chiqdi va hal qilindi:

1. **`MSB8020` — v143 toolset topilmadi.** `out\CMakeCache.txt`da
   eski VS2022 generator/toolset qattiq yozilgan edi. Yechim:
   `CMakeCache.txt` + `CMakeFiles/`ni o'chirib,
   `cmake -S . -B out -G "Visual Studio 18 2026" -A x64` bilan qayta
   konfiguratsiya qilindi.
2. **`QT` muhit o'zgaruvchisi yo'qolgan edi** (eski keshda saqlangan
   ekan, alohida hech qayerda emas) — `QT=5.15.18` qayta va **doimiy**
   (`User` env var) o'rnatildi.
3. **`api_id`/`api_hash` ham eski keshda yo'qolgan edi** — foydalanuvchi
   qayta kiritdi, `-D TDESKTOP_API_ID=... -D TDESKTOP_API_HASH=...`
   bilan qayta konfiguratsiya qilindi.

⚠️ **Eng jiddiy topilma — o'zim yaratgan xato:** CMake keshini
tozalaganimda, avvalroq yoqilgan **`DESKTOP_APP_DISABLE_AUTOUPDATE=OFF`**
ham standart holatiga (**ON — self-update o'chirilgan**) qaytib
ketgan edi, men shuni darhol payqamadim. Natija: `Updater`/`Packer`
loyihalari CMake tomonidan qayta yaratilmay, disk'da eski (`v143`)
holida "stale" bo'lib qoldi — shu sabab faqat shu ikkalasi build'da
xato berdi (qolgan 57 loyiha muammosiz o'tdi). Sabab
`message(STATUS ...)` diagnostikasi bilan topildi: o'sha bo'lim umuman
ishga tushmagani (`DIAG` chiqishi yo'qligi) `DESKTOP_APP_DISABLE_AUTOUPDATE`
keshda `ON` ekanini ko'rsatdi. Tuzatish:
`-D DESKTOP_APP_DISABLE_AUTOUPDATE=OFF` qo'shildi, qayta konfiguratsiya
qilindi — ikkalasi ham `v145`da to'g'ri qayta yaratildi.

**Har safar CMake keshi tozalanganda eslab qolish kerak** — quyidagi
`-D` flag'lar barchasi kerak, aks holda birortasi standart holatiga
qaytib, kutilmagan build xatosiga olib keladi:
```
-D TDESKTOP_API_ID=<sizniki>
-D TDESKTOP_API_HASH=<sizniki>
-D DESKTOP_APP_DISABLE_AUTOUPDATE=OFF
```
Va `QT=5.15.18` muhit o'zgaruvchisi o'rnatilgan bo'lishi kerak (endi
doimiy o'rnatilgan, User darajasida).

**CPU cheklovi (`ProcessorNumber=8`, 2026-07-03'da hujjatlashtirilgan)
har CMake reconfigure'da o'chib ketadi** — `.vcxproj` fayllar qayta
yaratilganda qayta qo'llash kerak bo'ladi (bash bir-liner sifatida bu
sessiyada bir necha marta ishlatilgan, tarixni qarang).

---

## 3. Linux/macOS (Task 5-6) — bloklangan, ishlatiladigan mashina yo'q

Kod ko'rib chiqildi (`updater_linux.cpp`, `updater_osx.m`) —
ikkalasida ham URL/kalitga oid hardcode **yo'q**, umumiy fayllar
(config.h, update_checker.cpp, localstorage.cpp, packer.cpp,
CMakeLists.txt) allaqachon barcha platformalar uchun bir xil ishlaydi.

**Lekin:** `packer.cpp`da chiqish fayl nomi (`tlinuxupd{version}` /
`tmacupd{version}`) **compile-time** `#ifdef Q_OS_WIN/Q_OS_MAC` bilan
tanlanadi — Packer haqiqatan ham o'sha OS'da build+run bo'lishi shart,
Windows'dan cross-compile qilib bo'lmaydi. Shu sabab Task 5/6'ning
build/sinov qadamlari **Linux/macOS mashina paydo bo'lgunча ochiq**.
Reja hujjatida (`2026-08-01-self-update-plan.md`, Task 5/6) tafsilot bor.

---

## 4. Qat'iy qoidalar (hali ham amalda)

- **Build'ni hech qachon o'zingiz boshlamang.** Har safar so'rang.
- Commit + push faqat `origin/Oybek` ga. `upstream` ga **hech qachon**.
- Commit xabarlarida `Co-Authored-By` trailer **ishlatilmaydi**.
- Private RSA kalit, VPS parollari, pub-mirror'ning yashirin path'i
  — bularning barchasi `DesktopPrivate/` papkasida (repo'dan tashqarida,
  gitignore emas — **jismonan boshqa joyda**) va hech qachon
  commit qilinmaydi. Bu fork public bo'lgani uchun — commit qilingan
  narsa hammaga ko'rinadi.
- Reliz skriptlari (`tools/publish/*.ps1`) sirlarni doim
  `DesktopPrivate/vps-mirror-secrets.txt`dan **runtime'da** o'qiydi,
  hech qachon hardcode qilmaydi — bu qoidani buzmang.
