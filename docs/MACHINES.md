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
   (`<tdesktop>`: laptopda `C:\TBuild\tdesktop`, PC'da `D:\TBuild\tdesktop`).
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

PC ustuni 2026-09-21 da to'ldirildi — har bir qator tekshirilgan, taxmin emas.

| | Laptop | PC |
|---|---|---|
| `hostname` | `DESKTOP-L2J53IK` | `DESKTOP-5CAUS66` |
| `<tdesktop>` repo | `C:\TBuild\tdesktop` | `D:\TBuild\tdesktop` — build ildizi `D:\TBuild` (bo'shliqsiz bo'lishi SHART: MSYS2/jom/gyp bo'shliqli yo'lda yiqiladi). `D:\This PC\Documents\Projects programming\Telegram\tdesktop` junction bo'lib shu yerga olib keladi, shuning uchun eski chatlar ochilaveradi |
| `<server>` repo | `C:\Users\Oybek\Documents\Projects programming\Telegram\customsync-server` | `D:\This PC\Documents\Projects programming\Telegram\customsync-server` |
| Loyiha soyabon papkasi (sessiya ochiladigan joy) | `C:\Users\Oybek\Documents\Projects programming\Telegram` | `D:\This PC\Documents\Projects programming\Telegram` — junction tufayli laptopdagi `C:\Users\Oybek\Documents\Projects programming\Telegram` yo'li ham SHU YERGA olib keladi |
| Build chiqishi | `<tdesktop>\out\Release\Telegram.exe` | `<tdesktop>\out\Release\Telegram.exe` (nisbiy yo'l bir xil; hali build qilinmagan) |
| Sinov uchun ishga tushiriladigan nusxa | `C:\Users\Oybek\Pictures\Release\Telegram.exe` (log: shu papkadagi `log.txt`) | **TAYYOR EMAS** — hali sozlanmagan (2026-09-21) |
| Build muhiti | Qt 6.11.1 (`QT` env), `Telegram.slnx` | VS Community 2026 (18.7) C++ bilan BOR, standart joyda emas: `E:\Application's datas\Visual Studio\Program Files` (vcvars64.bat shu yerda). Windows SDK `10.0.26100.0` BOR. Python 3.11. MSVC toolset `14.44` (v144.4) **BOR** (2026-09-21 da o'rnatildi, `cl 19.44.35228` bilan tasdiqlandi) va `14.51` (v145) ham bor. `run-prepare.bat` `-vcvars_ver=14.44` beradi, ya'ni Windows 7 qo'llab-quvvatlashi SAQLANADI. Qt alohida o'rnatilmaydi: `prepare\win.bat` uni manbadan yig'adi (`QT=6.11.2`, modullar: qtbase/qtimageformats/qtshadertools/qtsvg) |
| Server testlari uchun baza sozlamasi | `<server>\src\CustomSync.Api\appsettings.Development.json` (gitignore — nusxalanadi yoki `scripts\db-bootstrap.ps1`) | **TAYYOR EMAS** — PostgreSQL PC'da umuman O'RNATILMAGAN (`psql` ham, Docker ham yo'q). Avval PostgreSQL o'rnatilsin, keyin `scripts\db-bootstrap.ps1`. Kodning o'zi toza yig'iladi (`dotnet build`: 0 xato), lekin 257 testdan 203 tasi bazasiz yiqiladi |
| Agent xotirasini sinxronlash | `C:\Users\Oybek\agent-sync-vault` (`agent-sync push/pull`) | `D:\This PC\Documents\Projects programming\agent-sync-vault` |
| CustomMod arxiv ildizi (`ArchiveRoot`) | `C:\Users\Oybek\Pictures\customizationMainFolder` (baza: `.../db/actioned_messages.db`) | `E:\customizationMainFolder` — 2026-09-21 da `C:\Users\Oybek\customizationMainFolder` dan KO'CHIRILDI (C: da atigi 12 GB bo'sh qolgan edi, arxiv esa 3.9 GB va o'sib boradi). Yo'l registrda: `HKCU\Software\CustomMod\TelegramDesktop` -> `archiveRootPath`. Eski papka `C:\Users\Oybek\customizationMainFolder.moved-20260921-233820` nomi bilan saqlanib turibdi (o'chirilmagan) |

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

### PC'dagi junction qoidasi (2026-09-21)

PC'da `C:\Users\Oybek\Documents\Projects programming` papkasi
`D:\This PC\Documents\Projects programming` ga junction. Claude Desktop
sessiyani **junction yo'li** (`C:...`) bilan eslab qoladi, CLI esa Node
orqali haqiqiy yo'lga (`D:...`) yechadi. Natijada bitta loyiha
`~\.claude\projects` ichida IKKI papkaga bo'linadi va sessiya tarixi
ko'rinmay qoladi.

Yechim: `C--...` nomli papkaning O'ZI `D--...` ga junction qilinadi
(skript: `make-project-junction.ps1`). Hozir shunday qilinganlar:

| Ilova izlaydigan nom | Haqiqiy papka |
|---|---|
| `C--Users-Oybek-Documents-Projects-programming-Telegram` | `D--This-PC-Documents-Projects-programming-Telegram` |
| `C--Users-Oybek-Documents-Projects-programming-Telegram-customsync-server` | `D--This-PC-Documents-Projects-programming-Telegram-customsync-server` |

**Yangi loyiha PC'da birinchi marta ochilganda shu junction'ni ham
qo'shing** — aks holda o'sha loyihaning tarixi yana bo'linadi.

`agent-sync` bunga moslangan: robocopy junction ICHIGA kirib nusxalaydi,
ya'ni mazmun ko'chma `C--...` nomi ostida vault'ga tushadi va laptop uni
o'z nomi bilan oladi. `D--...` nishonlari va `*.pre-junction-*`
zaxiralari esa push'da chetlatiladi, aks holda vault'da (va laptopda)
ikkinchi, hech kim o'qimaydigan nusxa paydo bo'lardi.
