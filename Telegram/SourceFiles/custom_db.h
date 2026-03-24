#pragma once

#include <QtCore/QString>
#include "base/basic_types.h"

class HistoryItem;

namespace CustomDB {

void Init();
void SaveMessage(not_null<HistoryItem*> item);
void MarkDeleted(qint64 msgId, const QString &peerId, const QString &localMediaPath = QString());

// HISTORY RETRIEVAL
QString GetMessageHistory(qint64 msgId, const QString &peerId);

// GHOST MODE READ HISTORY
void SaveGhostRead(const QString &peerId, qint64 msgId);
qint64 GetGhostRead(const QString &peerId);

} // namespace CustomDB
