#include "taskwindow.h"

#include <QClipboard>
#include <QAbstractTextDocumentLayout>
#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QDialog>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QStyle>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextCursor>
#include <QTextFragment>
#include <QTextLayout>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QPlainTextEdit>

#include <functional>

#include <windows.h>

namespace {
constexpr const char kDefaultModelLabel[] = "Default";

QUrl buildApiUrl(const QString &baseUrl, const QString &pathSuffix) {
    QUrl url(baseUrl.trimmed());
    if (!url.isValid())
        return QUrl();
    QString path = url.path();
    if (!path.endsWith('/'))
        path += '/';
    QString suffix = pathSuffix;
    if (suffix.startsWith('/'))
        suffix.remove(0, 1);
    url.setPath(path + suffix);
    return url;
}

QString normalizeModelName(const QString &name) {
    if (name == QLatin1String(kDefaultModelLabel))
        return QString();
    return name;
}

bool isCodeBlock(const QTextBlock &block) {
    const QTextBlockFormat format = block.blockFormat();
    if (format.hasProperty(QTextFormat::BlockCodeFence))
        return true;
    if (format.hasProperty(QTextFormat::BlockCodeLanguage))
        return true;
    return block.charFormat().fontFixedPitch();
}

bool isInlineCodeFormat(const QTextCharFormat &format) {
    if (format.fontFixedPitch())
        return true;
    const QFont font = format.font();
    if (font.fixedPitch())
        return true;
    const QString family = font.family();
    if (family.contains("mono", Qt::CaseInsensitive)
        || family.contains("courier", Qt::CaseInsensitive)
        || family.contains("consolas", Qt::CaseInsensitive)) {
        return true;
    }
    const QStringList families = format.fontFamilies().toStringList();
    for (const QString &entry : families) {
        if (entry.contains("mono", Qt::CaseInsensitive)
            || entry.contains("courier", Qt::CaseInsensitive)
            || entry.contains("consolas", Qt::CaseInsensitive)) {
            return true;
        }
    }
    const QVariant hintProp = format.property(QTextFormat::FontStyleHint);
    if (hintProp.isValid()) {
        const int hint = hintProp.toInt();
        if (hint == QFont::TypeWriter || hint == QFont::Monospace)
            return true;
    }
    const QVariant familyProp = format.property(QTextFormat::FontFamily);
    if (familyProp.isValid()) {
        const QString propFamily = familyProp.toString();
        if (propFamily.contains("mono", Qt::CaseInsensitive)
            || propFamily.contains("courier", Qt::CaseInsensitive)
            || propFamily.contains("consolas", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QFont resolveBaseTextFont(QTextDocument *doc) {
    if (!doc)
        return QFont();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (isCodeBlock(block))
            continue;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            const QTextCharFormat format = fragment.charFormat();
            if (isInlineCodeFormat(format))
                continue;
            const QFont font = format.font();
            if (font.pointSizeF() > 0 || font.pixelSize() > 0)
                return font;
        }
    }
    return doc->defaultFont();
}

class MarkdownTextBrowser : public QTextBrowser {
public:
    explicit MarkdownTextBrowser(QWidget *parent = nullptr)
        : QTextBrowser(parent) {}

    void setZoomCallback(std::function<void()> callback) {
        zoomCallback = std::move(callback);
    }

    void setZoomDeltaCallback(std::function<void(int)> callback) {
        zoomDeltaCallback = std::move(callback);
    }

protected:
    void wheelEvent(QWheelEvent *event) override {
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            int delta = event->angleDelta().y();
            if (delta == 0)
                delta = event->pixelDelta().y();
            if (delta == 0) {
                event->accept();
                return;
            }
            int steps = qAbs(delta) / 120;
            if (steps == 0)
                steps = 1;
            if (delta > 0) {
                zoomIn(steps);
                if (zoomDeltaCallback)
                    zoomDeltaCallback(steps);
            } else {
                zoomOut(steps);
                if (zoomDeltaCallback)
                    zoomDeltaCallback(-steps);
            }
            if (zoomCallback)
                zoomCallback();
            event->accept();
            return;
        }
        QTextBrowser::wheelEvent(event);
    }

private:
    std::function<void()> zoomCallback;
    std::function<void(int)> zoomDeltaCallback;
};

class MarkdownCodeHighlighter : public QSyntaxHighlighter {
public:
    explicit MarkdownCodeHighlighter(QTextDocument *parent)
        : QSyntaxHighlighter(parent) {
        keywordFormat.setForeground(QColor("#d73a49"));
        keywordFormat.setFontWeight(QFont::Bold);

        stringFormat.setForeground(QColor("#032f62"));

        numberFormat.setForeground(QColor("#005cc5"));

        commentFormat.setForeground(QColor("#6a737d"));

        keywordPatterns = {
            QRegularExpression("\\b(auto|bool|break|case|catch|class|const|continue|def|default|"
                               "delete|do|else|enum|export|extends|false|final|finally|for|"
                               "foreach|from|function|if|implements|import|inline|interface|"
                               "lambda|let|namespace|new|nullptr|null|operator|private|protected|"
                               "public|return|static|struct|switch|template|this|throw|true|try|"
                               "typedef|typename|using|var|virtual|void|volatile|while)\\b")
        };

        stringPatterns = {
            QRegularExpression(R"("([^"\\]|\\.)*")"),
            QRegularExpression(R"('([^'\\]|\\.)*')")
        };

        numberPattern = QRegularExpression("\\b\\d+(?:\\.\\d+)?\\b");
        singleLineCommentPatterns = {
            QRegularExpression("//[^\\n]*"),
            QRegularExpression("#[^\\n]*")
        };
        multiLineCommentStart = QRegularExpression("/\\*");
        multiLineCommentEnd = QRegularExpression("\\*/");
    }

protected:
    void highlightBlock(const QString &text) override {
        setCurrentBlockState(0);
        if (!isCodeBlock(currentBlock()))
            return;

        for (const QRegularExpression &pattern : keywordPatterns) {
            auto it = pattern.globalMatch(text);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), keywordFormat);
            }
        }

        for (const QRegularExpression &pattern : stringPatterns) {
            auto it = pattern.globalMatch(text);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), stringFormat);
            }
        }

        auto numberIt = numberPattern.globalMatch(text);
        while (numberIt.hasNext()) {
            const QRegularExpressionMatch match = numberIt.next();
            setFormat(match.capturedStart(), match.capturedLength(), numberFormat);
        }

        for (const QRegularExpression &pattern : singleLineCommentPatterns) {
            auto it = pattern.globalMatch(text);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), commentFormat);
            }
        }

        int startIndex = 0;
        if (previousBlockState() == 1) {
            startIndex = 0;
        } else {
            const QRegularExpressionMatch match = multiLineCommentStart.match(text);
            startIndex = match.hasMatch() ? match.capturedStart() : -1;
        }

        while (startIndex >= 0) {
            const QRegularExpressionMatch endMatch = multiLineCommentEnd.match(text, startIndex);
            int commentLength = 0;
            if (endMatch.hasMatch()) {
                commentLength = endMatch.capturedEnd() - startIndex;
                setFormat(startIndex, commentLength, commentFormat);
            } else {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
                setFormat(startIndex, commentLength, commentFormat);
            }
            const QRegularExpressionMatch nextStart = multiLineCommentStart.match(text,
                                                                                  startIndex + commentLength);
            startIndex = nextStart.hasMatch() ? nextStart.capturedStart() : -1;
        }
    }

private:
    QTextCharFormat keywordFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat commentFormat;
    QList<QRegularExpression> keywordPatterns;
    QList<QRegularExpression> stringPatterns;
    QRegularExpression numberPattern;
    QList<QRegularExpression> singleLineCommentPatterns;
    QRegularExpression multiLineCommentStart;
    QRegularExpression multiLineCommentEnd;
};

QPoint clampToScreen(const QPoint &pos, const QSize &size, const QRect &available) {
    int x = pos.x();
    int y = pos.y();
    const int maxX = available.x() + available.width() - size.width();
    const int maxY = available.y() + available.height() - size.height();
    if (x > maxX)
        x = maxX;
    if (y > maxY)
        y = maxY;
    if (x < available.x())
        x = available.x();
    if (y < available.y())
        y = available.y();
    return QPoint(x, y);
}

void moveNearCursor(QWidget *widget, const QPoint &cursorPos) {
    QScreen *screen = QGuiApplication::screenAt(cursorPos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect available = screen->availableGeometry();
    widget->move(clampToScreen(cursorPos, widget->size(), available));
}

const QString &markdownCss() {
    static const QString css =
        "body {"
        "  font-family: 'Segoe UI', 'Noto Sans', Helvetica, Arial;"
        "  font-size: 12pt;"
        "  color: #24292f;"
        "}"
        "a { color: #0969da; text-decoration: none; }"
        "a:hover { text-decoration: underline; }"
        "p { margin: 8px 0; }"
        "h1 { font-size: 20pt; border-bottom: 1px solid #d0d7de; padding-bottom: 4px; }"
        "h2 { font-size: 16pt; border-bottom: 1px solid #d0d7de; padding-bottom: 2px; }"
        "h3 { font-size: 14pt; }"
        "ul, ol { margin-left: 20px; }"
        "pre { border: 1px solid #d0d7de; padding: 8px; margin: 12px 0; }"
        "blockquote {"
        "  color: #24292f;"
        "  border-left: 4px solid #9ec5fe;"
        "  background-color: #f2f7ff;"
        "  margin: 8px 0;"
        "  padding: 6px 10px;"
        "  border-radius: 4px;"
        "}"
        "table { border-collapse: collapse; }"
        "th, td { border: 1px solid #d0d7de; padding: 4px 8px; }"
        "hr { border: 0; border-top: 1px solid #d0d7de; margin: 12px 0; }";
    return css;
}
}

TaskWindow *TaskWindow::s_activeMenu = nullptr;
HHOOK TaskWindow::s_keyboardHook = nullptr;
HHOOK TaskWindow::s_mouseHook = nullptr;

TaskWindow::TaskWindow(const QList<TaskDefinition> &taskList,
                       const AppSettings &settings,
                       QWidget *parent)
    : QWidget(parent,
              Qt::Tool | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint
              | Qt::FramelessWindowHint)
    , tasks(taskList)
    , activeTaskIndex(-1)
    , settings(settings)
    , networkManager(new QNetworkAccessManager(this))
    , loadingWindow(nullptr)
    , loadingTimer(nullptr)
    , loadingLabel(nullptr)
    , animationTimer(nullptr)
    , dotCount(0)
    , responseWindow(nullptr)
    , responseView(nullptr)
    , followUpInput(nullptr)
    , sawStreamFormat(false)
    , requestInFlight(false)
    , menuActiveIndex(-1) {
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);

    if (!this->settings.proxy.isEmpty()) {
        QUrl proxyUrl(this->settings.proxy);
        if (proxyUrl.isValid()) {
            QNetworkProxy proxy;
            proxy.setType(QNetworkProxy::HttpProxy);
            proxy.setHostName(proxyUrl.host());
            proxy.setPort(proxyUrl.port());
            networkManager->setProxy(proxy);
        }
    }

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

    for (int i = 0; i < tasks.size(); ++i) {
        const TaskDefinition &task = tasks.at(i);
        QString text = task.name.isEmpty() ? tr("<Unnamed>") : task.name;
        auto *btn = new QPushButton(text, container);
        btn->setStyleSheet(
            "QPushButton {"
            "   text-align: left;"
            "   padding: 2px 8px;"
            "   border: 1px solid #adadad;"
            "   background-color: #e1e1e1;"
            "}"
            "QPushButton:focus, QPushButton:hover, QPushButton[menuActive=\"true\"] {"
            "   outline: 0;"
            "   border: 1px solid #0078d7;"
            "   background-color: #e5f1fb;"
            "}"
        );
        btn->setProperty("menuIndex", i);
        btn->setMouseTracking(true);
        btn->installEventFilter(this);
        menuButtons.append(btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            activeTaskIndex = i;
            hide();
            showLoadingIndicator();

            const QString original = captureSelectedText();
            if (original.isEmpty()) {
                hideLoadingIndicator();
                return;
            }

            QClipboard *clipboard = QGuiApplication::clipboard();
            const TaskDefinition task = tasks.at(i);
            clipboard->setText(task.prompt + original);

            startConversation(task, original);
        });
        layout->addWidget(btn);
    }

    mainLayout->addWidget(container);

    adjustSize();

    const QPoint cursorPos = QCursor::pos();
    moveNearCursor(this, cursorPos);

    applyNoActivateStyle();
    show();
    raise();
}

TaskWindow::~TaskWindow() {
    removeMenuHooks();
    hideLoadingIndicator();
}

QString TaskWindow::captureSelectedText() {
    QClipboard *clipboard = QGuiApplication::clipboard();
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
        const QString text = clipboard->text();
        if (!text.isEmpty())
            return text;
    }
    return QString();
}

QString TaskWindow::applyCharLimit(const QString &text) const {
    if (settings.maxChars > 0 && text.length() > settings.maxChars)
        return text.left(settings.maxChars);
    return text;
}

void TaskWindow::startConversation(const TaskDefinition &task, const QString &originalText) {
    resetConversationState();
    appendMessageToHistory("system", task.prompt);
    appendMessageToHistory("user", applyCharLimit(originalText));
    sendRequestWithHistory(task);
}

void TaskWindow::sendRequestWithHistory(const TaskDefinition &task) {
    resetRequestState();
    setRequestInFlight(true);

    const QUrl requestUrl = buildApiUrl(settings.apiEndpoint, "chat/completions");
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + settings.apiKey.toUtf8());

    QJsonArray messagesArray;
    for (const ChatMessage &msg : messageHistory) {
        QJsonObject item;
        item["role"] = msg.role;
        item["content"] = msg.content;
        messagesArray.append(item);
    }

    QJsonObject body;
    const QString modelName = task.modelName.isEmpty()
        ? normalizeModelName(settings.modelName)
        : normalizeModelName(task.modelName);
    if (!modelName.isEmpty())
        body["model"] = modelName;
    body["messages"] = messagesArray;
    body["max_tokens"] = task.maxTokens;
    body["temperature"] = task.temperature;
    if (!task.insertMode)
        body["stream"] = true;
    QJsonDocument bodyDoc(body);

    QNetworkReply *reply = networkManager->post(request, bodyDoc.toJson());
    connect(reply, &QNetworkReply::readyRead, this, [this, task, reply]() {
        handleReplyReadyRead(task, reply);
    });
    connect(reply, &QNetworkReply::finished, this, [this, task, reply]() {
        handleReplyFinished(task, reply);
    });
}

void TaskWindow::sendFollowUpMessage() {
    if (requestInFlight || !followUpInput)
        return;
    if (activeTaskIndex < 0 || activeTaskIndex >= tasks.size())
        return;

    const QString rawText = followUpInput->toPlainText();
    const QString trimmed = rawText.trimmed();
    if (trimmed.isEmpty())
        return;

    const QString sendText = applyCharLimit(trimmed);
    appendMessageToHistory("user", sendText);
    appendTranscriptBlock(formatUserMessageBlock(sendText));
    followUpInput->clear();
    updateResponseView();

    showLoadingIndicator();
    sendRequestWithHistory(tasks.at(activeTaskIndex));
}

void TaskWindow::handleReplyReadyRead(const TaskDefinition &task, QNetworkReply *reply) {
    const QByteArray chunk = reply->readAll();
    if (chunk.isEmpty())
        return;

    responseBody.append(chunk);
    streamBuffer.append(chunk);

    bool appended = false;
    while (true) {
        const int lineEnd = streamBuffer.indexOf('\n');
        if (lineEnd < 0)
            break;
        const QByteArray line = streamBuffer.left(lineEnd);
        streamBuffer.remove(0, lineEnd + 1);
        const QString delta = parseStreamDelta(line);
        if (!delta.isEmpty()) {
            pendingResponseText += delta;
            appended = true;
        }
    }

    if (sawStreamFormat) {
        hideLoadingIndicator();
        if (!task.insertMode)
            ensureResponseWindow();
    }

    if (appended && !task.insertMode)
        updateResponseView();
}

void TaskWindow::handleReplyFinished(const TaskDefinition &task, QNetworkReply *reply) {
    hideLoadingIndicator();

    if (!streamBuffer.isEmpty()) {
        const QString delta = parseStreamDelta(streamBuffer);
        if (!delta.isEmpty())
            pendingResponseText += delta;
        streamBuffer.clear();
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString errStr = reply->errorString();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("LLM request failed (%1): HTTP status %2")
                                  .arg(errStr)
                                  .arg(statusCode));
        reply->deleteLater();
        setRequestInFlight(false);
        return;
    }

    if (!sawStreamFormat && pendingResponseText.isEmpty()) {
        pendingResponseText = extractResponseTextFromJson(responseBody);
    }

    reply->deleteLater();

    if (!pendingResponseText.isEmpty())
        appendMessageToHistory("assistant", pendingResponseText);

    if (task.insertMode) {
        if (!pendingResponseText.isEmpty())
            insertResponse(pendingResponseText);
        pendingResponseText.clear();
        setRequestInFlight(false);
        return;
    }

    if (responseWindow || !pendingResponseText.isEmpty()) {
        ensureResponseWindow();
        if (!pendingResponseText.isEmpty()) {
            appendTranscriptBlock(pendingResponseText);
            pendingResponseText.clear();
        }
        updateResponseView();
    }
    setRequestInFlight(false);
}

void TaskWindow::insertResponse(const QString &text) {
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);

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
}

void TaskWindow::ensureResponseWindow() {
    if (responseWindow)
        return;

    responseWindow = new QDialog(this);
    responseWindow->setAttribute(Qt::WA_DeleteOnClose, true);
    responseWindow->setWindowFlags(responseWindow->windowFlags() | Qt::Dialog);
    responseWindow->installEventFilter(this);

    auto *lay = new QVBoxLayout(responseWindow);
    auto *view = new MarkdownTextBrowser(responseWindow);
    responseView = view;
    QFont font = responseView->font();
    font.setFamilies(QStringList{"Segoe UI", "Noto Sans", "Helvetica", "Arial"});
    font.setPointSize(12);
    responseView->setFont(font);
    responseView->document()->setDefaultFont(font);
    responseView->setReadOnly(true);
    responseView->setOpenExternalLinks(true);
    responseView->document()->setDefaultStyleSheet(markdownCss());
    responseView->document()->setDocumentMargin(8);
    responseView->setStyleSheet("QTextBrowser { background-color: #ffffff; }");
    new MarkdownCodeHighlighter(responseView->document());
    view->setZoomCallback([this]() {
        applyMarkdownStyles();
    });
    view->setZoomDeltaCallback([this](int steps) {
        handleResponseZoomDelta(steps);
    });
    lay->addWidget(view);

    auto *input = new QPlainTextEdit(responseWindow);
    input->setPlaceholderText(tr("Type a follow-up message"));
    const QColor borderColor = responseView->palette().color(QPalette::Mid);
    input->setStyleSheet(
        QString("QPlainTextEdit { background-color: #ffffff; border: 1px solid %1; padding: 4px; } "
                "QPlainTextEdit:disabled { background-color: #f0f0f0; }")
            .arg(borderColor.name())
    );
    input->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    input->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input->setFocusPolicy(Qt::StrongFocus);
    input->installEventFilter(this);
    followUpInput = input;
    connect(input, &QPlainTextEdit::textChanged, this, [this]() {
        QTimer::singleShot(0, this, &TaskWindow::updateFollowUpHeight);
    });
    if (input->document() && input->document()->documentLayout()) {
        connect(input->document()->documentLayout(),
                &QAbstractTextDocumentLayout::documentSizeChanged,
                this,
                [this](const QSizeF &) { updateFollowUpHeight(); });
    }
    updateFollowUpHeight();

    lay->addWidget(input);
    setRequestInFlight(requestInFlight);

    responseWindow->setWindowTitle(tr("LLM Response"));
    applyResponsePrefs();

    const QPoint cursorPos = QCursor::pos();
    moveNearCursor(responseWindow, cursorPos);

    responseWindow->show();
    responseWindow->raise();
    responseWindow->activateWindow();
}

void TaskWindow::updateResponseView() {
    if (!responseView)
        return;
    QScrollBar *bar = responseView->verticalScrollBar();
    const bool atBottom = bar && bar->value() >= bar->maximum();
    const QString displayText = buildDisplayMarkdown();
    responseView->document()->setMarkdown(displayText, QTextDocument::MarkdownDialectGitHub);
    applyMarkdownStyles();
    if (atBottom)
        bar->setValue(bar->maximum());
}

void TaskWindow::applyMarkdownStyles() {
    if (!responseView)
        return;
    QTextDocument *doc = responseView->document();
    if (!doc)
        return;

    const qreal codeBlockMargin = 8.0;
    QTextCharFormat inlineCodeFormat;
    inlineCodeFormat.setFontFamilies(QStringList{"Consolas"});
    inlineCodeFormat.setFontFixedPitch(true);
    inlineCodeFormat.setBackground(QColor("#f6f8fa"));
    const QFont baseFont = resolveBaseTextFont(doc);
    if (baseFont.pointSizeF() > 0) {
        inlineCodeFormat.setFontPointSize(baseFont.pointSizeF());
    } else if (baseFont.pixelSize() > 0) {
        inlineCodeFormat.setProperty(QTextFormat::FontPixelSize, baseFont.pixelSize());
    }

    QTextCharFormat blockCodeCharFormat = inlineCodeFormat;

    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (isCodeBlock(block)) {
            QTextCursor blockCursor(block);
            QTextBlockFormat blockFormat = block.blockFormat();
            blockFormat.setBackground(QColor("#f6f8fa"));
            const QTextBlock prevBlock = block.previous();
            const QTextBlock nextBlock = block.next();
            const bool isFirstBlock = !prevBlock.isValid() || !isCodeBlock(prevBlock);
            const bool isLastBlock = !nextBlock.isValid() || !isCodeBlock(nextBlock);
            blockFormat.setTopMargin(isFirstBlock ? codeBlockMargin : 0.0);
            blockFormat.setBottomMargin(isLastBlock ? codeBlockMargin : 0.0);
            blockCursor.setBlockFormat(blockFormat);

            blockCursor.select(QTextCursor::BlockUnderCursor);
            blockCursor.mergeCharFormat(blockCodeCharFormat);
            continue;
        }

        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            if (!isInlineCodeFormat(fragment.charFormat()))
                continue;
            QTextCursor cursor(doc);
            cursor.setPosition(fragment.position());
            cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(inlineCodeFormat);
        }
    }
}

void TaskWindow::updateFollowUpHeight() {
    if (!followUpInput)
        return;
    QTextDocument *doc = followUpInput->document();
    if (!doc)
        return;
    const int maxLines = 5;
    int visualLines = 0;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        QTextLayout *layout = block.layout();
        if (layout)
            visualLines += qMax(1, layout->lineCount());
        else
            visualLines += 1;
    }
    if (visualLines < 1)
        visualLines = 1;
    if (visualLines > maxLines)
        visualLines = maxLines;

    const int lineHeight = QFontMetrics(followUpInput->font()).lineSpacing();
    const int frame = followUpInput->frameWidth();
    const int docMargin = qRound(doc->documentMargin());
    const int verticalPadding = frame * 2 + docMargin * 2 + 4;
    const int targetHeight = lineHeight * visualLines + verticalPadding;
    if (followUpInput->height() != targetHeight) {
        followUpInput->setFixedHeight(targetHeight);
        followUpInput->updateGeometry();
    }
}

void TaskWindow::appendMessageToHistory(const QString &role, const QString &content) {
    if (content.trimmed().isEmpty())
        return;
    messageHistory.append({role, content});
}

void TaskWindow::appendTranscriptBlock(const QString &markdown) {
    if (markdown.trimmed().isEmpty())
        return;
    if (!transcriptText.isEmpty() && !transcriptText.endsWith("\n\n"))
        transcriptText += "\n\n";
    transcriptText += markdown;
}

QString TaskWindow::formatUserMessageBlock(const QString &text) const {
    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace("\r", "\n");
    const QStringList lines = normalized.split('\n');

    QString block = "---\n";
    block += "> **You**: \n";
    for (const QString &line : lines)
        block += "> " + line + "  \n";
    block += "\n---";
    return block;
}

QString TaskWindow::buildDisplayMarkdown() const {
    QString displayText = transcriptText;
    if (!pendingResponseText.isEmpty()) {
        if (!displayText.isEmpty() && !displayText.endsWith("\n\n"))
            displayText += "\n\n";
        displayText += pendingResponseText;
    }
    return displayText;
}

QString TaskWindow::extractResponseTextFromJson(const QByteArray &data) const {
    const QJsonDocument respDoc = QJsonDocument::fromJson(data);
    if (!respDoc.isObject())
        return QString();
    const QJsonObject respObj = respDoc.object();
    const QJsonArray choices = respObj.value("choices").toArray();
    if (choices.isEmpty())
        return QString();
    const QJsonObject msg = choices.first().toObject()
        .value("message")
        .toObject();
    return msg.value("content").toString();
}

void TaskWindow::resetRequestState() {
    responseBody.clear();
    streamBuffer.clear();
    pendingResponseText.clear();
    sawStreamFormat = false;
}

void TaskWindow::resetConversationState() {
    messageHistory.clear();
    transcriptText.clear();
    pendingResponseText.clear();
    resetRequestState();
    setRequestInFlight(false);
    if (followUpInput)
        followUpInput->clear();
    if (responseView)
        responseView->clear();
}

void TaskWindow::setRequestInFlight(bool inFlight) {
    requestInFlight = inFlight;
    if (followUpInput)
        followUpInput->setEnabled(!inFlight);
    if (!inFlight && followUpInput && responseWindow && responseWindow->isVisible()) {
        responseWindow->raise();
        responseWindow->activateWindow();
        QTimer::singleShot(0, this, [this]() {
            if (followUpInput)
                followUpInput->setFocus(Qt::OtherFocusReason);
        });
    }
}

QString TaskWindow::parseStreamDelta(const QByteArray &line) {
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return QString();
    if (!trimmed.startsWith("data:"))
        return QString();
    sawStreamFormat = true;
    const QByteArray payload = trimmed.mid(5).trimmed();
    if (payload == "[DONE]")
        return QString();

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject())
        return QString();
    const QJsonObject obj = doc.object();
    const QJsonArray choices = obj.value("choices").toArray();
    if (choices.isEmpty())
        return QString();
    const QJsonObject choice = choices.first().toObject();
    QString deltaText = choice.value("delta").toObject().value("content").toString();
    if (deltaText.isEmpty())
        deltaText = choice.value("message").toObject().value("content").toString();
    return deltaText;
}

void TaskWindow::applyResponsePrefs() {
    QSize targetSize(600, 200);
    int targetZoom = 0;
    if (activeTaskIndex >= 0 && activeTaskIndex < tasks.size()) {
        TaskDefinition &task = tasks[activeTaskIndex];
        if (task.responseWidth > 0 && task.responseHeight > 0)
            targetSize = QSize(task.responseWidth, task.responseHeight);
        targetZoom = task.responseZoom;
        if (task.responseWidth != targetSize.width()
            || task.responseHeight != targetSize.height()) {
            task.responseWidth = targetSize.width();
            task.responseHeight = targetSize.height();
            emit taskResponsePrefsChanged(activeTaskIndex, targetSize, task.responseZoom);
        }
    }

    if (responseWindow)
        responseWindow->resize(targetSize);

    if (responseView) {
        if (targetZoom > 0)
            responseView->zoomIn(targetZoom);
        else if (targetZoom < 0)
            responseView->zoomOut(-targetZoom);
        applyMarkdownStyles();
    }
}

void TaskWindow::handleResponseResize(const QSize &size) {
    if (!size.isValid())
        return;
    if (activeTaskIndex < 0 || activeTaskIndex >= tasks.size())
        return;
    TaskDefinition &task = tasks[activeTaskIndex];
    if (task.responseWidth == size.width() && task.responseHeight == size.height())
        return;
    task.responseWidth = size.width();
    task.responseHeight = size.height();
    emit taskResponsePrefsChanged(activeTaskIndex, size, task.responseZoom);
}

void TaskWindow::handleResponseZoomDelta(int steps) {
    if (steps == 0)
        return;
    if (activeTaskIndex < 0 || activeTaskIndex >= tasks.size())
        return;
    TaskDefinition &task = tasks[activeTaskIndex];
    const int newZoom = task.responseZoom + steps;
    if (newZoom == task.responseZoom)
        return;
    task.responseZoom = newZoom;
    emit taskResponsePrefsChanged(activeTaskIndex,
                                  QSize(task.responseWidth, task.responseHeight),
                                  task.responseZoom);
}

bool TaskWindow::eventFilter(QObject *watched, QEvent *event) {
    if (followUpInput && watched == followUpInput.data()) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                if (keyEvent->modifiers().testFlag(Qt::ShiftModifier))
                    return false;
                sendFollowUpMessage();
                return true;
            }
        }
    }
    if (responseWindow && watched == responseWindow.data()
        && event->type() == QEvent::Close) {
        emit taskResponsePrefsCommitRequested();
    }
    if (responseWindow && watched == responseWindow.data()
        && event->type() == QEvent::Resize) {
        auto *resizeEvent = static_cast<QResizeEvent *>(event);
        handleResponseResize(resizeEvent->size());
    }
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            && qobject_cast<QPushButton*>(watched)) {
            auto *btn = qobject_cast<QPushButton*>(watched);
            btn->click();
            return true;
        }
    }
    if (event->type() == QEvent::Enter) {
        if (auto *btn = qobject_cast<QPushButton *>(watched)) {
            bool ok = false;
            const int index = btn->property("menuIndex").toInt(&ok);
            if (ok)
                setMenuActiveIndex(index);
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TaskWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    applyNoActivateStyle();
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd)
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    setMenuActiveIndex(-1);
    installMenuHooks();
}

void TaskWindow::hideEvent(QHideEvent *event) {
    removeMenuHooks();
    QWidget::hideEvent(event);
}

bool TaskWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        auto *msg = static_cast<MSG *>(message);
        if (msg && msg->message == WM_MOUSEACTIVATE) {
            if (result)
                *result = MA_NOACTIVATE;
            return true;
        }
    }
    return QWidget::nativeEvent(eventType, message, result);
}

void TaskWindow::installMenuHooks() {
    s_activeMenu = this;
    if (!s_keyboardHook) {
        s_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                           GetModuleHandleW(nullptr), 0);
    }
    if (!s_mouseHook) {
        s_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc,
                                        GetModuleHandleW(nullptr), 0);
    }
}

void TaskWindow::removeMenuHooks() {
    if (s_activeMenu != this)
        return;
    s_activeMenu = nullptr;
    if (s_keyboardHook) {
        UnhookWindowsHookEx(s_keyboardHook);
        s_keyboardHook = nullptr;
    }
    if (s_mouseHook) {
        UnhookWindowsHookEx(s_mouseHook);
        s_mouseHook = nullptr;
    }
}

LRESULT CALLBACK TaskWindow::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_activeMenu) {
        const auto *data = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (s_activeMenu->handleHookKey(static_cast<UINT>(data->vkCode)))
                return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK TaskWindow::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_activeMenu) {
        switch (wParam) {
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN: {
                const auto *data = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam);
                s_activeMenu->handleHookMouseClick(data->pt);
                break;
            }
            default:
                break;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool TaskWindow::handleHookKey(UINT vk) {
    if (!isVisible())
        return false;
    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    switch (vk) {
        case VK_ESCAPE:
            close();
            return true;
        case VK_TAB:
            if (shift)
                selectPreviousMenuItem();
            else
                selectNextMenuItem();
            return true;
        case VK_LEFT:
        case VK_UP:
            selectPreviousMenuItem();
            return true;
        case VK_RIGHT:
        case VK_DOWN:
            selectNextMenuItem();
            return true;
        case VK_RETURN:
        case VK_SPACE:
            activateMenuItem();
            return true;
        default:
            return false;
    }
}

void TaskWindow::handleHookMouseClick(const POINT &pt) {
    if (!isVisible())
        return;
    if (!isPointInsideMenu(pt))
        close();
}

bool TaskWindow::isPointInsideMenu(const POINT &pt) const {
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return false;
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect))
        return false;
    return PtInRect(&rect, pt) != 0;
}

void TaskWindow::setMenuActiveIndex(int index) {
    if (index < -1 || index >= menuButtons.size())
        return;
    if (menuActiveIndex == index)
        return;
    menuActiveIndex = index;
    for (int i = 0; i < menuButtons.size(); ++i) {
        QPushButton *btn = menuButtons[i];
        const bool active = (i == menuActiveIndex);
        if (btn->property("menuActive").toBool() == active)
            continue;
        btn->setProperty("menuActive", active);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
        btn->update();
    }
}

void TaskWindow::selectNextMenuItem() {
    if (menuButtons.isEmpty())
        return;
    if (menuActiveIndex < 0)
        setMenuActiveIndex(0);
    else
        setMenuActiveIndex((menuActiveIndex + 1) % menuButtons.size());
}

void TaskWindow::selectPreviousMenuItem() {
    if (menuButtons.isEmpty())
        return;
    if (menuActiveIndex < 0)
        setMenuActiveIndex(menuButtons.size() - 1);
    else
        setMenuActiveIndex((menuActiveIndex - 1 + menuButtons.size()) % menuButtons.size());
}

void TaskWindow::activateMenuItem() {
    if (menuActiveIndex < 0 || menuActiveIndex >= menuButtons.size())
        return;
    menuButtons[menuActiveIndex]->click();
}

void TaskWindow::applyNoActivateStyle() {
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const LONG_PTR desired = exStyle | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    if (desired != exStyle)
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, desired);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
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
    const QPoint pos = QCursor::pos();
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

void TaskWindow::keyPressEvent(QKeyEvent *ev) {
    if (ev->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(ev);
}
