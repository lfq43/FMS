#include "MainWindow.h"

#include <QVBoxLayout>

#include "QtAntDesign.h"

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("文枢");

    m_mainShell = new QtAntDesign(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_mainShell);

    resize(m_mainShell->size());
}
