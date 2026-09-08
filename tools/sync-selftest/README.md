# sync_selftest

`custom_sync_record` va `custom_sync_crypto` ni
`docs/sync-protocol/test-vectors.json` ga qarshi tekshiradi.

Bu ikki fayl ataylab tdesktop'ning hech qanday sarlavhasiga bog'liq
emas, shuning uchun ular bu yerda **sekundlar ichida** kompilyatsiya
qilinadi — to'liq build (~34 daqiqa) kerak emas.

## Qurish

```
C:/TBuild/Libraries/win64/Qt-6.11.1/bin/qt-cmake.bat ^
  -S C:/TBuild/tdesktop/tools/sync-selftest ^
  -B C:/TBuild/st-build ^
  -DOPENSSL_ROOT_DIR=C:/TBuild/Libraries/win64/openssl3 ^
  -DZLIB_INCLUDE_DIR=C:/TBuild/Libraries/win64/zlib ^
  -DZLIB_LIBRARY=C:/TBuild/Libraries/win64/zlib/Release/libzs.lib

cmake --build C:/TBuild/st-build --config Release
```

Ikkita tuzoq, ikkalasi ham amalda uchradi:

- **ZLIB yo'llari shart.** `Qt6::Core` uni tranzitiv talab qiladi va
  topa olmasa `find_package(Qt6)` yiqiladi. Xato xabari zlib haqida
  emas, Qt haqida bo'ladi — adashtiradi.
- **Build papkasi `%TEMP%` ostida bo'lmasin.** MSBuild'ning FileTracker
  komponenti u yerda `FTK1011` bilan yiqiladi.

## Ishga tushirish

```
set PATH=C:\TBuild\Libraries\win64\Qt-6.11.1\bin;%PATH%
C:\TBuild\st-build\Release\sync_selftest.exe ^
  C:\TBuild\tdesktop\docs\sync-protocol\test-vectors.json
```

`Qt6Core.dll` uchun `PATH` shart, aks holda dastur jimgina ishga
tushmaydi.

Chiqish kodi: 0 — hammasi mos, 1 — nomuvofiqlik yoki holatlar soni
kutilganidan farq qildi, 2 — faylni o'qib bo'lmadi.

## Nima uchun holatlar soni tekshiriladi

`record_id` JSON'da **obyekt**, massiv emas — holatlar `record_id.cases`
ichida. Agar kod uni massiv deb o'qisa, Qt bo'sh massiv qaytaradi, sikl
tanasi hech qachon ishlamaydi va dastur "hammasi mos keldi" deb
chiqadi, aslida bittasini ham tekshirmagan holda.

Shuning uchun `main.cpp` aynan **11 ta** holat tekshirilganini talab
qiladi. Vektorlarga yangi holat qo'shilsa, bu son ham yangilanishi
kerak — bu ataylab shunday.

## To'liq build'siz sintaksis tekshiruvi

`custom_sync_client.cpp` va `custom_sync_outbox.cpp` selftest'ga
kirmaydi (birinchisi QtNetwork'ga, ikkinchisi SQLite va `custom_db`'ga
tayanadi), ya'ni ular faqat 34 daqiqalik to'liq build'da
kompilyatsiya qilinadi. 2026-09-03 da aynan shu sabab bitta
kompilyatsiya xatosi commit'ga kirib ketdi.

Ularni build'siz tekshirish mumkin. Fayllar tdesktop'ning PCH'idan
faqat uchta narsani oladi (`Fn`, `operator""_q`, `not_null`),
shuning uchun kichik shim yetarli:

```cpp
// pch_shim.h
#pragma once
#include <functional>
#include <QtCore/QString>
#include <QtCore/QByteArray>
template <typename Signature> using Fn = std::function<Signature>;
[[nodiscard]] inline QByteArray operator""_q(const char *d, std::size_t n) {
    return QByteArray::fromRawData(d, n);
}
[[nodiscard]] inline QString operator""_q(const char16_t *d, std::size_t n) {
    return QString::fromRawData(reinterpret_cast<const QChar*>(d), n);
}
```

```
cl /nologo /W4 /std:c++20 /Zc:__cplusplus /EHsc /Zs /permissive- ^
   /DQT_NO_KEYWORDS /DQT_NO_CAST_FROM_BYTEARRAY ^
   /FI pch_shim.h ^
   /I C:/TBuild/tdesktop/Telegram/SourceFiles ^
   /I C:/TBuild/Libraries/win64/qt_5.15.18/qtbase/src/3rdparty/sqlite ^
   /I C:/TBuild/Libraries/win64/openssl3/include ^
   /I C:/TBuild/tdesktop/Telegram/ThirdParty/GSL/include ^
   /I C:/TBuild/Libraries/win64/Qt-6.11.1/include ^
   /I C:/TBuild/Libraries/win64/Qt-6.11.1/include/QtCore ^
   /I C:/TBuild/Libraries/win64/Qt-6.11.1/include/QtNetwork ^
   Telegram/SourceFiles/custom_sync_client.cpp
```

Yo'llar ATAYLAB absolyut. `Libraries/` repo ichida emas, uning YONIDA
(`C:\TBuild\Libraries`), buyruq esa repo ildizidan ishga tushiriladi --
nisbiy yozilsa `QtCore/QString` topilmay `C1083` beradi va xato Qt
o'rnatilmagandek ko'rinadi.

`cl` ni topish uchun avval:

```
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
```

(VS 2022 emas, **18**. Aniq yo'lni `vswhere.exe -latest -property installationPath` beradi.)

`/Zs` — faqat sintaksis, obyekt fayl yozilmaydi. Define'lar muhim:
`QT_NO_CAST_FROM_BYTEARRAY` va `QT_NO_KEYWORDS` haqiqiy build'da
yoqilgan va ularsiz tekshiruv haqiqatdan yumshoqroq bo'ladi.

## Bitta faylni to'liq build'siz kompilyatsiya qilish

`/Zs` shim usuli tdesktop'ning butun sarlavha daraxtini tortadigan
fayllarda ishlamaydi (`custom_db.cpp`, `custom_tab_*.cpp`). Ular uchun
34 daqiqalik to'liq build SHART emas -- MSBuild bitta faylni
kompilyatsiya qila oladi:

```
call "C:\Program Files\Microsoft Visual Studio8\Community\VC\Auxiliary\Buildcvars64.bat"
msbuild C:\TBuild	desktop\out\Telegram\Telegram.vcxproj /nologo /v:m ^
  /p:Configuration=Release /p:Platform=x64 ^
  /p:SelectedFiles=C:\TBuild	desktop\Telegram\SourceFiles\custom_tab_sync.cpp ^
  /t:ClCompile
```

PCH allaqachon qurilgani uchun bu ~1 daqiqa oladi. Link bosqichi
bajarilmaydi, ya'ni `Telegram.exe` ga tegilmaydi.

2026-09-07 da `custom_tab_sync.cpp` (800+ qator UI kodi) uchta ketma-ket
to'liq build'ni yeb qo'ydi, chunki har safar faqat bitta kompilyatsiya
xatosi topilardi. Shu buyruq bilan uchalasi ham bir necha daqiqada
topilishi mumkin edi.

🔴 **`CMakeLists.txt` ga tegdingizmi — avval qayta generatsiya
qiling.** MSBuild `/t:ClCompile` CMake'ni qayta yurgizmaydi, ya'ni
ESKI `.vcxproj` bilan, ESKI bayroqlar bilan kompilyatsiya qilasiz.
Yangi `target_compile_definitions` bo'lsa, kodingiz `#ifdef` ostida
butunlay tashlab yuboriladi va kompilyator "muvaffaqiyat" deydi —
aslida u sizning kodingizni umuman ko'rmagan:

```
cmake -S C:/TBuild/tdesktop -B C:/TBuild/tdesktop/out
grep -c YOUR_MACRO out/Telegram/Telegram.vcxproj    # 0 bo'lmasligi kerak
```

2026-09-08 da Task 9 aynan shunday "tekshirilgan" edi: `.vcxproj`
09-07 15:37 dan, `CMakeLists.txt` esa 09-08 12:13 dan edi.

⚠️ Yangi Qt signali/sloti qo'shsangiz, kompilyatsiya yetarli emas —
u AUTOMOC'da yaratiladi va xato faqat LINK bosqichida chiqadi.
Tekshirish: `msbuild out/Telegram/Telegram_autogen.vcxproj`, keyin
`grep <signal> out/Telegram/Telegram_autogen/include_Release/*/moc_*.cpp`.

MSB8028 ogohlantirishlari (`intermediate directory ... shared`) normal --
ular tdesktop'ning tashqi kutubxonalariga tegishli, sizning kodingizga
emas.

`custom_db.cpp` bu usul bilan tekshirilmaydi — u tdesktop'ning butun
sarlavha daraxtini tortadi. Undagi chaqiruvlarni alohida kichik probe
faylida takrorlab tekshirish mumkin.
