#pragma once

#include <QString>

class QAbstractButton;
class QApplication;
class QPushButton;
class QWidget;

/**
 * Application-wide look: dark Fusion palette plus one stylesheet.
 * Widgets opt into variants through dynamic properties instead of inline
 * stylesheets, so all colours live in Theme.cpp.
 *
 *   role   = "accent" | "danger"   (button emphasis)
 *   active = true                  (toggle button currently "on")
 *   warning = true                 (a value that explains a slow game or a wrong picture)
 */
namespace Theme {

void apply(QApplication& app);

// Creates a push button; role may be nullptr.
QPushButton* button(const QString& text, QWidget* parent,
                    const char* role = nullptr, const QString& tooltip = {});

// Marks a toggle-style button as on/off (re-polishes only when it changes).
void setActive(QAbstractButton* button, bool active);

// Marks a value label as a warning (same re-polish rule as setActive).
void setWarning(QWidget* widget, bool warning);

} // namespace Theme
