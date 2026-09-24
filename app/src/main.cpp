#include "EditorController.h"
#include "StartupArgs.h"
#include "StillFrameProvider.h"

#include <QDirIterator>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
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

    const cv::StartupArgs args = cv::StartupArgs::parse(app.arguments().mid(1));

    // The editor outlives the engine, whose QML binds to it; the engine owns the image provider.
    auto *stills = new StillFrameProvider;
    EditorController editor(stills);
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("still"), stills);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.setInitialProperties({{QStringLiteral("editor"), QVariant::fromValue(&editor)}});
    engine.loadFromModule("ClipViewer", "Main");

    if (!args.videoPath.isEmpty())
        editor.openFile(args.videoPath);

    return QGuiApplication::exec();
}
