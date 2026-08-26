#pragma once

#include "custom_db.h"

class HistoryItem;
class PeerData;
namespace Main {
class Session;
} // namespace Main

namespace CustomDB {

[[nodiscard]] PeerKey Key(not_null<PeerData*> peer);
[[nodiscard]] PeerKey Key(not_null<HistoryItem*> item);
[[nodiscard]] PeerKey Key(Main::Session &session, PeerId peerId);

} // namespace CustomDB
