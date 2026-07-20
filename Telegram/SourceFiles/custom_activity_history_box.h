#pragma once

#include <QtCore/QString>

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class BoxContent;
} // namespace Ui

namespace CustomActivityHistory {

// Ikkala UI joyidan (Peers tab "Tarixni ko'rish" tugmasi va Profil sahifasi
// "Faollik tarixi" tugmasi) chaqiriladigan umumiy "Tarix ko'ruvchi" Box.
// displayName — sarlavhada ko'rsatish uchun (masalan peer->name()).
[[nodiscard]] object_ptr<Ui::BoxContent> MakeHistoryBox(
	not_null<Main::Session*> session,
	const QString &peerId,
	const QString &displayName);

} // namespace CustomActivityHistory
