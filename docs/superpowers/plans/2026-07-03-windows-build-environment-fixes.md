# Windows Build Environment Fixes — 2026-07-03

Bu hujjat "Dynamic Device Spoof" (`2026-06-30-dynamic-device-spoof.md`)
feature'ni build qilish jarayonida chiqqan, **feature'ning o'ziga aloqasi
yo'q**, sof Windows build-muhiti muammolarini va ularning yechimini
qayd etadi. Agar loyiha qaytadan clone qilinsa yoki submodule'lar
yangilansa, xuddi shu muammolar qaytalanishi mumkin — shu sabab bu yerda
hujjatlashtirildi.

## 1. PDB race condition (C1090 "PDB API call failed, error code 12")

**Sabab:** MSVC'ning `/MP` (MultiProcessorCompilation) yoqilgan holda,
bitta loyihaning barcha `.cpp` fayllari **bitta umumiy compiler PDB**
fayliga (`vc143.pdb`, `DebugInformationFormat=ProgramDatabase` / `/Zi`)
yozadi. Kam RAM + yuqori parallellik + katta translation unit'lar (Qt,
webrtc) kombinatsiyasida bu yozish mexanizmi (hatto `/FS` bilan ham)
vaqti-vaqti bilan qulab tushdi va PDB faylini buzib qo'ydi.

**Yakuniy yechim (`out/**/*.vcxproj`, barcha 42 fayl):**
```xml
<DebugInformationFormat>OldStyle</DebugInformationFormat>  <!-- /Z7 -->
```
`/Zi` (ProgramDatabase) o'rniga `/Z7` (OldStyle) — har bir `.obj` fayl
debug ma'lumotini o'zida saqlaydi, umumiy compiler PDB fayli **umuman
ishlatilmaydi**. Bu xato turi tuzilishi jihatidan endi yuz berolmaydi.
Linker darajasidagi yakuniy `Telegram.pdb` (debugging uchun) o'zgarishsiz
ishlab turadi.

**Oraliq (yetarli bo'lmagan) qadamlar, tarix uchun:**
- `/FS` (Force Synchronous PDB Writes) qo'shildi — yordam berdi, lekin
  og'ir yuk ostida yetarli bo'lmadi.
- Buzilgan `vc143.pdb`ni qo'lda o'chirish orqali vaqtincha davom etish
  mumkin edi, lekin sabab yo'qolmagani uchun qayta-qayta chiqaverdi.

## 2. CPU parallellik balansi (`ProcessorNumber` / VS "max parallel project builds")

`out/**/*.vcxproj` da har bir loyiha uchun:
```xml
<MultiProcessorCompilation>true</MultiProcessorCompilation>
<ProcessorNumber>8</ProcessorNumber>
```
`ProcessorNumber` — bitta loyiha ichidagi parallel `cl.exe` soni (16 ta
logical processor'ning yarmi). Bundan tashqari, Visual Studio'ning shaxsiy
sozlamasi (**Tools → Options → Projects and Solutions → Build and Run →
"maximum number of parallel project builds"**) ni **4** ga o'rnatish
tavsiya qilingan — bu tashqi (loyihalar orasidagi) parallellikni
cheklaydi, RAM sarfini nazoratda ushlaydi.

> 16 ga (100%) yoki 14 ga (~87%) oshirish tezlashtiradi, lekin 16GB RAM'da
> "Telegram" loyihasi (minglab fayl) yakka o'zi shuncha thread bilan
> compile qilinganda RAM to'lib, Windows freeze bo'lib qolishi kuzatildi.
> 8 — RAM va tezlik orasidagi ishlagan muvozanat.

## 3. Qt5 / Qt6 API mos kelmasligi

Loyiha **Qt 5.15.18** ga qarshi build qilinadi
(`Libraries/win64/Qt-5.15.18`), lekin `dev` branch'dan merge qilingan
ba'zi kod va `lib_ui` submodule pin (`4ef4b13`, `editor-31` branch)
Qt 6.5+ da qo'shilgan API'larni ishlatgan edi.

### 3a. `Telegram/lib_ui/ui/accessible/ui_accessible_widget.h` / `.cpp` (submodule)

`QAccessible::Attribute`, `QAccessibleAttributesInterface`,
`QAccessibleSelectionInterface` — bularning barchasi Qt 6.5+ da
qo'shilgan, Qt 5.15'da mavjud emas. `#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)`
guard'lari bilan o'rab qo'yildi — Qt5'da bu funksionallik shunchaki
compile qilinmaydi (accessibility darajasi biroz kamayadi, lekin ilova
ishlaydi).

> ⚠️ **MUHIM:** Bu — `lib_ui` **submodule ichidagi** lokal patch, hech
> qayerga (na `origin`, na `desktop-app/lib_ui` upstream'iga) push
> qilinmagan — chunki bu repo bizniki emas. Agar kelajakda
> `git submodule update` ishlatilsa yoki lib_ui qayta clone qilinsa, bu
> patch **yo'qoladi** va build yana buziladi. Xohlasangiz, bu patch'ni
> `.patch` fayl sifatida saqlab, submodule yangilanganda qayta qo'llash
> mumkin.

### 3b. `Telegram/SourceFiles/info/media/info_media_grid_zoom.cpp:188`

`QNativeGestureEvent::position()` — faqat Qt6'da bor (Qt5'da faqat
`localPos()`). Fix:
```cpp
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	const auto anchor = e->position().toPoint();
#else // Qt >= 6.0.0
	const auto anchor = e->localPos().toPoint();
#endif // Qt < 6.0.0
```
Bu — main repo'dagi oddiy fayl, git orqali normal commit/push qilinadi
(submodule muammosi yo'q).

## 4. Yetishmagan prebuilt kutubxonalar (`Libraries/win64/zlib`)

Rasmiy `Telegram/build/prepare/prepare.py` skripti Windows uchun zlib'ni
`-DZLIB_BUILD_MINIZIP=ON` bilan build qilishi kerak edi, lekin mahalliy
zlib checkout (`51b7f2ab...`) bu bosqichsiz o'tkazilgan bo'lib chiqdi —
natijada linker kerakli fayllarni topolmadi:

- `libzsd.lib` / `libzs.lib` — mavjud `zlibstaticd.lib`/`zlibstatic.lib`
  fayllaridan nusxa olib, kutilgan nom bilan saqlandi.
- `libminizipsd.lib` / `libminizips.lib` — `Libraries/win64/zlib/contrib/minizip/`
  ichidagi source (`zip.c`, `unzip.c`, `ioapi.c`, `iowin32.c`) to'g'ridan-
  to'g'ri `cl.exe`/`lib.exe` bilan qo'lda compile qilindi, zlib bilan bir
  xil sozlamalarda (`/DZLIB_WINAPI`, mos runtime: Debug=`/MTd`,
  Release=`/MT`).

> Bu fayllar `Libraries/win64/` ichida, git bilan boshqarilmaydi (build
> input, source emas) — shuning uchun commit qilinmaydi, lekin agar
> `Libraries/win64` papkasi qayta tayyorlansa (`win.bat` qayta ishga
> tushirilsa), bu qadam ham qayta bajarilishi kerak bo'ladi.

## Xulosa jadvali

| Muammo | Joylashuv | Turi | Commit qilinganmi? |
|---|---|---|---|
| PDB race (`/Zi`→`/Z7`) | `out/**/*.vcxproj` (42 fayl) | Build config (generated) | ❌ Yo'q — `out/` git-ignored, cmake qayta configure qilinsa yo'qoladi |
| CPU parallellik | `out/**/*.vcxproj` + VS shaxsiy sozlamasi | Build config | ❌ Yo'q (yuqoridagi kabi) |
| QAccessible Qt5 compat | `Telegram/lib_ui/ui/accessible/*` | Submodule patch | ⚠️ Faqat lokal commit, push qilinmagan |
| QNativeGestureEvent Qt5 compat | `Telegram/SourceFiles/info/media/info_media_grid_zoom.cpp` | Main repo | ✅ Ha |
| zlib/minizip lib fayllari | `Libraries/win64/zlib/**` | Build input (binary) | ❌ Yo'q — git bilan boshqarilmaydi |
