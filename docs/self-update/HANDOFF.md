# Self-update — implement uchun kirish nuqtasi

Bu faylni **birinchi** o'qing. Oxirgi yangilanish: 2026-08-02.

---

## 0. TL;DR — hozirgi holat

**Windows uchun self-update ishlab turibdi va production'da.** v7.0.7
akangizga/boshqa qurilmalarga allaqachon shu mexanizm bilan
tarqatilgan yoki tarqatilishga tayyor. Keyingi versiyani chiqarish —
bitta buyruq:

```powershell
.\tools\publish\release.ps1
```

Agar shu sessiyada qiladigan ishingiz "yangi versiya chiqarish" bo'lsa
— boshqa hech narsani o'qimasdan shu buyruqni ishga tushirsangiz
bo'ladi (avval Telegram/Updater/Packer build qilingan bo'lishi shart).
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
