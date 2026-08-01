# Multi-Device Sync — Plan Series Index

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement these plans task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Spec:** [`docs/superpowers/specs/2026-07-29-multi-device-sync-backend-design.md`](../specs/2026-07-29-multi-device-sync-backend-design.md)

Spec bitta plan uchun juda katta, shuning uchun **5 ta mustaqil plan**ga
bo'lingan. Har biri o'zicha ishlaydigan, sinaladigan natija beradi va
o'zidan oldingilariga tayanadi.

---

## Tartib va sabab

| № | Plan | Natija | Fayl |
|---|---|---|---|
| 1a | Backend poydevori | Sxema, runtime config, qurilma auth, JWT, audit | [01a-backend-foundation](2026-07-29-multi-device-sync-01a-backend-foundation.md) ✅ |
| 1b | Backend sync yadrosi | `seq`, push/pull, media, kalitlar, pagination, WS, `.cmx` | [01b-backend-sync](2026-07-29-multi-device-sync-01b-backend-sync.md) ✅ |
| 2 | `tdesktop` sync agenti | Desktop ma'lumoti serverga chiqadi — birinchi haqiqiy qiymat | [02-tdesktop-agent](2026-07-29-multi-device-sync-02-tdesktop-agent.md) ✅ |
| 3 | `server-controller` web app | Ko'rish, boshqarish, statistika | [03-web-controller](2026-07-29-multi-device-sync-03-web-controller.md) ✅ |
| 4 | Storage lifecycle manager | Monitoring, retention, arxivlash, xavfsiz o'chirish | [04-storage-lifecycle](2026-07-29-multi-device-sync-04-storage-lifecycle.md) ✅ |
| 5 | Always-on TDLib capture service | Muammo A hal bo'ladi | `…-05-capture-service.md` |

**1 nima uchun 1a va 1b ga bo'lindi:** bitta hujjat sifatida u o'qib
bo'lmaydigan darajada uzun bo'lardi va delegatsiya qilinganda ishonchsiz
bajarilardi. 1a o'zicha ishlaydigan natija beradi (auth va konfiguratsiya
ishlaydigan API), 1b esa uning ustiga sync qatlamini quradi.

**Nima uchun 4 → 5 dan oldin:** capture service 24/7 hamma narsani
yozadi. Bu oqimni ochishdan **oldin** xotira boshqaruvi, monitoring va
retention tayyor turishi kerak — aks holda disk birinchi haftadayoq
to'lib qolishi mumkin.

**Nima uchun 5 oxirida:** TDLib eng katta yangi bog'liqlik va eng katta
noma'lumlik. U oxirida bo'lgani uchun, u bilan muammo chiqsa ham
1-4 allaqachon ishlab turgan bo'ladi.

---

## Butun seriya bo'ylab amal qiladigan qoidalar

Bu qoidalar har bir plandagi har bir task uchun kuchda. Ular takrorlanmaydi.

### K1 — Konfiguratsiya kodda bo'lmaydi

Sozlanishi mumkin bo'lgan **har qanday** qiymat `server_settings`
jadvalida saqlanadi va web app'dan tahrirlanadi. `appsettings.json` da
faqat bootstrap qiymatlari qoladi (DB connection string, port, log yo'li).

Kodda literal bo'lishi **taqiqlangan**: sync interval, batch hajmi,
sahifa hajmi, retention muddatlari, disk chegaralari, rate limit
qiymatlari, timeout'lar, feature toggle'lar.

Sabab: "kichik o'zgarish uchun kodga tegilmasin va qayta deploy
qilinmasin" — bu loyihaning aniq talabi.

### K2 — Hech qachon UI oqimini bloklamaslik

- tdesktop: barcha tarmoq va crypto ishi `crl::async` fon oqimida
- web app: barcha deshifrlash va indekslash Web Worker'da
- Ro'yxatlar: virtual scrolling, hech qachon to'liq render emas

### K3 — Offset pagination taqiqlangan

Faqat keyset (cursor) pagination + `seq` snapshot. Sabab va SQL:
spec 5.6-bo'lim.

### K4 — Yozish idempotent

Har qanday push/import qayta yuborilishi mumkin va natija o'zgarmasligi
kerak. `record_id` PRIMARY KEY buni ta'minlaydi.

### K5 — Sync o'chiq bo'lganda regressiya nolga teng

tdesktop'ning mavjud capture logikasi va UI'si sync o'chiq bo'lganda
bugungidan hech qanday farq qilmasligi kerak. Bu har bir tdesktop
task'ida tekshiriladi.

### K6 — TDD

Har task: avval yiqiladigan test → yiqilishini ko'rish → minimal
implementatsiya → o'tishini ko'rish → commit.

### K7 — Commit uslubi

Imperativ sarlavha, tanada **nima uchun** (nima qilinganini diff ko'rsatadi).
`Co-Authored-By` trailer **qo'shilmaydi**. Branch: `Oybek`.
`upstream` remote'ga **hech qachon** push qilinmaydi.

---

## Repozitoriylar

| Komponent | Joyi |
|---|---|
| tdesktop sync agenti | Mavjud `tdesktop` repo, `Oybek` branch |
| server-backend | **Yangi repo**: `customsync-server` |
| server-controller | Xuddi shu repo, `web/` katalogida |
| capture service | Xuddi shu repo, alohida .NET proyekt |

Server tomonining hammasi bitta repoda — birga deploy qilinadi va
umumiy kontraktlarni (DTO, test vektorlari) baham ko'radi.
