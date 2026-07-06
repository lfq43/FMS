#pragma once

#include <QWidget>
#include <QEvent>
#include "MaterialTabWidget.h"
#include "AntScrollArea.h"
#include "CarouselWidget.h"
#include "SkeletonWidget.h"
#include "AntProfileTable.h"
#include "TabContentWidget.h"
#include "AntTabWidgetContainer.h"

class FunctionPage : public QWidget
{
	Q_OBJECT

public:
	FunctionPage(QWidget* parent);
	~FunctionPage();
protected:
	void resizeEvent(QResizeEvent* event) override;
private:
	void initViewPage();
signals:
	void resized(int w, int h);				// 用于通知其他组件调整大小
	void windowMoved(QPoint globalPos);		// 窗口移动时发出信号
private:
	MaterialTabWidget* tabWidget = nullptr;
	AntScrollArea* scrollArea1 = nullptr;
	AntScrollArea* scrollArea2 = nullptr;
	CarouselWidget* carousel = nullptr;
	QList<SkeletonWidget*> skeletons;
	QVector<AntProfileTable::TableColumnItems> rowItems;
	AntProfileTable* table = nullptr;
	QStandardItemModel* tableModel = nullptr;
	AntTabWidgetContainer* antTabContainer = nullptr;
	QWidget* container = nullptr;
};
