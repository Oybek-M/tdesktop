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

// Hisoblagichni diskdagi va indeksdagi HOZIRGI holatdan qayta o'rnatadi.
//
// Nega kerak (2026-09-26): Init() va AddBytes() hisoblagichni faqat
// OSHIRADI, kamaytiradigan yo'l yo'q edi. Fayl seans o'rtasida yo'qolsa
// (foydalanuvchi o'chirsa), raqam qayta ishga tushirilmaguncha yuqoriligicha
// qolardi. Amalda: 5.24 GB lik ikki fayl o'chirilgandan keyin ham kvota
// 10.4 GB deb ko'rsatib, oldindan yuklashni keraksiz to'xtatib turdi.
//
// ReconcileMediaIndex() dan KEYIN chaqirilishi shart: u 'present' bo'lgan,
// lekin diskda yo'q yozuvlarni 'missing' ga tushiradi, ya'ni SQL yig'indisi
// aynan shundan keyin to'g'ri bo'ladi. Papka ham qayta skanerlanadi, chunki
// startdagi skaner reconcile'dan OLDIN ishlaydi va u ham eskirgan bo'lishi
// mumkin (12:01:55 skaner, 12:01:59 reconcile).
//
// Fon oqimidan (Maintenance navbatidan) chaqirilsin — papkani yurib chiqadi.
void ResyncFromDisk();

// Kvota to'lgan bo'lsa ogohlantirish oynasini ko'rsatadi.
//
// Toast EMAS, balki tasdiqlash talab qiladigan box — foydalanuvchi
// talabi. Muammo hal bo'lmaguncha (hajm kvotadan pastga tushmaguncha
// yoki kvota kengaytirilmaguncha) HAR ISHGA TUSHISHDA takrorlanadi.
// Hech qanday fayl avtomatik o'chirilmaydi.
//
// 2026-09-26: ResyncFromDisk() dan KEYIN, Maintenance navbatidan
// chaqiriladi (main_session.cpp). Ilgari Session konstruktorida edi va
// shu sababli eskirgan raqamni ko'rsatardi. Fon oqimidan chaqirilgani
// uchun chaqiruvchi uni crl::on_main ga o'rashi SHART — box faqat asosiy
// oqimda ko'rsatiladi.
void ShowQuotaAlertIfNeeded();

} // namespace CustomMediaQuota
