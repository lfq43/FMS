#include "HomePage.h"
#include <QLabel>
#include <QLayout>
#include "DesignSystem.h"

HomePage::HomePage(QWidget* parent)
	: QWidget(parent)
{
	setObjectName("HomePage");
	setStyleSheet("#HomePage { background-color: transparent; }");

	videoWindow = new FramelessVideoWindow(DesignSystem::instance()->homeVideoFilePath(), this);

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(videoWindow);
}

HomePage::~HomePage()
{
}

void HomePage::showEvent(QShowEvent* event)
{
	// 因为隐藏标题栏，所以需要调整视频窗口大小
	resize(DesignSystem::instance()->contentSize());
}