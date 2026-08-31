#pragma once

#include <QtCore/QString>
#include <QtCore/QPointer>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QPainter>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

#include "base/weak_ptr.h"
#include "core/application.h"
#include "core/version.h"
#include "custom_branding.h"
#include "custom_db.h"
#include "custom_activity_history_box.h"
#include "custom_media_quota.h"
#include "custom_settings.h"
#include "custom_upstream_checker.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "main/main_session.h"
#include "ui/userpic_view.h"
#include "ui/painter.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/layer_manager.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "window/window_peer_menu.h"
#include "window/themes/window_theme.h"
#include "inline_bots/bot_attach_web_view.h"
#include "styles/style_basic.h"
#include "styles/style_custom_mod.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

class CustomModWindow;
extern QPointer<CustomModWindow> gInstance;

void ShowCustomBox(object_ptr<Ui::BoxContent> box);

#ifdef Q_OS_WIN
void ApplyTitleBar(QWidget *w);
#endif

constexpr int kAvatarColorsCount = 7;
QColor AvatarFallbackColor(int idx);

void PaintPeerAvatar(
	QPainter &p,
	const QRect &rect,
	const QString &peerId,
	const QString &name,
	Main::Session *session,
	Ui::PeerUserpicView &view);

[[nodiscard]] object_ptr<Ui::BoxContent> ChoosePeerBox(
	not_null<Main::Session*> session,
	FnMut<bool(not_null<Data::Thread*>)> &&chosen,
	rpl::producer<QString> title);

[[nodiscard]] QString MakeWordDiff(
	const QString &before,
	const QString &after);

void AddAvatarPeerRow(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	const QString &peerId,
	const QString &name,
	Fn<void()> onDelete);

class CustomTabBar final : public Ui::RpWidget {
public:
	CustomTabBar(QWidget *parent, std::initializer_list<QString> names);

	[[nodiscard]] rpl::producer<int> tabSelected() const;
	void setActiveTab(int index);

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *e) override;

private:
	std::vector<QString> _names;
	int _active = 0;
	rpl::event_stream<int> _tabSelected;
};

// Tab Builders
void fillGeneralTab(not_null<Ui::VerticalLayout*> content);
void fillUpstreamCheckSection(not_null<Ui::VerticalLayout*> content);
void fillPeerSection(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	const QString &title,
	const QString &description,
	bool isWhiteList,
	Fn<bool(const QString&)> containsFn,
	Fn<void(const QString&, const QString&)> addFn,
	Fn<void(const QString&)> removeFn,
	Fn<std::vector<std::pair<QString, QString>>()> getAllFn,
	Fn<void(const QString&)> conflictFn,
	Fn<bool(const QString&)> conflictContainsFn,
	const QString &conflictName);
void fillPerChatSection(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller);
void fillActivityHistorySection(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	Fn<void()> onRebuild);
void fillPeersTab(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	Fn<void()> onRebuild);
void fillArchiveTab(
	not_null<Ui::VerticalLayout*> content,
	Fn<void()> onRefresh);
void fillAboutTab(
	not_null<Ui::VerticalLayout*> content,
	QWidget *dialogParent,
	Fn<void()> onArchiveChanged);
