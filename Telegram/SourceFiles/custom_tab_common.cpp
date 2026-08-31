#include "custom_tab_common.h"
#include "custom_mod_window.h"

QPointer<CustomModWindow> gInstance;

void ShowCustomBox(object_ptr<Ui::BoxContent> box) {
	if (gInstance) {
		gInstance->showBox(std::move(box));
	}
}

QColor AvatarFallbackColor(int idx) {
	static const QColor kColors[kAvatarColorsCount] = {
		QColor(0xE5, 0x39, 0x35), QColor(0x1E, 0x88, 0xE5),
		QColor(0x43, 0xA0, 0x47), QColor(0xFB, 0x8C, 0x00),
		QColor(0x8E, 0x24, 0xAA), QColor(0x00, 0x89, 0x7B),
		QColor(0xD8, 0x1B, 0x60),
	};
	return kColors[((idx % kAvatarColorsCount) + kAvatarColorsCount)
		% kAvatarColorsCount];
}

void PaintPeerAvatar(
		QPainter &p,
		const QRect &rect,
		const QString &peerId,
		const QString &name,
		Main::Session *session,
		Ui::PeerUserpicView &view) {
	bool ok = false;
	const auto rawId = peerId.toLongLong(&ok);
	if (ok && session) {
		const auto peer = session->data().peerLoaded(PeerId(uint64(rawId)));
		if (peer) {
			peer->paintUserpic(p, view, rect.x(), rect.y(), rect.width());
			return;
		}
	}
	// Fallback: harf + rang.
	p.setRenderHint(QPainter::Antialiasing);
	const auto idx = ok ? int(std::abs(rawId % kAvatarColorsCount)) : 0;
	p.setBrush(AvatarFallbackColor(idx));
	p.setPen(Qt::NoPen);
	p.drawEllipse(rect.adjusted(1, 1, -1, -1));
	p.setPen(Qt::white);
	QFont f;
	f.setPixelSize(rect.width() * 15 / 38);
	f.setBold(true);
	p.setFont(f);
	const auto letter = name.isEmpty() ? u"?"_q : name.left(1).toUpper();
	p.drawText(rect, letter, QTextOption(Qt::AlignCenter));
}


[[nodiscard]] object_ptr<Ui::BoxContent> ChoosePeerBox(
		not_null<Main::Session*> session,
		FnMut<bool(not_null<Data::Thread*>)> &&chosen,
		rpl::producer<QString> title) {
	auto types = InlineBots::PeerTypes();
	types |= InlineBots::PeerType::Bot;
	types |= InlineBots::PeerType::User;
	types |= InlineBots::PeerType::Group;
	types |= InlineBots::PeerType::Broadcast;

	return Window::PrepareChooseRecipientBox(
		session,
		std::move(chosen),
		std::move(title),
		nullptr,
		types);
}


[[nodiscard]] QString MakeWordDiff(
		const QString &before,
		const QString &after) {
	if (before == after) return before;
	const auto bWords = before.split(u' ', Qt::SkipEmptyParts);
	const auto aWords = after.split(u' ', Qt::SkipEmptyParts);
	auto start = 0;
	while (start < bWords.size() && start < aWords.size()
	       && bWords[start] == aWords[start]) {
		++start;
	}
	auto bEnd = int(bWords.size());
	auto aEnd = int(aWords.size());
	while (bEnd > start && aEnd > start
	       && bWords[bEnd - 1] == aWords[aEnd - 1]) {
		--bEnd; --aEnd;
	}
	auto parts = QStringList();
	for (auto i = 0; i < start; ++i) parts.append(bWords[i]);
	for (auto i = start; i < bEnd; ++i)
		parts.append(u"[-"_q + bWords[i] + u"]"_q);
	for (auto i = start; i < aEnd; ++i)
		parts.append(u"[+"_q + aWords[i] + u"]"_q);
	for (auto i = bEnd; i < bWords.size(); ++i) parts.append(bWords[i]);
	return parts.join(u' ');
}


#ifdef Q_OS_WIN
void ApplyTitleBar(QWidget *w) {
	const auto dark = Window::Theme::IsNightMode();
	const HWND hwnd = reinterpret_cast<HWND>(w->winId());
	if (!hwnd) return;

	static const auto kBuild = QOperatingSystemVersion::current().microVersion();

	// Dark/light mode toggle (Win10 build 17763+)
	if (kBuild >= 17763) {
		const BOOL value = dark ? TRUE : FALSE;
		static const auto kDarkAttr = (kBuild >= 18985) ? DWORD(20) : DWORD(19);
		DwmSetWindowAttribute(hwnd, kDarkAttr, &value, sizeof(value));
	}

	// Custom caption color (Win11 build 22000+)
	// DWMWA_CAPTION_COLOR = 35
	// Dark: #1F2936  →  COLORREF(R=0x1F, G=0x29, B=0x36)
	// Light: 0xFFFFFFFE = DWMWA_COLOR_DEFAULT (system resets to its default)
	if (kBuild >= 22000) {
		const COLORREF color = dark ? RGB(0x1F, 0x29, 0x36) : COLORREF(0xFFFFFFFE);
		DwmSetWindowAttribute(hwnd, DWORD(35), &color, sizeof(color));
	}
}

}
#endif

CustomTabBar::CustomTabBar(QWidget *parent, std::initializer_list<QString> names)
: Ui::RpWidget(parent)
, _names(names) {
	setFixedHeight(st::defaultBoxButton.height + st::customModTabBarVSkip);
	setCursor(Qt::PointingHandCursor);
	setAttribute(Qt::WA_OpaquePaintEvent);
	style::PaletteChanged() | rpl::on_next([=] {
		update();
	}, lifetime());
}

void CustomTabBar::setActiveTab(int index) {
	_active = index;
	update();
}

void CustomTabBar::paintEvent(QPaintEvent *) {
		Painter p(this);
		const auto h = height();
		p.fillRect(rect(), st::windowBg);
		p.fillRect(0, h - 1, width(), 1, st::shadowFg->c);
		if (_names.empty()) return;
		const auto tabW = width() / int(_names.size());
		for (auto i = 0; i < int(_names.size()); ++i) {
			const auto active = (i == _active);
			p.setPen(active
				? st::windowActiveTextFg->c
				: st::windowSubTextFg->c);
			p.setFont(active ? st::semiboldFont : st::normalFont);
			p.drawText(
				QRect(i * tabW, 0, tabW, h - 3),
				_names[i],
				QTextOption(Qt::AlignCenter));
			if (active) {
				p.fillRect(
					i * tabW + 4, h - 3,
					tabW - 8, 2,
					st::windowActiveTextFg->c);
			}
		}
	}


void CustomTabBar::mousePressEvent(QMouseEvent *e) {
		if (_names.empty()) return;
		const auto tabW = width() / int(_names.size());
		const auto idx = e->pos().x() / tabW;
		if (idx >= 0 && idx < int(_names.size())) {
			_active = idx;
			_tabSelected.fire_copy(idx);
			update();
		}
	}


void AddAvatarPeerRow(
		not_null<Ui::VerticalLayout*> content,
		not_null<Window::SessionController*> controller,
		const QString &peerId,
		const QString &name,
		Fn<void()> onDelete) {
	constexpr int kRowH    = 56;
	constexpr int kAvSize  = 38;
	constexpr int kPadL = 14;
	constexpr int kPadR = 12;
	constexpr int kGap  = 12;
	constexpr int kDelBtnW = 76;

	const auto row = content->add(object_ptr<Ui::RpWidget>(content));
	row->setFixedHeight(kRowH);

	// Avatar circle — real userpic agar peer cache da bo'lsa,
	// aks holda fallback (harf + rang).
	const auto av = Ui::CreateChild<Ui::RpWidget>(row);
	av->setFixedSize(kAvSize, kAvSize);
	const auto userpicView = std::make_shared<Ui::PeerUserpicView>();
	const auto session = &controller->session();
	av->paintRequest() | rpl::on_next([=](QRect) {
		Painter p(av);
		PaintPeerAvatar(
			p,
			QRect(0, 0, kAvSize, kAvSize),
			peerId,
			name,
			session,
			*userpicView);
	}, av->lifetime());

	const auto nameLabel = Ui::CreateChild<Ui::FlatLabel>(
		row,
		rpl::single(name.isEmpty() ? peerId : name),
		st::boxLabel);
	const auto idLabel = Ui::CreateChild<Ui::FlatLabel>(
		row,
		rpl::single(u"ID: "_q + peerId),
		st::customModHintLabel);
	const auto delBtn = Ui::CreateChild<Ui::RoundButton>(
		row,
		rpl::single(u"O'chirish"_q),
		st::attentionBoxButton);
	delBtn->setFixedWidth(kDelBtnW);
	av->show();
	nameLabel->show();
	idLabel->show();
	delBtn->show();

	row->paintRequest() | rpl::on_next([=](QRect) {
		Painter p(row);
		p.fillRect(kPadL + kAvSize + kGap, kRowH - 1,
			row->width() - kPadL - kAvSize - kGap - kPadR, 1,
			st::shadowFg->c);
	}, row->lifetime());

	const auto layoutRow = [=](int w) {
		const auto avY = (kRowH - kAvSize) / 2;
		av->move(kPadL, avY);
		av->update();
		const auto textX = kPadL + kAvSize + kGap;
		const auto textW = w - textX - kGap - kDelBtnW - kPadR;
		if (textW <= 0) return;
		nameLabel->resizeToWidth(textW);
		nameLabel->move(textX, 10);
		nameLabel->update();
		idLabel->resizeToWidth(textW);
		idLabel->move(textX, 10 + nameLabel->height() + 2);
		idLabel->update();
		const auto btnY = (kRowH - st::defaultBoxButton.height) / 2;
		delBtn->move(w - kPadR - kDelBtnW, btnY);
	};
	row->widthValue() | rpl::on_next(layoutRow, row->lifetime());
	if (content->width() > 0) layoutRow(content->width());

	if (onDelete) {
		delBtn->addClickHandler([=] {
			delBtn->setDisabled(true);
			onDelete();
		});
	}
}

