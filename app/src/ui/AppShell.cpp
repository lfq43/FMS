#include "AppShell.h"
#include <qevent.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qscreen.h>
#include "StyleSheet.h"
#include "AntMessageManager.h"
#include "AntTooltipManager.h"
#include "DialogViewController.h"
#include "AntButton.h"
#include <QToolButton>
#include <QLabel>
#include <QShowEvent>
#include <QWindow>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDesktopServices>
#include <QDrag>
#include <QFrame>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMessageBox>
#include <QPainterPath>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QStyle>
#include <QUrl>
#include "NotificationManager.h"
#include "DesignSystem.h"
#include "TransparentMask.h"
#include "MaskWidget.h"
#include "ThemeSwitcher.h"
#include "AntToggleButton.h"
#include "AuthService.h"
#include "FileService.h"
#include "FolderService.h"

#include <functional>
#include <memory>

namespace {
enum class LineIcon
{
	File,
	Folder,
	Refresh,
	Trash,
	More
};

QIcon makeLineIcon(LineIcon icon, const QColor& color = QColor("#1677ff"))
{
	QPixmap pixmap(24, 24);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);

	if (icon == LineIcon::File) {
		painter.drawRoundedRect(QRectF(6, 3.5, 12, 17), 2, 2);
		painter.drawLine(QPointF(13.5, 4), QPointF(18, 8.5));
		painter.drawLine(QPointF(14, 4), QPointF(14, 9));
		painter.drawLine(QPointF(14, 9), QPointF(18, 9));
		painter.drawLine(QPointF(8.5, 13), QPointF(15.5, 13));
		painter.drawLine(QPointF(8.5, 16), QPointF(14, 16));
	}
	else if (icon == LineIcon::Folder) {
		painter.drawPath([] {
			QPainterPath path;
			path.moveTo(3.5, 8);
			path.quadTo(3.5, 6, 5.5, 6);
			path.lineTo(10, 6);
			path.lineTo(12, 8.5);
			path.lineTo(18.5, 8.5);
			path.quadTo(20.5, 8.5, 20.5, 10.5);
			path.lineTo(20.5, 17.5);
			path.quadTo(20.5, 19.5, 18.5, 19.5);
			path.lineTo(5.5, 19.5);
			path.quadTo(3.5, 19.5, 3.5, 17.5);
			path.closeSubpath();
			return path;
		}());
	}
	else if (icon == LineIcon::Refresh) {
		painter.drawArc(QRectF(5.2, 5.2, 13.6, 13.6), 35 * 16, 285 * 16);
		painter.drawLine(QPointF(18.1, 7.2), QPointF(18.1, 12.2));
		painter.drawLine(QPointF(18.1, 7.2), QPointF(13.2, 7.2));
		painter.drawArc(QRectF(5.2, 5.2, 13.6, 13.6), 215 * 16, 85 * 16);
	}
	else if (icon == LineIcon::Trash) {
		painter.drawLine(QPointF(8, 7), QPointF(16, 7));
		painter.drawLine(QPointF(10, 5), QPointF(14, 5));
		painter.drawRoundedRect(QRectF(7, 9, 10, 11), 2, 2);
		painter.drawLine(QPointF(10, 11.5), QPointF(10, 17.5));
		painter.drawLine(QPointF(14, 11.5), QPointF(14, 17.5));
	}
	else if (icon == LineIcon::More) {
		painter.setBrush(color);
		painter.setPen(Qt::NoPen);
		painter.drawEllipse(QPointF(7, 12), 1.7, 1.7);
		painter.drawEllipse(QPointF(12, 12), 1.7, 1.7);
		painter.drawEllipse(QPointF(17, 12), 1.7, 1.7);
	}

	return QIcon(pixmap);
}

class IconActionButton : public QPushButton
{
public:
	IconActionButton(LineIcon icon, const QString& tipText, QWidget* parent)
		: QPushButton(parent), m_tipText(tipText)
	{
		setIcon(makeLineIcon(icon));
		setIconSize(QSize(20, 20));
		setFixedSize(32, 28);
		setCursor(Qt::PointingHandCursor);
		setStyleSheet(
			"QPushButton { background: transparent; border: none; border-radius: 8px; padding: 4px; }"
			"QPushButton:disabled { opacity: 0.45; }"
			"QPushButton:hover:enabled { background: #eaf3ff; }"
		);
	}

protected:
	void enterEvent(QEnterEvent* event) override
	{
		QPushButton::enterEvent(event);
		if (isEnabled() && !m_tipText.isEmpty()) {
			AntTooltipManager::instance()->showTooltip(this, m_tipText, AntTooltipManager::Position::Top);
		}
	}

	void leaveEvent(QEvent* event) override
	{
		AntTooltipManager::instance()->hideTooltip();
		QPushButton::leaveEvent(event);
	}

private:
	QString m_tipText;
};

QString formatFileSize(qint64 bytes)
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

QString safeLocalName(QString value)
{
	value = value.trimmed();
	if (value.isEmpty()) {
		return "未命名文件";
	}

	const QString invalidChars = "\\/:*?\"<>|";
	for (const QChar ch : invalidChars) {
		value.replace(ch, "_");
	}
	return value;
}

bool revealInFileManager(const QString& path)
{
	if (path.isEmpty() || !QFileInfo::exists(path)) {
		return false;
	}

	const QFileInfo info(path);
#ifdef Q_OS_WIN
	if (info.isDir()) {
		return QProcess::startDetached("explorer.exe", { QDir::toNativeSeparators(info.absoluteFilePath()) });
	}
	return QProcess::startDetached("explorer.exe", { "/select," + QDir::toNativeSeparators(info.absoluteFilePath()) });
#else
	return QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir() ? info.absoluteFilePath() : info.absolutePath()));
#endif
}

void scanFolderTree(const QString& folderPath, int& folderCount, int& fileCount)
{
	folderCount = 0;
	fileCount = 0;

	QDirIterator iterator(folderPath, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (iterator.hasNext()) {
		iterator.next();
		const QFileInfo info = iterator.fileInfo();
		if (info.isDir()) {
			++folderCount;
		}
		else if (info.isFile()) {
			++fileCount;
		}
	}
}

bool confirmLargeFolderImport(QWidget* parent, const QString& folderPath)
{
	int folderCount = 0;
	int fileCount = 0;
	scanFolderTree(folderPath, folderCount, fileCount);

	const int totalCount = folderCount + fileCount;
	if (totalCount < 200 && folderCount < 50) {
		return true;
	}

	const QFileInfo folderInfo(folderPath);
	const QString message = QString("将导入“%1”中的 %2 个文件夹、%3 个文件。\n如果选中了项目目录或构建目录，可能会生成大量备份记录。是否继续？")
		.arg(folderInfo.fileName().isEmpty() ? folderPath : folderInfo.fileName())
		.arg(folderCount)
		.arg(fileCount);

	return QMessageBox::question(parent, "确认导入文件夹", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}

class FileTableWidget : public QTableWidget
{
public:
	using QTableWidget::QTableWidget;

	std::function<void(int fileId, int targetFolderId)> moveFileToFolder;

protected:
	void mousePressEvent(QMouseEvent* event) override
	{
		m_dragStartPos = event->pos();
		QTableWidget::mousePressEvent(event);
	}

	void mouseMoveEvent(QMouseEvent* event) override
	{
		if (!(event->buttons() & Qt::LeftButton) ||
			(event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
			QTableWidget::mouseMoveEvent(event);
			return;
		}

		const QModelIndex index = indexAt(m_dragStartPos);
		if (!index.isValid() || index.column() == 5 || !item(index.row(), 0) ||
			item(index.row(), 0)->data(Qt::UserRole + 1).toString() != "file") {
			QTableWidget::mouseMoveEvent(event);
			return;
		}

		auto* mimeData = new QMimeData();
		mimeData->setData("application/x-fms-file-id", QByteArray::number(item(index.row(), 0)->data(Qt::UserRole).toInt()));

		QDrag drag(this);
		drag.setMimeData(mimeData);
		drag.exec(Qt::MoveAction);
	}

	void dragEnterEvent(QDragEnterEvent* event) override
	{
		if (event->mimeData()->hasFormat("application/x-fms-file-id")) {
			event->acceptProposedAction();
			return;
		}
		QTableWidget::dragEnterEvent(event);
	}

	void dragMoveEvent(QDragMoveEvent* event) override
	{
		const QModelIndex index = indexAt(event->position().toPoint());
		if (index.isValid() && item(index.row(), 0) &&
			item(index.row(), 0)->data(Qt::UserRole + 1).toString() == "folder") {
			event->acceptProposedAction();
			return;
		}
		event->ignore();
	}

	void dropEvent(QDropEvent* event) override
	{
		if (!event->mimeData()->hasFormat("application/x-fms-file-id")) {
			QTableWidget::dropEvent(event);
			return;
		}

		const QModelIndex index = indexAt(event->position().toPoint());
		if (!index.isValid() || !item(index.row(), 0) ||
			item(index.row(), 0)->data(Qt::UserRole + 1).toString() != "folder") {
			event->ignore();
			return;
		}

		const int fileId = event->mimeData()->data("application/x-fms-file-id").toInt();
		const int targetFolderId = item(index.row(), 0)->data(Qt::UserRole).toInt();
		if (moveFileToFolder) {
			moveFileToFolder(fileId, targetFolderId);
		}
		event->acceptProposedAction();
	}

private:
	QPoint m_dragStartPos;
};
}

AppShell::AppShell(QWidget* parent)
	: QWidget(parent)
{
	ui.setupUi(this);

	setWindowTitle("文枢");
	setObjectName("AppShell");
#ifdef Q_OS_LINUX
	setWindowFlags(Qt::FramelessWindowHint);
	setAttribute(Qt::WA_Hover, true);
	installEventFilter(this);
#endif

#ifdef Q_OS_WIN
	setWindowFlags(Qt::FramelessWindowHint);
	setAttribute(Qt::WA_NoSystemBackground);
#endif
	QSize miniSize(1120, 725);
	setMinimumSize(miniSize);

	ui.main_widget->setStyleSheet(StyleSheet::mainQss(DesignSystem::instance()->backgroundColor()));

	int w = 0, h = 0;
	QScreen* screen = QGuiApplication::primaryScreen();
	if (screen)
	{
		QSize screenSize = screen->availableSize();
		w = int(screenSize.width() * 0.50);
		h = int(screenSize.height() * 0.60);
		if (w < miniSize.width() && h < miniSize.height())
		{
			w = miniSize.width();
			h = miniSize.height();
		}
		resize(w, h);
	}
	setContentsMargins(0, 0, 0, 0);

	// 初始化全局设计系统，后续控件会读取主题和主窗口指针。
	DesignSystem::instance()->setThemeMode(DesignSystem::Light);
	DesignSystem::instance()->setMainWindow(this);
	ThemeSwitcher* themeSwitcher = new ThemeSwitcher(this);
	themeSwitcher->setThemeColor();
	// 注册全局透明遮罩。
	TransparentMask* tpMask = new TransparentMask(this);
	DesignSystem::instance()->setTransparentMask(tpMask);
	// 注册深色主题切换动画遮罩。
	MaskWidget* darkMask = new MaskWidget(w, h, this);
	DesignSystem::instance()->setDarkMask(darkMask);

	// 调整导航栏、标题栏和内容区的基础样式。
	ui.navi_widget->setFixedWidth(m_naviWidth);
	ui.navi_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	ui.navi_widget->setStyleSheet(StyleSheet::naviQss(DesignSystem::instance()->widgetBgColor()));
	ui.titleBar->setStyleSheet(StyleSheet::titleBarQss());
	ui.central->setStyleSheet(StyleSheet::centralQss());
	ui.titleBar->setFixedHeight(m_titleBarHeight);

	QHBoxLayout* titleLay = new QHBoxLayout(ui.titleBar);
	titleLay->setContentsMargins(20, 0, rightMargin, 0);
	titleLay->setSpacing(titleBarSpacing);
	QFont font;
	font.setPointSizeF(16);
	font.setBold(true);
	QLabel* title = new QLabel("文枢", ui.titleBar);
	title->setFont(font);
	// 创建标题栏窗口控制按钮。
	btnMin = new QToolButton(ui.titleBar);
	btnMax = new QToolButton(ui.titleBar);
	btnClose = new QToolButton(ui.titleBar);
	// 设置标题栏按钮图标。
	btnMin->setIcon(DesignSystem::instance()->btnMinIcon());
	btnMax->setIcon(DesignSystem::instance()->btnMaxIcon());
	btnClose->setIcon(DesignSystem::instance()->btnCloseIcon());
	// 设置标题栏按钮样式。
	btnMin->setStyleSheet(StyleSheet::toolBtnQss());
	btnMax->setStyleSheet(StyleSheet::toolBtnQss());
	btnClose->setStyleSheet(StyleSheet::toolBtnQss());
	btnMin->setFixedSize(32, 32);
	btnMax->setFixedSize(32, 32);
	btnClose->setFixedSize(34, 34);
	int extraWidth = 100;	// 标题栏右侧控件的额外间距宽度。
	m_widgetTotalWidth = btnMin->width() + btnMax->width() + btnClose->width();

	QStringList searchItems = {
		"文档", "图片", "视频", "压缩包", "PDF", "最近添加", "未上传"
	};
	antInput = new AntInput(300, searchItems, ui.titleBar);
	antInput->setFixedWidth(254);
	antInput->setFixedHeight(46);
	antInput->setPlaceholderText("搜索文件");

	qreal dpiScale = QApplication::primaryScreen()->devicePixelRatio();
	int widgetTotalWidth = rightMargin + btnMin->width() + btnMax->width() + btnClose->width() +
		antInput->width() + 3 * titleBarSpacing;
	m_widgetTotalWidthPhysicalPixels = static_cast<int>(widgetTotalWidth * dpiScale);
	m_titleLeftTotalWidthPhysicalPixels = static_cast<int>(m_naviWidth * dpiScale);
	m_titleBarHeightPhysicalPixels = static_cast<int>(m_titleBarHeight * dpiScale);

	// 将标题、搜索框和窗口控制按钮加入标题栏。
	titleLay->addWidget(title);
	titleLay->addStretch();
	titleLay->addWidget(antInput);
	titleLay->addWidget(btnMin);
	titleLay->addWidget(btnMax);
	titleLay->addWidget(btnClose);
	totalSpacingWidth = 3 * titleBarSpacing;
	QVBoxLayout* naviLay = new QVBoxLayout(ui.navi_widget);
	ui.navi_widget->layout()->setContentsMargins(0, 0, 0, 0);
	CircularAvatar* avatar = new CircularAvatar(QSize(42, 42), ":/Imgs/noLogin.svg", ":/Imgs/github.svg", ui.navi_widget);
	avatar->allowLogin(AuthService::isLoggedIn());
	// 创建页面容器。
	QVBoxLayout* contentLay = new QVBoxLayout(ui.central);
	contentLay->setContentsMargins(0, 0, 0, 0);
	contentLay->setSpacing(0);
	stackedWidget = new SlideStackedWidget(ui.central);
	contentLay->addWidget(stackedWidget);
	// 创建各主页面。
	QWidget* homePage = new QWidget(stackedWidget);
	QWidget* settingsPage = new QWidget(stackedWidget);
	QWidget* aboutPage = new QWidget(stackedWidget);
	homePage->setStyleSheet("background-color: transparent;");
	settingsPage->setStyleSheet("background-color: transparent;");
	aboutPage->setStyleSheet("background-color: transparent;");

	QVBoxLayout* homeLayout = new QVBoxLayout(homePage);
	homeLayout->setContentsMargins(28, 22, 28, 28);
	homeLayout->setSpacing(18);

	QHBoxLayout* toolbarLayout = new QHBoxLayout();
	toolbarLayout->setContentsMargins(0, 0, 0, 0);
	toolbarLayout->setSpacing(12);

	AntButton* addFileBtn = new AntButton("添加", 10.5, homePage);
	addFileBtn->setFixedSize(96, 48);
	QPushButton* newFileBtn = new QPushButton("新建文件", homePage);
	newFileBtn->setFixedSize(88, 34);
	newFileBtn->setCursor(Qt::PointingHandCursor);
	QPushButton* newFolderBtn = new QPushButton("新建文件夹", homePage);
	newFolderBtn->setFixedSize(98, 34);
	newFolderBtn->setCursor(Qt::PointingHandCursor);
	QPushButton* backFolderBtn = new QPushButton("返回上级", homePage);
	backFolderBtn->setEnabled(false);
	backFolderBtn->setFixedSize(88, 34);
	backFolderBtn->setCursor(Qt::PointingHandCursor);
	QPushButton* uploadBtn = new QPushButton("上传", homePage);
	uploadBtn->setEnabled(false);
	uploadBtn->setFixedSize(82, 34);
	uploadBtn->setCursor(Qt::PointingHandCursor);
	const QString toolbarButtonStyle =
		"QPushButton { color: #8c8c8c; background: #f5f5f5; border: 1px solid #d9d9d9; border-radius: 6px; }"
		"QPushButton:enabled { color: #1677ff; background: #ffffff; border-color: #1677ff; }"
		"QPushButton:hover:enabled { color: #0958d9; border-color: #0958d9; }";
	newFileBtn->setStyleSheet(toolbarButtonStyle);
	newFolderBtn->setStyleSheet(toolbarButtonStyle);
	backFolderBtn->setStyleSheet(toolbarButtonStyle);
	uploadBtn->setStyleSheet(toolbarButtonStyle);

	QLabel* localHint = new QLabel("本地文件", homePage);
	QFont hintFont;
	hintFont.setPointSizeF(13);
	hintFont.setBold(true);
	localHint->setFont(hintFont);
	localHint->setStyleSheet("color: #1f1f1f;");

	toolbarLayout->addWidget(addFileBtn);
	toolbarLayout->addWidget(newFileBtn);
	toolbarLayout->addWidget(newFolderBtn);
	toolbarLayout->addWidget(backFolderBtn);
	toolbarLayout->addWidget(uploadBtn);
	toolbarLayout->addStretch();
	toolbarLayout->addWidget(localHint);

	QLabel* breadcrumbLabel = new QLabel("全部文件", homePage);
	breadcrumbLabel->setTextFormat(Qt::RichText);
	breadcrumbLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
	breadcrumbLabel->setOpenExternalLinks(false);
	breadcrumbLabel->setCursor(Qt::PointingHandCursor);
	breadcrumbLabel->setStyleSheet(
		"QLabel { color: #6b7280; font-size: 13px; padding: 4px 0 10px 0; }"
	);

	FileTableWidget* fileTable = new FileTableWidget(0, 6, homePage);
	fileTable->setHorizontalHeaderLabels({"文件名", "大小", "类型", "备份状态", "修改时间", "操作"});
	fileTable->verticalHeader()->setVisible(false);
	fileTable->setShowGrid(false);
	fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	fileTable->setSelectionMode(QAbstractItemView::SingleSelection);
	fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	fileTable->setAlternatingRowColors(false);
	fileTable->setFrameShape(QFrame::NoFrame);
	fileTable->setAcceptDrops(true);
	fileTable->setDragEnabled(true);
	fileTable->setDropIndicatorShown(true);
	fileTable->setDragDropMode(QAbstractItemView::DragDrop);
	fileTable->setStyleSheet(
		"QTableWidget { background: transparent; border: none; color: #1f1f1f; font-size: 13px; }"
		"QHeaderView::section { background: transparent; color: #7b88a1; border: none; border-bottom: 1px solid #edf1f7; padding: 10px 12px; font-weight: 500; }"
		"QTableWidget::item { border-bottom: 1px solid #f0f3f8; padding: 8px 12px; }"
		"QTableWidget::item:selected { background: #f3f7ff; color: #1f1f1f; }"
	);
	fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	fileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	fileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	fileTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
	fileTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
	fileTable->horizontalHeader()->resizeSection(5, 132);
	fileTable->setColumnWidth(5, 132);
	fileTable->setIconSize(QSize(24, 24));

	FileService* fileService = new FileService(this);
	FolderService* folderService = new FolderService(this);
	auto currentFolderId = std::make_shared<int>(-1);

	auto makeActionButton = [](LineIcon icon, const QString& tipText, QWidget* parent) {
		return new IconActionButton(icon, tipText, parent);
	};

	auto makeMoreButton = [](QWidget* parent) {
		return new IconActionButton(LineIcon::More, "更多操作", parent);
	};

	auto showTodoMenuMessage = [](const QString& actionName) {
		AntMessageManager::instance()->showMessage(AntMessage::Info, actionName + "功能将在下一阶段接入");
	};

	auto chooseTargetFolder = [folderService, fileTable](const QString& title, int currentFolder) {
		QStringList labels;
		QList<int> ids;
		labels << "全部文件";
		ids << -1;

		std::function<void(int, QString)> collectFolders = [&](int parentId, QString prefix) {
			const QList<FolderInfo> folders = folderService->listFolders(AuthService::currentUserId(), parentId);
			for (const FolderInfo& folder : folders) {
				if (folder.folderId == currentFolder) {
					continue;
				}
				labels << (prefix + folder.name);
				ids << folder.folderId;
				collectFolders(folder.folderId, prefix + folder.name + " / ");
			}
		};
		collectFolders(-1, QString());

		bool ok = false;
		const QString selected = QInputDialog::getItem(fileTable, title, "目标位置：", labels, 0, false, &ok);
		if (!ok) {
			return currentFolder;
		}

		const int selectedIndex = labels.indexOf(selected);
		return selectedIndex >= 0 ? ids.at(selectedIndex) : currentFolder;
	};

	auto updateBreadcrumb = [breadcrumbLabel, backFolderBtn, folderService, currentFolderId]() {
		if (!AuthService::isLoggedIn()) {
			breadcrumbLabel->setText("<a href='-1' style='color:#1f1f1f;font-weight:600;text-decoration:none;'>全部文件</a>");
			backFolderBtn->setEnabled(false);
			return;
		}

		QList<QPair<int, QString>> parts;
		parts.append({-1, "全部文件"});
		const QList<FolderInfo> path = folderService->folderPath(AuthService::currentUserId(), *currentFolderId);
		for (const FolderInfo& folder : path) {
			parts.append({folder.folderId, folder.name});
		}
		QStringList htmlParts;
		for (int i = 0; i < parts.size(); ++i) {
			const QString color = (i == parts.size() - 1) ? "#1f1f1f" : "#8a95aa";
			const QString weight = (i == parts.size() - 1) ? "600" : "400";
			htmlParts << QString("<a href='%1' style='color:%2;font-weight:%3;text-decoration:none;'>%4</a>")
				.arg(parts.at(i).first)
				.arg(color, weight, parts.at(i).second.toHtmlEscaped());
		}
		breadcrumbLabel->setText(htmlParts.join("<span style='color:#b8c0cf;'> / </span>"));
		backFolderBtn->setEnabled(*currentFolderId > 0);
	};

	auto refreshFileTable = std::make_shared<std::function<void()>>();

	auto addFolderRow = [fileTable, folderService, currentFolderId, refreshFileTable, makeActionButton, makeMoreButton, showTodoMenuMessage](const FolderInfo& info) {
		const int row = fileTable->rowCount();
		fileTable->insertRow(row);
		fileTable->setRowHeight(row, 56);

		auto* nameItem = new QTableWidgetItem(info.name);
		nameItem->setIcon(makeLineIcon(LineIcon::Folder));
		nameItem->setData(Qt::UserRole, info.folderId);
		nameItem->setData(Qt::UserRole + 1, "folder");
		nameItem->setData(Qt::UserRole + 2, info.localPath);
		fileTable->setItem(row, 0, nameItem);
		fileTable->setItem(row, 1, new QTableWidgetItem("-"));
		fileTable->setItem(row, 2, new QTableWidgetItem("文件夹"));
		fileTable->setItem(row, 3, new QTableWidgetItem("本地目录"));
		fileTable->setItem(row, 4, new QTableWidgetItem(info.updatedAt.isValid() ? info.updatedAt.toString("yyyy-MM-dd HH:mm") : "-"));

		QWidget* actionWidget = new QWidget(fileTable);
		QHBoxLayout* actionLayout = new QHBoxLayout(actionWidget);
		actionLayout->setContentsMargins(0, 0, 0, 0);
		actionLayout->setSpacing(8);

		QPushButton* deleteAction = makeActionButton(LineIcon::Trash, "删除", actionWidget);
		QPushButton* moreAction = makeMoreButton(actionWidget);

		actionLayout->addWidget(deleteAction);
		actionLayout->addWidget(moreAction);
		actionLayout->addStretch();
		fileTable->setCellWidget(row, 5, actionWidget);

		auto renameFolder = [fileTable, actionWidget, folderService, refreshFileTable]() {
			const int row = fileTable->indexAt(actionWidget->pos()).row();
			if (row < 0) {
				return;
			}

			const int folderId = fileTable->item(row, 0)->data(Qt::UserRole).toInt();
			const QString oldName = fileTable->item(row, 0)->text();
			bool ok = false;
			const QString newName = QInputDialog::getText(fileTable, "重命名文件夹", "文件夹名称：", QLineEdit::Normal, oldName, &ok).trimmed();
			if (!ok || newName.isEmpty() || newName == oldName) {
				return;
			}

			if (folderService->renameFolder(AuthService::currentUserId(), folderId, newName)) {
				(*refreshFileTable)();
				AntMessageManager::instance()->showMessage(AntMessage::Success, "文件夹已重命名");
			}
			else {
				AntMessageManager::instance()->showMessage(AntMessage::Error, "重命名失败，请检查是否重名");
			}
		};

		auto deleteFolder = [fileTable, actionWidget, folderService]() {
			const int rowToRemove = fileTable->indexAt(actionWidget->pos()).row();
			if (rowToRemove < 0) {
				return;
			}

			const int folderId = fileTable->item(rowToRemove, 0)->data(Qt::UserRole).toInt();
			if (folderService->trashFolder(AuthService::currentUserId(), folderId)) {
				fileTable->removeRow(rowToRemove);
				AntMessageManager::instance()->showMessage(AntMessage::Success, "文件夹已移入回收站");
			}
			else {
				AntMessageManager::instance()->showMessage(AntMessage::Error, "删除文件夹失败");
			}
		};

		QObject::connect(deleteAction, &QPushButton::clicked, fileTable, deleteFolder);

		QObject::connect(moreAction, &QPushButton::clicked, fileTable, [fileTable, actionWidget, moreAction, renameFolder, showTodoMenuMessage]() {
			QMenu menu(moreAction);
			menu.setStyleSheet(
				"QMenu { background: #ffffff; border: 1px solid #e5e7eb; border-radius: 8px; padding: 8px; }"
				"QMenu::item { min-width: 132px; padding: 8px 18px; color: #1f1f1f; border-radius: 4px; }"
				"QMenu::item:selected { background: #eaf3ff; color: #1677ff; }"
			);
			QAction* moveAction = menu.addAction("移动到");
			QAction* copyAction = menu.addAction("复制到");
			QAction* renameAction = menu.addAction("重命名");
			QAction* revealAction = menu.addAction("在文件管理器中显示");
			menu.addSeparator();
			QAction* newFolderAction = menu.addAction("新建文件夹");
			QAction* detailAction = menu.addAction("详细信息");
			QAction* selectedAction = menu.exec(moreAction->mapToGlobal(QPoint(0, moreAction->height())));
			if (selectedAction == renameAction) {
				renameFolder();
			}
			else if (selectedAction == revealAction) {
				const int row = fileTable->indexAt(actionWidget->pos()).row();
				if (row < 0 || !fileTable->item(row, 0)) {
					return;
				}
				const QString folderPath = fileTable->item(row, 0)->data(Qt::UserRole + 2).toString();
				if (!revealInFileManager(folderPath)) {
					AntMessageManager::instance()->showMessage(AntMessage::Warning, "无法定位文件夹，可能是软件内新建目录或原目录已被移动");
				}
			}
			else if (selectedAction == moveAction || selectedAction == copyAction || selectedAction == newFolderAction || selectedAction == detailAction) {
				showTodoMenuMessage(selectedAction->text());
			}
		});
	};

	auto addFileRow = [fileTable, fileService, currentFolderId, refreshFileTable, chooseTargetFolder, makeActionButton, makeMoreButton, showTodoMenuMessage](const LocalFileInfo& info) {
		const int row = fileTable->rowCount();
		fileTable->insertRow(row);
		fileTable->setRowHeight(row, 56);

		auto* nameItem = new QTableWidgetItem(info.fileName);
		nameItem->setIcon(makeLineIcon(LineIcon::File));
		nameItem->setData(Qt::UserRole, info.fileId);
		nameItem->setData(Qt::UserRole + 1, "file");
		nameItem->setData(Qt::UserRole + 2, info.filePath);
		fileTable->setItem(row, 0, nameItem);
		fileTable->setItem(row, 1, new QTableWidgetItem(formatFileSize(info.fileSize)));
		fileTable->setItem(row, 2, new QTableWidgetItem(info.extension.isEmpty() ? "文件" : info.extension.toUpper()));
		fileTable->setItem(row, 3, new QTableWidgetItem(info.isBackedUp ? "已备份" : "仅路径"));
		fileTable->setItem(row, 4, new QTableWidgetItem(info.modifiedAt.isValid() ? info.modifiedAt.toString("yyyy-MM-dd HH:mm") : "-"));

		QWidget* actionWidget = new QWidget(fileTable);
		QHBoxLayout* actionLayout = new QHBoxLayout(actionWidget);
		actionLayout->setContentsMargins(0, 0, 0, 0);
		actionLayout->setSpacing(8);

		QPushButton* versionAction = makeActionButton(LineIcon::Refresh, "更新版本", actionWidget);
		QPushButton* deleteAction = makeActionButton(LineIcon::Trash, "删除", actionWidget);
		QPushButton* moreAction = makeMoreButton(actionWidget);

		actionLayout->addWidget(versionAction);
		actionLayout->addWidget(deleteAction);
		actionLayout->addWidget(moreAction);
		actionLayout->addStretch();
		fileTable->setCellWidget(row, 5, actionWidget);

		auto deleteFile = [fileTable, actionWidget, fileService]() {
			const int rowToRemove = fileTable->indexAt(actionWidget->pos()).row();
			if (rowToRemove < 0) {
				return;
			}

			const int fileId = fileTable->item(rowToRemove, 0)->data(Qt::UserRole).toInt();
			if (fileService->trashFile(AuthService::currentUserId(), fileId)) {
				fileTable->removeRow(rowToRemove);
				AntMessageManager::instance()->showMessage(AntMessage::Success, "文件已移入回收站");
			}
			else {
				AntMessageManager::instance()->showMessage(AntMessage::Error, "删除文件失败");
			}
		};

		QObject::connect(deleteAction, &QPushButton::clicked, fileTable, deleteFile);

		QObject::connect(versionAction, &QPushButton::clicked, fileTable, [fileTable, actionWidget, fileService]() {
			if (!AuthService::isLoggedIn()) {
				AntMessageManager::instance()->showMessage(AntMessage::Warning, "请先登录后再更新版本");
				return;
			}

			const int row = fileTable->indexAt(actionWidget->pos()).row();
			if (row < 0) {
				return;
			}

			const int fileId = fileTable->item(row, 0)->data(Qt::UserRole).toInt();
			const QString filePath = QFileDialog::getOpenFileName(fileTable, "选择新版本文件");
			if (filePath.isEmpty()) {
				return;
			}

			if (fileService->createNewVersion(AuthService::currentUserId(), fileId, filePath, "用户手动上传新版本", FileService::backupOnAddEnabled())) {
				QFileInfo updatedInfo(filePath);
				fileTable->item(row, 0)->setText(updatedInfo.fileName());
				fileTable->item(row, 1)->setText(formatFileSize(updatedInfo.size()));
				fileTable->item(row, 2)->setText(updatedInfo.suffix().isEmpty() ? "文件" : updatedInfo.suffix().toUpper());
				fileTable->item(row, 3)->setText(FileService::backupOnAddEnabled() ? "已备份" : "仅路径");
				fileTable->item(row, 4)->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"));
				AntMessageManager::instance()->showMessage(AntMessage::Success, "新版本已保存");
			}
			else {
				AntMessageManager::instance()->showMessage(AntMessage::Error, "保存新版本失败");
			}
		});

		QObject::connect(moreAction, &QPushButton::clicked, fileTable, [fileTable, actionWidget, moreAction, fileService, currentFolderId, refreshFileTable, chooseTargetFolder, showTodoMenuMessage]() {
			QMenu menu(moreAction);
			menu.setStyleSheet(
				"QMenu { background: #ffffff; border: 1px solid #e5e7eb; border-radius: 8px; padding: 8px; }"
				"QMenu::item { min-width: 132px; padding: 8px 18px; color: #1f1f1f; border-radius: 4px; }"
				"QMenu::item:selected { background: #eaf3ff; color: #1677ff; }"
			);
			QAction* moveAction = menu.addAction("移动到");
			QAction* copyAction = menu.addAction("复制到");
			QAction* renameAction = menu.addAction("重命名");
			QAction* historyAction = menu.addAction("查看历史版本");
			QAction* revealAction = menu.addAction("在文件管理器中显示");
			menu.addSeparator();
			QAction* detailAction = menu.addAction("详细信息");
			QAction* selectedAction = menu.exec(moreAction->mapToGlobal(QPoint(0, moreAction->height())));
			if (!selectedAction) {
				return;
			}

			const int row = fileTable->indexAt(actionWidget->pos()).row();
			if (row < 0 || !fileTable->item(row, 0)) {
				return;
			}

			const int fileId = fileTable->item(row, 0)->data(Qt::UserRole).toInt();
			if (selectedAction == moveAction) {
				const int targetFolderId = chooseTargetFolder("移动到", *currentFolderId);
				if (targetFolderId == *currentFolderId) {
					return;
				}
				if (fileService->moveFile(AuthService::currentUserId(), fileId, targetFolderId)) {
					(*refreshFileTable)();
					AntMessageManager::instance()->showMessage(AntMessage::Success, "文件已移动");
				}
				else {
					AntMessageManager::instance()->showMessage(AntMessage::Error, "移动失败，请检查目标位置是否已有同名文件");
				}
			}
			else if (selectedAction == copyAction) {
				const int targetFolderId = chooseTargetFolder("复制到", *currentFolderId);
				if (fileService->copyFile(AuthService::currentUserId(), fileId, targetFolderId)) {
					(*refreshFileTable)();
					AntMessageManager::instance()->showMessage(AntMessage::Success, "文件已复制");
				}
				else {
					AntMessageManager::instance()->showMessage(AntMessage::Error, "复制失败");
				}
			}
			else if (selectedAction == revealAction) {
				const QString filePath = fileTable->item(row, 0)->data(Qt::UserRole + 2).toString();
				if (!revealInFileManager(filePath)) {
					AntMessageManager::instance()->showMessage(AntMessage::Warning, "无法定位文件，可能已被移动或删除");
				}
			}
			else if (selectedAction == renameAction || selectedAction == historyAction || selectedAction == detailAction) {
				showTodoMenuMessage(selectedAction->text());
			}
		});
	};

	*refreshFileTable = [fileTable, fileService, folderService, addFolderRow, addFileRow, updateBreadcrumb, currentFolderId]() {
		fileTable->setRowCount(0);
		updateBreadcrumb();
		const QList<FolderInfo> folders = folderService->listFolders(AuthService::currentUserId(), *currentFolderId);
		for (const FolderInfo& folder : folders) {
			addFolderRow(folder);
		}
		const QList<LocalFileInfo> files = fileService->listFiles(AuthService::currentUserId(), *currentFolderId);
		for (const LocalFileInfo& file : files) {
			addFileRow(file);
		}
	};
	if (AuthService::isLoggedIn()) {
		(*refreshFileTable)();
	}

	fileTable->moveFileToFolder = [fileService, refreshFileTable](int fileId, int targetFolderId) {
		if (!AuthService::isLoggedIn()) {
			AntMessageManager::instance()->showMessage(AntMessage::Warning, "请先登录后再移动文件");
			return;
		}

		if (fileService->moveFile(AuthService::currentUserId(), fileId, targetFolderId)) {
			(*refreshFileTable)();
			AntMessageManager::instance()->showMessage(AntMessage::Success, "文件已移动到文件夹");
		}
		else {
			AntMessageManager::instance()->showMessage(AntMessage::Error, "移动失败，请检查目标位置是否已有同名文件");
		}
	};

	auto createFolderFromUi = [this, folderService, currentFolderId, refreshFileTable]() {
		if (!AuthService::isLoggedIn()) {
			AntMessageManager::instance()->showMessage(AntMessage::Warning, "请先登录后再新建文件夹");
			return;
		}

		bool ok = false;
		const QString folderName = QInputDialog::getText(this, "新建文件夹", "文件夹名称：", QLineEdit::Normal, "", &ok).trimmed();
		if (!ok || folderName.isEmpty()) {
			return;
		}

		if (folderService->createFolder(AuthService::currentUserId(), *currentFolderId, folderName)) {
			(*refreshFileTable)();
			AntMessageManager::instance()->showMessage(AntMessage::Success, "文件夹已创建");
		}
		else {
			AntMessageManager::instance()->showMessage(AntMessage::Error, "创建失败，请检查是否重名");
		}
	};

	auto createEmptyFileFromUi = [this, fileService, currentFolderId, refreshFileTable]() {
		if (!AuthService::isLoggedIn()) {
			AntMessageManager::instance()->showMessage(AntMessage::Warning, "请先登录后再新建文件");
			return;
		}

		bool ok = false;
		QString fileName = QInputDialog::getText(this, "新建文件", "文件名：", QLineEdit::Normal, "新建文本文档.txt", &ok).trimmed();
		if (!ok || fileName.isEmpty()) {
			return;
		}

		fileName = safeLocalName(fileName);
		const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/FMS/new-files";
		QDir().mkpath(tempRoot);
		QString tempPath = QDir(tempRoot).filePath(fileName);
		if (QFile::exists(tempPath)) {
			QFileInfo info(fileName);
			tempPath = QDir(tempRoot).filePath(QString("%1_%2.%3")
												  .arg(info.completeBaseName())
												  .arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"))
												  .arg(info.suffix().isEmpty() ? "txt" : info.suffix()));
		}

		QFile file(tempPath);
		if (!file.open(QIODevice::WriteOnly)) {
			AntMessageManager::instance()->showMessage(AntMessage::Error, "新建文件失败，请检查临时目录权限");
			return;
		}
		file.close();

		if (fileService->addLocalFile(AuthService::currentUserId(), *currentFolderId, tempPath, FileService::backupOnAddEnabled())) {
			(*refreshFileTable)();
			AntMessageManager::instance()->showMessage(AntMessage::Success, "文件已创建");
		}
		else {
			AntMessageManager::instance()->showMessage(AntMessage::Error, "新建文件保存失败");
		}
	};

	auto importFolderTree = [fileService, folderService](int ownerId, int parentFolderId, const QString& folderPath, int& folderCount, int& fileCount, int& failCount) {
		std::function<void(int, const QDir&)> importDir = [&](int targetParentId, const QDir& sourceDir) {
			int createdFolderId = -1;
			if (!folderService->createFolder(ownerId, targetParentId, sourceDir.dirName(), &createdFolderId, sourceDir.absolutePath())) {
				++failCount;
				return;
			}
			++folderCount;

			const QFileInfoList childDirs = sourceDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
			for (const QFileInfo& childDir : childDirs) {
				importDir(createdFolderId, QDir(childDir.absoluteFilePath()));
			}

			const QFileInfoList childFiles = sourceDir.entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase);
			for (const QFileInfo& childFile : childFiles) {
				if (fileService->addLocalFile(ownerId, createdFolderId, childFile.absoluteFilePath(), FileService::backupOnAddEnabled())) {
					++fileCount;
				}
				else {
					++failCount;
				}
			}
		};

		QDir rootDir(folderPath);
		if (!rootDir.exists()) {
			++failCount;
			return;
		}
		importDir(parentFolderId, rootDir);
	};

	connect(backFolderBtn, &QPushButton::clicked, this, [folderService, currentFolderId, refreshFileTable]() {
		if (*currentFolderId <= 0) {
			return;
		}

		const QList<FolderInfo> path = folderService->folderPath(AuthService::currentUserId(), *currentFolderId);
		*currentFolderId = path.isEmpty() ? -1 : path.last().parentId;
		(*refreshFileTable)();
	});

	connect(breadcrumbLabel, &QLabel::linkActivated, this, [currentFolderId, refreshFileTable](const QString& link) {
		bool ok = false;
		const int targetFolderId = link.toInt(&ok);
		if (!ok || targetFolderId == *currentFolderId) {
			return;
		}

		*currentFolderId = targetFolderId;
		(*refreshFileTable)();
	});

	connect(newFileBtn, &QPushButton::clicked, this, createEmptyFileFromUi);
	connect(newFolderBtn, &QPushButton::clicked, this, createFolderFromUi);

	connect(fileTable, &QTableWidget::cellClicked, this, [fileTable, currentFolderId, refreshFileTable](int row, int column) {
		if (column == 5) {
			return;
		}
		if (row < 0 || !fileTable->item(row, 0)) {
			return;
		}

		const QString itemType = fileTable->item(row, 0)->data(Qt::UserRole + 1).toString();
		if (itemType == "folder") {
			*currentFolderId = fileTable->item(row, 0)->data(Qt::UserRole).toInt();
			(*refreshFileTable)();
			return;
		}

		if (itemType == "file") {
			const QString filePath = fileTable->item(row, 0)->data(Qt::UserRole + 2).toString();
			if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
				AntMessageManager::instance()->showMessage(AntMessage::Warning, "文件路径不存在，可能已被移动或删除");
				return;
			}
			if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
				AntMessageManager::instance()->showMessage(AntMessage::Error, "无法打开文件，请检查系统默认打开方式");
			}
		}
	});

	QVBoxLayout* settingsLayout = new QVBoxLayout(settingsPage);
	settingsLayout->setContentsMargins(28, 22, 28, 28);
	settingsLayout->setSpacing(18);

	QLabel* settingsTitle = new QLabel("文件设置", settingsPage);
	QFont settingsTitleFont;
	settingsTitleFont.setPointSizeF(15);
	settingsTitleFont.setBold(true);
	settingsTitle->setFont(settingsTitleFont);
	settingsTitle->setStyleSheet("color: #1f1f1f;");

	QWidget* backupSettingRow = new QWidget(settingsPage);
	QHBoxLayout* backupSettingLayout = new QHBoxLayout(backupSettingRow);
	backupSettingLayout->setContentsMargins(0, 0, 0, 0);
	backupSettingLayout->setSpacing(12);

	QVBoxLayout* backupTextLayout = new QVBoxLayout();
	backupTextLayout->setContentsMargins(0, 0, 0, 0);
	backupTextLayout->setSpacing(4);
	QLabel* backupTitle = new QLabel("添加文件时自动备份", backupSettingRow);
	QFont backupTitleFont;
	backupTitleFont.setPointSizeF(12.5);
	backupTitleFont.setBold(true);
	backupTitle->setFont(backupTitleFont);
	backupTitle->setStyleSheet("color: #1f1f1f;");
	QLabel* backupDesc = new QLabel("开启后会复制一份文件到应用存储目录，关闭后只记录原始路径。", backupSettingRow);
	backupDesc->setStyleSheet("color: #6b7280; font-size: 12px;");
	backupTextLayout->addWidget(backupTitle);
	backupTextLayout->addWidget(backupDesc);

	AntToggleButton* backupToggle = new AntToggleButton(QSize(58, 30), backupSettingRow);
	backupToggle->setShowText(true);
	backupToggle->setChecked(FileService::backupOnAddEnabled());
	backupSettingLayout->addLayout(backupTextLayout);
	backupSettingLayout->addStretch();
	backupSettingLayout->addWidget(backupToggle);
	connect(backupToggle, &AntToggleButton::toggled, this, [](bool checked) {
		FileService::setBackupOnAddEnabled(checked);
		AntMessageManager::instance()->showMessage(
			AntMessage::Success,
			checked ? "已开启文件备份模式" : "已切换为仅保存路径模式"
		);
	});

	settingsLayout->addWidget(settingsTitle);
	settingsLayout->addWidget(backupSettingRow);

	QWidget* backupPathRow = new QWidget(settingsPage);
	QHBoxLayout* backupPathLayout = new QHBoxLayout(backupPathRow);
	backupPathLayout->setContentsMargins(0, 0, 0, 0);
	backupPathLayout->setSpacing(10);
	QLabel* backupPathTitle = new QLabel("备份保存位置", backupPathRow);
	backupPathTitle->setMinimumWidth(110);
	backupPathTitle->setStyleSheet("color: #1f1f1f; font-size: 13px;");
	QLineEdit* backupPathEdit = new QLineEdit(FileService::backupRootPath(), backupPathRow);
	backupPathEdit->setPlaceholderText("输入或选择备份目录");
	backupPathEdit->setClearButtonEnabled(true);
	backupPathEdit->setContextMenuPolicy(Qt::CustomContextMenu);
	backupPathEdit->setStyleSheet(
		"QLineEdit { background: #ffffff; border: 1px solid #d9d9d9; border-radius: 6px; padding: 7px 10px; color: #374151; }"
		"QLineEdit:focus { border-color: #1677ff; }"
	);
	QPushButton* chooseBackupPathBtn = new QPushButton("选择目录", backupPathRow);
	chooseBackupPathBtn->setCursor(Qt::PointingHandCursor);
	chooseBackupPathBtn->setStyleSheet(
		"QPushButton { color: #1677ff; background: #ffffff; border: 1px solid #1677ff; border-radius: 6px; padding: 7px 12px; }"
		"QPushButton:hover { color: #0958d9; border-color: #0958d9; }"
	);
	backupPathLayout->addWidget(backupPathTitle);
	backupPathLayout->addWidget(backupPathEdit, 1);
	backupPathLayout->addWidget(chooseBackupPathBtn);

	auto saveBackupPath = [backupPathEdit]() {
		const QString inputPath = backupPathEdit->text().trimmed();
		if (inputPath.isEmpty()) {
			backupPathEdit->setText(FileService::backupRootPath());
			AntMessageManager::instance()->showMessage(AntMessage::Warning, "备份路径不能为空");
			return;
		}
		if (QDir::fromNativeSeparators(inputPath) == FileService::backupRootPath()) {
			return;
		}

		if (!FileService::setBackupRootPath(inputPath)) {
			backupPathEdit->setText(FileService::backupRootPath());
			AntMessageManager::instance()->showMessage(AntMessage::Error, "备份路径保存失败，请检查目录权限");
			return;
		}

		backupPathEdit->setText(FileService::backupRootPath());
		AntMessageManager::instance()->showMessage(AntMessage::Success, "备份路径已更新");
	};

	connect(backupPathEdit, &QLineEdit::editingFinished, this, saveBackupPath);
	connect(backupPathEdit, &QLineEdit::customContextMenuRequested, this, [backupPathEdit](const QPoint& pos) {
		QMenu menu(backupPathEdit);
		menu.setStyleSheet(
			"QMenu { background: #ffffff; border: 1px solid #d9d9d9; border-radius: 6px; padding: 4px; }"
			"QMenu::item { padding: 6px 22px; color: #1f1f1f; }"
			"QMenu::item:selected { background: #eaf3ff; color: #1677ff; }"
			"QMenu::item:disabled { color: #bfbfbf; }"
		);

		QAction* cutAction = menu.addAction("剪切");
		QAction* copyAction = menu.addAction("复制");
		QAction* pasteAction = menu.addAction("粘贴");
		menu.addSeparator();
		QAction* selectAllAction = menu.addAction("全选");
		cutAction->setEnabled(backupPathEdit->hasSelectedText());
		copyAction->setEnabled(backupPathEdit->hasSelectedText());
		pasteAction->setEnabled(true);

		const QAction* selectedAction = menu.exec(backupPathEdit->mapToGlobal(pos));
		if (selectedAction == cutAction) {
			backupPathEdit->cut();
		}
		else if (selectedAction == copyAction) {
			backupPathEdit->copy();
		}
		else if (selectedAction == pasteAction) {
			backupPathEdit->paste();
		}
		else if (selectedAction == selectAllAction) {
			backupPathEdit->selectAll();
		}
	});

	connect(chooseBackupPathBtn, &QPushButton::clicked, this, [backupPathEdit]() {
		const QString selectedDir = QFileDialog::getExistingDirectory(nullptr, "选择备份保存目录", backupPathEdit->text());
		if (selectedDir.isEmpty()) {
			return;
		}

		if (!FileService::setBackupRootPath(selectedDir)) {
			AntMessageManager::instance()->showMessage(AntMessage::Error, "备份路径保存失败，请检查目录权限");
			return;
		}
		backupPathEdit->setText(FileService::backupRootPath());
		AntMessageManager::instance()->showMessage(AntMessage::Success, "备份路径已更新");
	});

	settingsLayout->addWidget(backupPathRow);
	settingsLayout->addStretch();

	QObject::connect(addFileBtn, &QPushButton::clicked, this, [this, addFileBtn, fileService, currentFolderId, refreshFileTable, importFolderTree]() {
		if (!AuthService::isLoggedIn()) {
			AntMessageManager::instance()->showMessage(AntMessage::Warning, "请先登录后再添加内容");
			return;
		}

		QMenu menu(addFileBtn);
		menu.setStyleSheet(
			"QMenu { background: #ffffff; border: 1px solid #d9d9d9; border-radius: 6px; padding: 6px; }"
			"QMenu::item { min-width: 132px; padding: 8px 18px; color: #1f1f1f; border-radius: 4px; }"
			"QMenu::item:selected { background: #eaf3ff; color: #1677ff; }"
		);
		QAction* addFilesAction = menu.addAction("添加文件");
		QAction* addFolderAction = menu.addAction("添加文件夹");
		const QAction* selectedAction = menu.exec(addFileBtn->mapToGlobal(QPoint(0, addFileBtn->height())));
		if (!selectedAction) {
			return;
		}

		int folderCount = 0;
		int fileCount = 0;
		int failCount = 0;

		if (selectedAction == addFilesAction) {
			const QStringList filePaths = QFileDialog::getOpenFileNames(this, "选择文件");
			if (filePaths.isEmpty()) {
				return;
			}

			for (const QString& filePath : filePaths) {
				if (fileService->addLocalFile(AuthService::currentUserId(), *currentFolderId, filePath, FileService::backupOnAddEnabled())) {
					++fileCount;
				}
				else {
					++failCount;
				}
			}
		}
		else if (selectedAction == addFolderAction) {
			const QString folderPath = QFileDialog::getExistingDirectory(this, "选择文件夹");
			if (folderPath.isEmpty()) {
				return;
			}
			if (!confirmLargeFolderImport(this, folderPath)) {
				return;
			}
			importFolderTree(AuthService::currentUserId(), *currentFolderId, folderPath, folderCount, fileCount, failCount);
		}

		(*refreshFileTable)();
		if (failCount == 0) {
			AntMessageManager::instance()->showMessage(AntMessage::Success, QString("已添加 %1 个文件夹，%2 个文件").arg(folderCount).arg(fileCount));
		}
		else if (folderCount > 0 || fileCount > 0) {
			AntMessageManager::instance()->showMessage(AntMessage::Warning, QString("已添加 %1 个文件夹，%2 个文件，%3 项失败").arg(folderCount).arg(fileCount).arg(failCount));
		}
		else {
			AntMessageManager::instance()->showMessage(AntMessage::Error, "添加失败，请检查是否重名、无权限或文件不可读");
		}
	});
	homeLayout->addLayout(toolbarLayout);
	homeLayout->addWidget(breadcrumbLabel);
	homeLayout->addWidget(fileTable, 1);

	stackedWidget->addWidget(homePage);
	stackedWidget->addWidget(settingsPage);
	stackedWidget->addWidget(aboutPage);
	stackedWidget->setCurrentIndex(0);
	// 创建左侧导航按钮。
	const int naviWidth = ui.navi_widget->width();
	const double iconSizeRatio = 0.56;
	const int buttonSize = naviWidth;
	const int iconSize = static_cast<int>(buttonSize * iconSizeRatio);
	CustomToolButton* btnHome = new CustomToolButton(QSize(iconSize, iconSize), ui.navi_widget);
	CustomToolButton* btnSettings = new CustomToolButton(QSize(iconSize, iconSize), ui.navi_widget);
	CustomToolButton* btnAbout = new CustomToolButton(QSize(iconSize, iconSize), ui.navi_widget);
	// 设置导航按钮图标和页面映射关系。
	auto* ins = DesignSystem::instance();
	buttonInfos = {
	{btnHome,ins->btnHomeIconPath(), ins->btnHomeActiveIconPath(), homePage},
	{btnSettings,ins->btnSettingsIconPath() ,ins->btnSettingsActiveIconPath() , settingsPage},
	{btnAbout,ins->btnAboutIconPath() , ins->btnAboutActiveIconPath(), aboutPage}
	};
	buttonInfos[stackedWidget->currentIndex()].button->setBtnChecked(true);
	for (const ButtonInfo& info : buttonInfos)
	{
		CustomToolButton* btn = info.button;
		btn->setSvgIcons(info.normalIcon, info.activeIcon);
		btn->setFixedSize(QSize(naviWidth - 4, naviWidth - 4));
		connect(btn, &QToolButton::clicked, [btn, btnHome, contentLay, this]()
			{
				ui.titleBar->setFixedHeight(m_titleBarHeight);

				// 页面动画期间临时禁用导航按钮，避免重复切换。
				for (ButtonInfo& infos : buttonInfos)
					infos.button->setEnabled(false);

				if (stackedWidget->isAnimationRunning()) return;

				// 更新当前选中的导航按钮和目标页面。
				QWidget* nextPage = nullptr;
				btn->setBtnChecked(true);
				for (ButtonInfo& infos : buttonInfos)
				{
					if (infos.button == btn)
						nextPage = infos.page;
					else
						infos.button->setBtnChecked(false);
				}

				stackedWidget->slideFromBottomToTop(nextPage, 250, [this]()
					{
						for (ButtonInfo& infos : buttonInfos)
							infos.button->setEnabled(true);
					});
			});
	}
	// 创建主题切换按钮。
	QPushButton* themeBtn = new QPushButton(ui.navi_widget);
	themeBtn->setStyleSheet("QPushButton { background-color: transparent; border: none; }");
	themeBtn->setFixedSize(naviWidth * 0.38, naviWidth * 0.38);
	themeBtn->setIcon(DesignSystem::instance()->setThemeIcon());
	themeBtn->setIconSize(themeBtn->size());
	connect(themeBtn, &QPushButton::clicked, this, [this, themeSwitcher, themeBtn]()
		{
			// 用按钮中心点作为主题切换动画的起点。
			themeSwitcher->startSwitchTheme(this->grab(), themeBtn, themeBtn->mapToGlobal(themeBtn->rect().center()));
		});
	connect(this, &AppShell::resized, themeSwitcher, &ThemeSwitcher::resizeByMainWindow);

	// 左侧导航栏布局。
	naviLay->addSpacing(28);
	naviLay->addWidget(avatar, 0, Qt::AlignHCenter);
	naviLay->addSpacing(16);
	naviLay->addWidget(btnHome, 0, Qt::AlignHCenter);
	naviLay->addWidget(btnSettings, 0, Qt::AlignHCenter);
	naviLay->addWidget(btnAbout, 0, Qt::AlignHCenter);
	naviLay->addStretch();
	naviLay->addWidget(themeBtn, 0, Qt::AlignHCenter);
	naviLay->addSpacing(70);

	AntMessageManager::instance();
	AntTooltipManager::instance();

	NotificationManager::instance()->getMainWindow(ui.main_widget);

	connect(this, &AppShell::resized, this, [=](int w, int h)
		{
			NotificationManager::instance()->relayoutNotifications(w, h);
		});

	DialogViewController* mDialog = new DialogViewController(AuthService::isLoggedIn(), this);
	avatar->addDialog(mDialog);
	connect(mDialog, &DialogViewController::successLogin, avatar, &CircularAvatar::allowLogin);
	connect(mDialog, &DialogViewController::successLogin, this, [currentFolderId, refreshFileTable](bool loginState) {
		if (loginState) {
			*currentFolderId = -1;
			(*refreshFileTable)();
		}
	});

	// 标题栏窗口控制按钮。
	connect(btnMax, &QToolButton::clicked, this, [this]()
		{
#ifdef Q_OS_WIN
			HWND hwnd = reinterpret_cast<HWND>(winId());

			// 当前最大化时恢复，否则最大化。
			WINDOWPLACEMENT wp;
			wp.length = sizeof(WINDOWPLACEMENT);
			GetWindowPlacement(hwnd, &wp);

			if (wp.showCmd == SW_MAXIMIZE)
			{
				ShowWindow(hwnd, SW_RESTORE);
			}
			else
			{
				ShowWindow(hwnd, SW_MAXIMIZE);
			}
#endif

#ifdef Q_OS_LINUX
			if (isMaximized()) showNormal();
			else showMaximized();
#endif
		});

	connect(btnMin, &QToolButton::clicked, this, [this]()
		{
#ifdef Q_OS_WIN
			HWND hwnd = reinterpret_cast<HWND>(winId());
			ShowWindow(hwnd, SW_MINIMIZE);
#endif

#ifdef Q_OS_LINUX
			showMinimized();
#endif
		});

	connect(btnClose, &QToolButton::clicked, this, [this]()
		{
			close();
		});

	connect(this, &AppShell::showStandardDialog, mDialog, &DialogViewController::buildStandardDialog);

	connect(this, &AppShell::resized, this, [this](int w, int h)
		{
			DesignSystem::instance()->getDarkMask()->resize(w, h);
		});

	// 主题变化后同步刷新主要控件样式。
	connect(ins, &DesignSystem::themeChanged, this, [=]()
		{
			ui.main_widget->setStyleSheet(StyleSheet::mainQss(DesignSystem::instance()->backgroundColor()));
			ui.navi_widget->setStyleSheet(StyleSheet::naviQss(DesignSystem::instance()->widgetBgColor()));
			ui.titleBar->setStyleSheet(StyleSheet::titleBarQss());
			btnMin->setIcon(DesignSystem::instance()->btnMinIcon());
			btnMax->setIcon(DesignSystem::instance()->btnMaxIcon());
			btnClose->setIcon(DesignSystem::instance()->btnCloseIcon());
			buttonInfos = {
				{btnHome,ins->btnHomeIconPath(), ins->btnHomeActiveIconPath(), homePage},
				{btnSettings,ins->btnSettingsIconPath() ,ins->btnSettingsActiveIconPath() , settingsPage},
				{btnAbout,ins->btnAboutIconPath() , ins->btnAboutActiveIconPath(), aboutPage}
			};
			for (const ButtonInfo& info : buttonInfos)
			{
				CustomToolButton* btn = info.button;
				btn->setSvgIcons(info.normalIcon, info.activeIcon);
			}
		});
}

AppShell::~AppShell()
{
}

void AppShell::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);

	emit resized(width(), height());

	// 保存内容区尺寸，供遮罩和动效组件使用。
	DesignSystem::instance()->setContentSize(QSize(width() - m_naviWidth, height()));
}

bool AppShell::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
	MSG* msg = static_cast<MSG*>(message);

	switch (msg->message) {
	case WM_NCCALCSIZE: {
		// 去掉系统非客户区，让自定义窗口内容占满窗口。
		if (msg->wParam) {
			*result = 0;
			return true;
		}
		break;
	}
	case WM_NCLBUTTONDBLCLK: {
		if (DesignSystem::instance()->getTransparentMask()->isVisible() ||
			DesignSystem::instance()->getDarkMask()->isVisible())
		{
			*result = 0;
			return true;
		}
		break;
	}
	case WM_NCHITTEST: {
		if (DesignSystem::instance()->getTransparentMask()->isVisible() ||
			DesignSystem::instance()->getDarkMask()->isVisible())
		{
			*result = HTCLIENT;
			return true;
		}

		const LONG borderWidth = 8;

		// Windows 原生消息返回的是物理像素。
		RECT winRect;
		GetWindowRect(HWND(winId()), &winRect);

		// 鼠标全局坐标也是物理像素。
		const LONG x = GET_X_LPARAM(msg->lParam);
		const LONG y = GET_Y_LPARAM(msg->lParam);

		const QPoint logicalPos = mapFromGlobal(QPoint(x, y));
		auto hitTitleButton = [this, logicalPos](QWidget* button) {
			return button && button->isVisible() && button->geometry().contains(button->parentWidget()->mapFrom(this, logicalPos));
		};
		if (hitTitleButton(btnMin) || hitTitleButton(btnMax) || hitTitleButton(btnClose) ||
			(antInput && antInput->isVisible() && antInput->geometry().contains(antInput->parentWidget()->mapFrom(this, logicalPos)))) {
			*result = HTCLIENT;
			return true;
		}

		// 根据最小/最大尺寸判断当前方向是否允许缩放。
		const bool canResizeWidth = minimumWidth() != maximumWidth();
		const bool canResizeHeight = minimumHeight() != maximumHeight();

		if (canResizeWidth && canResizeHeight &&
			x >= winRect.left && x < winRect.left + borderWidth &&
			y >= winRect.top && y < winRect.top + borderWidth) {
			*result = HTTOPLEFT;
			return true;
		}
		if (canResizeWidth && canResizeHeight &&
			x >= winRect.right - borderWidth && x < winRect.right &&
			y >= winRect.top && y < winRect.top + borderWidth) {
			*result = HTTOPRIGHT;
			return true;
		}
		if (canResizeWidth && canResizeHeight &&
			x >= winRect.left && x < winRect.left + borderWidth &&
			y >= winRect.bottom - borderWidth && y < winRect.bottom) {
			*result = HTBOTTOMLEFT;
			return true;
		}
		if (canResizeWidth && canResizeHeight &&
			x >= winRect.right - borderWidth && x < winRect.right &&
			y >= winRect.bottom - borderWidth && y < winRect.bottom) {
			*result = HTBOTTOMRIGHT;
			return true;
		}
		// 左边。
		if (canResizeWidth &&
			x >= winRect.left && x < winRect.left + borderWidth) {
			*result = HTLEFT;
			return true;
		}
		// 右边。
		if (canResizeWidth &&
			x >= winRect.right - borderWidth && x < winRect.right) {
			*result = HTRIGHT;
			return true;
		}
		// 上边。
		if (canResizeHeight &&
			y >= winRect.top && y < winRect.top + borderWidth) {
			*result = HTTOP;
			return true;
		}
		// 下边。
		if (canResizeHeight &&
			y >= winRect.bottom - borderWidth && y < winRect.bottom) {
			*result = HTBOTTOM;
			return true;
		}
		int rightBoundary = winRect.right - m_widgetTotalWidthPhysicalPixels;
		if (x > winRect.left + m_titleLeftTotalWidthPhysicalPixels && x < rightBoundary
			&& y > winRect.top && y < winRect.top + m_titleBarHeightPhysicalPixels)
		{
			*result = HTCAPTION;
			return true;
		}
		// 其余区域交给 Qt 默认处理。
		break;
	}
	default:
		break;
	}
#endif // Q_OS_WIN
	return QWidget::nativeEvent(eventType, message, result);
}

void AppShell::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);

#ifdef Q_OS_WIN
	m_hwnd = reinterpret_cast<HWND>(winId());
	if (!m_hwnd) return;

	LONG style = GetWindowLong(m_hwnd, GWL_STYLE);

	style |= WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
	SetWindowLong(m_hwnd, GWL_STYLE, style);

	DWORD cornerPref = 2;
	DwmSetWindowAttribute(m_hwnd, 33, &cornerPref, sizeof(cornerPref));
#endif // Q_OS_WIN
}

void AppShell::moveEvent(QMoveEvent* event)
{
	QWidget::moveEvent(event);
	// 通知悬浮层当前窗口左上角的全局坐标。
	emit windowMoved(this->mapToGlobal(QPoint(0, 0)));
}

void AppShell::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::WindowStateChange)
	{
		QWindowStateChangeEvent* stateEvent = static_cast<QWindowStateChangeEvent*>(event);

		if (isMaximized())
		{
			btnMax->setIcon(DesignSystem::instance()->btnRestoreIcon());
			ui.main_widget->layout()->setContentsMargins(0, 0, 5, 0);
		}
		else if (stateEvent->oldState() & Qt::WindowMaximized)
		{
			btnMax->setIcon(DesignSystem::instance()->btnMaxIcon());
			ui.main_widget->layout()->setContentsMargins(0, 0, 0, 0);
		}
	}

	QWidget::changeEvent(event);
}

#ifdef Q_OS_LINUX

// Linux 下通过事件过滤器更新边缘缩放光标，并在鼠标释放后解除光标锁定。
bool AppShell::eventFilter(QObject* obj, QEvent* event)
{
	if (event->type() == QEvent::HoverMove && !m_isLockCursor)
	{
		QHoverEvent* hoverEvent = static_cast<QHoverEvent*>(event);
		QPoint pos = hoverEvent->position().toPoint();
		updateCursor(pos);
		return true;
	}
	if (event->type() == QEvent::MouseButtonRelease)
	{
		QMouseEvent* me = static_cast<QMouseEvent*>(event);
		m_isLockCursor = false;
		return true;
	}

	return QObject::eventFilter(obj, event);
}

// Linux 下按住窗口边缘启动系统缩放，按住标题栏启动系统移动。
void AppShell::mousePressEvent(QMouseEvent* event)
{
	if (event->button() != Qt::LeftButton) return;

	m_isLockCursor = true;
	updateCursor(event->pos());

	if (currentEdge != Qt::Edges())
	{
		window()->windowHandle()->startSystemResize(currentEdge);
	}
	else if (event->pos().x() > rect().left() + m_naviWidth && event->pos().x() < rect().right()
		&& event->pos().y() > rect().top() && event->pos().y() < m_titleBarHeight)
	{
		window()->windowHandle()->startSystemMove();
	}
}

// Linux 下双击标题栏切换最大化和普通窗口状态。
void AppShell::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton && event->pos().y() > rect().top() && event->pos().y() < m_titleBarHeight)
	{
		if (isMaximized()) showNormal();
		else showMaximized();
	}
}

// 根据鼠标位置判断当前命中的窗口边缘，并切换对应的缩放光标。
void AppShell::updateCursor(const QPoint& pos)
{
	QRectF r = rect();
	Qt::Edges newEdge = Qt::Edges();

	const bool left = pos.x() < r.left() + edgeWidth;
	const bool right = pos.x() > r.right() - edgeWidth;
	const bool top = pos.y() < r.top() + edgeWidth;
	const bool bottom = pos.y() > r.bottom() - edgeWidth;

	if (top && left)        newEdge = Qt::TopEdge | Qt::LeftEdge;
	else if (top && right)  newEdge = Qt::TopEdge | Qt::RightEdge;
	else if (bottom && left) newEdge = Qt::BottomEdge | Qt::LeftEdge;
	else if (bottom && right) newEdge = Qt::BottomEdge | Qt::RightEdge;
	else if (top)    newEdge = Qt::TopEdge;
	else if (bottom) newEdge = Qt::BottomEdge;
	else if (left)   newEdge = Qt::LeftEdge;
	else if (right)  newEdge = Qt::RightEdge;

	// 避免重复 setCursor。
	if (newEdge != currentEdge)
	{
		currentEdge = newEdge;

		switch (newEdge)
		{
		case Qt::TopEdge | Qt::LeftEdge:
		case Qt::BottomEdge | Qt::RightEdge:
			setCursor(Qt::SizeFDiagCursor);
			break;
		case Qt::TopEdge | Qt::RightEdge:
		case Qt::BottomEdge | Qt::LeftEdge:
			setCursor(Qt::SizeBDiagCursor);
			break;
		case Qt::TopEdge:
		case Qt::BottomEdge:
			setCursor(Qt::SizeVerCursor);
			break;
		case Qt::LeftEdge:
		case Qt::RightEdge:
			setCursor(Qt::SizeHorCursor);
			break;
		default:
			setCursor(Qt::ArrowCursor);
			break;
		}
	}
}

#endif


