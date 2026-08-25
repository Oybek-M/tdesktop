# Track C uchun platformalararo test vektorlarini generatsiya qiladi.
# Natija: docs/sync-protocol/test-vectors.json
import hashlib, hmac, json, sys
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

def h(b): return b.hex()

# ---- record_id ----
def record_id(kind, peer_hash, msg_id, occurred_at):
    m = hashlib.sha256()
    m.update(kind.encode()); m.update(b"\x00")
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

def peer_hash(peer_id):
    return hmac.new(peer_key, peer_id.encode(), hashlib.sha256).digest()[:16].hex()

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
      "customsync-peer-v1":    h(peer_key)
    }
  },

  "peer_hash": {
    "_note": "HMAC-SHA256(peer_key, peer_id)[0..16] -> hex. peer_id — O'NLIK SATR.",
    "cases": [
      {"peer_id": "7053823996",      "peer_hash": peer_hash("7053823996")},
      {"peer_id": "562952781246744", "peer_hash": peer_hash("562952781246744")},
      {"peer_id": "0",               "peer_hash": peer_hash("0")}
    ]
  },

  "record_id": {
    "_note": [
      "SHA256(kind || 0x00 || peer_hash || 0x00 || msg_id_decimal || 0x00 || occurred_at_decimal)",
      "msg_id MANFIY bo'lishi mumkin (avatar -photo_id, story -story_id).",
      "Ishora SAQLANADI: -42 va 42 turli yozuvlar."
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
ph = peer_hash("7053823996")
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
        "kind": kind, "peer_hash": ph, "msg_id": mid,
        "occurred_at": occ, "record_id": record_id(kind, ph, mid, occ)
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
print("  record_id:", len(vec["record_id"]["cases"]), "holat")
print("  aes_gcm  :", len(vec["aes_gcm"]["cases"]), "holat")
print("  pbkdf2   :", len(vec["pbkdf2"]["cases"]), "holat")
