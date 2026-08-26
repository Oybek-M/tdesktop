# Track C uchun platformalararo test vektorlarini generatsiya qiladi.
# Natija: docs/sync-protocol/test-vectors.json
import hashlib, hmac, json, sys
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

def h(b): return b.hex()

# ---- record_id ----
# spec §0.12 (2026-08-26): account_hash qo'shildi -- akkaunt ajratmasi
# bo'lmasa, ikkita akkauntning bir xil (peer_hash, msg_id) juftligi
# serverda bir-birining ustiga yozilib ketardi.
def record_id(kind, account_hash, peer_hash, msg_id, occurred_at):
    m = hashlib.sha256()
    m.update(kind.encode()); m.update(b"\x00")
    m.update(account_hash.encode()); m.update(b"\x00")
    m.update(peer_hash.encode()); m.update(b"\x00")
    m.update(str(msg_id).encode()); m.update(b"\x00")
    m.update(str(occurred_at).encode())
    return m.hexdigest()

# ---- HKDF-SHA256 (RFC 5869), salt yo'q ----
def hkdf(master, info, length=32):
    prk = hmac.new(b"\x00" * 32, master, hashlib.sha256).digest()
    okm, t, i = b"", b"", 1
    while len(okm) < length:
        t = hmac.new(prk, t + info.encode() + bytes([i]), hashlib.sha256).digest()
        okm += t; i += 1
    return okm[:length]

master = bytes(range(32))                     # 00 01 02 ... 1f
peer_key = hkdf(master, "customsync-peer-v1")
content_key = hkdf(master, "customsync-content-v1")
media_key = hkdf(master, "customsync-media-v1")
account_key = hkdf(master, "customsync-account-v1")

def account_hash(account_id):
    return hmac.new(account_key, account_id.encode(), hashlib.sha256).digest()[:16].hex()

# spec §0.12: peer_hash endi account_id ni ham kiritadi -- shu bilan
# ikkita akkauntning bir xil peer_id'si turli peer_hash beradi.
def peer_hash(account_id, peer_id):
    msg = account_id.encode() + b"\x00" + peer_id.encode()
    return hmac.new(peer_key, msg, hashlib.sha256).digest()[:16].hex()

vec = {
  "_comment": [
    "Track C platformalararo test vektorlari.",
    "BESHALA app ham shu natijalarni AYNAN qayta hosil qila olishi SHART:",
    "tdesktop (C++/OpenSSL), server-backend (.NET), server-controller (Web Crypto),",
    "tmobile-android, tmobile-ios.",
    "Bir platformada base64 padding yoki nonce tartibi boshqacha bo'lsa,",
    "hamma narsa JIMGINA buziladi. Bu fayl buni birinchi kunda ushlaydi."
  ],
  "version": 1,

  "hkdf": {
    "_note": "HKDF-SHA256, salt = 32 bayt nol, info = quyidagi satr",
    "master_key_hex": h(master),
    "derived": {
      "customsync-content-v1": h(content_key),
      "customsync-media-v1":   h(media_key),
      "customsync-peer-v1":    h(peer_key),
      "customsync-account-v1": h(account_key)
    }
  },

  "account_hash": {
    "_note": [
      "spec §0.12 (2026-08-26). HMAC-SHA256(account_key, account_id)[0..16] -> hex.",
      "account_id — O'NLIK SATR (tdesktop: session().userId())."
    ],
    "cases": [
      {"account_id": "111222333", "account_hash": account_hash("111222333")},
      {"account_id": "444555666", "account_hash": account_hash("444555666")},
      {"account_id": "0",         "account_hash": account_hash("0")}
    ]
  },

  "peer_hash": {
    "_note": [
      "spec §0.12 (2026-08-26). HMAC-SHA256(peer_key, account_id || 0x00 || peer_id)[0..16] -> hex.",
      "account_id va peer_id — O'NLIK SATR. Eski formula (account_id'siz) ESKIRGAN."
    ],
    "cases": [
      {"account_id": "111222333", "peer_id": "7053823996",      "peer_hash": peer_hash("111222333", "7053823996")},
      {"account_id": "111222333", "peer_id": "562952781246744", "peer_hash": peer_hash("111222333", "562952781246744")},
      {"account_id": "111222333", "peer_id": "0",               "peer_hash": peer_hash("111222333", "0")},
      {"account_id": "444555666", "peer_id": "7053823996",      "peer_hash": peer_hash("444555666", "7053823996")}
    ]
  },

  "record_id": {
    "_note": [
      "spec §0.12 (2026-08-26).",
      "SHA256(kind || 0x00 || account_hash || 0x00 || peer_hash || 0x00 || msg_id_decimal || 0x00 || occurred_at_decimal)",
      "msg_id MANFIY bo'lishi mumkin (avatar -photo_id, story -story_id).",
      "Ishora SAQLANADI: -42 va 42 turli yozuvlar.",
      "Oxirgi ikkita holat: bir xil kind/peer_hash/msg_id/occurred_at, FAQAT account_hash farq qiladi -- ikkalasi turli record_id berishi SHART (akkaunt ajratmasi tekshiruvi, §0.12)."
    ],
    "cases": []
  },

  "aes_gcm": {
    "_note": [
      "AES-256-GCM. nonce 12 bayt, tag 16 bayt.",
      "ciphertext_hex ichida tag YO'Q — u alohida maydonda.",
      "Ba'zi kutubxonalar tag'ni ciphertext oxiriga qo'shadi — ajratib oling."
    ],
    "cases": []
  },

  "pbkdf2": {
    "_note": "PBKDF2-HMAC-SHA256. Oddiy o'ramlar 600000, email escrow 2000000.",
    "cases": []
  }
}

# record_id holatlari — manfiy va nol ham bor
acc_a = "111222333"
acc_b = "444555666"
ah_a = account_hash(acc_a)
ah_b = account_hash(acc_b)
ph = peer_hash(acc_a, "7053823996")
for kind, mid, occ in [
    ("deleted",    395278,          1787000000),
    ("edited",     390234,          1787000001),
    ("activity",   0,               1787000002),
    ("media_index", 597,            1787000003),
    ("media_index", -5190442718973336697, 1787000004),   # avatar: -photo_id
    ("media_index", -12345,         1787000005),          # story: -story_id
    ("tombstone",  0,               1787000006),
]:
    vec["record_id"]["cases"].append({
        "kind": kind, "account_hash": ah_a, "peer_hash": ph, "msg_id": mid,
        "occurred_at": occ, "record_id": record_id(kind, ah_a, ph, mid, occ)
    })

# Akkaunt ajratmasi tekshiruvi: bir xil kind/peer_hash/msg_id/occurred_at,
# faqat account_hash farq qiladi -- record_id ham farq qilishi SHART.
for ah in (ah_a, ah_b):
    vec["record_id"]["cases"].append({
        "kind": "deleted", "account_hash": ah, "peer_hash": ph,
        "msg_id": 999999, "occurred_at": 1787000100,
        "record_id": record_id("deleted", ah, ph, 999999, 1787000100)
    })

# AES-GCM holatlari
for name, nonce, pt in [
    ("bo'sh matn",   bytes(range(12)),        b""),
    ("oddiy JSON",   bytes(range(12,24)),     b'{"text":"salom"}'),
    ("UTF-8 va emoji", bytes(range(24,36)),   "O'chirildi 📌 —— test".encode("utf-8")),
]:
    aes = AESGCM(content_key)
    ct_with_tag = aes.encrypt(nonce, pt, None)
    ct, tag = ct_with_tag[:-16], ct_with_tag[-16:]
    vec["aes_gcm"]["cases"].append({
        "name": name,
        "key_hex": h(content_key),
        "nonce_hex": h(nonce),
        "plaintext_hex": h(pt),
        "ciphertext_hex": h(ct),
        "tag_hex": h(tag)
    })

# PBKDF2 holatlari
for pwd, salt, it in [
    ("parol123", bytes(range(16)), 600000),
    ("parol123", bytes(range(16)), 2000000),
    ("uzun parol-ibora bilan sinov", bytes(range(16,32)), 600000),
]:
    kek = hashlib.pbkdf2_hmac("sha256", pwd.encode(), salt, it, 32)
    vec["pbkdf2"]["cases"].append({
        "password": pwd, "salt_hex": h(salt),
        "iterations": it, "kek_hex": h(kek)
    })

out = sys.argv[1]
with open(out, "w", encoding="utf-8", newline="\n") as f:
    json.dump(vec, f, indent=2, ensure_ascii=False)
print("yozildi:", out)
print("  account_hash:", len(vec["account_hash"]["cases"]), "holat")
print("  peer_hash   :", len(vec["peer_hash"]["cases"]), "holat")
print("  record_id   :", len(vec["record_id"]["cases"]), "holat")
print("  aes_gcm     :", len(vec["aes_gcm"]["cases"]), "holat")
print("  pbkdf2      :", len(vec["pbkdf2"]["cases"]), "holat")
