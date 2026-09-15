# Kompyuterlar va yo'llar — agentlar uchun MAJBURIY

Loyiha ("tdesktop-customization": CustomMod + customsync-server) **ikki
kompyuterda** olib boriladi. Yo'llar kompyuterga qarab farq qiladi.
Hujjatlarda, xotirada yoki promptlarda uchragan har qanday mutlaq yo'l
(`C:\TBuild\...`, `D:\Oybek\...`) — **faqat o'sha kompyuter uchun** to'g'ri.

## Sessiya boshida (Claude, Gemini, boshqa agent)

1. Kompyuterni aniqlang: `hostname` (PowerShell: `$env:COMPUTERNAME`).
2. Quyidagi jadvaldan shu kompyuter qatorini oling va **faqat shu
   yo'llarni** ishlating.
3. Yo'lni ishlatishdan oldin mavjudligini tekshiring (`Test-Path` /
   `ls`). Repo uchun: `git -C <yo'l> remote -v` kutilgan remote'ni
   ko'rsatishi shart.
4. Kompyuter jadvalda yo'q yoki yo'l topilmasa — **taxmin qilmang**:
   - repo'ni toping: `custom_db.cpp` bor papka (tdesktop) va
     `CustomSync.sln` bor papka (server);
   - topilgan yo'llarni jadvalga yangi qator qilib yozing va
     foydalanuvchiga ayting;
   - topilmasa — foydalanuvchidan so'rang.
5. Hujjatdagi boshqa kompyuter yo'lini ko'rsangiz, uni jadval orqali shu
   kompyuterdagi yo'lga "tarjima" qiling (masalan laptop'dagi
   `C:\TBuild\tdesktop\docs\...` -> PC'dagi `<tdesktop>\docs\...`).
   Hujjatlarning o'zini yo'l bo'yicha qayta yozish shart emas.

Hujjatlarda yangi yo'l yozayotganda iloji bo'lsa **nisbiy** yozing:
`<tdesktop>/docs/...`, `<server>/PROGRESS.md`.

## Agentlar uchun kirish nuqtalari

| Agent | Qoida qayerdan o'qiladi |
|---|---|
| Claude Code | global `~\.claude\CLAUDE.md` ("Kompyuterga bog'liq yo'llar" bo'limi, `agent-sync` bilan ko'chadi) + `docs/superpowers/PROJECTS.md` boshidagi ogohlantirish |
| Gemini / Antigravity | repo ildizidagi `GEMINI.md` (git bilan ko'chadi) + har prompt faylining §0 bloki |
| Har qanday prompt | pastdagi standart §0 blokini promptning BOSHIGA qo'ying |

Upstream `CLAUDE.md` / `AGENTS.md` ga ATAYLAB tegilmagan — upstream
merge'da konflikt bermasligi uchun.

### Prompt'lar uchun standart §0 bloki (nusxalang)

```markdown
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
```

## Ish kunining oxirida (kompyuter almashishidan oldin)

1. Ikkala repo: `git status` toza, hammasi `origin/Oybek` ga push qilingan.
2. `agent-sync push "<izoh>"` — agent xotirasi va chatlar.
3. Keyingi kompyuterda: ikkala repo'da `git pull --ff-only`, keyin
   `agent-sync pull`.
4. `~\.gemini\GEMINI.md` `agent-sync` bilan KO'CHMAYDI (u faqat
   `.gemini\antigravity` ni oladi) — shuning uchun Gemini qoidalari
   repo'dagi `GEMINI.md` ga yozilgan.

## Jadval

| | Laptop | PC |
|---|---|---|
| `hostname` | `DESKTOP-L2J53IK` | ❓ birinchi sessiyada to'ldirilsin |
| `<tdesktop>` repo | `C:\TBuild\tdesktop` | `D:\Oybek\Telegram\tdesktop` (❓ tasdiqlansin) |
| `<server>` repo | `C:\Users\Oybek\Documents\Projects programming\Telegram\customsync-server` | ❓ |
| Loyiha soyabon papkasi (sessiya ochiladigan joy) | `C:\Users\Oybek\Documents\Projects programming\Telegram` | ❓ |
| Build chiqishi | `<tdesktop>\out\Release\Telegram.exe` | ❓ |
| Sinov uchun ishga tushiriladigan nusxa | `C:\Users\Oybek\Pictures\Release\Telegram.exe` (log: shu papkadagi `log.txt`) | ❓ |
| Build muhiti | Qt 6.11.1 (`QT` env), `Telegram.slnx` | ❓ |
| Server testlari uchun baza sozlamasi | `<server>\src\CustomSync.Api\appsettings.Development.json` (gitignore — nusxalanadi yoki `scripts\db-bootstrap.ps1`) | ❓ |
| Agent xotirasini sinxronlash | `C:\Users\Oybek\agent-sync-vault` (`agent-sync push/pull`) | ❓ |

Remote'lar (hamma kompyuterda bir xil): tdesktop -> `origin` =
`Oybek-M/tdesktop` fork, branch `Oybek` (upstream'ga push TAQIQ);
server -> `Oybek-M/customsync-server`, branch `Oybek`.

## Claude xotirasi va yo'llar

Claude Code loyiha xotirasini **sessiya ochilgan papka yo'li** bo'yicha
saqlaydi: `~\.claude\projects\<yo'l, belgilar '-' ga almashtirilgan>\memory\`.
Masalan laptop'da soyabon papka uchun
`C--Users-Oybek-Documents-Projects-programming-Telegram`.

`agent-sync pull` fayllarni aynan shu nom bilan tiklaydi. PC'da sessiya
**boshqa yo'ldan** ochilsa, Claude boshqa papkani qidiradi va laptop
xotirasini ko'rmaydi. Shuning uchun:

- PC'dagi birinchi sessiyada agent o'z xotira papkasi bo'sh ekanini
  sezsa — `~\.claude\projects\` ichidan laptop papkasini topib,
  `memory\` ni o'z papkasiga nusxalasin (ustiga yozmasdan, faqat yo'q
  fayllarni) va foydalanuvchiga aytsin.
- Xotiradagi mutlaq yo'llar ham laptop'niki — 5-qadam bo'yicha
  tarjima qilinadi.

Muhim holat va qarorlar xotirada emas, repo hujjatlarida:
`<tdesktop>\docs\superpowers\PROJECTS.md`, `<server>\PROGRESS.md`. Xotira
yo'qolsa ham ish davom etadi.
