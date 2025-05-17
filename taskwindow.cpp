#include "taskwindow.h"
#include "taskwidget.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QKeyEvent>

TaskWindow::TaskWindow(const QList<TaskWidget*>& tasks, QWidget* parent)
    : QWidget(parent,
              Qt::Window
              | Qt::WindowStaysOnTopHint
              | Qt::CustomizeWindowHint
              | Qt::WindowTitleHint)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMinimumWidth(400);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    for (TaskWidget* task : tasks) {
        if (!task) continue;
        QString text = task->name().isEmpty()
                       ? tr("<Без имени>")
                       : task->name();
        auto* btn = new QPushButton(text, this);
        connect(btn, &QPushButton::clicked, this, [this, task]() {
            close();
        });
        layout->addWidget(btn);
    }

    setLayout(layout);
    adjustSize();

    show();
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);
}

void TaskWindow::keyPressEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(ev);
}