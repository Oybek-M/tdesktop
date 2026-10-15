#pragma once

#include "core/update_verify.h"

// A7 (2026-10-13): CustomMod relizlarining kanali va tartib raqami.
//
// Asosiy versiya (AppVersion, core/version.h) DOIM upstream bilan bir xil
// qoladi: custom_upstream_checker.cpp rasmiy relizlarni aynan shu raqam
// bilan solishtiradi. O'z relizlarimiz V2 "canary-public" kanalida
// "7.3.1 canary #N" bo'lib chiqadi -- eski "alpha N" ning V2 dagi o'rni
// (7.3.x da alpha V2 imzosi bilan ishlamaydi: Packer -channel ni -alpha
// bilan qabul qilmaydi, launcher esa alpha'da ma'lumotni
// TelegramForcePortable'ga ko'chiradi).
//
// Kanal va hisoblagich CMake'dagi TDESKTOP_UPDATE_CHANNEL /
// TDESKTOP_CANARY_COUNTER orqali EMAS, shu yerda beriladi: ular Telegram
// target'ining compile definition'lari, bittasini o'zgartirish butun
// loyihani qayta kompilyatsiya qiladi. Bu faylni faqat core/update_channel.h
// o'qiydi, u esa 9 ta .cpp ga kiradi.
//
// Har relizda kCounter ni 1 ga oshiring. Upstream merge'dan keyin
// (AppVersion o'zgarganda) 1 dan qayta boshlang -- versiya
// (asosiy << 32 | hisoblagich) bo'lgani uchun yangi asos har doim katta.
// Batafsil: docs/self-update/a7-canary-releases.md

namespace CustomRelease {

inline constexpr auto kUpdateChannel = Core::Updates::Channel::CanaryPublic;
inline constexpr auto kCounter = 3;

} // namespace CustomRelease
