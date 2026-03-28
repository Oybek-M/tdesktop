#include "custom_mod_settings.h"
#include "custom_settings.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtCore/QSettings>
#include <QtGui/QPalette>

CustomModSettings::CustomModSettings(QWidget *parent) : QWidget(parent) {
    setWindowTitle("CustomMod Settings");
    setFixedSize(400, 500);
    
    // Telegram Desktop style dark theme background
    setStyleSheet(
        "QWidget { background-color: #17212b; color: #ffffff; font-family: 'Segoe UI', sans-serif; }"
        "QCheckBox { spacing: 10px; font-size: 14px; padding: 10px; border-radius: 5px; }"
        "QCheckBox:hover { background-color: #242f3d; }"
        "QCheckBox::indicator { width: 18px; height: 18px; }"
        "QLabel#Title { font-size: 20px; font-weight: bold; color: #64b5f6; margin-bottom: 20px; }"
        "QLabel#Section { font-size: 12px; font-weight: bold; color: #50a7ea; margin-top: 15px; margin-left: 5px; text-transform: uppercase; }"
        "QPushButton { background-color: #2b5278; border: none; padding: 10px; border-radius: 5px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #36638e; }"
    );

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(5);

    auto title = new QLabel("CustomMod Features", this);
    title->setObjectName("Title");
    layout->addWidget(title);

    // --- PRIVACY SECTION ---
    auto privacyLabel = new QLabel("Privacy & Security", this);
    privacyLabel->setObjectName("Section");
    layout->addWidget(privacyLabel);

    addSetting(layout, "Ghost Mode", "ghostMode", 
               "Hide online status, typing, and read receipts.");
    addSetting(layout, "Anti-Delete", "antiDelete", 
               "Show deleted messages (indicated by icon).");
    addSetting(layout, "Anti-Edit", "antiEdit", 
               "Show original content of edited messages.");

    // --- BYPASS SECTION ---
    auto bypassLabel = new QLabel("Bypass Restrictions", this);
    bypassLabel->setObjectName("Section");
    layout->addWidget(bypassLabel);

    addSetting(layout, "Bypass Forward/Copy", "bypassRestrictions", 
               "Enable forwarding and copying in restricted chats.");
    addSetting(layout, "Spoof Mobile Status", "spoofMobile", 
               "Pretend you are using a mobile device.");
    addSetting(layout, "Offline Database", "offlineDb", 
               "Save messages locally for offline viewing.");

    layout->addStretch();

    auto closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &CustomModSettings::close);
    layout->addWidget(closeBtn);
}

void CustomModSettings::addSetting(QVBoxLayout *layout, const QString &label, const QString &id, const QString &description) {
    auto container = new QWidget(this);
    auto vLayout = new QVBoxLayout(container);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(2);

    auto cb = new QCheckBox(label, container);
    
    // Check initial state
    bool checked = false;
    if (id == "ghostMode") checked = CustomSettings::GhostMode();
    else if (id == "bypassRestrictions") checked = CustomSettings::BypassRestrictions();
    else if (id == "antiDelete") checked = CustomSettings::AntiDelete();
    else if (id == "antiEdit") checked = CustomSettings::AntiEdit();
    else if (id == "spoofMobile") checked = CustomSettings::SpoofMobile();
    else if (id == "offlineDb") checked = CustomSettings::OfflineDb();
    
    cb->setChecked(checked);

    connect(cb, &QCheckBox::stateChanged, [id](int state) {
        CustomSettings::Set(id, state == Qt::Checked);
    });

    auto descLabel = new QLabel(description, container);
    descLabel->setStyleSheet("color: #708499; font-size: 11px; margin-left: 35px; margin-bottom: 5px;");

    vLayout->addWidget(cb);
    vLayout->addWidget(descLabel);
    
    layout->addWidget(container);
}
