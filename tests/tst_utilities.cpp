#include <QtTest>

#include "core/Utilities.h"

class TstUtilities : public QObject
{
    Q_OBJECT
private slots:
    void sanitizeName_data()
    {
        QTest::addColumn<QString>("in");
        QTest::addColumn<QString>("out");
        QTest::newRow("spaces")    << "Hello World"   << "HELLO_WORLD";
        QTest::newRow("collapse")  << "a -- b..c"     << "A_B_C";
        QTest::newRow("trim")      << "  [Game] (1) " << "GAME_1";
        QTest::newRow("junk")      << "Tekken!@# 7"   << "TEKKEN_7";
        QTest::newRow("empty")     << ""              << "";
        QTest::newRow("only-junk") << "!!!"           << "";
    }
    void sanitizeName()
    {
        QFETCH(QString, in);
        QFETCH(QString, out);
        QCOMPARE(QString::fromStdString(Utilities::sanitizeName(in.toStdString())), out);
    }

    void generateUniqueId()
    {
        QCOMPARE(Utilities::generateUniqueId("My Game", {}), std::string("GAME_MY_GAME"));
        QCOMPARE(Utilities::generateUniqueId("My Game", {"GAME_MY_GAME"}),
                 std::string("GAME_MY_GAME_2"));
        QCOMPARE(Utilities::generateUniqueId("My Game", {"GAME_MY_GAME", "GAME_MY_GAME_2"}),
                 std::string("GAME_MY_GAME_3"));
        QCOMPARE(Utilities::generateUniqueId("!!!", {}), std::string("GAME_GAME"));
    }

    void joinStrings()
    {
        QCOMPARE(Utilities::joinStrings({}), std::string());
        QCOMPARE(Utilities::joinStrings({"a"}), std::string("a"));
        QCOMPARE(Utilities::joinStrings({"a", "b", "c"}, "|"), std::string("a|b|c"));
    }

    void trimQuotesAndSpaces()
    {
        QCOMPARE(Utilities::trimQuotesAndSpaces("  \"C:\\x y.exe\"  "), std::string("C:\\x y.exe"));
        QCOMPARE(Utilities::trimQuotesAndSpaces("\"\""), std::string());
    }

    void fileHelpers()
    {
        QCOMPARE(Utilities::fileNameNoExt("C:/g/game.exe"), std::string("game"));
        QCOMPARE(Utilities::fileExtension("C:/g/game.EXE"), std::string("EXE"));
        QCOMPARE(Utilities::fileExtension("noext"), std::string());
    }

    void isValidExePath()
    {
        QVERIFY(!Utilities::isValidExePath("C:/definitely/not/here.exe"));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString exe = dir.filePath("a.EXE");
        const QString txt = dir.filePath("a.txt");
        QVERIFY(QFile(exe).open(QIODevice::WriteOnly));
        QVERIFY(QFile(txt).open(QIODevice::WriteOnly));
        QVERIFY(Utilities::isValidExePath(exe.toStdWString()));
        QVERIFY(!Utilities::isValidExePath(txt.toStdWString()));
    }

    void formatFileSize()
    {
        QCOMPARE(Utilities::formatFileSize(0), std::string("0.00 B"));
        QCOMPARE(Utilities::formatFileSize(1536), std::string("1.50 KB"));
        QCOMPARE(Utilities::formatFileSize(5ull * 1024 * 1024 * 1024), std::string("5.00 GB"));
    }
};

QTEST_APPLESS_MAIN(TstUtilities)
#include "tst_utilities.moc"
