# Self-Update Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** CustomMod'ni ilova ichidan yangilanadigan qilish — har rebuild'dan keyin fleshka yoki cloud orqali qo'lda tarqatish shart bo'lmasin. Boshqa foydalanuvchilar (masalan aka) yangilanishni o'zi olsin.

**Holat:** Reja yozilgan, **implement boshlanmagan** — foydalanuvchi ruxsatini kutmoqda.

---

## 1. Asosiy topilma: mexanizm allaqachon mavjud

Noldan hech narsa yozilmaydi. tdesktop'ning butun updater mashinasi
kodda turibdi va sinovdan o'tgan:

| Fayl | Vazifasi |
|---|---|
| `Telegram/SourceFiles/core/update_checker.cpp` | Tekshirish, yuklab olish, **RSA imzo tekshiruvi** |
| `Telegram/SourceFiles/_other/packer.cpp` | Paketni yasash va **imzolash** |
| `Telegram/SourceFiles/_other/updater_win.cpp` | Windows'da ishlab turgan `.exe` ni almashtirish |
| `Telegram/SourceFiles/_other/updater_linux.cpp` | Linux uchun shu ish |
| `Telegram/SourceFiles/_other/updater_osx.m` | macOS uchun shu ish |

U shunchaki **o'chirilgan**:

```
out/CMakeCache.txt:  DESKTOP_APP_DISABLE_AUTOUPDATE:BOOL=ON
```

Ishning katta qismi — mavjud mexanizmni **o'zimizga qaratish**, qayta
yozish emas.

---

## 2. ⚠️ Eng jiddiy xavf

> Updater'ni yoqishdan **oldin** URL va public key almashtirilishi SHART.

Aks holda "yangilanishni tekshirish" **rasmiy Telegram build'ini** yuklab
olib, fork'ni butunlay bosib ketadi — CustomMod'ning barcha
funksionalligi yo'qoladi va foydalanuvchi buni ilova qayta ishga
tushgandan keyingina biladi.

Shuning uchun bu plandagi tartib qat'iy: **avval kalit va URL, keyin
yoqish.** Task 3 dan oldin Task 2 bajarilmasa, bu xato sodir bo'ladi.

---

## 3. Arxitektura

### 3.1 Uch mirror — tanlov emas, zaxira

Uchala manzil ham **bir xil imzolangan paketni** beradi. Klient ularni
tartib bo'yicha sinaydi va birinchi javob berganidan oladi.

```
1. VPS (himoyalangan)   https://updates.example.uz/secure/…   ← asosiy
2. VPS (oddiy)          https://updates.example.uz/pub/…      ← zaxira
3. GitHub Releases      https://github.com/…/releases/…       ← oxirgi chora
```

**Nima uchun bu xavfsiz:** paket RSA bilan imzolangan va imzo klientda
tekshiriladi. Mirror buzilgan yoki ishonchsiz bo'lsa ham, u yaroqli
yangilanish yasay olmaydi — imzo mos kelmasa paket rad etiladi.

**Foydasi:** VPS o'chsa yoki domen muddati tugasa, yangilanish GitHub
orqali kelaveradi. Bitta nuqtaga bog'liqlik yo'q.

### 3.2 api_id muammosi: mirror'larni yopish

Build ichida shaxsiy `api_id` / `api_hash` bor
(`TDESKTOP_API_ID=28454823`). Suiiste'mol qilinsa Telegram uni
bloklaydi va **barcha** build'laringiz ishlamay qoladi.

#### Teskari muhandislikdan himoya qilib bo'ladimi — yo'q

Buni oshkora aytish kerak, chunki noto'g'ri taxmin ustiga qaror
qurilmasin:

**Klient tomonidagi sirni himoya qilib bo'lmaydi.** Ilova `api_id`
bilan Telegram'ga ulanishi kerak → u ilova ichida bo'lishi shart →
ilovani ishga tushira olgan odam uni chiqarib ola oladi. Bu
printsipial cheklov, implementatsiya sifati masalasi emas.

Obfuskatsiya (VMProtect, Themida va shu kabilar) narxni oshiradi,
lekin to'smaydi. Ustiga: qimmat, build'ni buzishi mumkin, antivirus
false-positive beradi va har yangi kompilyator versiyasida qayta
sozlash kerak. Bu loyiha uchun foydasi xarajatiga arzimaydi.

#### Buning o'rniga: muammoni yo'q qilamiz

Xavf shifrlashning kuchida emas — **paketning ochiq joyda turishida**.
Ochiq joyda turmasa, teskari muhandislik savoli umuman tug'ilmaydi.

Shuning uchun **uchala mirror ham yopiq bo'ladi:**

| Mirror | Himoya |
|---|---|
| VPS (secure) | HTTP Basic auth |
| VPS (pub) | Maxfiy prefiks (URL'ni bilmagan topa olmaydi) |
| GitHub | **Private repo + Personal Access Token** |

GitHub'ni yopiq qilish yangi mexanizm talab qilmaydi — bu VPS-secure
uchun allaqachon rejalashtirilgan `Authorization` sarlavhasining aynan
o'zi. Ya'ni bitta mexanizm, uchta manzil.

#### ⚠️ Qaror (2026-08-02): shifrlash qatlami SKIP qilindi

Implement paytida topilgan gap: bu qatlamni qo'shish `update_checker.cpp`
ning yuklab olish yo'liga real deshifrlash kodi qo'shishni talab
qilardi (packer/reliz skripti shifrlaydi, lekin hech bir task klientda
deshifrlashni qo'shmagan edi — bu o'zi ham bir kamchilik edi).

Ko'rib chiqilgach, foydasi past deb topildi: AES kaliti **xuddi shu
binary ichida** joylashadi (ilova bilan birga tarqaladi) — demak bu
faqat "tokenni bilmagan, lekin binary'ni ham olmagan" tor holatga
foyda beradi. Binary'ga ega har kim (ya'ni ilovani o'rnatgan har kim)
kalitni ham darhol oladi, shuning uchun bu qatlam haqiqiy hujumchidan
deyarli himoya qilmaydi — xuddi api_id himoyalanmasligi haqidagi
3.2-bo'limning yuqoridagi xulosasi bilan bir xil mantiq.

Qaror: **shifrlash qo'shilmaydi.** Faqat ikki qatlam qoladi — yopiq
mirror (token/parol) + RSA imzo. Bular real himoya beradi: mirror
buzilsa ham soxta paket imzosiz o'tolmaydi, imzo kaliti esa
binary'da yo'q (faqat public key bor).

```
Yopiq mirror → paketni olish uchun token kerak
Imzo         → mirror buzilsa ham soxta yangilanish yasab bo'lmaydi
```

### 3.3 Reliz oqimi

```
build → packer (SIQADI + imzolaydi) → 3 mirror'ga yuklash → tekshirish
```

`packer` build natijasini **siqadi va imzolaydi** — ikkalasini birga.
Biz ustiga shifrlash va yuklashni qo'shamiz.

Bularning hammasi **bitta skript** bilan bajariladi. Qo'lda qadam qancha
kam bo'lsa, xato ehtimoli shuncha kam.

### 3.4 Kalitlar

| Kalit | Qayerda | Commit qilinadimi |
|---|---|---|
| RSA private (imzolash) | Faqat sizning mashinangizda | **Yo'q, hech qachon** |
| RSA public (tekshirish) | Manba kodda | Ha — u ochiq bo'lishi kerak |
| Shifrlash kaliti | Manba kodda | Ha — baribir binary ichida bo'ladi |

⚠️ **Private kalitni yo'qotsangiz** boshqa yangilanish chiqara olmaysiz —
barcha o'rnatilgan nusxalar qo'lda almashtirilishi kerak bo'ladi.
Uni zaxiralash Task 2 ning majburiy qismi.

---

### 3.5 Kelajakdagi kengayish — bugundan hisobga olinadi

Bu tizim oxir-oqibat **barcha 5 ta app** ni qamraydi va boshqaruv
backend'ga o'tadi. Bugun buni qurmaymiz, lekin **keyin qayta yozishga
majbur qilmaydigan qarorlar** qabul qilamiz.

**Yakuniy ko'rinish:**

| Nima | Qanday yangilanadi |
|---|---|
| `tdesktop` | Ilova ichidan (bu plan) |
| `tmobile-android` | Ilova ichidan, xuddi shu manifest formati |
| `tmobile-ios` | Ilova ichidan (App Store'siz, ad-hoc) |
| `server-backend` | **CI/CD** — o'zini avtomatik yangilaydi |
| `server-controller` | Backend bilan birga |

Va: **backend boshqaruv panelidan klientlarga yangilanish chiqarish** —
ya'ni yangi reliz yuklab, qaysi platformaga va qachon tarqatilishini
o'sha yerdan boshqarish.

#### Buning bugungi dizaynga ta'siri — bitta qoida

> **Manifest formati statik nginx va backend uchun bir xil bo'lishi
> shart.**

Ya'ni klient "manifest fayl" so'raydi va uni kim bergani — nginx'dagi
statik fayl yoki backend'dagi endpoint — **ahamiyatsiz** bo'lishi kerak.

Buning natijasi:

1. Bugun statik fayllar bilan boshlaymiz — backend'ni kutmaymiz
2. Backend tayyor bo'lganda u shunchaki **yana bitta mirror** bo'ladi
3. Ko'chish = URL almashtirish, klient kodi o'zgarmaydi
4. Statik mirror'lar **doimiy zaxira** bo'lib qoladi — backend o'chsa
   yangilanish baribir keladi

Bu muhim: agar manifest'ni backend'ga bog'lab qo'ysak, backend
o'chganda yangilanish ham to'xtaydi — va aynan shunday paytda
tuzatish chiqarish kerak bo'lishi mumkin.

#### Manifest'ga bugundan kiritiladigan maydonlar

Kelajakda kerak bo'ladi, hozir ishlatilmasa ham **bo'sh joy
qoldiriladi** (keyin format o'zgarmasligi uchun):

```
platform      win | linux | mac | android | ios
channel       stable | beta          (kelajakda bosqichma-bosqich tarqatish uchun)
min_version   undan eski versiyalar to'g'ridan-to'g'ri yangilana olmaydi
mandatory     majburiy yangilanishmi
notes         reliz izohi (UI'da ko'rsatiladi)
```

Task 1 da aniqlangan mavjud format bu maydonlarni qabul qilmasa —
ular alohida yon fayl (`meta.json`) sifatida beriladi, asosiy formatni
buzmasdan.

#### Bu ish qayerda rejalashtiriladi

Backend tomonidagi qism **bu planda emas** — u multi-device
seriyasiga `06 — release management` sifatida qo'shiladi va C
yo'nalishi boshlanganda yoziladi. Bu yerda faqat **format
muvofiqligi** ta'minlanadi.

---

## 4. Bosqichlar

| Bosqich | Mazmuni | Nima uchun shu tartibda |
|---|---|---|
| **1** | Mavjud updater kontraktini o'rganish | Format ma'lum bo'lmasa qolgan hamma narsa taxminga quriladi |
| **2** | Kalitlar va infratuzilma | Yoqishdan oldin bo'lishi shart (2-bo'limdagi xavf) |
| **3** | Windows uchun yoqish va sinash | Siz va aka ishlatadigan platforma |
| **4** | Reliz skripti va mirror'lar | Qo'lda qadamlarni yo'qotadi |
| **5** | Linux | Windows ishlagach mexanik takrorlash |
| **6** | macOS | Eng murakkab: codesign / notarization |

---

## Task 1: Mavjud updater kontraktini hujjatlashtirish

Bu **tadqiqot** task'i. Kod yozilmaydi. Sababi: `update_checker.cpp`
qanday URL tuzilishini va qanday fayl formatini kutishini aniq bilmasdan
turib server tomonini qurish — taxminga qurish demakdir, va u faqat
birinchi sinovda ma'lum bo'ladi.

**Files:**
- Read: `Telegram/SourceFiles/core/update_checker.cpp`
- Read: `Telegram/SourceFiles/_other/packer.cpp`
- Create: `docs/self-update/updater-contract.md`

- [ ] **Step 1: Tekshiruv oqimini o'qish**

`update_checker.cpp` da quyidagilarni aniqlang va yozib oling:

1. Qaysi URL so'raladi? (konstanta nomi va to'liq shakli)
2. Javob qanday formatda kutiladi? (matn, JSON, binar?)
3. Versiya raqami qanday taqqoslanadi?
4. Paket fayl nomi qanday hosil qilinadi?
5. `UpdatesPublicKey` qayerda ta'riflangan va nechta kalit bor?
6. `HttpChecker` va `MtpChecker` — ikkalasi ham ishlatiladimi, qaysi
   biri birinchi?

- [ ] **Step 2: Paket formatini o'qish**

`packer.cpp` da:

1. Paket ichida nima bor? (fayllar ro'yxati, siqishmi?)
2. Imzo qayerda va qanday joylashadi?
3. Packer qanday argumentlar bilan chaqiriladi?
4. Chiqish fayli nomi qanday hosil bo'ladi?

- [ ] **Step 3: Kontraktni yozish**

`docs/self-update/updater-contract.md` da aniq yozing:

- Server tomonda qanday fayllar, qanday yo'llarda bo'lishi kerak
- Har birining formati (misol bilan)
- Klient qanday tartibda so'raydi

**Bu hujjat keyingi barcha task'lar uchun manba bo'ladi.** Taxmin
qoldirmang — kodda topilmagan narsani "aniqlanishi kerak" deb
belgilang, o'ylab topmang.

- [ ] **Step 4: Kelajakka moslikni baholash**

3.5-bo'limdagi maydonlar (`platform`, `channel`, `min_version`,
`mandatory`, `notes`) mavjud formatga sig'adimi — aniqlang.

Sig'masa: ular alohida `meta.json` yon fayl sifatida beriladi degan
qarorni kontrakt hujjatida yozib qo'ying. Bu keyinroq backend
boshqaruv paneli qo'shilganda formatni **buzmasdan** kengaytirish
imkonini beradi.

**Nima uchun hozir:** format bir marta chiqarilgach, uni o'zgartirish
eski versiyadagi klientlarni sindiradi — ular yangi formatni
tushunmaydi va yangilana olmaydi, ya'ni ularni qo'lda almashtirishga
to'g'ri keladi. Aynan shu narsadan qochish uchun bu ish qilinmoqda.

- [ ] **Step 4: Commit**

```bash
git add docs/self-update/updater-contract.md
git commit -m "docs: document the existing updater contract

Recorded before building anything server-side: the URL layout, response
format and package structure are dictated by update_checker.cpp, and
guessing them would only surface as a failure on the first real update."
```

---

## Task 2: Kalitlar va infratuzilma

⚠️ **Task 3 dan oldin tugallanishi SHART** (2-bo'limdagi xavf).

**Files:**
- Create: `docs/self-update/key-management.md`
- Modify: manba koddagi public key konstantasi (Task 1 da aniqlangan joy)

- [x] **Step 1: RSA kalit juftligini yaratish**

**Bajarildi, lekin 2048 emas 1024-bit bilan** — sabab
`key-management.md` §1'da yozilgan: `packer.cpp`/`update_checker.cpp`
`hSigLen=128` (bayt) ni fayl formatining bir qismi sifatida qattiq
yozib qo'ygan, bu 1024-bit imzo uzunligi.

- [x] **Step 2: Private kalitni xavfsiz joyga qo'yish**

`DesktopPrivate/` (repo tashqarisida), `.gitignore`ga qo'shildi.
Qo'lda zaxiralash 2026-08-02'da foydalanuvchi tomonidan bajarildi
(`key-management.md` §4).

- [x] **Step 3: Public kalitni kodga qo'yish**

`config.h` va `packer.cpp` ikkalasida almashtirildi (bir xil kalit).

- [x] ~~**Step 4: Shifrlash kalitini yaratish**~~ — SKIP QILINDI

2026-08-02: foydalanuvchi tasdiqladi. Sabab 3.2-bo'limda yozilgan —
AES kaliti binary ichida bo'lgani uchun real himoya bermaydi.

- [x] **Step 5: URL konstantalarini almashtirish**

VPS mirror (`updates.2007.uz/secure`, Basic-auth URL ichida) va yopiq
Telegram kanal (`kFeedChannelId`, `ResolveOwnChannel`) kodga yozildi.
GitHub repo (`Oybek-M/tdesktop-releases`, private) mavjud, lekin
`HttpChecker` bir vaqtda faqat bitta prefiks bilan ishlaydi — GitHub'ni
avtomatik urinish Task 4'da hal qilinadi.

- [x] **Step 6: Infratuzilmani tayyorlash**

DNS (`updates.2007.uz` → `109.199.108.248`), nginx vhost (`/secure/`
Basic-auth, `/4f2c434f2f125037fdc4ad93/` pub) va certbot sertifikati
2026-08-02'da foydalanuvchi tomonidan VPS'da to'g'ridan-to'g'ri
bajarildi (avtomatik delegatsiya permission classifier tomonidan
bloklangani uchun). Tekshirildi: `/secure/current` to'g'ri parol bilan
404, noto'g'ri parol bilan 401, `/pub/current` autentifikatsiyasiz 404,
sertifikat ishonchli (curl `-k` flag'siz ishlaydi).

- [x] **Step 7: Hujjatlashtirish va commit**

`key-management.md` da yozing: kalit qayerda, zaxira qayerda, yo'qolsa
nima qilish kerak.

```bash
git add -A
git commit -m "feat: replace update signing key and URLs with our own

Done before enabling the updater, not after: with the official key and
URLs still in place, a single update check would replace this fork with
stock Telegram Desktop and silently remove every customisation."
```

---

## Task 3: Windows uchun yoqish

**Files:**
- Modify: build konfiguratsiyasi (`DESKTOP_APP_DISABLE_AUTOUPDATE=OFF`)

- [ ] **Step 1: Task 2 bajarilganini tasdiqlash**

Boshlashdan oldin tekshiring:

```bash
grep -rn "updates.tdesktop.com" Telegram/SourceFiles/core/update_checker.cpp
```

Kutilgan: **hech narsa topilmasligi kerak.** Topilsa — Task 2 tugamagan,
davom etmang.

- [ ] **Step 2: Autoupdate'ni yoqish**

```bash
cmake -S . -B out -D DESKTOP_APP_DISABLE_AUTOUPDATE=OFF
```

- [ ] **Step 3: Build**

⚠️ **~34 daqiqa. Foydalanuvchidan so'rang.**

Bu build `Updater.exe` ni ham yasaydi — u ishlab turgan `Telegram.exe`
ni almashtiradigan yordamchi dastur.

- [ ] **Step 4: Sinov paketi yasash**

Task 1 da aniqlangan `packer` argumentlari bilan, versiyani joriydan
bittaga oshirib.

- [ ] **Step 5: Lokal mirror bilan sinash**

Lokal HTTP server ko'taring va URL'ni vaqtincha unga qarating:

```bash
python -m http.server 8000 --directory ./test-updates
```

Tekshiriladigan holatlar:

| Holat | Kutilgan |
|---|---|
| Yangi versiya bor | Aniqlanadi, yuklab olinadi, qayta ishga tushirishni taklif qiladi |
| Qayta ishga tushirilgandan keyin | Yangi versiya ishlaydi, **CustomMod sozlamalari saqlanadi** |
| **Imzo buzilgan paket** | **Rad etiladi** — bu eng muhim tekshiruv |
| Server yetib bo'lmaydi | Jimgina o'tkazib yuboriladi, ilova normal ishlaydi |
| Versiya joriydan eski | Yangilanish taklif qilinmaydi |

**Imzo tekshiruvini albatta sinang:** paketning bir baytini o'zgartirib,
rad etilishiga ishonch hosil qiling. Bu ishlamasa, butun xavfsizlik
modeli qog'ozda qoladi.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat: enable in-app updates on Windows

Verified that a package with a tampered byte is rejected: without that
check the signing key would be decorative and any mirror could push
arbitrary code."
```

---

## Task 4: Reliz skripti va mirror'lar

**Files:**
- Create: `tools/release/publish.ps1` (yoki `.sh`)

- [ ] **Step 1: Skriptni yozish**

Bitta buyruq bilan to'liq reliz:

```
1. Versiyani tekshirish (version.h dagi bilan mos kelishi)
2. packer bilan imzolash
3. Shifrlash
4. Uchala mirror'ga yuklash
5. Har biridan qaytarib yuklab olib, checksum solishtirish
6. Natijani ko'rsatish
```

**5-qadam majburiy:** yuklandi deb hisoblab qo'yish yetarli emas —
har bir mirror haqiqatan to'g'ri fayl berayotganini tekshirish kerak.
Buzilgan mirror foydalanuvchilarga yangilanishni to'sadi.

- [ ] **Step 2: Xato holatlarini boshqarish**

| Holat | Xatti-harakat |
|---|---|
| Bitta mirror ishlamadi | Ogohlantirish, qolganlariga davom etish |
| **Hamma mirror ishlamadi** | **Xato bilan to'xtash** |
| Checksum mos kelmadi | Xato, o'sha mirror "buzuq" deb belgilanadi |
| Private kalit topilmadi | Darhol to'xtash, aniq xabar |

- [ ] **Step 3: Uchi-uchiga sinash**

Haqiqiy mirror'larga reliz chiqaring va boshqa mashinadagi (yoki
virtual mashinadagi) o'rnatilgan nusxa uni olishini tekshiring.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "chore: add one-command release publishing

The script re-downloads from every mirror and compares checksums rather
than trusting the upload result: a mirror that silently stores a
truncated file blocks updates for everyone pointed at it, and that
failure is invisible until someone reports the app never updating."
```

---

## Task 5: Linux

- [ ] **Step 1: `updater_linux.cpp` ni ko'rib chiqish**
- [ ] **Step 2: Linux uchun build va paket**
- [ ] **Step 3: Task 3 dagi jadval bo'yicha sinash**
- [ ] **Step 4: Reliz skriptiga Linux qo'shish**
- [ ] **Step 5: Commit**

---

## Task 6: macOS

⚠️ Bu bosqich boshqalaridan murakkabroq: macOS imzolanmagan ilovani
ishga tushirishga qarshilik qiladi (Gatekeeper), va yangilanish
jarayoni ham imzolangan bo'lishi kerak.

- [ ] **Step 1: Codesign talablarini aniqlash**

Apple Developer hisobi bor. Aniqlash kerak: ad-hoc imzolash yetarlimi,
yoki notarization ham kerakmi. Bu javob qolgan qadamlarni belgilaydi.

- [ ] **Step 2: `updater_osx.m` ni ko'rib chiqish**
- [ ] **Step 3: Build, imzolash, paket**
- [ ] **Step 4: Sinash**
- [ ] **Step 5: Reliz skriptiga macOS qo'shish**
- [ ] **Step 6: Commit**

---

## Qamrovga kirmaydi

**Birinchi o'rnatish.** Self-update faqat *yangilanishni* hal qiladi.
Akangizga birinchi nusxani baribir qo'lda berasiz (fleshka yoki cloud).
Undan keyin hamma narsa avtomatik.

Agar birinchi o'rnatish ham qulay bo'lishi kerak bo'lsa — bu alohida
kichik ish (oddiy installer), keyinroq muhokama qilinadi.

**Mobil klientlar uchun yangilanish.** Android va iOS ilovalar hali
mavjud emas (D yo'nalishi). Ular yozilganda shu manifest formatidan
foydalanadi — 3.5-bo'lim shuni ta'minlaydi.

**Backend'ning o'zini yangilash (CI/CD) va boshqaruv panelidan reliz
chiqarish.** Bular C yo'nalishiga tegishli va u yerda
`06 — release management` plani sifatida yoziladi. Bu yerda faqat
formatning ularga mos bo'lishi kafolatlanadi.

---

## Qabul qilish mezonlari

1. Rasmiy `updates.tdesktop.com` manzillari kodda **umuman qolmagan**.
2. Rasmiy public key kodda **umuman qolmagan**.
3. Imzosi buzilgan paket **rad etiladi** (amalda sinalgan).
4. Yangilanishdan keyin **CustomMod sozlamalari va arxivi saqlanadi**.
5. Uchala mirror ham ishlaydi; ikkitasi o'chirilganda uchinchisi orqali
   yangilanish keladi.
6. Reliz bitta buyruq bilan chiqadi va har bir mirror tekshiriladi.
7. Server yetib bo'lmaganda ilova normal ishlaydi, xato ko'rsatmaydi.
8. Private kalit repozitoriyda **yo'q** va zaxirasi bor.
