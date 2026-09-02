# Loyihalar holati — HAR SESSIYA SHU YERDAN BOSHLANADI

Oxirgi yangilanish: **2026-09-02**

> ✅ **2026-08-26 dagi ko'p akkauntli aralashuv xatosi HAL QILINDI.**
> Protokol tomoni: spec §0.12 (`record_id` ga `account_hash`),
> `test-vectors.json` qayta generatsiya qilindi, .NET tasdiqladi.
> tdesktop tomoni: sxema v10 (`account_id`) qurildi.
> Tashxis: [`../superpowers/specs/2026-08-26-multi-account-db-isolation-design.md`](../superpowers/specs/2026-08-26-multi-account-db-isolation-design.md)

> Bu fayl bir nechta loyiha o'rtasida "kim nimani bajardi" ni
> ko'rsatadi. Sessiya boshida o'qing, oxirida yangilang.

---

## Umumiy holat

| Loyiha | Holat | Keyingi qadam |
|---|---|---|
| **tdesktop** (CustomMod) | 🟢 v7.1.1 chiqarildi, ishlab turibdi | Plan 02 Task 4 (outbox qatlami va enqueue nuqtalari) |
| **customsync-server** | ✅ 01a va 01b TUGADI | — |
| **server-controller** | ⚪ boshlanmagan | 01a/01b tugagach |
| **tmobile-android** | ⚪ muhokama qilinmagan | — |
| **tmobile-ios** | ⚪ muhokama qilinmagan | — |

---

## tdesktop — protokolga tegishli holat

| Nima | Holat |
|---|---|
| `qtwebsockets` moduli | ✅ qurildi va sinovdan o'tdi (2026-08-25) |
| Sxema versiyasi | **v12** qo'llangan va tasdiqlangan (2026-08-30). **v13** (`actioned_messages.read_at`, A17) kodda tayyor, build kutilmoqda. 🔴 Track C uchun keyingi bo'sh versiya — **v14** (v13 BAND) |
| `account_id` (ko'p akkaunt) | ✅ **BOR** — 5 ta jadvalda. `activity_history` ataylab FILTRLANMAYDI (spec §0.13) |
| `media_index` jadvali | ✅ mavjud, 1543 yozuv |
| `sha256` maydoni | ❌ **BO'SH** — hisoblash hali yozilmagan |
| `sync_outbox` / `sync_state` | ❌ yo'q — plan 02 Task 3 |
| `CustomSync` moduli | ❌ yo'q |
| `PeerNameCache` | ✅ mavjud (`peer_directory` ning lokal proyeksiyasi) |
| Retention (activity) | ✅ 30 kun, tozalash ishlaydi |

### tdesktop'da 2026-08-25 da tuzatilgan (Track C ga ta'sir qiladi)

| Xato | Nima uchun protokolga tegishli |
|---|---|
| Rasmlar arxivlanmasdi | `media_index` da `image` yozuvlari faqat 08-24 dan bor — undan oldingi rasmlar YO'Q. Sync ularni topa olmaydi |
| Qo'lda yuklangan media indekslanmasdi | L1 yo'li `media_index` ga yozmasdi — `GetArchivedMediaPath()` ularni ko'rmasdi |
| Placeholder qayta arxivlanardi | ASL MATN o'rniga marker saqlanardi. ✅ TASDIQLANDI: 12 -> 0, haqiqiy 7 ta xabar tegilmadi, chat ochilgach yangi buzilgan paydo bo'lmadi |

⚠️ **Sync uchun muhim:** eski (2026-08-24 dan oldingi) rasmlar
arxivda YO'Q. Ular `media_index` da ham yo'q, ya'ni sync ularni
"yo'qolgan" deb ham ko'rsata olmaydi. Bu qabul qilingan
yo'qotish — tiklab bo'lmaydi.

### tdesktop'da Track C uchun QOLGAN ishlar

1. ✅ **Sxema v10 — `account_id`** (5 ta jadval) + media tuzatishlari — kod tayyor, build o'tdi.
   Batafsil reja:
   [`../superpowers/specs/2026-08-26-multi-account-db-isolation-design.md`](../superpowers/specs/2026-08-26-multi-account-db-isolation-design.md)
   §4. Sync'dan OLDIN bajariladi.
2. **`sha256` hisoblash** — `media_index` ga to'ldirish
   (yangi fayllar + mavjud 1543 ta uchun backfill)
3. **Sxema v14** — `sync_outbox` + `sync_state`
   ⚠️ v9 band (placeholder tozalash), v10 band (`account_id`), v11 band (`activity_history.source` ustuni, A16 §1), v12 band (story backfill, A16 §1), v13 band (`actioned_messages.read_at`, A17)
4. `CustomSync` moduli — plan 02 ning qolgan qismi

---

## customsync-server — implement holati

**Papka:** `Projects programming\Telegram\customsync-server`
**Branch:** `Oybek` — `dotnet test`: **105 test, hammasi o'tadi**

🔴 **Aniq holat va keyingi qadam shu loyihaning `PROGRESS.md`
faylida.** Quyidagisi faqat qisqacha.

| Plan | Holat |
|---|---|
| **01a** — backend poydevori | ✅ 7 task + rejadan tashqari 6b (rol avtorizatsiyasi) |
| **01b** — sync yadrosi | ✅ 9 task'ning hammasi, deploy fayllari bilan |
| 02 — tdesktop agenti | 🟡 11 task'dan 3 tasi. Vektorlar C++ da tasdiqlandi, sxema v14 qurildi |
| 03 — web controller | ⚪ boshlanmagan |
| 04, 05, 06 | ⚪ boshlanmagan |

### 01b da nima ishlaydi

`seq` cursor (commit tartibi kafolati) · push/pull · tombstone (ikki
yo'nalishli) · kontent-adresli media + kvota · kalit o'ramlari + rate
limiting · keyset pagination + statistika · WebSocket bildirishnoma ·
`.cmx` import/eksport · platformalararo kripto vektorlari.

### 🔴 Plan 02 (tdesktop agenti) boshlashdan oldin bilish shart

- **`record_id` formulasi o'zgargan** — spec §0.12, `account_hash`
  qo'shildi. `activity` kind uchun u **bo'sh satr**.
- **`peer_hash` formulasi o'zgarmagan.**
- **`tombstone` push qilganda `target_record_id` OCHIQ maydonda ham
  yuborilishi shart** — spec §0.13. Server `payload` ni o'qiy olmaydi.
- **`sha256` ochiq matn ustidan**, shifrlashdan OLDIN (§0.5).
- **Sxema versiyasi v13 band** — A17 (`actioned_messages.read_at`).
  `sync_outbox` + `sync_state` migratsiyasi **v14** bo'ladi.
  Batafsil: `../superpowers/specs/2026-08-31-a17-message-read-time-design.md`
- Beshala vektor oilasi .NET da tasdiqlangan — C++ tomoni ham
  `test-vectors.json` ga qarshi tekshirilishi shart.

### tdesktop'da Track C uchun QOLGAN ishlar

1. ✅ Sxema v10 (`account_id`) — qurildi.
2. **`sha256` hisoblash** — `media_index` ga to'ldirish (yangi fayllar
   + mavjud 1543 ta uchun backfill).
3. **Sxema v11** — `sync_outbox` + `sync_state` (v10 endi `account_id`).
4. `CustomSync` moduli — plan 02.

## Kelishilgan qarorlar (qisqacha)

To'liq matn: spec §0.

| № | Qaror |
|---|---|
| 0.1 | `qtwebsockets` alohida quriladi — Qt qayta qurilmaydi ✅ |
| 0.2 | tdesktop sxemasi v9 (sync uchun v10 bo'ladi) |
| 0.3 | Retention: mijozda qabul filtri + serverda uzunroq + tombstone |
| 0.4 | `media_index` sync'ga to'liq kiritiladi |
| 0.5 | `sha256` majburiy, **ochiq matn** ustidan |
| 0.6 | Yangi kind'lar: `media_index`, `tombstone`; manfiy `msg_id` |
| 0.7 | Yagona custom eksport formati (`.cmx`), qo'lda ochish yo'li bilan |
| 0.8 | Arxiv ildizi sozlanadi — yo'llar kodda saqlanmaydi |
| 0.9 | Kvota: mijozda ham, serverda ham |
| 0.10 | `peer_directory` va `PeerNameCache` birlashtirildi |
| 0.11 | Plan 06 — reliz boshqaruvi API orqali |

---

## 🔴 Buzilmaydigan qoidalar

1. **Local-first.** Server o'chsa mijozlar to'liq ishlashda davom etadi.
2. **Imzo lokalda.** Maxfiy kalitlar serverga hech qachon chiqmaydi.
3. **`test-vectors.json` — kontrakt.** Har platforma uni qayta hosil
   qila olishi shart.
4. **Retention tombstone yaratmaydi.** Lokal tozalash global
   o'chirish emas.
5. **`sha256` shifrlashdan OLDIN.** Aks holda dedup butunlay buziladi.
6. **`activity` kind akkauntlar bo'ylab BIRLASHGAN qoladi.** Boshqa
   hamma narsa akkaunt bo'yicha ajratiladi, lekin faollik tarixi
   kuzatilayotgan odam haqidagi obyektiv fakt — kim kuzatganiga
   bog'liq emas. Akkauntlarga bo'lib tashlash last-seen bypass
   qamrovini **buzadi**; aksincha, ko'p akkaunt uni **yaxshilaydi**.
