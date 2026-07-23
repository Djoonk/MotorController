#include <QApplication>
#include <QGuiApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    // Налаштування для плавного та точного масштабування інтерфейсу під HiDPI дисплеї мобільних пристроїв
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough
        );

    QApplication a(argc, argv);

    MainWindow w;
    w.showMaximized(); // Мобільний інтерфейс обов'язково розгортаємо на весь екран пристрою

    return a.exec();
}
