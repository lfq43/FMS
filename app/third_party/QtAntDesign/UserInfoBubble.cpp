#include "UserInfoBubble.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QVBoxLayout>

#include "AntMessageManager.h"
#include "AuthService.h"
#include "DesignSystem.h"
#include "StyleSheet.h"

namespace {
QString formatStorageSize(qint64 bytes)
{
	if (bytes < 1024) {
		return QString("%1 B").arg(bytes);
	}
	const double kb = bytes / 1024.0;
	if (kb < 1024) {
		return QString::number(kb, 'f', 1) + " KB";
	}
	const double mb = kb / 1024.0;
	if (mb < 1024) {
		return QString::number(mb, 'f', 1) + " MB";
	}
	return QString::number(mb / 1024.0, 'f', 1) + " GB";
}

int currentUserFileCount()
{
	if (!AuthService::isLoggedIn()) {
		return 0;
	}

	QSqlQuery query;
	query.prepare("SELECT COUNT(*) FROM files WHERE owner_id = :owner_id AND is_trashed = 0");
	query.bindValue(":owner_id", AuthService::currentUserId());
	if (!query.exec() || !query.next()) {
		qDebug() << "查询用户文件数量失败:" << query.lastError().text();
		return 0;
	}
	return query.value(0).toInt();
}

QString displayLastLogin()
{
	const QString lastLogin = AuthService::currentLastLoginAt();
	return lastLogin.isEmpty() ? QStringLiteral("本次为首次登录") : lastLogin;
}
}

UserInfoBubble::UserInfoBubble(QWidget* parent)
	: QWidget(parent)
{
	setAttribute(Qt::WA_TranslucentBackground);
	setWindowFlags(Qt::FramelessWindowHint);

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(margin + arrowWidth + spacing, margin, margin + spacing, margin);
	layout->setSpacing(3);

	QFont font;
	font.setPointSize(11);

	m_nameLabel = new QLabel(this);
	m_nameLabel->setFont(font);

	font.setPointSize(10);
	m_roleLabel = new QLabel(this);
	m_storageLabel = new QLabel(this);
	m_fileCountLabel = new QLabel(this);
	m_lastLoginLabel = new QLabel(this);
	m_roleLabel->setFont(font);
	m_storageLabel->setFont(font);
	m_fileCountLabel->setFont(font);
	m_lastLoginLabel->setFont(font);

	QWidget* infoWidget = new QWidget(this);
	infoWidget->setObjectName("storageInfoWidget");
	infoWidget->setMinimumHeight(86);
	auto theme = DesignSystem::instance()->currentTheme();
	infoWidget->setStyleSheet(StyleSheet::gradientQss(theme.vipGradientStartColor, theme.vipGradientMidColor, theme.vipGradientEndColor));

	QVBoxLayout* infoLayout = new QVBoxLayout(infoWidget);
	infoLayout->setContentsMargins(12, 8, 12, 8);
	infoLayout->setSpacing(4);
	infoLayout->addWidget(m_roleLabel);
	infoLayout->addWidget(m_storageLabel);
	infoLayout->addWidget(m_fileCountLabel);
	infoLayout->addWidget(m_lastLoginLabel);

	QWidget* bottomWid = new QWidget(this);
	QHBoxLayout* bottomHlay = new QHBoxLayout(bottomWid);
	bottomHlay->setContentsMargins(0, 0, 0, 0);

	QStringList strList;
	strList << "个人中心" << "退出登录";
	QStringList objectNames, qssList;
	objectNames << "profileButton" << "logoutButton";
	qssList << StyleSheet::signInBtnQss() << StyleSheet::logoutBtnQss();

	for (int i = 0; i < strList.size(); ++i)
	{
		QPushButton* btn = new QPushButton(strList[i], bottomWid);
		btn->setFont(font);
		btn->setObjectName(objectNames[i]);
		btn->setMinimumWidth(70);
		btn->setMinimumHeight(25);
		btn->setCursor(Qt::PointingHandCursor);
		btn->setStyleSheet(qssList[i]);
		bottomHlay->addWidget(btn);

		if (i == 0)
		{
			bottomHlay->addStretch();
		}

		connect(btn, &QPushButton::clicked, this, [this]()
			{
				QPushButton* btn = qobject_cast<QPushButton*>(sender());
				if (!btn) return;

				if (btn->objectName() == "profileButton")
				{
					AntMessageManager::instance()->showMessage(AntMessage::Info, "个人中心功能将在用户模块中完善");
				}
				else if (btn->objectName() == "logoutButton")
				{
					AuthService::logout();
					emit exitLogin(false);
				}
			});
	}

	layout->addSpacing(8);
	layout->addWidget(m_nameLabel);
	layout->addSpacing(8);
	layout->addWidget(infoWidget);
	layout->addSpacing(8);
	layout->addWidget(bottomWid);
	layout->addSpacing(14);

	refreshUserInfo();
}

UserInfoBubble::~UserInfoBubble()
{
}

void UserInfoBubble::refreshUserInfo()
{
	const QString displayName = AuthService::currentDisplayName().isEmpty()
		? QStringLiteral("未登录用户")
		: AuthService::currentDisplayName();
	const QString username = AuthService::currentUsername().isEmpty()
		? QStringLiteral("-")
		: AuthService::currentUsername();
	const QString storageText = QString("%1 / %2")
		.arg(formatStorageSize(AuthService::currentStorageUsedBytes()))
		.arg(formatStorageSize(AuthService::currentStorageQuotaBytes()));

	m_nameLabel->setText(QString("当前用户：%1").arg(displayName));
	m_roleLabel->setText(QString("账号：%1").arg(username));
	m_storageLabel->setText(QString("存储空间：%1").arg(storageText));
	m_fileCountLabel->setText(QString("文件数量：%1").arg(currentUserFileCount()));
	m_lastLoginLabel->setText(QString("上次登录：%1").arg(displayLastLogin()));
}

void UserInfoBubble::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	p.setPen(Qt::NoPen);

	const int radius = 6;
	QRect rectBubble(arrowWidth, 0, width() - 5, height() - 5);
	QRectF r = rectBubble.adjusted(margin, margin, -margin, -margin);

	int ax = r.left();
	int ay = arrowOffsetY;
	int ah = arrowHeight;

	QPainterPath path;
	path.moveTo(ax, ay);
	path.lineTo(ax - arrowWidth, ay + ah / 2);
	path.lineTo(ax, ay + ah);
	path.lineTo(ax, r.bottom() - radius);
	path.arcTo(QRectF(r.left(), r.bottom() - 2 * radius, 2 * radius, 2 * radius), 180, 90);
	path.lineTo(r.right() - radius, r.bottom());
	path.arcTo(QRectF(r.right() - 2 * radius, r.bottom() - 2 * radius, 2 * radius, 2 * radius), 270, 90);
	path.lineTo(r.right(), r.top() + radius);
	path.arcTo(QRectF(r.right() - 2 * radius, r.top(), 2 * radius, 2 * radius), 0, 90);
	path.lineTo(r.left() + radius, r.top());
	path.arcTo(QRectF(r.left(), r.top(), 2 * radius, 2 * radius), 90, 90);
	path.lineTo(ax, ay);

	p.setBrush(DesignSystem::instance()->currentTheme().userBubbleBgColor);
	p.drawPath(path);
}

void UserInfoBubble::leaveEvent(QEvent* e)
{
	QWidget::leaveEvent(e);

	QTimer::singleShot(100, this, [this]()
		{
			if (!this->underMouse())
			{
				emit requestHide();
			}
		});
}
