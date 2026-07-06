#include "MaterialTabWidget.h"
#include <QPainter>
#include "StyleSheet.h"
#include <QTimer>
#include <QVBoxLayout>
#include "DesignSystem.h"

MaterialTabBar::MaterialTabBar(QWidget* parent)
	:QTabBar(parent), m_indicatorPosX(0), m_indicatorWidth(0)
{
	auto theme = DesignSystem::instance()->currentTheme();
	setStyleSheet(StyleSheet::hTabQss(theme.primaryColor, theme.tabTextColor));
	setDrawBase(false);
	setExpanding(false);
	QFont font;
	font.setPointSizeF(11.5);
	setFont(font);
	m_animation = new QPropertyAnimation(this, "indicatorPos", this);
	m_animation->setDuration(250);
	m_animation->setEasingCurve(QEasingCurve::OutCubic);

	connect(this, &QTabBar::currentChanged, this, [this, font](int index)
		{
			QRect rect = tabRect(index);
			QString text = tabText(index);
			QFontMetrics fm(font);
			int textWidth = fm.horizontalAdvance(text);
			m_indicatorWidth = textWidth;

			int targetX = rect.x() + (rect.width() - textWidth) / 2;

			// 如果只有一个标签，直接设置位置，不用动画
			if (count() == 1)
			{
				m_indicatorPosX = targetX;
				update();
				return;
			}

			// 多个标签时才使用动画
			m_animation->stop();
			m_animation->setStartValue(m_indicatorPosX);
			m_animation->setEndValue(targetX);
			m_animation->start();
		});

	connect(DesignSystem::instance(), &DesignSystem::themeChanged, this, [this]()
		{
			auto theme = DesignSystem::instance()->currentTheme();
			setStyleSheet(StyleSheet::hTabQss(theme.primaryColor, theme.tabTextColor));
		});
}

MaterialTabBar::~MaterialTabBar()
{
}

void MaterialTabBar::paintEvent(QPaintEvent* event)
{
	QTabBar::paintEvent(event);
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	painter.setBrush(DesignSystem::instance()->primaryColor()); // 蓝色指示器
	QRect rect(m_indicatorPosX, height() - 3, m_indicatorWidth, 3);
	painter.drawRect(rect);
}

QSize MaterialTabBar::tabSizeHint(int index) const
{
	QString text = tabText(index);
	QFontMetrics fm(font());
	int textWidth = fm.horizontalAdvance(text);
	int padding = 40; // 左右 padding
	return QSize(textWidth + padding, 60); // 高度可调
}

void MaterialTabBar::updateIndicatorPosition(int index)
{
	QRect rect = tabRect(index);
	QFontMetrics fm(font());
	QString text = tabText(index);
	int textWidth = fm.horizontalAdvance(text);
	m_indicatorWidth = textWidth;
	m_indicatorPosX = rect.x() + (rect.width() - textWidth) / 2;
}

void MaterialTabBar::resizeEvent(QResizeEvent* event)
{
	QTabBar::resizeEvent(event);

	// 布局改变后更新指示器位置
	if (count() > 0)
	{
		updateIndicatorPosition(currentIndex());
		update();
	}
}

MaterialTabWidget::MaterialTabWidget(QWidget* parent)
	: QWidget(parent)
{
	m_tabBar = new MaterialTabBar(this);
	m_stackedWidget = new SlideStackedWidget(this);

	layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(m_tabBar);
	layout->addWidget(m_stackedWidget);

	connect(m_tabBar, &QTabBar::currentChanged, this, &MaterialTabWidget::onTabClicked);

	m_currentIndex = 0;
}

MaterialTabWidget::~MaterialTabWidget()
{
}

void MaterialTabWidget::setCurrentIndex(int index)
{
	if (index < 0 || index >= m_stackedWidget->count())
		return;

	m_tabBar->setCurrentIndex(index);
	m_stackedWidget->setCurrentIndex(index);
	m_currentIndex = index;
	emit itemIndexChanged(index);
}

void MaterialTabWidget::onTabClicked(int index)
{
	if (m_isAnimating || index == m_currentIndex)
		return;

	bool forward = index > m_currentIndex;

	QWidget* nextWidget = m_stackedWidget->widget(index);

	m_isAnimating = true;

	m_stackedWidget->slideToPage(nextWidget,
		forward ? SlideStackedWidget::RightToLeft : SlideStackedWidget::LeftToRight,
		m_animationDuration, SlideStackedWidget::OutCubic, [this, index]()
		{
			m_currentIndex = index;
			m_tabBar->setCurrentIndex(index);
			m_isAnimating = false;
			emit itemIndexChanged(index);
		});
}

void MaterialTabWidget::addTab(QWidget* wid, QString tabName)
{
	m_stackedWidget->addWidget(wid);
	m_tabBar->addTab(tabName);
}

QWidget* MaterialTabWidget::getWidget(int index)
{
	return m_stackedWidget->widget(index);
}

QVBoxLayout* MaterialTabWidget::getLayout()
{
	return layout;
}