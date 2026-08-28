# A15: Bot Token Authorization (1-bosqich) — Texnik Spesifikatsiya va Sinov Qo'llanmasi

## 1. Umumiy Ma'lumot va 1-bosqich Maqsadi

Telegram MTProto protokolida botlar uchun alohida avtorizatsiya usuli (uth.importBotAuthorization) mavjud. Biroq bot akkauntlar user-only metodlarga (messages.getDialogs, contacts.getContacts, ccount.updateStatus) so'rov yuborganda serverdan BOT_METHOD_INVALID yoki USER_BOT_INVALID xatolarini qaytarishi kutiladi.

Ushbu 1-bosqichning asosiy maqsadi — to'liq bot klientini yaratishdan oldin, MTProto xatti-harakatini HAQIQIY bot tokeni bilan tekshirish (probe qilish):
1. Foydalanuvchiga bot token orqali kirish imkoniyatini beruvchi kirish ekrani yaratildi.
2. Ishga tushish vaqtida avtomatik yuboriladigan user-only so'rovlar xatolari ushlanib, cheksiz qayta urinish yoki dastur qulashining oldi olindi.
3. Diagnostika uchun har bir so'rov natijasi [BOTPROBE] prefiksi bilan log faylga yozilishi ta'minlandi.

---

## 2. O'zgartirilgan va Yaratilgan Fayllar

1. **Yaratildi:** Telegram/SourceFiles/intro/intro_bot_token.h — BotTokenWidget klassi deklaratsiyasi.
2. **Yaratildi:** Telegram/SourceFiles/intro/intro_bot_token.cpp — Bot token kiritish maydoni, ogohlantirish matni, MTPauth_ImportBotAuthorization so'rovi va inish(result) orqali sessiya yaratish logikasi.
3. **O'zgartirildi:** Telegram/CMakeLists.txt — yangi intro_bot_token fayllari CMake loyihasiga qo'shildi.
4. **O'zgartirildi:** Telegram/SourceFiles/intro/intro_qr.h va Telegram/SourceFiles/intro/intro_qr.cpp — QR login ekraniga Bot token orqali kirish havolasi qo'shildi (goNextOrBack<BotTokenWidget>()).
5. **O'zgartirildi:** Telegram/SourceFiles/apiwrap.cpp:
   - equestContacts() (contacts.getContacts): fail handler qo'shildi, [BOTPROBE] logi yozildi, BOT_METHOD_INVALID / USER_BOT_INVALID da contactsLoaded = true bilan to'xtatildi.
   - equestMoreDialogs() (messages.getDialogs): fail handler qo'shildi, [BOTPROBE] logi yozildi, BOT_METHOD_INVALID / USER_BOT_INVALID da listReceived = true qilinib, cheksiz qayta yuklash tsikli to'xtatildi.
6. **O'zgartirildi:** Telegram/SourceFiles/api/api_updates.cpp (ccount.updateStatus):
   - .done() va .fail() handlerlari qo'shildi, [BOTPROBE] logi yozildi.

---

## 3. Log Fayli Joylashuvi

Telegram/SourceFiles/logs.cpp dagi _logsFilePath funksiyasiga ko'ra:
- Log fayli Telegram Desktop'ning ishchi katalogida (cWorkingDir()) joylashgan **log.txt** (yoki debug rejimida **DebugLogs/log.txt**) fayliga yoziladi.
- Odatdagi standart o'rnatishlarda bu papka:
  - Windows: %APPDATA%\Telegram Desktop\log.txt (yoki portativ versiyada Telegram.exe joylashgan papkada log.txt).

---

## 4. Foydalanuvchi Uchun Sinov Yo'riqnomasi

1. **Bot token oling:** Telegram ichida @BotFather botiga kiring va test uchun yangi bot yarating (yoki mavjud botingiz tokenini oling).
2. **Akkaunt qo'shing:**
   - Telegram Desktop yon menyusidan **Add Account** (Akkaunt qo'shish) tugmasini bosing.
   - QR kod ekrani pastida paydo bo'lgan **Bot token orqali kirish** havolasini bosing.
   - Chiqqan ekranga @BotFather bergan tokenni kiriting va **Davom etish** (Next) tugmasini bosing.
3. **Log faylini tekshiring:**
   - Dastur ishga tushgandan so'ng log.txt faylini oching.
   - Matn ichidan **[BOTPROBE]** qatorlarini qidiring (masalan: Ctrl + F -> [BOTPROBE]).
4. **Natijalarni qayd eting:**
   Quyidagi satrlar bo'yicha qanday natija qaytganini aniqlang:
   - [BOTPROBE] contacts.getContacts -> ... (OK yoki FAIL: XATO_KODI)
   - [BOTPROBE] messages.getDialogs -> ... (OK yoki FAIL: XATO_KODI)
   - [BOTPROBE] account.updateStatus -> ... (OK yoki FAIL: XATO_KODI)

Ushbu log natijalari 2-bosqichda bot klientining dialoglar va xabarlar arxitekturasini qanday qurish kerakligini aniq belgilab beradi.
