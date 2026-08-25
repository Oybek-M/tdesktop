# Loyihalar holati — HAR SESSIYA SHU YERDAN BOSHLANADI

Oxirgi yangilanish: **2026-08-25**

> Bu fayl bir nechta loyiha o'rtasida "kim nimani bajardi" ni
> ko'rsatadi. Sessiya boshida o'qing, oxirida yangilang.

---

## Umumiy holat

| Loyiha | Holat | Keyingi qadam |
|---|---|---|
| **tdesktop** (CustomMod) | 🟢 v7.1.1 chiqarildi, ishlab turibdi | Sync agenti (plan 02) |
| **customsync-server** | ⚪ boshlanmagan | Plan 01a Task 1 — solution skeleti |
| **server-controller** | ⚪ boshlanmagan | 01a/01b tugagach |
| **tmobile-android** | ⚪ muhokama qilinmagan | — |
| **tmobile-ios** | ⚪ muhokama qilinmagan | — |

---

## tdesktop — protokolga tegishli holat

| Nima | Holat |
|---|---|
| `qtwebsockets` moduli | ✅ qurildi va sinovdan o'tdi (2026-08-25) |
| Sxema versiyasi | **v9** (sync jadvallari HALI YO'Q) |
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
| Placeholder qayta arxivlanardi | ASL MATN o'rniga ko'rsatish markeri saqlanardi (12 yozuv). v9 migratsiyasi tozalaydi |

⚠️ **Sync uchun muhim:** eski (2026-08-24 dan oldingi) rasmlar
arxivda YO'Q. Ular `media_index` da ham yo'q, ya'ni sync ularni
"yo'qolgan" deb ham ko'rsata olmaydi. Bu qabul qilingan
yo'qotish — tiklab bo'lmaydi.

### tdesktop'da Track C uchun QOLGAN ishlar

1. **`sha256` hisoblash** — `media_index` ga to'ldirish
   (yangi fayllar + mavjud 1543 ta uchun backfill)
2. **Sxema v10** — `sync_outbox` + `sync_state`
   ⚠️ v9 allaqachon band (placeholder tozalash migratsiyasi)
3. `CustomSync` moduli — plan 02 ning qolgan qismi

---

## customsync-server — boshlanish nuqtasi

**Papka:** `Projects programming\Telegram\customsync-server`

**Tartib:** 01a → 01b → 02 → 03 → 04 → 05 → 06

⚠️ **Implement boshlashdan OLDIN** spec **§0 REVIZIYA** ni o'qing —
11 ta qaror bor va ular asosiy matndan ustun turadi.

---

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
