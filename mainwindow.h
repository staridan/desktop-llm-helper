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
#include <QStringList>

#include "hotkeymanager.h"
#include "configstore.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TaskWidget;
class TaskWindow;
class QNetworkAccessManager;

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
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void handleTaskTabClicked(int index);
    void handleTaskTabMoved(int from, int to);
    void requestCloseTask(int index);
    void removeTaskWidget(TaskWidget *task);
    void updateTaskResponsePrefs(int taskIndex, const QSize &size, int zoom);
    void commitTaskResponsePrefs();
    void requestModelList();

private:
    Ui::MainWindow *ui;
    QString prevHotkey;
    bool hotkeyCaptured;
    HotkeyManager *hotkeyManager;
    bool loadingConfig;
    QSystemTrayIcon *trayIcon;
    QPointer<TaskWindow> menuWindow;
    QNetworkAccessManager *modelNetworkManager;
    QStringList availableModels;

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
    int addTabIndex() const;
    bool isAddTabIndex(int index) const;
    void ensureAddTab();
    void ensureAddTabLast();
    void applyModelList(const QStringList &models);
    void updateModelCombos(const QString &defaultModel);
    void setModelRefreshEnabled(bool enabled);
    QString currentDefaultModel() const;
};

#endif // MAINWINDOW_H
