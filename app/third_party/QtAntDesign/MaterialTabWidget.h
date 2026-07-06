#pragma once

#include <QTabBar>
#include <QTabWidget>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QVBoxLayout>
#include "SlideStackedWidget.h"

class MaterialTabBar :public QTabBar
{
	Q_OBJECT
		Q_PROPERTY(int indicatorPos READ indicatorPos WRITE setIndicatorPos)
public:
	MaterialTabBar(QWidget* parent = nullptr);
	~MaterialTabBar();

	int indicatorPos() const { return m_indicatorPosX; }
	void setIndicatorPos(int pos)
	{
		m_indicatorPosX = pos;
		update();
	}
protected:
	void paintEvent(QPaintEvent* event) override;
	QSize tabSizeHint(int index) const override;
	void resizeEvent(QResizeEvent* event);
private:
	void updateIndicatorPosition(int index);
	int m_indicatorPosX;
	int m_indicatorWidth;
	QPropertyAnimation* m_animation;
};

class MaterialTabWidget : public QWidget
{
	Q_OBJECT
public:
	explicit MaterialTabWidget(QWidget* parent = nullptr);
	~MaterialTabWidget();

	MaterialTabBar* tabBar() const { return m_tabBar; }
	SlideStackedWidget* stackedWidget() const { return m_stackedWidget; }

	void setAnimationDuration(int msec) { m_animationDuration = msec; }
	int animationDuration() const { return m_animationDuration; }

	void addTab(QWidget* wid, QString tabName);
	QWidget* getWidget(int index);
	QVBoxLayout* getLayout();
public slots:
	void setCurrentIndex(int index);
signals:
	void itemIndexChanged(int index);

private slots:
	void onTabClicked(int index);

private:
	MaterialTabBar* m_tabBar;
	SlideStackedWidget* m_stackedWidget;
	QVBoxLayout* layout;

	int m_animationDuration = 300;
	bool m_isAnimating = false;

	int m_currentIndex = 0;
};
