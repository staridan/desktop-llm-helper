#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "taskwidget.h"
#include "taskwindow.h"
#include "hotkeymanager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>
#include <QLineEdit>
#include <QMetaObject>
#include <QStandardPaths>
#include <QTabBar>
#include <QMenu>
#include <QAction>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QCloseEvent>
#include <QApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow *MainWindow::instance = nullptr;

#ifdef Q_OS_WIN
class GlobalKeyInterceptor {
public:
    static bool capturing;
    static HHOOK hookHandle;

    enum ModBits { MB_CTRL = 1, MB_ALT = 2, MB_SHIFT = 4, MB_WIN = 8 };

    static int modState;

    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode < 0)
            return CallNextHookEx(hookHandle, nCode, wParam, lParam);
        auto kb = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if (!capturing)
            return CallNextHookEx(hookHandle, nCode, wParam, lParam);
        if (wParam == WM_KEYDOWN) {
            int vk = kb->vkCode;
            switch (vk) {
                case VK_CONTROL:
                case VK_LCONTROL:
                case VK_RCONTROL:
                    modState |= MB_CTRL;
                    return 1;
                case VK_MENU:
                case VK_LMENU:
                case VK_RMENU:
                    modState |= MB_ALT;
                    return 1;
                case VK_SHIFT:
                case VK_LSHIFT:
                case VK_RSHIFT:
                    modState |= MB_SHIFT;
                    return 1;
                case VK_LWIN:
                case VK_RWIN:
                    modState |= MB_WIN;
                    return 1;
                default: {
                    QString seq;
                    if (modState & MB_CTRL) seq += "Ctrl+";
                    if (modState & MB_ALT) seq += "Alt+";
                    if (modState & MB_SHIFT) seq += "Shift+";
                    if (modState & MB_WIN) seq += "Win+";

                    wchar_t name[64] = {0};
                    if (GetKeyNameTextW(
                            MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16,
                            name, 64) > 0) {
                        seq += QString::fromWCharArray(name);
                    } else {
                        seq += QString::number(vk);
                    }

                    if (MainWindow::instance) {
                        QMetaObject::invokeMethod(
                            MainWindow::instance,
                            "setHotkeyText",
                            Qt::QueuedConnection,
                            Q_ARG(QString, seq)
                        );
                    }
                    modState = 0;
                    return 1;
                }
            }
        } else if (wParam == WM_KEYUP) {
            int vk = kb->vkCode;
            switch (vk) {
                case VK_CONTROL:
                case VK_LCONTROL:
                case VK_RCONTROL:
                    modState &= ~MB_CTRL;
                    break;
                case VK_MENU:
                case VK_LMENU:
                case VK_RMENU:
                    modState &= ~MB_ALT;
                    break;
                case VK_SHIFT:
                case VK_LSHIFT:
                case VK_RSHIFT:
                    modState &= ~MB_SHIFT;
                    break;
                case VK_LWIN:
                case VK_RWIN:
                    modState &= ~MB_WIN;
                    break;
                default: break;
            }
        }
        return CallNextHookEx(hookHandle, nCode, wParam, lParam);
    }

    static void start() {
        if (!hookHandle) {
            hookHandle = SetWindowsHookExW(
                WH_KEYBOARD_LL,
                hookProc,
                GetModuleHandleW(nullptr),
                0
            );
        }
    }

    static void stop() {
        if (hookHandle) {
            UnhookWindowsHookEx(hookHandle);
            hookHandle = nullptr;
        }
    }
};

bool GlobalKeyInterceptor::capturing = false;
HHOOK GlobalKeyInterceptor::hookHandle = nullptr;
int GlobalKeyInterceptor::modState = 0;
#endif  // Q_OS_WIN

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
      , ui(new Ui::MainWindow)
      , hotkeyCaptured(false)
      , hotkeyManager(new HotkeyManager(this))
      , loadingConfig(false) {
    instance = this;
    ui->setupUi(this);
    // Include application name in the window title
    setWindowTitle(QCoreApplication::applicationName() + " - " + tr("Settings"));

    ui->tasksTabWidget->setMovable(true);
    connect(ui->tasksTabWidget->tabBar(), &QTabBar::tabMoved,
            this, &MainWindow::saveConfig);

    connect(ui->lineEditApiEndpoint, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditModelName, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditApiKey, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditProxy, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditHotkey, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditMaxChars, &QLineEdit::textChanged, this, &MainWindow::saveConfig);

    ui->lineEditHotkey->installEventFilter(this);

    createTrayIcon();

#ifdef Q_OS_WIN
    connect(hotkeyManager, &HotkeyManager::hotkeyPressed,
            this, &MainWindow::handleGlobalHotkey);
#endif

    loadConfig();
    hotkeyManager->registerHotkey(ui->lineEditHotkey->text());
}

MainWindow::~MainWindow() {
#ifdef Q_OS_WIN
    GlobalKeyInterceptor::stop();
#endif
    delete ui;
    instance = nullptr;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev) {
#ifdef Q_OS_WIN
    if (obj == ui->lineEditHotkey) {
        if (ev->type() == QEvent::FocusIn) {
            prevHotkey = ui->lineEditHotkey->text();
            hotkeyCaptured = false;
            GlobalKeyInterceptor::capturing = true;
            GlobalKeyInterceptor::start();
        } else if (ev->type() == QEvent::FocusOut) {
            GlobalKeyInterceptor::capturing = false;
            if (!hotkeyCaptured)
                ui->lineEditHotkey->setText(prevHotkey);
        }
    }
#endif
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    event->ignore();
    hide();
}

void MainWindow::setHotkeyText(const QString &text) {
    ui->lineEditHotkey->setText(text);
    hotkeyCaptured = true;
    saveConfig();
}

QString MainWindow::configFilePath() const {
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                              + QDir::separator() + QCoreApplication::applicationName();
    QDir().mkpath(configDir);
    return configDir + QDir::separator() + "config.json";
}

void MainWindow::applyDefaultSettings() {
    ui->lineEditApiEndpoint->setText("https://api.openai.com/v1/chat/completions");
    ui->lineEditModelName->setText("gpt-4.1-mini");
    ui->lineEditApiKey->setText("YOUR_API_KEY_HERE");
    ui->lineEditProxy->setText("");
    ui->lineEditHotkey->setText("Win+Z");
    ui->lineEditMaxChars->setText(QString::number(1000));

    auto *task = new TaskWidget;
    task->setName("🧠 Explane");
    task->setPrompt("Explain the meaning of what will be written. The explanation should be brief, in 1-4 sentences.");
    task->setInsertMode(false);
    task->setMaxTokens(300);
    task->setTemperature(0.5);
    connect(task, &TaskWidget::removeRequested, this, &MainWindow::removeTaskWidget);
    connect(task, &TaskWidget::configChanged, this, &MainWindow::saveConfig);
    QString label = task->name();
    ui->tasksTabWidget->addTab(task, label);
}

void MainWindow::loadConfig() {
    const QString path = configFilePath();
    QFile file(path);
    if (!file.exists()) {
        applyDefaultSettings();
        saveConfig();
        return;
    }
    if (!file.open(QIODevice::ReadOnly))
        return;

    loadingConfig = true;
    const QByteArray data = file.readAll();
    file.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        loadingConfig = false;
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonObject settings = root.value("settings").toObject();

    ui->lineEditApiEndpoint->setText(settings.value("apiEndpoint").toString());
    ui->lineEditModelName->setText(settings.value("modelName").toString());
    ui->lineEditApiKey->setText(settings.value("apiKey").toString());
    ui->lineEditProxy->setText(settings.value("proxy").toString());
    ui->lineEditHotkey->setText(settings.value("hotkey").toString());
    ui->lineEditMaxChars->setText(QString::number(settings.value("maxChars").toInt()));

    const QJsonArray tasks = root.value("tasks").toArray();
    for (const QJsonValue &value: tasks) {
        const QJsonObject obj = value.toObject();
        auto *task = new TaskWidget;
        task->setName(obj.value("name").toString());
        task->setPrompt(obj.value("prompt").toString());
        task->setInsertMode(obj.value("insert").toBool(true));
        task->setMaxTokens(obj.value("maxTokens").toInt(300));
        task->setTemperature(obj.value("temperature").toDouble(0.5));

        connect(task, &TaskWidget::removeRequested, this, &MainWindow::removeTaskWidget);
        connect(task, &TaskWidget::configChanged, this, &MainWindow::saveConfig);
        connect(task, &TaskWidget::configChanged, this, [this, task]() {
            int idx = ui->tasksTabWidget->indexOf(task);
            if (idx != -1) {
                QString label = task->name().isEmpty() ? tr("<Unnamed>") : task->name();
                ui->tasksTabWidget->setTabText(idx, label);
            }
        });

        QString tabLabel = task->name().isEmpty() ? tr("<Unnamed>") : task->name();
        ui->tasksTabWidget->addTab(task, tabLabel);
    }

    loadingConfig = false;
}

void MainWindow::saveConfig() {
    if (loadingConfig)
        return;

    QJsonObject settings{
        {"apiEndpoint", ui->lineEditApiEndpoint->text()},
        {"modelName", ui->lineEditModelName->text()},
        {"apiKey", ui->lineEditApiKey->text()},
        {"proxy", ui->lineEditProxy->text()},
        {"hotkey", ui->lineEditHotkey->text()},
        {"maxChars", ui->lineEditMaxChars->text().toInt()}
    };

    QJsonArray tasksArray;
    for (int i = 0; i < ui->tasksTabWidget->count(); ++i) {
        if (auto *task = qobject_cast<TaskWidget *>(ui->tasksTabWidget->widget(i))) {
            QJsonObject taskObj{
                {"name", task->name()},
                {"prompt", task->prompt()},
                {"insert", task->insertMode()},
                {"maxTokens", task->maxTokens()},
                {"temperature", task->temperature()}
            };
            tasksArray.append(taskObj);
        }
    }

    QJsonObject root{
        {"settings", settings},
        {"tasks", tasksArray}
    };

    QJsonDocument doc(root);
    QFile file(configFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson());
        file.close();
    }

    hotkeyManager->registerHotkey(ui->lineEditHotkey->text());
}

void MainWindow::on_pushButtonAddTask_clicked() {
    auto *task = new TaskWidget;
    connect(task, &TaskWidget::removeRequested, this, &MainWindow::removeTaskWidget);
    connect(task, &TaskWidget::configChanged, this, &MainWindow::saveConfig);
    connect(task, &TaskWidget::configChanged, this, [this, task]() {
        int idx = ui->tasksTabWidget->indexOf(task);
        if (idx != -1) {
            QString lbl = task->name().isEmpty() ? tr("<Unnamed>") : task->name();
            ui->tasksTabWidget->setTabText(idx, lbl);
        }
    });

    QString tabLabel = task->name().isEmpty() ? tr("<Unnamed>") : task->name();
    int index = ui->tasksTabWidget->addTab(task, tabLabel);
    ui->tasksTabWidget->setCurrentIndex(index);

    saveConfig();
}

void MainWindow::removeTaskWidget(TaskWidget *task) {
    int index = ui->tasksTabWidget->indexOf(task);
    if (index != -1) {
        QWidget *page = ui->tasksTabWidget->widget(index);
        ui->tasksTabWidget->removeTab(index);
        page->deleteLater();
        saveConfig();
    }
}

QList<TaskWidget *> MainWindow::currentTasks() const {
    QList<TaskWidget *> list;
    for (int i = 0; i < ui->tasksTabWidget->count(); ++i) {
        if (auto *task = qobject_cast<TaskWidget *>(ui->tasksTabWidget->widget(i)))
            list.append(task);
    }
    return list;
}

void MainWindow::handleGlobalHotkey() {
    new TaskWindow(currentTasks());
}

void MainWindow::createTrayIcon() {
    QMenu *trayMenu = new QMenu(this);
    QAction *restoreAction = trayMenu->addAction(tr("Settings"));
    QAction *quitAction = trayMenu->addAction(tr("Exit"));

    connect(restoreAction, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icons/app.png"));
    trayIcon->setToolTip(QCoreApplication::applicationName());
    trayIcon->setContextMenu(trayMenu);
    connect(trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);
    trayIcon->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger ||
        reason == QSystemTrayIcon::DoubleClick) {
        showNormal();
        raise();
        activateWindow();
    }
}
