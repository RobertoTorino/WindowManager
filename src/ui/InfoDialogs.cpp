#include "InfoDialogs.h"

#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace InfoDialogs {

void showInfo(QWidget* parent)
{
    auto* dlg = new QDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("WindowManager - Info");
    dlg->resize(780, 620);

    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(false);
    browser->setMarkdown(
        "# WindowManager\n\n"
        "Manage the windows of a game or emulator: borderless fullscreen, exact sizes, "
        "monitor moves and more.\n\n"
        "### Quick start\n\n"
        "1. **Add game...** - pick an `.exe`, `.bat` or `.cmd` and give it a name.\n"
        "2. **Start** - launches it (batch files are followed to the real game process).\n"
        "3. Pick the game window in the table, then use the tabs below it.\n\n"
        "### Top row\n\n"
        "| Button | Description |\n"
        "|--------|-------------|\n"
        "| **Scan** | Refresh the list of windows owned by the target. |\n"
        "| **Monitor 1 / 2** | Move the selected window to that monitor (Ctrl+S toggles). |\n"
        "| **Stability** | Keep the window at its position if the game moves it. |\n"
        "| **Focus** | Bring the game window back to the front. |\n"
        "| **Destroy** | Ask the selected window to close. |\n"
        "| **Reset All** | Restore normal styles and forget the saved position. |\n"
        "| **Shutdown** | Terminate this session's game and its loader processes. |\n\n"
        "### Tabs\n\n"
        "| Tab | Description |\n"
        "|-----|-------------|\n"
        "| **Window** | Show/hide/minimize/maximize, windowed or borderless, style toggles, **Fit to monitor...**. |\n"
        "| **Position & Size** | Nudge, resize, exact size, overscan, saved position. |\n"
        "| **Presets** | One-click resolutions. |\n"
        "| **Downscale** | Render the game larger than the monitor and show it scaled down: sharper, nothing falls off the screen. |\n"
        "| **Capture** | Screenshot or burst-capture the window. |\n\n"
        "Green buttons show a state that is currently on.\n\n"
        "### Downscaled view\n\n"
        "Makes the game render more pixels than the monitor has and shows them scaled down on a "
        "full-screen overlay. Keyboard and gamepad go to the game and the mouse is forwarded (for games that read the "
        "mouse from their window). Stop with **Ctrl+Alt+D**, the Stop button, or by closing the game. Only adds detail "
        "if the game re-creates its picture at the window size (RPCS3 does).\n\n"
        "### Fit to monitor\n\n"
        "After **Start**, once the game window has settled, a popup shows the window and picture "
        "rectangles, the target monitor and the exact correction (position X/Y, width, height). "
        "**Apply correction** moves and resizes the window so its picture fills the monitor, then "
        "re-measures to confirm. It is also available as **Fit to monitor...** (Ctrl+F).\n\n"
        "### Predefined sizes (for testing purposes):\n\n"
        "| # | Name | Resolution |\n"
        "|---|------|------------|\n"
        "| 1 | **Full HD (FHD)** | **1920 x 1080** |\n"
        "| 2 | **Quad HD (QHD / 2K)** | **2560 x 1440** |\n"
        "| 3 | **4K Ultra HD (UHD)** | **3840 x 2160** |\n"
        "| 4 | **5K** | **5120 x 2880** |\n"
        "| 5 | **6K** | **6016 x 3384** |\n"
        "| 6 | **8K Ultra HD (UHD-2)** | **7680 x 4320** |\n\n"
        "### Common Window Modes:\n\n"
        "| # | Mode | Description |\n"
        "|---|------|-------------|\n"
        "| 1 | **Fullscreen** | **Covers the entire screen, often exclusive mode for games.** |\n"
        "| 2 | **Windowed** | **Standard resizable window with title bar and borders.** |\n"
        "| 3 | **Borderless** | **Looks fullscreen but technically a window without borders.** |\n"
        "| 4 | **Hidden** | **Window exists but is invisible.** |\n\n"
        "### Window States (WinAPI):\n\n"
        "| # | State | WinAPI constant | Description |\n"
        "|---|-------|-----------------|-------------|\n"
        "| 1 | **Normal / Restored** | **SW_SHOWNORMAL / SW_RESTORE** | **Standard window size.** |\n"
        "| 2 | **Minimized** | **SW_MINIMIZE** | **Shrunk to taskbar.** |\n"
        "| 3 | **Maximized** | **SW_SHOWMAXIMIZED** | **Fills the screen.** |\n"
        "| 4 | **Hidden** | **SW_HIDE** | **Window exists but invisible.** |\n"
        "| 5 | **Shown / Activated** | **SW_SHOW / SW_SHOWNA** | **Visible and optionally activated.** |\n\n"
        "### Window Styles:\n\n"
        "| # | Style | Description |\n"
        "|---|-------|-------------|\n"
        "| 1 | **WS_OVERLAPPEDWINDOW** | **Typical app window: border, title bar, min/max buttons.** |\n"
        "| 2 | **WS_POPUP** | **Borderless window, often used for fullscreen.** |\n"
        "| 3 | **WS_BORDER** | **Thin border around the window.** |\n"
        "| 4 | **WS_CAPTION** | **Adds title bar.** |\n"
        "| 5 | **WS_SYSMENU** | **Adds system menu.** |\n"
        "| 6 | **WS_MINIMIZEBOX** | **Adds minimize/maximize buttons.** |\n"
        "| 7 | **WS_SIZEBOX / WS_THICKFRAME** | **Allows resizing by dragging edges.** |\n"
        "| 8 | **WS_DISABLED** | **Window cannot receive input.** |\n"
        "| 9 | **WS_VISIBLE** | **Initially visible.** |\n\n"
        "### Extended Window Styles:\n\n"
        "| # | Style | Description |\n"
        "|---|-------|-------------|\n"
        "| 1 | **WS_EX_TOPMOST** | **Window stays above all others.** |\n"
        "| 2 | **WS_EX_TOOLWINDOW** | **Small title bar, used for floating tool windows.** |\n"
        "| 3 | **WS_EX_APPWINDOW** | **Forces a window to appear in the taskbar.** |\n"
        "| 4 | **WS_EX_NOACTIVATE** | **Window shows without stealing focus.** |\n"
        "| 5 | **WS_EX_LAYERED** | **Allows transparency and alpha blending.** |\n"

    );

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::accept);
    auto* layout = new QVBoxLayout(dlg);
    layout->addWidget(browser);
    layout->addWidget(buttons);
    dlg->open();
}

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
