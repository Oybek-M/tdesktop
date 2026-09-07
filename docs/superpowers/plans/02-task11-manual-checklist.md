# Task 11: Qo'lda tekshirish cheklisti (K5 qoidasi va UI regressiya)

Ushbu cheklist har bir yangi qo'shilgan qurilmada (tdesktop, Android, iOS, capture service) sinxronizatsiya va mavjud funksiyalarning regressiyasiz to'g'ri ishlashini tasdiqlash uchun ishlatiladi.

> [!CAUTION]
> **BAZA BILAN XAVFSIZ ISHLASH QOIDASI:**
> Jonli arxiv bazasini (`actioned_messages.db`) hech qachon to'g'ridan-to'g'ri `sqlite3` yoki boshqa dasturlar bilan ochmang (bu `-wal` va `-shm` fayllarini buzishi mumkin).
> SQL so'rovlarni tekshirish uchun har doim bazani va uning `-wal`, `-shm` fayllarini vaqtinchalik papkaga nusxalab, faqat **nusxa** ustida ishlang:
> ```cmd
> mkdir C:\Temp\db_check
> copy "C:\Users\Oybek\Pictures\customizationMainFolder\db\actioned_messages.db*" C:\Temp\db_check\
> sqlite3 C:\Temp\db_check\actioned_messages.db "SELECT count(*) FROM sync_outbox;"
> rmdir /s /q C:\Temp\db_check
> ```

---

## 1-bo'lim: Sinxronizatsiya o'chiq holat (Sync off — K5 tekshiruvi)

Sozlamalar: **CustomMod Sozlamalari -> Sinxronizatsiya -> "Sinxronizatsiyani yoqish" O'CHIQ** holatida bo'lishi shart.

- [ ] **1.1. Ilova ishga tushish vaqti (Startup time)**
  - *Qanday tekshiriladi:* Ilovani to'liq yopib, qayta ishga tushiring. Ishga tushish vaqti avvalgi versiyalar bilan bir xil bo'lishi (kechikishlar, muzlashlar yo'qligi) kerak.
  - *Kutilgan natija:* Ilova odatdagi tezlikda ishga tushadi.

- [ ] **1.2. O'chirilgan xabarlar saqlanishi va ko'rinishi (Deleted messages)**
  - *Qanday tekshiriladi:* Boshqa akkauntdan sinov xabari yuborib, uni "Barcha uchun o'chirish" qiling. Chatda xabar ustida qizil o'chirilganlik belgisi va matni qolishini tekshiring.
  - *Kutilgan natija:* O'chirilgan xabar odatdagidek to'liq ko'rinadi va saqlanadi.

- [ ] **1.3. Tahrirlangan xabarlar tarixi (Edit history)**
  - *Qanday tekshiriladi:* Xabarni yuboring va keyin tahrirlang. Xabar menyusidan yoki ustiga bosib tahrirlar tarixini ko'ring.
  - *Kutilgan natija:* Eski tahrir matni va vaqti to'g'ri ko'rsatiladi.

- [ ] **1.4. Faollik tarixi oynasi (Activity History)**
  - *Qanday tekshiriladi:* Profil yoki chat menyusidan "Activity History" oynasini oching.
  - *Kutilgan natija:* Onlayn/oflayn vaqtlari, ism o'zgarishlari va avatarlar tarixi odatdagidek ochiladi va ko'rinadi.

- [ ] **1.5. Arvoh o'qish rejimi (Ghost Mode)**
  - *Qanday tekshiriladi:* Ghost Mode yoqilgan holatda kelgan xabarni ochib o'qing.
  - *Kutilgan natija:* Yuboruvchi tomonda xabar "o'qilmagan" (bitta belgi) bo'lib qoladi.

- [ ] **1.6. `sync_outbox` jadvalining bo'sh qolishi (Outbox isolation)**
  - *Qanday tekshiriladi:* Yuqoridagi amallardan so'ng, yuqoridagi xavfsiz nusxalash usuli bilan bazani nusxalab, so'rov bajaring:
    ```cmd
    sqlite3 C:\Temp\db_check\actioned_messages.db "SELECT count(*) FROM sync_outbox;"
    ```
  - *Kutilgan natija:* Natija qat'iy `0` bo'lishi shart. Sync o'chiq bo'lganda birorta ham yozuv outbox'ga tushmaydi.

- [ ] **1.7. Tarmoq faolligi yo'qligi (No network activity)**
  - *Qanday tekshiriladi:* Windows `resmon.exe` (Resource Monitor) -> "Network" bo'limida `Telegram.exe` jarayonini tanlang. Sync server manziliga hech qanday TCP ulanish yoki HTTP trafik bo'lmasligi kerak.
  - *Kutilgan natija:* Sync serveriga biron marta ham so'rov yuborilmaydi.

---

## 2-bo'lim: Sinxronizatsiya yoqilgan, lekin server o'chiq (Server unreachable)

Sozlamalar: Server URL sifatida ulanib bo'lmaydigan manzil (masalan, `http://127.0.0.1:59999`) ko'rsatilgan va sync yoqilgan.

- [ ] **2.1. Ilova normal ishlashi (App normal)**
  - *Qanday tekshiriladi:* Ilovadan odatdagidek foydalaning (xabar yozish, guruhlarni ko'rish).
  - *Kutilgan natija:* Hech qanday xatolik oynasi (crash/freeze) chiqmaydi, ilova silliq ishlaydi.

- [ ] **2.2. Lokal capture ishlashi (Capture works)**
  - *Qanday tekshiriladi:* Yangi xabar yuborib uni o'chiring yoki tahrirlang.
  - *Kutilgan natija:* Mahalliy bazaga odatdagidek yoziladi, AntiDelete to'liq ishlaydi.

- [ ] **2.3. `sync_outbox` navbatining to'lishi (Outbox queues up)**
  - *Qanday tekshiriladi:* Xavfsiz nusxa orqali `sync_outbox` jadvalini tekshiring:
    ```cmd
    sqlite3 C:\Temp\db_check\actioned_messages.db "SELECT count(*), kind FROM sync_outbox GROUP BY kind;"
    ```
  - *Kutilgan natija:* Yozuvlar navbatda to'planadi (soni 0 dan katta bo'ladi).

- [ ] **2.4. UI xatolik va backoff ko'rsatishi (UI status & backoff)**
  - *Qanday tekshiriladi:* CustomMod -> Sinxronizatsiya tabini oching.
  - *Kutilgan natija:* "Xatoliklar:" qatorida ketma-ket xatolar soni (consecutive failures) va oxirgi xato matni ko'rinadi, qayta urinish oralig'i eksponensial oshadi (backoff).

- [ ] **2.5. UI muzlamasligi (No freeze)**
  - *Qanday tekshiriladi:* Orkestrator tik urib serverga ulanishga uringan soniyalarda oynani siljiting, matn kiriting.
  - *Kutilgan natija:* UI mutlaqo qotmaydi (tarmoq chaqiruvlari asinxron).

- [ ] **2.6. Server qaytganda navbat bo'shashi (Queue drains on recovery)**
  - *Qanday tekshiriladi:* Serverni ishga tushiring yoki to'g'ri URL kiriting. "Hozir sinxronlash" tugmasini bosing yoki navbatdagi tsiklni kuting.
  - *Kutilgan natija:* Barcha to'plangan outbox yozuvlari serverga yuboriladi va outbox bo'shaydi (`count(*) == 0`).

---

## 3-bo'lim: Kalit mavjud emas / qulflangan holat (Key unavailable)

Sozlamalar: Qurilma hali enroll qilinmagan yoki master kalit DPAPI orqali ochilmagan.

- [ ] **3.1. Lokal capture ishlashi (Local capture uninterrupted)**
  - *Qanday tekshiriladi:* Xabarlarni o'chirish va tahrirlash amallarini bajaring.
  - *Kutilgan natija:* Xabarlar lokal arxivda to'liq saqlanadi.

- [ ] **3.2. `sync_outbox` yozilmasligi yoki to'xtab turishi (Safe outbox gate)**
  - *Qanday tekshiriladi:* Kalit bo'lmaganda `KeysAvailable()` false bo'ladi. Xavfsiz nusxadan outbox'ni tekshiring.
  - *Kutilgan natija:* Kalitsiz soxta/shifrlanmagan yozuvlar yozilmaydi, ma'lumot buzilishi bo'lmaydi.

- [ ] **3.3. Push to'xtab turishi (Push halted)**
  - *Qanday tekshiriladi:* Sinxronizatsiya tabidagi holatni tekshiring.
  - *Kutilgan natija:* Kalit bo'lmagani sababli serverga shifrlanmagan kontent yuborilmaydi.

- [ ] **3.4. Ma'lumot yo'qolmasligi (Zero data loss)**
  - *Qanday tekshiriladi:* Kalit o'rnatilgach (yoki qulf ochilgach) capture qilingan xabarlarni tekshiring.
  - *Kutilgan natija:* Hech qanday lokal ma'lumot yo'qolmagan bo'ladi.

---

## 4-bo'lim: Sinxronizatsiya tabini birinchi ko'rish (UI & Enrolment sanity)

Sozlamalar: CustomMod oynasini oching va 8-chi ("Sinxronizatsiya") tabiga o'ting.

- [ ] **4.1. Tab ochilishi va barqarorlik (Tab opens cleanly)**
  - *Qanday tekshiriladi:* CustomMod oynasida "Sinxronizatsiya" tabini bosing.
  - *Kutilgan natija:* Oyna hech qanday xatosiz ochiladi, ilova yiqilmaydi.

- [ ] **4.2. Boshqaruv elementlari ko'rinishi (Controls & layout)**
  - *Qanday tekshiriladi:* Har bir seksiyani (Asosiy sozlamalar, Ro'yxatdan o'tish, Holat, Diagnostika) ochib ko'ring.
  - *Kutilgan natija:* Matnlar va yorliqlar kesilib qolmagan, input maydonlari va tugmalar to'liq sig'gan.

- [ ] **4.3. Yoqish/o'chirish tugmasi xotirasi (Toggle persistence)**
  - *Qanday tekshiriladi:* "Sinxronizatsiyani yoqish" tugmasini bosing, CustomMod oynasini yoping va qayta oching.
  - *Kutilgan natija:* Tugma avval qoldirilgan holatda (yoqilgan yoki o'chirilgan) saqlanib qoladi.

- [ ] **4.4. Orkestratorsiz holat renderi (Null orchestrator safety)**
  - *Qanday tekshiriladi:* Sync o'chiq holatda Status bo'limini ko'ring.
  - *Kutilgan natija:* Status yorlig'ida "Holat: Sinxronizatsiya to'xtatilgan (yoqilmagan)" toza chiqadi, bo'sh ko'rsatkichlarga murojaat tufayli crash bo'lmaydi.

- [ ] **4.5. Ro'yxatdan o'tish tasdiqlov oynasi (Enrollment confirmation dialog)**
  - *Qanday tekshiriladi:* Kod kiritmasdan yoki kod kiritib "Qurilmani ro'yxatdan o'tkazish" tugmasini bosing.
  - *Kutilgan natija:* Tarmoqqa so'rov yuborishdan OLDIN foydalanuvchiga tasdiqlash oynasi (ConfirmBox) chiqadi. Bekor qilinsa, tarmoqqa murojaat qilinmaydi.
