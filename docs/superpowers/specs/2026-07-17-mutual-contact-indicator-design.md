# Mutual-Contact Indikator — Design

**Sana:** 2026-07-17
**Qamrov:** Chat ro'yxati, Contacts ro'yxati, Profil sarlavhasi — har biriga mustaqil yoqish/o'chirish va mustaqil emoji sozlamasi bilan.

---

## 1. Nima uchun

Foydalanuvchi AyuGram/NovaGram/MonoGram kabi klientlarda ko'rgan: ba'zi kontaktlar ismi yonida kichik emoji (🤝, 👌, ✅) chiqadi — bu ular sizni ham qaytarib contact'ga qo'shganini bildiradi.

Tekshiruv natijasi: bu **bypass emas**. Telegram serveri `User` obyektida rasman `mutual_contact` flag'ini yuboradi (`api.tl:89`), va bu tdesktop kodida allaqachon to'liq parse qilinadi (`data_session.cpp:696`, `UserData::flags() & UserDataFlag::MutualContact`, ishlatilgan joy: `history_view_contact_status.cpp`). Bizga faqat shu mavjud ma'lumotni UI'da ko'rsatish kerak.

**Muhim cheklov:** `mutual_contact=true` amalda faqat siz allaqachon shu odamni contact'ga qo'shgan bo'lsangiz (`isContact()=true`) ma'noli bo'ladi — ya'ni bu "sizga notanish odam sizni yashirincha qo'shdi" emas, balki "siz bilgan odam sizni ham qaytarib tasdiqladi" degani. Shart: `user->isContact() && user->isMutualContact()`. Guruh/kanallarga taalluqli emas — faqat `PeerData::asUser()` mavjud bo'lganda.

---

## 2. Uchta ko'rsatish joyi

| # | Joy | Fayl | Funksiya |
|---|-----|------|----------|
| 1 | Chat ro'yxati (dialogs) | `dialogs/dialogs_entry.cpp` | `Entry::chatListNameText()` |
| 2 | Contacts ro'yxati | `boxes/peer_list_controllers.h/.cpp` | `ContactsBoxController` (faqat shu klass — boshqa peer-list oynalariga, masalan "guruhga a'zo qo'shish", ta'sir qilmaydi) |
| 3 | Profil sarlavhasi | `info/profile/info_profile_top_bar.cpp` + `info/profile/info_profile_values.cpp` | `TopBar` konstruktori, `NameValue()` |

Har uch joyda ham `PeerData`/`UserData` allaqachon mavjud (tasdiqlangan) — qo'shimcha so'rov/fetch kerak emas.

---

## 3. Sozlamalar (CustomMod → General tab)

Yangi kichik bo'lim, "🤝 Mutual-Contact Indikatori" sarlavhasi bilan, mavjud "🛡️ Privacy & Custom Mods" bo'limidan keyin (yoki undan oldin — implementatsiya bosqichida aniq joylashuv tanlanadi, YAGNI: eng oson qo'shiladigan joy).

3 ta mustaqil qator, har birida: **toggle** (yoqish/o'chirish) + **matn maydoni** (emoji/belgi kiritish uchun):

1. "Chat ro'yxatida ko'rsatish" — toggle + emoji input, standart qiymat `🤝`
2. "Contacts ro'yxatida ko'rsatish" — toggle + emoji input, standart qiymat `🤝`
3. "Profilda ko'rsatish" — toggle + emoji input, standart qiymat `🤝`

Vizual joylashuv — mavjud "📱 Qurilma ko'rinishini almashtirish" bo'limidagi kabi chiroyli, tartibli ko'rinishda (sarlavha + har bir qator uchun toggle, yonida/ostida emoji input).

---

## 4. Ma'lumotlar qatlami (`custom_settings.h/.cpp`)

Mavjud pattern (`spoofDeviceModel`/`SetString`/`UpdateString`) aynan takrorlanadi, yangi 6 ta maydon:

```cpp
// Values struct ichiga:
bool mutualContactShowInChatList = true;
QString mutualContactChatListEmoji = u"🤝"_q;
bool mutualContactShowInContactsList = true;
QString mutualContactContactsListEmoji = u"🤝"_q;
bool mutualContactShowInProfile = true;
QString mutualContactProfileEmoji = u"🤝"_q;
```

- `UpdateValue()`/`UpdateString()`/`Init()` ga mos `id` satrlari qo'shiladi (masalan `"mutualContactShowInChatList"`, `"mutualContactChatListEmoji"` va h.k.), xuddi `spoofMobile`/`spoofDeviceModel` kabi.
- Global helper funksiyalar: `MutualContactShowInChatList()`, `MutualContactChatListEmoji()` va h.k. (6 ta, `SpoofDeviceModel()` patterniga o'xshash `inline` funksiyalar).
- **Rejalashtirish bosqichida qaror o'zgartirildi:** umumiy `ShouldShowMutualContactBadge()` helper `custom_settings.cpp`ga QO'SHILMAYDI — bu modul hozircha `UserData`/`data/data_user.h`ga bog'liq emas, va shu bog'liqlikni kiritish keraksiz layering-buzilish bo'lar edi (`isMutualContact()` degan alohida getter ham `UserData`da yo'q, faqat xom `flags() & UserDataFlag::MutualContact` bor). Buning o'rniga ikki qatorli shart (`user->isContact() && (user->flags() & UserDataFlag::MutualContact)`) har 3 joyning o'zida to'g'ridan-to'g'ri yoziladi — bu YAGNI'ga mos, chunki shart juda qisqa va faqat 3 marta takrorlanadi. Batafsil: implementatsiya rejasi `docs/superpowers/plans/2026-07-17-mutual-contact-indicator-plan.md`.

---

## 5. Amalga oshirish tafsilotlari (har bir joy uchun)

### 5.1 Chat ro'yxati

`Entry::chatListNameText()` (`dialogs/dialogs_entry.cpp:335-345`) — **faqat shu funksiya o'zgaradi, `chatListName()`ning o'zi EMAS** (chunki `chatListName()` — `const QString&` qaytaradigan asosiy manba, saralash/qidiruv uchun ham ishlatiladi (`History::chatListName()`, `history.cpp:3020`); uni o'zgartirish saralash/qidiruvni buzadi). `chatListNameText()` ichida `Entry::asHistory()` (`dialogs_entry.h:88`, mavjud metod) orqali `History*` olinadi, undan `history->peer()->asUser()` bilan `UserData*` tekshiriladi. Agar user mavjud, `ShouldShowMutualContactBadge(user)` true va `MutualContactShowInChatList()` yoqilgan bo'lsa — `chatListName()`dan olingan matnga probel + `MutualContactChatListEmoji()` qo'shib, shu kengaytirilgan matn `_chatListNameText.setText(...)`ga beriladi (faqat vizual Text::String keshiga ta'sir qiladi, `chatListName()`ning o'z qiymati o'zgarmaydi).

### 5.2 Contacts ro'yxati

Aniq nuqta topildi: `ContactsBoxController::createRow(not_null<UserData*> user)` (`boxes/peer_list_controllers.cpp:815-818`) hozir oddiy `std::make_unique<PeerListRow>(user)` qaytaradi. `PeerListRow::generateName()` — `virtual` (`boxes/peer_list_box.h:98`), bazaviy implementatsiyasi `peer()->userpicPaintingPeer()->name()` (`boxes/peer_list_box.cpp:826-828`). Yechim: kichik lokal subclass (masalan `ContactsBoxController.cpp` ichida anonim namespace'da) — `generateName()`ni override qilib, shart bajarilsa emoji qo'shadigan qator klassi, va `createRow()` shu subclass'ni qaytaradi. Bu o'zgarish faqat `ContactsBoxController`ga tegishli — boshqa `PeerListRow` ishlatuvchi oynalarga (masalan a'zo qo'shish, forward tanlash) ta'sir qilmaydi.

### 5.3 Profil sarlavhasi

`NameValue()` funksiyasiga o'xshash yangi producer yoki mavjudini `TopBar` ichida wrap qilib, `rpl::map` bosqichida emoji qo'shiladi (faqat `peer->asUser()` bo'lganda).

---

## 6. Qamrovdan tashqari (bu safar)

- Last seen / profil rasmi yashirilganda "ko'rish" (alohida, tasdiqlanmagan gipoteza — foydalanuvchi qo'shimcha ma'lumot to'plamoqda, keyinroq alohida ko'rib chiqiladi).
- Emoji tanlash uchun maxsus emoji-picker UI (oddiy matn input yetarli, foydalanuvchi istalgan belgi/emoji yozadi).
