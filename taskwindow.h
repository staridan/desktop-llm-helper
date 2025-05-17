#ifndef TASKWINDOW_H
#define TASKWINDOW_H

#include <QWidget>
#include <QList>
#include <QEvent>
#include <QKeyEvent>

class TaskWidget;


class TaskWindow : public QWidget
{
    Q_OBJECT
public:
    explicit TaskWindow(const QList<TaskWidget*>& tasks, QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* ev) override;
    void changeEvent(QEvent* event) override;

};

#endif // TASKWINDOW_H