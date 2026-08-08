# Loyiha yo'nalishlari — holat jadvali

Bu fayl qaysi ish **hozir faol**, qaysi biri **to'xtatib qo'yilgan** va
qaysi biri **hali muhokama bosqichida** ekanini ko'rsatadi. Yangi
sessiya boshlanganda birinchi shu yerga qarang.

Oxirgi yangilanish: 2026-08-09

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
| A3 | Log shovqinini kamaytirish (`API Warning: not loaded minimal channel applied.`) | Past ustuvorlik, tekshirilmagan |
| A4 | Upstream v7.0.7 → v7.0.9 sync (to'liq `dev→SafeWall→Customizations↔Oybek` zanjiri orqali) | ✅ 2026-08-06, 100 commit, 1 konflikt (`lib_ui` submodule) |
| A5 | Build v7.0.9 + haqiqiy self-update sinovi | ✅ 2026-08-08: build+reliz+update-apply hammasi tasdiqlandi (2 marta chiqarildi — `publish.ps1`dagi path bug tuzatilgach) |
| A6 | Qt 5.15.18 → Qt 6.5+ ga o'tish | 🕓 Keyingi tasklarga qo'shildi (2026-08-07). Qat'iy texnik zaruriyat yo'q — sof tarixiy: `Libraries\win64`da faqat Qt5 SDK bor. O'tish `lib_ui` fork'ini keraksiz qiladi va upstream bilan uzoq muddatli mosligini yaxshilaydi, lekin alohida katta ish (Qt6 SDK sozlash + qayta konfiguratsiya + boshqa yashirin Qt5/6 nomuvofiqliklar chiqishi mumkin — bitta misol allaqachon topilgan: `info_media_grid_zoom.cpp`dagi `QNativeGestureEvent::position()`). |
| A7 | VS2022→VS2026 ko'chishi bilan bog'liq build-muhit tuzatishlari | ✅ 2026-08-08 — toolset, QT env var, api_id/api_hash, `DESKTOP_APP_DISABLE_AUTOUPDATE` qayta yoqildi. Tafsilot: `docs/self-update/HANDOFF.md` §2.6 |
| A8 | `publish.ps1` Packer path bug (release-staging prefiksi) | ✅ 2026-08-08 — tuzatildi, v7.0.9 qayta chiqarildi, sinovdan o'tdi. Tafsilot: `docs/self-update/HANDOFF.md` §0.1 |
| A9 | Upstream (rasmiy) versiya tekshiruvchisi — Custom Window'da rasmiy tdesktop'da yangi reliz bor-yo'qligini avto/qo'lda bildirish | ✅ 2026-08-09: to'liq implement, build va real-muhitda qo'lda sinov (7/7 band) muvaffaqiyatli. Spec: `docs/superpowers/specs/2026-08-08-upstream-update-checker-design.md`, reja: `docs/superpowers/plans/2026-08-08-upstream-update-checker-plan.md` (5/5 task ✅, yakuniy code review APPROVED) |
| A10 | "Bitta tugma" sync+build+publish pipeline — foydalanuvchi mavjud bo'lganda, A9 yangilanish borligini bildirgach, sync→build→3 mirror'ga reliz ketma-ketligini bitta buyruq/tugma bilan boshlash (hozirgi 4-5 qo'lda qadam o'rniga) | 🕓 Keyingi tasklarga qo'shildi (2026-08-08). **Ataylab yarim-avtomatik** — to'liq unattended emas: build resurs-qoidasi ("build oldidan doim so'rash"), merge-konflikt qarorlari va reliz oldidan tekshiruv hali inson ishtirokini talab qiladi. A9 asosida keladi, undan keyin brainstorming qilinadi. |
| A11 | Story post-vaqti — yashirilgan/berkitilgan last-seen'ni bilvosita aniqlash uchun qo'shimcha signal: kuzatilayotgan user story qo'yganda, uni ko'rish vaqti mavjud "Activity History" arxiviga avtomatik yoziladi (boshqa manba topilmaganda foydalaniladigan fallback). Ko'rish paytida mavjud `storyAnonymousView` sozlamasi ishlatiladi — ko'rganimiz yashirin qoladi. | 🕓 Keyingi tasklarga qo'shildi (2026-08-08), user tomonidan taklif qilindi. Activity History Log (`custom_activity_history.cpp`/`custom_db.cpp`ning `activity_history` jadvali) ustiga qurilishi kerak — implementatsiyadan oldin alohida brainstorming/spec kerak. |

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

## C — Multi-device sync ekotizimi (5 app) ⏸️ TO'XTATIB QO'YILGAN

Spec va planlar **to'liq yozilgan**, implement boshlanmagan.
Foydalanuvchi aniq aytdi: boshlash vaqtini u belgilaydi.

**Hujjatlar:**
- Spec: [`specs/2026-07-29-multi-device-sync-backend-design.md`](specs/2026-07-29-multi-device-sync-backend-design.md)
- Planlar: [`plans/2026-07-29-multi-device-sync-00-index.md`](plans/2026-07-29-multi-device-sync-00-index.md) va undan keyingi 6 ta fayl

**Muhim:** bu ish **tdesktop repozitoriysida bajarilmaydi.** Backend,
web app va capture service **alohida papkada** yaratiladi.

> ⚠️ **Ishni boshlashdan oldin foydalanuvchidan qaysi papkadan
> foydalanishni so'rash SHART.** O'zim tanlamayman.

**Boshlash tartibi (foydalanuvchi ruxsatidan keyin):**
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
