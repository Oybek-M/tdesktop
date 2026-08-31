# A18 — screenshot bloki va rasm forward bypass'i

**Sana:** 2026-08-31
**So'ragan:** foydalanuvchi
**Holat:** 🔴 OCHIQ — **YUQORI USTUVORLIK**

---

## 1. Belgilar

1. **Private kanalda screenshot olinmaydi** — surat o'rniga Telegram
   oynasining ORQASIDAGI narsa tushadi. Oddiy (private bo'lmagan)
   chat va kanallarda normal ishlaydi.
2. **Private kanaldan bypass-forward** to'liq ishlamaydi: **video
   o'tadi, oddiy rasm o'tmaydi.**

---

## 2. Screenshot bloki — ILDIZ SABAB TOPILDI

### Mexanizm

`SessionController::setupScreenshotProtection()`
— `window/window_session_controller.cpp:1922`:

```cpp
Core::App().screenshotProtection().addReason(activeChatValue(
) | rpl::map([](Dialogs::Key key) {
    const auto peer = key.peer();
    return peer
        ? (Data::AllowsForwardingValue(peer)
            | rpl::map(!rpl::mappers::_1))
        : (rpl::single(false) | rpl::type_erased);
}) | rpl::flatten_latest(), lifetime());
```

Faol chat forward'ga ruxsat bermasa → `ScreenshotProtection` yoqiladi
→ `Platform::SetWindowScreenshotProtection(window, true)` →
Windows'da `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)`
(`platform/win/specific_win.cpp:530`).

Bu **butun ilova oynasiga** qo'llanadi (`QApplication::topLevelWidgets()`
bo'ylab), shuning uchun capture'da oyna umuman ko'rinmaydi va
orqasidagi narsa tushadi. Belgi aynan shu.

### Nima uchun bypass ishlamayapti

Forkda bypass **bor**, lekin faqat BITTA variantida:

| Funksiya | Bypass |
|---|---|
| `PeerData::allowsForwarding()` — `data_peer.cpp:1739` | ✅ bor |
| `AllowsForwardingValue()` — `data_peer_values.cpp:377` | ❌ **YO'Q** |

Screenshot himoyasi **reaktiv (producer)** variantini ishlatadi, ya'ni
bypass'dan chetda qolgan. Bool variantiga bypass qo'shilganda bu
o'xshash funksiya e'tibordan qolgan.

### Yechim

`AllowsForwardingValue()` boshiga bool variantidagi kabi tekshiruv:

```cpp
if (::CustomSettings::BypassRestrictions()) {
    return rpl::single(true);
}
```

Bu bitta o'zgarish screenshot muammosini hal qiladi va shu bilan
birga forward ruxsatiga reaksiya qiladigan boshqa UI joylarini ham
izchil qiladi (hozir bool va producer bir-biriga zid javob beradi —
bu o'z-o'zidan nuqson).

### Qolgan himoya sabablari — TEGILMAYDI

`ScreenshotProtection` bir necha "sabab" bilan ishlaydi. Faqat
yuqoridagi chat-sababi bypass qilinadi:

| Sabab | Joyi | Qaror |
|---|---|---|
| Faol chat forward'ni taqiqlagan | `window_session_controller.cpp:1922` | ✅ bypass |
| Passcode lock | `core/application.cpp:194` | ❌ tegilmaydi |
| To'lov oynasi | `payments_checkout_process.cpp:340` | ❌ tegilmaydi |
| TTL media qatlami | `ttl_media_layer_widget.cpp:383` | ⚠️ ko'rib chiqilsin |
| Media ko'rgich | `media_view_overlay_widget.cpp:6050` | ⚠️ ko'rib chiqilsin |

Passcode va to'lov — bular foydalanuvchining O'Z xavfsizligi uchun,
ularni o'chirish zarar keltiradi.

Media ko'rgich (`contentNeedsScreenshotProtection`) alohida yo'l —
rasm/videoni to'liq ekranda ochganda ishlaydi. Uni ham bypass qilish
kerakmi, sinovdan keyin hal qilinadi.

### MobileView bilan aloqasi — YO'Q

Foydalanuvchi `spoofMobile` bilan bog'liq bo'lishi mumkinligini
so'radi. **Aloqasi yo'q:** `spoofMobile` MTProto darajasida ishlaydi
(server qanday qurilma ko'rishi), screenshot bloki esa Windows'ning
`SetWindowDisplayAffinity` API'si — mutlaqo boshqa qatlam.

---

## 3. Rasm forward bypass'i — TAXMIN, tekshirilishi kerak

### Hozirgi kod

`apiwrap.cpp` (~3920) da bypass-forward media zanjiri. Ikki shox:

**Rasm:**
1. `photo->location(true).name()` — foydalanuvchi saqlagan joy
2. `CustomDB::GetSavedMediaPath(...)` — AntiDelete nusxasi
3. `CustomDB::GetArchivedMediaPath(...)` — L2 arxivi
4. Hech biri topilmasa → faqat matn + toast

**Hujjat (video):**
1. `document->filepath(true)` — Telegram keshi
2. `GetSavedMediaPath` → 3. `GetArchivedMediaPath`

### Taxmin

Video ishlaydi, chunki hujjatlar Telegram keshida **fayl sifatida**
yotadi va `document->filepath(true)` uni beradi.

Rasmlar boshqacha saqlanadi — `photo->location(true)` faqat
foydalanuvchi rasmni QO'LDA saqlagan bo'lsa to'ladi. Aks holda
uchala manba ham bo'sh bo'lib chiqadi.

⚠️ **Bu TAXMIN.** Implementdan oldin tekshirilsin: private kanaldagi
rasm uchun uchala manba ham haqiqatan bo'shmi? Log qo'yib aniqlansin.

### Yo'nalish (tasdiqlangach)

Rasm uchun to'rtinchi manba: Telegram'ning o'z rasm keshidan
(`Image`/`PhotoMedia`) baytlarni olib vaqtinchalik faylga yozish va
shu faylni yuklash. Aniq API tekshiruvdan keyin belgilanadi.

---

## 4. Tartib

1. Screenshot bypass (2-bo'lim) — **kichik va aniq**, darhol qilinadi
2. Rasm forward (3-bo'lim) — avval tashxis, keyin implement

Ikkinchisi birinchisiga bog'liq emas, alohida bajarilishi mumkin.

---

## 5. Bajarildi (2026-08-31)

- `Telegram/SourceFiles/data/data_peer_values.cpp` dagi `AllowsForwardingValue` funksiyasi boshiga `CustomSettings::BypassRestrictions()` tekshiruvi qo'shildi.
- Commit: `f675f7bd8d` (`feat(restriction): bypass screenshot restriction in AllowsForwardingValue (A18)`).
