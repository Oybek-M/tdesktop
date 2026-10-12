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

**Kalit zaxirasi (A7 dan OLDIN, foydalanuvchi qiladi):** `DesktopPrivate\
update-v2\` dagi maxfiy kalitlarni oflayn joyga (USB / parol menejeri) nusxalash.
Ildiz kaliti yo'qolsa, kelajakdagi barcha klientlarni faqat qo'lda o'rnatish
bilan almashtirish mumkin bo'ladi.

## 4. Sinov rejasi (VPS va haqiqiy tdata'ga tegmaydi)

1. Packer paketni yozgach o'zi tekshiradi (`VerifyPackedV2`).
2. To'liq sikl: alohida sinov papkasidagi A27 klient + `tdata/prefix` ->
   `http://127.0.0.1:<port>` (lokal HTTP server: `current6` + V2 paket).
   Kutilgan: yuklab olish -> `VerifyUpdate` o'tadi -> "ready".
3. Salbiy sinov: upstream kalit bilan imzolangan / buzilgan paket rad etiladi.

## 5. Progress

- [x] Upstream sxemasini o'rganish, dizayn (2026-10-13)
- [ ] Kalitlar va manifest (DesktopPrivate + `Resources/update`)
- [ ] Standart prefiks `/secure/v2`
- [ ] `publish.ps1` V2 bayroqlari
- [ ] Build (CustomSync sessiyasini oldindan ogohlantirish)
- [ ] Sinov 1-3
- [ ] Merge qoidasi hujjatda (Resources/update — bizniki)
- [ ] VPS tomoni + reliz — A7 da
