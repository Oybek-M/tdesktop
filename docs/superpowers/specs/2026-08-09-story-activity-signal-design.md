# Story Post-Vaqti Signali va Media Zaxirasi — Design Spec

**Sana:** 2026-08-09
**Kod nomi:** A11 (`docs/superpowers/PROJECTS.md`)
**Qamrov:** Kuzatilayotgan (Activity History ro'yxatidagi) userlar yangi story qo'yganda: (1) story'ning qo'yilgan vaqtini mavjud "Faoliyat Tarixi" arxiviga fallback signal sifatida yozish, (2) ixtiyoriy ravishda story media'sini (foto/video) lokal doimiy zaxiraga saqlash. Ikkalasi ham **story'ni ochmasdan/ko'rmasdan** amalga oshiriladi — egasiga hech qanday signal yuborilmaydi.

---

## 0. Nega bu kerak

Foydalanuvchi kuzatilayotgan (masalan mutual-contact yoki whitelist'dagi) kontaktning "yashirin"/berkitilgan so'nggi faolligini bilvosita aniqlashning qo'shimcha manbasini xohlaydi: agar ism/username/rasm o'zgarishi va last-seen kabi mavjud signallar orqali hech narsa topilmasa, story qo'yilgan vaqt ham "hozirgina faol bo'lgan" degan bilvosita dalil bo'la oladi. Bu — boshqa manba topilmaganda ishlatiladigan **fallback** signal, asosiy emas.

Ushbu brainstorming davomida qo'shimcha savol chiqdi: story media'sini (24 soatdan keyin yo'qoladigan tarkib) ham avtomatik zaxiralab qo'yish mumkinmi — bu tabiiy ravishda §2 ga (media backup) aylandi.

---

## 1. Story Post-Vaqti Kuzatuvi (timestamp signal)

### 1.1 Trigger va ma'lumot manbai

`custom_activity_history.cpp`ning mavjud `Init(session)` funksiyasiga **yangi obuna** qo'shiladi (fayl/klass ko'paytirilmaydi — mavjud `peerUpdates` obunasi qatoriga):

```cpp
session->data().stories().itemsChanged(
) | rpl::on_next([=](PeerId peerId) {
    ...
}, session->lifetime());
```

`Data::Stories::itemsChanged()` — `rpl::producer<PeerId>`, peer'ning story ro'yxati o'zgarganda (yangi story, o'chirish, expire va h.k.) chaqiriladi. Story ochilmaydi — faqat metadata o'qiladi:

```cpp
const auto &stories = session->data().stories();
const auto source = stories.source(peerId);   // const StoriesSource*
if (!source || source->ids.empty()) return;
const auto latest = *ranges::max_element(
    source->ids, ranges::less{}, &StoryIdDates::date);
const auto story = stories.lookup({peerId, latest.id});
if (!story) return;
const auto date = (*story)->date();  // TimeId, unix timestamp
```

`Data::StoriesSource::ids` — `base::flat_set<StoryIdDates>`, peer'ning barcha (server tomonidan bizga ko'rinadigan) hozirgi story'lari, sana bilan. Bu — sof lokal state, tarmoq so'rovi yubormaydi (story ro'yxati allaqachon oddiy sinxronizatsiya orqali kelgan).

### 1.2 Qamrov

Xuddi mavjud `peerUpdates` filtri kabi:
```cpp
const auto user = session->data().peer(peerId)->asUser();
if (!user) return;  // faqat User (shaxsiy), kanal/guruh story'lari kuzatilmaydi
const auto peerIdStr = QString::number(user->id.value);
if (!CustomSettings::ShouldTrackActivity(peerIdStr, user->isContact())) return;
```
Alohida ro'yxat **yo'q** — mavjud Activity History Include/Exclude ro'yxati bilan bir xil.

### 1.3 Saqlash

Yangi ustun **kerak emas** — mavjud `activity_history` jadvalining `field` mexanizmiga yangi qiymat qo'shiladi: `"story"`. Mavjud `RecordField()` helper (allaqachon `custom_activity_history.cpp`da bor) qayta ishlatiladi — dedup avtomatik (faqat sana haqiqatan o'zgarganda, ya'ni yangi story kelganda, yozuv qo'shiladi):

```cpp
RecordField(peerIdStr, u"story"_q, QString::number(date), base::unixtime::now());
```

`newValue` = story'ning haqiqiy `date()` (qachon qo'yilgani), `observedAt` = biz buni **qachon aniqladik** (deyarli bir xil bo'ladi, lekin kontseptual jihatdan farqli — xuddi "status" maydonidagi `EncodeStatus`/`observedAt` juftligi kabi).

### 1.4 Ko'rsatish (UI)

`custom_activity_history_box.cpp` (History Viewer Box) — hozir `DecodeStatusLabel()` orqali "status" maydonini o'qiydigan matnga aylantiradi. Xuddi shunday, `field == "story"` uchun yangi decode:

```cpp
// custom_activity_history.cpp ga qo'shiladi:
QString DecodeStoryLabel(const QString &encoded) {
    bool ok = false;
    const auto ts = encoded.toLongLong(&ok);
    if (!ok || ts <= 0) return u"noma'lum"_q;
    return u"📖 Hikoya qo'ydi: "_q
        + QDateTime::fromSecsSinceEpoch(ts).toString(u"dd.MM.yyyy HH:mm"_q);
}
```

Box'dagi render logikasi `field`ga qarab `DecodeStatusLabel`/`DecodeStoryLabel`/xom matn orasida tanlaydi (mavjud pattern, kengaytiriladi).

---

## 2. Story Media Zaxirasi (ixtiyoriy, alohida sozlama)

### 2.1 Nega xavfsiz

`Data::Story::photo()`/`document()` — oddiy `PhotoData*`/`DocumentData*`, xabarlardagi media bilan **bir xil klass**, umumiy fayl-yuklash infratuzilmasidan foydalanadi. Story'ni "ko'rilgan" deb belgilaydigan yagona funksiya — `Stories::markAsRead()` — butun kodbazada **faqat bitta joyda** chaqiriladi: `media/stories/media_stories_controller.cpp:1367` (interaktiv story viewer UI). Faylni fon rejimida yuklash (`photo->load()`/`document->save()`) bu funksiyani **hech qachon** chaqirmaydi — ikkalasi arxitektura jihatidan mustaqil.

**Xavfsizlik talabi (implementatsiya va code-review bosqichida tekshiriladi):** yangi kod hech qanday holatda `Stories::markAsRead()`ni chaqirmasligi kerak. Shu shart bajarilsa, media yuklash **tabiiy ravishda anonim** — `storyAnonymousView`/`ShouldAnonymousStory`ga bog'liqlik shart emas (chunki "ko'rish" signali umuman yuborilmaydi).

### 2.2 Sozlama

Story vaqt-kuzatuvidan **mustaqil, alohida** toggle (foydalanuvchi tanlovi bo'yicha):

```cpp
// custom_settings.h, Values struct:
bool storyMediaBackupEnabled = false;  // standart: o'chirilgan (disk-sarflovchi, opt-in)
```

Custom Window'da yangi tugma (Ghost Mode/AntiDelete/StoryAnonymousView qatorida): **"Hikoya media'sini saqlash"**, tavsif: "Kuzatilayotgan userlarning hikoya (story) rasmi/videosi avtomatik, ko'rmasdan lokal saqlanadi."

### 2.3 Trigger va oqim

Xuddi §1.1'dagi `itemsChanged` obunasi ichida, qo'shimcha shart bilan:

```cpp
if (CustomSettings::StoryMediaBackupEnabled()
        && CustomSettings::ShouldTrackActivity(peerIdStr, user->isContact())) {
    if (const auto photo = (*story)->photo()) {
        photo->load(Data::FileOriginStory(peerId, latest.id));
    } else if (const auto doc = (*story)->document()) {
        doc->save(
            Data::FileOriginStory(peerId, latest.id),
            QString());
    }
}
```

Yuklash tugaganini aniqlash: `document()` uchun mavjud, ishlab turgan namuna bor — `data_document.cpp`dagi `DocumentData::finishLoad()` allaqachon `CustomDB::SaveMediaFile(cachePath, "file")` chaqiradi (hozircha global `AntiDelete()` bayrog'iga bog'liq, dokument darajasida peer-context yo'qligi sababli). **Story media uchun bu mavjud hook'ka bog'lanmaymiz** (chunki u `AntiDelete()`ga bog'liq, story-media-backup emas) — buning o'rniga chaqiruvchi tomonda (`custom_activity_history.cpp`) yuklash tugashini kuzatib, mustaqil ravishda `CustomDB::SaveMediaFile(path, "image"/"video")` chaqiramiz. Aniq "yuklash tugadi" signali (`session().downloaderTaskFinished()` yoki `PhotoData`/`DocumentData`ning update observable'i) implementatsiya bosqichida aniq kod o'qib tanlanadi — `data_photo.cpp`da bunday hook hali yo'q (faqat `data_document.cpp`da bor), shuning uchun photo tomoni uchun yangi, mustaqil kuzatuv yozamiz.

Natijaviy fayl `CustomDB::SaveMediaFile()` orqali mavjud `~/customizationMainFolder/medias/{images,videos}/` daraxtiga nusxalanadi (AntiDelete media arxivi bilan **bir xil papka**, alohida "stories" quyi-papkasi qo'shilmaydi — YAGNI, fayl nomi allaqachon noyob bo'ladi, chunki `photo`/`document` ID asosida generatsiya qilinadi).

### 2.4 Saqlash muddati

Muddatsiz — AntiDelete media arxivi bilan bir xil falsafa: qo'lda tozalanmaguncha turadi, avtomatik pruning yo'q.

---

## 3. Fayllar

| Fayl | O'zgarish |
|---|---|
| `custom_activity_history.h/cpp` | `itemsChanged()` obunasi, `DecodeStoryLabel()`, media-backup trigger logikasi |
| `custom_settings.h/cpp` | `storyMediaBackupEnabled` maydoni + getter/setter |
| `custom_mod_window.cpp` | Yangi toggle: "Hikoya media'sini saqlash" |
| `custom_activity_history_box.cpp` | `field == "story"` uchun render/label |
| `custom_db.h/cpp` | O'zgarish **kerak emas** — mavjud `SaveActivityHistoryEntry`/`GetLatestActivityHistoryValue`/`SaveMediaFile` qayta ishlatiladi |

---

## 4. Chegaralar (qasddan qamrovga kiritilmagan)

- Kanal/guruh story'lari — faqat `User` peer'lar kuzatiladi (mavjud Activity History cheklovi bilan bir xil).
- Story matni/caption'i saqlanmaydi — faqat vaqt (§1) va media fayli (§2, ixtiyoriy).
- "Ko'rganlar" ro'yxati yoki story reaksiyalari — bu spec doirasida emas.
- Real-vaqtda bildirishnoma ("X hikoya qo'ydi!") — bu spec faqat Activity History arxiviga yozishni qamraydi, alohida toast/bildirishnoma qo'shilmaydi (Activity History Viewer Box'ning o'zi buni ko'rsatadi, xuddi ism/rasm o'zgarishlari kabi).

---

## 5. Xavfsizlik eslatmasi (2026-08-09 muhokamasidan)

Ushbu spec yozilishidan oldin, mavjud "Hikoyalarni anonim ko'rish" (`ShouldAnonymousStory`) mexanizmi ikkinchi (test) akkountdan real tekshirildi va **to'g'ri ishlayotgani tasdiqlandi** (tafsilot: `docs/superpowers/specs/2026-05-19-custom-mod-improvements-design.md` §5). Ushbu yangi funksiya (§1 va §2) shu mexanizmga **umuman tegmaydi va undan mustaqil** — chunki ikkalasi ham story'ni hech qachon "ochmaydi"/"ko'rmaydi" (`markAsRead()` chaqirilmaydi), shuning uchun `stories.readStories`/`stories.incrementStoryViews` signal-lari bilan aloqasi yo'q.
