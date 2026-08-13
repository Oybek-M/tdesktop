# Loyiha yo'nalishlari — holat jadvali

Bu fayl qaysi ish **hozir faol**, qaysi biri **to'xtatib qo'yilgan** va
qaysi biri **hali muhokama bosqichida** ekanini ko'rsatadi. Yangi
sessiya boshlanganda birinchi shu yerga qarang.

Oxirgi yangilanish: 2026-08-13

---

## ⚡ Umumiy ustuvorlik tartibi (2026-08-13 qayta rejalashtirildi)

Foydalanuvchi limit tugashi sababli bu yerda aniq bosqichlarga bo'lib
qo'yildi — boshqa Claude sessiyasida ham davom ettirish mumkin bo'lsin
uchun.

1. **1-bosqich — mayda, tez bitadigan tasklar.** Hozircha alohida
   build talab qilmaydigan ochiq kod-darajasidagi ish yo'q (A11'ning
   qolgan qismi ham build talab qiladi — pastga qarang). Agar shu
   bosqichda yangi mayda task paydo bo'lsa, avval shular yopiladi.
2. **2-bosqich — A6 (Qt 5.15.18 → Qt 6.5+ migratsiyasi).** To'liq
   yakunlanadi.
3. **BITTA umumiy build+qo'lda tekshiruv** — 1- va 2-bosqichdagi
   barcha o'zgarishlarni (jumladan **A11 Task 6**: story-signal
   feature'ning build+qo'lda tekshiruvi) **bitta build siklida**
   birga qamrab oladi, alohida-alohida build qilinmaydi (resurs
   tejash uchun).
4. **3-bosqich — Track C: `customsync-server`.** Yangi, mustaqil
   repo. Joylashuv **kelishildi**: top-level yangi papka
   `C:\Users\Oybek\Documents\Projects programming\customsync-server`
   (2-tavsiya emas — ikkalasi ham `Telegram\Telegram\` ichiga
   joylashgan tdesktop'ning o'z build-scaffold'i, `DesktopPrivate`
   maxfiy kalitlari bilan bir joyda — shu sabab rad etildi). Boshlash
   tartibi: `01a` backend poydevori → ... (to'liq ro'yxat pastda, C
   bo'limida).

> ⚠️ **Implement qilishni (Qt6 migratsiya ham, customsync-server ham)
> boshlashdan oldin har safar foydalanuvchidan alohida ruxsat so'rash
> SHART.** Bu hujjat yangilanishi hali implement boshlanganini
> anglatmaydi — faqat rejani qayd etadi.

---

## A — tdesktop CustomMod ⚡ FAOL

Hozirgi asosiy ish. Boshqa yo'nalishlar bunga aralashmasligi kerak.

**Repo:** `Telegram/tdesktop`, branch `Oybek`
**Holat:** ✅ **v7.0.9 build qilindi, chiqarildi va real-world
self-update sinovi muvaffaqiyatli o'tdi** (2026-08-08). Birinchi
urinishda update-apply ishlamagan edi — ildiz sabab (`Packer.exe`ning
`release-staging\` prefiksini noto'g'ri arxivlashi) topilib tuzatildi,
paket qayta chiqarildi, qayta sinovda versiya haqiqatan v7.0.7'dan
v7.0.9'ga o'zgardi. Tafsilot:
[`../self-update/HANDOFF.md`](../self-update/HANDOFF.md) §0.1.

**Ochiq vazifalar:**

| № | Vazifa | Holat |
|---|---|---|
| A1 | To'liq qo'lda tekshiruv | ✅ 2026-08-01, kritik muammo yo'q |
| A2 | Upstream v7.0.5 → v7.0.7 sync | ✅ 2026-08-01, `2e61fdcbc2` (162 commit, 2 konflikt) |
| A3 | Log shovqinini kamaytirish (`API Warning: not loaded minimal channel applied.`) | 🟡 2026-08-09: root cause topildi (`data_session.cpp:966`, rasmiy kod, `LOG` shartsiz yoziladi — `DEBUG_LOG`ga o'tkazish mumkin edi), lekin user tuzatmaslikni tanladi — kod o'zgarishsiz qoldi. Past ustuvor, kerak bo'lsa keyinroq qaytiladi. |
| A4 | Upstream v7.0.7 → v7.0.9 sync (to'liq `dev→SafeWall→Customizations↔Oybek` zanjiri orqali) | ✅ 2026-08-06, 100 commit, 1 konflikt (`lib_ui` submodule) |
| A5 | Build v7.0.9 + haqiqiy self-update sinovi | ✅ 2026-08-08: build+reliz+update-apply hammasi tasdiqlandi (2 marta chiqarildi — `publish.ps1`dagi path bug tuzatilgach) |
| A6 | Qt 5.15.18 → Qt 6.11.1 ga o'tish | 🔨 **Build'gacha bo'lgan barcha tayyorgarlik ishlari yakunlandi (2026-08-13).** Brainstorming orqali tasdiqlangan, spec: `docs/superpowers/specs/2026-08-13-qt6-migration-design.md`. Investigatsiya natijasi: rasmiy `tdesktop`/`desktop-app` build tizimi Qt6'ni allaqachon to'liq qo'llab-quvvatlaydi (`Telegram\build\prepare\win.bat qt6`), bu YANGI/experimental ish emas — Windows'da rasmiy default hamon Qt5 (Windows 7 moslik uchun), Qt6 ixtiyoriy `qt6` bayrog'i bilan. Bajarildi: (1) `lib_ui` submodule Qt5-fork (`Oybek-M/lib_ui`)dan rasmiy `desktop-app/lib_ui`ga qaytarildi (commit `cd9d356ddd`) — bu Qt6'gacha Qt5 build'ni vaqtincha buzadi, kutilgan holat; (2) butun `SourceFiles` bo'ylab Qt5-only API sweep qilindi (`QRegExp`, `QTextCodec`, `qAsConst`, `QStringRef`, `QDesktopWidget` va h.k. — topilmadi); (3) 3 ta `QNativeGestureEvent::pos()/globalPos()` chaqiruvi `position()/globalPosition()`ga o'zgartirildi (commit `914c1b1a95`: `info_media_grid_zoom.cpp`, `editor_paint.cpp`, `media_view_overlay_widget.cpp`). **Qolgan yagona qadam:** `win.bat qt6` (Qt'ni source'dan build qilish, taxminan soatlab) — **faqat user signal berganda** boshlanadi (laptop band bo'lmagan vaqtda). Shundan keyin `configure.bat x64 qt6 ...` + to'liq build, so'ng build paytida chiqishi mumkin bo'lgan qo'shimcha kompilyatsiya xatolarini tuzatish. A11 Task 6 bilan **bitta umumiy build**da tekshiriladi. |
| A7 | VS2022→VS2026 ko'chishi bilan bog'liq build-muhit tuzatishlari | ✅ 2026-08-08 — toolset, QT env var, api_id/api_hash, `DESKTOP_APP_DISABLE_AUTOUPDATE` qayta yoqildi. Tafsilot: `docs/self-update/HANDOFF.md` §2.6 |
| A8 | `publish.ps1` Packer path bug (release-staging prefiksi) | ✅ 2026-08-08 — tuzatildi, v7.0.9 qayta chiqarildi, sinovdan o'tdi. Tafsilot: `docs/self-update/HANDOFF.md` §0.1 |
| A9 | Upstream (rasmiy) versiya tekshiruvchisi — Custom Window'da rasmiy tdesktop'da yangi reliz bor-yo'qligini avto/qo'lda bildirish | ✅ 2026-08-09: to'liq implement, build va real-muhitda qo'lda sinov (7/7 band) muvaffaqiyatli. Spec: `docs/superpowers/specs/2026-08-08-upstream-update-checker-design.md`, reja: `docs/superpowers/plans/2026-08-08-upstream-update-checker-plan.md` (5/5 task ✅, yakuniy code review APPROVED) |
| A10 | "Bitta tugma" sync+build+publish pipeline — foydalanuvchi mavjud bo'lganda, A9 yangilanish borligini bildirgach, sync→build→3 mirror'ga reliz ketma-ketligini bitta buyruq/tugma bilan boshlash (hozirgi 4-5 qo'lda qadam o'rniga) | 🕓 Keyingi tasklarga qo'shildi (2026-08-08). **Ataylab yarim-avtomatik** — to'liq unattended emas: build resurs-qoidasi ("build oldidan doim so'rash"), merge-konflikt qarorlari va reliz oldidan tekshiruv hali inson ishtirokini talab qiladi. A9 asosida keladi, undan keyin brainstorming qilinadi. |
| A11 | Story post-vaqti — yashirilgan/berkitilgan last-seen'ni bilvosita aniqlash uchun qo'shimcha signal: kuzatilayotgan user story qo'yganda, uni ochmasdan/ko'rmasdan story'ning qo'yilgan vaqti mavjud "Activity History" arxiviga yoziladi (fallback signal) + ixtiyoriy story media (foto/video) zaxirasi. | 🔨 2026-08-09: 5/6 task implement+review qilindi va push qilindi (Task 1-5: settings maydoni, story-vaqt signali hook, media-backup logikasi, Custom Window toggle, History Box formatlash). **Faqat Task 6 (build + qo'lda tekshiruv) qoldi** — bu alohida emas, A6 (Qt6) bilan **bitta umumiy build**da qilinadi (2026-08-13 qayta rejalashtirildi, yuqoridagi ustuvorlik bo'limiga qarang). Spec: `docs/superpowers/specs/2026-08-09-story-activity-signal-design.md`, reja: `docs/superpowers/plans/2026-08-09-story-activity-signal-plan.md`. Xavfsizlik: mavjud "Hikoyalarni anonim ko'rish" (`ShouldAnonymousStory`) real ikkinchi akkountdan tekshirilib, to'g'ri ishlayotgani tasdiqlandi (§5, spec ichida). |
| A12 | 🔴 **KRITIK — moliyaviy ta'sirli crash:** Telegram Stars sovg'a qilish (gift) jarayonida ilova qulaydi. Reprodutsiya (2026-08-09, foydalanuvchi tomonidan): (1) CustomMod client'da stars sotib olib boshqa userga gift qilishga urinilganda — Visa kartadan pul yechildi, asosiy oynaga qaytganda **crash**, qayta ishga tushirilganda stars **noto'g'ri o'z profiliga** kredit bo'lgan (gift qilinmagan). (2) Xuddi shu urinish **rasmiy (official, tuzatilmagan) tdesktop client'da qaytarilgan** — yana Visa'dan pul yechildi, gift qabul qiluvchi chatida 100 stars paydo bo'lgani zahoti yana **crash** (to'liq yopilish), lekin qayta ishga tushirilganda bu safar gift **to'g'ri yetib borgan**. | ⏸️ **PAUZA QILINDI (2026-08-09).** `telegramdesktop/tdesktop` GitHub issue'lari tekshirildi (`gh search issues`) — aniq mos crash-report topilmadi. Eng yaqin: [#29972](https://github.com/telegramdesktop/tdesktop/issues/29972) "Concurrency issues in Telegram marketplace purchases" (stars yechilib gift boshqa/noto'g'ri joyga borishi) — lekin bu **server-tomon (MTProto) race condition** deb topilib, maintainer "bu tdesktop-specific emas, bugs.telegram.org'ga" deb yopgan, hech qanday fix/PR yo'q. User qarori: agar mavjud ma'lumot topilmasa, o'zimiz resurs sarflab debug qilmaymiz — bu ehtimol upstream/server xatosi, rasmiy tomondan tuzatilsa keyingi sync orqali o'zi keladi. **Keyinroq qayta ko'rib chiqish mumkin** agar: (a) user yana takrorlasa va aniqroq repro/crash-log to'plansa, (b) GitHub'da keyinchalik tegishli issue/fix paydo bo'lsa. |

**v7.0.7 bilan kelgan yangi bog'liqliklar:** `Telegram/ThirdParty/libcbor`
va `libfido2` (FIDO2/passkey). Build muammosiz o'tdi.

**v7.0.9 sync bilan kelgan muhim o'zgarish:** `Customizations` branch'i
eski (Saidjon-davri) tarixdan tozalanib, `Oybek`ga tekislandi —
force-push qilindi. `Telegram/lib_ui` submodule'i endi rasmiy
`desktop-app/lib_ui` o'rniga bizning fork'imizga (`Oybek-M/lib_ui`)
ishora qiladi — bizning Qt5 moslik patch'imiz uchun (rasmiyda yo'q,
faqat fork'da). Tafsilot: `docs/self-update/HANDOFF.md` §2.5.

---

## B — Self-update mexanizmi ✅ Windows tayyor, Linux/macOS bloklangan

Har rebuild'dan keyin ilovani fleshka/cloud orqali qo'lda tarqatish
muammosini hal qiladi. Bir nechta desktop qurilma va boshqa
foydalanuvchilar (masalan aka) paydo bo'lganda bu muammo keskinlashadi.

**Holat (2026-08-08):** Windows uchun **to'liq ishlab turibdi,
production'da, va endi to'liq real-world update-apply sinovidan
o'tgan** (v7.0.7 → v7.0.9, haqiqiy "Check for Updates" tugmasi orqali,
fayl almashtirish bosqichigacha tasdiqlangan — ilgari faqat signature
tekshiruvi sinalgan edi). Yo'lda topilgan `publish.ps1` path bug'i
(§0.1) tuzatildi. Kirish nuqtasi:
[`../self-update/HANDOFF.md`](../self-update/HANDOFF.md).

Keyingi versiyani chiqarish bitta buyruq: `.\tools\publish\release.ps1`
(tdesktop repo ichida, `out\Release` build qilingandan keyin).

**Linux/macOS (Task 5-6):** kod ko'rib chiqilgan, qo'shimcha
o'zgartirish kerak emasligi tasdiqlangan, lekin build/sinov qadamlari
**Linux/macOS mashina yo'qligi sababli bloklangan** — Packer.exe'ning
chiqish fayl nomi compile-time aniqlanadi, cross-compile qilib
bo'lmaydi. Mashina paydo bo'lganda davom ettiriladi.

**A yo'nalishiga bog'liqligi:** yo'q. Update server oddiy statik fayl
(nginx) — B yo'nalishiga ham bog'liq emas, mustaqil bajarilishi mumkin.

---

## C — Multi-device sync ekotizimi (5 app) ⏸️ NAVBATDA (3-bosqich)

Spec va planlar **to'liq yozilgan**, implement boshlanmagan.
2026-08-13: foydalanuvchi bilan ustuvorlik qayta rejalashtirildi —
Track A/Qt (A6) to'liq va bitta build bilan yakunlangach boshlanadi
(yuqoridagi "Umumiy ustuvorlik tartibi" bo'limiga qarang).

**Repo joylashuvi — KELISHILDI (2026-08-13):** yangi top-level papka
```
C:\Users\Oybek\Documents\Projects programming\customsync-server
```
(Foydalanuvchi taklif qilgan ikkita variant — `...\Telegram\` va
`...\Telegram\Telegram\` — rad etildi: ikkalasi ham aslida
tdesktop'ning o'z C++ build-scaffold'i, `DesktopPrivate` maxfiy
signing-kalitlari bilan bir joyda, umumiy "Telegram loyihalari" uyi
emas. `Telegram BOTs\` konvensiyasiga o'xshab, mustaqil top-level
papka tanlandi.)

**Hujjatlar:**
- Spec: [`specs/2026-07-29-multi-device-sync-backend-design.md`](specs/2026-07-29-multi-device-sync-backend-design.md)
- Planlar: [`plans/2026-07-29-multi-device-sync-00-index.md`](plans/2026-07-29-multi-device-sync-00-index.md) va undan keyingi 6 ta fayl

**Muhim:** bu ish **tdesktop repozitoriysida bajarilmaydi.** Backend,
web app va capture service **alohida papkada** (yuqorida) yaratiladi.

> ⚠️ **Implement qilishni boshlashdan oldin foydalanuvchidan yakuniy
> ruxsat so'rash SHART** — papka joylashuvi kelishilgan bo'lsa ham,
> boshlash vaqtini alohida tasdiqlaydi.

**Boshlash tartibi (foydalanuvchi ruxsatidan keyin, A6+build
yakunlangach):**
1. `01a` — backend poydevori
2. `01b` — backend sync yadrosi
3. `02` — tdesktop sync agenti (bu qism tdesktop repo'sida)
4. `03` — web controller
5. `04` — storage lifecycle
6. `05` — always-on TDLib capture service

---

## D — Mobil klientlar 🕓 KEYINGA QOLDIRILGAN

`tmobile-android` (exteraGram fork) va `tmobile-ios` (Telegram-iOS fork).

**Holat:** hali muhokama qilinmagan. Foydalanuvchi aniq aytdi —
tafsilotlarni keyin batafsil muhokama qilamiz.

**Allaqachon ma'lum bo'lgan cheklovlar** (C yo'nalishi muhokamasida
aniqlangan, muhokama boshlanganda esga olinsin):
- iOS ilovani to'xtatadi va Telegram o'chirish hodisasi uchun push
  yubormaydi → iOS'da capture tabiatan to'liq bo'lmaydi, u amalda
  asosan **viewer** bo'lib qoladi
- Samsung Note 10+ batareya boshqaruvi Android fon xizmatini o'ldiradi
- iOS uchun macOS + Xcode kerak (foydalanuvchida bor)

---

## Doimiy qoidalar

- Build ishga tushirishdan **oldin doim so'rash** — ~34 daqiqa oladi va
  boshqa og'ir ilovalar bilan raqobatlashadi
- Commit + push faqat `origin/Oybek` ga; `upstream` ga **hech qachon**
- Commit'larda `Co-Authored-By` trailer **ishlatilmaydi**
