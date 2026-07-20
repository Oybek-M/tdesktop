#pragma once

#include <QtCore/QString>

namespace Main {
class Session;
} // namespace Main

namespace Data {
class LastseenStatus;
} // namespace Data

namespace CustomActivityHistory {

// Session yaratilganda BIR MARTA chaqiriladi (main_session.cpp konstruktori
// ichida). Ism/username/rasm/last-seen o'zgarishlarini
// session().changes().peerUpdates(...) orqali kuzatadi va
// CustomSettings::ShouldTrackActivity() true bo'lgan User peerlar uchun
// CustomDB::activity_history jadvaliga yozadi. Obuna session bilan bir xil
// umr ko'radi (session->lifetime() ga bog'langan).
void Init(not_null<Main::Session*> session);

// Xom Data::LastseenStatus qiymatini activity_history uchun ixcham matn
// kodiga aylantiradi: "online:<unixts>" / "offline:<unixts>" /
// "recently" / "within_week" / "within_month" / "long_ago" / "empty".
[[nodiscard]] QString EncodeStatus(const Data::LastseenStatus &status, int32 now);

// EncodeStatus() natijasini inson o'qiy oladigan matnga aylantiradi.
// History Viewer Box (custom_activity_history_box.cpp, later task) shu
// funksiyani ishlatadi.
[[nodiscard]] QString DecodeStatusLabel(const QString &encoded);

} // namespace CustomActivityHistory
