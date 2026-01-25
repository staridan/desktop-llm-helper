#ifndef TASKWIDGET_H
#define TASKWIDGET_H

#include <QWidget>
#include <QString>
#include <QSize>

namespace Ui {
    class TaskWidget;
}

struct TaskDefinition;

class TaskWidget : public QWidget {
    Q_OBJECT

public:
    explicit TaskWidget(QWidget *parent = nullptr);
    ~TaskWidget();

    QString name() const;
    QString prompt() const;
    bool insertMode() const;
    void setName(const QString &name);
    void setPrompt(const QString &prompt);
    void setInsertMode(bool insert);

    int maxTokens() const;
    double temperature() const;
    void setMaxTokens(int tokens);
    void setTemperature(double temp);

    void setResponseWindowSize(const QSize &size);
    void setResponseZoom(int zoom);

    TaskDefinition toDefinition() const;
    void applyDefinition(const TaskDefinition &definition);

signals:
    void removeRequested(TaskWidget *task);
    void configChanged();

private slots:
    void on_pushButtonDelete_clicked();

private:
    Ui::TaskWidget *ui;
    int responseWidth = 600;
    int responseHeight = 200;
    int responseZoomValue = 0;
};

#endif // TASKWIDGET_H
