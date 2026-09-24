#include "TimeFormat.h"

#include <QTest>

using namespace cv;

class TimeFormatTest : public QObject
{
    Q_OBJECT

private slots:
    void format_data()
    {
        QTest::addColumn<double>("seconds");
        QTest::addColumn<QString>("expected");
        QTest::newRow("zero") << 0.0 << "0:00.000";
        QTest::newRow("fraction") << 5.25 << "0:05.250";
        QTest::newRow("minutes") << 75.5 << "1:15.500";
        QTest::newRow("hours") << 3723.004 << "1:02:03.004";
        QTest::newRow("45 frames at 30 fps") << 45 / 30.0 << "0:01.500";
        QTest::newRow("rounds up") << 59.9999 << "1:00.000";
        QTest::newRow("negative") << -3.0 << "0:00.000";
    }
    void format()
    {
        QFETCH(double, seconds);
        QFETCH(QString, expected);
        QCOMPARE(TimeFormat::format(seconds), expected);
    }

    void parseValid_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<double>("expected");
        QTest::newRow("seconds") << "75.5" << 75.5;
        QTest::newRow("m:ss") << "1:15.5" << 75.5;
        QTest::newRow("hh:mm:ss") << "01:02:03.004" << 3723.004;
        QTest::newRow("whitespace") << " 0:05 " << 5.0;
    }
    void parseValid()
    {
        QFETCH(QString, text);
        QFETCH(double, expected);
        const auto parsed = TimeFormat::parse(text);
        QVERIFY(parsed.has_value());
        QVERIFY(qAbs(*parsed - expected) < 0.0005);
    }

    void parseInvalid_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("empty") << "";
        QTest::newRow("letters") << "abc";
        QTest::newRow("negative") << "-5";
        QTest::newRow("seconds over 59") << "1:75";
        QTest::newRow("too many fields") << "1:2:3:4";
        QTest::newRow("fraction in minutes") << "1.5:00";
    }
    void parseInvalid()
    {
        QFETCH(QString, text);
        QVERIFY(!TimeFormat::parse(text).has_value());
    }

    void formatRoundTripsThroughParse()
    {
        const double time = 123.456;
        QCOMPARE(qRound64(*TimeFormat::parse(TimeFormat::format(time)) * 1000), 123456);
    }

    void formatShort_data()
    {
        QTest::addColumn<double>("seconds");
        QTest::addColumn<QString>("expected");
        QTest::newRow("zero") << 0.0 << "0:00";
        QTest::newRow("truncates") << 9.99 << "0:09";
        QTest::newRow("minutes") << 83.456 << "1:23";
        QTest::newRow("hours") << 3723.0 << "1:02:03";
        QTest::newRow("negative") << -1.0 << "0:00";
    }
    void formatShort()
    {
        QFETCH(double, seconds);
        QFETCH(QString, expected);
        QCOMPARE(TimeFormat::formatShort(seconds), expected);
    }
};

QTEST_GUILESS_MAIN(TimeFormatTest)
#include "tst_timeformat.moc"
