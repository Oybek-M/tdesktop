#pragma once

#include "custom_sync_outbox.h"

#include <QtCore/QByteArray>

namespace CustomSync {

enum class BuildStatus {
    Ok,          // payload muvaffaqiyatli qurildi
    SourceGone,  // manba qator lokal bazada yo'q -- hech qachon qurilmaydi, outboxdan o'chirilishi kerak
    Unsupported, // kind hali qo'llab-quvvatlanmaydi -- outboxda qoladi
};

struct BuildResult {
    BuildStatus status = BuildStatus::Unsupported;
    QByteArray json;
};

// Outbox yozuvi uchun shifrlanadigan JSON payload'ni hosil qiladi.
// Har bir payload §0.14 bo'yicha "account_id" va "peer_id" (o'nlik satrlar)
// bilan boshlanadi.
[[nodiscard]] BuildResult Build(const OutboxEntry &entry);

} // namespace CustomSync
