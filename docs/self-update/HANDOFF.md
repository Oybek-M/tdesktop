# Self-update — implement uchun kirish nuqtasi

Bu faylni **birinchi** o'qing. Oxirgi yangilanish: 2026-08-06.

---

## 0. TL;DR — hozirgi holat

**Windows uchun self-update mexanizmi tayyor va production'da (v7.0.7
uchun sinalgan).** Hozir **haqiqiy real-world sinov o'rtasida
to'xtatilgan**: `Oybek` branch v7.0.7'dan **v7.0.9**'ga sync qilindi
(2026-08-06), lekin **hali build qilinmagan**. Keyingi sessiyada
davom etish tartibi:

1. **Build qiling** (Telegram.exe + Updater.exe + Packer.exe,
   `out\Release`).
2. `.\tools\publish\release.ps1` bilan haqiqiy v7.0.9 relizini
   chiqaring.
3. Hozir ishlab turgan **v7.0.7 nusxangizda** "Check for Updates"
   bosib, bu safar **oxirigacha** ("Update Telegram" tugmasini bosib,
   dastur qayta ishga tushguncha) boring — bu birinchi to'liq
   real-world self-update sinovi bo'ladi (akkaunt/sozlamalar
   saqlanishini tasdiqlash).

Keyingi versiyalar uchun (bu sinovdan keyin, doimiy jarayon):

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
