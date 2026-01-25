#include "mainwindow.h"

#include <QApplication>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>
#include <QColor>
#include <QMessageBox>
#include <QObject>
#include <QPalette>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("Desktop LLM Helper");
    QStyle *fusionStyle = QStyleFactory::create("Fusion");
    if (fusionStyle) {
        a.setStyle(fusionStyle);
    }

    QPalette palette = a.palette();
    palette.setColor(QPalette::Window, QColor(240, 240, 240));
    palette.setColor(QPalette::WindowText, QColor(0, 0, 0));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 225));
    palette.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
    palette.setColor(QPalette::Text, QColor(0, 0, 0));
    palette.setColor(QPalette::Button, QColor(240, 240, 240));
    palette.setColor(QPalette::ButtonText, QColor(0, 0, 0));
    palette.setColor(QPalette::BrightText, QColor(255, 0, 0));
    palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    a.setPalette(palette);
    a.setStyleSheet(
        "QToolTip { color: #000000; background-color: #ffffe1; border: 1px solid #b5b5b5; }"
        "QMenu::item:selected { background-color: #0078d7; color: #ffffff; }"
    );

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
