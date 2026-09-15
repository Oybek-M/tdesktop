# Gemini / Antigravity — shu repo uchun kirish yo'riqnomasi (CustomMod)

> Bu fayl upstream'da YO'Q — CustomMod fork'iga tegishli. Repo bo'yicha
> umumiy qoidalar upstream `AGENTS.md` da; quyidagilar undan USTUN turadi.

## 🖥️ 1. AVVAL: qaysi kompyuterdasiz (multi-device)

Loyiha **ikki kompyuterda** (laptop va PC) olib boriladi. Hujjatlar,
promptlar va agent xotirasidagi mutlaq yo'llar (`C:\TBuild\...`,
`C:\Users\Oybek\...`) **boshqa kompyuterniki** bo'lishi mumkin.

1. `hostname` ni aniqlang.
2. Yo'llarni FAQAT [`docs/MACHINES.md`](docs/MACHINES.md) jadvalidagi shu
   kompyuter qatoridan oling. Ishlatishdan oldin mavjudligini tekshiring.
3. Kompyuter jadvalda yo'q yoki yo'l topilmasa — taxmin qilmang:
   qidiring, jadvalga yozing yoki foydalanuvchidan so'rang.
4. Ish boshida: `git fetch origin && git status -sb`. Boshqa kompyuterda
   qilingan ish bo'lishi mumkin — `origin/Oybek` oldinda bo'lsa avval
   `git pull --ff-only`. Ish oxirida hammasini push qiling (keyingi ish
   boshqa kompyuterda davom etadi).

## 2. Qayerdan boshlash

1. `docs/superpowers/PROJECTS.md` — qaysi ish faol, keyingi qadam.
2. Joriy reja: `docs/superpowers/plans/2026-09-13-peak-performance-and-stability-plan.md`.
3. Sizga berilgan prompt fayli (`docs/superpowers/plans/*-prompt.md`).

customsync-server alohida repo va alohida sessiyada olib boriladi —
bu repo ichida uning ustida ishlamang.

## 3. Buzilmaydigan qoidalar

- Build'ni FAQAT foydalanuvchi qiladi. Agent build ham, ilovani ishga
  tushirish ham qilmaydi.
- Push faqat `origin Oybek`; upstream'ga HECH QACHON. Commit'larda
  `Co-Authored-By` YO'Q. Faqat o'zgartirgan fayllarni stage qiling
  (`cmake` submodule'ga tegmang).
- CustomMod bazasiga faqat ilova yopiq holda, zaxira nusxa va undo log
  bilan yoziladi. O'lchov — bazaning nusxasida.
- Eski build'ni yangi `tdata` ustiga ishga tushirmang — barcha akkauntlar
  yo'qoladi (2026-09-11 da sodir bo'lgan).
- Fayllarni butunlay o'chirmang — faqat Recycle Bin.
- Izohlar o'zbekcha, "nega" ni tushuntiradi, apostrof ASCII `'`.
  Foydalanuvchiga javoblar o'zbek tilida.
