# A7 — O'z relizlarimiz: "canary #N" (2026-10-13)

## Qaror

Foydalanuvchi (2026-10-13): asosiy versiya upstream bilan bir xil qolsin,
relizlar "alpha N" ko'rinishida chiqsin. Agar iloji bo'lmasa, oxirgi chora
sifatida asosiy versiya raqami oshiriladi. Relizlar hammaga yuboriladi,
eski versiyadagilarga esa 1 marta qo'lda o'rnatiladi.

Natija: **asosiy versiyani oshirish shart emas.** 7.3.x da "alpha N" ning
V2 dagi o'rinbosari bor: **canary-public #N**, ya'ni `7.3.1 canary #N`.

## Nega eski alpha emas

`alpha-releases.md` (7.0.9 davri) dagi yo'l 7.3.x da ishlamaydi:

- **Packer:** `-channel` ni `-alpha`/`-beta` bilan birga qabul qilmaydi
  ("cannot be combined"). V2 imzosi esa faqat `-channel` orqali ishlaydi.
- **launcher.cpp `CheckPortableVersionFolder()`:** alpha build'da
  `Assert(*AlphaPrivateKey != 0)` bajariladi. Bizning stub'da kalit bo'sh,
  shuning uchun ilova startda crash bo'ladi. Bundan tashqari:
  - `cForceWorkingDir(TelegramForcePortable)` alohida tdata ochadi, natijada
    akkauntlar ko'rinmaydi;
  - debug log majburiy yoqiladi.
- **Alpha URL imzosi:** buning uchun alohida RSA alpha kaliti kerak.

V2 da alpha'ning o'rnini canary egallagan va u yuqoridagi muammolarning
hech biriga ega emas:

- versiya 64 bit: `(asos << 32) | N`;
- `tdata/version` da marker `0x7FFFFFFE`, uni Updater.exe ham taniydi;
- ishchi papka va tdata o'zgarmaydi.

## Qanday ishlaydi

| Qism | Joy / mazmun |
|---|---|
| Kanal + N | `Telegram/SourceFiles/custom_release.h`: `kUpdateChannel`, `kCounter`. CMake opsiyasi (`TDESKTOP_CANARY_COUNTER`) EMAS: u target-wide compile definition, uni o'zgartirish butun Telegram'ni qayta kompilyatsiya qiladi. Hozir esa faqat `core/update_channel.h` ni o'qiydigan 9 ta .cpp qayta kompilyatsiya bo'ladi |
| Feed o'qish | `core/update_checker.cpp` `HttpChecker::parseCanaryResponse()`: `win64.canary = {released, counter, link}`. `stable` yozuvi faqat asos kattaroq bo'lsa olinadi. Log satri: `Update Info: Canary feed offers <asos> #<N>, running ...` |
| MTP canary | Upstream canary'ni Telegram kanalidan oladi. Bizda bu yo'l o'chiq (`MtpChecker::start` -> `fail`), canary build ham `HttpChecker` ishlatadi |
| Manifest v2 | `Telegram/Resources/update/manifest.min.json`: `canary-public -> [["oy-2026a"]]` qo'shildi, ildiz kaliti bilan qayta imzolandi (`manifest.sig`). v1 zaxirasi: `DesktopPrivate\update-v2\manifest-v1.*`. Paket ichida v2 bor, shuning uchun v1 ni saqlagan klient ham uni o'zlashtiradi |
| Reliz | `tools/publish/publish.ps1` `custom_release.h` ni o'qiydi. Packer'ga `-channel canary-public -counter N` beradi, paket nomi `td-update-win-x64-<ver>-canary-<N>`, feed yozuvi `canary`. `-Api` yo'li canary'ni hali qo'llamaydi, chunki server feed'ga `counter` yozmaydi. Hozircha faqat scp yo'li ishlaydi |

## Kanal siyosati (`ChannelPolicyAllows`, upstream)

| Build | Qabul qiladigan paket |
|---|---|
| canary-public | canary-public (versiya joriydan katta); stable/beta faqat asos KATTAROQ bo'lsa. Bu canary'dan oddiy versiyaga qaytish yo'li |
| stable | faqat stable (beta faqat "beta o'rnatish" yoqilgan bo'lsa). **Canary paketini QABUL QILMAYDI** |

Shu sababli birinchi canary build har bir mashinaga 1 marta qo'lda
o'rnatiladi (laptop, PC, akam). 7.2.x lar V2 ni o'qiy olmagani uchun
bu qo'lda o'rnatish baribir kerak edi.

## Ko'rinishi

Upstream'niki, o'zgartirilmagan:

- About/Settings'da versiya: `7.3.1 canary #N x64`.
- Sarlavha panelida `CANARY #N` yorlig'i turadi.
- Versiya almashganda changelog xabari chiqmaydi.
- "Beta versiyalarni o'rnatish" tugmasi yashiriladi.
- Yangilanish har 10–15 daqiqada tekshiriladi (stable'da 8 soatda).

Nomni o'zgartirish oson: `core/update_channel.h` dagi `CanaryTitleLabel()`
va `CanaryVersionSuffix()`.

## Reliz tartibi

1. `custom_release.h` da `kCounter` ni +1 qiling. Upstream merge'dan keyin
   `AppVersion` o'zgargan bo'lsa, hisoblagich 1 dan boshlanadi.
2. Build: Telegram (Packer/Updater faqat o'zgargan bo'lsa).
3. `tools/publish/release.ps1 -DryRun` ni ishga tushiring. So'ng
   foydalanuvchi ruxsati bilan `release.ps1` ni ishga tushiring.
4. Klient log'ida `Canary feed offers ...` qatori chiqishi kerak. Keyin
   yuklab olish va qayta ishga tushirish bo'ladi, oxirida versiya `#N`
   ko'rsatadi.

## Holat

- [x] Kod, manifest v2 va `publish.ps1` (2026-10-13)
- [x] Dry-run: Packer `td-update-win-x64-7003001-canary-1` ni imzoladi
      va o'zi tekshirdi (`VerifyPackedV2`). Feed:
      `{"win64":{"canary":{"released":7003001,"counter":1,"link":"/win/td-update-win-x64-7003001-canary-1"}}}`
- [x] Build (2026-10-14 09:22): `C:\TBuild\staged\efde411217-20261014\`
      (SHA256 `5f9e30dc4f132fdd...`); exe ichida manifest v2 bor
- [x] Laptop'ga o'rnatildi (2026-10-14 23:51): log'da `canary: #1`; feed
      hali yo'q -> `Update Error: ... 203` (404) kutilgan
- [ ] VPS: reliz SSH kaliti hali ruxsat etilmagan (`Permission denied
      (publickey)`); `/secure/v2` ni `publish.ps1` o'zi yaratadi. Yarim
      yuklangan 7.2.7 ni tekshirish
      - 10-14: VPS'ga root kaliti EMAS, SFTP-only foydalanuvchi
        `customsync-release` bilan yoziladi (CustomSync rejasi: chroot
        `/var/www/updates`, `ForceCommand internal-sftp`; kalit qo'shish
        egasining qarori). `publish.ps1` (`2eeff6ae42`) `sftp -b` ishlatadi:
        `R/win/<paket>.part` va `R/current6.tmp` ni yuklab, posix-rename
        bilan almashtiradi (R = `/secure/v2`, `/<pub_prefix>/v2`). Shell
        buyrug'i yo'q, `pub_prefix` chiqishda niqoblanadi. Serverga
        ulanmasdan soxta sftp bilan sinalgan.
- [ ] PC va akamga #1 ni 1 marta qo'lda o'rnatish
- [x] e2e (2026-10-15): canary #2 (`80489011e3`) VPS'ga chiqdi, laptop
      #1 -> #2 o'zi yangilandi (08:24 topdi, 08:28 `canary: #2`). GitHub
      mirror qo'lda (`publish.ps1` tuzatildi, `b2d0bb5568`)
