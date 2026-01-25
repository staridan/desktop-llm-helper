#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QEvent>
#include <QList>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QPointer>
#include <QSize>

#include "hotkeymanager.h"
#include "configstore.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TaskWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    static QPointer<MainWindow> instance;

public slots:
    void setHotkeyText(const QString &text);
    void handleGlobalHotkey();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_pushButtonAddTask_clicked();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void removeTaskWidget(TaskWidget *task);
    void updateTaskResponsePrefs(int taskIndex, const QSize &size, int zoom);
    void commitTaskResponsePrefs();

private:
    Ui::MainWindow *ui;
    QString prevHotkey;
    bool hotkeyCaptured;
    HotkeyManager *hotkeyManager;
    bool loadingConfig;
    QSystemTrayIcon *trayIcon;

    void createTrayIcon();
    void loadConfig();
    void saveConfig();
    void applyDefaultSettings();
    void applyConfig(const AppConfig &config);
    AppConfig buildConfigFromUi() const;
    QList<TaskDefinition> currentTaskDefinitions() const;
    void addTaskTab(const TaskDefinition &definition, bool makeCurrent);
    void connectTaskSignals(TaskWidget *task);
    void updateTaskTabTitle(TaskWidget *task);
    void clearTasks();
};

#endif // MAINWINDOW_H
