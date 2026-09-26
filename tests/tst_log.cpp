// Log: the app.log file users attach to bug reports.
#include "diagnostics/Log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
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

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

const QDateTime Time(QDate(2026, 9, 26), QTime(14, 3, 12, 345));

} // namespace

class LogTest : public QObject
{
    Q_OBJECT

private slots:
    void formatLine_hasTimeLevelThreadAndCategory()
    {
        QCOMPARE(Log::formatLine(QtWarningMsg, "qt.multimedia", QStringLiteral("no audio"), Time, 0x1a2c),
                 "2026-09-26 14:03:12.345 W [1a2c] qt.multimedia: no audio\n");
    }

    void formatLine_leavesOutDefaultCategory()
    {
        QCOMPARE(Log::formatLine(QtInfoMsg, "default", QStringLiteral("Opening a.mp4"), Time, 1),
                 "2026-09-26 14:03:12.345 I [1] Opening a.mp4\n");
        QCOMPARE(Log::formatLine(QtDebugMsg, nullptr, QStringLiteral("x"), Time, 1),
                 "2026-09-26 14:03:12.345 D [1] x\n");
    }

    void formatLine_indentsContinuationLines()
    {
        QCOMPARE(Log::formatLine(QtCriticalMsg, "default", QStringLiteral("ffmpeg failed:\r\nline 2\nline 3\n"), Time, 1),
                 "2026-09-26 14:03:12.345 C [1] ffmpeg failed:\n    line 2\n    line 3\n");
    }

    void rotate_shiftsOldLogsAndDropsTheOldest()
    {
        QTemporaryDir dir;
        const QDir folder(dir.path());
        writeFile(folder.filePath("app.log"), "run 3");
        writeFile(folder.filePath("app.1.log"), "run 2");
        writeFile(folder.filePath("app.2.log"), "run 1");

        Log::rotate(dir.path(), 2);

        QVERIFY(!QFile::exists(folder.filePath("app.log")));
        QCOMPARE(readFile(folder.filePath("app.1.log")), "run 3");
        QCOMPARE(readFile(folder.filePath("app.2.log")), "run 2");
        QVERIFY(!QFile::exists(folder.filePath("app.3.log")));
    }

    void rotate_withNoLogs_doesNothing()
    {
        QTemporaryDir dir;
        Log::rotate(dir.path());
        QVERIFY(QDir(dir.path()).isEmpty());
    }

    void start_writesHeaderAndMessages_andRotatesOnNextStart()
    {
        QTemporaryDir dir;
        const QString logs = QDir(dir.path()).filePath("logs"); // created by start
        QVERIFY(Log::start(logs, QStringLiteral("ClipViewerDesktop test")));
        QCOMPARE(Log::currentPath(), QDir(logs).filePath("app.log"));
        qWarning("first run %d", 1);
        Log::stop(QStringLiteral("bye"));
        QVERIFY(Log::currentPath().isEmpty());
        qWarning("not logged"); // after stop

        const QByteArray first = readFile(QDir(logs).filePath("app.log"));
        QVERIFY(first.startsWith("ClipViewerDesktop test\n\n"));
        QVERIFY(first.contains(" W ["));
        QVERIFY(first.contains("first run 1\n"));
        QVERIFY(first.contains("bye\n"));
        QVERIFY(!first.contains("not logged"));

        QVERIFY(Log::start(logs, QStringLiteral("second")));
        Log::stop();
        QCOMPARE(readFile(Log::previousPath(logs)), first);
        QVERIFY(readFile(QDir(logs).filePath("app.log")).startsWith("second\n"));
    }

    void start_inUnwritableFolder_fails()
    {
        QTemporaryDir dir;
        const QString file = QDir(dir.path()).filePath("file");
        writeFile(file, "x");
        QVERIFY(!Log::start(QDir(file).filePath("logs"), QStringLiteral("header"))); // a file is in the way
        QVERIFY(Log::currentPath().isEmpty());
        qWarning("still works without a log");
    }

    void hasCrashReport_findsTheMarkerAtALineStart()
    {
        QTemporaryDir dir;
        const QString crashed = QDir(dir.path()).filePath("crashed.log");
        writeFile(crashed, QByteArray("header\n\nline\n\n") + Log::CrashMarker + " 2026-09-26 14:03:12 ===\n");
        QVERIFY(Log::hasCrashReport(crashed));

        const QString quoted = QDir(dir.path()).filePath("quoted.log");
        writeFile(quoted, QByteArray("header\n2026-09-26 I [1] Status: ") + Log::CrashMarker + "\n");
        QVERIFY(!Log::hasCrashReport(quoted));

        QVERIFY(!Log::hasCrashReport(QDir(dir.path()).filePath("missing.log")));
    }
};

QTEST_GUILESS_MAIN(LogTest)
#include "tst_log.moc"
