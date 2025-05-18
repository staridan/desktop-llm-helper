#include "taskwindow.h"
#include "taskwidget.h"

#include <QColor>
#include <QCursor>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScreen>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QTextEdit>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

TaskWindow::TaskWindow(const QList<TaskWidget *> &tasks, QWidget *parent)
    : QWidget(parent,
              Qt::Window | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint
              | Qt::FramelessWindowHint) {
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(0);

    auto *container = new QWidget(this);
    container->setObjectName("container");
    container->setStyleSheet("QWidget#container { "
        "  background-color: white; "
        "  border-radius: 0px; "
        "}");

    auto *shadow = new QGraphicsDropShadowEffect(container);
    shadow->setBlurRadius(10);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 0);
    container->setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);

    for (TaskWidget *task: tasks) {
        if (!task)
            continue;
        QString text = task->name().isEmpty() ? tr("<Без имени>") : task->name();
        auto *btn = new QPushButton(text, container);
        connect(btn, &QPushButton::clicked, this, [this, task]() {
            this->hide();

#ifdef Q_OS_WIN
            INPUT copyInputs[4] = {};
            copyInputs[0].type = INPUT_KEYBOARD;
            copyInputs[0].ki.wVk = VK_CONTROL;
            copyInputs[1].type = INPUT_KEYBOARD;
            copyInputs[1].ki.wVk = 'C';
            copyInputs[2].type = INPUT_KEYBOARD;
            copyInputs[2].ki.wVk = 'C';
            copyInputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
            copyInputs[3].type = INPUT_KEYBOARD;
            copyInputs[3].ki.wVk = VK_CONTROL;
            copyInputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(4, copyInputs, sizeof(INPUT));
#endif

            QClipboard *clipboard = QGuiApplication::clipboard();
            QString original = clipboard->text();
            QString prompt = task->prompt();
            QString combined = prompt + original;
            clipboard->setText(combined);

            QString configPath = QCoreApplication::applicationDirPath()
                                 + QDir::separator() + "config.json";
            QFile configFile(configPath);
            if (!configFile.open(QIODevice::ReadOnly))
                return;
            QJsonDocument configDoc = QJsonDocument::fromJson(configFile.readAll());
            configFile.close();
            QJsonObject settings = configDoc.object().value("settings").toObject();
            QString apiEndpoint = settings.value("apiEndpoint").toString();
            QString modelName = settings.value("modelName").toString();
            QString apiKey = settings.value("apiKey").toString();
            QString proxyStr = settings.value("proxy").toString();

            auto *manager = new QNetworkAccessManager(this);
            if (!proxyStr.isEmpty()) {
                QUrl proxyUrl(proxyStr);
                if (proxyUrl.isValid()) {
                    QNetworkProxy proxy;
                    proxy.setType(QNetworkProxy::HttpProxy);
                    proxy.setHostName(proxyUrl.host());
                    proxy.setPort(proxyUrl.port());
                    manager->setProxy(proxy);
                }
            }

            QUrl url(apiEndpoint);
            QNetworkRequest request(url);
            request.setHeader(QNetworkRequest::ContentTypeHeader,
                              "application/json");
            request.setRawHeader("Authorization",
                                 "Bearer " + apiKey.toUtf8());

            QJsonObject systemMessage;
            systemMessage["role"] = "system";
            systemMessage["content"] = prompt;
            QJsonObject userMessage;
            userMessage["role"] = "user";
            userMessage["content"] = original;
            QJsonArray messagesArray;
            messagesArray.append(systemMessage);
            messagesArray.append(userMessage);
            QJsonObject body;
            body["model"] = modelName;
            body["messages"] = messagesArray;
            QJsonDocument bodyDoc(body);

            manager->post(request, bodyDoc.toJson());
            connect(manager, &QNetworkAccessManager::finished, this,
                    [this, task](QNetworkReply *reply) {
                        QByteArray respData = reply->readAll();
                        reply->deleteLater();
                        QJsonDocument respDoc = QJsonDocument::fromJson(respData);
                        if (!respDoc.isObject())
                            return;
                        QJsonObject respObj = respDoc.object();
                        QJsonArray choices = respObj.value("choices").toArray();
                        if (choices.isEmpty())
                            return;
                        QJsonObject msg = choices.first().toObject()
                                .value("message")
                                .toObject();
                        QString result = msg.value("content").toString();

                        if (task->insertMode()) {
#ifdef Q_OS_WIN
                            QClipboard *clipboard =
                                    QGuiApplication::clipboard();
                            clipboard->setText(result);

                            INPUT pasteInputs[4] = {};
                            pasteInputs[0].type = INPUT_KEYBOARD;
                            pasteInputs[0].ki.wVk = VK_CONTROL;
                            pasteInputs[1].type = INPUT_KEYBOARD;
                            pasteInputs[1].ki.wVk = 'V';
                            pasteInputs[2].type = INPUT_KEYBOARD;
                            pasteInputs[2].ki.wVk = 'V';
                            pasteInputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
                            pasteInputs[3].type = INPUT_KEYBOARD;
                            pasteInputs[3].ki.wVk = VK_CONTROL;
                            pasteInputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
                            SendInput(4, pasteInputs, sizeof(INPUT));
#endif
                        } else {
                            auto *respWin = new QWidget;
                            respWin->setAttribute(Qt::WA_DeleteOnClose, true);

                            auto *lay = new QVBoxLayout(respWin);
                            auto *edit = new QTextEdit(respWin);
                            edit->setPlainText(result);
                            edit->setReadOnly(true);
                            lay->addWidget(edit);

                            respWin->setWindowTitle(tr("Ответ LLM"));
                            respWin->resize(600, 400);

                            QPoint cursorPos = QCursor::pos();
                            QScreen *screen = QGuiApplication::screenAt(cursorPos);
                            if (!screen)
                                screen = QGuiApplication::primaryScreen();
                            QRect available = screen->availableGeometry();
                            int x = cursorPos.x();
                            int y = cursorPos.y();
                            int w = respWin->width();
                            int h = respWin->height();
                            if (x + w > available.x() + available.width())
                                x = available.x() + available.width() - w;
                            if (y + h > available.y() + available.height())
                                y = available.y() + available.height() - h;
                            if (x < available.x())
                                x = available.x();
                            if (y < available.y())
                                y = available.y();
                            respWin->move(x, y);

                            respWin->show();
                            respWin->raise();
                            respWin->activateWindow();
                        }
                        this->deleteLater();
                    });
        });
        layout->addWidget(btn);
    }

    mainLayout->addWidget(container);

    adjustSize();

    QPoint cursorPos = QCursor::pos();
    QScreen *screen = QGuiApplication::screenAt(cursorPos);
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

void TaskWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::ActivationChange && !isActiveWindow()) {
        close();
    }
    QWidget::changeEvent(event);
}

void TaskWindow::keyPressEvent(QKeyEvent *ev) {
    if (ev->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(ev);
}