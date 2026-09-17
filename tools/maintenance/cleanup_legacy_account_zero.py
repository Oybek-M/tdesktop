#!/usr/bin/env python3
"""
cleanup_legacy_account_zero.py — Legacy `account_id = 0` yozuvlarini xavfsiz tozalash skripti.

Qoidalar va talablar (A6 B2):
1. Jonli bazaga yozishdan oldin ilova (Telegram.exe) yopiqligini tekshiradi.
2. Ishga tushishdan oldin zaxira nusxa (backup) oladi.
3. Barcha o'zgarishlar uchun JSON formatida `undo` log yozadi (orqaga qaytarish imkoni).
4. FAQAT aniq egasi topilgan (barcha jadvallarda faqat bitta non-zero account_id mavjud bo'lgan)
   peer'lar qatorlarini yangilaydi. Konfliktli (bir nechta akkaunt) yoki dalili yo'q (0 ta akkaunt)
   peer'larga QAT'IYAN TEGMAYDI.
5. Har bir jadval bo'yicha oldin/keyin sanoqlarini ko'rsatadi.

Qo'llash:
  # 1. Sinov (hech narsa o'zgarmaydi):
  python cleanup_legacy_account_zero.py --db-path "C:/path/to/copy.db" --dry-run

  # 2. Nusxada qo'llash:
  python cleanup_legacy_account_zero.py --db-path "C:/path/to/copy.db" --apply

  # 3. O'zgarishlarni orqaga qaytarish:
  python cleanup_legacy_account_zero.py --db-path "C:/path/to/copy.db" --undo "undo_log_xxx.json"
"""

import argparse
import json
import os
import shutil
import sqlite3
import subprocess
import sys
from datetime import datetime

DEFAULT_LIVE_DB = os.path.normpath(r"C:\Users\Oybek\Pictures\customizationMainFolder\db\actioned_messages.db")

def is_telegram_running():
    """Windows tizimida Telegram.exe ishlayotganini tekshiradi."""
    try:
        output = subprocess.check_output(
            ["tasklist", "/FI", "IMAGENAME eq Telegram.exe", "/NH"],
            text=True, stderr=subprocess.DEVNULL
        )
        return "Telegram.exe" in output
    except Exception:
        return False

def make_backup(db_path):
    """Baza fayli va uning WAL/SHM fayllaridan zaxira nusxa oladi."""
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    backup_path = f"{db_path}.pre-legacy-cleanup-{timestamp}.bak"
    print(f"[*] Zaxira nusxa olinmoqda: {backup_path}")
    shutil.copy2(db_path, backup_path)
    for ext in ["-wal", "-shm"]:
        sidecar = db_path + ext
        if os.path.exists(sidecar):
            shutil.copy2(sidecar, backup_path + ext)
    return backup_path

def find_peer_attributions(conn):
    """
    Barcha jadvallardagi (actioned_messages, text_cache, activity_history, media_index)
    ma'lumotlarni tahlil qilib, har bir peer uchun aniq egasini aniqlaydi.
    
    Qaytaradi:
      attributions: {peer_id: account_id} (faqat 1 ta aniq egasi bo'lganlar)
      conflicts:    {peer_id: [account_ids]} (bir nechta egasi bo'lganlar)
      unknowns:     set(peer_id) (hech qanday egasi topilmaganlar)
    """
    cur = conn.cursor()
    
    # 1. account_id = 0 bo'lgan barcha peer_id larni to'playmiz
    legacy_peers = set()
    tables_to_check = ['actioned_messages', 'text_cache', 'activity_history', 'media_index']
    for tbl in tables_to_check:
        cur.execute(f"SELECT DISTINCT peer_id FROM {tbl} WHERE account_id = 0")
        for row in cur.fetchall():
            if row[0]:
                legacy_peers.add(str(row[0]))

    # 2. Har bir peer bo'yicha barcha jadvallardan non-zero account_id larni qidiramiz
    attributions = {}
    conflicts = {}
    unknowns = set()

    for peer in legacy_peers:
        accounts_found = set()
        for tbl in tables_to_check:
            cur.execute(f"SELECT DISTINCT account_id FROM {tbl} WHERE peer_id = ? AND account_id != 0", (peer,))
            for row in cur.fetchall():
                if row[0] and row[0] != 0:
                    accounts_found.add(row[0])
        
        if len(accounts_found) == 1:
            attributions[peer] = list(accounts_found)[0]
        elif len(accounts_found) > 1:
            conflicts[peer] = sorted(list(accounts_found))
        else:
            unknowns.add(peer)

    return attributions, conflicts, unknowns

def count_zeros(conn):
    """Jadvallar kesimida account_id = 0 qatorlar sonini hisoblaydi."""
    cur = conn.cursor()
    counts = {}
    tables = ['actioned_messages', 'text_cache', 'activity_history', 'media_index']
    for tbl in tables:
        cur.execute(f"SELECT COUNT(*) FROM {tbl} WHERE account_id = 0")
        cnt0 = cur.fetchone()[0]
        cur.execute(f"SELECT COUNT(*) FROM {tbl}")
        cnt_tot = cur.fetchone()[0]
        counts[tbl] = (cnt0, cnt_tot)
    return counts

def apply_cleanup(conn, attributions, dry_run=False):
    """
    Aniq egasi topilgan peerlar uchun qatorlarni yangilaydi va undo log yozadi.
    """
    cur = conn.cursor()
    undo_records = []
    tables = ['actioned_messages', 'text_cache', 'activity_history', 'media_index']
    
    updated_counts = {t: 0 for t in tables}

    if not dry_run:
        cur.execute("BEGIN TRANSACTION")

    for peer_id, account_id in attributions.items():
        # actioned_messages (has 'id' column)
        cur.execute("SELECT id FROM actioned_messages WHERE peer_id = ? AND account_id = 0", (peer_id,))
        am_ids = [r[0] for r in cur.fetchall()]
        if am_ids:
            updated_counts['actioned_messages'] += len(am_ids)
            for row_id in am_ids:
                undo_records.append({
                    "table": "actioned_messages",
                    "pk_col": "id",
                    "pk_val": row_id,
                    "old_acc": 0,
                    "new_acc": account_id
                })
            if not dry_run:
                cur.execute("UPDATE actioned_messages SET account_id = ? WHERE peer_id = ? AND account_id = 0",
                            (account_id, peer_id))

        # activity_history (has 'id' column)
        cur.execute("SELECT id FROM activity_history WHERE peer_id = ? AND account_id = 0", (peer_id,))
        ah_ids = [r[0] for r in cur.fetchall()]
        if ah_ids:
            updated_counts['activity_history'] += len(ah_ids)
            for row_id in ah_ids:
                undo_records.append({
                    "table": "activity_history",
                    "pk_col": "id",
                    "pk_val": row_id,
                    "old_acc": 0,
                    "new_acc": account_id
                })
            if not dry_run:
                cur.execute("UPDATE activity_history SET account_id = ? WHERE peer_id = ? AND account_id = 0",
                            (account_id, peer_id))

        # text_cache (composite key: account_id, peer_id, msg_id)
        cur.execute("SELECT msg_id FROM text_cache WHERE peer_id = ? AND account_id = 0", (peer_id,))
        tc_msg_ids = [r[0] for r in cur.fetchall()]
        if tc_msg_ids:
            updated_counts['text_cache'] += len(tc_msg_ids)
            for m_id in tc_msg_ids:
                undo_records.append({
                    "table": "text_cache",
                    "peer_id": peer_id,
                    "msg_id": m_id,
                    "old_acc": 0,
                    "new_acc": account_id
                })
            if not dry_run:
                cur.execute("""
                    UPDATE OR REPLACE text_cache
                    SET account_id = ?
                    WHERE peer_id = ? AND account_id = 0
                """, (account_id, peer_id))

        # media_index (composite key: account_id, peer_id, msg_id)
        cur.execute("SELECT msg_id FROM media_index WHERE peer_id = ? AND account_id = 0", (peer_id,))
        mi_msg_ids = [r[0] for r in cur.fetchall()]
        if mi_msg_ids:
            updated_counts['media_index'] += len(mi_msg_ids)
            for m_id in mi_msg_ids:
                undo_records.append({
                    "table": "media_index",
                    "peer_id": peer_id,
                    "msg_id": m_id,
                    "old_acc": 0,
                    "new_acc": account_id
                })
            if not dry_run:
                cur.execute("""
                    UPDATE OR REPLACE media_index
                    SET account_id = ?
                    WHERE peer_id = ? AND account_id = 0
                """, (account_id, peer_id))

    if not dry_run:
        conn.commit()

    return updated_counts, undo_records

def revert_undo(conn, undo_file_path):
    """Undo log faylidan foydalanib o'zgarishlarni orqaga qaytaradi."""
    with open(undo_file_path, "r", encoding="utf-8") as f:
        records = json.load(f)

    cur = conn.cursor()
    cur.execute("BEGIN TRANSACTION")
    reverted_count = 0

    for rec in records:
        tbl = rec["table"]
        if "pk_col" in rec:
            cur.execute(f"UPDATE {tbl} SET account_id = ? WHERE {rec['pk_col']} = ?",
                        (rec["old_acc"], rec["pk_val"]))
        else:
            cur.execute(f"UPDATE OR REPLACE {tbl} SET account_id = ? WHERE peer_id = ? AND msg_id = ? AND account_id = ?",
                        (rec["old_acc"], rec["peer_id"], rec["msg_id"], rec["new_acc"]))
        reverted_count += 1

    conn.commit()
    print(f"[+] Undo muvaffaqiyatli bajarildi: {reverted_count} ta yozuv orqaga qaytarildi.")

def main():
    parser = argparse.ArgumentParser(description="Legacy account_id=0 tozalash skripti")
    parser.add_argument("--db-path", default=DEFAULT_LIVE_DB, help="Baza fayli yo'li")
    parser.add_argument("--dry-run", action="store_true", help="Faqat hisoblash, o'zgarish kiritmaslik")
    parser.add_argument("--apply", action="store_true", help="O'zgarishlarni bazaga qo'llash")
    parser.add_argument("--undo", help="Undo log fayli orqali orqaga qaytarish")
    args = parser.parse_args()

    db_path = os.path.normpath(os.path.abspath(args.db_path))
    print("=" * 60)
    print("  Legacy account_id = 0 Tozalash Skripti (CustomMod DB)")
    print("=" * 60)
    print(f"Baza yo'li: {db_path}")

    if not os.path.exists(db_path):
        print(f"[!] Xato: Baza fayli topilmadi: {db_path}")
        sys.exit(1)

    is_live = (db_path.lower() == DEFAULT_LIVE_DB.lower())

    # 1. Telegram ishlayotganini tekshirish
    if args.apply and not args.dry_run:
        if is_telegram_running():
            if is_live:
                print("[!] XATO: Telegram.exe hozirda ishlab turibdi!")
                print("    Jonli bazaga yozishdan oldin Telegram'ni butunlay yoping.")
                sys.exit(1)
            else:
                print("[!] OGOHLANTIRISH: Telegram.exe ishlab turibdi, ammo ko'rsatilgan")
                print("    baza jonli baza emas, alohida nusxa ekani tasdiqlandi.")

    conn = sqlite3.connect(db_path)

    # 2. Agar undo rejimi bo'lsa
    if args.undo:
        if not os.path.exists(args.undo):
            print(f"[!] Xato: Undo fayli topilmadi: {args.undo}")
            sys.exit(1)
        revert_undo(conn, args.undo)
        conn.close()
        return

    # 3. Oldingi holatni hisoblash
    before_counts = count_zeros(conn)

    print("\n--- Boshlang'ich holat (account_id = 0 qatorlar) ---")
    for tbl, (z, tot) in before_counts.items():
        pct = (z / tot * 100) if tot > 0 else 0
        print(f"  {tbl:<20}: {z:>6} / {tot:>6} ({pct:>5.1f}%)")

    # 4. Peer egalik tahlili
    print("\n[*] Peer'lar bo'yicha egalik tahlil qilinmoqda...")
    attributions, conflicts, unknowns = find_peer_attributions(conn)
    total_peers = len(attributions) + len(conflicts) + len(unknowns)

    print(f"  Jami legacy peer'lar     : {total_peers}")
    print(f"  -> Aniq egasi topildi    : {len(attributions)} peer (yangilanadi)")
    print(f"  -> Bir nechta akkaunt    : {len(conflicts)} peer (KONFLIKT - tegilmaydi)")
    print(f"  -> Hech qanday dalil yo'q: {len(unknowns)} peer (NO EVIDENCE - tegilmaydi)")

    if not args.apply and not args.dry_run:
        print("\n[!] DIQQAT: Hech qanday amal tanlanmadi. '--dry-run' yoki '--apply' parametrini bering.")
        conn.close()
        return

    # 5. Agar apply bo'lsa va dry_run bo'lmasa, zaxira nusxa olamiz
    if args.apply and not args.dry_run:
        make_backup(db_path)

    # 6. O'zgarishlarni qo'llash yoki simulyatsiya qilish
    is_dry = args.dry_run or not args.apply
    mode_str = "SIMULYATSIYA (DRY-RUN)" if is_dry else "HAQIQIY QO'LLASH (APPLY)"
    print(f"\n[*] Rejim: {mode_str}")

    updated_counts, undo_records = apply_cleanup(conn, attributions, dry_run=is_dry)

    # 7. Undo log yozish
    if not is_dry and undo_records:
        ts = datetime.now().strftime("%Y%m%d-%H%M%S")
        undo_filename = f"cleanup_legacy_undo_{ts}.json"
        undo_path = os.path.join(os.path.dirname(db_path), undo_filename)
        with open(undo_path, "w", encoding="utf-8") as f:
            json.dump(undo_records, f, indent=2)
        print(f"[+] Undo log yozildi: {undo_path} ({len(undo_records)} ta yozuv)")

    # 8. Yakuniy holat
    after_counts = count_zeros(conn) if not is_dry else {
        tbl: (before_counts[tbl][0] - updated_counts[tbl], before_counts[tbl][1])
        for tbl in before_counts
    }

    print("\n--- Yakuniy natija ---")
    print(f"{'Jadval':<20} | {'Oldin (acc=0)':<13} | {'Yangilandi':<10} | {'Qoldi (acc=0)':<13}")
    print("-" * 65)
    for tbl in before_counts:
        b_z = before_counts[tbl][0]
        upd = updated_counts[tbl]
        a_z = after_counts[tbl][0]
        print(f"{tbl:<20} | {b_z:>13} | {upd:>10} | {a_z:>13}")

    print("\n" + "=" * 60)
    print(f"Jami yangilangan qatorlar: {sum(updated_counts.values())}")
    print("=" * 60)

    conn.close()

if __name__ == "__main__":
    main()
