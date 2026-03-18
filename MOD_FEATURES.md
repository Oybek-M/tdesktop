# Custom Telegram Mod (Saidjon Edition) - Tizim Xususiyatlari

Ushbu hujjatda rasmiy Telegram Desktop kodi ustida qilingan barcha xakerlik (modding) ishlari va ularning qanday ishlashi tushuntirilgan.

## 1. Ghost Mode (Mutlaq Yashirinlik)
- **Status yashirish:** Siz onlayn bo'lganingiz, yozayotganingiz (typing) va xabarlarni o'qiganingiz qarama-qarshi tarafga umuman ko'rinmaydi.
- **Local Ghost Read (Temir Xotira):** Xabarlarni o'qiganingizda sizning kompyuteringizdagi yashirin SQLite bazaga (`tdesktop_custom.sqlite`) "bu o'qildi" deb yozib qo'yiladi. Telegramdan chiqib qayta kirsangiz, server "bu o'qilmagan" desa ham, dastur lokal bazaga tayanib uni **o'qilgan** holatida qoldiradi (Aka Messenger va AyuGram texnologiyasi).
- **Ovozli va Video xabarlar yashirinligi:** Ovozli xabarni eshitsangiz yoki yumaloq videoni ko'rsangiz ham, jo'natuvchida "eshitildi" degan belgi (ko'k nuqtacha) yo'qolmaydi.

## 2. Anti-Delete (O'chirilgan xabarlarni qutqarish)
- **Xabarlarni ushlab qolish:** Kimdir xabarni o'chirib yuborsa (Delete for everyone), dastur uni kompyuter xotirasidan o'chirmaydi.
- **Maxsus UI (Aka Messenger dizayni):** O'chirilgan xabarlarning tepasiga qizil rangda `—— DELETED ——` yozuvi qo'shiladi. Bu chat ro'yxatida ham, javoblarda (reply) ham ko'rinib turadi.

## 3. Anti-Edit (Tahrirlangan xabarlar tarixi)
- **Tarixni saqlash:** Xabar tahrirlanganda (Edit), uning eski versiyasi yo'qolib ketmaydi.
- **Maxsus UI:** Yangi xabar tagidan `—— EDITED ——` yozuvi bilan uning eski versiyalari zanjir shaklida terilib boraveradi.

## 4. True Offline Media (Media fayllarni mangu saqlash)
- **Avto-nusxalash:** Siz ko'rgan har qanday video, rasm, fayl yoki ovozli xabar Telegram keshiga tushishi bilan, dastur avtomatik ravishda uning nusxasini `Downloads/Telegram_AntiDelete/` papkasiga ko'chirib o'tkazadi.
- **Crash-free (Xavfsiz):** Bu jarayon mutlaqo orqa fonda, fayl 100% yuklanib bo'lgandan keyingina ishlaydi, shuning uchun Telegram qotmaydi.
- **Chidamlilik:** Agar siz Telegramni o'chirib yuborsangiz yoki keshni tozalab yuborsangiz ham, o'chirilgan videolar sizning kompyuteringizda mangu saqlanib qoladi.
- **Tizimni qayta o'rnatish:** Kompyuterga virus tushib format qilganda, faqatgina `.sqlite` fayli va `Telegram_AntiDelete` papkasini saqlab qolib, yangi tizimga o'tkazsangiz, barcha o'chirilgan tarix 100% tiklanadi.

## 5. Ma'lumotlar bazasi (Custom SQLite)
- **Thread-Safe Architecture:** Baza bilan ishlashda qotib qolishlar (segmentation fault) bo'lmasligi uchun `QMutex` texnologiyasi orqali sinxronlashtirilgan. Xotira keshlanadi va juda tez ishlaydi.
