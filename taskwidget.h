#ifndef TASKWIDGET_H
#define TASKWIDGET_H

#include <QWidget>

namespace Ui {
class TaskWidget;
}

class TaskWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TaskWidget(QWidget *parent = nullptr);
    ~TaskWidget();

signals:
    void removeRequested(TaskWidget* task);

private slots:
    void on_pushButtonDelete_clicked();

private:
    Ui::TaskWidget *ui;
};

#endif // TASKWIDGET_H