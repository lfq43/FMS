#pragma once

#include <QWidget>

class QtAntDesign;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QtAntDesign *m_mainShell = nullptr;
};
