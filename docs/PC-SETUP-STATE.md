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
