#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "taskwidget.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>
#include <QLineEdit>
#include <QMetaObject>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow* MainWindow::instance = nullptr;

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

        auto kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        if (!capturing)
            return CallNextHookEx(hookHandle, nCode, wParam, lParam);

        if (wParam == WM_KEYDOWN) {
            int vk = kb->vkCode;
            switch (vk) {
                case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
                    modState |= MB_CTRL; return 1;
                case VK_MENU: case VK_LMENU: case VK_RMENU:
                    modState |= MB_ALT; return 1;
                case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
                    modState |= MB_SHIFT; return 1;
                case VK_LWIN: case VK_RWIN:
                    modState |= MB_WIN; return 1;
                default: {
                    QString seq;
                    if (modState & MB_CTRL)  seq += "Ctrl+";
                    if (modState & MB_ALT)   seq += "Alt+";
                    if (modState & MB_SHIFT) seq += "Shift+";
                    if (modState & MB_WIN)   seq += "Win+";

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
                case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
                    modState &= ~MB_CTRL; break;
                case VK_MENU: case VK_LMENU: case VK_RMENU:
                    modState &= ~MB_ALT; break;
                case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
                    modState &= ~MB_SHIFT; break;
                case VK_LWIN: case VK_RWIN:
                    modState &= ~MB_WIN; break;
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
                GetModuleHandleW(NULL),
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
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , hotkeyCaptured(false)
{
    instance = this;
    ui->setupUi(this);

    connect(ui->lineEditApiEndpoint, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditModelName,   &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditApiKey,      &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditProxy,       &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditHotkey,      &QLineEdit::textChanged, this, &MainWindow::saveConfig);

    ui->lineEditHotkey->installEventFilter(this);

    loadConfig();
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_WIN
    GlobalKeyInterceptor::stop();
#endif
    delete ui;
    instance = nullptr;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
#ifdef Q_OS_WIN
    if (obj == ui->lineEditHotkey) {
        if (ev->type() == QEvent::FocusIn) {
            prevHotkey = ui->lineEditHotkey->text();
            hotkeyCaptured = false;
            GlobalKeyInterceptor::capturing = true;
            GlobalKeyInterceptor::start();
            ui->lineEditHotkey->clear();
        } else if (ev->type() == QEvent::FocusOut) {
            GlobalKeyInterceptor::capturing = false;
            if (!hotkeyCaptured)
                ui->lineEditHotkey->setText(prevHotkey);
        }
    }
#endif
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::setHotkeyText(const QString &text)
{
    ui->lineEditHotkey->setText(text);
    hotkeyCaptured = true;
    saveConfig();
}

QString MainWindow::configFilePath() const
{
    return QCoreApplication::applicationDirPath()
         + QDir::separator()
         + "config.json";
}

void MainWindow::loadConfig()
{
    QString path = configFilePath();
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
        return;

    QJsonObject root = doc.object();
    QJsonObject settings = root.value("settings").toObject();

    ui->lineEditApiEndpoint->setText(settings.value("apiEndpoint").toString());
    ui->lineEditModelName->setText(settings.value("modelName").toString());
    ui->lineEditApiKey->setText(settings.value("apiKey").toString());
    ui->lineEditProxy->setText(settings.value("proxy").toString());
    ui->lineEditHotkey->setText(settings.value("hotkey").toString());

    QJsonArray tasks = root.value("tasks").toArray();
    for (const QJsonValue &value : tasks) {
        QJsonObject obj = value.toObject();
        TaskWidget* task = new TaskWidget;
        task->setName(obj.value("name").toString());
        task->setPrompt(obj.value("prompt").toString());
        task->setInsertMode(obj.value("insert").toBool(true));

        connect(task, &TaskWidget::removeRequested, this, &MainWindow::removeTaskWidget);
        connect(task, &TaskWidget::configChanged,    this, &MainWindow::saveConfig);

        ui->tasksTabWidget->addTab(task,
            tr("Задача %1").arg(ui->tasksTabWidget->count() + 1));
    }
}

void MainWindow::saveConfig()
{
    QJsonObject root;
    QJsonObject settings;
    settings["apiEndpoint"] = ui->lineEditApiEndpoint->text();
    settings["modelName"]   = ui->lineEditModelName->text();
    settings["apiKey"]      = ui->lineEditApiKey->text();
    settings["proxy"]       = ui->lineEditProxy->text();
    settings["hotkey"]      = ui->lineEditHotkey->text();
    root["settings"] = settings;

    QJsonArray tasksArray;
    for (int i = 0; i < ui->tasksTabWidget->count(); ++i) {
        TaskWidget* task = qobject_cast<TaskWidget*>(
            ui->tasksTabWidget->widget(i));
        if (!task) continue;

        QJsonObject obj;
        obj["name"]   = task->name();
        obj["prompt"] = task->prompt();
        obj["insert"] = task->insertMode();
        tasksArray.append(obj);
    }
    root["tasks"] = tasksArray;

    QJsonDocument doc(root);
    QFile file(configFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void MainWindow::on_pushButtonAddTask_clicked()
{
    TaskWidget* task = new TaskWidget;
    connect(task, &TaskWidget::removeRequested, this, &MainWindow::removeTaskWidget);
    connect(task, &TaskWidget::configChanged,   this, &MainWindow::saveConfig);

    int index = ui->tasksTabWidget->addTab(
        task, tr("Задача %1").arg(ui->tasksTabWidget->count() + 1));
    ui->tasksTabWidget->setCurrentIndex(index);

    saveConfig();
}

void MainWindow::removeTaskWidget(TaskWidget* task)
{
    int index = ui->tasksTabWidget->indexOf(task);
    if (index != -1) {
        QWidget* page = ui->tasksTabWidget->widget(index);
        ui->tasksTabWidget->removeTab(index);
        page->deleteLater();
        saveConfig();
    }
}