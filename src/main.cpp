#include <QApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include <QUrl>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QCommandLineParser>
#include <QImageWriter>
#include <QFile>
#include <cstdio>
#include <KLocalizedString>
#include <KLocalizedQmlContext>
#include <KIconTheme>

#include "WallpaperProcessor.h"

// ── F3: headless CLI batch mode ──────────────────────────────────────────
// walltz --input photo.jpg [--width 1920 --height 1080] [--blur]
//       [--out result.png] [--pattern-type N] ...
// Runs the same renderCore as the GUI. Q5: runs under QCoreApplication so no
// display server is required.
static int runCli(QCoreApplication &app, const QCommandLineParser &parser)
{
    Q_UNUSED(app);
    const QString inPath = parser.value(QStringLiteral("input"));
    if (inPath.isEmpty() || !QFile::exists(inPath)) {
        std::fprintf(stderr, "walltz: --input file not found: %s\n", qPrintable(inPath));
        return 2;
    }

    WallpaperProcessor proc;
    if (parser.isSet(QStringLiteral("width")))
        proc.setTargetWidth(parser.value(QStringLiteral("width")).toInt());
    if (parser.isSet(QStringLiteral("height")))
        proc.setTargetHeight(parser.value(QStringLiteral("height")).toInt());
    if (parser.isSet(QStringLiteral("blur")))
        proc.setBlurMode(true);
    if (parser.isSet(QStringLiteral("no-blur")))
        proc.setBlurMode(false);
    if (parser.isSet(QStringLiteral("blur-radius")))
        proc.setBlurRadius(parser.value(QStringLiteral("blur-radius")).toInt());
    if (parser.isSet(QStringLiteral("saturation")))
        proc.setSaturationFactor(parser.value(QStringLiteral("saturation")).toDouble());
    if (parser.isSet(QStringLiteral("bg-zoom")))
        proc.setBgZoom(parser.value(QStringLiteral("bg-zoom")).toDouble());
    if (parser.isSet(QStringLiteral("gradient-angle")))
        proc.setGradientAngle(parser.value(QStringLiteral("gradient-angle")).toDouble());
    if (parser.isSet(QStringLiteral("gradient-style")))
        proc.setBgGradientStyle(parser.value(QStringLiteral("gradient-style")).toInt());
    if (parser.isSet(QStringLiteral("gradient-preset")))
        proc.setBgGradientPreset(parser.value(QStringLiteral("gradient-preset")).toInt());
    if (parser.isSet(QStringLiteral("mood")))
        proc.setAutoMood(parser.value(QStringLiteral("mood")).toInt());
    if (parser.isSet(QStringLiteral("use-v2")))
        proc.setUseV2(true);
    if (parser.isSet(QStringLiteral("vignette")))
        proc.setVignetteStrength(parser.value(QStringLiteral("vignette")).toDouble());
    if (parser.isSet(QStringLiteral("grain")))
        proc.setGrainStrength(parser.value(QStringLiteral("grain")).toDouble());
    if (parser.isSet(QStringLiteral("ca")))
        proc.setCaStrength(parser.value(QStringLiteral("ca")).toDouble());
    if (parser.isSet(QStringLiteral("frame")))
        proc.setPhotoFrame(true);
    if (parser.isSet(QStringLiteral("frame-width")))
        proc.setPhotoFrameWidth(parser.value(QStringLiteral("frame-width")).toInt());
    if (parser.isSet(QStringLiteral("fg-zoom")))
        proc.setFgZoom(parser.value(QStringLiteral("fg-zoom")).toDouble());
    if (parser.isSet(QStringLiteral("pip-zoom")))
        proc.setPipZoom(parser.value(QStringLiteral("pip-zoom")).toDouble());
    if (parser.isSet(QStringLiteral("pattern")))
        proc.setBgPatternEnabled(true);
    if (parser.isSet(QStringLiteral("pattern-type")))
        proc.setBgPatternType(parser.value(QStringLiteral("pattern-type")).toInt());
    if (parser.isSet(QStringLiteral("preset"))) {
        const QString preset = parser.value(QStringLiteral("preset"));
        proc.applyParamPreset(preset);   // named preset overrides the above
    }

    QString outPath;
    if (!proc.processSingleImage(inPath, outPath)) {
        std::fprintf(stderr, "walltz: processing failed\n");
        return 1;
    }

    // Optional --out overrides the default "<name>.wp.png"
    if (parser.isSet(QStringLiteral("out"))) {
        const QString want = parser.value(QStringLiteral("out"));
        QFile::remove(want);
        if (!QFile::copy(outPath, want)) {
            std::fprintf(stderr, "walltz: could not copy result to %s\n", qPrintable(want));
            return 1;
        }
        outPath = want;
    }

    std::printf("walltz: wrote %s\n", qPrintable(outPath));
    return 0;
}

// Shared CLI option set — one table, used by both headless and (future) GUI paths.
static void addCliOptions(QCommandLineParser &parser)
{
    parser.setApplicationDescription(i18n("Walltz — wallpaper composition tool"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        { { QStringLiteral("i"), QStringLiteral("input") },
          i18n("Input image (headless mode)"), QStringLiteral("file") },
        { QStringLiteral("out"), i18n("Output path (default <name>.wp.png)"), QStringLiteral("file") },
        { QStringLiteral("width"), i18n("Target width"), QStringLiteral("px") },
        { QStringLiteral("height"), i18n("Target height"), QStringLiteral("px") },
        { QStringLiteral("blur"), i18n("Use blur background (default)") },
        { QStringLiteral("no-blur"), i18n("Use colour/gradient background") },
        { QStringLiteral("blur-radius"), i18n("Blur radius (0 = auto)"), QStringLiteral("px") },
        { QStringLiteral("saturation"), i18n("Saturation factor"), QStringLiteral("x") },
        { QStringLiteral("bg-zoom"), i18n("Background zoom"), QStringLiteral("x") },
        { QStringLiteral("gradient-angle"), i18n("Gradient angle (deg)"), QStringLiteral("deg") },
        { QStringLiteral("gradient-style"), i18n("0=solid 1=preset 2=mood"), QStringLiteral("n") },
        { QStringLiteral("gradient-preset"), i18n("Gradient preset index 0-11"), QStringLiteral("n") },
        { QStringLiteral("mood"), i18n("Mood index 0-5"), QStringLiteral("n") },
        { QStringLiteral("use-v2"), i18n("Use V2 mood palettes") },
        { QStringLiteral("vignette"), i18n("Vignette strength 0-1"), QStringLiteral("x") },
        { QStringLiteral("grain"), i18n("Grain strength 0-1"), QStringLiteral("x") },
        { QStringLiteral("ca"), i18n("Chromatic aberration 0-1"), QStringLiteral("x") },
        { QStringLiteral("frame"), i18n("Photo frame on") },
        { QStringLiteral("frame-width"), i18n("Frame width %% of min dim"), QStringLiteral("n") },
        { QStringLiteral("fg-zoom"), i18n("Foreground zoom"), QStringLiteral("x") },
        { QStringLiteral("pip-zoom"), i18n("PiP zoom"), QStringLiteral("x") },
        { QStringLiteral("pattern"), i18n("Enable pattern background") },
        { QStringLiteral("pattern-type"), i18n("Pattern type id"), QStringLiteral("n") },
        { QStringLiteral("preset"), i18n("Apply a saved parameter preset"), QStringLiteral("name") },
    });
}

// Q5: headless entry — QCoreApplication, no display server needed.
static int runHeadlessMain(QCoreApplication &app)
{
    QCoreApplication::setOrganizationName(QStringLiteral("Walltz"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("walltz.app"));
    QCoreApplication::setApplicationName(QStringLiteral("walltz"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));
    KLocalizedString::setApplicationDomain("walltz");

    QCommandLineParser parser;
    addCliOptions(parser);
    parser.process(app);
    if (!parser.isSet(QStringLiteral("input"))) {
        parser.showHelp(0);   // no --input → print usage and exit
        return 0;
    }
    return runCli(app, parser);
}

int main(int argc, char *argv[])
{
    // Q5: headless CLI must not construct QApplication (needs a display).
    // Scan argv for --input/-i before choosing the app type.
    bool headless = false;
    for (int i = 1; i < argc; ++i) {
        const QByteArray a = QByteArray(argv[i]);
        if (a == "--input" || a == "-i") { headless = true; break; }
    }

    if (headless) {
        QCoreApplication app(argc, argv);
        return runHeadlessMain(app);
    }

    KIconTheme::initTheme();
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("Walltz"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("walltz.app"));
    QCoreApplication::setApplicationName(QStringLiteral("walltz"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));
    QApplication::setApplicationDisplayName(i18n("Walltz"));

    // CLI options double as GUI flags: --help/--version must work here too.
    QCommandLineParser parser;
    addCliOptions(parser);
    parser.process(app);

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
        if (!processor) {
            processor = window->findChild<WallpaperProcessor *>();
        }
        if (processor) {
            processor->setWindow(window);
        }
    }

    return app.exec();
}
