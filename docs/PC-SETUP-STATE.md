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
