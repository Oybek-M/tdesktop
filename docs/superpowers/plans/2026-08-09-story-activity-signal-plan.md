# Story Post-Vaqti Signali va Media Zaxirasi Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Kuzatilayotgan userlar story qo'yganda, story'ni ochmasdan/ko'rmasdan (a) qo'yilgan vaqtini "Faoliyat Tarixi" arxiviga yozish va (b) ixtiyoriy ravishda story media'sini (foto/video) lokal doimiy zaxiraga saqlash.

**Architecture:** `custom_activity_history.cpp`ning mavjud `Init(session)` funksiyasiga `Data::Stories::itemsChanged()` (`rpl::producer<PeerId>`) obunasi qo'shiladi. Story hech qachon ochilmaydi — faqat `Data::Stories::source(peerId)` orqali sof lokal metadata (`StoriesSource::ids`, sana bilan) o'qiladi. Vaqt — mavjud `activity_history` jadvaliga `field="story"` sifatida (mavjud `RecordField()` dedup helper orqali) yoziladi. Media backup — alohida, standart o'chirilgan tugma bilan, `PhotoData::load()`/`DocumentData::save()` orqali fon rejimida yuklanadi, tugash `session->downloaderTaskFinished()` global signali orqali kuzatiladi, natija `CustomDB::SaveMediaFile()` (document) yoki to'g'ridan-to'g'ri `QImage::save()` (photo) orqali `~/customizationMainFolder/medias/{images,videos}/` ga yoziladi. **Xavfsizlik invarianti:** yangi kod `Stories::markAsRead()`ni hech qachon chaqirmaydi (shu bilan story "ko'rilgan" deb belgilanmaydi, egasiga hech qanday signal yubormaydi).

**Tech Stack:** C++17, Qt 5.15, tdesktop'ning `rpl`/`Data::Stories`/`base::flat_set` infratuzilmasi. Yangi fayl yo'q — faqat mavjud fayllar o'zgaradi, CMakeLists.txt'ga tegilmaydi.

**Muhim — build siyosati:** Bu loyihada build ~30-60 daqiqa oladi va **har doim ishga tushirishdan oldin foydalanuvchidan ruxsat so'ralishi shart**. Shu sababli bu rejada **faqat bitta build-va-tekshirish bosqichi bor — eng oxirida, Task 6da**. Task 1-5 faqat kod yozadi, build qilmaydi.

---

## Fayl tuzilishi

| Fayl | Maqsad |
|---|---|
| `Telegram/SourceFiles/custom_settings.h/cpp` | `storyMediaBackupEnabled` maydoni + getter/setter |
| `Telegram/SourceFiles/custom_activity_history.h/cpp` | `itemsChanged()` obunasi, story-vaqt yozuvi, `DecodeStoryLabel()`, media-backup trigger va tugash-kuzatuv logikasi |
| `Telegram/SourceFiles/custom_mod_window.cpp` | Yangi toggle: "Hikoya media'sini saqlash" |
| `Telegram/SourceFiles/custom_activity_history_box.cpp` | `field == "story"` uchun render/label |

---

### Task 1: `CustomSettings`ga `storyMediaBackupEnabled` maydoni

**Files:**
- Modify: `Telegram/SourceFiles/custom_settings.h`
- Modify: `Telegram/SourceFiles/custom_settings.cpp`

- [ ] **Step 1: `Values` struct'iga yangi maydon qo'shish**

`custom_settings.h`da, `Values` struct'ining oxiriga (`upstreamLastCheckedAt` maydonidan keyin, struct yopilishidan oldin):

```cpp
    qint64 upstreamLastCheckedAt = 0;         // unix timestamp (soniya)
    // ── Story media zaxirasi (A11) ───────────────────────────────────────
    // Story vaqt-kuzatuvidan MUSTAQIL, alohida tugma — disk-sarflovchi
    // funksiya, shuning uchun standart holatda O'CHIRILGAN (opt-in).
    bool storyMediaBackupEnabled = false;
};
```

- [ ] **Step 2: Getter va `id` qatorini qo'shish**

Xuddi shu faylda, `inline qint64  UpstreamLastCheckedAt()` qatoridan keyin (88-qator atrofida):

```cpp
inline qint64  UpstreamLastCheckedAt()         { return Get().upstreamLastCheckedAt; }
inline bool    StoryMediaBackupEnabled()       { return Get().storyMediaBackupEnabled; }
```

- [ ] **Step 3: `Init()`da yangi qiymatni o'qish**

`custom_settings.cpp`da, `Init()` ichida `gValues.upstreamLastCheckedAt = ...` qatoridan keyin:

```cpp
    gValues.upstreamLastCheckedAt = settings.value(
        "upstreamLastCheckedAt", qint64(0)).toLongLong();
    gValues.storyMediaBackupEnabled = settings.value(
        "storyMediaBackupEnabled", false).toBool();
```

- [ ] **Step 4: `UpdateValue`ga yangi branch qo'shish**

`UpdateValue` funksiyasida (`custom_settings.cpp`, 159-173 qatorlar), oxirgi `else if` dan keyin:

```cpp
void UpdateValue(const QString &id, bool value) {
    if (id == "ghostMode") gValues.ghostMode = value;
    // ... mavjud branch'lar o'zgarishsiz ...
    else if (id == "upstreamCheckEnabled") gValues.upstreamCheckEnabled = value;
    else if (id == "storyMediaBackupEnabled") gValues.storyMediaBackupEnabled = value;
}
```

- [ ] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/custom_settings.h Telegram/SourceFiles/custom_settings.cpp
git commit -m "feat(settings): add story media backup toggle field"
```

---

### Task 2: Story post-vaqti kuzatuvi (`custom_activity_history.cpp`)

**Files:**
- Modify: `Telegram/SourceFiles/custom_activity_history.h`
- Modify: `Telegram/SourceFiles/custom_activity_history.cpp`

- [ ] **Step 1: Header'ga `DecodeStoryLabel` e'lonini qo'shish**

`custom_activity_history.h`da, `DecodeStatusLabel` e'lonidan keyin:

```cpp
[[nodiscard]] QString DecodeStatusLabel(const QString &encoded);

// EncodeStatus() bilan bir xil rolda — "story" maydoni uchun. `encoded`
// — story'ning qo'yilgan vaqti (unix timestamp, matn sifatida).
[[nodiscard]] QString DecodeStoryLabel(const QString &encoded);

} // namespace CustomActivityHistory
```

(Fayl oxiridagi `} // namespace CustomActivityHistory` qatorini o'chirmang — yuqoridagi kod parchasi shu qatordan OLDIN joylashadi.)

- [ ] **Step 2: Includes qo'shish**

`custom_activity_history.cpp`ning boshidagi include blokiga (`#include "data/data_user.h"` qatoridan keyin):

```cpp
#include "data/data_user.h"
#include "data/data_stories.h"
#include "data/data_story.h"
#include <range/v3/algorithm/max_element.hpp>
```

- [ ] **Step 3: `DecodeStoryLabel` implementatsiyasi**

`custom_activity_history.cpp`da, `DecodeStatusLabel` funksiyasining yopilishidan (`}` , 81-qator atrofida) keyin:

```cpp
QString DecodeStoryLabel(const QString &encoded) {
	bool ok = false;
	const auto ts = encoded.toLongLong(&ok);
	if (!ok || ts <= 0) {
		return u"noma'lum"_q;
	}
	return u"📖 Hikoya qo'ydi: "_q
		+ QDateTime::fromSecsSinceEpoch(ts).toString(u"dd.MM.yyyy HH:mm"_q);
}
```

- [ ] **Step 4: `Init()`ga story-vaqt obunasini qo'shish**

`custom_activity_history.cpp`da, `Init(session)` funksiyasi ichida, mavjud `session->changes().peerUpdates(...)` blokining yopilishidan (`}, session->lifetime());`, 119-qator atrofida) keyin, funksiya yopilishidan (`}`) oldin:

```cpp
	}, session->lifetime());

	// ── Story post-vaqti signali (A11 §1) ────────────────────────────────
	// Story HECH QACHON ochilmaydi — faqat mavjud sinxronizatsiya orqali
	// kelgan metadata (sana) o'qiladi. Stories::markAsRead() bu yerda
	// HECH QACHON chaqirilmaydi (xavfsizlik invarianti).
	session->data().stories().itemsChanged(
	) | rpl::on_next([=](PeerId peerId) {
		const auto user = session->data().peer(peerId)->asUser();
		if (!user) {
			return; // faqat User (shaxsiy chat) kuzatiladi
		}
		const auto peerId2 = QString::number(user->id.value);
		if (!CustomSettings::ShouldTrackActivity(peerId2, user->isContact())) {
			return;
		}

		auto &stories = session->data().stories();
		const auto source = stories.source(peerId);
		if (!source || source->ids.empty()) {
			return; // hozircha aktiv story yo'q
		}
		const auto latest = *ranges::max_element(
			source->ids,
			ranges::less{},
			&Data::StoryIdDates::date);
		const auto found = stories.lookup({ peerId, latest.id });
		if (!found) {
			return;
		}
		const auto story = *found;
		const auto now = base::unixtime::now();

		RecordField(
			peerId2,
			u"story"_q,
			QString::number(story->date()),
			now);
	}, session->lifetime());
}
```

**Diqqat:** yuqoridagi kod bloki `Init()` funksiyasining ICHIDA, oxirgi yopilish qavsidan oldin joylashadi — `Init()`ning o'zining yopilish qavsi (`}`) endi shu blokdan keyin keladi (yuqoridagi kod parchasida ko'rsatilgan).

- [ ] **Step 5: Commit**

```bash
git add Telegram/SourceFiles/custom_activity_history.h Telegram/SourceFiles/custom_activity_history.cpp
git commit -m "feat(activity-history): track story post-time as fallback activity signal"
```

---

### Task 3: Story media zaxirasi (`custom_activity_history.cpp`)

**Files:**
- Modify: `Telegram/SourceFiles/custom_activity_history.cpp`

Bu task Task 2 bilan bir xil faylni o'zgartiradi — Task 2 tugab, commit qilingandan KEYIN boshlanadi (ketma-ket, parallel emas).

- [ ] **Step 1: Includes qo'shish**

`custom_activity_history.cpp`ning include blokiga, Task 2da qo'shilgan qatorlardan keyin:

```cpp
#include <range/v3/algorithm/max_element.hpp>
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "ui/image/image.h"
#include "custom_db.h"
#include "base/flat_set.h"
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
```

(`custom_db.h` allaqachon fayl boshida bor edi — qayta qo'shilsa duplikat bo'lmaydi, lekin avval tekshirib, yo'q bo'lsagina qo'shing.)

- [ ] **Step 2: Fon-yuklash holatini kuzatuvchi ichki struct va funksiyalar**

`custom_activity_history.cpp`da, `namespace { ... }` anonim bloki ichida (`RecordField` funksiyasidan OLDIN, 16-qator atrofida):

```cpp
struct PendingStoryMedia {
	PhotoData *photo = nullptr;
	DocumentData *document = nullptr;
	std::shared_ptr<Data::PhotoMedia> photoMedia; // photo bo'lsa, media view'ni tirik saqlaydi
};

std::vector<PendingStoryMedia> gPendingStoryMedia;

QString SaveStoryImage(const QImage &image, PhotoId id) {
	const auto baseDir = QStandardPaths::writableLocation(
		QStandardPaths::HomeLocation)
		+ u"/customizationMainFolder/medias/images"_q;
	QDir().mkpath(baseDir);
	const auto path = baseDir + u"/story_"_q + QString::number(id) + u".jpg"_q;
	image.save(path, "JPEG");
	return path;
}

void CheckPendingStoryMedia() {
	for (auto it = gPendingStoryMedia.begin(); it != gPendingStoryMedia.end();) {
		auto done = false;
		if (it->document) {
			const auto path = it->document->filepath(true);
			if (!path.isEmpty()) {
				CustomDB::SaveMediaFile(path, u"video"_q);
				done = true;
			}
		} else if (it->photo && it->photoMedia) {
			if (it->photoMedia->loaded()) {
				if (const auto image = it->photoMedia->image(
						Data::PhotoSize::Large)) {
					const auto qimg = image->original();
					if (!qimg.isNull()) {
						SaveStoryImage(qimg, it->photo->id);
					}
				}
				done = true;
			}
		} else {
			done = true; // noto'g'ri holat, ro'yxatdan chiqarish
		}
		it = done ? gPendingStoryMedia.erase(it) : std::next(it);
	}
}
```

- [ ] **Step 3: Yuklab olishni ishga tushiruvchi funksiya**

Xuddi shu anonim `namespace { ... }` blokida, yuqoridagi funksiyalardan keyin:

```cpp
void MaybeBackupStoryMedia(
		not_null<Data::Story*> story,
		Data::FileOriginStory origin) {
	if (!CustomSettings::StoryMediaBackupEnabled()) {
		return;
	}
	if (const auto photo = story->photo()) {
		auto media = photo->createMediaView();
		photo->load(Data::PhotoSize::Large, origin);
		gPendingStoryMedia.push_back({ photo, nullptr, std::move(media) });
	} else if (const auto document = story->document()) {
		document->save(origin, QString());
		gPendingStoryMedia.push_back({ nullptr, document, nullptr });
	}
}
```

- [ ] **Step 4: Dedup uchun "qayta ishlangan story'lar" to'plami**

Step 2da yozilgan `std::vector<PendingStoryMedia> gPendingStoryMedia;` qatoridan DARHOL keyin, bitta yangi qator qo'shiladi (mavjud qatorni o'chirmang/qayta yozmang):

```cpp
std::vector<PendingStoryMedia> gPendingStoryMedia;
base::flat_set<FullStoryId> gProcessedStoryMedia; // <-- YANGI qator
```

- [ ] **Step 5: `Init()`ga `downloaderTaskFinished()` obunasi va trigger chaqiruvini qo'shish**

Task 2da qo'shilgan `itemsChanged()` blokining ICHIDA, `RecordField(...)` chaqiruvidan keyin (lekin `itemsChanged()` blokining yopilishidan oldin):

```cpp
		RecordField(
			peerId2,
			u"story"_q,
			QString::number(story->date()),
			now);

		const auto fullId = FullStoryId{ peerId, latest.id };
		if (!gProcessedStoryMedia.contains(fullId)) {
			gProcessedStoryMedia.emplace(fullId);
			MaybeBackupStoryMedia(story, fullId);
		}
	}, session->lifetime());

	session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		CheckPendingStoryMedia();
	}, session->lifetime());
}
```

(Yuqoridagi `}, session->lifetime());` va undan keyingi `session->downloaderTaskFinished()` bloki — bular Task 2da yozilgan `itemsChanged()` obunasi TUGAGANDAN keyin, `Init()` funksiyasining oxirgi yopilish qavsidan OLDIN keladi.)

- [ ] **Step 6: Commit**

```bash
git add Telegram/SourceFiles/custom_activity_history.cpp
git commit -m "feat(activity-history): optional background story media backup"
```

---

### Task 4: Custom Window'da yangi toggle

**Files:**
- Modify: `Telegram/SourceFiles/custom_mod_window.cpp`

- [ ] **Step 1: `addToggle` lambda'sining `id` tekshiruviga yangi branch qo'shish**

`custom_mod_window.cpp`da, `addToggle` lambda'si ichida (494-511 qatorlar atrofida), `storyAnonymousView` branch'idan keyin:

```cpp
		else if (id == u"storyAnonymousView"_q) current = val.storyAnonymousView;
		else if (id == u"storyMediaBackupEnabled"_q) current = val.storyMediaBackupEnabled;
```

- [ ] **Step 2: Yangi tugmani "Privacy & Custom Mods" bo'limiga qo'shish**

Xuddi shu faylda, `storyAnonymousView` tugmasi chaqiruvidan keyin (667-670 qatorlar atrofida):

```cpp
	addToggle(
		u"storyAnonymousView"_q,
		u"Hikoyalarni anonim koʻrish"_q,
		u"Hikoyani koʻrganingiz haqida egasiga bildirish yuborilmaydi."_q);
	addToggle(
		u"storyMediaBackupEnabled"_q,
		u"Hikoya media'sini saqlash"_q,
		u"Kuzatilayotgan userlarning hikoya (story) rasmi/videosi avtomatik, ko'rmasdan lokal saqlanadi. Disk joyini sarflaydi, standart holatda o'chirilgan."_q);
```

- [ ] **Step 3: Commit**

```bash
git add Telegram/SourceFiles/custom_mod_window.cpp
git commit -m "feat(activity-history): add story media backup toggle to Custom Window"
```

---

### Task 5: Faoliyat Tarixi oynasida ko'rsatish

**Files:**
- Modify: `Telegram/SourceFiles/custom_activity_history_box.cpp`

- [ ] **Step 1: `FormatEntryLine`ga `field == "story"` holatini qo'shish**

`custom_activity_history_box.cpp`da, `FormatEntryLine` funksiyasi ichidagi `fieldLabel` lambda'siga (22-28 qatorlar):

```cpp
	const auto fieldLabel = [&] {
		if (entry.field == u"name"_q) return u"Ism"_q;
		if (entry.field == u"username"_q) return u"Username"_q;
		if (entry.field == u"photo"_q) return u"Rasm"_q;
		if (entry.field == u"status"_q) return u"Last-seen"_q;
		if (entry.field == u"story"_q) return u"Hikoya"_q;
		return entry.field;
	}();
```

Va `valueLabel`/`oldLabel` hisoblashda (29-37 qatorlar) `"story"` uchun ham `DecodeStoryLabel` ishlatilishi kerak:

```cpp
	const auto valueLabel = (entry.field == u"status"_q)
		? DecodeStatusLabel(entry.newValue)
		: (entry.field == u"story"_q)
		? DecodeStoryLabel(entry.newValue)
		: (entry.newValue.isEmpty() ? u"(bo'sh)"_q : entry.newValue);
	if (!entry.hasOldValue) {
		return fieldLabel + u": "_q + valueLabel + u" (kuzatish boshlandi, "_q + when + u")"_q;
	}
	const auto oldLabel = (entry.field == u"status"_q)
		? DecodeStatusLabel(entry.oldValue)
		: (entry.field == u"story"_q)
		? DecodeStoryLabel(entry.oldValue)
		: (entry.oldValue.isEmpty() ? u"(bo'sh)"_q : entry.oldValue);
	return fieldLabel + u": '"_q + oldLabel + u"' -> '"_q + valueLabel + u"' ("_q + when + u")"_q;
```

- [ ] **Step 2: Commit**

```bash
git add Telegram/SourceFiles/custom_activity_history_box.cpp
git commit -m "feat(activity-history): display story post-time entries in History Viewer Box"
```

---

### Task 6: Build va qo'lda tekshiruv (bitta build, oxirida)

**Files:** yo'q (faqat build + qo'lda sinov)

- [ ] **Step 1: Build uchun ruxsat so'rash**

Build boshlashdan oldin foydalanuvchidan **aniq ruxsat so'rang**. Ruxsat berilgach:

```powershell
cmake --build out --config Release --target Telegram
```

- [ ] **Step 2: Build xatolarini tuzatish (agar bo'lsa)**

Eng ehtimoliy xato manbalari:
- `range/v3/algorithm/max_element.hpp` include yo'li — kodbazada `ranges::` allaqachon keng ishlatiladi (`data_stories.cpp`da ko'p joyda), shuning uchun bu kutubxona allaqachon mavjud target'ga bog'langan; agar linker/include xatosi chiqsa, boshqa faylda `ranges::max_element` qanday include qilinganini tekshiring.
- `Data::PhotoSize::Large` enum qiymati — `data/data_photo.h` yoki `data/data_types.h`da aniq nomini tasdiqlang (agar `Large` mavjud bo'lmasa, xato matnidagi taklif qilingan muqobil qiymatni ishlating).
- Xato matnini o'qing, aniq qatorga qarang — taxmin qilmasdan tuzating.

- [ ] **Step 3: Ilovani ishga tushirish va qo'lda tekshirish**

```powershell
& "out\Release\Telegram.exe" -debug
```

Tekshirish ro'yxati:
1. Custom Window → General tab → "Hikoyalarni anonim ko'rish" tugmasidan keyin yangi "Hikoya media'sini saqlash" tugmasi ko'rinadimi, standart holatda o'chirilganmi?
2. Activity History Include ro'yxatidagi (yoki mutual-contact) birortasi story qo'yganda, "Faoliyat Tarixi" oynasini ochib, jurnalida "📖 Hikoya qo'ydi: <sana>" yozuvi paydo bo'lganini tasdiqlang.
3. "Hikoya media'sini saqlash" tugmasini yoqib, kuzatilayotgan birov yangi story qo'yishini kuting (yoki mavjud test-akkountdan foydalaning) — bir necha soniyadan keyin `~/customizationMainFolder/medias/images/` yoki `medias/videos/` papkasida `story_<id>.jpg`/mos fayl paydo bo'lganini tasdiqlang.
4. Bir xil story uchun `itemsChanged` bir necha marta qayta ishga tushsa ham (masalan oynani qayta ochib-yopib), media fayli faqat BIR marta yuklab olinganini (qayta-qayta yuklanmaganini) tasdiqlang.
5. Ilova jurnalida (`-debug` log) `stories.readStories`/`stories.incrementStoryViews` so'rovlari YUBORILMAGANINI tasdiqlang (ya'ni bu funksiya ishlagan payt story hech qachon "ko'rilgan" deb belgilanmagan).

- [ ] **Step 4: Yakuniy commit**

```bash
git add -A
git commit -m "test(activity-history): manual verification of story post-time signal and media backup"
```

---

## O'z-o'zini ko'rib chiqish (self-review)

**Spec qamrovi:**
- §1.1 (trigger va manba) → Task 2 Step 4 ✅
- §1.2 (qamrov) → Task 2 Step 4, `ShouldTrackActivity` chaqiruvi ✅
- §1.3 (saqlash) → Task 2 Step 4, `RecordField` qayta ishlatilishi ✅
- §1.4 (ko'rsatish) → Task 2 Step 3 (`DecodeStoryLabel`) + Task 5 ✅
- §2.1 (xavfsizlik) → Task 3 — `markAsRead()` hech qayerda chaqirilmaydi, izohda alohida ta'kidlangan ✅
- §2.2 (sozlama) → Task 1 + Task 4 ✅
- §2.3 (trigger va oqim) → Task 3 Step 3, Step 5 ✅
- §2.4 (saqlash muddati) → muddatsiz, avtomatik pruning qo'shilmagan (spec talabiga mos, alohida kod kerak emas) ✅

**Moslik izohi (spec'dan bitta aniqlashtirish):** Spec §2.3 "aniq 'yuklash tugadi' signali implementatsiya bosqichida aniq kod o'qib tanlanadi" degan edi — bu reja yozilishi paytida aniqlandi: `DocumentData::filepath(true)` (video/document uchun) va `Data::PhotoMedia::loaded()`/`image()->original()` (foto uchun), ikkalasi ham `session->downloaderTaskFinished()` global signali orqali polling qilinadi — xuddi mavjud `DocumentData::finishLoad()`dagi AntiDelete hook'i bilan bir xil oddiy, umumiy yondashuv.

**Placeholder skanerlash:** Yo'q — har bir qadamda to'liq kod bor.

**Tur mosligi:** `DecodeStoryLabel`, `MaybeBackupStoryMedia`, `PendingStoryMedia`, `gPendingStoryMedia`/`gProcessedStoryMedia` nomlari Task 2/3 ichida bir xil ishlatilgan; `CustomSettings::StoryMediaBackupEnabled()` Task 1 (e'lon) va Task 3/4 (chaqiruv)da bir xil.
