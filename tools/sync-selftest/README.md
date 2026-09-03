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
   /I Telegram/SourceFiles ^
   /I Libraries/win64/qt_5.15.18/qtbase/src/3rdparty/sqlite ^
   /I Libraries/win64/openssl3/include ^
   /I Telegram/ThirdParty/GSL/include ^
   /I Libraries/win64/Qt-6.11.1/include ^
   /I Libraries/win64/Qt-6.11.1/include/QtCore ^
   /I Libraries/win64/Qt-6.11.1/include/QtNetwork ^
   Telegram/SourceFiles/custom_sync_client.cpp
```

`/Zs` — faqat sintaksis, obyekt fayl yozilmaydi. Define'lar muhim:
`QT_NO_CAST_FROM_BYTEARRAY` va `QT_NO_KEYWORDS` haqiqiy build'da
yoqilgan va ularsiz tekshiruv haqiqatdan yumshoqroq bo'ladi.

`custom_db.cpp` bu usul bilan tekshirilmaydi — u tdesktop'ning butun
sarlavha daraxtini tortadi. Undagi chaqiruvlarni alohida kichik probe
faylida takrorlab tekshirish mumkin.
