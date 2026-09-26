# PC (DESKTOP-5CAUS66) sozlash holati

Oxirgi yangilanish: **2026-09-21 08:50**

Bu hujjat PC'ni ish holatiga keltirish jarayonining qayerda to'xtaganini
yozadi. Yo'llar uchun `docs/MACHINES.md` jadvaliga qarang.

---

## TUGAGAN ISHLAR

### Repolar — hammasi joyida
| Repo | Yo'l | Holat |
|---|---|---|
| tdesktop | `D:\TBuild\tdesktop` | branch `Oybek`, 26729 commit, **to'liq tarix**, **39/39 submodul**, toza |
| customsync-server | `...\Telegram\customsync-server` | branch `Oybek` |
| sehexport | `...\SehExport` | `main` |
| UFL-cpt-data-extractor | `...\StartUps\UFL` | `main` |
| shaharga-nazar | `...\StartUps\Shaharga nazar\hokimga_project` | `master` |
| SmartCity.Backend | `...\DC\SmartCity\backend\SmartCity.Api` | avvaldan bor edi |

### Junction'lar (yo'llarni moslash uchun, fayl ko'chirilmagan)
```
C:\Users\Oybek\Documents\Projects programming  ->  D:\This PC\Documents\Projects programming
C:\Users\Oybek\Desktop\hokimga_project         ->  ...\StartUps\Shaharga nazar\hokimga_project
D:\This PC\...\Telegram\tdesktop               ->  D:\TBuild\tdesktop
```
Natija: laptopdagi chatlarning **23 tasi ham** PC'da ochiladi.

### Agent muhiti
- 5 ta agent tiklandi: `xato-tuzatuvchi`, `feature-dev`, `ui-dizayner`,
  `deploy-yordamchi`, `rejalashtiruvchi`
- Claude Desktop yon panel indeksi tiklandi (27 yozuv)
- `agent-sync.ps1` kengaytirildi: `agents/`, `commands/`, `plugins/`,
  Desktop sessiya indeksi, MCP OAuth (bo'sh tokenlardan himoya bilan)
- **Tuzatilgan jiddiy xato**: skript `.credentials.json` ni BOM bilan
  yozardi, Node esa BOM'da `JSON.parse` xatosi beradi — ya'ni `pull`
  PC'dagi loginni yo'q qilardi. Commit `16d0804` (agent-sync-vault).

### Build muhiti
- Visual Studio Community 2026 (18.7) C++ bilan — `E:\Application's datas\Visual Studio\Program Files`
- Windows SDK `10.0.26100.0`, Python 3.11.9
- `cl.exe` 19.51 (toolset 14.51 / v145) ishlaydi, sinovdan o'tgan

---

## TO'XTAGAN JOY — birinchi qilinadigan ish

### MSVC 14.44 toolset chala o'rnatilgan
- Papka bor: `...\VC\Tools\MSVC\14.44.35207` — lekin atigi **33 MB / ~500 MB**,
  `cl.exe` va `link.exe` YO'Q
- `vs_installer.exe` 2026-09-21 08:03 da boshlangan, 08:05 da **qotgan**
- `setup.exe` jarayonlari elevated — oddiy sessiyadan to'xtatib bo'lmaydi
- Qayta ishga tushirishga urinildi, UAC so'rovi bekor qilindi

**Yechim (qo'lda, GUI orqali):**
```
Start -> Visual Studio Installer -> Modify -> Individual components
      -> qidiruv: 14.44
      -> "MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.44)"
      -> Modify
```
Komponent ID (katalogdan tasdiqlangan):
`Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64`

**Muqobil:** 14.44 dan voz kechib 14.51 bilan ketish. Buning uchun
`D:\TBuild\run-prepare.bat` dan `-vcvars_ver=14.44` ni olib tashlang.
Natija: Windows 7 qo'llab-quvvatlashi yo'qoladi (rasmiy hujjat 14.44 ni
"majburiy" deydi).

### Keyin: prepare (Qt + 33 kutubxona)
Tayyor turibdi, ishga tushirish:
```
powershell -NoProfile -ExecutionPolicy Bypass -File D:\TBuild\run-prepare.ps1
```
- `cl` versiyasi `19.44.x` ekanini tekshirib, keyin boshlaydi
  (papka nomiga qaramaydi — u erta paydo bo'ladi, aldaydi)
- `JOBS=3` — i5-2400 (4 yadro) va ~8 GB bo'sh RAM uchun moslangan
- Qt manbadan yig'iladi: `QT=6.11.2`, modullar qtbase/qtimageformats/qtshadertools/qtsvg
- Uzilsa xavfsiz: `prepare.py` tugagan bosqichlarni keshlaydi
- `win.bat qt6 **silent**` — SHART. Usiz `prepare.py` eskirgan bosqichda
  `(r)ebuild, rebuild (a)ll, (s)kip...?` deb savol beradi va javobni
  `msvcrt.getch()` bilan KONSOLDAN o'qiydi. Fon jarayonida konsol yo'q,
  shuning uchun 09-21 08:33 dagi urinish 2/33-bosqichda (ThirdParty/msys64)
  darhol `FAILED` bergan edi — aslida bu kompilyator muammosi EMAS edi.
- Loglardagi `'vswhere.exe' is not recognized` va PowerShell
  `The string is missing the terminator` xatolari ZARARSIZ: VS papkasi nomida
  apostrof bor (`Application's datas`) va `vswhere` PATH da emas, lekin
  `vcvars64.bat` baribir muvaffaqiyatli ishlaydi (tekshirildi: cl 19.44.35228).
- Holat: `D:\TBuild\prepare-status.txt`, loglar: `D:\TBuild\prepare-logs\`

**Kutish:** i5-2400 da yuklash bir necha soat, kompilyatsiya undan ham ko'p.

### Undan keyin: Telegram build
`api_id` = `28454823` (docs/superpowers/plans/2026-08-01-self-update-plan.md),
`api_hash` foydalanuvchida — repo'ga ATAYLAB yozilmagan.

```
configure.bat x64 qt6 -D TDESKTOP_API_ID=28454823 -D TDESKTOP_API_HASH=<sizniki>
```

---

## HAL QILINMAGAN BOSHQA MASALALAR

### 1. Ajralib ketgan repolar (ikki kompyuterda ishlashdan OLDIN hal qilinsin)
| Repo | Mahalliy, GitHub'da yo'q | GitHub'da, mahalliyda yo'q |
|---|---|---|
| `IBOS\iBOS_CRM\iBOS_Back` | **70** | **112** |
| `DC\SmartCity\frontend` | **23** | **81** |
| `DC\SmartCity\test-connection` | 4 | 5 |
| `IBOS\iBOS_CRM\iBOS_Front` | 0 | 26 |

Commit qilinmagan fayllar ham bor: `SmartCrm.Backend` 17, `SmartCity` 17,
`iBOS_CRM` 2, `SmartCRM.Frontend` 1.

`iBOS_Back` dagi 70 commit faqat shu PC'da — GitHub'da yo'q.
Nega ajralgani aniqlanmagan.

### 2. Remote'siz papka
`DC\SmartCity\firmware` — 1 fayl, 2 commit, remote YO'Q. Faqat shu PC'da.

### 3. Plagin avtorizatsiyasi fayl orqali ko'chmaydi
Laptopdagi `.credentials.json` da ham 100 ta `mcpOAuth` yozuvining
**hammasida** `accessToken` bo'sh. Ya'ni plaginlarni PC'da alohida ulash
kerak (`/mcp`, interaktiv terminal).

### 4. C: diskda joy
97.5 GB band, ~13 GB bo'sh. Eng kattalari: `AppData` 21.9 GB
(Google 5.85, Packages 3.66, npm-cache 2.29, Temp 1.64),
`hiberfil.sys` 6.38 GB, `customizationMainFolder` 3.35 GB.
Eng xavfsiz yechimlar: `powercfg /h off` (6.4 GB),
`Local\Temp` tozalash (1.6 GB), `customizationMainFolder` ni D: ga
ko'chirib junction qo'yish (3.35 GB).

### 5. Sirlar hisoboti (boshlanmagan)
agent-sync-vault repo'sida: `BEGIN PRIVATE KEY` 4 faylda 18 marta,
`AKIA...` 1 ta, `ghp_` 13 faylda. Repo private, lekin VPS SSH kaliti
bo'lsa tekshirish kerak.

---

## MUHIT HAQIDA ESLATMA

- Xalqaro kanal sekin: GitHub'dan o'lchangan **75-135 KB/s**, ping ~100 ms.
  Ichki (Toshkent) kanal esa 729 Mbit/s, ping 10 ms. Ya'ni muammo uy
  internetida emas, xalqaro tranzitda. Katta klonlarni bo'lak-bo'lak qilish kerak.
- `git clone` bir bo'lak — uzilsa noldan. Sayoz klon + `fetch --deepen`
  bo'laklari esa saqlanadi. tdesktop aynan shu usulda olindi.

---

## 2026-09-23: prepare TUGADI (33/33) va configure O'TDI

### Qt qo'lda qurildi

prepare'ning har bir bosqichi `removeDir(stage)` bilan boshlanadi, ya'ni
qayta urinish Qt daraxtini butunlay o'chiradi. Klon + qurish soatlab
vaqt olgani uchun Qt prepare'dan TASHQARIDA qurildi (`cmake --build`
Debug/Release + `cmake --install`, `--parallel 3` bilan -- 4 da xotira
tugab ninja `FAILED: [code=143]` bergan edi).

Keyin prepare'ning kesh kaliti QO'LDA yozildi, aks holda keyingi ishga
tushirishda Qt yana o'chirilardi. Usul: prepare.py ga vaqtincha
`DUMPKEY` shohchasi qo'shildi (`writeCacheKey(stage)` + `continue`),
`win.bat qt6 qt_6.11.2 silent` bilan chaqirildi, keyin yamoq olib
tashlandi. Kalit muhitga bog'liq, shuning uchun uni AYNAN o'sha
argumentlar bilan hisoblatish shart. Natija: `[29/33]
(Libraries/qt_6.11.2): SKIPPING`.

Bosqich nomlarini prepare'ga BERMANG: nomlangan bosqich `Forced`
bo'lib qayta quriladi. Nomsiz ishga tushirilsa kesh kalitiga qaraydi.
`crashpad` esa Windows'da umuman ro'yxatga olinmaydi (u faqat `mac:`
bosqichi) -- nomlansa "Unknown argument: crashpad".

### configure: uchta alohida to'siq

1. **CMake VS nusxasini reestrdan topadi**, ya'ni junction'ni EMAS,
   apostrofli HAQIQIY yo'lni oladi -> `MSB4092`. vcvars'ni junction
   orqali chaqirish yetarli emas. Yechim:
   `-D "CMAKE_GENERATOR_INSTANCE=E:\VS2026,version=18.7.11919.86"`.
   - Ajratuvchi VERGUL (`?` bilan "invalid field").
   - `version=` SHART: junction'ni VS Installer tanimaydi.
   - Muhit o'zgaruvchisi sifatida ISHLAMAYDI ("will be ignored,
     because CMAKE_GENERATOR is not set").
2. **CMake keshi buzilgan qiymatni saqlab qoladi.** Noto'g'ri
   `CMAKE_GENERATOR_INSTANCE` bir marta yozilsa, keyingi to'g'ri
   urinishlar ham eski qiymat bilan yiqiladi. `out\CMakeCache.txt`
   ni olib tashlash kerak.
3. **v143 toolset bu mashinada BUZUQ.** `MSBuild\Microsoft\VC\v170\
   Microsoft.Build.CppTasks.Common.dll` (575 KB, 2026-09-21) aslida
   DLL emas: birinchi baytlari `21 3C 61 72` = `!<arch>`, ya'ni ar
   arxivi. `[Reflection.AssemblyName]::GetAssemblyName` ham, MSBuild
   ham uni o'qiy olmaydi (`MSB4062 ... Unknown file format`).
   Haqiqiy yo'l orqali ham xuddi shunday, demak junction aybdor emas.

### Toolset va Windows 7

`cmake/run_cmake.py` rasmiy kodda `-T v143` ni QATTIQ yozadi, lekin u
`cmake` SUBMODULE'ida -- unga tegilmaydi. Yechim: run_cmake.py aniq
`-G` berilganda o'zining `-A`/`-T` bloklarini butunlay o'tkazib
yuboradi. Shuning uchun `run-configure.bat` generator, arxitektura va
toolset'ni o'zi beradi:

    configure.bat qt6 "-GVisual Studio 18 2026" -Ax64 -Tv145 ...

`x64` so'zi BERILMAYDI: run_cmake.py uni `vsArch` deb tushunadi va
aniq generator bilan birga "x86/x64/arm switch is supported only with
Visual Studio" xatosini beradi. `-Ax64` qo'shib yoziladi.

v145 targets'ning O'Z kompilyatori 14.51 bo'lardi, bu esa Windows 7
qo'llab-quvvatlashini yo'qotardi. CMake'ning `-T v145,version=14.44`
yo'li ISHLAMAYDI -- `14.44`, `14.44.35207`, `14.44.17.14`ning hammasi
"invalid version specification" beradi, chunki VS2026 yonma-yon
toolset papkasini VS versiyasi (`14.44.17.14`) bilan nomlagan, CMake
esa bu formatni qabul qilmaydi.

**Yechim: MSBuild'ning `VCToolsVersion` xossasi.** U targets'ni v180
da (sog'lom DLL) qoldiradi, lekin `cl.exe`/`link.exe` ni
`14.44.35207` dan oladi:

    -D CMAKE_VS_GLOBALS=VCToolsVersion=14.44.35207

`CMAKE_VS_GLOBALS` uni har bir `.vcxproj` ga yozadi, ya'ni VS IDE dan
qurilsa ham amal qiladi. Tasdiqlangan: sinov dasturi `_MSC_VER = 1944`
berdi, `CL.exe` yo'li `E:\VS2026\VC\Tools\MSVC.44.35207\...`.

Ya'ni **Windows 7 qo'llab-quvvatlashi SAQLANADI** va v143 ni tiklash,
VS2022 Build Tools o'rnatish yoki admin huquqi KERAK EMAS.

Buzuq v170 DLL'i o'z holicha qoldi. U faqat `-Tv143` ga to'sqinlik
qiladi; hozirgi sxemada umuman ishlatilmaydi. Istasangiz VS Installer
-> Repair uni tiklaydi (admin kerak), lekin bu shart emas.

### Holat

`D:\TBuild\tdesktop\out\Telegram.slnx` yaratildi, `TDESKTOP_API_ID` va
`TDESKTOP_API_HASH` keshda. Qurish hali BOSHLANMAGAN.

---

## 2026-09-24: PC'da BOG'LASH IMKONSIZ -- ildiz sabab va qaror

> **!!! BU BO'LIM ESKIRGAN VA XATO !!!**
> Bu yerdagi jadval va xulosa ishonchsiz: o'sha sinovlarda bayroqlar
> `link.exe` ga umuman yetib bormagan (global `/p:` item metadata'ni
> bekor qilmaydi). Hujjatning ENG OXIRIDAGI
> "YUQORIDAGI XULOSA NOTO'G'RI EDI -- tuzatish" bo'limini o'qing.
> Quyidagi matn faqat tarix uchun saqlangan.

Kompilyatsiya (1091 manbadan 1037 obj) muvaffaqiyatli o'tdi. Yiqilgan
joy -- faqat BOG'LASH (link) bosqichi.

### O'lchangan ildiz sabab

    1037 ta .obj   = 22.0 GB
    kutubxonalar   =  5.8 GB
    ------------------------
    linker kirishi ~ 28 GB

Linker bu 28 GB ni bir o'tishda qayta ishlaydi, shuning uchun uning
~26 GB commit talabi kutilgan holat. `WholeProgramOptimization` va
`LinkTimeCodeGeneration` YOQILMAGAN (tekshirilgan) -- LTCG sabab emas.

PC'da 16 GB RAM bor, undan linkerга ~12 GB tegadi. Qolgan ~14 GB HDD
pagefile'ga chiqadi. HDD'da tasodifiy o'qish ~10 ms, NVMe SSD'da
~50-100 us -- **100 barobar** farq. Natijada linker qotib qoladi.

### Sinalgan va YORDAM BERMAGAN yo'llar

| # | Sozlama | Cho'qqi commit | CPU (60s da) |
|---|---|---|---|
| 1 | ikkala OPT yoqiq | 31.2 GB | 0.5 s |
| 2 | ICF o'chiq, REF yoqiq | 20.6 GB | 4.3 s |
| 3 | ikkala OPT o'chiq | 26.4 GB | 0.9 s |
| 4 | 3 + `/DEBUG:FASTLINK` | 26.5 GB | 0.6 s |

Nega hech biri ishlamadi: ularning hammasi linker QANDAY ishlashini
o'zgartiradi, QANCHA ma'lumot o'qishini emas. Kirish 28 GB bo'lib
qolaverdi. `/DEBUG:FASTLINK` ham yordam bermadi, chunki u LINKER'ning
PDB'siga ta'sir qiladi, obj fayllar ichidagi debug ma'lumotiga emas --
obj'lar allaqachon to'liq debug bilan kompilyatsiya qilingan edi.

Pagefile'ni kattalashtirish (D: ga 32 GB, commit limit 23 -> 55 GB)
LNK1102 ni yo'qotdi, LEKIN o'rniga o'tkazuvchanlik devorini ochdi.
Ya'ni u qattiq xatoni hal qildi, sekinlikni emas.

### Variantlar

- **A. `C:` (SSD) da ~30 GB bo'shatib, pagefile'ni o'sha yerga qo'yish.**
  PC'ni laptop bilan bir xil sharoitga keltiradi. Eng yaxshisi, agar
  SSD'da joy topilsa (hozir C: 111 GB dan atigi ~5 GB bo'sh).
- **B. Kod PC'da, build laptopda.** Hozirgi qaror. Repo `origin/Oybek`
  orqali sinxron, hech nima ko'chirish shart emas.
- **C. Debug simvollarsiz qayta kompilyatsiya.** obj hajmini keskin
  kamaytiradi, lekin 3-5 soat va simvollar butunlay yo'qoladi.
  Tavsiya etilmaydi.

### PC'da tayyor holda turgan narsalar

prepare 33/33, Qt 6.11.2 (qurilgan + kesh kaliti), `configure` o'tgan,
`Telegram.slnx` va API kalitlari joyida, 1037 obj fayl qurilgan.
Ya'ni RAM ko'paytirilsa yoki A varianti bajarilsa, faqat bog'lash
qoladi -- qaytadan boshlash shart emas.

---

## 2026-09-24 (kechqurun): YUQORIDAGI XULOSA NOTO'G'RI EDI -- tuzatish

> **DIQQAT.** Yuqoridagi "PC'da BOG'LASH IMKONSIZ" bo'limidagi jadval va
> undan chiqarilgan xulosa **ishonchsiz**. Sabab: o'sha to'rtala
> "sinov" da bayroqlar `link.exe` ga umuman YETIB BORMAGAN. Quyida
> nima bo'lgani, qanday aniqlangani va to'g'ri usul yozilgan.
> Bu bo'limni o'qimasdan yuqoridagi jadvalga tayanmang.

### Xato nimada edi

Bayroqlar MSBuild'ga shunday berilgan edi:

    /p:OptimizeReferences=false
    /p:EnableCOMDATFolding=false
    /p:GenerateDebugInformation=DebugFastLink

Bu **global property** shakli. Lekin `Microsoft.CppCommon.targets`
da `Link` vazifasi qiymatlarni **item metadata** dan oladi:

    E:/VS2026/MSBuild/Microsoft/VC/v180/Microsoft.CppCommon.targets
    1223:  EnableCOMDATFolding      ="%(Link.EnableCOMDATFolding)"
    1231:  GenerateDebugInformation ="%(Link.GenerateDebugInformation)"
    1267:  OptimizeReferences       ="%(Link.OptimizeReferences)"

`%(Link.X)` qiymati `.vcxproj` ichidagi `<ItemDefinitionGroup><Link>`
blokidan keladi. **MSBuild'da global property item metadata'ni bekor
qilmaydi.** Ya'ni `/p:` bilan berilgan uchala bayroq ham e'tiborsiz
qoldirilgan.

Tasdiq -- `Telegram.vcxproj` dagi Release bloki hech qachon
o'zgarmagan:

    Condition: '$(Configuration)|$(Platform)'=='Release|x64'
      DebugInformationFormat     = OldStyle     (ya'ni /Z7)
      GenerateDebugInformation   = true         (ya'ni /DEBUG)
      OptimizeReferences         = true         (ya'ni /OPT:REF)
      Optimization               = MaxSpeed
      WholeProgramOptimization   = (yo'q)       <- LTCG haqiqatan o'chiq

### Nega bu darhol sezilmadi

Jadvaldagi raqamlar (31.2 / 20.6 / 26.4 / 26.5 GB) bir-biridan farq
qilgani "bayroqlar ta'sir qilyapti, lekin yetarli emas" degan taassurot
bergan. Aslida ular **bitta o'zgarmagan jarayonning turli vaqtlarda
olingan o'lchovlari** edi. Bog'lash bosqichida commit vaqt o'tishi
bilan o'sib boradi, shuning uchun qachon o'lchansa shuncha raqam
chiqadi.

Hujjatning o'zida "avvalgi 16.6 GB o'lchovi bog'lash O'RTASIDAN
olingan edi, cho'qqi emas" deb yozilgan -- ya'ni ogohlantirish bor
edi, lekin u xulosaga ta'sir qilmagan.

**Saboq:** o'lchov farq qilishi bayroq ishlaganini ANGLATMAYDI. Avval
bayroq haqiqatan qo'llanganini tasdiqlash kerak, keyin o'lchash.

### Bayroq qo'llanganini QANDAY tekshirish kerak

Tartib bo'yicha, birinchisi eng arzoni:

1. **Targets faylini o'qing.** Vazifa `%(Item.X)` dan oladimi yoki
   `$(Property)` dan? `%(...)` bo'lsa `/p:` ishlamaydi.

       grep -nE "GenerateDebugInformation|OptimizeReferences" \
         "E:/VS2026/MSBuild/Microsoft/VC/v180/Microsoft.CppCommon.targets"

2. **tlog'ni o'qing** (bog'lash tugagan bo'lsa unda haqiqiy buyruq
   satri turadi, UTF-16LE):

       out/Telegram/Telegram.dir/Release/Telegram.tlog/link.command.1.tlog

   Diqqat: bog'lash TUGAMAGAN bo'lsa fayl 2 bayt (faqat BOM) bo'ladi va
   hech narsa isbotlamaydi -- bizda aynan shunday edi.

3. **MSBuild'ni `-v:diag` bilan** ishga tushirib `Link` vazifasining
   parametrlarini ko'ring.

### To'g'ri usul: ForceImportAfterCppTargets

`cmake` submodule'iga ham, repo fayllariga ham tegmasdan `Link`
metadata'sini almashtirish mumkin. `Microsoft.Cpp.Current.targets`
da rasmiy ilgak bor:

    148:  <Import Condition="Exists('$(ForceImportAfterCppTargets)')"
                  Project="$(ForceImportAfterCppTargets)"/>

U eng oxirida import qilinadi, shuning uchun undagi
`ItemDefinitionGroup` `.vcxproj` dagisini ustidan yozadi.
(`ForceImportBeforeCppTargets` ham bor, 14-qatorda -- lekin u ERTA
import qilinadi va `.vcxproj` uni ustidan yozib yuboradi. Kerakligi
**After**.)

Fayl `D:/TBuild/link-tuning.props` da turadi va build shunday
chaqiriladi:

    cmake --build ... -- /p:ForceImportAfterCppTargets=D:\TBuild\link-tuning.props

Bekor qilish uchun shunchaki shu bitta bayroqni olib tashlash yetarli.

### Haqiqiy ildiz sabab: /Z7 (OldStyle debug)

Obj fayllar qayta o'lchandi -- oldingi "22.0 GB" ham past baho ekan:

    2059 ta .obj = 25.8 GB,  o'rtacha 12.8 MB
    eng kattalari:
       317.0 MB  info_profile_actions.obj
       241.7 MB  star_gift_box.obj
       237.8 MB  history_widget.obj
       236.7 MB  history_view_compose_controls.obj
       204.4 MB  star_gift_auction_box.obj

Bitta `.cpp` dan 317 MB obj chiqishi optimizatsiyalangan koddan emas,
**debug ma'lumotidan**. Debug ma'lumotisiz Release obj odatda 1-3 MB.
Ya'ni 25.8 GB ning taxminan 80-85 foizi debug ma'lumoti.

`/Z7` (OldStyle) da debug ma'lumoti har bir obj fayl ICHIGA joylanadi
va har bir obj o'ziga kerakli **barcha tiplarning to'liq nusxasini**
ko'taradi. Qt sarlavhasini qo'shgan har bir fayl butun Qt tip jadvalini
olib yuradi. Linker `/DEBUG` tufayli bularning hammasini o'qib,
tiplarni deduplikatsiya qilib, bitta PDB ga birlashtirishi kerak --
xotirani aynan shu yeydi.

`/Zi` (ProgramDatabase) da esa tiplar HAR BIR LOYIHA uchun bir marta
birlashtiriladi: 2059 nusxa o'rniga ~40 ta.

### Keyingi qadamlar (tartib muhim)

| | Qayta kompilyatsiya | PDB | Izoh |
|---|---|---|---|
| **A. `/DEBUG:FASTLINK`** | **kerak emas** | bor, obj'larga bog'liq | Avval SHU sinaladi |
| **B. `/Zi` (`/Z7` o'rniga)** | kerak (soatlar) | to'liq, mustaqil | Tarqatish uchun |
| **C. Debug ma'lumotisiz** | kerak (soatlar) | yo'q | Oxirgi chora |

**A birinchi bo'lishi shart**, chunki 2059 obj joyida turibdi va u
faqat bog'lashni qayta ishga tushiradi (~30-60 daqiqa, soatlar emas).
A o'tsa -- bu mashinada bog'lash mumkinligi isbotlanadi va B ga
o'tiladi. A ham yiqilsa -- ANA SHUNDA "PC'da imkonsiz" xulosasi
haqiqatan asoslangan bo'ladi.

### Pagefile haqida -- bu alohida, haqiqiy muammo edi

Bayroq xatosidan mustaqil ravishda quyidagilar aniqlangan va ular
o'z kuchida qoladi:

- `C:` bo'shatildi (8.3 -> 20.9 GB), arxiv `E:` ga ko'chirildi.
- Pagefile: `C:` 16 GB (SSD) + `D:` 8 GB (HDD), commit limit 40 GB.
  `LNK1102: out of memory` shundan keyin yo'qoldi.
- **Jismoniy disklar:**

      Disk 0  WDC WD10EURX-63UY4Y0  932 GB  = D:  (AV-GP, 5400 rpm)
      Disk 1  ST1000DM003-1ER162    932 GB  = E:  (Barracuda, 7200 rpm)
      Disk 2  Ramsta SSD S800       112 GB  = C:

- **Hal qilinmagan nuqson:** 28 GB obj/lib `D:` da turibdi va `D:` da
  yana 8 GB pagefile bor. Ya'ni linker BIR disk boshidan ham o'qiydi,
  ham swap yozadi -- bu seek raqobati. O'lchovda tasdiqlandi:
  `C:` pagefile 0.34 GB, `D:` pagefile 1.48 GB ishlatilgan, ya'ni
  swap'ning ko'p qismi SSD'ga emas, HDD'ga tushgan.
- **Tavsiya (admin kerak):** `D:` dagi pagefile'ni O'CHIRIB, `E:` ga
  24 GB qo'yish. Shunda `D:` faqat o'qish bilan, `E:` faqat swap bilan
  shug'ullanadi, ikkalasi alohida shpindel va `E:` tezroq disk.
  Qo'shish emas, KO'CHIRISH kerak -- `D:` qolsa raqobat saqlanadi.
  Agar A/B varianti ishlasa, bu umuman keraksiz bo'lib qolishi mumkin.

### Umumiy saboqlar

1. Build tizimida bayroq berishdan oldin **u qayerdan o'qilishini**
   aniqlang (property yoki item metadata).
2. Raqam o'zgargani bayroq ishlaganini isbotlamaydi. Uzoq davom
   etadigan jarayonda o'lchov vaqti raqamni o'zgartiradi.
3. "Imkonsiz" degan xulosani chiqarishdan oldin gipoteza haqiqatan
   sinalganini tekshiring.

---

## 2026-09-25 (tun): FASTLINK ISHLADI, qolgan yagona to'siq - pagefile joyi

Bir kechada ikkita mustaqil to'siq ochildi va uchinchisi aniqlandi.

### To'siq 1: bayroq yetib bormasligi -- HAL QILINDI

Yuqoridagi bo'limda tasvirlangan. Yechim: `link-tuning.props` +
`/p:ForceImportAfterCppTargets=`. Tasdiqlash usuli -- ishlayotgan
`link.exe` ning javob faylini o'qish:

    Get-CimInstance Win32_Process -Filter "Name='link.exe'" | ForEach-Object {
      if ($_.CommandLine -match '@"?([^"]+\.rsp)"?') { Get-Content $Matches[1] -Raw }
    }

`/DEBUG:FASTLINK` `.rsp` da ko'rindi -- ya'ni mexanizm ishladi.

### To'siq 2: bayroqning o'zi qo'llab-quvvatlanmasligi -- HAL QILINDI

Bayroq yetib borgani bilan linker uni RAD ETDI:

    LINK : warning LNK4315: /DEBUG:FASTLINK is no longer supported.
    Using /DEBUG:FULL instead. Use a VS 2022 toolchain to continue
    building with /DEBUG:FASTLINK

`/DEBUG:FASTLINK` **VS 2026 linkeridan (14.51) olib tashlangan** va u
jimgina `/DEBUG:FULL` ga o'tib ketadi. Ogohlantirish "warning" bo'lgani
uchun build to'xtamaydi -- shuning uchun uni ATAYLAB qidirish kerak.

Yechim: 14.44 (VS 2022 17.14 toolseti) linkerini majburan tanlash.
`Link` vazifasi yo'lni PROPERTY dan oladi, shuning uchun oddiy
`PropertyGroup` yetarli (metadata muammosi bu yerda YO'Q):

    Microsoft.CppCommon.targets
      1308:  ToolExe  ="$(LinkToolExe)"
      1309:  ToolPath ="$(LinkToolPath)"

`link-tuning.props` ichida:

    <LinkToolPath>E:\VS2026\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64</LinkToolPath>
    <LibToolPath>E:\VS2026\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64</LibToolPath>

QO'SHIMCHA FOYDA: obj fayllar 14.44 bilan kompilyatsiya qilingan
(`_MSC_VER = 1944`). 14.51 bilan bog'lash ARALASH holat edi; 14.44
linkeri mos juftlik. Windows 7 uchun ham aynan 14.44 tanlangan edi.

Natija: `LNK4315` yo'qoldi, FASTLINK qabul qilindi.

### O'lchangan natijalar -- birinchi marta HAQIQIY raqamlar

| Sozlama | link commit | CPU/60s | Holat |
|---|---|---|---|
| `/DEBUG:FULL` (14.51) | 29.4 GB | 2.0 s | qotgan |
| `/DEBUG:FASTLINK` (14.44), boshida | 10.0 GB | **16.8 s** | **sog'lom** |
| `/DEBUG:FASTLINK` (14.44), 30 daq keyin | 17.5 GB | 0.5 s | qotgan |

Ya'ni FASTLINK cho'qqi commit'ni **29.4 -> 17.5 GB** ga tushirdi va
bog'lash boshida haqiqatan sog'lom ketdi. Keyin qotdi.

### To'siq 3: swap NOTO'G'RI DISKKA tushyapti -- HAL QILINMAGAN

Qotgan paytdagi o'lchov hammasini ko'rsatdi:

    Disk bandligi (5 s):
      D:  135 %   <- obj o'qish + swap, IKKALASI shu yerda
      E:    3 %   <- bo'sh turibdi
      C:    0 %   <- SSD umuman ishlatilmayapti

    pagefile ishlatilgan:  C: 0.52 GB    D: 2.30 GB
    RAM: link.exe 11.68 GB, bo'sh 0.3 GB

16 GB RAM'da linkerga ~11.7 GB tegadi. Commit 17.5 GB, demak ~5.8 GB
swap'ga chiqishi kerak. Windows o'sha swap'ni `D:` ga yubordi -- aynan
25.8 GB obj o'qilayotgan diskka. Disk boshi o'qish va yozish orasida
sakrab, 135% ga to'yindi. `C:` (SSD) esa 0% da bo'sh turdi.

**Sabab:** Windows pagefile tanlashda BO'SH JOYGA qaraydi (`C:` da
~5 GB, `D:` da 808 GB), disk TEZLIGINI bilmaydi.

**Kerakli o'zgarish (admin + qayta yuklash):**

    D:  ->  No paging file      (Set bosilsin)
    E:  ->  Custom 24576/24576  (Set bosilsin)
    C:  ->  tegilmaydi, 16384/16384

Har bir disk uchun **Set** ni ALOHIDA bosish shart -- 2026-09-24 da
aynan shu qolib ketib, sozlama qo'llanmagan edi.

Shundan keyin: `D:` faqat o'qiydi, swap `C:` (SSD) va `E:` (bo'sh,
7200 rpm) ga tushadi, ikkalasi alohida shpindel.

### Kuzatuv vositalaridagi ikkita xato (tuzatilgan)

1. `watch-link.ps1` asosiy bog'lashni topa olmadi. Sabab: u har bir
   `link.exe` ning `.rsp` faylini o'qirdi, lekin MSBuild `.rsp` ni
   vaqtinchalik papkaga yozadi va linker o'qib bo'lgach O'CHIRADI.
   Endi zaxira belgi bor: commit > 2 GB bo'lsa bu Telegram bog'lashi
   (codegen vositalari 0.2 GB dan oshmaydi).

2. `tail -f` kuzatuvchisi log faylini band qilib, PowerShell'ning
   `Add-Content` ini bloklab qo'ydi -- loglar yangilanmay qoldi va
   xatolik jim ketdi (`-EA 0` uni yashirdi). Bir faylga bir vaqtda
   `tail -f` va `Add-Content` qo'ymang.

### Keyingi qadam

1. Pagefile `D:` -> `E:` (admin, qayta yuklash) va FASTLINK bilan
   qaytadan bog'lash. Obj'lar joyida, qayta kompilyatsiya shart emas.
2. O'tsa -- PC'da bog'lash mumkinligi ISBOTLANADI. Keyin `/Zi` bilan
   to'liq qayta kompilyatsiya qilib, tarqatishga yaroqli mustaqil PDB
   olish masalasi hal qilinadi (FASTLINK PDB'si obj fayllarga bog'liq,
   boshqa mashinada ishlamaydi).
3. Agar shunda ham qotsa -- keyingi o'zgaruvchi `/OPT:REF` ni o'chirish
   (bir vaqtda BITTA o'zgaruvchi).

---

## 2026-09-25 00:45: BSOD (MEMORY_MANAGEMENT) -- va u XOTIRA YETMASLIGIDAN EMAS

Oxirgi sinov (FASTLINK + 14.44 linker + pagefile C:/E:) **qulash bilan
tugadi**: `MEMORY_MANAGEMENT` ko'k ekrani, tizim o'zi qayta yuklandi.

### MUHIM: o'lchovlar "RAM yetmadi" degan talqinni RAD ETADI

Qulashdan bir daqiqa oldingi yozuvlar (`link-watch.log`):

    [00:42:06] CPU 10.5s/60s | commit 5.95 GB | RAM'da 11.93 GB | tizim 12.5/56.0 GB  <- sog'lom
    [00:43:06] CPU 10.4s/60s | commit 6.78 GB | RAM'da 12.08 GB | tizim 13.3/56.0 GB  <- sog'lom

Ya'ni qulash paytida:

- `link.exe` **sog'lom** ishlayotgandi (10.4 s/60s, me'yor >10)
- uning commit'i atigi **6.78 GB** edi (avvalgi urinishlarda 17-29 GB)
- tizim commit'i **13.3 / 56 GB** -- chegaragacha **42 GB bo'sh joy**
- swap deyarli ishlatilmayotgandi

Bu uchala tuzatish (ForceImportAfterCppTargets, 14.44 linker, pagefile
E: ga) **ishlaganini** ko'rsatadi -- xotira talabi 29.4 GB dan 6.8 GB
gacha tushgan edi. Qulash konfiguratsiyadan EMAS.

### Qulashlar tarixi -- bu birinchi marta emas

`Get-WinEvent -FilterHashtable @{LogName='System'; Id=41}` da
"rebooted without cleanly shutting down":

    2026-09-25 00:45:50   <- bugungi, MEMORY_MANAGEMENT
    2026-09-23 16:14:34   <- bir soat ichida BESH marta
    2026-09-23 16:08:40
    2026-09-23 16:03:06
    2026-09-23 15:56:04
    2026-09-23 15:17:00
    2026-09-19 09:30:44
    2026-08-02 11:42:34

09-23 dagi klaster (1 soatda 5 ta qulash) o'sha paytda sezilmagan.
Ya'ni bu mashinada **takrorlanuvchi beqarorlik** bor va u bizning
build sozlamalarimizdan oldin ham mavjud edi.

### Nega crash dump yozilmagan

`CrashDumpEnabled = 7` (avtomatik), `DumpFile = C:\Windows\MEMORY.DMP`,
lekin `C:\Windows\Minidump` BO'SH va `BugCheck` (Event Id 1001) yozuvi
butun jurnalda BITTA ham yo'q.

Ehtimoliy sabab: `C:` da atigi 6.6 GB bo'sh joy bor, avtomatik dump
esa undan ko'proq talab qilishi mumkin -- Windows yoza olmay, jimgina
tashlab yuboradi. Kelajakda dalil qolishi uchun `C:` da joy bo'shatish
yoki `DedicatedDumpFile` ni `E:` ga qo'yish kerak.

### Apparat ma'lumoti

    ChannelA-DIMM0   8 GB  Samsung  S/N C9040CF5
    ChannelB-DIMM0   8 GB  Samsung  S/N C9040CF5   <- BIR XIL seriya raqami

Ikkala modul bir xil seriya raqamini ko'rsatishi g'ayrioddiy. Bu
SMBIOS/SPD ma'lumotining noto'g'ri yozilgani bo'lishi mumkin (eski
Sandy Bridge platformasida uchraydi), lekin e'tiborga olish kerak.
Tezlik maydoni ham bo'sh.

WHEA-Logger da apparat xatosi yo'q (bu RAM nosozligini INKOR ETMAYDI --
xotira xatolari odatda WHEA ga tushmaydi).

### XULOSA va TAVSIYA

`MEMORY_MANAGEMENT` (0x1A) qulashi 42 GB commit zaxirasi bor holda,
sog'lom ishlayotgan jarayon ustida sodir bo'lgan. Mantiqiy xotira
tugashi bilan izohlab bo'lmaydi. Eng ehtimoliy sabablar:

1. **Nosoz RAM moduli** (eng ehtimoliy)
2. Drayver xotirani buzishi
3. Eski platformada xotira nazoratchisi beqarorligi

**BIRINCHI QADAM: xotirani sinash.**

    mdsched.exe          (Windows'ning o'zida, qayta yuklanadi)

yoki ishonchliroq: MemTest86 (USB'dan, kamida 4 o'tish, bir necha soat).

**Bu build'dan MUHIMROQ:** nosoz RAM ma'lumotni JIMGINA buzadi.
Bu mashinada CustomMod ma'lumotlar bazasi bor (399k+ xabar, 2026-09-21
da merge qilingan). Agar RAM nosoz bo'lsa, u bazani ham buzishi mumkin
-- va 2026-08-27 dagi DB buzilishi ham esga tushadi.

### Build bo'yicha qaror

Foydalanuvchi qarori (2026-09-25): **PC'da build qilishdan voz
kechiladi.** Kod PC'da yoziladi, build laptopda qilinadi.
Reja: `LAPTOP-BUILD-PLAN.md` (endi PAUZADA emas, FAOL).

`D:\TBuild` dagi hamma narsa joyida qoladi (2059 obj saqlangan,
qulashdan keyin ham tekshirilgan) -- RAM tuzatilsa yoki ko'paytirilsa,
faqat bog'lash qoladi.

**Laptopga o'tishda OLIB KETILADIGAN tuzatishlar:**

`link-tuning.props` va `/p:ForceImportAfterCppTargets=` -- laptopda
KERAK BO'LMASLIGI mumkin, chunki u FASTLINK'siz ham uddalaydi. Lekin
agar laptopda ham xotira muammosi chiqsa, bu ikkalasi tayyor yechim:

1. Bayroqni ItemDefinitionGroup orqali berish (global `/p:` ISHLAMAYDI)
2. `/DEBUG:FASTLINK` uchun `LinkToolPath` ni 14.44 ga yo'naltirish
   (VS 2026 linkeri bu bayroqni qo'llab-quvvatlamaydi)

---

## 2026-09-26: E: diski bo'shatilmoqda, VS D: ga qayta o'rnatiladi, PC roli o'zgardi

### Qaror: PC'da tdesktop qurilmaydi

Foydalanuvchi qarori: bu mashinada faqat kod yoziladi va yengil
loyihalar (web-api, web-app) quriladi. tdesktop build laptopda.
Shuning uchun katta pagefile keraksiz: `C:` 16 -> 4 GB, `D:` 16 GB
o'z holicha. Commit limit 36 GB. `C:` da bo'sh joy 24.6 -> 37.2 GB.

Bu qaror /DEBUG:FASTLINK ishlamagani uchun EMAS -- u ishlagan
(29.4 -> 6.8 GB). Sabab: nosoz E: disk va shu mashinaning roli.
PC'da tdesktop qurish kerak bo'lsa, usul yuqorida yozilgan.

### Bajarilgan

- Pagefile E: dan olib tashlandi (E: xatolari 3 daqiqada 7 -> 0).
- 67.5 GB E: -> D:\E-disk-backup-20260925 ga nusxalandi.
- CustomMod arxivi D:\customizationMainFolder ga ilovaning o'z
  vositasi bilan ko'chirildi (registr: archiveRootPath=D:/...).
- Baza ko'chirishda buzildi (fayl 3 sahifaga kesilgan, sarlavha 4002
  sahifa deydi, fayl 3999). Dasturning o'z premigrate zaxirasidan
  tiklandi, integrity_check ok. Buzuq fayl saqlandi:
  db\actioned_messages.db.CORRUPT-20260925-2320
- Visual Studio E: dan o'chirildi (E:\Application's datas ~0.25 GB qoldi).

### SABOQ: ochiq SQLite bazasini qo'lda nusxalamang

Ilova ochiq turganda baza (WAL rejimi) nusxasi yaxlit chiqmaydi.
D:\E-disk-backup-20260925 dagi actioned_messages.db shu sababli
YAROQSIZ (media fayllar yaxshi). Ilovaning o'z ko'chirish
vositasidan foydalaning yoki avval ilovani yoping.

### VS qayta o'rnatish -- to'siqlar

- Product: D:\VS2026, Download cache: D:\VS2026-cache (ular bir-biriga
  kirmasligi shart: "root installation path cannot overlap with
  package cache path").
- "Shared components" maydoni kulrang va o'zgarmaydi (mashinada bir
  marta belgilanadi) -- 6 GB E: da qoladi.
- "System cache, tools, SDKs with fixed locations" 27.57 GB HAR DOIM
  C: ga tushadi. C: da kamida shuncha bo'sh joy kerak.
- ASP.NET and web development workload KERAK (customsync-server va
  boshqa web loyihalar). Uni olib tashlab joy tejab bo'lmaydi.

### O'rnatishdan keyin yangilanadigan yo'llar (hali BAJARILMAGAN)

    D:\TBuild\run-build.bat        vcvars64.bat + CMAKE (E:\VS2026 -> D:\VS2026)
    D:\TBuild\run-configure.bat    CMAKE_GENERATOR_INSTANCE (endi kerak emas bo'lishi mumkin)
    D:\TBuild\link-tuning.props    LinkToolPath / LibToolPath

E:\Applications main ichida Steam (~20 GB o'yin), Antigravity IDE,
Cisco Packet Tracer, SKLauncher, Mem Reduct bor. O'rnatilgan dasturlarni
papka nusxalab ko'chirib BO'LMAYDI (registr E: ga bog'langan).
Steam o'yinlarini Steam Settings > Storage orqali ko'chiring.

---

## 2026-09-26 (kech): E: BO'SHATILDI -- yakuniy holat

`E:` diski (ST1000DM003, Disk 1) nosoz ekani tasdiqlandi; undagi hamma
narsa `D:` ga olindi va `E:` deyarli bo'sh (930.3 / 932 GB bo'sh).
Qolgan: `E:\Application's datas` (0.67 GB, VS "shared" komponentlari)
va `bootTel.dat`. Diskni fizik almashtirish qoldi.

### Yangi joylar (PC)

| Nima | Endi | Avval |
|---|---|---|
| Visual Studio 2026 (18.10) | `D:\VS2026` | `E:\Application's datas\Visual Studio\Program Files` |
| VS yuklab olish keshi | `D:\DVS2026-cache` (registr: CachePath) | `E:` |
| Steam (engine + WoT Blitz) | `D:\Steam` (registr `d:/steam`) | `E:\Applications main` |
| SKLauncher ma'lumotlari | `D:\Apps\SKLauncher` | `E:\Applications main\SKLauncher` |
| CustomMod arxivi | `D:\customizationMainFolder` | `E:\customizationMainFolder` |
| Zaxira (67.5 GB) | `D:\E-disk-backup-20260925` | -- |
| Pagefile | C: 4 GB + D: 16 GB | C: 16 + E: 24 (BSOD sababi) |

Steam: papkani nusxalab (robocopy), keyin `D:\Steam\steam.exe` ni
ishga tushirish yetarli -- Steam registrni o'zi tuzatadi (kutubxona
papkasi Steam'ning o'zi ichida bo'lsa). SKLauncher: launcher `C:\Program
Files\sklauncher` da, u faqat MA'LUMOT papkasini `%APPDATA%\.sklauncher\
location.json` (`dataDir`) va `instances.json` (`directory`) dan oladi --
ikkalasi `D:\Apps\SKLauncher` ga o'zgartirildi (`.bak-20260926` nusxalari
shu yerda). Mem Reduct `C:\Program Files\Mem Reduct` da o'z nusxasi bor
edi (E: dagisi ortiqcha edi). Antigravity IDE va Cisco Packet Tracer
o'chirildi (kerak bo'lsa `D:\Apps` ga qayta o'rnatiladi; sozlamalari
`C:` da).

### Skriptlar yangilandi (D:\TBuild ildizi, repoda EMAS)

`run-build.bat`, `run-configure.bat`, `run-prepare.bat`,
`run-prepare-rest.bat`, `run-prepare.ps1`, `link-tuning.props`:
`E:\VS2026` -> `D:\VS2026`, `configure` dagi
`CMAKE_GENERATOR_INSTANCE=D:\VS2026,version=18.10.12217.157`.
Yangi VS da ikkala toolset ham bor: `14.44.35207` va `14.51.36231`.
`D:\TBuild\tdesktop\out\CMakeCache.txt` eski `E:` yo'llarini eslab
qolgan -- tdesktop shu yerda qurilsa, `out` ni tozalab `configure`
qaytadan ishlatish kerak. `out` (37.8 GB, chala 2 MB Telegram.exe) ATAYLAB
o'chirilmadi: 2059 obj -- bir kunlik ish, va FASTLINK bilan bog'lash
sog'lom ketgan edi.

### VS qayta o'rnatish -- to'siqlar (keyingi safar uchun)

1. **Product va Download cache bir-biriga kirmasin**: "The root
   installation path cannot overlap with package cache path".
2. **"Shared components" maydoni kulrang**, o'rnatuvchida o'zgarmaydi.
   U registrda saqlanadi:
   `HKLM\SOFTWARE\Microsoft\VisualStudio\Setup\SharedInstallationPath`.
   Eski o'rnatmadan `E:` da qolgan, shuning uchun Android NDK yozishda
   *"The request failed due to a fatal device hardware error"* bilan
   yiqildi (1024 amaldan 1 tasi). Yechim: Android komponentlarini
   olib tashlash (Modify). Disk almashtirilgach VS ni toza o'rnatib,
   bu qiymatni `D:\VS-Shared` ga qo'yish (admin, regedit).
3. `C:` ga har doim ~27.6 GB "fixed location" komponentlar tushadi
   (Windows SDK, .NET). C: da o'rnatish oldidan kamida ~30 GB bo'sh joy
   kerak. Buning uchun C: pagefile 16 -> 4 GB qilindi (bo'sh joy
   24.6 -> 37.2 GB). O'rnatishdan keyin C: bo'sh joyi ~19.9 GB.
4. **ASP.NET and web development workload KERAK** (customsync-server
   va boshqa web loyihalar). Uni olib tashlab joy tejab bo'lmaydi.
5. Xato logi: `%TEMP%\dd_setup_*_errors.log` (haqiqiy sabab shu yerda,
   oynadagi "Sorry, something went wrong" hech narsa demaydi).
   "Could not sync DCAT registration ... Access denied" -- zararsiz.

### Qolgan ishlar

- E: diskini fizik almashtirish (Disk 1, ST1000DM003, 92+ apparat xatosi)
- Shundan keyin VS ni toza o'rnatish + SharedInstallationPath
- PostgreSQL o'rnatish: o'rnatuvchi `D:\E-disk-backup-20260925\Installers\
  postgresql-17.6-1-windows-x64.exe` da
- RAM sinovi (`mdsched.exe`), 09-23 dagi klaster hali to'liq tushuntirilmagan
- Eski Uninstall registri yozuvlari (Steam, WoT Blitz, Cisco, Mem Reduct,
  Antigravity) hamon `E:` ga ishora qilishi mumkin -- zararsiz
- Steam va Public Desktop yorliqlari (admin talab qiladi) eski `E:` yo'liga
  ishora qilishi mumkin -- `D:\Steam\steam.exe` dan yangisini yarating
