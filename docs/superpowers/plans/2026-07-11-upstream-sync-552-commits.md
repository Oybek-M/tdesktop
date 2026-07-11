# Upstream tdesktop bilan sinxronlash (552 commit) — Reja

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Sana**: 2026-07-11
**Status**: REJALASHTIRISH — implementatsiya HALI BOSHLANMAGAN (user aniq so'radi: avval reja, keyin implement)
**Prioritet**: O'RTA — shoshilinch emas, lekin qanchalik kech qolsak shunchalik og'irlashadi

---

## Qisqacha xulosa va tavsiya

**Tavsiyam: HA, sync qilish kerak, lekin BIR YO'LA emas — bosqichma-bosqich, alohida branch/worktree'da, build+regression test bilan tasdiqlangandan keyingina `Oybek`ga qo'shish.**

Sabab: 11 kunda (2026-06-30 dan beri) upstream 552 commit oldinga ketgan —
bu tdesktop qanchalik faol rivojlanayotganini ko'rsatadi (xavfsizlik
patchlari, MTProto protokol o'zgarishlari, Qt yangilanishlari shular
ichida bo'lishi mumkin). Qancha kech qolsak, keyingi sync shuncha og'ir
bo'ladi — shuning uchun butunlay kechiktirish yomon strategiya. Lekin bir
martada 552 commit + 379 fayl + bizning 24 ta eng nozik custom fayl bilan
to'qnashuvni hal qilish — bitta katta, xavfli, qiyin bekor qilinadigan
operatsiya bo'lardi. Bosqichma-bosqich (kichikroq partiyalarda) qilish
xavfni sezilarli kamaytiradi va muammo chiqsa aniq qaysi bosqichda
ekanini bilish imkonini beradi.

---

## Joriy holat (2026-07-11, `git fetch upstream` orqali tekshirildi)

| Metrika | Qiymat |
|---|---|
| Bizning branch | `Oybek` (`origin` = `Oybek-M/tdesktop.git`) |
| Upstream branch | `upstream/dev` (`telegramdesktop/tdesktop.git`) |
| Umumiy ota-commit (merge-base) | `7f8c690d85` — **2026-06-30** |
| Biz upstream'dan orqada | **552 commit** |
| Bizning o'zimizning commit'larimiz | **49 commit** (shu jumladan shu sessiyadagi 10+ ta CustomMod v2.0 commit) |
| Upstream'da o'zgargan fayllar | **379 ta** (`Telegram/SourceFiles` ichida) |
| Bizda o'zgargan fayllar | **56 ta** |
| **Ikkalasida ham o'zgargan fayllar (to'qnashuv xavfi)** | **24 ta** — pastga qarang |
| Upstream diff hajmi | +49,036 / -8,419 qator |
| `lib_ui` submodule | Bizda ham, upstream'da ham merge-base'dan farqli commit'ga ishora qiladi — ALOHIDA hal qilish kerak |

### 24 ta to'qnashuv-xavfli fayl (ikkala tomon ham o'zgartirgan)

Bular aynan bizning eng chuqur customization qilingan joylarimiz —
diqqat markazida bo'lishi kerak:

```
api/api_sending.cpp                          data/data_user.cpp
api/api_updates.cpp                          history/history.cpp / .h
apiwrap.cpp                                  history/history_inner_widget.cpp
boxes/peers/edit_peer_info_box.cpp           history/history_item.cpp / .h
chat_helpers/message_field.cpp               history/history_item_helpers.cpp
core/application.cpp                         history/view/history_view_element.cpp / .h
core/sandbox.cpp                             history/view/history_view_list_widget.cpp
data/data_channel.cpp                        info/media/info_media_grid_zoom.cpp
data/data_histories.cpp                      main/main_account.cpp
data/data_peer.cpp                           window/main_window.cpp
data/data_session.cpp
```

**Nega bu xavfli**: `apiwrap.cpp` (Forward Bypass kaskadi),
`history_item.cpp` (o'chirish/tahrirlash arxivi, `setDeletedLocally()`),
`data_session.cpp` (bu sessiyada qo'shilgan foto-saqlash hook'i),
`data_peer.cpp`/`data_channel.cpp`/`data_user.cpp` (spoof/AntiDelete
integratsiyasi) — bularning barchasi CustomMod'ning yuragi. Upstream
shu fayllarni ham faol o'zgartirgan bo'lsa, avtomatik merge ko'p hollarda
ishlamaydi va qo'lda konflikt hal qilish kerak bo'ladi.

---

## Nega umuman sync qilish kerak (foyda)

1. **Xavfsizlik/bug-fix patchlari** — 552 commitda muqarrar ravishda
   xavfsizlik tuzatishlari bor (masalan, oldingi sessiyada topilgan
   Qt6-ga bog'liq submodule muammosi kabi narsalar upstream'da hal
   qilingan bo'lishi mumkin).
2. **MTProto protokol o'zgarishlari** — Telegram serveri tomonidan
   protokolga yangi talablar qo'yilsa, juda eski client versiyalar
   ishlamay qolishi mumkin (bloklanish xavfi emas, lekin funksionallik
   buzilishi mumkin).
3. **Texnik qarz jamg'arilishi** — qancha kech qolsak, keyingi safar
   yanada ko'proq commit va yanada ko'proq to'qnashuv bo'ladi. 552 hozir
   qiyin bo'lsa, 1500+ keyinroq battar bo'ladi.
4. **Yangi funksiyalar** — foydali upstream funksiyalarni olish imkoniyati
   (masalan, upstream changelog'ida "Screen reader support",
   accessibility yaxshilanishlari kabi narsalar ko'rilgan edi oldingi
   sessiyada).

## Nega ehtiyot bo'lish kerak (xavf)

1. **24 ta yuqori-xavfli fayl** — aynan CustomMod'ning yuragi.
2. **`lib_ui` submodule** — bizda mahalliy Qt5-moslik patch bor
   (commit `3168d26`, faqat lokal, hech qachon push qilinmagan). Upstream
   submodule pointer'ini olsak, bu patch qayta qo'llash kerak bo'lishi
   mumkin — yoki upstream o'zi allaqachon tuzatgan bo'lishi mumkin.
3. **Build environment** — shu sessiyada ~3 soatlab vaqt ketgan build
   muammolarini (PDB race, Qt5/Qt6 nomuvofiqligi, minizip) upstream
   o'zgarishlari qayta faollashtirishi yoki yangilarini qo'shishi mumkin.
4. **Regression xavfi** — barcha CustomMod funksiyalari (spoof, AntiDelete,
   AntiEdit, Forward Bypass, Backup/Restore, Ghost Read) upstream'ning
   o'zgargan `HistoryItem`/`DocumentData`/`PhotoData`/`Data::Session`
   API'lariga bog'liq — bittasi buzilsa boshqalari ham zanjir bo'ylab
   buzilishi mumkin.

---

## Yondashuv variantlari

### A) Bitta katta merge (`git merge upstream/dev`, bir yo'la)
- ✅ Tez, bitta commit
- ❌ 552 commit + 24 fayl to'qnashuvini BIR SESSIYADA hal qilish kerak — juda
  charchatuvchi va xato qilish ehtimoli yuqori
- ❌ Muammo chiqsa qaysi qismi sabab ekanini bilish qiyin

### B) Bosqichma-bosqich merge (TAVSIYA ETILADI)
- Upstream'dagi 552 commitni sana bo'yicha 3-5 ta partiyaga bo'lib,
  har birini alohida merge qilish (masalan, har 100-150 commitda bitta
  checkpoint)
- Har bosqichdan keyin: build + asosiy funksiyalarni tekshirish
- ✅ Muammo chiqsa aniq qaysi bosqichda ekani ma'lum bo'ladi
- ✅ Har bosqich boshqarib bo'ladigan hajmda
- ❌ Umumiy vaqt ko'proq (bir necha sessiya)

### C) Tanlab cherry-pick (faqat muhim fix'lar)
- Faqat xavfsizlik/kritik bug-fix commit'larni qo'lda tanlab olish
- ✅ Eng kam qisqa muddatli xavf
- ❌ Upstream'ning changelog/commit tarixini qo'lda ko'rib chiqish talab
  qiladi (552 ta commit — juda ko'p vaqt), muhim commit'larni o'tkazib
  yuborish xavfi, va bog'liq commit'lar zanjirini uzib qo'yish (masalan
  bitta feature 5 ta commit bo'lib kelgan bo'lsa, faqat 2 tasini olish
  muvofiqsizlikka olib kelishi mumkin)

**Tanlov: B (bosqichma-bosqich merge)** — 552 commit uchun eng
muvozanatli yechim: C kabi tanlab-olishning to'liqsizlik xavfisiz, A kabi
bitta ulkan xavfli operatsiyasiz.

---

## Amaliy qadamlar (implementatsiya boshlanganda)

### 0-bosqich: Xavfsizlik tayyorgarligi
- [ ] Joriy `Oybek` branch holatidan backup tag yaratish:
      `git tag pre-upstream-sync-2026-07-11`
- [ ] Ishni ALOHIDA branch'da olib borish: `git checkout -b upstream-sync-wip`
      (yoki `git worktree` orqali alohida papkada) — `Oybek`ni to'g'ridan-
      to'g'ri xavf ostiga qo'ymaslik uchun
- [ ] Joriy build holatini tasdiqlash (agar hali qilinmagan bo'lsa) —
      "sync qilishdan oldin ishlagan edi" degan aniq boshlang'ich nuqta
      kerak, aks holda keyingi build xatolari sync sabablimi yoki
      oldindan bor edimi — bilib bo'lmaydi

### 1-bosqich: Submodule holatini tekshirish
- [ ] `lib_ui` uchun: upstream qaysi commit'ga ishora qilishini ko'rish
      (`git ls-tree upstream/dev Telegram/lib_ui`)
- [ ] O'sha commit atrofida bizning Qt5-moslik muammosi (QAccessible
      Qt6-only API'lar) hali ham mavjudmi tekshirish — balki upstream
      o'zi tuzatgan
- [ ] Boshqa submodule'lar (`lib_crl`, `lib_tl`, `lib_webview`, `cmake`)
      uchun ham xuddi shunday — bular kamroq xavfli, chunki biz ularda
      lokal patch qilmaganmiz

### 2-bosqich: Bosqichma-bosqich merge (har partiyadan keyin build+test)
- [ ] Upstream commit tarixini partiyalarga bo'lish (masalan
      `git log --oneline 7f8c690d85..upstream/dev | wc -l` bilan
      taqsimlab, har birida ~150 commit)
- [ ] Partiya 1: `git merge <intermediate-commit-1>`
- [ ] Konfliktlarni hal qilish — 24 ta ro'yxatdagi fayllarga alohida
      e'tibor, har birida BIZNING custom logikani (masalan
      `history_item.cpp`dagi `setDeletedLocally()`) yo'qotmaslik
- [ ] Build va asosiy funksiyalarni tekshirish (pastga qarang)
- [ ] Partiya 2, 3, ... — xuddi shunday, `upstream/dev`ga yetguncha

### 3-bosqich: To'liq regression test (har partiyadan keyin qisqa, oxirida to'liq)
- [ ] Device spoof (Mobile-View-Mode, platform/icon)
- [ ] AntiDelete/AntiEdit + Forward Bypass
- [ ] Backup export/import (Vazifa 1.1-2.3 — ayniqsa merge-restore mantiqi)
- [ ] Foto/media proaktiv saqlash hook'lari
- [ ] CustomMod oynasi UI (mojibake qaytmaganini tekshirish)

### 4-bosqich: `Oybek`ga qo'shish
- [ ] `upstream-sync-wip` branch to'liq build+test'dan o'tgach, `Oybek`ga
      merge qilish (yoki fast-forward, agar branch strategiyasi shunga
      imkon bersa)
- [ ] `origin/Oybek`ga push (odatdagidek — Co-Authored-By'siz, faqat
      `origin`ga, HECH QACHON `upstream`ga emas)

---

## Ochiq savollar (implementatsiya boshlanganda hal qilinadi)

1. Partiyalarni qanday bo'lish (sana bo'yichami, yoki upstream'dagi
   yirik feature-branch merge'lariga qarabmi)?
2. `saidjon` remote (`CppRwbDev/tdesktop.git`) — bu boshqa hamkasbning
   forki. Uning `Oybek` branch'imizga aloqasi bormi, sync paytida
   inobatga olish kerakmi? (Hozircha noma'lum — user bilan aniqlash kerak)
3. Har partiyadan keyin to'liq build (~1.5 soat) qilish shart emas — 552
   commitda necha marta build qilish kerakligini vaqt byudjeti bilan
   muvozanatlash kerak.

---

## Vaqt taxmini

Bosqichma-bosqich yondashuv bilan: **taxminan 6-10 soat** (bir necha
sessiyaga bo'lingan holda), asosiy vaqt konfliktlarni qo'lda hal qilish
va har bosqichdan keyingi build+testga ketadi. Bitta katta merge (A
varianti) nazariy jihatdan tezroq bo'lishi mumkin, lekin xavf sezilarli
yuqori.
