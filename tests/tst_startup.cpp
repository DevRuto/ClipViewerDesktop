// StartupArgs, PlayerClickGesture and AppSettings: the small pieces the app shell relies on.
#include "AppSettings.h"
#include "PlayerClickGesture.h"
#include "StartupArgs.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace cv;

namespace {

void writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

} // namespace

class StartupTest : public QObject
{
    Q_OBJECT

private slots:
    // ---- StartupArgs ----

    void noArguments_opensNothing() { QCOMPARE(StartupArgs::parse({}).videoPath, QString()); }

    void firstPath_isOpened()
    {
        QCOMPARE(StartupArgs::parse({"C:\\clips\\a.mp4"}).videoPath, "C:\\clips\\a.mp4");
        QCOMPARE(StartupArgs::parse({"a.mp4", "b.mp4"}).videoPath, "a.mp4");
    }

    void flags_areIgnored()
    {
        QCOMPARE(StartupArgs::parse({"--verbose"}).videoPath, QString());
        QCOMPARE(StartupArgs::parse({"--verbose", "a.mp4"}).videoPath, "a.mp4");
    }

    void makePathsAbsolute_leavesFlags()
    {
        const QString dir = QDir::tempPath();
        const QStringList result = StartupArgs::makePathsAbsolute({"--verbose", "a.mp4"}, dir);
        QCOMPARE(result.size(), 2);
        QCOMPARE(result[0], "--verbose");
        QCOMPARE(QDir::fromNativeSeparators(result[1]), QDir(dir).filePath("a.mp4"));
    }

    // ---- PlayerClickGesture ----

    void doubleClickOnAnEdge_seeks()
    {
        PlayerClickGesture back;
        QCOMPARE(back.click(0.1, 1000), PlayerClickAction::TogglePlay);
        QCOMPARE(back.click(0.1, 1250), PlayerClickAction::UndoToggleAndSeekBack);

        PlayerClickGesture forward;
        QCOMPARE(forward.click(0.9, 1000), PlayerClickAction::TogglePlay);
        QCOMPARE(forward.click(0.9, 1250), PlayerClickAction::UndoToggleAndSeekForward);
    }

    void doubleClickInTheMiddle_togglesTwice()
    {
        PlayerClickGesture gesture;
        QCOMPARE(gesture.click(0.5, 1000), PlayerClickAction::TogglePlay);
        QCOMPARE(gesture.click(0.5, 1100), PlayerClickAction::TogglePlay);
    }

    void slowSecondClick_isASingleClick()
    {
        PlayerClickGesture gesture;
        gesture.click(0.1, 1000);
        QCOMPARE(gesture.click(0.1, 1301), PlayerClickAction::TogglePlay);
    }

    void secondClickOnTheOtherEdge_isASingleClick()
    {
        PlayerClickGesture gesture;
        gesture.click(0.1, 1000);
        QCOMPARE(gesture.click(0.9, 1100), PlayerClickAction::TogglePlay);
    }

    void tripleClick_seeksOnceThenStartsOver()
    {
        PlayerClickGesture gesture;
        gesture.click(0.9, 1000);
        QCOMPARE(gesture.click(0.9, 1100), PlayerClickAction::UndoToggleAndSeekForward);
        QCOMPARE(gesture.click(0.9, 1200), PlayerClickAction::TogglePlay);
        QCOMPARE(gesture.click(0.9, 1300), PlayerClickAction::UndoToggleAndSeekForward);
    }

    // ---- AppSettings ----

    void settings_missingFile_givesDefaults()
    {
        QTemporaryDir dir;
        QCOMPARE(AppSettings::load(dir.filePath("nope.json")), AppSettings{});
    }

    void settings_roundTrip_keepsUnknownFields()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("sub/settings.json");
        QDir().mkpath(dir.filePath("sub"));
        writeFile(path, "{ \"volume\": 0.25, \"futureSetting\": 42 }");

        AppSettings settings = AppSettings::load(path);
        QCOMPARE(settings.volume, 0.25);
        settings.smartCut = false;
        settings.reencode.maxHeight = 720;
        settings.lastExportFolder = "D:/exports";
        settings.lastFrameFolder = "D:/frames";
        settings.theme = "paper";
        settings.alwaysOnTop = true;
        settings.clickControls = false;
        settings.subtitles.size = 130;
        QVERIFY(settings.save(path));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(json.value("futureSetting").toInt(), 42);
        QCOMPARE(json.value("smartCut").toBool(true), false);
        QCOMPARE(json.value("reencode").toObject().value("maxHeight").toInt(), 720);
        QCOMPARE(json.value("theme").toString(), QString("paper"));
        QCOMPARE(json.value("alwaysOnTop").toBool(), true);
        QCOMPARE(json.value("clickControls").toBool(true), false);
        QCOMPARE(json.value("subtitles").toObject().value("size").toInt(), 130);
        QCOMPARE(AppSettings::load(path), settings);
    }

    void settings_invalidValues_fallBack()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("s.json"), "{ \"volume\": 7, \"muted\": \"yes\", \"theme\": 3, \"alwaysOnTop\": 1, \"clickControls\": 0 }");
        const AppSettings settings = AppSettings::load(dir.filePath("s.json"));
        QCOMPARE(settings.volume, 1.0);
        QCOMPARE(settings.muted, false);
        QCOMPARE(settings.theme, QString("graphite"));
        QCOMPARE(settings.alwaysOnTop, false);
        QCOMPARE(settings.clickControls, true);
    }

    void settings_garbage_givesDefaults()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("settings.json"), "not json");
        QCOMPARE(AppSettings::load(dir.filePath("settings.json")), AppSettings{});
    }
};

QTEST_GUILESS_MAIN(StartupTest)
#include "tst_startup.moc"
