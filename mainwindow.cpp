#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "taskwidget.h"
#include <QScrollBar>
#include <QLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->scrollAreaTasks->setFrameShape(QFrame::NoFrame);
    ui->scrollAreaTasks->viewport()->setAutoFillBackground(false);
    // ui->scrollAreaTasks->setStyleSheet("background: transparent; border: none;");

    ui->scrollAreaTasks->verticalScrollBar()->installEventFilter(this);
    adjustScrollPadding();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButtonAddTask_clicked()
{
    TaskWidget* task = new TaskWidget;
    connect(task, &TaskWidget::removeRequested, this, &MainWindow::removeTaskWidget);
    ui->verticalLayoutScroll->addWidget(task);
}

void MainWindow::removeTaskWidget(TaskWidget* task)
{
    ui->verticalLayoutScroll->removeWidget(task);
    task->deleteLater();
}

void MainWindow::adjustScrollPadding()
{
    bool scrollbarVisible = ui->scrollAreaTasks->verticalScrollBar()->isVisible();
    int rightPadding = scrollbarVisible ? 10 : 0;
    ui->verticalLayoutScroll->setContentsMargins(0, 0, rightPadding, 0);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->scrollAreaTasks->verticalScrollBar()
        && (event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
        adjustScrollPadding();
    }
    return QMainWindow::eventFilter(obj, event);
}