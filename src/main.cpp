#include <QApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include <QUrl>
#include <QQuickStyle>
#include <QQuickWindow>
#include <KLocalizedString>
#include <KLocalizedQmlContext>
#include <KIconTheme>

#include "WallpaperProcessor.h"

int main(int argc, char *argv[])
{
    KIconTheme::initTheme();
    QApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("walltz");

    QApplication::setOrganizationName(QStringLiteral("Walltz"));
    QApplication::setOrganizationDomain(QStringLiteral("walltz.app"));
    QApplication::setApplicationName(QStringLiteral("walltz"));
    QApplication::setApplicationDisplayName(i18n("Walltz"));
    QApplication::setApplicationVersion(QStringLiteral("0.2.0"));

    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));

    qmlRegisterType<WallpaperProcessor>("org.walltz.processor", 1, 0, "WallpaperProcessor");

    QQmlApplicationEngine engine;

    auto ctx = new KLocalizedQmlContext(&engine);
    engine.rootContext()->setContextObject(ctx);

    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/org/walltz/walltz/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    // Wire the root QQuickWindow to the processor for Wayland screen detection
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (window) {
        auto *processor = engine.singletonInstance<WallpaperProcessor *>(
            QStringLiteral("org.walltz.processor"), QStringLiteral("WallpaperProcessor"));
        // If not a singleton, find via root object's children
        if (!processor) {
            processor = window->findChild<WallpaperProcessor *>();
        }
        if (processor) {
            processor->setWindow(window);
        }
    }

    return app.exec();
}
