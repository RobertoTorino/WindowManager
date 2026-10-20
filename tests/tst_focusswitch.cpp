#include <QtTest>

#include "window/FocusSwitch.h"

using namespace FocusSwitch;

class TstFocusSwitch : public QObject
{
    Q_OBJECT
private slots:
    void owner()
    {
        constexpr qint64 app = 100;
        constexpr qint64 game = 200;
        QCOMPARE(ownerOf(game, app, game, false), Owner::Game);
        QCOMPARE(ownerOf(app, app, game, false), Owner::App);
        QCOMPARE(ownerOf(300, app, game, false), Owner::Other);
        QCOMPARE(ownerOf(0, app, game, false), Owner::Other);         // nothing in front (desktop switch)
        QCOMPARE(ownerOf(300, app, game, true), Owner::Game);         // a second process of the game's exe
        QCOMPARE(ownerOf(300, app, 0, true), Owner::Game);            // the game, before its window was found
        QCOMPARE(ownerOf(app, app, 0, false), Owner::None);           // no game: no switch
        QCOMPARE(ownerOf(300, app, 0, false), Owner::None);
    }

    void toggleGoesToTheOtherSide()
    {
        QCOMPARE(toggle(Owner::Game), Action::ShowApp);
        QCOMPARE(toggle(Owner::App), Action::FocusGame);
        QCOMPARE(toggle(Owner::Other), Action::FocusGame);
        QCOMPARE(toggle(Owner::None), Action::None);
    }

    void labels()
    {
        QCOMPARE(label(Owner::Game), QString("Focus: game"));
        QCOMPARE(label(Owner::App), QString("Focus: app"));
        QVERIFY(label(Owner::None).isEmpty());
    }

    void doublePress()
    {
        DoublePress p(2000);
        QVERIFY(!p.press(10'000));          // first: only a warning
        QVERIFY(p.waiting(11'000));
        QVERIFY(p.press(11'500));           // second within 2 s: terminate
        QVERIFY(!p.waiting(11'600));        // and it starts over
        QVERIFY(!p.press(12'000));
        QVERIFY(!p.press(14'500));          // too late: counts as a new first press
        QVERIFY(p.press(16'400));
        QVERIFY(!DoublePress().waiting(0));
    }

    void parse_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("valid");
        QTest::addColumn<int>("vk");
        QTest::addColumn<bool>("ctrl");
        QTest::addColumn<bool>("alt");
        QTest::addColumn<bool>("shift");
        QTest::newRow("F1")            << "F1" << true << 0x70 << false << false << false;
        QTest::newRow("f12 lower")     << "f12" << true << 0x7B << false << false << false;
        QTest::newRow("F24")           << "F24" << true << 0x87 << false << false << false;
        QTest::newRow("ctrl alt g")    << "Ctrl+Alt+G" << true << int('G') << true << true << false;
        QTest::newRow("spaces, lower") << " ctrl + shift + 5 " << true << int('5') << true << false << true;
        QTest::newRow("pause")         << "Pause" << true << 0x13 << false << false << false;
        QTest::newRow("empty")         << "" << false << 0 << false << false << false;
        QTest::newRow("F25")           << "F25" << false << 0 << false << false << false;
        QTest::newRow("only modifier") << "Ctrl+" << false << 0 << false << false << false;
        QTest::newRow("unknown mod")   << "Hyper+A" << false << 0 << false << false << false;
        QTest::newRow("unknown key")   << "Ctrl+Banana" << false << 0 << false << false << false;
    }

    void parse()
    {
        QFETCH(QString, text);
        QFETCH(bool, valid);
        const auto key = parseHotkey(text);
        QCOMPARE(key.has_value(), valid);
        if (!key)
            return;
        QTEST(key->virtualKey, "vk");
        QTEST(key->ctrl, "ctrl");
        QTEST(key->alt, "alt");
        QTEST(key->shift, "shift");
    }

    void textRoundTrips()
    {
        for (const QString& text : {"F1", "Ctrl+Alt+G", "Shift+F12", "Pause", "Ctrl+Win+0"}) {
            const auto key = parseHotkey(text);
            QVERIFY2(key.has_value(), qPrintable(text));
            QCOMPARE(hotkeyText(*key), text);
            QCOMPARE(parseHotkey(hotkeyText(*key)), key);
        }
        QCOMPARE(hotkeyText(*parseHotkey("alt+pageup")), QString("Alt+PageUp"));
    }
};

QTEST_APPLESS_MAIN(TstFocusSwitch)
#include "tst_focusswitch.moc"
