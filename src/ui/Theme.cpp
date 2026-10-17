#include "Theme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QPalette>
#include <QPushButton>
#include <QStyle>

namespace {

const char* kStyleSheet = R"qss(
QWidget { font-size: 13px; }
QMainWindow, QDialog { background: #1e1f22; }

QGroupBox {
    border: 1px solid #3a3d41; border-radius: 6px;
    margin-top: 14px; padding: 12px 8px 8px 8px;
    font-weight: 600; color: #b5b8bd;
}
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }

QPushButton {
    background: #2d2f34; border: 1px solid #3d4045; border-radius: 5px;
    padding: 5px 12px; min-height: 20px; color: #e6e6e6;
}
QPushButton:hover    { background: #383b41; }
QPushButton:pressed  { background: #25272b; }
QPushButton:disabled { background: #26282b; color: #6b6e73; border-color: #323438; }
QPushButton:focus    { border-color: #4a90a4; }

QPushButton[role="accent"] { background: #0f7b7b; border-color: #14a0a0; color: white; }
QPushButton[role="accent"]:hover { background: #139494; }
QPushButton[role="danger"] { background: #7a2323; border-color: #a33a3a; color: white; }
QPushButton[role="danger"]:hover { background: #983030; }
QPushButton[role]:disabled { background: #26282b; color: #6b6e73; border-color: #323438; }
QPushButton[active="true"] {
    background: #1b8a4b; border-color: #2fb86a; color: white; font-weight: 600;
}
QPushButton[active="true"]:hover { background: #20a058; }

QTableWidget {
    background: #1b1c1f; alternate-background-color: #212225;
    gridline-color: #2c2e32; border: 1px solid #3a3d41; border-radius: 4px;
    selection-background-color: #0f7b7b; selection-color: white;
}
QHeaderView::section {
    background: #2a2c30; color: #b5b8bd; padding: 5px 8px;
    border: none; border-right: 1px solid #1e1f22; font-weight: 600;
}

QTabWidget::pane { border: 1px solid #3a3d41; border-radius: 6px; top: -1px; }
QTabBar::tab {
    padding: 7px 18px; background: #25272b; color: #b5b8bd;
    border: 1px solid #3a3d41; border-bottom: none;
    border-top-left-radius: 5px; border-top-right-radius: 5px; margin-right: 2px;
}
QTabBar::tab:selected { background: #2f3237; color: white; }
QTabBar::tab:hover:!selected { background: #2b2d31; }

QSpinBox {
    background: #1b1c1f; border: 1px solid #3d4045; border-radius: 4px;
    padding: 3px 24px 3px 6px; min-height: 20px; color: #e6e6e6;
    selection-background-color: #0f7b7b;
}
QSpinBox:focus { border-color: #4a90a4; }
QSpinBox::up-button, QSpinBox::down-button {
    subcontrol-origin: border; width: 18px; background: #2d2f34;
    border-left: 1px solid #3d4045;
}
QSpinBox::up-button   { subcontrol-position: top right;    border-top-right-radius: 4px; }
QSpinBox::down-button { subcontrol-position: bottom right; border-bottom-right-radius: 4px;
                        border-top: 1px solid #3d4045; }
QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: #383b41; }
QSpinBox::up-arrow   { image: url(:/arrow-up.svg);   width: 8px; height: 8px; }
QSpinBox::down-arrow { image: url(:/arrow-down.svg); width: 8px; height: 8px; }

QComboBox, QLineEdit {
    background: #1b1c1f; border: 1px solid #3d4045; border-radius: 4px;
    padding: 3px 6px; min-height: 20px; color: #e6e6e6;
    selection-background-color: #0f7b7b;
}
QComboBox:focus, QLineEdit:focus { border-color: #4a90a4; }
QComboBox QAbstractItemView { background: #1b1c1f; selection-background-color: #0f7b7b; }

QStatusBar { background: #17181a; color: #b5b8bd; }
QStatusBar::item { border: none; }
QMenuBar { background: #17181a; color: #d0d2d6; }
QMenuBar::item:selected { background: #2d2f34; }
QMenu { background: #25272b; color: #e6e6e6; border: 1px solid #3a3d41; }
QMenu::item:selected { background: #0f7b7b; }
QToolTip { background: #25272b; color: #e6e6e6; border: 1px solid #3a3d41; }
QLabel#hint { color: #8b8f95; }
QLabel#fitDelta { font-size: 15px; font-weight: 600; color: #e6e6e6; }
QLabel#fitOk { color: #2fb86a; font-weight: 600; }
)qss";

} // namespace

namespace Theme {

void apply(QApplication& app)
{
    app.setStyle("Fusion");

    QPalette p;
    const QColor base(0x1b, 0x1c, 0x1f), window(0x1e, 0x1f, 0x22), text(0xe6, 0xe6, 0xe6);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, QColor(0x21, 0x22, 0x25));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(0x2d, 0x2f, 0x34));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, QColor(0x25, 0x27, 0x2b));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Highlight, QColor(0x0f, 0x7b, 0x7b));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(0x80, 0x84, 0x8a));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6b, 0x6e, 0x73));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6b, 0x6e, 0x73));
    app.setPalette(p);

    app.setStyleSheet(kStyleSheet);
}

QPushButton* button(const QString& text, QWidget* parent, const char* role, const QString& tooltip)
{
    auto* b = new QPushButton(text, parent);
    if (role)
        b->setProperty("role", role);
    if (!tooltip.isEmpty())
        b->setToolTip(tooltip);
    return b;
}

void setActive(QAbstractButton* button, bool active)
{
    if (!button || button->property("active").toBool() == active)
        return;
    button->setProperty("active", active);
    button->style()->unpolish(button);
    button->style()->polish(button);
}

} // namespace Theme
