# Custom Window (CustomMod Settings) Redesign — Design

**Sana:** 2026-07-13
**Qamrov:** `General` va `Peers` tab'lari (Archive/About — keyinroq, alohida)
**Maqsad:** IT bo'lmagan foydalanuvchi (masalan, foydalanuvchining akasi) uchun tushunarli, kamroq texnik jargon bilan, kamroq bir-biriga o'xshash/dublikat bloklar bilan qayta tashkil qilish.

---

## 1. Umumiy tamoyillar

- Har bir tab kamroq, aniqroq ajratilgan bloklardan iborat bo'lsin.
- Texnik/dublikat tuzilmalar (bir xil UI ikki marta takrorlanishi) kamaytirilsin, lekin funksionallik yo'qolmasin.
- Har bir toggle/bo'lim tepasida (yoki ostida) IT bo'lmagan odam tushuna oladigan qisqa izoh bo'lsin (bu allaqachon qisman bor — `addToggle()`ning `description` parametri — saqlab qolinadi va yangi bo'limlarga ham qo'llanadi).
- Kerak bo'lmagan / o'chirilgan funksiyaga tegishli murakkab forma (masalan Device Spoof yoqilmagan bo'lsa, uning nom/tur maydonlari) yashirilsin — `Ui::SlideWrap` orqali animatsiyali collapse.
- Ziddiyatli holatlar (masalan bir chat ham White ham Black List'da bo'lishi) UI darajasida OLDINDAN oldini olinsin, xato xabari bilan emas — avtomatik hal qilinsin.

---

## 2. General tab — yangi tartib

Joriy holat (`custom_mod_window.cpp`): "Privacy & Ghost Mode" (3 toggle + Device Spoof forma ichida ko'milgan) → "Cheklovlar" (1 toggle) → "Xabarlar tarixi" (3 toggle) → "🎨 Branding".

**Yangi tartib:**

### 2.1 Device Spoof (eng tepada, alohida blok)

- Sarlavha: `"📱 Qurilma ko'rinishini almashtirish"` (hozirgi `"Mobil qurilma koʻrinishi"` toggle nomi saqlanadi, lekin blok sarlavhasi sifatida ajratiladi).
- Toggle: `spoofMobile` (joriy nomi va tavsifi saqlanadi: *"Telegram mobil ilovadan ishlatilayotgandek koʻrinadi."*).
- Toggle **OFF** bo'lganda: quyidagi hammasi (nom/versiya maydonlari, 4 ta preset tugma, Saqlash tugmasi) `Ui::SlideWrap` ichida **yashirinadi** (animatsiyali collapse) — non-IT foydalanuvchi kerak bo'lmagan formani ko'rmaydi.
- Toggle **ON** bo'lganda: forma ochiladi — "Qurilma nomi", "Tizim versiyasi", 4 ta preset (Android/iOS/Windows/Linux), va 💾 Saqlash tugmasi (bular allaqachon bor, faqat joylashuv va collapse-behavior yangi).

### 2.2 Privacy & Custom Mods (birlashtirilgan blok)

Joriy 3 ta alohida sarlavha ("Privacy & Ghost Mode" qolgan 2 toggle, "Cheklovlar", "Xabarlar tarixi") **bittaga** birlashtiriladi:

- Sarlavha: `"🛡️ Privacy & Custom Mods"`
- Toggle'lar tartibi (barchasi mavjud, faqat guruhlanish o'zgaradi):
  1. `ghostMode` — Ghost Mode. **Qo'shimcha izoh** (yangi, mavjud tavsif ostiga
     kichik matn sifatida): *"Yoqilgach, to'liq kuchga kirishi uchun 1-2
     daqiqa ketishi mumkin."* — sabab: `api/api_updates.cpp`dagi
     `updateOnline()` online-holat yangilanishini serverga ataylab
     throttling bilan (server config `onlineUpdatePeriod`, standart ~2
     daqiqa) jo'natadi, spam-so'rovlarni oldini olish uchun. Ghost Mode
     O'CHIRILGANDA bu kechikish yo'q (deyarli darhol qayta "online"
     bo'ladi) — izoh faqat YOQISH holatiga tegishli.
  2. `storyAnonymousView` — Hikoyalarni anonim ko'rish
  3. `bypassRestrictions` — Cheklangan chatda nusxalash va yuborish
  4. `antiDelete` — Anti-Delete
  5. `antiEdit` — Anti-Edit
  6. `offlineDb` — Offline xabar bazasi
- Har birining mavjud `description` matni saqlanadi.

### 2.3 Branding (o'zgarishsiz, joyida qoladi — endi 3-blok)

Window nomi, mod nomi, icon — joriy holatda, faqat endi tab'ning oxirgi bloki.

---

## 3. Peers tab — yangi tartib

Joriy holat: `fillPeerSection(isWhitelist=true)`, `fillPeerSection(isWhitelist=false)`, `fillPerChatSection()` — uchtasi mustaqil, ular orasida hech qanday cross-check yo'q.

**Yangi tartib — 3 blok saqlanadi, lekin ular orasida ziddiyat-nazorati qo'shiladi:**

### 3.1 White List va Black List (ikkalasi ham alohida blok, joriy joyida)

Har birida (joriy holatda ham bor, o'zgarishsiz qoladi):
- Kategoriya toggle'lari: **Shaxsiy chatlar**, **Guruhlar**, **Kanallar** (joriy nomlanish: "Barcha shaxsiy chatlar (Users)" va h.k. — engil soddalashtiriladi).
- "Chat tanlash" tugmasi (Telegram'ning o'z chat-tanlash oynasini ochadi).
- "ID orqali qo'shish" (qo'lda ID kiritish) — ikkinchi darajali variant sifatida qoladi.
- Qo'shilgan chatlar ro'yxati (avatar + nom + o'chirish tugmasi).
- "Barchasini tozalash" tugmasi.

**YANGI: Ziddiyat-nazorati (mutual exclusion)**

1. **Kategoriya darajasida**: Agar WhiteList'da "Guruhlar" ON qilinsa va shu payt BlackList'da "Guruhlar" ON bo'lsa — BlackList'dagi "Guruhlar" **avtomatik OFF** qilinadi (va aksincha, har qanday yo'nalishda, har uch kategoriya uchun mustaqil). BlackList'dagi mos toggle vizual ravishda bir lahzalik holat o'zgarishini ko'rsatadi (oddiy toggle animatsiyasi, qo'shimcha "bloklangan" indikator shart emas — chunki bu faqat OFF holatga o'tadi, taqiqlanmaydi).
2. **Individual chat darajasida**: Agar bitta chat allaqachon BlackList'da bo'lsa-yu, uni WhiteList'ga qo'shsangiz (Chat tanlash yoki ID orqali) — u **avtomatik BlackList'dan olib tashlanadi** (va aksincha). Foydalanuvchiga bitta toast ko'rsatiladi: *"«Ism» BlackList'dan olib tashlandi va WhiteList'ga qo'shildi."*

**Arxitektura izohi**: Bu ikki `fillPeerSection()` chaqiruvi orasida umumiy holatga muhtoj (hozir mustaqil). Implementatsiya: kategoriya toggle'lari uchun umumiy `rpl::event_stream<CustomSettings::PeerType>` (yoki oddiy `Fn` callback juftligi) — bitta list categoriyani yoqqanda, ikkinchi list'ning mos toggle'iga xabar beriladi (uning `rpl::variable<bool>`sini false qilib, `toggleOn()` producer shu orqali yangilanadi). Individual chat qo'shishda esa `CustomSettings::IsInBlocklist()/IsInWhitelist()` allaqachon bor funksiyalardan foydalanib, qo'shishdan oldin ikkinchi ro'yxatdan avtomatik o'chirish (`RemoveFromBlocklist`/`RemoveFromWhitelist` — agar mavjud bo'lmasa, qo'shiladi) qo'shiladi, va ikkinchi ro'yxat UI'si (agar ekranda bo'lsa) shu zahoti yangilanishi kerak — buning uchun ham umumiy `state` obyekti ikkala section orasida ulashiladi.

### 3.2 Individual sozlamalar / istisnolar (avvalgi "Per-Chat Sozlamalar", qayta nomlangan)

- Sarlavha: `"⚙️ Individual sozlamalar (istisnolar)"`.
- Yangi tushuntirish matni: *"Agar biror chat uchun faqat bitta funksiyani (masalan faqat Ghost Mode) alohida sozlamoqchi bo'lsangiz — shu yerdan qo'shing. Bu ro'yxat White/Black List'dan KEYIN tekshiriladi (ular ustunroq)."*
- Qolgan funksionallik (Chat tanlash, har bir entry uchun Ghost/AntiDelete/AntiEdit alohida toggle, o'chirish) — o'zgarishsiz.
- Bu blok White/Black List bilan ziddiyat-nazoratiga KIRMAYDI (chunki u allaqachon "past prioritet, istisno" sifatida ishlaydi — mustaqil chatlar to'plami).

---

## 4. Qamrovdan tashqari (bu safar)

- Archive va About tab'lari — foydalanuvchi alohida ko'rib chiqadi.
- Vizual polish (rang, shrift, spacing) — agar kerak bo'lsa, keyingi bosqichda `ui-dizayner` agent orqali.

---

## 5. Texnik eslatmalar (implementatsiya uchun)

- `custom_mod_window.cpp` — asosiy fayl, 1941 qator (allaqachon katta — bo'lim ko'chirilganda fayl tuzilishini yaxshilash imkoniyati bor, lekin bu safar YAGNI: faqat kerakli qismlarni ko'chirish/birlashtirish, qo'shimcha refaktoring shart emas).
- `custom_settings.h/.cpp` — `SetWhitelistCategory`/`SetBlocklistCategory`, `AddToWhitelist`/`AddToBlocklist`, `IsInWhitelist`/`IsInBlocklist` funksiyalari mavjud — conflict-check uchun kengaytiriladi (masalan `AddToWhitelist()` ichida avtomatik `RemoveFromBlocklist()` chaqirilishi, agar mavjud bo'lsa; xuddi shu Blocklist tarafida ham).
- `Ui::SlideWrap` — Device Spoof formasini collapse qilish uchun, kodda allaqachon boshqa joylarda (`fillPeerSection`dagi `emptyWrap`) ishlatilgan pattern — yangi joyda ham xuddi shu pattern qo'llanadi.
