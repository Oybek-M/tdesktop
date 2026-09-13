# Plan 04 Task 6 — tekshiruvdan chiqqan tuzatishlar (prompt)

Repo: `customsync-server` (branch `Oybek`). Tekshirilgan commit: `ce9a032`.
Asl talablar: [`04-task6-prompt.md`](04-task6-prompt.md). Boshlang'ich
holat: `dotnet test` -> 146/146, 0 warning.

Tekshiruv (2026-09-14, Claude, tdesktop sessiyasi): testlar mustaqil
yurgizildi; Gemini'nikidan BOSHQA 6 ta mutatsiya qilindi. 3 tasi testlar
tomonidan ushlandi (read-back SHA, ConfirmAsync SHA, orphan `NOT EXISTS`),
**3 tasi ushlanmadi** — quyidagi T1–T3. Kod o'qishdan yana 3 ta xato
chiqdi — K1–K3. Hammasi tuzatilmaguncha Task 7 (rejalashtirilgan ishlar)
BOSHLANMASIN: Task 7 purge'ni nazoratsiz, har kecha ishga tushiradi.

## Kod xatolari

### K1. `ConfirmAsync` retention'ni qayta baholamaydi — `never_delete` chetlab o'tiladi
`awaiting_confirmation` run kunlab kutishi mumkin. Shu orada operator
peer uchun `never_delete` qo'shsa yoki siyosatni o'chirsa ham,
`ConfirmAsync` arxivdagi hamma yozuvni o'chiradi.
- Tuzatish: arxivdan olingan kalitlar bo'yicha bazadagi joriy qatorlarni
  o'qib, `GetMatchedRecordsAsync` dagi bilan AYNAN bir xil evaluator
  filtridan (`ShouldAct && PolicyId == run.PolicyId`) o'tkazing — faqat
  o'tganlari o'chiriladi. Evaluator chaqiruvini bitta yordamchi metodga
  ajrating, ikki yo'l bir kodni ishlatsin.
- Test: Execute (manual target) -> `never_delete` qo'shish -> Confirm ->
  yozuv joyida.

### K2. Yetim blob `DELETE` shartni qayta tekshirmaydi (poyga)
`selectOldOrphansSql` va `deleteOldOrphansSql` alohida; `DELETE` faqat
`hash = ANY(...)`. Oradagi paytda `ExistsAsync` `orphaned_at` ni NULL
qilsa yoki push yangi `record_media` havolasini qo'shsa, blob baribir
o'chadi — havola qilingan blob yo'qoladi.
- Tuzatish: bitta so'rov: `DELETE FROM media_blobs mb WHERE
  mb.orphaned_at IS NOT NULL AND mb.orphaned_at <= @cutoff AND NOT EXISTS
  (SELECT 1 FROM record_media rm WHERE rm.hash = mb.hash) RETURNING
  storage_path`; fayllar RETURNING'dan. Yangi havola poygasini yopish
  uchun orphan sweep'ni yozuvlar o'chirilishidan ALOHIDA qisqa
  tranzaksiyaga oling va uning boshida `LOCK TABLE record_media IN SHARE
  MODE` (push'lar faqat shu qisqa tranzaksiya davomida kutadi). Nega
  alohida — izohda yozing.

### K3. Nomzodlar evaluator'dan OLDIN `Take(limit)` — siyosat "och qoladi"
Eng eski 5000 nomzod `never_delete` bilan himoyalangan yoki boshqa
siyosatga tegishli bo'lsa, `matched = 0`, har run `nothing_to_do`, undan
yangiroq yaroqli yozuvlar hech qachon o'chmaydi — disk to'ladi (Task 6/7
aynan shuning oldini olishi kerak).
- Tuzatish: keyset sahifalash (`received_at, seq`) — `limit` ta mos
  yozuv to'planguncha yoki nomzodlar tugaguncha; umumiy skan chegarasi
  (masalan 50 000) bilan.
- Test: 6 ta himoyalangan eski yozuv + 1 ta yaroqli yangiroq yozuv,
  `limit = 5` -> yaroqli yozuv o'chadi.

## Test bo'shliqlari (mutatsiya o'tib ketdi)

### T1. `PolicyId == policyId` sharti olib tashlansa hech bir test yiqilmaydi
Bu shart bo'lmasa `delete_only` siyosati `archive_then_delete` siyosati
yutgan yozuvlarni ARXIVSIZ o'chiradi.
- Test: bir xil qamrovda priority 20 `archive_then_delete` (A) va
  priority 10 `delete_only` (B). `ExecuteAsync(B)` -> hech narsa
  o'chmaydi.

### T2. Arxivga media yozilmasa hech bir test yiqilmaydi
`archive_then_delete` da media arxivga tushmasa, yozuv o'chadi, blob 24
soatdan keyin o'chadi — media hech qayerda qolmaydi.
- Test: media'li yozuv purge qilinadi -> target'dagi arxivni
  `CmxReader` bilan o'qib `media/<hash>` baytlari diskdagi blob bilan bir
  xil; `MediaRef.Size/Nonce` `media_blobs` dan. Diskda yo'q blob ->
  `MissingMedia = 1`.

### T3. Yuborishdan oldingi `CmxReader` yozuvlar soni tekshiruvi sinalmagan
Kichik; buzilgan arxivni simulyatsiya qilish qiyin bo'lsa, tekshiruvni
alohida `internal static` metodga ajratib shu metodni sinang.

## Mayda (bitta commit'da, test shart emas)

- `PROGRESS.md`: Task 6 qatorida commit `7e9add8` yozilgan — haqiqiysi
  `ce9a032` (+ shu tuzatish commit'i). "Tekshiruvda topilgan" ustuniga
  K1–K3 va T1–T3 ni yozing.
- `ConfirmAsync`: stream `CanSeek` bo'lmasa (kelajakdagi S3 target)
  hash'dan keyin o'qish buziladi — seek qilib bo'lmasa vaqtinchalik
  faylga ko'chirib, o'sha fayldan hash va o'qish.
- `ConfirmAsync` holat o'tishini shartli qiling (`UPDATE archive_runs SET
  status='completed' ... WHERE run_id=@id AND
  status='awaiting_confirmation'`): ikki parallel tasdiq run'dagi
  `deleted_count` ni 0 bilan ustiga yozmasin.
- Istisno bilan tugagan run (target exception, arxiv soni mos kelmadi)
  `archive_runs` ga `failed_*` bo'lib yozilsin — hozir iz qolmaydi.
- Orphan sweep hozir faqat mos yozuv topilgan run'da ishlaydi; Task 7
  uni alohida chaqira olishi uchun public metod qiling.
- Test sinfidagi qamrovi global siyosatlar (`pol_purge_all`, `Kind =
  activity`, `PeerHash` yo'q) boshqa testlarning yozuvlariga ham ta'sir
  qiladi — hozir specificity tufayli o'tadi, lekin mo'rt. Unga ham
  test peer'i bering yoki test oxirida o'chiring.

## Tugatish

- `dotnet build` 0 warning, `dotnet test` hammasi o'tadi.
- K1–K3 va T1 uchun: yangi testni yozib, tuzatishni vaqtincha qaytarib,
  yiqilishini ko'rsating (hisobotda).
- `Co-Authored-By` YO'Q. Push faqat `origin/Oybek`.
