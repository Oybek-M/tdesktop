# Self-update — implement uchun kirish nuqtasi

Bu faylni **birinchi** o'qing. Kontekst oldingi sessiyada to'plangan.

---

## 1. Avval shu ikkalasini o'qing

| Fayl | Nima bor |
|---|---|
| [`updater-contract.md`](updater-contract.md) | Mavjud mexanizm qanday ishlashi — kodda **tasdiqlangan** faktlar |
| [`../superpowers/plans/2026-08-01-self-update-plan.md`](../superpowers/plans/2026-08-01-self-update-plan.md) | 6 bosqichli plan, task'lar bilan |

Ikkalasi ham to'liq o'qilishi kerak. Kontrakt hujjatidagi faktlar
kodda tekshirilgan — ularni qayta tekshirishga vaqt sarflamang.

---

## 2. ⚠️ Buzilmasligi kerak bo'lgan tartib

> **Task 2 (kalit + URL almashtirish) Task 3 (updater'ni yoqish) dan
> OLDIN tugallanishi SHART.**

Sabab: hozir kodda rasmiy Telegram'ning public key'i (`config.h:45`,
`config.h:53`) va rasmiy yangilanish manbai turibdi. Updater shu
holatda yoqilsa, birinchi tekshiruvning o'zi **rasmiy Telegram
build'ini** yuklab olib bu fork'ni bosib ketadi — CustomMod'ning
barcha funksionalligi yo'qoladi va buni faqat ilova qayta ishga
tushgandan keyin bilib qolinadi.

Task 3 ning birinchi qadami aynan shu tekshiruv:

```bash
grep -rn "updates.tdesktop.com" Telegram/SourceFiles/core/update_checker.cpp
```

Natija bo'sh bo'lmasa — davom etmang.

---

## 3. Ish taqsimoti

| Kim | Nima |
|---|---|
| **Siz (Sonnet)** | Kod o'zgarishlari: `config.h` kalitlari, URL/kanal, `MtpChecker`, packer skripti |
| **Foydalanuvchi** | `DESKTOP_APP_DISABLE_AUTOUPDATE=OFF` (CMake flag) va build |

CMake flag'ni **o'zingiz yoqmang** — u kod tayyor bo'lgandan keyin
foydalanuvchi tomonidan yoqiladi.

---

## 4. Qat'iy qoidalar

- **Build'ni hech qachon o'zingiz boshlamang.** ~34 daqiqa oladi va
  foydalanuvchining boshqa og'ir ilovalari bilan raqobatlashadi.
  Har safar **so'rang**.
- Commit + push faqat `origin/Oybek` ga. `upstream` ga **hech qachon**.
- Commit xabarlarida `Co-Authored-By` trailer **ishlatilmaydi**.
- Private RSA kalit repozitoriyga **hech qachon** commit qilinmaydi.
  `.gitignore` ga `*-private.pem` qo'shing.

---

## 5. Hozirgi holat

- Branch: `Oybek`, upstream **v7.0.7** ga sync qilingan
- Tarmoqda **tekshirilmagan bitta o'zgarish** bor:
  `custom_mod_window.cpp` dagi peer-picker tuzatishi
  (commit: "fix: show channels without posting rights in the peer pickers").
  Build hozir ketmoqda. Agar kompilyatsiya xatosi chiqsa —
  ehtimol shu o'zgarishdan, avval shuni ko'ring.
- Self-update bo'yicha **hali hech qanday kod yozilmagan** —
  faqat hujjatlar.

---

## 6. Birinchi qadam

Plandagi **Task 1** ning qolgan qismi: kontrakt hujjatining
6-bo'limidagi 5 ta ochiq savolga javob topish. Ular dizaynni
o'zgartirmaydi, faqat tafsilotlarni to'ldiradi — lekin Task 2 ni
boshlashdan oldin `packer.cpp` argumentlari va `/current` fayl
formati aniq bo'lishi kerak.

Eng muhim savol (yopiq Telegram kanali ishlaydimi) **allaqachon hal
qilingan** — kontrakt hujjatining 5.1/5.2 bo'limlariga qarang.
