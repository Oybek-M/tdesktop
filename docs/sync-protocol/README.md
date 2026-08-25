# Sync protokoli — YAGONA HAQIQAT MANBAI

Bu papka **bir nechta loyiha** o'rtasidagi kontraktdir:

| Loyiha | Repo |
|---|---|
| `tdesktop` (CustomMod) | `Oybek-M/tdesktop` — **bu repo** |
| `customsync-server` | alohida papka |
| `server-controller` | `customsync-server` ichida |
| `tmobile-android` | kelajakda |
| `tmobile-ios` | kelajakda |

## 🔴 Asosiy qoida

**Bu papka NUSXALANMAYDI.** Boshqa loyihalar unga **havola** qiladi.

Nusxa olingan hujjat birinchi kundanoq eskira boshlaydi va ikki
loyiha bir-biridan sezdirmay uzoqlashadi. Interop buzilishining
eng keng tarqalgan sababi aynan shu.

## Fayllar

| Fayl | Nima uchun |
|---|---|
| `test-vectors.json` | **Eng muhimi.** Mashina tekshiradigan kontrakt |
| `CHANGELOG.md` | Protokolga tegilgan har o'zgarish — sana va sabab bilan |
| `STATUS.md` | Qaysi loyihada nima bajarilgan |

To'liq dizayn: [`../superpowers/specs/2026-07-29-multi-device-sync-backend-design.md`](../superpowers/specs/2026-07-29-multi-device-sync-backend-design.md)
— ayniqsa **§0 REVIZIYA**.

## Ish tartibi

```
Protokolga tegadigan o'zgarish qilyapsizmi?
  │
  ├─ HA  → 1. test-vectors.json ni yangilang (kerak bo'lsa)
  │        2. CHANGELOG.md ga BIR QATOR yozing
  │        3. STATUS.md da o'z loyihangiz qatorini yangilang
  │
  └─ YO'Q → hech narsa qilinmaydi
```

**Har yangi sessiya `STATUS.md` va `CHANGELOG.md` dan boshlanadi.**

## Nima uchun `test-vectors.json` eng muhim

Hujjat eskirsa — buni hech kim sezmaydi. Test vektori eskirsa —
**test yiqiladi**.

Beshala platforma turli kriptografik kutubxonalardan foydalanadi:

| Platforma | Kutubxona |
|---|---|
| tdesktop | OpenSSL (C++) |
| server-backend | .NET `System.Security.Cryptography` |
| server-controller | Web Crypto API |
| android | Java/Kotlin JCA |
| ios | CryptoKit |

Bittasida base64 padding, nonce tartibi yoki tag joylashuvi
boshqacha bo'lsa — hamma narsa **jimgina** buziladi. Yozuvlar
push bo'ladi, deshifrlash muvaffaqiyatsiz bo'ladi, ma'lumot
yo'qoladi va sabab oylab topilmaydi.

Vektorlar buni **birinchi kunda** ushlaydi.

### Har platformada nima sinaladi

```
1. HKDF-SHA256      master -> content/media/peer kalitlari
2. HMAC-SHA256      peer_id -> peer_hash (16 bayt, hex)
3. SHA256           record_id formulasi (MANFIY msg_id ham!)
4. AES-256-GCM      shifrlash va deshifrlash, tag ALOHIDA
5. PBKDF2-HMAC-SHA256   parol -> KEK (600k va 2M iteratsiya)
```

### Tuzoqlar — vektorlar aynan shularni ushlaydi

🔴 **AES-GCM tag joylashuvi.** Ba'zi kutubxonalar (Python
`cryptography`, .NET) tag'ni ciphertext oxiriga qo'shadi, boshqalari
(OpenSSL past darajali API) alohida beradi. Vektorlarda ular
**alohida maydonlarda** — `ciphertext_hex` ichida tag YO'Q.

🔴 **Manfiy `msg_id`.** Avatar `-photo_id`, story `-story_id`
ishlatadi. `record_id` formulasi o'nlik satr sifatida qo'shadi,
ya'ni **ishora saqlanishi shart** — `-42` va `42` turli yozuvlar.
Vektorlarda ikkita manfiy holat bor.

🔴 **`peer_id` — o'nlik SATR**, son emas. `"7053823996"`, `7053823996` emas.

🔴 **HKDF salt** — 32 bayt **nol** (bo'sh emas).

## Vektorlarni qayta generatsiya qilish

`docs/sync-protocol/generate-vectors.py` (Python 3 + `cryptography`):

```bash
python docs/sync-protocol/generate-vectors.py docs/sync-protocol/test-vectors.json
```

⚠️ Vektorlarni **o'zgartirish — protokolni buzish** demakdir.
Faqat `CHANGELOG.md` ga yozib, barcha loyihalarni yangilagandan
keyin qiling.
