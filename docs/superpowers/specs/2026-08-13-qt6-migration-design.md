# A6 — Qt 5.15.18 → Qt 6.11.1 migratsiyasi (dizayn)

**Holat:** Tasdiqlangan (2026-08-13, brainstorming orqali)

## 1. Maqsad

CustomMod fork'ni rasmiy `desktop-app`/`tdesktop` allaqachon qo'llab-quvvatlaydigan
Qt 6.11.1'ga o'tkazish. Qat'iy texnik zaruriyat yo'q (Qt5.15.18 hozir ham
ishlayapti), lekin bu:
- bizning `lib_ui` Qt5-moslik fork'imizni (`Oybek-M/lib_ui`) keraksiz qiladi —
  rasmiy `desktop-app/lib_ui`ga qaytish mumkin bo'ladi;
- upstream sync'lar bilan uzoq muddatli mosligimizni yaxshilaydi (upstream
  asta-sekin Qt5 qo'llab-quvvatlashni yo'qotib bormoqda).

## 2. Topilmalar (investigatsiya, 2026-08-13)

Rasmiy `tdesktop`/`desktop-app` build tizimi Qt6'ni **allaqachon to'liq
qo'llab-quvvatlaydi** — bu yangi/experimental ish emas:

- `Telegram/build/qt_version.py`: Windows'da `qt6` argumenti berilsa
  `QT=6.11.1` tanlanadi (aks holda default `5.15.18`).
- `Telegram/build/prepare/win.bat qt6` → `prepare.py qt6` — Qt6'ni source'dan
  build qiladigan rasmiy buyruq. Har bir kutubxona uchun **cache-key**
  mexanizmi bor (`Libraries/win64/cache_keys/`) — Qt-versiyaga bog'liq
  bo'lmagan kutubxonalar (ffmpeg, openssl va h.k.) qayta build qilinmaydi,
  faqat Qt-versiyaga bog'liq stage'lar ishga tushadi.
- `Libraries/win64/patches/` (desktop-app'ning rasmiy `patches` submodule'i)
  allaqachon `qtbase_6.11.0` patch papkasiga ega (build vaqtida `6.11.1`ga
  qarshi tekshiriladi — versiya farqi kichik, lekin **build vaqtida
  tasdiqlanishi kerak**, pastga qarang — Xavf #1).
- `Telegram/cmake/external/qt/CMakeLists.txt` allaqachon
  `QT_VERSION_MAJOR GREATER_EQUAL 6` shartlari bilan Qt6 uchun to'liq
  yo'l-yo'riqqa ega (ANGLE o'rniga to'g'ridan-to'g'ri D3D/DXGI linklash,
  `Qt6BundledHarfbuzz`/`Qt6BundledLibpng`/`Qt6BundledPcre2` va h.k.) — bu
  qism v7.0.9 sync orqali allaqachon kelgan, o'zgartirish shart emas.
- `configure.py` ham `qt6` argumentini qo'llab-quvvatlaydi.
- Bizning `custom_*.cpp`/`custom_*.h` fayllarimizda (CustomMod'ning o'z
  qo'shimchalari) Qt6'da olib tashlangan API'lar (`QRegExp`, `QTextCodec`,
  `qAsConst`, `QDesktopWidget`, `QStringRef`/`.midRef()`) **ishlatilmagan** —
  grep orqali tekshirildi, natija bo'sh.
- Ma'lum yagona nomuvofiqlik (upstream kodda, PROJECTS.md A6 qatorida
  ilgari qayd etilgan): `Telegram/SourceFiles/info/media/info_media_grid_zoom.cpp`
  dagi `QNativeGestureEvent::position()` chaqiruvi.

## 3. Ko'lam

**Ichida:**
1. Qt 6.11.1'ni source'dan build qilish (`win.bat qt6`), mavjud
   `Libraries/win64/Qt-5.15.18`ga tegmasdan, alohida `Qt-6.11.1` papkasida.
2. `.gitmodules` + submodule pointer: `Telegram/lib_ui`ni bizning Qt5-fork
   (`Oybek-M/lib_ui`, branch `oybek-qt5-patch`) dan rasmiy
   `desktop-app/lib_ui`ga qaytarish.
3. `configure.bat x64 qt6 -D TDESKTOP_API_ID=... -D TDESKTOP_API_HASH=...`
   bilan CMake'ni Qt6'ga qayta generatsiya qilish.
4. Build paytida chiqadigan **barcha** kompilyatsiya xatolarini tuzatish —
   ma'lum bo'lgani (`info_media_grid_zoom.cpp`) va build paytida yangi
   chiqadiganlari (oldindan to'liq bashorat qilib bo'lmaydi — Xavf #2).
5. **A11 Task 6** (story-signal feature'ining build+qo'lda tekshiruvi) shu
   BITTA Qt6 build siklida birga qamrab olinadi — alohida Qt5 build qilib
   keyin Qt6'ga o'tilmaydi, resurs tejash uchun.
6. To'liq qo'lda tekshiruv (ilova ishga tushishi, asosiy funksiyalar,
   CustomMod tugmalari, self-update, story-signal — pastga qarang §6).

**Tashqarida (bu task doirasida qilinmaydi):**
- Linux/macOS uchun Qt6 (Track B/self-update kabi allaqachon Windows'ga
  cheklangan holat davom etadi — Linux/macOS mashina yo'qligi sababli).
- `qt6_highsierra` patch'lari (faqat macOS'ga tegishli, `prepare.py`da
  ko'rilgan, Windows uchun aloqasi yo'q).
- Qt6'ning yangi funksiyalaridan (masalan `QtQuick`) foydalanish — faqat
  mavjud Widgets-asosidagi kodni ishlab turishini ta'minlash yetarli.

## 4. Xavf-xatarlar va noaniqliklar

1. **Patch versiya farqi:** `Libraries/win64/patches` da `qtbase_6.11.0`
   bor, lekin `qt_version.py` `6.11.1`ni so'raydi. `prepare.py` build
   vaqtida bu patch'larni qanday moslashtirishini (aniq versiya mosligini
   talab qiladimi, yoki eng yaqinini oladimi) oldindan bilmaymiz —
   **build ishga tushganda birinchi tekshiriladigan narsa**. Agar
   mos kelmasa: `Libraries/win64/patches` submodule'ini yangilash
   (`git submodule update --remote`) yoki `qt_version.py`da versiyani
   `6.11.0`ga moslashtirish variantlari bor.
2. **Noma'lum kompilyatsiya xatolari:** `info_media_grid_zoom.cpp`dan
   tashqari, Qt6'ga o'tishda yana boshqa fayllarda ham Qt5-only API
   chiqishi mumkin (bu odatiy holat — PROJECTS.md A6 qatorida ham
   ilgari shu xavf qayd etilgan). Bular build vaqtida bittalab
   aniqlanadi va tuzatiladi (systematic-debugging yondashuvi bilan,
   root-cause asosida — masalan `#if QT_VERSION` shart bilan yechish
   kerakmi yoki haqiqiy API o'zgarishi kerakmi, har birida farqlanadi).
3. **Build vaqti:** Qt'ni source'dan build qilish odatiy ~34 daqiqalik
   loyiha build'idan sezilarli uzoqroq (taxminan soatlab, aniq vaqt
   noma'lum — bu birinchi urinish). **User signal berganda boshlanadi**
   (kelishilgan, savol-javob bosqichida).
4. **lib_ui submodule pointer'i:** rasmiy `desktop-app/lib_ui`ga
   qaytishda, agar u orada boshqa (Qt5 bilan mos kelmaydigan) o'zgarishlar
   qo'shgan bo'lsa muammo yo'q — chunki biz Qt6'ga o'tayapmiz, ularning
   Qt6-asosidagi kodi endi to'g'ridan-to'g'ri mos keladi (aslida bizning
   fork shu yerda faqat Qt5 uchun MAXSUS patch qo'shgan edi).

## 5. Fallback / xavfsizlik

Foydalanuvchida `out/Release`dagi hozirgi ishlab turgan (Qt5.15.18-asosidagi)
build allaqachon boshqa joyga clone qilingan va undan mustaqil foydalanadi —
shuning uchun Qt6 migratsiyasi davomida asosiy `tdesktop` repo/build papkasida
muammo chiqsa ham, ishlab turgan nusxa xavfsiz qoladi. Qo'shimcha
fallback-saqlash choralari (masalan Qt5 papkasini maxsus backup qilish)
**shart emas** — foydalanuvchi buni allaqachon hal qilgan.

## 6. Sinov rejasi (yakuniy build'dan keyin)

A9/A11'da ishlatilgan formatga o'xshab, qo'lda tekshiruv bandlari:
1. Ilova ishga tushadi, login holati saqlangan.
2. Asosiy UI (chat ro'yxati, xabar yuborish/qabul qilish) ishlaydi.
3. Custom Window ochiladi, barcha bo'limlar (General + Peers) render
   bo'ladi, hech qanday vizual regressiya yo'q (Qt6'da widget rendering
   farq qilishi mumkin — tekshirish kerak).
4. Story ko'rish/anonim-story funksiyasi ishlaydi (A11 xavfsizlik
   invarianti — `markAsRead()` chaqirilmasligi).
5. Self-update ("Check for Updates") ishlaydi.
6. AntiDelete/AntiEdit ishlaydi.
7. **A11 Task 6 uchun maxsus:** story post-vaqti Activity History'ga
   yoziladi, media-backup (agar yoqilgan bo'lsa) ishlaydi.

## 7. Bog'liq hujjatlar

- `docs/superpowers/PROJECTS.md` — A6 qatori, "Umumiy ustuvorlik tartibi".
- `docs/superpowers/plans/2026-08-09-story-activity-signal-plan.md` — A11
  Task 6 shu yerda joylashgan, Qt6 build bilan birga bajariladi.
