/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "intro/intro_bot_token.h"

#include "config.h"
#include "lang/lang_keys.h"
#include "intro/intro_widget.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "styles/style_intro.h"

namespace Intro {
namespace details {

BotTokenWidget::BotTokenWidget(
	QWidget *parent,
	not_null<Main::Account*> account,
	not_null<Data*> data)
: Step(parent, account, data)
, _token(this, st::introName, rpl::single(u"Bot token"_q))
, _warning(
	this,
	// Ogohlantirish ATAYLAB ekranda turadi: 1-bosqichda bot akkaunt
	// bilan chat ro'yxati va kontaktlar ishlashi TEKSHIRILMAGAN.
	// Foydalanuvchi bo'sh oynani nuqson deb o'ylamasligi kerak.
	rpl::single(u"Sinov rejimi: bot akkauntda chat ro'yxati va "
		"kontaktlar ishlamasligi mumkin."_q),
	st::introDescription) {
	setTitleText(rpl::single(u"Bot token orqali kirish"_q));
	setDescriptionText(rpl::single(u"@BotFather bergan tokenni kiriting"_q));
	setErrorCentered(true);

	_token->changes() | rpl::on_next([=] {
		hideError();
	}, _token->lifetime());

	_token->submits() | rpl::on_next([=] {
		submit();
	}, _token->lifetime());

	setMouseTracking(true);
}

QString BotTokenWidget::accessibilityName() {
	return u"Bot token orqali kirish"_q;
}

rpl::producer<QString> BotTokenWidget::nextButtonText() const {
	return tr::lng_intro_next();
}

void BotTokenWidget::setInnerFocus() {
	_token->setFocusFast();
}

void BotTokenWidget::activate() {
	Step::activate();
	_token->show();
	_warning->show();
	setInnerFocus();
}

void BotTokenWidget::cancelled() {
	api().request(base::take(_sentRequest)).cancel();
}

void BotTokenWidget::resizeEvent(QResizeEvent *e) {
	Step::resizeEvent(e);
	updateControlsGeometry();
}

void BotTokenWidget::updateControlsGeometry() {
	const auto fieldTop = contentTop() + st::introStepFieldTop;
	_token->moveToLeft(contentLeft(), fieldTop);

	const auto warningTop = fieldTop
		+ st::introName.heightMin
		+ st::introPhoneTop;
	_warning->resizeToWidth(st::introNextButton.width);
	_warning->moveToLeft(contentLeft(), warningTop);
}

void BotTokenWidget::submit() {
	if (_sentRequest) {
		return;
	}
	const auto token = _token->getLastText().trimmed();
	if (token.isEmpty()) {
		showError(rpl::single(u"Iltimos, bot tokenni kiriting."_q));
		setInnerFocus();
		return;
	}
	hideError();

	// QR oqimidan farqli o'laroq bu yerda oraliq bosqich yo'q: server
	// darhol `auth.Authorization` qaytaradi va `finish()` sessiyani
	// o'zi yaratadi (intro_signup.cpp dagi nameSubmitDone bilan bir xil).
	_sentRequest = api().request(MTPauth_ImportBotAuthorization(
		MTP_int(0), // flags — hozircha ishlatilmaydi
		MTP_int(ApiId),
		MTP_string(ApiHash),
		MTP_string(token)
	)).done([=](const MTPauth_Authorization &result) {
		_sentRequest = 0;
		finish(result);
	}).fail([=](const MTP::Error &error) {
		_sentRequest = 0;
		showTokenError(error);
	}).send();
}

void BotTokenWidget::showTokenError(const MTP::Error &error) {
	if (MTP::IsFloodError(error)) {
		showError(tr::lng_flood_error());
		setInnerFocus();
		return;
	}
	const auto type = error.type();
	if (type == u"ACCESS_TOKEN_INVALID"_q) {
		showError(rpl::single(
			u"Bot token noto'g'ri. Tekshirib qayta kiriting."_q));
	} else if (type == u"ACCESS_TOKEN_EXPIRED"_q) {
		showError(rpl::single(u"Bot token muddati o'tgan."_q));
	} else {
		// Noma'lum xatoni YASHIRMAYMIZ: 1-bosqichning maqsadi aynan
		// server nima qaytarishini bilish, shuning uchun tur nomi
		// foydalanuvchiga ko'rsatiladi.
		showError(rpl::single(u"Xatolik: "_q + type));
	}
	setInnerFocus();
}

} // namespace details
} // namespace Intro
