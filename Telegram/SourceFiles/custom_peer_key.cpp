#include "custom_peer_key.h"

#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"

namespace CustomDB {

PeerKey Key(not_null<PeerData*> peer) {
	return Key(peer->session(), peer->id);
}

PeerKey Key(not_null<HistoryItem*> item) {
	return Key(item->history()->session(), item->history()->peer->id);
}

PeerKey Key(Main::Session &session, PeerId peerId) {
	return PeerKey{
		.accountId = qint64(session.userId().bare),
		.peerId = QString::number(peerId.value),
	};
}

} // namespace CustomDB
