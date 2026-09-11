# 2026-09-11 — v7.2.7 build, akkauntlar logout'i va saqlangan xabarlar aralashuvi

Holat: **ma'lumot tiklandi, ildiz sabablar kodda yopildi (`14f5d0c0a8`),
build 19:04 da o'tdi va 3 ta chat sinovda tasdiqlandi.** Ishga tushishdagi
~1 daqiqalik qotish hali ochiq (§8). Vaqtlar mahalliy (UTC+5).

---

## 1. Xronologiya

| Vaqt | Hodisa |
|---|---|
| 09-09 14:42 | `1ef536b901` — self-update'dan Telegram feed kanali olib tashlandi, faqat HTTP mirror qoldi |
| 09-10 12:24 | `24a87d07a2` — kuzatuv yoqilganda joriy holat yoziladi (`snapshot`), bufer 10 -> 60 daqiqa |
| 09-10 23:15 | `9b081b2a43` — upstream **v7.2.7** merge. `rlottie` (C++) -> `tlottie` (Rust, `Libraries` da alohida: `prepare.py rust tlottie`) |
| 09-11 ~08:10 | Build yiqildi: eskirgan `Telegram.sln` ochilgan (CMake 4.2 + VS 2026 endi `Telegram.slnx` yozadi). Eski fayl `Telegram.sln.stale-20260802` ga qayta nomlandi |
| 09-11 12:14 | Build `55 succeeded, 1 failed`: `C1083 sqlite3.c` — CustomDB SQLite'ni Qt 5.15.18 manba daraxtidan olardi, u o'chirilgan edi |
| 09-11 12:33 | `952c32360b` — SQLite 3.51.3 `Telegram/ThirdParty/sqlite` ga ko'chirildi (tde2e dagi nusxa `tdsqlite3_*` nomli, yaramaydi) |
| 09-11 13:15 | Build **56 succeeded, 0 failed** (40:56) |
| 09-11 14:45 | Yangi build ishga tushdi -> oq blur, to'liq qotish. Log: `OpenGL: Force-disabled`, `Renderer: [Raster]`. Eski sozlamada `disableOpenGL=true` turgan edi; crash dump yo'q, asosiy oqim bloklangan |
| 09-11 15:08 | **Xato tavsiya:** A/B test uchun `Telegram_OLD.exe` (merge'dan oldingi build) shu `tdata` ustida ishga tushirildi. U `key_datas` ni qayta yozdi; 15:10:48 da yangi build `could not decrypt map` -> **9 ta akkaunt logout** |
| 09-11 15:29 | Qayta login (1 akkaunt, `1474449522`). Toza sozlamalar -> `Renderer: [QRhi]`, D3D11, RTX 3050 Ti. Sekinlik login'dan keyingi resync edi, CPU tinchidi |
| 09-11 15:45 | Chatlarda saqlangan xabarlar yo'qligi aniqlandi — ular DB'da **boshqa akkauntlar nomida** turgan (§2) |
| 09-11 18:19–18:34 | DB tuzatish A+B (§3) |
| 09-11 18:50 | `14f5d0c0a8` — ildiz sabablar yopildi (§4) |
| 09-11 18:55 | `tdata` yetim qoldiqlari Recycle Bin'ga (§5) |

CustomMod ma'lumotlari (`customizationMainFolder\db`, `medias`, `config`,
registr) `tdata` dan tashqarida — logout ularga **tegmadi**.

---

## 2. Aralashuv — dalil

`account_id` = Telegram user ID (`custom_peer_key.cpp:21`). Chiquvchi
xabarning `sender_id` si egasini aniq ko'rsatadi:

| Peer | `account_id` yozilgani | Chiquvchi, `sender_id=1474449522` | `sender_id=1334067829` |
|---|---|---|---|
| 7053823996 | 1334067829 | **283** | 7 |
| 7815103103 | 1334067829 | **127** | 4 |

`text_cache` va `media_index` ham shu akkauntni ko'rsatdi.

---

## 3. DB tuzatish (bajarildi, mustaqil tekshirildi)

Ilova yopiq holda, `BEGIN IMMEDIATE`, har qator eski qiymati tekshirilib,
undo log COMMIT'dan oldin fsync qilinib.

| | Qatorlar |
|---|---|
| **A** — user/chat peerlar -> `1474449522` | 1059 (1042 toza + 17 tasida marker olib tashlandi, `sender_id` tuzatildi) |
| **B** — kanal `backup` qatorlari (103 kanal) | 5220 (11 tasida `is_out` 1 -> 0) |
| O'chirilgan qator | **0** (jami 48 227 -> 48 227) |

Natija: 7053823996 da ko'rinadigan o'chirilgan xabar **0 -> 615**,
7815103103 da **0 -> 247**. `integrity_check` ok, dublikat kalit 0,
restore dry-run: 6279 tasini qaytarsa bo'ladi.

Akkaunt bo'yicha o'zgarish: 1334067829 −3505, 7815174989 −2399,
6059877171 −323, 6607303857 −52, 1474449522 +6279.

**Ataylab qoldirilgan:**
- 52 user/chat peer: 32 tasi haqiqatan boshqa akkaunt chati, 12 ta
  "joined Telegram" shovqini, 2 tasi ikkala akkauntda, 2 ta Saved
  Messages (7815174989, 6607303857), 2 tasida dalil yo'q, basic group
  281479796986935, servis chati 777000.
- 17 juftlik ortiqchasi + 41 ichki dublikat; kanallarda 2241 dublikat +
  1376 mavjud kalit. Hech biri o'chirilmadi.

**Artefaktlar** (`C:\Users\Oybek\Pictures\customizationMainFolder\db\`):
- `actioned_messages.db.pre-account-merge-20260911-181903.bak` — to'liq zaxira
- `account_merge_report_dry-run_20260911-183257.json`, `account_merge_report_apply_20260911-183419.json`
- `account_merge_undo_20260911-183419.json` (6.4 MB)
- `account_merge_tools_20260911\account_merge.py` (dry-run sukut, `--apply`)
- `account_merge_tools_20260911\account_merge_restore.py <undo.json> [--apply]` —
  faqat hozirgi qiymati undo'dagi "new" ga teng qatorlarni qaytaradi

---

## 4. Ildiz sabablar (`14f5d0c0a8`)

| # | Sabab | Tuzatish |
|---|---|---|
| 1 | `loadDeletedMessages()` placeholder'i (Local + server ID) `_nonChannelMessages` ga kirardi. `updateDeleteMessages` faqat ID bilan keladi -> boshqa akkauntdagi o'chirish placeholder'ga tushib, DB'ga noto'g'ri chat nomidan yozilardi; `emplace` haqiqiy xabarni ham soya qilardi | Placeholder xaritaga kirmaydi; `unregisterMessage`/`changeMessageId` faqat aynan shu element bo'lsa o'chiradi (Assert o'rniga) |
| 2 | Allaqachon o'chirilgan xabar qayta o'chirilganda marker'li matn asl matn o'rniga yozilardi va `account_id=0` qatori o'zlashtirilardi | `isDeletedLocally()` bo'lsa qayta yozilmaydi; `MarkDeleted` marker'ni olib tashlaydi |
| 3 | `processNonChannelMessagesDeleted` oddiy INSERT -> dublikat | `MarkDeleted` (avval UPDATE) |
| 4 | `TryRecordBackgroundDelete` hamma akkauntlar `text_cache` idan birinchisini olardi. 2431 msg_id bir necha akkauntda uchraydi, bitta akkaunt ichida takror 0 | Faqat update kelgan akkaunt; bir nechta peer chiqsa yozilmaydi |
| 5 | `addOlderSlice` bo'sh slice'da `loadDeletedMessages()` chaqirmasdi -> butun tarixi o'chirilgan chat bo'sh (7779845655: DB'da 26 ta) | Bo'sh slice'da ham chaqiriladi |

v16 migratsiyasi (A21) qolgan legacy qatorlarni ifloslangan dalil
asosida "yagona akkaunt"ga biriktirgan — aralashuvning bir qismi shundan.

---

## 5. `tdata` tozalash

"2.5 GB" ning 1.8 GB i hozirgi akkauntning `user_data` keshi edi —
Telegram uni qayta login'dan keyin o'zi tozalagan. Qolgan yetimlar
(ichidagi eng yangi fayl 15:08 dan oldin): `user_data#2..#12` va 11 ta
eski akkaunt papkasi — **608.9 MB, 22 papka, Recycle Bin'ga**
(qaytariladi). `tdata` 810 -> 201 MB. Jonli qism (`D877F783D5D3EF8C`,
`user_data`, `key_datas`, `settingss`) tegilmagan, ildizdagi kichik
`...s` fayllar ataylab qoldirilgan. Manifest:
`account_merge_tools_20260911\tdata_cleanup_manifest_20260911.json`.

---

## 6. Qolgan ishlar

| # | Ish | Izoh |
|---|---|---|
| 1 | ~~Build `14f5d0c0a8` + sinov~~ | ✅ 19:04 build, 3 chat tiklandi (§8) |
| 1a | **Ishga tushishdagi qotish** | Har startda ~1 daqiqa javob bermaydi, oxirida oq blur; keyin ishlaydi. Tezlik scan'ining birinchi nishoni |
| 2 | **Legacy `account_id=0`** | 824 deleted (154 user/chat peer) + 14 110 backup HAR akkauntda "eski yozuv, akkaunt noma'lum" bilan ko'rinadi; `text_cache` da 8449 legacy; `GetCachedTextAndDate` hali `IN (0,?)` |
| 3 | **S1 — media ko'rgich foni miltillashi, stories foni** | Merge regressiyasi emas: eski S1, GPU yo'li qaytgani uchun ko'rindi. Tayyor: `experimental_options.json` ga `"use-qt-rhi": false` (faqat ilova YOPIQ holda — chiqishda `Write()` qayta yozadi). UI toggle rad etiladi (Win x64 da scope false); ANGLE kompilyatsiya qilinmagan -> native OpenGL |
| 4 | **`readInboxTill` + injected elementlar** | `isRegular()==false` bo'lgani uchun o'qish belgisi qo'yilmaydi, log'da 64 xato |
| 5 | **Tezlik qayta scan** | Resync tugagach |
| 6 | Qolgan marker qatorlar | 21 ta (19 — 1334067829, 2 — 1474449522); 2 tasi tozalansa bo'sh |
| 7 | Disk | `Release\DebugLogs` 729 MB, `release-staging` 222 MB — ruxsat kerak; Recycle Bin'ni user tozalaydi |

---

## 7. Saboqlar

1. **Eski build'ni yangi `tdata` ustida HECH QACHON ishga tushirmang.**
   `key_datas` joyida qayta yoziladi — qaytarilmaydi. A/B uchun `tdata`
   nusxasi + `-workdir`.
2. CustomMod DB'ga yozish — faqat ilova yopiq, zaxira + undo log bilan.
3. Log o'qishda yo'lni tekshiring: `AppData\Roaming\Telegram Desktop` —
   rasmiy Telegram, CustomMod — `Pictures\Release`.
4. Build uchun `Telegram.slnx` ochiladi, `.sln` emas.
5. Agent fayllarni butunlay o'chirmaydi — Recycle Bin.

---

## 8. Sinov natijasi — qisman stabil holat (2026-09-11 kechqurun)

Build: **19:04**, `14 succeeded, 0 failed, 42 up-to-date` (11:32).
Yakuniy link bosqichi chiqishsiz 3-6 daqiqa turadi — bu normal
(`link.exe` 5.3 GB RAM bilan faol ishlagani o'lchandi).

| Tekshiruv | Natija |
|---|---|
| 7053823996 | ✅ tiklandi |
| 7815103103 | ✅ tiklandi |
| 7779845655 (komilov, butun tarixi o'chirilgan) | ✅ tiklandi — `addOlderSlice` tuzatishi ishladi |
| "eski yozuv, akkaunt noma'lum" belgisi | Hali uchratilmadi — bunday xabari bor chat ochilmagan bo'lishi mumkin, tasdiqlanmagan |
| Ishga tushish | 🔴 ~1 daqiqa qotib turadi, oxirida oq blur, keyin ishchi holatga o'tadi |
