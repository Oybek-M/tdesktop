# Plan 04 Task 6 — ikki fazali xavfsiz o'chirish (Gemini uchun prompt)

Repo: `customsync-server` (branch `Oybek`). Plan matni:
`tdesktop/docs/superpowers/plans/2026-07-29-multi-device-sync-04-storage-lifecycle.md`
§Task 6. **Plandagi namuna kodni ko'r-ko'rona ko'chirma** — unda quyida
sanalgan jimgina ma'lumot yo'qotadigan xatolar bor. Quyidagi talablar
plandan USTUN.

Boshlang'ich holat: `dotnet test` -> 135/135, 0 warning. Oldin
`PROGRESS.md` §3 ("Buzmaslik kerak") va §4 (test qoidalari) ni o'qing.

## Mavjud narsalar (qayta yozmang)

- `Storage/RetentionPolicy.cs` — sof `RetentionEvaluator.Evaluate(...)`,
  `never_delete` himoyasi, `retention.<kind>_days` yashirin siyosatlari.
- `Storage/Targets/IArchiveTarget.cs` — `UploadAsync`, `DownloadAsync`,
  `RequiresExplicitConfirmation`; `ManualDownloadTarget`.
- `CmxWriter` / `CmxReader` (`Core/Interchange`), `AuditService`.

## Majburiy talablar

### 1. Qamrov evaluator orqali (plandagi `RetentionScope` yo'q)
- `PreviewAsync(policyId)` / `ExecuteAsync(policyId, target, ...)`.
  `policyId` — `retention_policies` dagi id YOKI yashirin
  `retention.<kind>_days` kaliti.
- SQL bilan faqat nomzodlar toraytiriladi (kind/peer/yosh — yosh
  **`received_at`** bo'yicha, evaluator ham shuni ishlatadi). Keyin HAR
  nomzod `RetentionEvaluator.Evaluate` dan o'tkaziladi va faqat
  `ShouldAct && PolicyId == policyId` bo'lganlar olinadi. Shunda
  `never_delete` va ustuvorlik hech qachon chetlab o'tilmaydi.
- Bir martada olinadigan yozuvlar soniga chegara (masalan 5000) —
  xotira cheklangan bo'lsin.

### 2. Tombstone'lar HECH QACHON o'chirilmaydi
`kind = "tombstone"` qatorlar qamrovdan so'zsiz chiqariladi (hatto
`retention.tombstone_days > 0` bo'lsa ham; izohda sababi). Sabab:
tombstone `UpsertSql` dagi `NOT EXISTS` orqali kechikib kelgan nishonni
qayta tirilishdan saqlaydi, va cursor'i orqada qolgan qurilma o'chirishni
faqat shu tombstone orqali biladi.

### 3. O'chirish faqat ARXIVLANGAN VERSIYANI o'chiradi
Yozuv tanlangandan keyin push uni yangiroq kuzatuv bilan almashtirishi
mumkin (`ON CONFLICT DO UPDATE`). Plandagi `WHERE record_id IN (...)`
arxivlanmagan yangi versiyani o'chirib yuboradi.
- `DELETE ... WHERE record_id = x AND seq = <arxivlangan seq>` (seq har
  yangilanishda o'zgaradi) — bitta tranzaksiyada, `RETURNING record_id,
  payload_size`.
- `record_media` faqat HAQIQATAN o'chgan yozuvlar uchun o'chiriladi.
- `DeletedCount`/`FreedBytes` — RETURNING natijasidan, tanlovdan emas.

### 4. Yetim media — plandagi usul YANGI yuklangan blob'larni o'chiradi
Klient avval blob'ni yuklaydi (`PUT /media`), keyin yozuvni push qiladi.
Plandagi "hech kim havola qilmaydigan hamma blob" shu oraliqdagi blob'ni
o'chiradi. Talab:
- Faqat **shu purge'da o'chgan yozuvlarga bog'langan** hash'lar nomzod.
- Blob darhol o'chirilmaydi: `media_blobs.orphaned_at` (yangi nullable
  ustun + migratsiya) belgilanadi. Blob faqat `orphaned_at` 24 soatdan
  eski VA hali ham havolasiz bo'lsa o'chiriladi (keyingi purge'larda).
- `MediaService.ExistsAsync` true qaytarsa `orphaned_at` NULL qilinadi
  (klient "bor ekan" deb yuklamay qo'ygan blob tirik qoladi).
- DB qatori tranzaksiya ichida o'chadi, fayl — commit'dan KEYIN.

### 5. Arxiv to'liq va xotiraga yuklanmaydi
- Arxiv `MemoryStream` emas, vaqtinchalik faylga yoziladi (keyin
  o'chiriladi).
- Yozuvlarga bog'langan blob'lar arxivga kiradi (`media/<hash>`),
  `MediaRef.Size/Nonce` — `media_blobs` dan. Diskda yo'q blob — natijada
  `MissingMedia` soni.
- Manifest: `Encrypted = true` (payload klientda shifrlangan),
  `KeyFingerprint = ""`, `SourceApp = "customsync-server"`.
- Yuborishdan OLDIN mahalliy fayl `CmxReader` bilan o'qib ko'riladi va
  yozuvlar soni tekshiriladi.

### 6. Tekshiruv
- SHA-256 mahalliy fayldan hisoblanadi; target qaytarib bergan oqimdan
  mustaqil qayta hisoblanadi. `upload.Sha256` ham berilgan bo'lsa u ham
  mos kelishi shart. Har qanday nomuvofiqlik -> hech narsa o'chirilmaydi,
  xato matnida "verification".

### 7. `RequiresExplicitConfirmation`
`ManualDownloadTarget` uchun arxiv yaratiladi va tekshiriladi, lekin
**o'chirilmaydi**: run holati `awaiting_confirmation`.
`ConfirmAsync(runId)` — arxivni target'dan qayta o'qiydi, run'dagi
SHA-256 bilan solishtiradi, ichidagi yozuvlarni o'qib 3-banddagi shart
bilan o'chiradi. Arxivda seq yo'q, shuning uchun moslik
`(record_id, observed_at, device_id)` bo'yicha (upsert aynan shular
o'zgarganda almashtiradi) — 3-bandni ham shu uchlik bilan qilish mumkin,
ikkala yo'l bir xil kodni ishlatsin.

### 8. `delete_only`
Arxivsiz, lekin 2-, 3-, 4-bandlar unga ham to'liq tegishli.

### 9. `ArchiveRunEntity` + migratsiya
`run_id, policy_id, target_id, status (completed | failed_upload |
failed_verification | awaiting_confirmation | nothing_to_do), started_at,
finished_at, matched_count, deleted_count, freed_bytes, archive_location,
sha256, error`. Snake_case, `DatabaseFixture` bilan ishlasin.

## Testlar (`PurgeSafetyTests.cs`, har test o'z `peer_hash` i bilan)

Plandagi 4 tadan tashqari:
1. `never_delete` himoyalagan yozuv purge'dan keyin joyida.
2. Tombstone yosh bo'lsa ham o'chmaydi.
3. Tanlov va o'chirish orasida superseded bo'lgan yozuv o'chmaydi
   (target'ning `UploadAsync` ichida push qilib simulyatsiya).
4. Boshqa (purge qilinmagan) yozuvga bog'langan blob o'chmaydi;
   hech qanday yozuvga bog'lanmagan yangi blob o'chmaydi.
5. Yetim blob darhol o'chmaydi, `orphaned_at` 24 soatdan eski bo'lganda
   o'chadi; `ExistsAsync` belgini tozalaydi.
6. `ManualDownloadTarget` bilan hech narsa o'chmaydi; `ConfirmAsync`
   dan keyin o'chadi; arxiv o'zgartirilgan bo'lsa `ConfirmAsync` rad
   etadi.
7. Target `Sha256` ni noto'g'ri qaytarsa — o'chirish yo'q.

Har bir xavfsizlik testini **ataylab buzib** (masalan seq shartini olib
tashlab) yiqilishini tekshiring va hisobotda qaysi buzish qaysi testni
yiqitganini yozing.

## Tugatish

- `dotnet build` 0 warning, `dotnet test` hammasi o'tadi.
- Commit(lar): `Co-Authored-By` YO'Q. Push faqat `origin/Oybek`.
- `PROGRESS.md`: Task 6 qatorini to'ldiring, plandan chetlashishlarni
  sabablari bilan yozing.
