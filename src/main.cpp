#include <QApplication>

#include "MainWindow.h"
#include "core/Logger.h"

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
    // Set up log file next to the executable before anything else so every
    // message — including Qt asserts — is captured from the very first line.
    {
        const std::filesystem::path exeDir =
            std::filesystem::path(argv[0]).parent_path();
        Logger::instance().setLogFile(exeDir / "windowmanager.log");
    }
    qInstallMessageHandler(qtMessageHandler);

    Logger::instance().info("=== WindowManager started ===", "main");

    QApplication app(argc, argv);
    QApplication::setApplicationName("WindowManager");
    QApplication::setOrganizationName("WindowManager");

    MainWindow window;
    window.show();

    const int result = app.exec();
    Logger::instance().info("=== WindowManager exiting (code " +
                            std::to_string(result) + ") ===", "main");
    return result;
}