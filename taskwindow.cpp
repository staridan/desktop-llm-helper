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
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QLabel>
#include <QFont>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

TaskWindow::TaskWindow(const QList<TaskWidget *> &tasks, QWidget *parent)
    : QWidget(parent,
              Qt::Window | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint
              | Qt::FramelessWindowHint)
      , loadingWindow(nullptr)
      , loadingTimer(nullptr)
      , loadingLabel(nullptr)
      , animationTimer(nullptr)
      , dotCount(0) {
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    QPushButton *firstButton = nullptr;

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(0);

    auto *container = new QWidget(this);
    container->setObjectName("container");
    container->setStyleSheet("QWidget#container { "
                             "  background-color: white; "
                             "  border-radius: 0; "
                             "}");

    auto *shadow = new QGraphicsDropShadowEffect(container);
    shadow->setBlurRadius(10);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 0);
    container->setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    for (TaskWidget *task: tasks) {
        if (!task)
            continue;
        QString text = task->name().isEmpty() ? tr("<Unnamed>") : task->name();
        auto *btn = new QPushButton(text, container);
        btn->setStyleSheet(
            "QPushButton {"
            "   text-align: left;"
            "   padding: 2px 8px;"
            "   border: 1px solid #adadad;"
            "   background-color: #e1e1e1;"
            "}"
            "QPushButton:focus {"
            "   outline: 0;"
            "   border: 1px solid #0078d7;"
            "   background-color: #e5f1fb;"
            "}"
        );
        btn->setMouseTracking(true);
        btn->installEventFilter(this);
        if (!firstButton)
            firstButton = btn;
        connect(btn, &QPushButton::clicked, this, [this, task]() {
            this->hide();
            this->showLoadingIndicator();

            QClipboard *clipboard = QGuiApplication::clipboard();
            QString original;

#ifdef Q_OS_WIN
            clipboard->clear(QClipboard::Clipboard);
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

            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 200) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QString text = clipboard->text();
                if (!text.isEmpty()) {
                    original = text;
                    break;
                }
            }
            if (original.isEmpty())
                return;
#else
            original = clipboard->text();
#endif

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

            int maxChars = settings.value("maxChars").toInt(0);
            QString sendText = original;
            if (maxChars > 0 && sendText.length() > maxChars)
                sendText = sendText.left(maxChars);

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
            userMessage["content"] = sendText;
            QJsonArray messagesArray;
            messagesArray.append(systemMessage);
            messagesArray.append(userMessage);

            QJsonObject body;
            body["model"] = modelName;
            body["messages"] = messagesArray;
            body["max_tokens"] = task->maxTokens();
            body["temperature"] = task->temperature();
            QJsonDocument bodyDoc(body);

            manager->post(request, bodyDoc.toJson());
            connect(manager, &QNetworkAccessManager::finished, this,
                    [this, task](QNetworkReply *reply) {
                        this->hideLoadingIndicator();
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
                            QFont font = edit->font();
                            font.setPointSize(12);
                            edit->setFont(font);
                            edit->setPlainText(result);
                            edit->setReadOnly(true);
                            lay->addWidget(edit);

                            respWin->setWindowTitle(tr("LLM Response"));
                            respWin->resize(600, 200);

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
                    });
        });
        layout->addWidget(btn);
    }

    mainLayout->addWidget(container);

    adjustSize();

    const int btnSize = 16;
    auto *closeBtn = new QPushButton(QString::fromUtf8("×"), this);
    closeBtn->setFixedSize(btnSize, btnSize);
    closeBtn->setFlat(true);
    closeBtn->setStyleSheet("QPushButton { "
                            "   background-color: #FFFFFF; "
                            "   border: 1px solid #BBBBBB; "
                            "   border-radius: 8px; "
                            "   padding-bottom: 2px; "
                            "} "
                            "QPushButton:hover, QPushButton:focus { "
                            "   background-color: #e81123; "
                            "   color: #FFFFFF; "
                            "   outline: 0; "
                            "}");
    int xBtn = width() - mainLayout->contentsMargins().right() - btnSize / 2;
    int yBtn = mainLayout->contentsMargins().top() - btnSize / 2;
    closeBtn->move(xBtn, yBtn);
    closeBtn->raise();
    connect(closeBtn, &QPushButton::clicked, this, &TaskWindow::close);

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
    if (firstButton)
        firstButton->setFocus(Qt::TabFocusReason);
}

bool TaskWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Enter) {
        if (auto *btn = qobject_cast<QPushButton *>(watched)) {
            btn->setFocus(Qt::MouseFocusReason);
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TaskWindow::showLoadingIndicator() {
    if (loadingWindow)
        return;
    loadingWindow = new QWidget(nullptr,
                                Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    loadingWindow->setAttribute(Qt::WA_ShowWithoutActivating);
    loadingWindow->setAttribute(Qt::WA_TransparentForMouseEvents);
    loadingWindow->setFocusPolicy(Qt::NoFocus);

    QLabel *label = new QLabel(tr("Loading"), loadingWindow);
    loadingLabel = label;
    dotCount = 0;

    QVBoxLayout *lay = new QVBoxLayout(loadingWindow);
    lay->setContentsMargins(5, 5, 5, 5);
    lay->addWidget(label);
    loadingWindow->setLayout(lay);

    loadingWindow->adjustSize();
    updateLoadingPosition();
    loadingWindow->show();

    loadingTimer = new QTimer(this);
    connect(loadingTimer, &QTimer::timeout,
            this, &TaskWindow::updateLoadingPosition);
    loadingTimer->start(16);

    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout,
            this, &TaskWindow::animateLoadingText);
    animationTimer->start(500);
}

void TaskWindow::hideLoadingIndicator() {
    if (loadingTimer) {
        loadingTimer->stop();
        loadingTimer->deleteLater();
        loadingTimer = nullptr;
    }
    if (animationTimer) {
        animationTimer->stop();
        animationTimer->deleteLater();
        animationTimer = nullptr;
    }
    if (loadingWindow) {
        loadingWindow->close();
        loadingWindow->deleteLater();
        loadingWindow = nullptr;
        loadingLabel = nullptr;
    }
}

void TaskWindow::updateLoadingPosition() {
    if (!loadingWindow)
        return;
    QPoint pos = QCursor::pos();
    loadingWindow->move(pos.x() + 10, pos.y() + 10);
}

void TaskWindow::animateLoadingText() {
    if (!loadingLabel)
        return;
    dotCount = (dotCount + 1) % 4;
    loadingLabel->setText(tr("Loading") + QString(dotCount, '.'));
    if (loadingWindow) {
        loadingWindow->adjustSize();
        updateLoadingPosition();
    }
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