#include "mainwindow.h"

#include <QApplication>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QObject>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // Ensure single instance using QLockFile
    QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tmpDir);
    QLockFile lockFile(tmpDir + QDir::separator() + "TextHelper.lock");
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock()) {
        QMessageBox::warning(nullptr,
                             QObject::tr("TextHelper"),
                             QObject::tr("Another instance of TextHelper is already running."));
        return 0;
    }

    a.setQuitOnLastWindowClosed(false);

    MainWindow w;
    w.hide();

    return a.exec();
}