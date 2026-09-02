# Plan 06, Task 1–4 — Gemini uchun prompt (server tomoni)

Bu faylni **butunlay nusxalab** Gemini'ga bering.

Kontekst: klient tomoni (Task 5) **allaqachon yozilgan va sinovdan
o'tgan** — `tdesktop/tools/publish/release-api.ps1`. Ya'ni kontrakt
endi taxmin emas, ishlaydigan kod bilan mahkamlangan. Server aynan
shunga mos kelishi kerak.

---

## PROMPT (shu yerdan pastini nusxalang)

Repo: `customsync-server`, branch `Oybek`.
Reja: `tdesktop/docs/superpowers/plans/2026-08-25-multi-device-sync-06-release-management.md`
Vazifa: **Task 1, 2, 3, 4** (Task 5 tayyor, Task 6 keyinroq).

`dotnet test` hozir 105 test bilan o'tadi — buzilmasin.

### Nima quriladi

`Modules/Releases/` moduli: reliz metadatasi, bo'lakli (resumable)
yuklash, sha256 tekshiruvi, mirror'larga tarqatish.

### 🔴 Buzilmaydigan xavfsizlik chegarasi

Server **imzolamaydi va imzoni tekshirmaydi**. Maxfiy kalitlar
(`packer_private.h`, `alpha_private.h`,
`customsync-updates-private.pem`) serverga **hech qachon** chiqmaydi.
Server tayyor, imzolangan paketni qabul qiladi va tarqatadi, xolos.
Imzoni klient o'zi tekshiradi.

Sababi: serverni buzgan odam soxta yangilanish tarqata olmasligi
kerak. Server ishonchli tomon EMAS.

### Wire kontrakti — klient aynan shuni yuboradi va kutadi

**1) Reliz yaratish**

```
POST /api/v1/releases
Authorization: Bearer <token>
{ "platform":"win64", "version":7001000, "channel":"stable",
  "sha256":"<HEX>", "size":54950589, "package_name":"tx64upd7001000" }

-> 200 { "id":"<string>", "state":"created" | "already_exists" }
```

🔴 Bir xil `(platform, version, channel)` qayta kelsa **409 EMAS,
200** va mavjud `id`. Agar `sha256` ham bir xil va yuklash tugagan
bo'lsa — `"state":"already_exists"`. Skriptni qayta ishga tushirish
**normal holat**, xato emas — bu reja aynan shu sababdan tug'ilgan.

**2) Yuklash seansini ochish**

```
POST /api/v1/releases/{id}/upload
{ "size": 54950589 }

-> 200 { "sid":"<string>", "received": 0 }
```

Shu reliz uchun tugallanmagan seans bo'lsa — **yangisini yaratmang**,
o'shani va uning haqiqiy `received` qiymatini qaytaring.

**3) Bo'lak yuborish**

```
PUT /api/v1/releases/{id}/upload/{sid}
Content-Range: bytes 4194304-8388607/54950589
Content-Type: application/octet-stream
<binary>

-> 200 { "received": 8388608 }
-> 416  agar Content-Range boshi joriy `received` ga teng bo'lmasa
```

**4) Holat**

```
GET /api/v1/releases/{id}/upload/{sid}
-> 200 { "sid":"...", "received": 6291456 }
```

🔴 **ENG MUHIM TALAB.** `received` — diskda **haqiqatan turgan** bayt
soni bo'lishi shart, shu jumladan **muvaffaqiyatsiz tugagan bo'lakdan
qolgan qism** ham.

Sabab: klient uzilgan bo'lakni ayni offsetdan qayta yubormaydi. U
har urinishdan keyin (muvaffaqiyat ham, xato ham) `GET` bilan
serverdan haqiqiy holatni so'raydi va **o'sha joydan** davom etadi.
Agar server yarim yozilgan bo'lakni hisobga olmasa yoki teskarisiga
yozib qo'yib hisobga olmasa — fayl buziladi yoki yuklash 416 bilan
qotib qoladi.

Ikkala xatti-harakat ham to'g'ri, faqat **rostini ayting**:
- yarim yozilgan qismni saqlaysiz → `received` unga qo'shiladi
- yoki atomar rad etasiz → `received` o'zgarmaydi

**5) Yakunlash**

```
POST /api/v1/releases/{id}/finish
{ "sha256":"<HEX>" }

-> 200 { "state":"complete" }
-> 422  checksum mos kelmasa (vaqtinchalik fayl o'chiriladi)
```

**6) Mirror'larga tarqatish**

```
POST /api/v1/releases/{id}/publish
POST /api/v1/releases/{id}/publish?only=vps-secure

-> 200 { "mirrors":[
     {"mirror":"vps-pub","state":"ok"},
     {"mirror":"vps-secure","state":"failed","error":"disk full"},
     {"mirror":"github","state":"ok"} ]}
```

🔴 Bitta mirror yiqilsa qolganlari **to'xtamaydi**. HTTP status
baribir 200 — qisman muvaffaqiyat xato emas, natija.

Mirror nomlari aynan shu uchtasi: `vps-secure`, `vps-pub`, `github`.
Klient ularni shu nom bilan hisobotga chiqaradi.

**7) Ro'yxat**

```
GET /api/v1/releases
-> relizlar + har birining mirror holati
```

### Boshqa talablar

- Mirror ro'yxati **kodda emas**, `server_settings` da (K1 qoidasi):
  `releases.mirrors`, `releases.verify_after_publish`.
- Har vazifa uchun avval **yiqiladigan test**, keyin implement
  (rejadagi Step tartibi).
- Migratsiya: `releases`, `release_mirrors`, `upload_sessions`.
  Ustunlar `snake_case`.

### Bajarishdan oldin

Rejaning Task 1–4 bo'limlarini to'liq o'qing — u yerda jadval
ustunlari, test kodlari va qadamlar aniq yozilgan.

### Tugatgach MAJBURIY tekshiruvlar

1. `dotnet build` → **0 warning**
2. `dotnet test` → **hammasi o'tadi**, eski 105 test ham
3. Yangi yaratgan har bir faylda tirnoq belgisi joyidami:
   `grep -c '"' <fayl>` — 0 chiqsa fayl buzilgan, qayta yozing
4. O'zingiz yozgan har bir endpoint uchun: klient yuboradigan
   JSON maydon nomlari bilan sizniki **bir xilmi**? (`sid`,
   `received`, `state`, `mirrors`, `mirror`, `error`)

### Hisobot

**Qisqa va aniq.** Faqat shular:
- qaysi fayllar yaratildi/o'zgardi
- test soni (avval → hozir)
- kontraktdan chetlashgan joy bo'lsa — qaysi va nega
- ishonchsiz joylar

Uzun tushuntirish kerak emas.

---

## Diffni qanday tekshiramiz (menga eslatma)

Gemini ilgari **mavjud bo'lmagan API'lar bilan kod o'ylab topgan** va
hisobotida "ishonchsiz joylar yo'q" deb yozgan edi. Diffni o'qish
yetarli emas.

Shu vazifa uchun eng kuchli tekshiruv **allaqachon tayyor**:

```powershell
# 1) Soxta server bilan klient hamon ishlayaptimi
.\tools\publish\test-release-api.ps1
.\tools\publish\test-release-api.ps1 -FailOnChunk 2

# 2) Haqiqiy server ko'tarilgach, ayni klient bilan
.\tools\publish\release.ps1 -Api https://<host> -Token $env:CUSTOMSYNC_TOKEN -DryRun
```

Agar haqiqiy server soxta serverdan boshqacha javob bersa — klient
darhol yiqiladi va qaysi maydon mos kelmagani ko'rinadi.
