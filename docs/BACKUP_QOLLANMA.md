# 💾 CustomMod To'liq Backup/Restore Qo'llanmasi

Laptopni almashtirish, dastur qayta o'rnatish yoki ma'lumotlarni xavfsiz saqlash uchun to'liq qo'llanma.

---

## 🎯 Nima saqlanadi (Backup v2)

Bitta `.zip` fayl quyidagi NARSALARNI o'z ichiga oladi:

| ✅ Saqlanadi | Joyi (texnik) |
|---|---|
| O'chirilgan xabarlar (Anti-Delete arxiv) | `actioned_messages.db` |
| Tahrirlangan xabarlar (Anti-Edit arxiv) | `actioned_messages.db` |
| Bomb-Media (rasm/video) | `customizationMainFolder/` |
| **White List + Black List** | `peer_lists.json` |
| **Branding sozlamalari** (nom, icon) | `branding.json` |
| **Ghost / Anti-Delete / Anti-Edit togglelar** | `settings.reg` (Registry) |
| **Per-Chat sozlamalar** (alohida chat overrides) | `settings.reg` (Registry) |
| Backup ma'lumoti (sana, qaysi kompyuter) | `manifest.json` |

| ❌ Saqlanmaydi | Sabab |
|---|---|
| Telegram akkauntlar (tdata) | Telegram Desktop ning asosiy ma'lumotlari, alohida ko'chirish kerak |
| Yuborilgan xabarlar tarixi | Telegram serverda saqlanadi, login qilsangiz kelib turadi |

---

## 📤 EKSPORT (Eski laptopda)

### Qadam 1: Eksport tugmasini bosish

1. Telegram (CustomMod) → Sozlamalar
2. **Customizations (By Oybek)** ni oching
3. **About** tab ga o'ting
4. **📤 To'liq zaxira nusxa olish** tugmasini bosing
5. Saqlash papkasini tanlang (masalan: `D:\Backup\`)
6. ✅ Toast: "Eksport saqlandi: D:\Backup\CustomModBackup_20260601_143022.zip"

### Qadam 2: ZIP faylini xavfsiz joyga ko'chirish

Quyidagi yo'llardan birini tanlang:

- 📱 **USB flash drive** — eng oson va xavfsiz
- ☁️ **Google Drive / OneDrive / Yandex Disk** — internet orqali
- 📧 **Email** o'zingizga jo'natish (lekin fayl katta bo'lsa muammo)
- 💾 **Tashqi qattiq disk**

### Qadam 3: (Ixtiyoriy) Telegram akkauntlarini ham ko'chirish

Agar yangi laptopda akkauntga qaytadan kirish bilan ovora bo'lishni xohlamasangiz:

1. Telegram Desktop ni **TO'LIQ YOPING** (tray icon ham yopiladi)
2. Quyidagi papkani to'liq nusxa oling:
   ```
   %AppData%\Telegram Desktop\tdata\
   ```
3. USB ga ko'chiring

---

## 📥 IMPORT (Yangi laptopda)

### Qadam 1: CustomMod ni o'rnatish

1. Yangi laptopga **Telegram Desktop CustomMod** o'rnating
2. Bir marta oching va keyin yoping (sistema papkalari yaratiladi)

### Qadam 2: (Ixtiyoriy) Telegram akkauntlarini ko'chirish

Agar 1-bosqichda akkauntlarni ham saqlagan bo'lsangiz:

1. Telegram Desktop ni **TO'LIQ YOPING**
2. Saqlangan `tdata` papkasini ushbu joyga ko'chiring:
   ```
   %AppData%\Telegram Desktop\tdata\
   ```
3. Telegram ni qayta oching → ✅ akkauntlar avtomatik kiradi

### Qadam 3: Backup ZIP faylini import qilish

1. Telegram (CustomMod) → Sozlamalar
2. **Customizations (By Oybek)** ni oching
3. **About** tab ga o'ting
4. **📥 Zaxira nusxadan tiklash** tugmasini bosing
5. `CustomModBackup_*.zip` faylini tanlang
6. Tasdiqlash oynasida **Yes** bosing
7. Toast: "Tiklash muvaffaqiyatli! ... Dastur 3 soniya ichida qayta yuklanadi..."
8. ✅ Dastur avtomatik qayta ochiladi
9. ✅ Hammasi joyida — White/Black list, Per-Chat, branding, arxiv

---

## ⚙️ Backup ichida nima borligini ko'rish

`CustomModBackup_*.zip` faylini Windows da o'ng tugma → **Extract All** qilsangiz, ichidan quyidagilarni ko'rasiz:

```
CustomModBackup_20260601_143022/
├── manifest.json                  ← Backup ma'lumoti
├── actioned_messages.db           ← O'chirilgan/tahrirlangan xabarlar (SQLite)
├── peer_lists.json                ← White + Black List
├── branding.json                  ← Branding sozlamalari
├── settings.reg                   ← Registry (togglelar + Per-Chat)
└── customizationMainFolder/       ← Bomb media (rasm/video)
    ├── images/
    └── videos/
```

### manifest.json misol

```json
{
    "version": 2,
    "createdAt": "2026-06-01T14:30:22",
    "sourceHost": "OYBEK-LAPTOP",
    "os": "Windows 11 Pro",
    "hasRegistry": true,
    "hasPeerLists": true,
    "hasBranding": true,
    "hasMedia": true
}
```

---

## 🔒 Xavfsizlik

### Backup faylida nima bor?

- ✅ **Telegram akkauntlar YO'Q** — backup boshqa odam qo'liga tushsa ham, sizning akkauntingiz xavfda emas
- ✅ Faqat **CustomMod ma'lumotlari**: arxiv, sozlamalar, list lar
- ✅ Backup ZIP shifrlanmagan — agar maxfiy ma'lumotlar bo'lsa, ZIP ni parol bilan qayta packing qiling (`7-Zip`, `WinRAR`)

### Backup ni xavfsiz saqlash maslahatlari

1. **2 ta nusxa qiling** — USB + cloud (xavfsizlik uchun)
2. **Sana yorlig'i** bilan saqlang — fayl nomida sana avtomatik bor
3. **3 oyda bir** yangi backup oling (yangi arxiv qo'shilgan bo'lishi mumkin)
4. **Maxfiy ma'lumot** bo'lsa — ZIP parol bilan (`7z a -p backup.7z files`)

---

## 🆘 Muammolar va yechimlar

### "Tiklash amalga oshmadi"
- Fayl `.zip` bo'lishi shart
- Fayl chiqib qolmagan (corrupted) bo'lmasligi kerak — qayta yuklab ko'ring
- Fayl ichida `actioned_messages.db` bo'lishi shart

### "Eksport amalga oshmadi"
- Saqlash papkasiga **yozish ruxsati** bor-yo'qligini tekshiring
- Papkada bo'sh joy yetarli (kamida 100 MB) bo'lsin
- Windows da PowerShell ishlatiladi — antivirus to'sib qo'ymagan bo'lsin

### Import dan keyin sozlamalar ko'rinmayapti
- Dastur avtomatik 3 soniyada restart bo'lishi kerak. Bo'lmasa, qo'lda qayta yoqing
- Registry import bo'lmagan bo'lishi mumkin — Tools (Tools+R) → `regedit` → `HKCU\Software\CustomMod` mavjudligini tekshiring

### Eski Backup v1 ni v2 ga import qilish
- ✅ Ishlaydi (orqaga moslik). Faqat eski backup da JSON va Registry bo'lmagani uchun, ular saqlanib qoladi (joriy qiymatlardan)

### "Dastur ochilmayapti" import dan keyin
- Registry import notog'ri bo'lishi mumkin
- `regedit` → `HKCU\Software\CustomMod` → o'ng tugma → **Delete**
- Dasturni qayta oching → default sozlamalar bilan ochiladi
- White List/Black List va arxiv saqlanib qoladi (JSON va DB faylda)

---

## 📋 Tezkor Reference

| Vazifa | Qadam |
|---|---|
| Oddiy backup | Custom Window → About → 📤 Export |
| Restore | Custom Window → About → 📥 Import → ZIP tanlash |
| Akkauntlar bilan ko'chirish | `tdata` papkani qo'lda + CustomMod backup |
| Faqat sozlamalar ko'chirish | Backup ichidagi `peer_lists.json` + `settings.reg` ni qo'lda |

---

## 🔄 Versiya tarixi

- **v2 (joriy)** — JSON sozlamalar + Registry + Manifest + Auto-restart
- **v1 (eski)** — Faqat DB + media

Eski backup (v1) ni yangi tizimga import qilish ishlaydi — yangi qismlar (Whitelist, Branding, Per-Chat) joriy qiymatlardan saqlanadi.

---

**Oxirgi yangilanish:** 2026-06-01  
**Qo'llanma versiyasi:** 1.0  
**Backup format versiyasi:** 2  
**Aloqada bog'liq DOC:** `DOCs/BRANDING_QOLLANMA.md`
