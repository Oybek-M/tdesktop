#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QCheckBox>

class CustomModSettings : public QWidget {
    Q_OBJECT

public:
    explicit CustomModSettings(QWidget *parent = nullptr);

private:
    void addSetting(QVBoxLayout *layout, const QString &label, const QString &id, const QString &description);
};
