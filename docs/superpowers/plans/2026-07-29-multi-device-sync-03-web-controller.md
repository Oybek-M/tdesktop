# Server Controller Web App Implementation Plan (03)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `server-backend` uchun boshqaruv va ko'rish yuzasi — serverni to'liq boshqarish, arxivni qidirish/filtrlash/saralash va statistika, hech qanday freez yoki lag'siz.

**Architecture:** Vue 3 + Vite + TypeScript SPA, PrimeVue komponentlari aaPanel uslubida stillangan. **Barcha kriptografiya va indekslash Web Worker'da** — asosiy oqim hech qachon bloklanmaydi. Deshifrlangan yozuvlar IndexedDB'da lokal indeksga tushadi, shuning uchun matn qidiruvi darhol ishlaydi. Master kalit **faqat Worker xotirasida** yashaydi va hech qachon saqlanmaydi.

**Tech Stack:** Vue 3, Vite, TypeScript, PrimeVue 4, Pinia, Web Crypto API, IndexedDB.

**Kirish sharti:** [01b](2026-07-29-multi-device-sync-01b-backend-sync.md) tugagan, server ishlab turibdi, `docs/sync-protocol/test-vectors.json` mavjud.

**Umumiy qoidalar:** [00-index](2026-07-29-multi-device-sync-00-index.md) dagi K1–K7. Ayniqsa **K2** (UI bloklanmaydi) va **K3** (offset pagination taqiqlangan).

---

## Ikki tekislik (plane)

Bu ajratish butun ilovaning tuzilishini belgilaydi:

| | Control plane | Data plane |
|---|---|---|
| Kalit kerakmi | **Yo'q** | Ha |
| Nimani ko'radi | Metadata | Deshifrlangan kontent |
| Qamrovi | Qurilmalar, sync holati, disk, sozlamalar, audit, kalit o'ramlari | Arxiv brauzeri, qidiruv, statistika |

**Muhim:** parol kiritilmasa ham ilova **to'liq foydali** bo'lib qoladi —
serverni boshqarish uchun kalit umuman kerak emas.

---

## File Structure

```
web/
├── index.html
├── vite.config.ts
├── package.json
└── src/
    ├── main.ts
    ├── App.vue
    ├── theme/
    │   └── aapanel.css            # aaPanel uslubidagi zich admin dizayni
    ├── api/
    │   ├── client.ts              # fetch wrapper: auth, refresh, AbortController
    │   ├── devices.ts
    │   ├── records.ts
    │   ├── stats.ts
    │   ├── settings.ts
    │   └── keys.ts
    ├── crypto/
    │   ├── worker.ts              # BARCHA crypto shu yerda. Kalit shu yerdan chiqmaydi.
    │   ├── worker-api.ts          # asosiy oqim uchun tipli proxy
    │   └── primitives.ts          # Web Crypto ustidan yupqa qatlam
    ├── db/
    │   ├── schema.ts              # IndexedDB store va indekslar
    │   └── sync.ts                # inkremental pull + deshifrlash + yozish
    ├── stores/
    │   ├── auth.ts
    │   ├── vault.ts               # qulf holati
    │   └── ui.ts
    ├── components/
    │   ├── AppShell.vue           # sidebar + topbar layout
    │   ├── StatCard.vue
    │   ├── RecordTable.vue        # virtual scroll + keyset pagination
    │   └── PeerName.vue           # peer_hash → ism
    └── views/
        ├── DashboardView.vue
        ├── DevicesView.vue
        ├── ArchiveView.vue
        ├── StatsView.vue
        ├── StorageView.vue
        ├── SettingsView.vue
        ├── KeysView.vue
        └── AuditView.vue
```

**Nima uchun `crypto/worker.ts` alohida:** master kalit u yerdan hech
qachon chiqmaydi. Asosiy oqim faqat "shu yozuvni ochib ber" deb so'raydi
va ochilgan matnni oladi — kalitning o'ziga hech qachon ega bo'lmaydi.
Bu XSS holatida ham kalitni himoya qiladi.

---

## Task 1: Skelet va aaPanel layout

**Files:**
- Create: `web/` loyihasi
- Create: `web/src/theme/aapanel.css`
- Create: `web/src/components/AppShell.vue`

- [ ] **Step 1: Loyihani yaratish**

```bash
cd customsync-server
npm create vite@latest web -- --template vue-ts
cd web
npm install
npm install primevue @primeuix/themes primeicons pinia vue-router
```

- [ ] **Step 2: Layout stilini yozish**

`web/src/theme/aapanel.css`:

```css
/*
 * aaPanel uslubidagi zich admin dizayni.
 *
 * Tamoyillar:
 *  - Ko'p ma'lumot kam joyda: qator balandligi 34px, shrift 13px
 *  - Animatsiya minimal: faqat hover va focus, 120ms
 *  - Soya va gradient yo'q — ular chiroyli, lekin uzun ro'yxatlarda
 *    har bir qator uchun qayta chiziladi va scroll'ni sekinlashtiradi
 */

:root {
  --cs-sidebar-width: 220px;
  --cs-topbar-height: 48px;
  --cs-row-height: 34px;
  --cs-radius: 4px;

  --cs-bg:        #f4f6f8;
  --cs-surface:   #ffffff;
  --cs-border:    #e3e7ec;
  --cs-text:      #1f2933;
  --cs-text-dim:  #6b7784;
  --cs-accent:    #1f7ae0;
  --cs-danger:    #d9484c;
  --cs-ok:        #2fa360;
}

:root[data-theme="dark"] {
  --cs-bg:        #16191d;
  --cs-surface:   #1e2227;
  --cs-border:    #2b3138;
  --cs-text:      #e6eaee;
  --cs-text-dim:  #8b95a1;
  --cs-accent:    #4a9bf0;
}

body {
  margin: 0;
  background: var(--cs-bg);
  color: var(--cs-text);
  font: 13px/1.5 -apple-system, "Segoe UI", Roboto, sans-serif;
}

.cs-shell     { display: grid; grid-template-columns: var(--cs-sidebar-width) 1fr;
                min-height: 100vh; }
.cs-sidebar   { background: var(--cs-surface);
                border-right: 1px solid var(--cs-border); }
.cs-main      { display: flex; flex-direction: column; min-width: 0; }
.cs-topbar    { height: var(--cs-topbar-height); display: flex; align-items: center;
                gap: 12px; padding: 0 16px; background: var(--cs-surface);
                border-bottom: 1px solid var(--cs-border); }
.cs-content   { padding: 16px; min-width: 0; }

.cs-card      { background: var(--cs-surface); border: 1px solid var(--cs-border);
                border-radius: var(--cs-radius); padding: 14px; }
.cs-grid      { display: grid; gap: 12px;
                grid-template-columns: repeat(auto-fill, minmax(220px, 1fr)); }

.cs-nav-item  { display: block; padding: 8px 16px; color: var(--cs-text-dim);
                text-decoration: none; transition: background 120ms, color 120ms; }
.cs-nav-item:hover      { background: var(--cs-bg); color: var(--cs-text); }
.cs-nav-item.is-active  { color: var(--cs-accent);
                          box-shadow: inset 2px 0 0 var(--cs-accent); }

/* Keng jadval o'z konteynerida gorizontal scroll qiladi — sahifa
   tanasi hech qachon gorizontal scroll qilmasligi kerak. */
.cs-table-wrap { overflow-x: auto; }
```

- [ ] **Step 3: `AppShell.vue` ni yozish**

Sidebar + topbar + `<router-view>`. Sidebar bo'limlari: Boshqaruv paneli,
Qurilmalar, Arxiv, Statistika, Xotira, Kalitlar, Sozlamalar, Audit.

Topbar'da: qulf holati indikatori (ochiq/yopiq), tema almashtirgich,
server holati nuqtasi.

- [ ] **Step 4: Ishga tushirib tekshirish**

```bash
npm run dev
```

Kutilgan: layout ko'rinadi, sidebar bo'limlari orasida o'tish ishlaydi.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat(web): scaffold Vue app with dense admin layout

No shadows or gradients in the base theme: they look better in isolation
but get repainted per row in long lists and measurably slow scrolling,
which is the opposite of what this panel needs."
```

---

## Task 2: API klienti va autentifikatsiya

Web app ham **oddiy qurilma** — `platform: "web"` bilan ro'yxatdan o'tadi.
Backend'da hech qanday maxsus holat yo'q.

**Files:**
- Create: `web/src/api/client.ts`
- Create: `web/src/stores/auth.ts`
- Create: `web/src/views/EnrollView.vue`

- [ ] **Step 1: API klientini yozish**

`web/src/api/client.ts`:

```ts
/**
 * Backend bilan aloqa. Uchta narsani hal qiladi:
 *  - JWT qo'shish va muddati tugaganda avtomatik yangilash
 *  - AbortController: eskirgan so'rovlar natijasi yangisini bosib
 *    ketmasligi uchun (qidiruvda tez yozganda klassik race)
 *  - Xatolarni bir xil shaklga keltirish
 */

let accessToken: string | null = null;
let expiresAt = 0;

export class ApiError extends Error {
  constructor(readonly status: number, message: string) {
    super(message);
  }
}

async function ensureToken(): Promise<string> {
  if (accessToken && Date.now() < expiresAt - 30_000) return accessToken;

  const deviceId = localStorage.getItem('cs.deviceId');
  const refreshToken = localStorage.getItem('cs.refreshToken');
  if (!deviceId || !refreshToken) throw new ApiError(401, 'not_enrolled');

  const response = await fetch('/api/v1/devices/refresh', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ deviceId, refreshToken }),
  });
  if (!response.ok) {
    localStorage.removeItem('cs.refreshToken');
    throw new ApiError(response.status, 'refresh_failed');
  }

  const data = await response.json();
  accessToken = data.accessToken;
  expiresAt = new Date(data.expiresAt).getTime();
  // Refresh token har ishlatilganda almashadi — yangisini saqlaymiz.
  localStorage.setItem('cs.refreshToken', data.refreshToken);
  return accessToken!;
}

export async function api<T>(
  path: string,
  options: RequestInit & { signal?: AbortSignal } = {},
): Promise<T> {
  const token = await ensureToken();
  const response = await fetch(path, {
    ...options,
    headers: {
      ...(options.headers ?? {}),
      Authorization: `Bearer ${token}`,
      ...(options.body ? { 'Content-Type': 'application/json' } : {}),
    },
  });

  if (!response.ok) {
    throw new ApiError(response.status, await response.text());
  }
  return response.status === 204 ? (undefined as T) : await response.json();
}

export async function enroll(
  code: string, name: string,
): Promise<void> {
  const response = await fetch('/api/v1/devices/enroll', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ code, name, platform: 'web' }),
  });
  if (!response.ok) throw new ApiError(response.status, 'enroll_failed');

  const data = await response.json();
  localStorage.setItem('cs.deviceId', data.deviceId);
  localStorage.setItem('cs.refreshToken', data.refreshToken);
  accessToken = data.accessToken;
  expiresAt = new Date(data.expiresAt).getTime();
}
```

**Xavfsizlik eslatmasi:** refresh token `localStorage` da. O'zi
joylashtirilgan (self-hosted), tashqi skript umuman yo'q, qat'iy CSP
bilan — bu holat uchun maqbul. Agar keyinchalik tashqi skript qo'shilsa,
bu qaror qayta ko'rib chiqilishi kerak.

- [ ] **Step 2: Enrollment ekranini yozish**

Ro'yxatdan o'tmagan holatda barcha marshrutlar `EnrollView` ga
yo'naltiriladi. Unda: kod kiritish maydoni va "Ulash" tugmasi.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat(web): add API client with token refresh and enrolment

The browser enrols as an ordinary device with platform 'web', so the
backend needs no special case for it. Refresh rotation means the stored
token changes on every renewal and a captured one is single-use."
```

---

## Task 3: Control plane ekranlari

Kalit talab qilmaydigan hamma narsa. Bu qism birinchi ishlaydi va o'zicha
foydali.

**Files:**
- Create: `web/src/views/DevicesView.vue`, `SettingsView.vue`, `AuditView.vue`, `StorageView.vue`, `DashboardView.vue`
- Create: `web/src/api/devices.ts`, `settings.ts`, `stats.ts`

- [ ] **Step 1: Qurilmalar ekrani**

Jadval: nom, platforma, ulangan sana, oxirgi ko'rinish, cursor, holat.
Amallar: "Bekor qilish" (tasdiq dialogi bilan), "Yangi kod yaratish".

Yangi kod modal oynada katta shriftda ko'rsatiladi (qo'lda ko'chirish
uchun) va amal qilish muddati sanog'i bilan.

- [ ] **Step 2: Sozlamalar ekrani**

`GET /api/v1/settings` dan kategoriya bo'yicha guruhlangan ro'yxat.
Har bir sozlama: kalit, tavsif, joriy qiymat, tahrirlash maydoni.

`valueType` ga qarab input turi: `int` → raqamli, `bool` → toggle,
`duration` → raqam + birlik, `string` → matn.

**Bu ekran K1 qoidasining foydalanuvchi tomoni** — bu yerda o'zgartirilgan
har qanday qiymat qayta deploy qilmasdan darhol kuchga kiradi.

- [ ] **Step 3: Audit va xotira ekranlari**

Audit: sana, amal, qurilma, tafsilot. Keyset pagination.

Xotira: umumiy hajm, yozuvlar/media taqsimoti, peer bo'yicha eng katta
10 talik (Task 9 da ismlar bilan to'ldiriladi).

- [ ] **Step 4: Boshqaruv paneli**

Yuqorida 4 ta `StatCard`: jami yozuvlar, disk hajmi, faol qurilmalar,
oxirgi sync. Pastda: so'nggi audit hodisalari va qurilmalar holati.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat(web): add control plane views

Everything here works without the master key, so the panel stays fully
useful for operating the server even when the vault is locked. The
settings screen is the user-facing half of the no-config-in-code rule:
values edited here take effect without a redeploy."
```

---

## Task 4: Crypto worker

**Files:**
- Create: `web/src/crypto/primitives.ts`
- Create: `web/src/crypto/worker.ts`
- Create: `web/src/crypto/worker-api.ts`
- Create: `web/tests/vectors.test.ts`

- [ ] **Step 1: Yiqiladigan testni yozish**

```bash
npm install -D vitest
```

`web/tests/vectors.test.ts`:

```ts
import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { computeRecordId, pbkdf2, hkdf } from '../src/crypto/primitives';

const vectors = JSON.parse(
  readFileSync('../docs/sync-protocol/test-vectors.json', 'utf8'));

const hex = (buffer: ArrayBuffer) =>
  [...new Uint8Array(buffer)].map(b => b.toString(16).padStart(2, '0')).join('');

describe('cross-platform vectors', () => {
  it('reproduces every record_id', async () => {
    for (const entry of vectors.record_id) {
      const { kind, peer_hash, msg_id, occurred_at } = entry.input;
      expect(await computeRecordId(kind, peer_hash, msg_id, occurred_at))
        .toBe(entry.expected);
    }
  });

  it('reproduces every PBKDF2 key', async () => {
    for (const entry of vectors.pbkdf2_hmac_sha256) {
      const { password, salt_hex, iterations } = entry.input;
      const derived = await pbkdf2(
        password, Uint8Array.from(Buffer.from(salt_hex, 'hex')), iterations, 32);
      expect(hex(derived)).toBe(entry.expected_key_hex);
    }
  }, 60_000); // 2M iteratsiya sekin — timeout oshirilgan

  it('reproduces every HKDF key', async () => {
    for (const entry of vectors.hkdf_sha256) {
      const { master_key_hex, salt_hex, info } = entry.input;
      const derived = await hkdf(
        Uint8Array.from(Buffer.from(master_key_hex, 'hex')),
        Uint8Array.from(Buffer.from(salt_hex, 'hex')),
        info, 32);
      expect(hex(derived)).toBe(entry.expected_key_hex);
    }
  });
});
```

- [ ] **Step 2: Testni ishga tushirib, yiqilishini ko'rish**

```bash
npx vitest run
```

Kutilgan: FAIL — `primitives.ts` mavjud emas.

- [ ] **Step 3: Primitivlarni yozish**

`web/src/crypto/primitives.ts`:

```ts
/**
 * Web Crypto API ustidan yupqa qatlam.
 * Barcha funksiyalar test-vectors.json ga mos kelishi SHART.
 */

const encoder = new TextEncoder();

export async function computeRecordId(
  kind: string, peerHash: string, msgId: number, occurredAt: number,
): Promise<string> {
  // kind ‖ 0x00 ‖ peerHash ‖ 0x00 ‖ msgId ‖ 0x00 ‖ occurredAt
  const parts = [
    encoder.encode(kind), Uint8Array.of(0),
    encoder.encode(peerHash), Uint8Array.of(0),
    encoder.encode(String(msgId)), Uint8Array.of(0),
    encoder.encode(String(occurredAt)),
  ];
  const total = parts.reduce((sum, p) => sum + p.length, 0);
  const buffer = new Uint8Array(total);
  let offset = 0;
  for (const part of parts) { buffer.set(part, offset); offset += part.length; }

  const digest = await crypto.subtle.digest('SHA-256', buffer);
  return [...new Uint8Array(digest)]
    .map(b => b.toString(16).padStart(2, '0')).join('');
}

export async function pbkdf2(
  password: string, salt: Uint8Array, iterations: number, lengthBytes: number,
): Promise<ArrayBuffer> {
  const base = await crypto.subtle.importKey(
    'raw', encoder.encode(password), 'PBKDF2', false, ['deriveBits']);
  return crypto.subtle.deriveBits(
    { name: 'PBKDF2', salt, iterations, hash: 'SHA-256' },
    base, lengthBytes * 8);
}

export async function hkdf(
  masterKey: Uint8Array, salt: Uint8Array, info: string, lengthBytes: number,
): Promise<ArrayBuffer> {
  const base = await crypto.subtle.importKey(
    'raw', masterKey, 'HKDF', false, ['deriveBits']);
  // Salt HAR DOIM oshkora beriladi — "salt yo'q" holati platformalarda
  // turlicha talqin qilinadi va jimgina interop buzilishiga olib keladi.
  return crypto.subtle.deriveBits(
    { name: 'HKDF', hash: 'SHA-256', salt, info: encoder.encode(info) },
    base, lengthBytes * 8);
}

export async function hmacSha256(
  key: Uint8Array, message: string,
): Promise<ArrayBuffer> {
  const hmacKey = await crypto.subtle.importKey(
    'raw', key, { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  return crypto.subtle.sign('HMAC', hmacKey, encoder.encode(message));
}

/**
 * AES-256-GCM. Kutilgan format: ciphertext ‖ tag(16) — Web Crypto
 * aynan shu tartibni kutadi, shuning uchun qo'shimcha ajratish kerak emas.
 *
 * Muvaffaqiyatsiz bo'lsa null qaytaradi. Chaqiruvchi buni tekshirishi
 * SHART — bitta buzilgan yozuv butun ro'yxatni yiqitmasligi kerak.
 */
export async function openSealed(
  rawKey: ArrayBuffer, nonce: Uint8Array, sealed: Uint8Array,
): Promise<ArrayBuffer | null> {
  try {
    const key = await crypto.subtle.importKey(
      'raw', rawKey, 'AES-GCM', false, ['decrypt']);
    return await crypto.subtle.decrypt(
      { name: 'AES-GCM', iv: nonce, tagLength: 128 }, key, sealed);
  } catch {
    return null;
  }
}
```

- [ ] **Step 4: Testni qayta ishga tushirish**

```bash
npx vitest run
```

Kutilgan: PASS — 3 ta test o'tdi.

- [ ] **Step 5: Worker'ni yozish**

`web/src/crypto/worker.ts` — master kalit shu modul ichida yashaydi va
`postMessage` orqali hech qachon tashqariga chiqmaydi:

```ts
/**
 * Barcha kriptografiya shu Worker'da bo'ladi.
 *
 * Ikki sabab:
 *  1. Asosiy oqim bloklanmaydi — minglab yozuvni deshifrlash sezilarli
 *     CPU ishi va uni UI oqimida qilish sahifani muzlatadi (qoida K2).
 *  2. Master kalit asosiy oqimga hech qachon o'tmaydi, shuning uchun
 *     XSS bo'lgan taqdirda ham kalitni o'g'irlab bo'lmaydi.
 */

import { pbkdf2, hkdf, openSealed } from './primitives';

let contentKey: ArrayBuffer | null = null;
let mediaKey: ArrayBuffer | null = null;
let peerKey: ArrayBuffer | null = null;

type Request =
  | { id: number; type: 'unlock'; passphrase: string; wrap: WrapPayload }
  | { id: number; type: 'lock' }
  | { id: number; type: 'openRecords'; records: SealedRecord[] };

self.onmessage = async (event: MessageEvent<Request>) => {
  const request = event.data;
  try {
    switch (request.type) {
      case 'unlock': {
        const kek = await pbkdf2(
          request.passphrase,
          request.wrap.salt, request.wrap.iterations, 32);
        const master = await openSealed(
          kek, request.wrap.nonce, request.wrap.wrappedKey);
        if (!master) {
          self.postMessage({ id: request.id, ok: false, error: 'wrong_passphrase' });
          return;
        }
        const raw = new Uint8Array(master);
        contentKey = await hkdf(raw, new Uint8Array(32), 'customsync-content-v1', 32);
        mediaKey   = await hkdf(raw, new Uint8Array(32), 'customsync-media-v1', 32);
        peerKey    = await hkdf(raw, new Uint8Array(32), 'customsync-peer-v1', 32);
        raw.fill(0);
        self.postMessage({ id: request.id, ok: true });
        return;
      }
      case 'lock': {
        contentKey = mediaKey = peerKey = null;
        self.postMessage({ id: request.id, ok: true });
        return;
      }
      case 'openRecords': {
        if (!contentKey) {
          self.postMessage({ id: request.id, ok: false, error: 'locked' });
          return;
        }
        const opened = [];
        for (const record of request.records) {
          const plain = await openSealed(contentKey, record.nonce, record.payload);
          // Buzilgan yozuv o'tkazib yuboriladi, butun to'plam yiqilmaydi.
          opened.push(plain
            ? { recordId: record.recordId, json: new TextDecoder().decode(plain) }
            : { recordId: record.recordId, corrupt: true });
        }
        self.postMessage({ id: request.id, ok: true, opened });
        return;
      }
    }
  } catch (error) {
    self.postMessage({ id: request.id, ok: false, error: String(error) });
  }
};
```

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat(web): add Web Crypto primitives verified against the vectors

All crypto runs in a Worker for two reasons: decrypting thousands of
records on the UI thread freezes the page, and keeping the master key out
of the main thread means an XSS cannot read it. The worker hands back
plaintext, never keys."
```

---

## Task 5: Qulf ochish va kalit o'ramlari

**Files:**
- Create: `web/src/stores/vault.ts`
- Create: `web/src/views/KeysView.vue`
- Create: `web/src/components/UnlockDialog.vue`

- [ ] **Step 1: Qulf ochish oqimi**

1. `GET /api/v1/keys/wraps` — mavjud o'ramlar ro'yxati
2. Foydalanuvchi turini tanlaydi (parol / tiklash kodi / email+PIN)
3. `GET /api/v1/keys/wraps/{id}` — salt, nonce, o'ralgan kalit
4. Worker'ga `unlock` yuboriladi
5. Muvaffaqiyat: `vault.locked = false`. Xato: aniq xabar

**Muhim:** master kalit hech qachon `localStorage` yoki `sessionStorage`
ga yozilmaydi. Sahifa yangilansa qayta ochish kerak — bu ataylab.

- [ ] **Step 2: Avtomatik qulflash**

Sozlamada belgilangan harakatsizlik vaqtidan keyin worker'ga `lock`
yuboriladi. Sanoq `mousemove`/`keydown` da qayta boshlanadi.

- [ ] **Step 3: Kalit o'ramlarini boshqarish ekrani**

Ro'yxat: turi, yorlig'i, yaratilgan sana, oxirgi ishlatilgan.
Amallar: yangi o'ram qo'shish, o'chirish.

**O'chirishda ogohlantirish majburiy:** "Bu oxirgi o'ram. O'chirsangiz
arxivga kirish butunlay yo'qoladi." — agar bitta o'ram qolgan bo'lsa,
o'chirishga umuman ruxsat bermang.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(web): add vault unlock and key wrap management

The master key is never persisted to storage -- a page refresh requires
unlocking again, which is the intended trade. Deleting the last remaining
wrap is blocked outright rather than warned about, because there is no
recovery from it."
```

---

## Task 6: IndexedDB indeks va inkremental sync

Bu qidiruvni **darhol** qiladigan qism.

**Files:**
- Create: `web/src/db/schema.ts`
- Create: `web/src/db/sync.ts`

- [ ] **Step 1: IndexedDB sxemasini yozish**

`web/src/db/schema.ts`:

```ts
/**
 * Lokal deshifrlangan indeks.
 *
 * Nima uchun kerak: server shifrlangan kontent ichini qidira olmaydi.
 * Yozuvlarni bir marta yuklab olib brauzerda deshifrlaymiz va shu yerda
 * indekslaymiz — shundan keyin qidiruv darhol va offline ishlaydi.
 */

export const DB_NAME = 'customsync';
export const DB_VERSION = 1;

export function openDatabase(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DB_NAME, DB_VERSION);

    request.onupgradeneeded = () => {
      const db = request.result;

      const records = db.createObjectStore('records', { keyPath: 'recordId' });
      // Kompozit indeks keyset pagination uchun: (occurredAt, seq)
      // juftligi qat'iy tartib beradi va bir xil vaqtli qatorlar
      // tushib qolmaydi yoki takrorlanmaydi.
      records.createIndex('byTime', ['occurredAt', 'seq']);
      records.createIndex('byPeerTime', ['peerHash', 'occurredAt', 'seq']);
      records.createIndex('byKindTime', ['kind', 'occurredAt', 'seq']);

      db.createObjectStore('meta', { keyPath: 'key' });
      db.createObjectStore('peers', { keyPath: 'peerHash' });
    };

    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}
```

- [ ] **Step 2: Inkremental sync'ni yozish**

`web/src/db/sync.ts`:

- `meta` store'da `cursor` saqlanadi
- `GET /api/v1/sync/pull?since=cursor&limit=500` sikl bilan
- Har bo'lak worker'ga `openRecords` uchun yuboriladi
- Deshifrlangan natija IndexedDB'ga yoziladi
- Cursor **bo'lak muvaffaqiyatli yozilgandan keyin** yangilanadi
- Jarayon davomida UI'da progress ko'rsatiladi (bloklanmaydi)

- [ ] **Step 3: Birinchi yuklash tajribasini yaxshilash**

Birinchi marta arxiv katta bo'lishi mumkin. Foydalanuvchi kutib
qolmasligi uchun:

- Progress: "12 400 / 48 000 yozuv indekslandi"
- **Qisman natijalar darhol ko'rinadi** — indeks to'liq tayyor bo'lishini
  kutmaydi
- Bekor qilish tugmasi
- Keyingi kirishlarda faqat yangi yozuvlar qo'shiladi (sekundlar)

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(web): add local decrypted index with incremental sync

The cursor advances only after a batch is written, so an interrupted sync
re-fetches that batch rather than skipping it -- dedup by recordId makes
the repeat harmless. Partial results render while indexing continues, so
the first load is usable before it finishes."
```

---

## Task 7: Arxiv brauzeri — virtual scroll va keyset pagination

Foydalanuvchining "freez bo'lmasin, pagination konflikt qilmasin"
talabining asosiy qismi.

**Files:**
- Create: `web/src/components/RecordTable.vue`
- Create: `web/src/views/ArchiveView.vue`

- [ ] **Step 1: Jadval komponentini yozish**

PrimeVue `DataTable` ni `scrollable` + `virtualScrollerOptions` bilan
lazy rejimda ishlating:

```vue
<DataTable
  :value="rows"
  scrollable
  scrollHeight="flex"
  :virtualScrollerOptions="{
    lazy: true,
    itemSize: 34,
    onLazyLoad: loadMore,
    showLoader: true,
    delay: 0,
  }"
  dataKey="recordId"
>
```

**Nima uchun virtual scroll:** 50 000 qatorni DOM'ga chiqarish brauzerni
muzlatadi. Virtual scroller faqat ko'rinadigan ~30 qatorni render qiladi.

- [ ] **Step 2: Keyset pagination'ni ulash**

```ts
/**
 * Snapshot so'rov boshlanganda bir marta olinadi va barcha sahifalar
 * uchun o'zgarmaydi. Bu paytda kelgan yangi yozuvlar qatorlarni surib
 * yubormaydi — ular tepadagi "N ta yangi yozuv" bannerida to'planadi.
 *
 * OFFSET umuman ishlatilmaydi (qoida K3).
 */
async function loadMore() {
  if (loading.value || exhausted.value) return;
  loading.value = true;

  const page = await api<RecordPage>(
    `/api/v1/records?snapshot=${snapshot.value}` +
    `&limit=${pageSize}&desc=${descending.value}` +
    (afterKey.value !== null
      ? `&afterKey=${afterKey.value}&afterSeq=${afterSeq.value}` : '') +
    filterQuery.value,
    { signal: controller.signal });

  rows.value.push(...page.records);
  afterKey.value = page.nextAfterKey;
  afterSeq.value = page.nextAfterSeq;
  exhausted.value = page.records.length < pageSize;
  loading.value = false;
}
```

- [ ] **Step 3: Saralash va filtr o'zgarganda qayta boshlash**

Saralash yo'nalishi yoki filtr o'zgarsa:

1. Joriy `AbortController` bekor qilinadi
2. `rows`, `afterKey`, `afterSeq` tozalanadi
3. **Yangi snapshot olinadi**
4. Birinchi sahifa yuklanadi

Eski so'rovning javobi kelib qolsa, u bekor qilingani uchun e'tiborsiz
qoladi — natijalar aralashib ketmaydi.

- [ ] **Step 4: "Yangi yozuvlar" banneri**

Fon sync yangi yozuv topsa, jadval **o'zgarmaydi**. Tepada banner
chiqadi: "14 ta yangi yozuv — yangilash". Bosilganda yangi snapshot
bilan qaytadan yuklanadi.

- [ ] **Step 5: Qo'lda tekshirish**

| Tekshiruv | Kutilgan |
|---|---|
| 50 000 yozuvli ro'yxatda scroll | Silliq, freez yo'q |
| Scroll paytida yangi yozuv keladi | Qatorlar surilmaydi, banner chiqadi |
| Saralashni asc/desc almashtirish | Darhol, dublikatsiz |
| Tez-tez filtr o'zgartirish | Natijalar aralashmaydi |

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat(web): add virtualised archive table with keyset paging

Rendering 50k rows into the DOM freezes the browser, so only the visible
~30 exist at a time. Every page is pinned to one snapshot taken when the
query started: records arriving mid-scroll collect in a banner instead of
shifting rows under the cursor and producing duplicates."
```

---

## Task 8: Qidiruv rejimi

**Files:**
- Modify: `web/src/views/ArchiveView.vue`
- Create: `web/src/db/search.ts`

- [ ] **Step 1: Ikki rejimni ajratish**

Qoida: **bitta so'rov — bitta manba.** Aralashtirish taqiqlanadi.

| Rejim | Qachon | Manba |
|---|---|---|
| Metadata | Qidiruv maydoni bo'sh | Server, keyset pagination |
| Qidiruv | Qidiruv maydonida matn bor | IndexedDB, o'sha keyset naqshi |

Ikkalasi ham bir xil sahifalash shartnomasini bajaradi, shuning uchun
`RecordTable` rejimni bilishi shart emas.

- [ ] **Step 2: Qidiruvni yozish**

IndexedDB kursori bilan `byTime` indeksi bo'yicha teskari yurish,
har yozuv matnini tekshirish, sahifa to'lgach to'xtash.

Filtr (peer/tur/sana) qidiruvdan **oldin** qo'llanadi — mos indeksdan
foydalanib qidiruv maydonini keskin toraytiradi.

- [ ] **Step 3: Debounce va bekor qilish**

```ts
/**
 * Har harfda so'rov yubormaslik uchun debounce, va eskirgan so'rovni
 * bekor qilish uchun ketma-ket raqam. Ikkinchisisiz sekinroq bajarilgan
 * eski qidiruv natijasi yangisini bosib ketishi mumkin — bu foydalanuvchi
 * uchun "qidiruv noto'g'ri ishlayapti" bo'lib ko'rinadi.
 */
let searchGeneration = 0;

const runSearch = debounce(async (text: string) => {
  const generation = ++searchGeneration;
  const results = await searchLocal(text, filters.value, pageSize);
  if (generation !== searchGeneration) return; // eskirgan — tashlab yuboramiz
  rows.value = results;
}, 250);
```

- [ ] **Step 4: Indeks tayyor emasligini bildirish**

Agar lokal indeks hali to'liq yuklanmagan bo'lsa, qidiruv natijasi tepasida
ogohlantirish: "Indeks 62% tayyor — natijalar to'liq bo'lmasligi mumkin."
Foydalanuvchi noto'g'ri xulosaga kelmasligi kerak.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat(web): add client-side full-text search over the local index

Search cannot run server-side because the payloads are encrypted, so it
runs entirely against IndexedDB using the same keyset contract as the
server mode -- the table component never learns which mode it is in.

Stale results are dropped by generation counter: without it a slower
earlier query can land after a newer one and overwrite correct results,
which reads to the user as search being broken."
```

---

## Task 9: Statistika va peer nomlari

**Files:**
- Create: `web/src/views/StatsView.vue`
- Create: `web/src/components/PeerName.vue`

- [ ] **Step 1: Peer directory'ni ochish**

`peer_directory` turidagi yozuvlar worker'da deshifrlanadi va
IndexedDB'ning `peers` store'iga yoziladi: `peerHash → {name, username, type}`.

`PeerName.vue` shu store'dan o'qiydi. Topilmasa — `peerHash` ning
birinchi 8 belgisi ko'rsatiladi (`a3f9c2d1…`), shunda hech bo'lmasa
qatorlarni farqlash mumkin.

- [ ] **Step 2: Statistika ekranini yozish**

Serverdan `GET /api/v1/stats/peers?sort=bytes|count|recent`.
Server `peer_hash` bo'yicha guruhlaydi (HMAC deterministik bo'lgani
uchun bu ishlaydi), web app natijaga ismlarni qo'shadi.

Ko'rsatiladigan narsalar:
- "Kim bilan eng ko'p yozilgan" — yozuvlar soni bo'yicha
- "Kimning xabari eng ko'p joy olgan" — hajm bo'yicha
- Vaqt bo'yicha faollik grafigi
- Tur bo'yicha taqsimot

- [ ] **Step 3: Grafiklarni kechiktirilgan yuklash**

```ts
// Chart kutubxonasi faqat statistika sahifasi ochilganda yuklanadi —
// asosiy bundle'ga qo'shilmaydi.
const Chart = defineAsyncComponent(() => import('primevue/chart'));
```

- [ ] **Step 4: Ism bo'yicha saralash cheklovi**

Ism bo'yicha alifbo saralash **faqat shu jamlanma ko'rinishida** mavjud
(ro'yxat kichik — brauzerda bir zumda saraladi). Xom yozuvlar
ro'yxatida bu yo'q, chunki server ismlarni bilmaydi. UI'da bu variant
u yerda umuman ko'rsatilmaydi — o'chirilgan tugma sifatida emas.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat(web): add statistics with client-side peer name resolution

Aggregation happens server-side on the HMAC peer_hash, which works for
COUNT/SUM/ORDER BY without the server ever learning who the peer is; the
browser adds names afterwards from the decrypted directory. Charts are
loaded on demand so the main bundle stays small."
```

---

## Task 10: Performance tekshiruvi

Foydalanuvchining aniq talabi. Bu tekshiruvlar o'tmasa, task tugagan
hisoblanmaydi.

- [ ] **Step 1: Katta ma'lumot bilan sinov**

Test ma'lumotini generatsiya qiling (50 000 yozuv, 500 peer) va
tekshiring:

| Tekshiruv | Talab |
|---|---|
| Birinchi indekslash | Progress ko'rinadi, sahifa javob beradi |
| Ro'yxatda scroll | Freez yo'q |
| Saralash almashtirish | 200ms ichida javob |
| Qidiruv yozish | Har harfda kechikish sezilmaydi |
| Filtr o'zgartirish | Natijalar aralashmaydi |
| Statistika sahifasi | 1 soniyada ochiladi |
| Uzoq ishlatishda xotira | Barqaror, o'smaydi |

- [ ] **Step 2: Asosiy oqim bloklanmasligini tasdiqlash**

Chrome DevTools → Performance:

- Indekslash paytida yozib oling
- Asosiy oqimda **50ms dan uzun bitta task bo'lmasligi** kerak
- Crypto ishi Worker oqimida ko'rinishi kerak, asosiy oqimda emas

Agar asosiy oqimda uzun task topilsa — u yerda nima bo'layotganini
aniqlang va Worker'ga ko'chiring.

- [ ] **Step 3: Bundle hajmini tekshirish**

```bash
npm run build
```

Asosiy bundle 500 KB (gzip) dan oshmasligi kerak. Oshsa — nima
qo'shilganini `rollup-plugin-visualizer` bilan aniqlang va kechiktirilgan
yuklashga o'tkazing.

- [ ] **Step 4: Commit**

```bash
git commit --allow-empty -m "test(web): verify no long main-thread tasks under load

Profiled indexing and scrolling with 50k records: no main-thread task
exceeds 50ms and all crypto appears on the worker thread. Recorded here
because this is the requirement the whole worker/virtual-scroll structure
exists to satisfy."
```

---

## Qabul qilish mezonlari (03)

1. Vitest test vektorlarini o'tkazadi (brauzer crypto server bilan mos).
2. Parolsiz control plane **to'liq ishlaydi**.
3. Parol bilan arxiv ochiladi; master kalit hech qachon saqlanmaydi.
4. 50 000 yozuvda scroll, saralash, filtr, qidiruv — **freez yo'q**.
5. Sahifalash davomida yangi yozuv kelsa dublikat/tushib qolish yo'q.
6. Statistika "kim bilan ko'p yozilgan" va "kim ko'p joy olgan"
   savollariga ismlar bilan javob beradi.
7. Asosiy oqimda 50ms dan uzun task yo'q.
8. Oxirgi kalit o'ramini o'chirishga ruxsat berilmaydi.

---

## Keyingi qadam

Plan 04 — Storage lifecycle manager: monitoring, retention siyosatlari,
4 ta arxiv target (qo'lda yuklab olish, S3-mos, SFTP/rsync, Telegram bot)
va xavfsiz ikki fazali o'chirish.
