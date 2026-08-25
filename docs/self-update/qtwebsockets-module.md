# `qtwebsockets` modulini mavjud Qt ustiga qurish

**Sana:** 2026-08-25
**Nima uchun:** Track C sync agenti `QWebSocket` ni ishlatadi
(spec §0.1). Bizning Qt 6.11.1 da bu modul qurilmagan —
`prepare.py:1560` faqat `qtbase qtimageformats qtsvg` ni oladi.

**Natija:** ✅ o'rnatildi va sinovdan o'tdi. Butun Qt qayta
qurilmadi.

---

## Qadamlar

```bash
# 1) Manba (3.9 MB)
cd C:/TBuild/Libraries/win64
git clone --depth 1 --branch v6.11.1 \
    https://code.qt.io/qt/qtwebsockets.git qtwebsockets

# 2) Konfiguratsiya
C:/TBuild/Libraries/win64/Qt-6.11.1/bin/qt-cmake.bat \
  -S C:/TBuild/Libraries/win64/qtwebsockets \
  -B C:/TBuild/Libraries/win64/qtwebsockets-build \
  -DOPENSSL_ROOT_DIR=C:/TBuild/Libraries/win64/openssl3 \
  -DZLIB_INCLUDE_DIR=C:/TBuild/Libraries/win64/zlib \
  -DZLIB_LIBRARY=C:/TBuild/Libraries/win64/zlib/Release/libzs.lib

# 3) Build + install — DIQQAT: RelWithDebInfo, Release EMAS
cmake --build . --config RelWithDebInfo --parallel 6
cmake --install . --config RelWithDebInfo
```

Natija: `Qt-6.11.1/lib/Qt6WebSockets.lib` (2.9 MB),
`lib/cmake/Qt6WebSockets/`, `include/QtWebSockets/` (21 header).

---

## Yo'lda chiqqan 3 ta to'siq — kelajak uchun

### 1. `WrapZLIB could not be found`

Qt tdesktop tayyorlagan zlib'ga bog'langan, lekin standart joyda
emas. Yechim — aniq yo'llar berish:

```
-DZLIB_INCLUDE_DIR=.../zlib
-DZLIB_LIBRARY=.../zlib/Release/libzs.lib
```

### 2. `WrapOpenSSL could not be found` (eng qiyini)

Header topildi (3.2.1), lekin kutubxona topilmadi. Sabab:
CMake `FindOpenSSL` `${OPENSSL_ROOT_DIR}/lib` da qidiradi,
bizda esa `openssl3/out/` da yotadi.

`-DOPENSSL_CRYPTO_LIBRARY=...` va `-DLIB_EAY_LIBRARY_RELEASE=...`
berish **YORDAM BERMADI** — `FindOpenSSL` ularni o'zi qayta
hisoblaydi.

**Ishlagan yechim** — standart joylashuvni junction bilan yasash
(nusxalash yo'q, admin kerak emas):

```cmd
mklink /J C:\TBuild\Libraries\win64\openssl3\lib ^
          C:\TBuild\Libraries\win64\openssl3\out
```

⚠️ Bu junction **saqlanishi kerak**. O'chirilsa modul qayta
qurilmaydi (mavjud `.lib` ishlashda davom etadi).

### 3. `error MSB8013: doesn't contain Release|x64`

Qt shunday sozlanganki, modul loyihalari `Debug` va
**`RelWithDebInfo`** konfiguratsiyalarini oladi — `Release` YO'Q.

`--config Release` ishlamaydi. **`--config RelWithDebInfo`**
ishlatiladi va u `Qt6WebSockets.lib` (suffikssiz = reliz
variant) hosil qiladi.

---

## Tekshiruv

`C:/TBuild/wstest` da minimal loyiha:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets)
target_link_libraries(wstest PRIVATE Qt6::Core Qt6::Network Qt6::WebSockets)
```

```cpp
#include <QtWebSockets/QWebSocket>
QWebSocket socket;
```

Natija: `-- TOPILDI Qt6WebSockets=6.11.1` va
`wstest.vcxproj -> wstest.exe` ✅

⚠️ Test loyihasida `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`
kerak bo'ldi — Qt `/MT` (statik) bilan qurilgan. tdesktop'ning
o'z build'i buni allaqachon to'g'ri qo'yadi, shuning uchun
tdesktop tomonda qo'shimcha sozlama kerak emas.

⚠️ Uzun yo'lda (`AppData/Local/Temp/claude/...`) MSBuild
`FileTracker : error FTK1011` beradi — bu yo'l uzunligi
muammosi, Qt bilan bog'liq emas. Qisqa yo'lda sinang.

---

## tdesktop'ga ulash (hali qilinmagan)

`Telegram/CMakeLists.txt` da:

```cmake
find_package(Qt6 REQUIRED COMPONENTS WebSockets)
target_link_libraries(Telegram PRIVATE Qt6::WebSockets)
```

Bu Track C plan 02 Task 9 da bajariladi.
