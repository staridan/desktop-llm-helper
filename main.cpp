#include "mainwindow.h"

#include <QApplication>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QObject>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("Desktop LLM Helper");

    // Ensure single instance using QLockFile
    QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tmpDir);
    QLockFile lockFile(tmpDir + QDir::separator() + "DesktopLLMHelper.lock");
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock()) {
        QMessageBox::warning(nullptr,
                             QObject::tr("DesktopLLMHelper"),
                             QObject::tr("Another instance of DesktopLLMHelper is already running."));
        return 0;
    }

    a.setQuitOnLastWindowClosed(false);

    MainWindow w;
    w.hide();

    return a.exec();
}