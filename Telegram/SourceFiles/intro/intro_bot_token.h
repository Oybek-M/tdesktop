/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "intro/intro_step.h"

namespace Ui {
class InputField;
class FlatLabel;
} // namespace Ui

namespace Intro {
namespace details {

// Bot token orqali kirish ekrani (A15, 1-bosqich — SINOV rejimi).
//
// Nima uchun alohida qadam: `auth.importBotAuthorization` oddiy
// telefon/QR oqimidan butunlay farq qiladi — kod tasdiqlash, parol
// va ro'yxatdan o'tish bosqichlari umuman yo'q. Bitta so'rov darhol
// `auth.Authorization` qaytaradi, shuning uchun bu eng sodda Step.
class BotTokenWidget final : public Step {
public:
	BotTokenWidget(
		QWidget *parent,
		not_null<Main::Account*> account,
		not_null<Data*> data);

	QString accessibilityName() override;

	void setInnerFocus() override;
	void activate() override;
	void cancelled() override;
	void submit() override;
	rpl::producer<QString> nextButtonText() const override;

	bool hasBack() const override {
		return true;
	}

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void updateControlsGeometry();
	void showTokenError(const MTP::Error &error);

	object_ptr<Ui::InputField> _token;
	object_ptr<Ui::FlatLabel> _warning;

	mtpRequestId _sentRequest = 0;

};

} // namespace details
} // namespace Intro
