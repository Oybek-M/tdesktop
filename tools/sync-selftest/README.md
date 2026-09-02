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
