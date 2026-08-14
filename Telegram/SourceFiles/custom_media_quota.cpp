#include "custom_media_quota.h"

#include "custom_db.h"
#include "custom_settings.h"
#include "crl/crl.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>

#include <atomic>

namespace CustomMediaQuota {
namespace {

// Fon skaneri UI oqimidan mustaqil yozadi — atomik.
std::atomic<long long> gUsedBytes{ 0 };
bool gInitialized = false;

[[nodiscard]] long long ScanFolderBytes(const QString &dir) {
    long long total = 0;
    QDirIterator it(
        dir,
        QDir::Files | QDir::NoSymLinks,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

} // namespace

QString ArchiveRoot() {
    return QDir::homePath() + "/customizationMainFolder";
}

void Init() {
    if (gInitialized) {
        return;
    }
    gInitialized = true;

    // 1) Darhol: indeksdagi yig'indi. Bu bitta SQL so'rov, ya'ni tekin.
    gUsedBytes.store(CustomDB::TotalArchivedMediaBytes());

    // 2) Fonda: haqiqiy papka hajmi. Indeks v7 da paydo bo'lgani uchun
    //    undan oldin arxivlangan fayllar indeksda yo'q — ular faqat
    //    skanerdan chiqadi. Skaner UI oqimida qilinsa ishga tushish
    //    sekinlashardi (o'n minglab fayl), shuning uchun crl::async.
    const auto mediaDir = ArchiveRoot() + "/medias";
    crl::async([mediaDir] {
        if (!QDir(mediaDir).exists()) {
            return;
        }
        const auto scanned = ScanFolderBytes(mediaDir);
        // Skaner davomida AddBytes() ishlagan bo'lishi mumkin, shuning
        // uchun kattarog'ini olamiz — kam baholashdan ko'ra ko'p baholash
        // xavfsizroq (kvota to'lganini o'tkazib yuborishdan ko'ra).
        auto current = gUsedBytes.load();
        while (scanned > current
            && !gUsedBytes.compare_exchange_weak(current, scanned)) {
            // compare_exchange_weak `current` ni yangilaydi — qayta urinamiz.
        }
    });
}

long long UsedBytes() {
    return gUsedBytes.load();
}

long long LimitBytes() {
    // Har chaqiruvda sozlamadan o'qiymiz — foydalanuvchi kvotani
    // Custom Window'da o'zgartirsa, qayta ishga tushirmasdan amal qilsin.
    return static_cast<long long>(CustomSettings::MediaBackupQuotaGb())
        * 1024 * 1024 * 1024;
}

bool IsFull() {
    return UsedBytes() >= LimitBytes();
}

void AddBytes(long long bytes) {
    if (bytes > 0) {
        gUsedBytes.fetch_add(bytes);
    }
}

} // namespace CustomMediaQuota
