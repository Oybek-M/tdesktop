# Alpha relizlar — o'z yangilanishlarimizni chiqarish

**Sana:** 2026-08-15

## Muammo

Rasmiy tdesktop versiyasi (`7.0.9`) o'zgarmagan holda o'z
yangilanishlarimizni chiqarish kerak. Rasmiy raqamni ko'tarish
(`7.0.10`) ikki sababdan yaramaydi: u rasmiy reliz bilan to'qnashadi va
upstream sync'da konflikt beradi.

## Yechim — kod YOZISH SHART EMAS

tdesktop'da bu mexanizm allaqachon bor va biz uni ishlatamiz.

`Telegram/build/version` tuzilishi:

```
AppVersion         7000009      = major*1000000 + minor*1000 + patch
AppVersionStr      7.0.9
AlphaVersion       0            = AppVersion*1000 + alpha  (alpha bo'lsa)
BetaChannel        0
```

`set_version.py` `X.Y.Z.N` shaklini qabul qiladi, bu yerda `N` — alpha
hisoblagichi (0–999):

```bash
python Telegram/build/set_version.py 7.0.9.1
```

Natija:

```
AppVersion         7000009        ← O'ZGARMAYDI
AppVersionStr      7.0.9          ← O'ZGARMAYDI
AlphaVersion       7000009001     ← faqat shu o'sadi
```

Keyingi reliz — `7.0.9.2` → `AlphaVersion 7000009002`. Rasmiy `7.0.9`
ga umuman tegilmaydi.

## Nima uchun bu ishlaydi

`core/update_checker.cpp:842-846`:

```cpp
const auto myVersion = isAvailableAlpha
    ? cAlphaVersion()
    : uint64(AppVersion);
const auto validVersion = (cAlphaVersion() || !isAvailableAlpha);
if (!validVersion || availableVersion <= myVersion) {
    return QString();  // yangilanish yo'q
}
```

Ya'ni alpha reliz alpha raqami bo'yicha solishtiriladi. `7000009002 >
7000009001` → yangilanish topiladi.

## ⚠️ Ikkita muhim natija

**1. Alpha'dan orqaga qaytib bo'lmaydi.** `validVersion` sharti
`cAlphaVersion() != 0` ni talab qiladi — ya'ni alpha bo'lmagan build
alpha yangilanishlarni umuman KO'RMAYDI. Bir marta alpha'ga o'tgach,
barcha qurilmalar alpha'da qolishi kerak.

**2. Alpha relizlar imzo talab qiladi.**
`countAlphaVersionSignature()` maxfiy kalitdan foydalanadi
(`DesktopPrivate/alpha_private.h`). Kalit yo'qolsa yangi alpha reliz
chiqarib bo'lmaydi — `key-management.md` ga qarang.

## Reliz tartibi

1. `python Telegram/build/set_version.py 7.0.9.N`
2. Build (Release/x64)
3. Packer bilan paket yasash va imzolash
4. Mirror'larga yuklash

## Qaysi build ishlayotganini bilish

`AppVersionStr` alpha'da ham "7.0.9" bo'lib qolavergani uchun ikkita
custom build'ni ajratib bo'lmaydi. Shuning uchun Custom Window →
About tab'ida to'liq versiya ko'rsatiladi:

```
CustomMod 7.0.9 · alpha 3
```

## Beta kanali

`set_version.py 7.0.9.beta` `BetaChannel 1` qo'yadi. Bizga hozircha
kerak emas — alpha hisoblagichi yetarli va soddaroq. Beta alohida
ochiq kanal uchun mo'ljallangan.
