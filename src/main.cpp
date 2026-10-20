#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>

#include "MainWindow.h"
#include "core/Logger.h"
#include "ui/Theme.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

// One running copy per install folder (and user): a second start brings the first to the front.
// Keyed by the folder, so a separate copy (a test build) can still run next to it.
static QString instanceKey()
{
    const QString user = qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER"));
    const QByteArray dir = QDir(QCoreApplication::applicationDirPath()).canonicalPath().toLower().toUtf8();
    return "WindowManager-" + user + "-"
           + QString::fromLatin1(QCryptographicHash::hash(dir, QCryptographicHash::Sha1).toHex().left(12));
}

static void qtMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    const std::string text = msg.toStdString();
    const std::string src  = ctx.category ? ctx.category : "Qt";
    switch (type) {
    case QtDebugMsg:    Logger::instance().debug(text, src); break;
    case QtInfoMsg:     Logger::instance().info (text, src); break;
    case QtWarningMsg:  Logger::instance().warn (text, src); break;
    case QtCriticalMsg: Logger::instance().error(text, src); break;
    case QtFatalMsg:
        Logger::instance().error("FATAL: " + text, src);
        std::abort();
    }
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    // Window control goes through X11 (XWayland is fine), so Qt must use the same coordinates.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");
#endif
    QApplication app(argc, argv);

    // Log next to the executable. applicationDirPath() is absolute, unlike
    // argv[0], which may be relative or just a bare name.
    Logger::instance().setLogFile(
        std::filesystem::path(QCoreApplication::applicationDirPath().toStdU16String())
        / "windowmanager.log");
    qInstallMessageHandler(qtMessageHandler);

    const QString key = instanceKey();
    {
        QLocalSocket running;
        running.connectToServer(key);
        if (running.waitForConnected(500)) {
#ifdef Q_OS_WIN
            AllowSetForegroundWindow(ASFW_ANY);   // let the running copy come to the front
#endif
            running.write("show\n");
            running.waitForBytesWritten(500);
            Logger::instance().info("WindowManager is already running: brought it to the front", "main");
            return 0;
        }
    }
    QLocalServer::removeServer(key);   // a socket file left by a crash (Linux)
    QLocalServer instances;
    instances.listen(key);

    Logger::instance().info("=== WindowManager started ===", "main");

    QApplication::setApplicationName("WindowManager");
    QApplication::setOrganizationName("WindowManager");
    Theme::apply(app);

    MainWindow window;
    window.show();
    QObject::connect(&instances, &QLocalServer::newConnection, &window, [&instances, &window]() {
        while (QLocalSocket *other = instances.nextPendingConnection()) {
            QObject::connect(other, &QLocalSocket::disconnected, other, &QObject::deleteLater);
            window.bringToFront();   // the only message is "show"
        }
    });

    const int result = app.exec();
    Logger::instance().info("=== WindowManager exiting (code " +
                            std::to_string(result) + ") ===", "main");
    return result;
}