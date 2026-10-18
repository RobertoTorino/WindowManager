#include "InfoDialogs.h"

#include "core/BuildInfo.h"

#include <QDate>
#include <QMessageBox>

namespace InfoDialogs {

void showAbout(QWidget* parent)
{
    QMessageBox::about(
        parent,
        "About WindowManager",
        QString("<b>WindowManager</b> %1<br><br>"
                "&copy; %2 RobertoTorino<br><br>"
                "Window management, true borderless fullscreen, fit to screen, "
                "repositioning and moving windows between monitors.")
            .arg(BuildInfo::description())
            .arg(QDate::currentDate().year()));
}

} // namespace InfoDialogs
