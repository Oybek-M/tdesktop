# A17 — arxivlangan xabarning o'qilgan vaqtini saqlash

**Sana:** 2026-08-31
**So'ragan:** foydalanuvchi
**Holat:** 🔴 OCHIQ — **YUQORI USTUVORLIK**
**Tegishli:** tdesktop-client **VA** customsync-server (ikkala loyihada ham)
**Bog'liq:** A14 (o'qilgan vaqt orqali faollik), A16 (faollik tarixi)

---

## 1. Muammo

AntiDelete va AntiEdit xabarni saqlab qoladi, lekin **o'qilgan vaqt
saqlanmaydi**. Xabar o'chirilgach, u qachon o'qilgani haqidagi
ma'lumot butunlay yo'qoladi.

Bu ikki sababga ko'ra muhim:

1. **Arxiv to'liqligi** — saqlangan xabar yonida "qachon o'qilgan"
   turishi tabiiy.
2. **A14 signali** — suhbatdosh xabarni o'qigan lahza, u FAOL
   bo'lganining isboti. A14 hujjatiga ko'ra bu last-seen'dan
   **mustaqil va aniqroq**: bitta kontaktda last-seen 18:31
   ko'rsatgan, o'qilgan vaqt esa 19:35 bo'lgan.

## 2. Hozirgi holat

`actioned_messages` jadvalida o'qilgan vaqt uchun **ustun yo'q**:

```
id, peer_id, msg_id, type, original_text, new_text, media_path,
is_out, msg_date, timestamp, notes, sender_id, is_media, account_id
```

`ghost_reads` jadvali boshqa narsa — u BIZNING ghost-rejimdagi o'qish
holatimizni saqlaydi, suhbatdoshnikini emas.

## 3. Ma'lumot manbalari

### 3.1 Real-vaqt signali (ASOSIY — bepul, API so'rovisiz)

`History::outboxRead(MsgId upTo)` — `history.h:222`.
`updateReadHistoryOutbox` update'idan chaqiriladi
(`api_updates.cpp:1421` va `1854`).

Bu ishga tushganda: shu chatdagi `upTo` gacha bo'lgan **barcha
chiquvchi** xabarlar **aynan shu lahzada** o'qilgan. Ya'ni vaqtni
so'ramasdan bilamiz.

Bu eng qimmatli manba, chunki:
- API so'rovi kerak emas (limit yo'q)
- Aniq lahzani beradi
- A14 uchun ham to'g'ridan-to'g'ri signal

### 3.2 Talab bo'yicha so'rov (ZAXIRA)

`messages.getOutboxReadDate#8c4bfe5d peer:InputPeer msg_id:int`
— `api.tl:2683`. Tayyor, ishlaydigan naqsh:
`api/api_who_reacted.cpp:290`.

Barcha xato holatlari o'sha yerda hal qilingan:

| Xato | Ma'nosi |
|---|---|
| `YOUR_PRIVACY_RESTRICTED` | Mening maxfiylik sozlamam to'sqinlik qilmoqda |
| `USER_PRIVACY_RESTRICTED` | Suhbatdoshning maxfiyligi |
| `MESSAGE_TOO_OLD` | Xabar juda eski — server bilmaydi |

### 3.3 🔴 Eng muhim cheklov

**`MESSAGE_TOO_OLD` borligi shuni anglatadiki, o'qilgan vaqtni
xabar O'CHIRILGANDA so'rash KECH.** O'sha paytda server allaqachon
"bilmayman" deyishi mumkin.

Shuning uchun asosiy yig'ish **real-vaqtda** (3.1) bo'lishi shart.
So'rov (3.2) faqat to'ldiruvchi.

### 3.4 Qamrov chegarasi

`getOutboxReadDate` faqat **shaxsiy chatdagi CHIQUVCHI** xabarlar
uchun ishlaydi (`user->input()` talab qiladi).

- Guruhlar uchun `messages.getMessageReadParticipants` bor
  (kim o'qigani, vaqti bilan) — **v1 ga KIRMAYDI**, keyinroq.
- Kiruvchi xabarlar (biz o'qiganimiz) — bu boshqa ma'lumot,
  qiymati past. **v1 ga kirmaydi.**

v1 qamrovi: **shaxsiy chat + chiquvchi xabar.**

## 4. Sxema o'zgarishi (v13)

`actioned_messages` ga bitta ustun:

```sql
ALTER TABLE actioned_messages
    ADD COLUMN read_at INTEGER NOT NULL DEFAULT 0
```

Qiymat ma'nosi (bitta ustunda, qo'shimcha migratsiyasiz):

| Qiymat | Ma'nosi |
|---|---|
| `0` | Noma'lum — hali aniqlanmagan |
| `> 0` | Unix timestamp — o'qilgan lahza |
| `-1` | Maxfiylik cheklovi (`*_PRIVACY_RESTRICTED`) |
| `-2` | Xabar juda eski (`MESSAGE_TOO_OLD`) |

Manfiy qiymatlar "urinib ko'rdik, bo'lmadi" degani — shunda tizim
bir xil xabar uchun qayta-qayta so'rov yubormaydi.

> ⚠️ **Sxema v13 shu ish uchun band bo'ladi.** Track C ning
> `sync_outbox` + `sync_state` jadvallari **v14** ga suriladi.
> `docs/sync-protocol/STATUS.md` yangilanishi SHART.

## 5. Ko'rsatish

Arxiv oynasida (`fillArchiveTab`) har yozuv yonida:

| `read_at` | Ko'rinishi |
|---|---|
| `> 0` | `✓✓ o'qilgan: 24.08.2026 19:35` |
| `0` | hech narsa ko'rsatilmaydi |
| `-1` | `✓✓ o'qilgan vaqti yashirilgan` |
| `-2` | hech narsa ko'rsatilmaydi (shovqin) |

## 6. A14 bilan bog'liqlik

Bu ish A14 ni **almashtirmaydi**, unga poydevor qo'yadi:

- **A17 (bu ish):** o'qilgan vaqtni ushlash va xabar yonida saqlash
- **A14 (keyin):** o'sha vaqtni `activity_history` ga `status`
  nuqtasi sifatida qo'shish (`source = 'read'`)

A16 §1 dagi story naqshi aynan shunday ishlaydi — o'sha yo'l
takrorlanadi. **A17 tugagach A14 arzon ishga aylanadi.**

## 7. customsync-server tomoni

`record_id` hisoblanishiga **ta'sir qilmaydi** — `read_at` mavjud
yozuvning maydoni, yangi kind emas.

Lekin sync qamroviga kirishi kerak:

- `message` kind'ining payload'iga `read_at` qo'shiladi
- Konflikt qoidasi: **kattaroq mutlaq qiymat yutadi**
  (0 = ma'lumot yo'q, har qanday aniq qiymat undan ustun;
  ikkala tomonda ham musbat bo'lsa — kichikrog'i, ya'ni
  BIRINCHI o'qilgan lahza to'g'ri)
- Bir qurilma vaqtni bilsa, boshqasi bilmasa — bilgan tomon yutadi

Bu qoida `docs/sync-protocol/` ga yozilishi kerak.

## 8. v1 ga KIRMAYDIGAN narsalar (YAGNI)

- Guruh xabarlari (`getMessageReadParticipants`)
- Kiruvchi xabarlarning o'qilgan vaqti
- Eski, allaqachon arxivlangan xabarlar uchun backfill
  (`MESSAGE_TOO_OLD` tufayli baribir ishlamaydi)
- A14 ning o'zi (alohida ish)
