#include <QApplication>

#include "MainWindow.h"
#include "core/Logger.h"
#include "ui/Theme.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

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

    Logger::instance().info("=== WindowManager started ===", "main");

    QApplication::setApplicationName("WindowManager");
    QApplication::setOrganizationName("WindowManager");
    Theme::apply(app);

    MainWindow window;
    window.show();

    const int result = app.exec();
    Logger::instance().info("=== WindowManager exiting (code " +
                            std::to_string(result) + ") ===", "main");
    return result;
}