#include "taskwindow.h"
#include "taskwidget.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QKeyEvent>
#include <QCursor>
#include <QScreen>
#include <QGuiApplication>
#include <QEvent>

TaskWindow::TaskWindow(const QList<TaskWidget*>& tasks, QWidget* parent)
    : QWidget(parent,
              Qt::Window
            | Qt::WindowStaysOnTopHint
            | Qt::CustomizeWindowHint
            | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMinimumWidth(200);
    setFocusPolicy(Qt::StrongFocus);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

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

    QPoint cursorPos = QCursor::pos();
    QScreen* screen = QGuiApplication::screenAt(cursorPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    QRect available = screen->availableGeometry();
    int x = cursorPos.x();
    int y = cursorPos.y();
    int w = width();
    int h = height();
    if (x + w > available.x() + available.width()) {
        x = available.x() + available.width() - w;
    }
    if (y + h > available.y() + available.height()) {
        y = available.y() + available.height() - h;
    }
    if (x < available.x()) {
        x = available.x();
    }
    if (y < available.y()) {
        y = available.y();
    }
    move(x, y);

    show();
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);
}

void TaskWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::ActivationChange && !isActiveWindow()) {
        close();
    }
    QWidget::changeEvent(event);
}

void TaskWindow::keyPressEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(ev);
}