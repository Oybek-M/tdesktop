#pragma once

#include "base/basic_types.h"
#include "crl/crl_time.h"

// Start tinchligi signali (A26, 2026-10-13).
//
// Nima uchun kerak: startdan keyingi texnik xizmat ishlari (media indeksini
// moslash, faollik tarixini siqish + keshni yuklash, kvota skaneri, birinchi
// sync sikli) ilgari QAT'IY 90 soniya kutardi -- start paytidagi bitta SQLite
// ulanishi uchun raqobatning oldini olish uchun (2026-09-12). Lekin o'lchov
// (7.3.1, iliq start) start og'ir qismi ~10 soniyada tugashini ko'rsatdi,
// qolgan ~80 soniya esa foydalanuvchi uchun "CustomMod ishlamayapti" degani
// edi: masalan faollik tarixi keshi yuklanmaguncha hech narsa yozilmaydi.
//
// Endi kutish asosiy oqim holatiga qarab: ish asosiy oqim ketma-ket bir
// necha soniya tinch bo'lganda boshlanadi (application.cpp dagi qotish
// kuzatuvchisining tiklari), lekin startdan kamida 20 soniya o'tgach. 90
// soniya faqat yuqori chegara bo'lib qoldi -- UI hech tinchimasa ham ish
// eski vaqtdan kechikmaydi.
//
// Barcha funksiyalar FAQAT asosiy oqimdan chaqiriladi.
namespace CustomStartupQuiet {

// Qotish kuzatuvchisi ishga tushganda (jarayon boshida bir marta).
void Start();

// Kuzatuvchining har tiki: `late` -- taymer qancha kechikdi (ms).
void NoteTick(crl::time late);

// `callback` start tinchiganda bir marta chaqiriladi. Tinchlik RO'YXATDAN
// O'TGANDAN KEYIN o'lchanadi: kech chaqirilsa ham (masalan, tarmoq
// kechikib, chat ro'yxati 5 daqiqadan keyin yuklansa) o'sha yuklanish
// tugashini kutadi. `name` -- log uchun.
void OnCalm(const char *name, Fn<void()> callback);

} // namespace CustomStartupQuiet
