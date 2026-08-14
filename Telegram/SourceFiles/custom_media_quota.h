#pragma once

#include <QtCore/QString>

// Katta media arxivi uchun disk kvotasi (2026-08-14).
//
// Nima uchun kerak: L2 qatlami (oldindan yuklash) kuzatilayotgan
// chatlardagi video/fayllarni avtomatik yuklaydi. Cheklovsiz bu diskni
// to'ldirib qo'yadi — 2026-08-14 da build paytida disk 11.9 GB gacha
// tushib, link bosqichi yiqilgan edi (LNK1180). Kvota shu takrorlanishning
// oldini oladi.
//
// MUHIM: kvota FAQAT oldindan yuklashni (L2/L3) to'xtatadi. Foydalanuvchi
// o'zi ochgan media (L1) baribir arxivlanadi — u faylni ataylab ochgan,
// uni yo'qotish noto'g'ri bo'lardi. Demak hajm kvotadan biroz oshib
// ketishi mumkin; foydalanuvchi bu haqda ishga tushishda ogohlantiriladi.
//
// Hech qanday fayl HECH QACHON avtomatik o'chirilmaydi (foydalanuvchi
// talabi).
namespace CustomMediaQuota {

// Ishga tushishda bir marta. Boshlang'ich qiymatni media indeksidan
// darhol oladi, so'ng fonda papkani skanerlab aniqlashtiradi (indeks
// v7 dan oldin yaratilgan eski fayllar hisobga olinishi uchun).
void Init();

[[nodiscard]] long long UsedBytes();
[[nodiscard]] long long LimitBytes();   // CustomSettings dan, har chaqiruvda
[[nodiscard]] bool IsFull();

// Arxivga yangi fayl qo'shilgach chaqiriladi — papkani qayta
// skanerlamaslik uchun.
void AddBytes(long long bytes);

// ~/customizationMainFolder — arxiv ildizi. Bir nechta modul shu yo'lni
// hisoblaydi, shuning uchun yagona joyda.
[[nodiscard]] QString ArchiveRoot();

// Kvota to'lgan bo'lsa ogohlantirish oynasini ko'rsatadi.
//
// Toast EMAS, balki tasdiqlash talab qiladigan box — foydalanuvchi
// talabi. Muammo hal bo'lmaguncha (hajm kvotadan pastga tushmaguncha
// yoki kvota kengaytirilmaguncha) HAR ISHGA TUSHISHDA takrorlanadi.
// Hech qanday fayl avtomatik o'chirilmaydi.
//
// Chat ro'yxati yuklangach chaqiriladi — sessiya konstruktorida hali
// oyna mavjud emas.
void ShowQuotaAlertIfNeeded();

} // namespace CustomMediaQuota
