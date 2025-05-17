#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QEvent>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TaskWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    static MainWindow* instance;

public slots:
    void setHotkeyText(const QString &text);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void on_pushButtonAddTask_clicked();
    void removeTaskWidget(TaskWidget* task);

private:
    Ui::MainWindow *ui;
    QString prevHotkey;
    bool hotkeyCaptured;
    void loadConfig();
    void saveConfig();
    QString configFilePath() const;
};

#endif // MAINWINDOW_H