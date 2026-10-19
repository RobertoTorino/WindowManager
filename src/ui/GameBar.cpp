#include "GameBar.h"

#include "Theme.h"

#include <QComboBox>
#include <QCompleter>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>

GameBar::GameBar(QWidget* parent)
    : QWidget(parent)
    , m_combo(new QComboBox(this))
{
    m_combo->setEditable(true);
    m_combo->setInsertPolicy(QComboBox::NoInsert);
    m_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_combo->lineEdit()->setPlaceholderText("Type to search saved games...");
    auto* completer = new QCompleter(m_combo->model(), m_combo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    m_combo->setCompleter(completer);

    auto* select = Theme::button("Add game...", this, nullptr, "Register a game executable or batch file");
    m_start   = Theme::button("Start",   this, "accent", "Launch the selected game");
    m_restart = Theme::button("Restart", this, nullptr, "Kill and relaunch the selected game");
    m_kill    = Theme::button("Kill",    this, "danger", "Terminate the selected game's processes");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(new QLabel("Game", this));
    layout->addWidget(m_combo, 1);
    layout->addWidget(select);
    layout->addSpacing(8);
    layout->addWidget(m_start);
    layout->addWidget(m_restart);
    layout->addWidget(m_kill);

    connect(select,    &QPushButton::clicked, this, &GameBar::selectRequested);
    connect(m_start,   &QPushButton::clicked, this, &GameBar::startRequested);
    connect(m_restart, &QPushButton::clicked, this, &GameBar::restartRequested);
    connect(m_kill,    &QPushButton::clicked, this, &GameBar::killRequested);
    connect(m_combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString path = selectedPath();
        if (!path.isEmpty())
            emit gameChosen(path);
    });
}

void GameBar::setGames(const QList<QPair<QString, QString>>& games, const QString& currentPath)
{
    m_games = games;

    QSignalBlocker blocker(m_combo);
    m_combo->clear();
    for (const auto& g : games) {
        m_combo->addItem(g.first, g.second);
        m_combo->setItemData(m_combo->count() - 1, g.second, Qt::ToolTipRole);
    }

    const int idx = currentPath.isEmpty() ? -1 : m_combo->findData(currentPath);
    // No target yet: show none rather than the first game, which would look selected but is not.
    m_combo->setCurrentIndex(idx);
    if (idx < 0)
        m_combo->lineEdit()->clear();

    const bool any = m_combo->count() > 0;
    m_start->setEnabled(any);
    m_restart->setEnabled(any);
    m_kill->setEnabled(any);
}

QString GameBar::selectedPath() const
{
    const QString path = m_combo->currentData().toString();
    if (!path.isEmpty())
        return path;

    // Free-typed text that matches a saved name
    const QString typed = m_combo->currentText().trimmed();
    for (const auto& g : m_games) {
        if (g.first.compare(typed, Qt::CaseInsensitive) == 0)
            return g.second;
    }
    return {};
}
