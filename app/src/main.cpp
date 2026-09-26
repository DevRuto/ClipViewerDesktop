#include "EditorController.h"
#include "StartupArgs.h"
#include "StillFrameProvider.h"
#include "Version.h"
#include "diagnostics/CrashHandler.h"
#include "diagnostics/Log.h"

#include <QDirIterator>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSysInfo>
#include <QTimer>

int main(int argc, char *argv[])
{
    // Before anything else, so a crash while Qt starts up is reported too.
    const QString logDir = cv::Log::defaultDir();
    const bool logging = cv::Log::start(
        logDir, QStringLiteral("ClipViewerDesktop %1\nQt %2, %3 (%4), %5")
                    .arg(QStringLiteral(CLIPVIEWER_VERSION), QString::fromLatin1(qVersion()),
                         QSysInfo::prettyProductName(), QSysInfo::kernelVersion(),
                         QSysInfo::currentCpuArchitecture()));
    cv::CrashHandler::install(logDir);
    const bool crashedLastTime = logging && cv::Log::hasCrashReport(cv::Log::previousPath(logDir));
    // Qt's few startup lines on the graphics backend, GPU and driver, which are behind many
    // rendering crashes. QT_LOGGING_RULES still overrides this.
    QLoggingCategory::setFilterRules(QStringLiteral("qt.scenegraph.general=true\nqt.rhi.general=true"));

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ClipViewerDesktop"));
    QGuiApplication::setApplicationVersion(QStringLiteral(CLIPVIEWER_VERSION));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/ClipViewer/assets/app-icon.png")));

    // Geist and Geist Mono (OFL), embedded in the QML module's resources.
    QDirIterator fonts(QStringLiteral(":/qt/qml/ClipViewer/assets/fonts"), {QStringLiteral("*.ttf")});
    while (fonts.hasNext())
        QFontDatabase::addApplicationFont(fonts.next());
    QFont font(QStringLiteral("Geist"));
    font.setPixelSize(13);
    QGuiApplication::setFont(font);

    // Basic is the most restyleable Controls style; Theme.qml supplies the Graphite colours.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    qInfo("Command line: %s", qUtf8Printable(app.arguments().join(QLatin1Char(' '))));
    const cv::StartupArgs args = cv::StartupArgs::parse(app.arguments().mid(1));

    // The editor outlives the engine, whose QML binds to it; the engine owns the image providers.
    auto *stills = new StillFrameProvider;
    auto *thumbnails = new StillFrameProvider;
    auto *subtitles = new StillFrameProvider;
    EditorController editor(stills, thumbnails, subtitles);
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("still"), stills);
    engine.addImageProvider(QStringLiteral("thumb"), thumbnails);
    engine.addImageProvider(QStringLiteral("subtitle"), subtitles);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.setInitialProperties({{QStringLiteral("editor"), QVariant::fromValue(&editor)}});
    engine.loadFromModule("ClipViewer", "Main");

    if (crashedLastTime)
        editor.showCrashNotice();
    if (!args.videoPath.isEmpty())
        editor.openFile(args.videoPath);

    // For checking the crash report: CLIPVIEWER_CRASH_TEST=segv|abort|throw|fatal.
    const QString crashTest = qEnvironmentVariable("CLIPVIEWER_CRASH_TEST");
    if (!crashTest.isEmpty())
        QTimer::singleShot(1000, &app, [crashTest] { cv::CrashHandler::crashForTesting(crashTest); });

    const int exitCode = QGuiApplication::exec();
    // The file stays open, so a crash during shutdown still gets into it.
    qInfo("Event loop finished (exit code %d)", exitCode);
    return exitCode;
}
