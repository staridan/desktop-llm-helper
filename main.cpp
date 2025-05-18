#include "mainwindow.h"

#include <QApplication>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QObject>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // Блокировка единственного экземпляра через QLockFile
    QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tmpDir);
    QLockFile lockFile(tmpDir + QDir::separator() + "TextHelper.lock");
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock()) {
        QMessageBox::warning(nullptr,
                             QObject::tr("TextHelper"),
                             QObject::tr("Другая копия TextHelper уже запущена."));
        return 0;
    }

    a.setQuitOnLastWindowClosed(false);

    MainWindow w;
    w.hide();

    return a.exec();
}