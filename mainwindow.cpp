#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "taskwidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
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