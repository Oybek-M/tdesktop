#include "custom_media_quota.h"

#include "custom_db.h"
#include "custom_settings.h"
#include "core/application.h"
#include "crl/crl.h"
#include "ui/boxes/confirm_box.h"
#include "window/window_controller.h"

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

void ShowQuotaAlertIfNeeded() {
    if (!IsFull()) {
        return;
    }
    const auto window = Core::App().activeWindow();
    if (!window) {
        return; // oyna yo'q — keyingi ishga tushishda takrorlanadi
    }
    const auto toGb = [](long long bytes) {
        return QString::number(double(bytes) / (1024.0 * 1024 * 1024), 'f', 1);
    };
    window->show(Ui::MakeConfirmBox({
        .text = u"Media arxivi kvotasi to'lgan.\n\nIshlatilgan: "_q
            + toGb(UsedBytes())
            + u" GB / "_q
            + toGb(LimitBytes())
            + u" GB\n\nKatta fayllarni oldindan yuklab olish TO'XTATILDI. "
              "Siz o'zingiz ochgan media baribir saqlanaveradi.\n\n"
              "Hech qanday fayl o'chirilmadi. Kvotani Custom Window "
              "orqali kengaytirishingiz yoki arxiv papkasini qo'lda "
              "tozalashingiz mumkin."_q,
        .inform = true,
    }));
}

} // namespace CustomMediaQuota
