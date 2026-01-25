#ifndef TASKWIDGET_H
#define TASKWIDGET_H

#include <QWidget>
#include <QString>
#include <QStringList>
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
    QString modelName() const;
    bool insertMode() const;
    void setName(const QString &name);
    void setPrompt(const QString &prompt);
    void setModelName(const QString &modelName);
    void setInsertMode(bool insert);

    int maxTokens() const;
    double temperature() const;
    void setMaxTokens(int tokens);
    void setTemperature(double temp);
    void setAvailableModels(const QStringList &models);
    void setRefreshEnabled(bool enabled);

    void setResponseWindowSize(const QSize &size);
    void setResponseZoom(int zoom);

    TaskDefinition toDefinition() const;
    void applyDefinition(const TaskDefinition &definition);

signals:
    void configChanged();
    void refreshModelsRequested();

private:
    Ui::TaskWidget *ui;
    int responseWidth = 600;
    int responseHeight = 200;
    int responseZoomValue = 0;
};

#endif // TASKWIDGET_H
