#include "taskwidget.h"
#include "ui_taskwidget.h"

TaskWidget::TaskWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TaskWidget)
{
    ui->setupUi(this);
    ui->radioInsert->setChecked(true);
}

TaskWidget::~TaskWidget()
{
    delete ui;
}

void TaskWidget::on_pushButtonDelete_clicked()
{
    emit removeRequested(this);
}