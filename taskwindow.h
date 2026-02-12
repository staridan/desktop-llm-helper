#ifndef TASKWINDOW_H
#define TASKWINDOW_H

#include <QWidget>
#include <QList>
#include <QEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QLabel>
#include <QPointer>
#include <QSize>

#include <windows.h>

#include "configstore.h"

class QByteArray;
class QHideEvent;
class QPushButton;
class QShowEvent;
class QNetworkAccessManager;
class QNetworkReply;
class QTextBrowser;
class QDialog;
class QPlainTextEdit;

struct ChatMessage {
    QString role;
    QString content;
};

class TaskWindow : public QWidget {
    Q_OBJECT

public:
    explicit TaskWindow(const QList<TaskDefinition> &taskList,
                        const AppSettings &settings,
                        QWidget *parent = nullptr);
    ~TaskWindow() override;

signals:
    void taskResponsePrefsChanged(int taskIndex, const QSize &size, int zoom);
    void taskResponsePrefsCommitRequested();

protected:
    void keyPressEvent(QKeyEvent *ev) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void updateLoadingPosition();
    void animateLoadingText();
    void sendFollowUpMessage();

private:
    QList<TaskDefinition> tasks;
    int activeTaskIndex;
    AppSettings settings;
    QNetworkAccessManager *networkManager;
    QWidget *loadingWindow;
    QTimer *loadingTimer;
    QLabel *loadingLabel;
    QTimer *animationTimer;
    int dotCount;

    QPointer<QDialog> responseWindow;
    QPointer<QTextBrowser> responseView;
    QPointer<QPlainTextEdit> followUpInput;
    QPointer<QPushButton> stopButton;
    QPointer<QNetworkReply> currentReply;
    QByteArray responseBody;
    QByteArray streamBuffer;
    QString transcriptText;
    QString pendingResponseText;
    QList<ChatMessage> messageHistory;
    bool sawStreamFormat;
    bool requestInFlight;
    QList<QPushButton *> menuButtons;
    int menuActiveIndex;

    static TaskWindow *s_activeMenu;
    static HHOOK s_keyboardHook;
    static HHOOK s_mouseHook;
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    QString captureSelectedText();
    QString applyCharLimit(const QString &text) const;
    void startConversation(const TaskDefinition &task, const QString &originalText);
    void sendRequestWithHistory(const TaskDefinition &task);
    void handleReplyReadyRead(const TaskDefinition &task, QNetworkReply *reply);
    void handleReplyFinished(const TaskDefinition &task, QNetworkReply *reply);
    void insertResponse(const QString &text);
    void ensureResponseWindow();
    void updateResponseView();
    void applyMarkdownStyles();
    void updateFollowUpHeight();
    void appendMessageToHistory(const QString &role, const QString &content);
    void appendTranscriptBlock(const QString &markdown);
    QString formatUserMessageBlock(const QString &text) const;
    QString buildDisplayMarkdown() const;
    QString extractResponseTextFromJson(const QByteArray &data) const;
    void resetRequestState();
    void resetConversationState();
    void setRequestInFlight(bool inFlight);
    void cancelRequest();
    QString parseStreamDelta(const QByteArray &line);
    void applyResponsePrefs();
    void handleResponseResize(const QSize &size);
    void handleResponseZoomDelta(int steps);
    void installMenuHooks();
    void removeMenuHooks();
    bool handleHookKey(UINT vk);
    void handleHookMouseClick(const POINT &pt);
    bool isPointInsideMenu(const POINT &pt) const;
    void setMenuActiveIndex(int index);
    void selectNextMenuItem();
    void selectPreviousMenuItem();
    void activateMenuItem();
    void applyNoActivateStyle();

    void showLoadingIndicator();
    void hideLoadingIndicator();
};

#endif // TASKWINDOW_H
