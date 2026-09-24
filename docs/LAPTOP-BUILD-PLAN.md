# Laptopda build qilish rejasi

> **HOLAT (2026-09-24 kechqurun): bu reja PAUZADA, shoshilinch emas.**
> U "PC'da bog'lash imkonsiz" degan xulosaga asoslangan edi, lekin
> o'sha xulosa NOTO'G'RI dalilga qurilgan: sinovlarda bayroqlar
> `link.exe` ga yetib bormagan (global `/p:` MSBuild item metadata'ni
> bekor qilmaydi). Batafsil: `PC-SETUP-STATE.md` ning eng oxirgi
> bo'limi. Avval PC'da `/DEBUG:FASTLINK` haqiqatan sinaladi; u ham
> yiqilsagina bu rejaga qaytiladi.
>
> Quyidagi qadamlar o'z-o'zidan to'g'ri va foydali -- laptopda qurish
> kerak bo'lganda ishlatiladi.

2026-09-24 da yozildi. Dastlabki sabab: PC'da bog'lash bosqichi
yiqilardi. Linkerga ~28 GB kirish keladi (2059 obj = 25.8 GB + 5.8 GB
kutubxona), PC'da 16 GB RAM va swap qisman HDD'da. Laptopda NVMe SSD
bor, shuning uchun u xuddi shu yukni uddalaydi.

**Muhim:** kirish hajmining 80-85 foizi -- `/Z7` (OldStyle) debug
ma'lumoti. Ya'ni laptopda ham to'liq yuk tushadi. Agar PC'da
`/DEBUG:FASTLINK` yoki `/Zi` ishlasa, laptop umuman kerak bo'lmasligi
mumkin.

Kod PC'da yoziladi, build laptopda qilinadi. Repo `origin/Oybek`
orqali sinxron, qo'lda hech nima ko'chirilmaydi.

---

## 0. Oldin: PC'dagi ishni olish

```
cd C:\TBuild\tdesktop
git pull --ff-only origin Oybek
git submodule update --init --recursive
```

Bu bilan quyidagilar keladi (hammasi PC'da tayyorlangan va sinalgan):

- `prepare.py` ning tarmoq chidamliligi: `curl --retry`, `libwebp`
  uchun `ARCH=$X8664`, `openssl3`/`ffmpeg`/`ada`/`qt` uchun `--depth 1`
- `docs/PC-SETUP-STATE.md` -- PC'dagi barcha to'siqlar va o'lchovlar
- `docs/MACHINES.md` -- yo'llar jadvali

---

## 1. Qt versiyasini TEKSHIRING (eng muhim qadam)

`MACHINES.md` laptopda **Qt 6.11.1** deb yozadi, repo esa endi
**6.11.2** talab qiladi. `Telegram/build/qt_version.py` `qt6`
argumenti berilganda `QT` ni MAJBURAN `6.11.2` qilib qo'yadi, ya'ni
`QT` muhit o'zgaruvchingiz e'tiborga olinmaydi.

```
dir C:\TBuild\Libraries\win64\Qt-6.11.2
```

- **Bor bo'lsa** -> 2-qadamga o'ting.
- **Yo'q bo'lsa** -> prepare Qt 6.11.2 ni manbadan yig'ishi kerak.
  Laptopda SSD bor, shuning uchun PC'dagidek azob bo'lmaydi, lekin
  baribir bir necha soat. Tarmoq uzilishlariga qarshi tuzatishlar
  allaqachon `prepare.py` da.

---

## 2. prepare

```
call "<VS yo'li>\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
cd /d C:\TBuild
call C:\TBuild\tdesktop\Telegram\build\prepare\win.bat qt6 silent
```

- `silent` SHART: usiz prepare o'zgargan bosqichda savol beradi va
  konsolsiz fon jarayonida darhol yiqiladi.
- Bosqich nomlarini BERMANG. Nomlangan bosqich `Forced` bo'lib qayta
  quriladi; nomsiz ishga tushirilsa kesh kalitiga qarab tayyorlarini
  `SKIPPING` qiladi.

---

## 3. configure

```
cd C:\TBuild\tdesktop\Telegram
configure.bat x64 qt6 -D TDESKTOP_API_ID=28454823 -D TDESKTOP_API_HASH=<hash>
```

`api_hash` ni `C:\Users\Oybek\Desktop\Credentials of Telegram-API.txt`
dan oling. Uni repo'ga, logga yoki commit xabariga YOZMANG.

**PC'dagi ikkita qo'shimchani laptopga KO'CHIRMANG** -- ular faqat
PC'ning muammolari uchun:

- `-D CMAKE_GENERATOR_INSTANCE=...` -- VS yo'lidagi apostrof uchun
- `-D CMAKE_VS_GLOBALS=VCToolsVersion=...` -- v143 targets buzuqligi uchun

Laptopda VS standart joyda bo'lsa, ikkalasi ham kerak emas.

---

## 4. Build

```
cmake --build C:\TBuild\tdesktop\out --config Release -- /m:3
```

`/m` ni yadro soniga qarab tanlang, bittasini OS uchun qoldiring.

### DIQQAT: bog'lash xotirasi

Laptopda ham linker **~26 GB commit** talab qiladi -- bu kirish
hajmidan kelib chiqadi (1037 obj = 22 GB + 5.8 GB kutubxona), mashinaga
bog'liq emas. Laptop buni NVMe SSD tufayli uddalaydi, lekin **commit
tomi yetarli bo'lishi kerak**.

Build'dan oldin tekshiring:

```powershell
$os=Get-CimInstance Win32_OperatingSystem
"Commit limit: {0:N1} GB" -f ($os.TotalVirtualMemorySize/1MB)
```

**35 GB dan kam bo'lsa** pagefile'ni kattalashtiring (SSD'da, `Initial
32768 / Maximum 32768`), aks holda `LNK1102: out of memory` olasiz.
PC'da aynan shu bo'lgan.

Bog'lash sekin ketsa, qotib qolganini ANIQLASH usuli (shunchaki
"ishlayapti" deb hisoblamang):

```powershell
$c1=(Get-Process link).CPU; Start-Sleep 60
"CPU o'sishi: {0:N1}s/60s" -f ((Get-Process link).CPU-$c1)
```

- 10 s dan yuqori -> sog'lom ish
- 5 s dan past -> swap'da qotgan, RAM/commit yetmayapti

---

## 5. Build'dan keyin

- Chiqish: `C:\TBuild\tdesktop\out\Release\Telegram.exe`
- Sinov nusxasi: `C:\Users\Oybek\Pictures\Release\` (log shu yerda)
- **tdata xavfi**: eski build'ni yangi `tdata` ustiga tushirmang --
  `key_datas` qayta yoziladi va BARCHA akkauntlar yo'qoladi
  (2026-09-11 da sodir bo'lgan).

---

## PC'da nima tayyor turibdi

PC'da faqat BOG'LASH qoladi -- qaytadan boshlash shart emas.
Hozir tayyor: prepare 33/33, Qt 6.11.2 (kesh kaliti bilan),
`configure` o'tgan, `Telegram.slnx`, API kalitlari, **2059 obj fayl**.

`C:` allaqachon bo'shatilgan (20.9 GB gacha) va pagefile SSD'ga
qo'yilgan (C: 16 GB + D: 8 GB, commit limit 40 GB). `LNK1102` shundan
keyin yo'qoldi.

Qolgan sinalmagan yo'llar, tartib bo'yicha:

1. `/DEBUG:FASTLINK` -- qayta kompilyatsiya KERAK EMAS, ~30-60 daqiqa
2. `/Zi` (`/Z7` o'rniga) -- qayta kompilyatsiya kerak, lekin to'liq
   mustaqil PDB beradi
3. `D:` dagi pagefile'ni `E:` ga ko'chirish (alohida, tezroq shpindel)

Ikkalasi ham `ForceImportAfterCppTargets` orqali beriladi -- `cmake`
submodule'iga tegmasdan. Usuli `PC-SETUP-STATE.md` oxirida.
