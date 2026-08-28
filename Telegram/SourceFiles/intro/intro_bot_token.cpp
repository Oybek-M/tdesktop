/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include intro/intro_bot_token.h

#include config.h
#include lang/lang_keys.h
#include intro/intro_widget.h
#include ui/widgets/fields/input_field.h
#include ui/widgets/labels.h
#include styles/style_intro.h

namespace Intro {
namespace details {

BotTokenWidget::BotTokenWidget(
	QWidget *parent,
	not_null<Main::Account*> account,
	not_null<Data*> data)
: Step(parent, account, data)
, _token(this, st::introCountry, rpl::single(uBot token_q))
, _warning(
	this,
	rpl::single(uSinov rejimi: bot akkauntda chat ro'yxati va kontaktlar ishlamasligi mumkin._q),
	st::introDescription) {

	setTitleText(rpl::single(uBot token orqali kirish_q));
	setDescriptionText(TextWithEntities{ u@BotFather bergan tokenni kiriting_q });
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
	return uBot token orqali kirish_q;
}

rpl::producer<QString> BotTokenWidget::nextButtonText() const {
	return tr::lng_intro_next();
}

void BotTokenWidget::setInnerFocus() {
	_token->setFocusFast();
}

void BotTokenWidget::activate() {
	Step::activate();
	setInnerFocus();
}

void BotTokenWidget::cancelled() {
	if (_requestId) {
		api().request(base::take(_requestId)).cancel();
	}
}

int BotTokenWidget::errorTop() const {
	return contentTop() + st::introErrorBelowLinkTop;
}

void BotTokenWidget::resizeEvent(QResizeEvent *e) {
	Step::resizeEvent(e);
	updateControlsGeometry();
}

void BotTokenWidget::updateControlsGeometry() {
	const auto fieldTop = contentTop() + st::introStepFieldTop;
	_token->moveToLeft(contentLeft(), fieldTop);

	const auto warningTop = fieldTop + _token->height() + st::introPhoneTop * 2;
	_warning->resizeToWidth(st::introNextButton.width);
	_warning->moveToLeft(contentLeft(), warningTop);
}

void BotTokenWidget::submit() {
	if (_requestId) {
		return;
	}
	const auto token = _token->getLastText().trimmed();
	if (token.isEmpty()) {
		showError(rpl::single(uIltimos, bot tokenni kiriting._q));
		setInnerFocus();
		return;
	}

	hideError();

	// Bot token orqali kirish so'rovi: auth.importBotAuthorization MTProto metodini chaqiramiz.
	_requestId = api().request(MTPauth_ImportBotAuthorization(
		MTP_int(0), // flags
		MTP_int(ApiId),
		MTP_string(ApiHash),
		MTP_string(token)
	)).done([=](const MTPauth_Authorization &result) {
		_requestId = 0;
		finish(result);
	}).fail([=](const MTP::Error &error) {
		_requestId = 0;
		showTokenError(error);
	}).send();
}

void BotTokenWidget::showTokenError(const MTP::Error &error) {
	if (MTP::IsFloodError(error)) {
		showError(tr::lng_flood_error());
		return;
	}
	const auto &err = error.type();
	if (err == uBOT_TOKEN_INVALID_q || err == uACCESS_TOKEN_INVALID_q) {
		showError(rpl::single(uBot token noto'g'ri. Tekshirib qayta kiriting._q));
	} else if (err == uBOT_TOKEN_EXPIRED_q || err == uACCESS_TOKEN_EXPIRED_q) {
		showError(rpl::single(uBot token muddati o'tgan._q));
	} else {
		showError(rpl::single(uXatolik: _q + err));
	}
	setInnerFocus();
}

} // namespace details
} // namespace Intro
