# 🎨 CustomMod Branding Qo'llanmasi

Dastur nomini, oyna sarlavhasini va icon ni o'zgartirish bo'yicha qadam-baqadam qo'llanma.

---

## 📂 Qaysi bo'limni o'qishni boshlash kerak?

| Nimani o'zgartirmoqchisiz? | Bo'lim |
|---|---|
| Oyna sarlavhasi ("Telegram" → boshqa so'z) | [✅ Bo'lim 1](#-bolim-1--xavfsiz-ozgartirishlar) |
| Custom Mod oynasi sarlavhasi | [✅ Bo'lim 1](#-bolim-1--xavfsiz-ozgartirishlar) |
| Dastur icon (taskbar, alt-tab) | [✅ Bo'lim 1](#-bolim-1--xavfsiz-ozgartirishlar) |
| Telegram.exe nomi (MyApp.exe) | [⚠️ Bo'lim 2](#%EF%B8%8F-bolim-2--rebuild-kerak-boladigan-ozgartirishlar) |
| Exe faylining icon (Properties → Icon) | [⚠️ Bo'lim 2](#%EF%B8%8F-bolim-2--rebuild-kerak-boladigan-ozgartirishlar) |
| Windows taskbar ID (Application Name) | [🚫 Bo'lim 3](#-bolim-3--tavsiya-etilmaydigan-ozgartirishlar) |

---

## ✅ Bo'lim 1 — XAVFSIZ O'zgartirishlar

> **Hech qanday ma'lumot yo'qolmaydi.** Akkauntlar, chatlar, sozlamalar saqlanadi. Faqat dasturni yopib qayta ochish kifoya.

### 🌟 EH OSON YO'L: Custom Settings UI orqali

1. Telegram → Sozlamalar → **Customizations (By Oybek)**
2. **General** tab oxiriga scroll qiling
3. **🎨 Branding** sektsiyasini ko'rasiz
4. 3 ta input field:
   - **Asosiy oyna nomi** — masalan `MyChat`
   - **Sozlamalar oyna nomi** — masalan `Mening Sozlamalarim`
   - **Icon fayl yo'li** — qo'lda yozish yoki **📁 Tanlash** tugma orqali
5. **💾 Saqlash** tugmasini bosing
6. Toast: "O'zgartirishlar uchun dasturni qayta yoqing"
7. Dasturni yoping va qayta oching → ✅

UI ostida JSON fayl avtomatik yangilanadi. Quyidagi qo'l usuli — alternativ variant.

---

### 1.1 Faylni topish

Birinchi marta dasturni ishga tushirsangiz, bu fayl avtomatik yaratiladi:

```
C:\Users\<sizning-foydalanuvchi>\AppData\Roaming\CustomMod\branding.json
```

**Ochish uchun:** Windows tugmasi + R → kiriting:
```
%AppData%\CustomMod\
```
→ `branding.json` faylini Notepad / VS Code bilan oching.

### 1.2 Fayl ko'rinishi

```json
{
  "windowTitle":    "Telegram",
  "customModTitle": "Customizations (By Oybek)",
  "iconPath":       ""
}
```

### 1.3 Har bir maydon nima qiladi

| Maydon | Nima qiladi | Misol |
|---|---|---|
| `windowTitle` | Asosiy Telegram oynasi yuqoridagi sarlavha (chat ochilmagan paytda) | `"MyChat"`, `"WorkApp"` |
| `customModTitle` | "Customizations (By Oybek)" oynasi sarlavhasi | `"Mening Sozlamalarim"` |
| `iconPath` | Dastur icon (taskbar, alt-tab, oyna burchagi). PNG yoki ICO fayl yo'li | `"C:/Users/Oybek/Desktop/myicon.png"` |

### 1.4 O'zgartirish qadamlari

**Misol 1 — Faqat nomni o'zgartirish:**
```json
{
  "windowTitle":    "MyChat",
  "customModTitle": "Sozlamalar",
  "iconPath":       ""
}
```

**Misol 2 — Icon qo'shish:**
```json
{
  "windowTitle":    "MyChat",
  "customModTitle": "Sozlamalar",
  "iconPath":       "C:/Users/Oybek/Pictures/mylogo.png"
}
```

### 1.5 Saqlash va ko'rish

1. Notepad da `Ctrl + S` bilan saqlang
2. Telegram (CustomMod) ni **yoping**
3. Qayta **oching**
4. ✅ Yangi nom va icon ko'rinadi

### ⚠️ Diqqat (Bo'lim 1 uchun)

- **Path yozish:** Windows da `\` o'rniga `/` ishlating: `C:/Users/...` (yoki `\\` ikki backslash)
- **Icon o'lcham:** Eng yaxshisi `256x256` PNG yoki `.ico` formati
- **Bo'sh `iconPath`:** `""` yozsangiz — standart Telegram icon qaytariladi
- **JSON sintaksisi:** Vergullarni unutmang, qo'shtirnoq ichida yozing

### 1.6 Agar fayl noto'g'ri yozilsa

JSON xato bo'lsa, dastur eski qiymatlarni ishlatadi. Hech narsa buzilmaydi. Faylni to'g'irlab qayta saqlang.

---

## ⚠️ Bo'lim 2 — REBUILD KERAK BO'LADIGAN O'zgartirishlar

> **Ma'lumot yo'qolmaydi.** Lekin dasturni qayta build qilishingiz kerak (Visual Studio orqali).

### 2.1 Telegram.exe nomini o'zgartirish (MyApp.exe ga)

**Fayl:** `Telegram/CMakeLists.txt`

**Qadamlar:**
1. Faylni VS Code yoki Notepad++ da oching
2. `Ctrl + F` bilan qidiring: `add_executable(Telegram`
3. Topgan qatorda `Telegram` so'zini siz xohlagan nom bilan almashtiring:

   Eski:
   ```cmake
   add_executable(Telegram WIN32 ...)
   ```

   Yangi:
   ```cmake
   add_executable(MyApp WIN32 ...)
   ```

4. **Saqlang**
5. Visual Studio da loyihani qayta **Build** qiling
6. Natija: `out/Debug/MyApp.exe` paydo bo'ladi

### 2.2 Exe faylining icon ni o'zgartirish

> Bu Windows Explorer da `Telegram.exe` belgisini o'ng tugma → `Properties` da ko'rinadigan icon.

**Fayl:** `Telegram/Resources/winrc/Telegram.rc`

**Qadamlar:**
1. Yangi icon ni `Telegram/Resources/winrc/` papkasiga ko'chiring
   - Fayl nomi: `myicon.ico` (256x256, multi-resolution ICO format)
   - Bepul converter: https://convertio.co/png-ico/

2. Faylni Notepad da oching
3. Qidiring: `IDI_ICON1 ICON`
4. O'zgartiring:

   Eski:
   ```
   IDI_ICON1 ICON "Telegram.ico"
   ```

   Yangi:
   ```
   IDI_ICON1 ICON "myicon.ico"
   ```

5. **Saqlang**
6. Visual Studio da qayta **Build** qiling
7. Natija: Yangi `MyApp.exe` ning icon o'zgargan bo'ladi

### 2.3 Bir vaqtning o'zida ham EXE nomini, ham iconni o'zgartirish

Yuqoridagi 2.1 va 2.2 ni ketma-ket bajaring → bitta build qiling.

### ⚠️ Diqqat (Bo'lim 2 uchun)

- Build vaqtida xato chiqsa, men bilan ulashing — fix qilamiz
- `Telegram.rc` da boshqa narsalarga tegmang — faqat `IDI_ICON1` qatorini
- ICO fayl albatta multi-resolution bo'lishi kerak (16, 32, 48, 256 px)
- CMakeLists.txt da boshqa joyga tegmang — faqat `add_executable(Telegram` qatori

---

## 🚫 Bo'lim 3 — TAVSIYA ETILMAYDIGAN O'zgartirishlar

> **OGOHLANTIRISH:** Bu o'zgartirish barcha sozlamalarni, akkauntlarni va chatlarni **YO'QOTADI**. Faqat siz nima qilayotganingizni bilsangiz va toza o'rnatishga tayyor bo'lsangiz, bajaring.

### 3.1 Windows Taskbar Application Name (Application ID)

**Bu nima?** Bu Windows ichki ID — dasturning taskbar grouping, registry path va `%AppData%` papkasining nomini belgilaydi.

**Fayl:** `Telegram/SourceFiles/core/launcher.cpp`

**Qator:** 338

**Eski:**
```cpp
QApplication::setApplicationName(u"TelegramDesktop"_q);
```

**Yangi:**
```cpp
QApplication::setApplicationName(u"MyApp"_q);
```

### 🚨 Nima yo'qoladi?

| Yo'qoladi | Sabab |
|---|---|
| Barcha akkauntlar | Registry path `HKCU\Software\TelegramDesktop\` → `HKCU\Software\MyApp\` ga o'tadi |
| Chat tarixi (tdata) | Yangi `%AppData%\MyApp\` papka yaratiladi, eski `%AppData%\Telegram Desktop\` ishlatilmaydi |
| Sozlamalar | Registry yangidan boshlanadi |
| CustomMod sozlamalari | `HKCU\Software\CustomMod\` — bu o'zgarmaydi |
| `branding.json` | `%AppData%\CustomMod\` — bu o'zgarmaydi |

### Qachon qilish mumkin?

- ✅ Toza dastur (hech qanday akkauntsiz) o'rnatmoqchi bo'lsangiz
- ✅ Avval barcha kerakli ma'lumotlarni backup qilgan bo'lsangiz (Custom Mod > About > Backup)
- ✅ Distribution qilish uchun yangi dastur tayyorlayotgan bo'lsangiz (boshqa odamlarga tarqatish)
- ❌ Hech qachon — agar mavjud akkauntlaringizni saqlashni xohlasangiz

### Agar baribir qilmoqchi bo'lsangiz

1. **Backup qiling:** Custom Mod > About tab > "Export Backup"
2. `launcher.cpp:338` ni o'zgartiring
3. Rebuild
4. Yangi dastur ochiladi — akkountga qaytadan kiring
5. Backup ni import qiling (kerak bo'lsa)

---

## 📋 Tezkor Reference

### Faqat oddiy o'zgartirish kerak?
→ [Bo'lim 1](#-bolim-1--xavfsiz-ozgartirishlar) — `branding.json`

### Telegram.exe nomini o'zgartirish?
→ [Bo'lim 2.1](#21-telegramexe-nomini-ozgartirish-myappexe-ga) — `CMakeLists.txt`

### Exe icon ni o'zgartirish?
→ [Bo'lim 2.2](#22-exe-faylining-icon-ni-ozgartirish) — `Telegram.rc`

### Hammasini to'liq o'zgartirish (distribution uchun)?
→ Bo'lim 1 + Bo'lim 2 + Bo'lim 3 (ehtiyot bo'ling!)

---

## 🆘 Muammo bo'lsa

| Muammo | Yechim |
|---|---|
| Dastur ochilmayapti | `branding.json` ni o'chiring — yangidan default yaratiladi |
| Icon ko'rinmayapti | PNG/ICO yo'lini tekshiring (`/` ishlating, `\` emas) |
| Yangi nom ko'rinmayapti | Dasturni to'liq yopib qayta oching (Tray icon ham yopiladi) |
| Build xatosi | Men bilan ulashing — fix qilamiz |
| JSON xatosi | https://jsonlint.com da tekshiring |

---

**Oxirgi yangilanish:** 2026-06-01  
**Qo'llanma versiyasi:** 1.0  
**Aloqada bog'liq DOC:** `DOCs/MEMORY.md` (texnik tafsilotlar)
