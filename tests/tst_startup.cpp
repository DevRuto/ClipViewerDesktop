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

class StartupTest : public QObject
{
    Q_OBJECT

private slots:
    // ---- StartupArgs ----

    void noArguments_opensClips() { QCOMPARE(StartupArgs::parse({}), (StartupArgs{StartupPage::Clips, {}})); }

    void editorFlag_opensEditor_data()
    {
        QTest::addColumn<QString>("flag");
        QTest::newRow("long") << "--editor";
        QTest::newRow("upper case") << "--EDITOR";
        QTest::newRow("short") << "-e";
    }
    void editorFlag_opensEditor()
    {
        QFETCH(QString, flag);
        QCOMPARE(StartupArgs::parse({flag}), (StartupArgs{StartupPage::Editor, {}}));
    }

    void clipsFlag_opensClips()
    {
        QCOMPARE(StartupArgs::parse({"--clips"}), (StartupArgs{StartupPage::Clips, {}}));
    }

    void videoPath_opensItInTheEditor()
    {
        QCOMPARE(StartupArgs::parse({"C:\\clips\\a.mp4"}), (StartupArgs{StartupPage::Editor, "C:\\clips\\a.mp4"}));
        QCOMPARE(StartupArgs::parse({"--clips", "a.mp4"}), (StartupArgs{StartupPage::Editor, "a.mp4"}));
        QCOMPARE(StartupArgs::parse({"--editor", "a.mp4", "b.mp4"}), (StartupArgs{StartupPage::Editor, "a.mp4"}));
    }

    void unknownFlags_areIgnored()
    {
        QCOMPARE(StartupArgs::parse({"--verbose"}), (StartupArgs{StartupPage::Clips, {}}));
    }

    void makePathsAbsolute_leavesFlags()
    {
        const QString dir = QDir::tempPath();
        const QStringList result = StartupArgs::makePathsAbsolute({"--editor", "a.mp4"}, dir);
        QCOMPARE(result.size(), 2);
        QCOMPARE(result[0], "--editor");
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
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ \"serverUrl\": \"https://clips.example\", \"futureSetting\": 42 }");
        file.close();

        AppSettings settings = AppSettings::load(path);
        QCOMPARE(settings.serverUrl, "https://clips.example");
        settings.compactLayout = true;
        QVERIFY(settings.save(path));

        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(json.value("futureSetting").toInt(), 42);
        QCOMPARE(json.value("compactLayout").toBool(), true);
        QCOMPARE(AppSettings::load(path), settings);
    }

    void settings_garbage_givesDefaults()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("settings.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not json");
        file.close();
        QCOMPARE(AppSettings::load(file.fileName()), AppSettings{});
    }
};

QTEST_GUILESS_MAIN(StartupTest)
#include "tst_startup.moc"
