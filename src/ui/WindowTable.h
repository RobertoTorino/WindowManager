#pragma once

#include <vector>

#include <QTableWidget>

#include "window/WindowManager.h"

/** Table of top-level windows belonging to the target executable. */
class WindowTable : public QTableWidget
{
    Q_OBJECT

public:
    explicit WindowTable(QWidget* parent = nullptr);

    // Repopulates the table and highlights activeHwnd. Never emits windowChosen.
    void setWindows(const std::vector<WindowManager::WindowInfo>& windows, quintptr activeHwnd);

    // Text shown in the centre while the table has no rows.
    void setEmptyMessage(const QString& text);

protected:
    bool viewportEvent(QEvent* event) override;

signals:
    // Emitted only when the user picks a different row.
    void windowChosen(quintptr hwnd);

private:
    std::vector<quintptr> m_hwnds;
    bool m_populating{false};
    QString m_emptyMessage;
};
