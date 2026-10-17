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
 */
namespace Theme {

void apply(QApplication& app);

// Creates a push button; role may be nullptr.
QPushButton* button(const QString& text, QWidget* parent,
                    const char* role = nullptr, const QString& tooltip = {});

// Marks a toggle-style button as on/off (re-polishes only when it changes).
void setActive(QAbstractButton* button, bool active);

} // namespace Theme
