# A13 — AntiDelete arxivini mustahkamlash (dizayn)

**Holat:** Tasdiqlangan (2026-08-13, brainstorming orqali)
**Tegishli:** `docs/superpowers/PROJECTS.md` A13 qatori

---

## 1. Muammo (qisqacha, Phase 1 natijasi)

2026-08-13 da suhbatdosh ("Xurshida | V", peer `7815103103`) butun chat
tarixini o'chirdi. CustomMod atigi **90 ta xabarni** saqlab qoldi —
hammasi o'sha kunning 15:24–17:01 oralig'idan (~1.5 soatlik "dum"),
holbuki chat tarixi kamida 2026-07-17 gacha borardi. Qolgan hamma narsa
jimgina yo'qoldi.

Bundan ham yomoni: **saqlangan 90 tasi ham ilovada ko'rinmadi.**
Screenshot bilan tasdiqlangan — chat ro'yxatda o'z joyida turibdi,
ichida esa "No messages here yet…" yozuvi, DB'da esa o'sha paytda 104 ta
xabar mavjud edi.

### 1.1 Aniqlangan nuqsonlar (kod + DB + registry dalillari bilan)

| № | Nuqson | Joy | Ta'sir |
|---|---|---|---|
| D1 | `if (isEmpty()) return;` — butun chat o'chirilganda `blocks` bo'shaydi, funksiya darhol chiqadi | `history.cpp:2186` | Saqlangan xabarlar DB'da bo'lsa ham **ko'rinmaydi** |
| D2 | `CacheMessageText()` butun kodbazada FAQAT bitta joydan chaqiriladi, `NewMessageType::Unread` sharti bilan | `data_session.cpp:3558-3576` | Scrollback orqali yuklangan **eski tarix hech qachon arxivlanmaydi** |
| D3 | `ShouldBackgroundCache()` global `antiDelete` bayrog'ini hisobga olmaydi, `ShouldAntiDelete()` esa oladi | `custom_settings.cpp:508-538` | Faqat global tugmaga tayangan foydalanuvchida AntiDelete "yoqilgan" ko'rinadi, fon-cache esa ishlamaydi |
| D4 | `PruneStaleCachedText(30)` — 30 kunlik TTL, kuzatilayotgan/kuzatilmayotgan chatni farqlamaydi | `custom_db.cpp:801-814, 848-852` | To'liq arxivga o'tilsa **eski arxivni o'chirib yuboradi** |
| D5 | `journal_mode=WAL` + `synchronous=NORMAL` | `custom_db.cpp:144-145` | To'satdan tok o'chganda oxirgi tranzaksiyalar yo'qolishi mumkin |
| D6 | O'lik registry kalitlari: `anti_delete`, `anti_edit`, `ghost_mode`, `bypass_restrictions`, `offline_db` (snake_case) — kod ularni o'qimaydi | `custom_settings.cpp:200` faqat camelCase o'qiydi | Foydalanuvchi sozlama yoqilgan deb o'ylaydi, aslida yo'q |
| D7 | `CustomDB::GetDeletedMessages()`ning butun kodbazada bitta chaqiruvchisi bor (`loadDeletedMessages`) | — | Yagona nosozlik nuqtasi: D1 ishlamasa, ma'lumotga umuman yo'l yo'q |

### 1.2 Empirik o'lchovlar (real DB, read-only)

- DB: `%APPDATA%\TelegramDesktop\CustomMod\actioned_messages.db`, 155 MB
- `actioned_messages` 268 286 qator, `text_cache` 16 149, `activity_history` 238 730
- O'rtacha xabar matni: `text_cache` 318 bayt, `actioned_messages` 176 bayt
- → 10 000 xabarli chatning to'liq arxivi ≈ **3 MB**
- `text_cache` yo'nalish taqsimoti: `is_out=0` → 13 271, `is_out=1` → 2 880
  (ya'ni chiquvchi xabarlar **qisman** tushadi — boshqa qurilmadan
  yuborilganlar `updateNewMessage` orqali keladi, shu klientdan
  yuborilganlar esa yo'q)

---

## 2. Maqsad va asosiy prinsip

**Maqsad:** AntiDelete arxivi hech qanday ma'lumot yo'qotmasin, va butun
chat o'chirilganda ham xabarlar chat ichida "o'chirilgan" belgisi bilan
ko'rinib tursin.

**Prinsip (arxitekturaviy o'zgarish):**

> Arxiv **o'chirilish hodisasiga** tayanmasin. Xabar ko'rilgan/yuklangan
> zahoti yoziladi; o'chirilish esa faqat mavjud yozuvga *belgi qo'yish*
> bo'ladi.

Hozirgi tizim teskari ishlaydi — o'chirilish paytida xabarni qutqarishga
urinadi (xotiradan yoki ingichka cache'dan), va aynan shu sabab
butun-chat o'chirilishida ish bermaydi.

---

### 2.1 Atama: "kuzatilayotgan chat"

Bu hujjatda **kuzatilayotgan chat** —
`CustomSettings::ShouldBackgroundCache(peerId)` `true` qaytaradigan
chat. K5 tuzatilgandan keyin bu quyidagi ustuvorlik zanjiri bo'ladi:
Blocklist → `false`; Whitelist (yoki whitelist-kategoriya) → `true`;
per-peer override → o'sha qiymat; aks holda global `antiDelete` bayrog'i.

### 2.2 Bosqichlar (implementatsiya tartibi)

Vazifalar shu tartibda bajariladi — har bosqich o'zicha qiymat beradi:

| Bosqich | Komponent | Nega shu tartibda |
|---|---|---|
| 1 | **K1** (ko'rsatish) | Eng katta va eng tez qiymat: allaqachon saqlangan 104 ta xabar darhol ko'rinadigan bo'ladi. Boshqa komponentlarga bog'liq emas. |
| 2 | **K5** + **K6** (sozlama + saqlanish) | To'g'rilik poydevori. K2 ulardan oldin kelsa, noto'g'ri gate bilan arxivlab, keyin pruning o'chirib yuborishi mumkin. |
| 3 | **K2** (arxiv qamrovi) | Asosiy tuzatish. K5/K6 tayyor bo'lgach xavfsiz. |
| 4 | **K3**, **K4** (qo'lda zaxira, media) | Yangi funksiyalar — mavjud nuqsonlar tuzatilgandan keyin qo'shiladi. |

Build **bitta marta**, hammasi tugagach (A6 Qt6 va A11 Task 6 bilan
birga) — resurs tejash qoidasiga muvofiq.

---

## 3. Komponentlar

### K1 — Ko'rsatish: bo'sh chatga ham inject qilish 🔴

**Muammo:** `History::loadDeletedMessages()` boshidagi
`if (isEmpty()) return;` (`isEmpty()` = `blocks.empty()`).

**Bu qatorni shunchaki o'chirib bo'lmaydi.** U haqiqiy invariantni
himoya qilyapti — mavjud izoh aniq aytadi: konstruktordan chaqirilsa
`insertMessageToBlocks → addNewToBack → addItemToBlock` zanjiri buziladi.

**Yechim:** shart *"chat bo'shmi"* dan *"biz xavfsiz bosqichdamizmi"* ga
almashtiriladi.

1. `History` klassiga `bool _deletedInjectionReady = false;` maydoni
   qo'shiladi.
2. U birinchi `addOlderSlice()`/`addNewerSlice()` **tugagach** `true`
   bo'ladi (konstruktor bosqichi allaqachon ortda qolgan bo'ladi).
3. `loadDeletedMessages()` boshidagi shart:
   `if (!_deletedInjectionReady) return;` — `isEmpty()` o'rniga.
4. Bo'sh tarixga birinchi elementni qo'yish uchun tdesktop'ning **o'z
   mavjud yo'li** qayta ishlatiladi — `insertJoinedMessage()` bo'sh
   holatni qanday hal qilsa, xuddi shu naqsh (implementator o'sha
   funksiyani o'qib, blok yaratish qismini takrorlaydi; yangi mexanizm
   ixtiro qilinmaydi).
5. Server bo'sh javob qaytarganda inject **qayta** bajariladi — aks holda
   tdesktop tarixni qayta so'rab, qo'yganimizni o'chirib yuboradi.

**Kutilgan natija:** chat ro'yxatda qoladi, ichida xabarlar
"o'chirilgan" belgisi bilan ko'rinadi (foydalanuvchi kutgan
"aka messenger" xatti-harakati).

### K2 — Arxiv qamrovi: "ilova ko'rgan hamma narsa" 🔴

Yangi fayl **`Telegram/SourceFiles/custom_archive.cpp` + `.h`**, ichida
yagona ommaviy funksiya:

```cpp
namespace CustomArchive {
// Xabarni arxivga yozadi, agar shu peer kuzatilayotgan bo'lsa.
// Idempotent: bir xil xabarni qayta yozish xavfsiz (INSERT OR REPLACE).
void MaybeArchiveItem(not_null<HistoryItem*> item);
} // namespace CustomArchive
```

Mantiq bitta joyda turadi, chaqiruv nuqtalari ingichka bo'ladi:

| Chaqiruv nuqtasi | Nimani qamraydi | Hozirgi holat |
|---|---|---|
| `Session::addNewMessage()` | real-vaqtda kelgan xabarlar | ✅ bor — mavjud inline kod helper'ga ko'chiriladi |
| `History::addOlderSlice()` | **scrollback — eski tarix** | 🆕 yo'q edi (D2) |
| `History::addNewerSlice()` | oraliqni to'ldirish | 🆕 yo'q edi (D2) |
| Yuborish yo'li — server tasdig'idan keyin | shu klientdan yuborilgan xabarlar | 🆕 yo'q edi |

**Muhim:** `Session::registerMessage()` yagona umumiy nuqta bo'lsa-da,
**hook sifatida ishlatilmaydi** — u `HistoryItem` konstruktoridan
chaqiriladi va o'sha paytda xabar hali to'liq qurilmagan bo'lishi mumkin
(matn/media hali o'rnatilmagan). Yuqoridagi 4 ta nuqta esa xabar to'liq
tayyor bo'lgan joylar.

### K3 — Qo'lda "to'liq zaxira"

Chat menyusiga tugma: **"Bu chatning butun tarixini arxivla"**.
Sahifalab (`messages.getHistory`) serverdan yuklaydi va arxivga yozadi.
Scrollback'ni kutmasdan, muhim chatni bir marta to'liq himoyalash uchun.

Jarayon fon rejimida, progress ko'rsatkichi bilan; bekor qilish mumkin.

### K4 — Media

Kuzatilayotgan chatlarda media (foto/video/ovoz) **avtomatik fon
rejimida yuklab olinadi** va mavjud `CustomDB::SaveMediaFile()` orqali
`~/customizationMainFolder/medias/…` ichiga saqlanadi.

Shu bilan ko'rilmagan/ochilmagan media ham butun-chat o'chirilishida
qutqariladi. Trafik va disk sarfi sezilarli ortadi — bu foydalanuvchi
tomonidan ongli tanlangan.

Mavjud `data_document.cpp:1046-1075` `finishLoad()` hook'i saqlanadi
(u yuklab olingan faylni ko'chiradi); yangi qism faqat **yuklashni
boshlash** bo'ladi.

### K5 — White/black/tanlangan list aniq ishlashi

1. `ShouldBackgroundCache()` `ShouldAntiDelete()` bilan **bir xil
   ustuvorlik zanjiriga** keltiriladi:
   `Blocklist → false` > `Whitelist → true` > per-peer override >
   **global bayroq** (hozir oxirgi bosqich yo'q — D3).
2. O'lik snake_case registry kalitlari (D6) migratsiya qilinadi: agar
   camelCase varianti mavjud bo'lmasa, snake_case qiymati bir marta
   ko'chiriladi, keyin eski kalit o'chiriladi.
3. Custom Window'da har bir chat uchun **haqiqiy holat** ko'rsatiladi:
   "kuzatilyapti / kuzatilmayapti" va **sababi** (qaysi qoida ishladi:
   blocklist / whitelist / per-peer / global). Foydalanuvchi endi
   taxmin qilmaydi.

### K6 — Saqlanish kafolati (retention + durability)

1. **Pruning (D4):** `PruneStaleCachedText()` kuzatilayotgan chatlarga
   **tegmaydi**. Faqat kuzatilmayotgan peerlarning yozuvlari eskirganda
   o'chiriladi. (To'liq arxivda 30 kunlik TTL halokatli bo'lardi.)
2. **Durability (D5) — qaror:** `synchronous=NORMAL` **saqlanadi**
   (`FULL`ga o'tish har bir yozuvni sekinlashtiradi, arxiv esa endi
   ancha ko'proq yozadi). Uning o'rniga:
   - ilova yopilishida `PRAGMA wal_checkpoint(TRUNCATE)`;
   - bo'sh vaqtda davriy checkpoint (har ~5 daqiqada, faqat oxirgi
     checkpoint'dan keyin yozuv bo'lgan bo'lsa).

   Sabab: WAL + NORMAL'da yo'qolish xavfi faqat oxirgi checkpoint'dan
   keyingi tranzaksiyalarga tegishli; muntazam checkpoint bu oynani
   daqiqalargacha qisqartiradi, yozuv tezligini esa saqlab qoladi.
3. Custom Window'da DB hajmi va arxiv statistikasi ko'rsatiladi.

---

## 4. Regressiya xavfsizligi (foydalanuvchining aniq sharti)

> "Bu ishlar boshqa qism features'ga salbiy ta'sir qilmasligi kerak."

Bu bo'lim **majburiy** — tegiladigan fayllar boshqa funksiyalar bilan
baham ko'riladi.

### 4.1 Ta'sir doirasidagi mavjud funksiyalar

| Fayl | Unda yashaydigan boshqa funksiyalar |
|---|---|
| `history/history.cpp` | **GhostMode** (7 ta joyda `CustomSettings::GhostMode()`), o'qilgan-belgisi mantig'i, `loadDeletedMessages()` |
| `data/data_session.cpp` | AntiDelete o'chirish yo'llari, AntiEdit, `processMessagesDeleted`, `processNonChannelMessagesDeleted` |
| `data/data_document.cpp` | AntiDelete media zaxirasi (`finishLoad()`) |
| `custom_settings.cpp` | GhostMode, AntiEdit, Story anonim ko'rish, Activity History, mutual-contact, upstream checker |
| `custom_db.cpp` | Activity History (A11), ghost reads, backup/export, media saqlash |

### 4.2 Qat'iy qoidalar

1. **O'chirilgan holatda bit-darajasida bir xil xatti-harakat.** Har bir
   yangi hook `CustomSettings::ShouldBackgroundCache(peerId)` (yoki
   tegishli gate) bilan o'ralgan bo'lishi shart. Gate `false` bo'lganda
   kod yo'li hozirgidan farq qilmasligi kerak.
2. **UI oqimini bloklamaslik — qaror:** `addOlderSlice()`/
   `addNewerSlice()` scroll paytida ishlaydigan **issiq yo'l**. Har bir
   xabar uchun alohida SQLite tranzaksiyasi jank keltirib chiqaradi.
   Shuning uchun `MaybeArchiveItem()` bitta xabarni darhol yozmaydi,
   balki **partiyaga qo'shadi**; partiya bitta tranzaksiyada
   (`BEGIN … COMMIT`) yoziladi — slice tugaganda yoki partiya
   ~200 xabarga yetganda, qaysi biri avval bo'lsa.
   Mavjud real-vaqt yo'li (`addNewMessage`) esa bittalab yozaveradi —
   u sekundiga bir necha xabardan oshmaydi, o'zgartirish shart emas.
3. **GhostMode mantig'iga tegilmaydi.** `history.cpp`dagi o'zgarishlar
   faqat `loadDeletedMessages()` va slice-yakuni hook'lari bilan
   cheklanadi.
4. **DB sxemasi orqaga mos.** Yangi ustun/jadval qo'shilsa,
   `schema_version` orqali migratsiya qilinadi; eski DB fayli
   ochilaverishi kerak (foydalanuvchida 155 MB real ma'lumot bor).
5. **AutoBackup'lar buzilmasin.** `CustomDB` export/import yo'llari
   (`AutoBackups/`) yangi sxema bilan ham ishlashi tekshiriladi.
6. **A11 (story signal) va Activity History'ga tegilmaydi.** Ular
   `activity_history` jadvalidan foydalanadi — bu ish `text_cache` va
   `actioned_messages` bilan cheklanadi.

### 4.3 Tekshirish

Har bir vazifadan keyin — mavjud AntiDelete/AntiEdit/GhostMode
xatti-harakati o'zgarmaganini tasdiqlash. Yakuniy build'dan keyin
to'liq qo'lda tekshiruv ro'yxati (§6).

---

## 5. Ko'lam tashqarisida

- **Mustaqil viewer oynasi** — foydalanuvchi chat ichida ko'rsatishni
  tanladi; screenshot chat ro'yxatda qolishini tasdiqladi, shuning uchun
  alohida oyna kerak emas (YAGNI).
- **A6 (Qt6 migratsiyasi)** — alohida vazifa, bu ish undan mustaqil.
- **Track C (customsync-server)** — keyinroq.
- **Linux/macOS** — hozirgi barcha ish Windows'da tekshiriladi.

---

## 6. Qo'lda tekshirish rejasi (yakuniy build'dan keyin)

1. **K1:** "Xurshida | V" chatini ochish — 104 ta saqlangan xabar
   "o'chirilgan" belgisi bilan ko'rinadimi?
2. **K1:** ilovani qayta ishga tushirib, o'sha chat yana to'g'ri
   ko'rsatilishini tasdiqlash (server bo'sh javobi ustidan yashab
   qolish).
3. **K2:** kuzatilayotgan chatda yuqoriga scroll qilib eski tarixni
   yuklash → DB'da `text_cache` qatorlari ortganini tasdiqlash.
4. **K2:** shu klientdan xabar yuborish → arxivga tushganini tasdiqlash.
5. **K3:** "Butun tarixni arxivla" tugmasi bir chatda ishlashini
   tasdiqlash.
6. **K4:** kuzatilayotgan chatga media kelganda, uni **ochmasdan**,
   `~/customizationMainFolder/medias/` ichida paydo bo'lishini
   tasdiqlash.
7. **K5:** Custom Window'da chat holati va sababi to'g'ri
   ko'rsatilishini tasdiqlash; global tugma bilan ham fon-cache
   ishlashini tasdiqlash (D3 tuzatilgani).
8. **K6:** 30 kundan eski arxiv yozuvlari kuzatilayotgan chatda
   saqlanib qolganini tasdiqlash.
9. **Regressiya:** GhostMode, AntiEdit, Activity History (A11),
   self-update, mutual-contact indikatori — hammasi avvalgidek
   ishlayotganini tasdiqlash.
10. **Performans:** uzun chatda tez scroll qilganda sekinlashuv
    (jank) yo'qligini tasdiqlash.

---

## 7. Ma'lumotlarni tiklash (bajarilgan)

Nuqsonlar tuzatilishidan oldin ham, "Xurshida | V" chatining saqlanib
qolgan **104 ta xabari** DB'dan chiqarib olindi
(`deleted` + `text_cache` + `backup` manbalarini birlashtirib,
2026-07-17 11:14 … 2026-08-13 17:01 oralig'i) va foydalanuvchiga
markdown fayl sifatida topshirildi. Bu K1 tuzatilgach ilovaning o'zida
ham ko'rinadi.
