# A27 — Self-update'ni 7.3.x V2 imzo sxemasiga ko'chirish

**Boshlangan:** 2026-10-13. **Holat:** quyidagi "Progress" ro'yxatiga qarang —
ish to'xtab qolsa, keyingi sessiya shu ro'yxatdan davom etadi.

---

## 1. Muammo

Upstream 7.3.0 yangilanish tekshiruvini butunlay almashtirdi: eski 1024-bit
RSA imzo (`config.h` dagi `UpdatesPublicKey`, `packer_private.h`) va v1 paket
formati olib tashlandi. v7.3.1 merge'da (`9a730f3c41`) `Telegram/Resources/
update/` dagi **upstream'ning** kalitlari keldi. Natija:

- hozir o'rnatilgan 7.3.1 build'imiz Telegram'ning ildiz kalitiga ishonadi va
  bizning (RSA) paketlarimizni rad etadi — self-update ishlamaydi;
- 7.2.x klientlar esa faqat v1 (RSA) paketni tushunadi.

Hozircha xavfsiz: manba faqat VPS mirror, MtpChecker o'chirilgan.

## 2. Upstream sxemasi (o'rganildi, 2026-10-13)

- **Ildiz kaliti** (Ed25519, `root-public.pem`) exe'ga mahkam joylanadi.
- **Manifest** (`manifest.min.json`) — imzolovchi kalitlar ro'yxati, kanal
  qoidalari ("AND-of-ORs": har guruhdan kamida bitta haqiqiy imzo), bekor
  qilinganlar. Ildiz kaliti uni imzolaydi (`manifest.sig`, 64 bayt, ajratilgan).
  Fayl bayt-ma-bayt imzolanadi — `.gitattributes`: `*.min.json -text`, oxirida
  yangi qator YO'Q.
- Uchala fayl build vaqtida `Telegram/cmake/update_keys_header.cmake` orqali
  `update_keys_data.h` ga joylanadi (`core/update_keys.cpp`).
- **Paket** — V2 konvert (`TDUP`, format 2): kanal, target (os/arch), versiya,
  manifest + uning imzosi, imzolar, payload. Tekshiruv:
  `Core::Updates::VerifyUpdate()` (`core/update_verify.cpp`).
- Paket ichidagi manifest saqlanganidan yangiroq bo'lsa, klient uni o'zlashtiradi
  (`Local::writeUpdateManifest`) — kalit almashtirish shu yo'l bilan.
- Manifestning umumiy `expires` maydoni faqat ma'lumot (tekshirilmaydi);
  **kalit** `expires` i tekshiriladi.
- Lenta formati o'zgarmagan: `<prefix>/current6` (Windows `AutoUpdateVersion`
  7.2.10 da ham 6), JSON `win64 -> stable -> {version, link}`. Ya'ni eski va
  yangi klientlar BIR XIL lentani o'qiydi.
- Packer: `-channel stable -keys-loc <papka> -local-key <ed25519.pem>
  -local-key-id <id>` (yoki bulut uchun `-emit-signing-input` / `-unsigned` +
  `-embed-signatures`). `-keys-loc` papkasidan uchala faylni o'qiydi va
  yozgandan keyin paketni o'zi tekshiradi (`VerifyPackedV2`).

## 3. Bizning dizayn

| Qism | Qaror |
|---|---|
| Ildiz kaliti | O'zimizning Ed25519 jufti. Maxfiy qismi faqat `C:\TBuild\DesktopPrivate\update-v2\` da (repo PUBLIC — hech qachon commit qilinmaydi) |
| Imzolovchi kalit | Ed25519, id `oy-2026a`, muddatsiz. Maxfiy qismi o'sha papkada |
| Manifest | `manifest_version` 1; `stable` va `beta` -> `[["oy-2026a"]]`; `revoked` bo'sh |
| Repo fayllari | `Telegram/Resources/update/{root-public.pem, manifest.min.json, manifest.sig}` — bizniki. **Upstream merge'da DOIM bizniki qoladi** |
| Lenta | A27 build'idan boshlab standart prefiks `.../secure/v2` (localstorage.cpp). Eski klientlar `.../secure` da (v1) qoladi — V2 paket ularni chalkashtirmaydi |
| Packer / reliz | `tools/publish/publish.ps1` V2 bayroqlari bilan; kalit yo'li DesktopPrivate'dan |
| O'tish | O'rnatilgan klientlar (laptop 7.3.1, PC 7.2.x) A27 build'ini qo'lda oladi (`install-staged.sh`); shundan keyin V2 self-update ishlaydi |
| VPS | `/secure/v2/` va yuklash kaliti — CustomSync sessiyasi qiladi (A7 paytida; u VPS'ni boshqaradi) |

**Kalit zaxirasi (A7 dan OLDIN, foydalanuvchi qiladi) — ✅ 2026-10-13
foydalanuvchi qo'lda arxivlab oldi (joyi faqat unga ma'lum):** `DesktopPrivate\
update-v2\` dagi maxfiy kalitlarni oflayn joyga (USB / parol menejeri) nusxalash.
Ildiz kaliti yo'qolsa, kelajakdagi barcha klientlarni faqat qo'lda o'rnatish
bilan almashtirish mumkin bo'ladi.

## 4. Sinov rejasi (VPS va haqiqiy tdata'ga tegmaydi)

1. Packer paketni yozgach o'zi tekshiradi (`VerifyPackedV2`) — O'TDI. Bu
   tekshiruv klient ishlatadigan AYNAN o'sha `Core::Updates::VerifyUpdate`
   (`packer.cpp:355`, `core/update_verify.cpp`) va `-keys-loc` dagi uchala
   faylni ishlatadi.
2. Statik tekshiruv (2026-10-13) — O'TDI: A27 `Telegram.exe` ichida
   `root-public.pem` (113 B), `manifest.min.json` (239 B), `manifest.sig`
   (64 B) — repo'dagi bizniki bilan bayt-ma-bayt bir xil, har biri 1 marta;
   upstream'ning uchala fayli (`9a730f3c41` dagi) — 0 marta; prefiks
   `updates.2007.uz/secure/v2` — bor. Ya'ni 1-bandda tekshirilgan kod + baytlar
   = klientdagi kod + baytlar.
3. Saqlangan manifest (o'tish xavfsizligi): `HeldManifest()`
   (`update_checker.cpp`) tdata'dagi manifestni BIZNING ildiz kaliti bilan
   tekshiradi — eski build yozgan upstream manifest (bo'lsa) rad etiladi va
   ichki (bizniki) ishlatiladi. Ya'ni mavjud tdata ustiga o'rnatish xavfsiz.
4. **To'liq sikl (yuklash -> tekshirish -> "ready" -> almashtirish) — A7
   da**, haqiqiy o'rnatma `/secure/v2` dan o'zini yangilaganda. Alohida sinov
   nusxasi bu mashinada QILINMAYDI: CustomMod arxiv papkasi
   (`ArchiveRoot()`) reyestrdagi umumiy QSettings
   (`CustomMod/TelegramDesktop`, `archiveRootPath`) dan olinadi — har qanday
   nusxa foydalanuvchining HAQIQIY bazasini ochadi (2026-08-27 dagi
   WAL/SHM buzilishiga o'xshash xavf).
5. Salbiy sinov (buzilgan / begona kalit bilan imzolangan paket rad
   etiladi) — upstream'ning o'z testi `tests/test_update_verify.cpp`
   qamrab olgan; bizda build qilinmagan (CMake'ni
   `-DTDESKTOP_BUILD_UPDATE_TESTS=ON` bilan qayta sozlash kerak — ixtiyoriy).

## 5. Progress

- [x] Upstream sxemasini o'rganish, dizayn (2026-10-13)
- [x] Kalitlar va manifest (2026-10-13): OpenSSL 3.2.2 (`genpkey -algorithm
  ed25519`; manifest `pkeyutl -sign -rawin`, `printf` bilan yangi qatorsiz).
  `DesktopPrivate\update-v2\`: `root-private.pem`, `root-public.pem`,
  `signing-oy-2026a-{private,public}.pem`, `manifest.min.json`, `manifest.sig`.
  Repo'ga faqat ochiq uchtasi; PEM LF bilan (Git'ning openssl'i CRLF yozadi).
  Manifest `issued` 1791836894, `expires` +10 yil.
- [x] Packer sinovi: 7.3.1 `Packer.exe ... -target win64 -channel stable
  -keys-loc ...\Resources\update -local-key ...\signing-oy-2026a-private.pem
  -local-key-id oy-2026a` -> `td-update-win-x64-7003002` ("written
  successfully" — yozishdan oldin o'zi tekshiradi). V2 paket NOMI:
  `td-update-win-x64-<versiya>`.
- [x] Standart prefiks `/secure/v2` (`storage/localstorage.cpp`)
- [x] `publish.ps1` V2 bayroqlari + `current6` (eski skript `current4` yozardi —
  7.x klientlar `current6` so'raydi, ya'ni avvaldan 404 edi) + barcha
  mirrorlarda `v2/` papka. `release.ps1 -DryRun` O'TDI: paket
  `td-update-win-x64-7003001`, lenta
  `{"win64":{"stable":{"link":"/win/td-update-win-x64-7003001","released":7003001}}}`.
  API yo'li (`-Api`, release-api.ps1) `/v2` joylashuvini server tomonida
  bilishi kerak — A7 da CustomSync sessiyasi bilan.
- [x] Build (2026-10-13 01:43, `7ece589feb`, `-m:4 -MP4`): nusxa
  `C:\TBuild\staged\7ece589feb-20261013\` (SHA256 `037f4f20cc6dfd0f...`).
  Packer/Updater qayta build bo'lmadi (kalitlarni ish vaqtida o'qiydi).
- [x] Sinov 1-3 (4-bo'lim) — O'TDI
- [x] O'rnatish: laptop — 2026-10-13 07:55 (`f829aa2e78`, A27+A26+A22 diag;
  start sinovi o'tdi).
- [ ] O'rnatish: PC (qo'lda). Undan keyin ham, A7 gacha, lenta
  bo'sh — yangilanish kelmaydi, xavf yo'q.
- [ ] Sinov 4 (to'liq sikl) — A7 da
- [x] Merge qoidasi: `Telegram/Resources/update/*` — DOIM bizniki (biz
  o'zgartirganimiz uchun upstream o'zgartirsa git konflikt ko'rsatadi)
- [ ] VPS tomoni + reliz — A7 da
