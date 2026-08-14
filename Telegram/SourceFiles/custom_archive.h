#pragma once

#include <QtCore/QString>

class HistoryItem;

namespace Main {
class Session;
} // namespace Main

namespace CustomArchive {

// Xabarni DOIMIY arxivga yozadi, agar shu peer kuzatilayotgan bo'lsa
// (CustomSettings::ShouldBackgroundCache). Idempotent — bir xil xabarni
// qayta berish xavfsiz (INSERT OR REPLACE).
//
// A13 kontekst: ilgari arxivga faqat real-vaqtda KELGAN xabarlar tushardi
// (data_session.cpp addNewMessage). Serverdan scrollback orqali yuklangan
// eski tarix va shu klientdan yuborilgan xabarlar arxivlanmasdi — natijada
// suhbatdosh butun chatni o'chirganda faqat oxirgi bir necha soatlik "dum"
// qutqarilardi. Endi barcha yo'llar shu funksiyadan o'tadi.
void MaybeArchiveItem(not_null<HistoryItem*> item);

// Partiya rejimi: BeginBatch/EndBatch orasidagi yozuvlar to'planadi va
// bitta tranzaksiyada saqlanadi. addOlderSlice/addNewerSlice scroll paytida
// ishlaydigan issiq yo'l bo'lgani uchun, har bir xabarga alohida SQLite
// tranzaksiyasi jank keltirib chiqarardi.
// Ichma-ich chaqirish xavfsiz (hisoblagich bilan).
void BeginBatch();
void EndBatch();

// A13/K1b: ishga tushishda arxivda o'chirilgan xabari bor chatlarni chat
// ro'yxatiga qaytaradi.
//
// Muammo: loadDeletedMessages() faqat History obyekti YUKLANGANDA ishlaydi,
// History esa chat ochilganda yaratiladi. Natijada suhbatdosh butun chatni
// o'chirgach, tiklangan tarix faqat foydalanuvchi o'sha chatni qidiruvdan
// topib ochgandagina paydo bo'lardi — asosiy ro'yxatda ko'rinmasdi.
void RestoreDeletedChats(not_null<Main::Session*> session);

// L3 (2026-08-14): xabar o'chirilganda, agar media hali arxivda bo'lmasa —
// yuklashga urinib ko'radi. Bu OXIRGI IMKONIYAT, kafolat emas: xabar
// o'chirilgach `file_reference` tez orada yaroqsiz bo'ladi va server
// so'rovni rad etishi mumkin. Urinish tekin, shuning uchun arziydi.
// history_item.cpp dagi setDeletedLocally() dan chaqiriladi — faqat
// fayl lokal keshda TOPILMAGAN holatda (topilgan bo'lsa u yerda
// nusxalanadi).
void TryRescueMedia(not_null<HistoryItem*> item);

// Arxivga yuklash tugagach data_document.cpp dagi finishLoad() shu
// funksiyani chaqiradi va media_index yozuvi 'pending' dan 'present' ga
// o'tadi. L2/L3 faqat yuklashni boshlaydi, natijani bilmaydi — shuning
// uchun holat aynan shu yerda tasdiqlanadi.
void NoteArchivedDownloadFinished(
	const QString &peerId,
	long long msgId,
	long long size);

// Davriy WAL checkpoint taymerini ishga tushiradi (A13/D5) — to'satdan tok
// o'chganda yo'qotish oynasini qisqartirish uchun. Ilova ishga tushganda
// bir marta chaqiriladi.
void StartMaintenance();

} // namespace CustomArchive
