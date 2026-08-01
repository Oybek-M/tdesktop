# Update-signing key — joylashuvi va boshqaruvi

**Holat:** kalit yaratildi va tekshirildi. Repozitoriyga **hech qachon**
kiritilmaydi.

---

## 1. Nega 2048 emas, 1024 bit

Reja (`2026-08-01-self-update-plan.md`, Task 2, Step 1) `openssl genrsa
... 2048` deb yozgan edi, lekin bu **kod bilan mos kelmaydi**:

`Telegram/SourceFiles/_other/packer.cpp:459` va
`Telegram/SourceFiles/core/update_checker.cpp:340-342` ikkalasi ham
`hSigLen = 128` (bayt) ni **fayl formatining bir qismi** sifatida qattiq
yozib qo'ygan — bu 1024-bit RSA imzo uzunligi. 2048-bit kalit 256-baytli
imzo beradi va ikkala joyda ham rad etiladi (`packer.cpp`: "Bad private
key, size: 256").

Bu formatni o'zgartirish mavjud, sinovdan o'tgan parsing kodiga tegish
degani — reja printsipiga zid ("mexanizmni qayta yozish emas,
yo'naltirish"). Shuning uchun **1024-bit** bilan davom etildi. Bu yagona
himoya qatlami emas — imzo faqat 3 qatlamdan biri (yopiq mirror +
shifrlash + imzo, reja 3.2-bo'lim) — shuning uchun bu murosaga arziydi.

Kalit sinaldi: `openssl dgst -sha1 -sign` / `-verify` bilan round-trip
qilindi, imzo aniq 128 bayt chiqdi (packer.cpp va update_checker.cpp
kutayotgan uzunlik bilan mos).

---

## 2. Joylashuvi

```
C:\Users\Oybek\Documents\Projects programming\Telegram\Telegram\DesktopPrivate\
├── customsync-updates-private.pem   ← RSA private (PKCS#1, "RSA PRIVATE KEY")
├── customsync-updates-public.pem    ← RSA public  (PKCS#1, "RSA PUBLIC KEY")
├── packer_private.h                 ← private key, packer.cpp shu yerdan o'qiydi
└── alpha_private.h                  ← bo'sh stub (alpha kanal ishlatilmaydi)
```

`DesktopPrivate` — `tdesktop` repozitoriyasining **tashqarisida**, unga
qardosh (`Telegram/Telegram/DesktopPrivate`, `tdesktop` bilan bir
darajada). `tdesktop`ning o'zi git repo, lekin bu papka **umuman git
ostida emas** — hech qanday `git add` uni ushlab ololmaydi.

`packer.cpp` shu papkani kutadi (nisbiy include:
`"../../../../DesktopPrivate/packer_private.h"`), shuning uchun bu joy
o'zboshimchalik emas — kod shu yerdan o'qishga yozilgan.

---

## 3. Nima uchun PKCS#1, PKCS#8 emas

OpenSSL 3.x'da `openssl genrsa` va `openssl rsa -pubout` **standart
holatda PKCS#8** (`-----BEGIN PRIVATE KEY-----` / `-----BEGIN PUBLIC
KEY-----`) chiqaradi. Lekin `PEM_read_bio_RSAPrivateKey` /
`PEM_read_bio_RSAPublicKey` (packer.cpp va config.h/update_checker.cpp
ikkalasida ham) **faqat PKCS#1** ni tushunadi
(`-----BEGIN RSA PRIVATE KEY-----` / `-----BEGIN RSA PUBLIC KEY-----`).

Shu sababli kalit `-traditional` (private) va `-RSAPublicKey_out`
(public) flag'lari bilan yaratildi — standart buyruqlar bilan
yaratilgan bo'lsa, ilova kalitni **o'qiy olmagan** bo'lardi (xato hatto
build vaqtida emas, birinchi update tekshiruvida chiqqan bo'lardi).

---

## 4. Zaxira — MAJBURIY

⚠️ **Bu kalit yo'qolsa, boshqa hech qanday yangilanish chiqara
olmaysiz.** Barcha o'rnatilgan nusxalar (siz + aka) qo'lda
almashtirilishi kerak bo'ladi — aynan shu muammoni yo'q qilish uchun
self-update qurilmoqda, shuning uchun bu holat maxsus jiddiy.

**Reja talabi (kamida ikkita joy):**

- [ ] Parol menejeriga (`customsync-updates-private.pem` matnini butunligicha)
- [ ] Shifrlangan USB yoki boshqa offline saqlagichga

Bular hozircha **bajarilmagan** — bu qo'lda, ilova tashqarisida
qilinadigan qadam, Claude buni siz uchun bajara olmaydi (parol
menejeriga kirish, fizik USB). Iltimos build'dan oldin shuni bajarib
qo'ying.

---

## 5. Kalit almashtirilishi kerak bo'lgan barcha joylar

Agar kelajakda kalitni yangilash kerak bo'lsa (masalan, sizga yoki
akangizga tegishli bo'lmagan biror kishi private kalitni ko'rgan
bo'lishi mumkin degan shubha tug'ilsa), **hammasi birga** yangilanishi
shart, aks holda imzo mos kelmay qoladi:

| Fayl | Nima |
|---|---|
| `DesktopPrivate/packer_private.h` | Yangi private kalit |
| `Telegram/SourceFiles/_other/packer.cpp` | Yangi public kalit (`PublicKey`) |
| `Telegram/SourceFiles/config.h` | Yangi public kalit (`UpdatesPublicKey`) |

Eski kalit bilan imzolangan **eski paketlar** yangi public key bilan
tekshirib bo'lmaydi — kalitni almashtirgandan keyin chiqarilgan versiya
avvalgi versiyalarni o'ziga tortolmaydi (ular eski kalitni kutadi). Bu
oddiy holatda muammo emas, chunki har doim eng oxirgi versiya joriy
kalit bilan qayta paketlanadi.
