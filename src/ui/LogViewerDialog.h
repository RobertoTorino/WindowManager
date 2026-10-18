#pragma once

#include <QDialog>

class QComboBox;
class QPlainTextEdit;

/**
 * Read-only viewer for the application log and the per-game logs written by Logger.
 * Opens on the current game's log.
 */
class LogViewerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LogViewerDialog(QWidget *parent = nullptr);

private:
    void populate();
    void load();
    void openFolder();

    QComboBox      *m_source{nullptr};
    QPlainTextEdit *m_text{nullptr};
};
