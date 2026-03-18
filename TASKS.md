# Custom Telegram Mod (Saidjon Edition) - Task List

## Phase 1: Ghost Mode & Database Persistence [IN PROGRESS]
- [x] Create custom SQLite database for offline caching.
- [x] Basic Ghost Mode (block read receipts, typing, and online status).
- [x] Anti-Delete UI (Aka Messenger style labels).
- [x] Anti-Edit UI (Full version history in chat bubbles).
- [ ] **[FIX NEEDED]** Local Ghost Read persistence:
    - [ ] Intercept server sync updates to override unread status.
    - [ ] Ensure local read status survives app restart for all cases.
- [x] Voice/Video message Ghost Mode (block content read receipts).

## Phase 2: True Offline Media [PLANNED]
- [ ] **Auto-Download Media:** Automatically download incoming photos/videos/voice messages in the background.
- [x] **Permanent Backup:** Copy all downloaded media to `Downloads/Telegram_AntiDelete/` immediately upon arrival or deletion.
- [x] **Media Database:** Store file paths in SQLite to link them with messages even if Telegram cache is cleared.

## Phase 3: System Resilience & Recovery [PLANNED]
- [ ] **DB Export/Import:** Tools to move the `.sqlite` and media folder between OS reinstalls.
- [ ] **Full Restoration:** Ensure that placing the old `.sqlite` and `Telegram_AntiDelete` folder back allows the mod to fully restore deleted/edited history.
- [ ] **Session Protection:** Keep the mod working even after a full system wipe (by restoring the custom DB).

## Phase 4: UI Enhancements [OPTIONAL]
- [ ] Color-coding for Deleted/Edited messages (e.g., Red/Grey background).
- [ ] Custom Icons for Ghost Mode status in the top bar.
