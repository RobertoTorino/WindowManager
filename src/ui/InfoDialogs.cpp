#include "InfoDialogs.h"

#include <QDate>
#include <QMessageBox>

namespace InfoDialogs {

void showAbout(QWidget* parent)
{
    QMessageBox::about(
        parent,
        "About WindowManager",
        QString("<b>WindowManager</b> %1<br>"
                "Built %2 %3<br><br>"
                "&copy; %4 RobertoTorino<br><br>"
                "Window management, true borderless fullscreen, fit to screen, "
                "repositioning and moving windows between monitors.")
            .arg(APP_VERSION_STR)
            .arg(__DATE__)
            .arg(__TIME__)
            .arg(QDate::currentDate().year()));
}

} // namespace InfoDialogs
