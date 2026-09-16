# PROMPT — A6: eskidan qolgan ochiq ishlar (tdesktop / CustomMod)

## 0. 🖥️ AVVAL: qaysi kompyuterdasiz (MAJBURIY)

Loyiha laptop va PC'da olib boriladi. Bu promptdagi yo'llar laptop'niki
bo'lishi mumkin.

1. `hostname` ni aniqlang.
2. `<tdesktop>\docs\MACHINES.md` jadvalidan shu kompyuter yo'llarini oling
   (`<tdesktop>` = `C:\TBuild\tdesktop` yoki `D:\Oybek\Telegram\tdesktop`,
   qaysi biri mavjud bo'lsa).
3. Jadvalda yo'q bo'lsa — topib jadvalga yozing va hisobotda ayting.
4. `git fetch origin && git status -sb`; orqada bo'lsa `git pull --ff-only`.
   Ish oxirida hammasini push qiling.

## 1. Buzilmaydigan qoidalar

- **Build qilmang, ilovani ishga tushirmang.** Build'ni faqat foydalanuvchi
  qiladi (~55 daqiqa) va hozir A4+/A5 bilan bitta partiyada kutilmoqda.
- **Jonli bazaga YOZMANG.** Har qanday o'lchov va sinov faqat
  `<ArchiveRoot>\db\actioned_messages.db` ning NUSXASIDA (nusxani vaqtinchalik
  papkaga oling). Jonli bazaga tegadigan skript yozsangiz — uni faqat
  TAYYORLANG, ishga tushirmang; ishga tushirishni foydalanuvchi ilova yopiq
  holda, zaxira nusxa bilan qiladi.
- Push faqat `origin Oybek`. Commit'larda `Co-Authored-By` YO'Q.
- Faqat o'zingiz o'zgartirgan fayllarni stage qiling; `cmake` submodule'ga
  tegmang.
- Upstream fayllarini iloji boricha o'zgartirmang. Agar B1 upstream faylini
  talab qilsa (`data/data_histories.cpp`, `history/history.cpp`) —
  o'zgartirish minimal bo'lsin va izohda `CUSTOM` belgisi bilan sababi
  yozilsin (merge paytida ajratib olish uchun).
- Izohlar o'zbekcha, "nega" ni tushuntiradi, apostrof ASCII `'`.
  Hisobot ham o'zbek tilida.

## 2. Hozirgi holat (kontekst)

Startdagi tezlik ishi tugadi va build kutmoqda:

| Commit | Nima |
|---|---|
| `4a477c8abd` | v17 qamrovchi indeks `actioned_messages(type, account_id, peer_id)` |
| `c27bdccb60` | `RestoreDeletedChats` DB qismi fonga, `PruneStaleActivityHistory` Maintenance'ga |
| `6eb4b30075` | profiling `CUSTOMMOD_PROFILE=1` ortiga, issiq yo'llardagi `PerfScope` lar olib tashlandi |
| `5d42a7aa47` | `RestoreDeletedChats` guard'i asosiy oqimda yasaladigan qilindi (UAF xavfi yopildi) |
| `ac0e0df4d1` | `CompactActivityHistory` peer bo'yicha bo'laklandi (A4) |

Sizning ishingiz ham SHU build partiyasiga qo'shiladi. Shuning uchun
o'zgarishlar kompilyatsiya xavfi past va mantiqan tekshirilgan bo'lishi kerak
— build'da yiqilsa butun partiya 55 daqiqa yo'qoladi.

Manba hujjat: `docs/superpowers/specs/2026-09-11-account-misattribution-incident.md`
§6 (qolgan ishlar 2 va 4).

---

## 3. B1 — `readInboxTill` va inject qilingan elementlar

### Muammo bayoni (hujjatdan)

"Inject qilingan (tiklangan o'chirilgan) elementlarda `isRegular() == false`
bo'lgani uchun o'qish belgisi qo'yilmaydi."

### Ma'lum kod yo'llari (qayta izlamang)

- `Telegram/SourceFiles/data/data_histories.cpp:351` —
  `Histories::readInboxOnNewMessage()`: `!item->isRegular()` bo'lsa
  `readClientSideMessage(item)` ga, aks holda `readInboxTill(..., force=true)` ga
  ketadi.
- `Telegram/SourceFiles/data/data_histories.cpp:257` —
  `readInboxTill(history, tillId, force)`, boshida
  `Expects(IsServerMsgId(tillId) || (!tillId && !force))`.
- `Telegram/SourceFiles/history/history.cpp:2127` —
  `History::readInboxTillNeedsRequest()`, u `readClientSideMessages()` ni
  chaqiradi.
- `History::readClientSideMessages()` faqat `_clientSideMessages` to'plami
  bo'yicha yuradi.
- Bizning inject qilish yo'li: `History::loadDeletedMessages()`
  (`history/history.cpp` ~2217) va `CustomArchive::RestoreDeletedChats()`.

### Talablar

1. **Avval dalil yig'ing, keyin tuzating.** Aniqlang:
   - tiklangan elementlar qaysi yo'l bilan yaratiladi va ular
     `_clientSideMessages` ga TUSHADIMI yoki yo'q;
   - `isRegular()` ularda nima uchun `false`;
   - natijada aynan NIMA buziladi: (a) o'qilmagan sanoq noto'g'ri qoladimi,
     (b) chat "o'qilmagan" bo'lib turaveradimi, (c) `Expects(...)` ishga
     tushadimi, (d) faqat log'da ortiqcha yozuv chiqadimi.
2. **Dalilsiz o'zgartirish kiritmang.** Eski debug loglar
   (`<Release papkasi>\DebugLogs\log_*.txt`) `Reading: ...` qatorlarini
   saqlaydi, lekin ular 2026-08-08 dagi eski nusxalar — ularga tayanish
   shart emas. Agar kod o'qish orqali muammo tasdiqlanmasa, **hech narsani
   o'zgartirmang** va hisobotda "muammo tasdiqlanmadi, sabab: ..." deb
   yozing. Bu ham to'g'ri natija.
3. Tuzatish qilinadigan bo'lsa, u quyidagilarni BUZMASLIGI shart:
   - Ghost Mode mantiqi (`CustomSettings::GhostMode()`, `GetGhostRead`,
     `History::inboxReadTillId()` dagi custom shart) — o'qilmagan xabar
     serverga o'qildi deb YUBORILMASLIGI kerak. Ghost Mode'ning butun
     ma'nosi shu.
   - `Expects()` shartlari: inject qilingan elementning `id` si server ID
     emas, shuning uchun uni `readInboxTill()` ga UZATMANG.
4. Eng ehtimolli to'g'ri yechim (tekshiring, ko'r-ko'rona bajarmang):
   tiklangan elementlarni `_clientSideMessages` orqali ro'yxatdan o'tkazish
   yoki o'qilmagan sanoqni hisoblashda ularni chetlab o'tish. Server
   so'roviga yangi yo'l OCHMANG.

### Qabul mezoni

- Muammo kod bo'yicha aniq tushuntirilgan (qaysi funksiya, qaysi qator).
- Yoki minimal tuzatish, yoki asoslangan "o'zgartirish kerak emas" xulosasi.
- Ghost Mode va `Expects()` shartlari buzilmagan.

---

## 4. B2 — legacy `account_id = 0` yozuvlari

### Muammo bayoni

Multi-akkaunt izolyatsiyasidan (v10) oldingi yozuvlarda `account_id = 0`.
O'qish filtri `kAccountFilterSql = "account_id IN (0, ?)"`
(`custom_db.cpp:185`) bo'lgani uchun bu yozuvlar HAR akkauntda ko'rinadi.

Hujjatdagi raqamlar (2026-09-11): 824 ta `deleted` (154 peer), 14 110 backup,
`text_cache` da 8449 legacy.

### Talablar

1. **Avval o'lchang** (bazaning NUSXASIDA, bugungi holat bo'yicha):
   jadval kesimida nechta `account_id = 0` qator bor, nechta peer,
   ularning `timestamp` oralig'i qanday.
2. Har bir legacy peer uchun egasini aniqlash MUMKINMI yoki yo'qligini
   ayting. Taklif qilinadigan mezon (tekshiring va kengaytiring): o'sha
   `peer_id` bo'yicha `account_id != 0` qatorlar mavjud bo'lsa va ular
   FAQAT bitta akkauntga tegishli bo'lsa — egasi aniq. Bir nechta akkaunt
   chiqsa — noaniq, TEGMANG.
3. **Natija: bajariladigan skript + hisobot**, jonli bazaga yozish EMAS.
   Skript talablari:
   - ishga tushishdan oldin ilova yopiqligini tekshiradi va zaxira nusxa
     oladi;
   - `undo` log yozadi (qaysi id qaysi qiymatdan qaysi qiymatga);
   - aniq egasi topilgan qatorlarni YANGILAYDI, noaniqlarini TEGMAYDI;
   - oxirida oldin/keyin sanoqlarini chiqaradi.
   Skriptni nusxada ishga tushirib, natijani hisobotga yozing.
4. Kod tomoni: `account_id IN (0, ?)` filtrini olib tashlashni HOZIR
   TAKLIF QILMANG. U legacy qatorlar bor ekan kerak. Migratsiyadan keyin
   nechta legacy qatorlar qolishini hisoblang va "filtr qachon olib
   tashlanishi mumkin" degan shartni hujjatga yozing.
5. Agar o'lchov ko'rsatsaki, legacy qatorlarning egasi deyarli
   aniqlanmaydi — buni ayting va migratsiyani tavsiya qilmang.

### Qabul mezoni

- Bugungi aniq raqamlar (taxmin emas, o'lchov).
- Tayyor, lekin ishga tushirilMAGAN skript + nusxada olingan natija.
- Filtrni qachon olib tashlash mumkinligi hujjatlashtirilgan.

---

## 5. B3 (ixtiyoriy) — `ActivityCacheLoad`

Rejada 162–175 ms deb qayd etilgan va "qabul qilinadigan" deb belgilangan.
Faqat bazaning nusxasida o'lchang. **150 ms dan oshsa** — `CompactActivityHistory`
dagi kabi bo'laklang (namuna: `custom_db.cpp`, `CompactActivityHistory`,
peer bo'yicha ~50 ms budjet bilan). Oshmasa — tegmang va o'lchovni hisobotga
yozing.

---

## 6. Build'siz tekshirish (majburiy)

Build qila olmaysiz, shuning uchun kompilyatsiya xavfini o'zingiz kamaytiring:

- O'zgartirgan har bir faylda qavslar balansini va `#include` larni
  tekshiring (yangi tur ishlatsangiz — sarlavha qo'shilganmi).
- `git grep` bilan: olib tashlagan yoki nomini o'zgartirgan har bir
  belgining boshqa chaqiruvlari qolmaganini tasdiqlang.
- SQL o'zgartirsangiz — uni bazaning nusxasida ishga tushirib, natija
  eski so'rov bilan AYNAN bir xil ekanini isbotlang (id'lar to'plamini
  solishtiring, faqat sanoqni emas).

## 7. Yakun

1. Har vazifa uchun **alohida commit** (`B1`, `B2`, `B3` aralashmasin).
2. `docs/superpowers/plans/2026-09-13-peak-performance-and-stability-plan.md`
   §A6 belgilarini yangilang; `docs/superpowers/PROJECTS.md` dagi "keyingi
   qadam" ni yangilang. customsync-server hujjatlariga TEGMANG — u alohida
   repo va alohida sessiya.
3. `origin Oybek` ga push qiling.
4. Hisobotda: har vazifa bo'yicha nima qilindi, qanday DALIL bilan,
   qaysi commit, nimalar ataylab qilinmadi. "Tuzatilmadi, chunki ..." ham
   to'liq javob hisoblanadi.
