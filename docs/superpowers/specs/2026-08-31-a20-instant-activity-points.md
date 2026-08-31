# A20 — lahza-nuqtalar "Online bo'lgan davrlar" ro'yxatiga tushmaydi

**Sana:** 2026-08-31
**Topgan:** foydalanuvchi (Guli, `5882234960`)
**Holat:** 🔴 OCHIQ — **YUQORI USTUVORLIK**
**Bog'liq:** A16 §1 (story → status), A19 (kesh nuqsoni)

---

## 1. Belgi

Kontakt bugun 15:03 da story qo'ygan. Faollik tarixi oynasida:

| Bo'lim | Story nuqtasi ko'rinadimi |
|---|---|
| To'liq o'zgarishlar jurnali | ✅ ha — 📖 belgisi bilan |
| **"Online bo'lgan davrlar"** | ❌ **yo'q** |
| **"Joriy holat"** | ❌ **yo'q** (A19) |

Ya'ni ma'lumot bazada bor, lekin foydalanuvchi eng ko'p qaraydigan
ikkita joyda ko'rinmaydi.

## 2. Baza holati (tasdiqlangan)

```
field='story'  new_value=1788170580   observed=31.08 15:10:39  source=observed
field='status' new_value=online:...   observed=31.08 15:03:00  source=story
```

A16 §1 mexanizmi **to'g'ri ishlagan**: status nuqtasi yaratilgan,
`observed_at` ga story QO'YILGAN vaqt yozilgan. Nuqson faqat
ko'rsatishda.

## 3. Ildiz sabab

`ReconstructOnlinePeriods` — `custom_activity_history_box.cpp:96`.

Algoritm `online:` va `offline:` yozuvlarini **juftlaydi**:

```cpp
if (e.newValue.startsWith(u"online:"_q)) {
    openFrom = e.observedAt;
} else if (e.newValue.startsWith(u"offline:"_q) && openFrom > 0) {
    result.append({ openFrom, till });
    openFrom = 0;
}
```

Story nuqtasi — **yolg'iz `online:`**, undan keyin `offline:`
kelmaydi. Shuning uchun `openFrom` ochiq qoladi va davr hech qachon
ro'yxatga qo'shilmaydi. Funksiya oxiridagi izohda bu tan olingan:

> "agar loop tugaganda `openFrom` hali ham > 0 bo'lsa... bu ochiq
> davr e'tiborga olinmaydi. v1 uchun qabul qilinadi"

O'sha paytda faqat `observed` yozuvlar bor edi va ochiq davr kamdan-
kam uchrardi. Endi `story` va `manual` manbalari bilan bu **odatiy
holatga** aylandi.

## 4. Tushuncha: davr emas, LAHZA

Story nuqtasi tabiatan davr emas. "15:03 da aniq onlayn bo'lgan"
degani — boshlanishi va tugashi yo'q.

Buni davrga aylantirishga urinish noto'g'ri bo'lardi (sun'iy
tugash vaqti o'ylab topilardi). To'g'ri yechim — ro'yxatda ikki xil
yozuvni ko'rsatish.

## 5. Yechim

`OnlinePeriod` struct'iga tur qo'shiladi:

```cpp
struct OnlinePeriod {
    qint64 from = 0;
    qint64 till = 0;
    bool instant = false;   // true = lahza (juftisiz nuqta)
    QString source;         // "observed" | "story" | "manual" | "buffer"
};
```

`ReconstructOnlinePeriods` da:

- `online:` + `offline:` juftligi → hozirgidek davr (`instant=false`)
- **Juftisiz `online:`** → `instant=true` nuqta sifatida qo'shiladi
- Loop tugaganda `openFrom > 0` bo'lsa — u ham lahza sifatida
  qo'shiladi (hozir tashlab yuborilmoqda)

⚠️ Hozirgi kod ketma-ket `online:` larda oxirgisini oladi
("ketma-ket online — oxirgisi ustun oladi"). Bu endi XATO bo'ladi:
har bir juftisiz `online:` **alohida lahza**, ular bir-birini
almashtirmasligi kerak.

### Ko'rsatish

| Tur | Ko'rinishi |
|---|---|
| Davr | `30.08 19:00 – 19:01` (hozirgidek) |
| Lahza, `source='story'` | `31.08 15:03 — 📖 hikoya qo'ygan` |
| Lahza, `source='manual'` | `30.08 12:16 — ✍️ qo'lda kiritilgan` |
| Lahza, boshqa | `31.08 15:03 — aniq lahza` |

Manba belgisi A16 §2 dagi jurnal belgilari bilan bir xil bo'lsin —
foydalanuvchi ikkala ro'yxatda bir xil tilni ko'radi.

## 6. Tartib

A19 bilan **bir vaqtda** qilinsin — ikkalasi ham bir xil belgining
(story ko'rinmayapti) ikki tomoni va bir xil faylga tegadi.

Sxema o'zgarishi **kerak emas** — faqat kod.

## 7. Sinov

`Guli` (`5882234960`) chatida faollik tarixini ochish:

- "Online bo'lgan davrlar" da `31.08 15:03 — 📖 hikoya qo'ygan`
  qatori paydo bo'lsin
- "Eng so'nggi aniqlangan holat" ham shu nuqtani ko'rsatsin (A19)
- `30.08 12:16` qatori `✍️ qo'lda kiritilgan` deb belgilansin

---

## 8. Bajarildi (2026-08-31)

- `Telegram/SourceFiles/custom_activity_history_box.cpp`: `OnlinePeriod` struct'iga `instant` va `source` maydonlari qo'shildi.
- `ReconstructOnlinePeriods`: ketma-ket yoki juftisiz qolgan `online:` yozuvlari `instant = true` (lahza) sifatida saqlanadigan qilindi.
- "Online bo'lgan davrlar" ro'yxatida lahza yozuvlari manba belgisi bilan (`📖 hikoya qo'ygan`, `✍️ qo'lda kiritilgan`, `⏱ buferdan tiklangan`, `aniq lahza`) ko'rsatiladi.
