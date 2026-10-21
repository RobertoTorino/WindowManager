#include "ReportsPanel.h"

#include "ui/ReportCharts.h"
#include "ui/SessionRecorder.h"
#include "ui/Theme.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString number(double v, const QString &unit = {})
{
    return v < 0 ? QString("-") : QString::number(qRound(v)) + unit;
}

QString played(qint64 seconds)
{
    return seconds >= 3600 ? QString("%1 h %2 min").arg(seconds / 3600).arg(seconds % 3600 / 60)
           : seconds >= 60 ? QString("%1 min").arg(seconds / 60)
                           : QString("%1 s").arg(seconds);
}

std::optional<QJsonObject> readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? std::optional<QJsonObject>(doc.object()) : std::nullopt;
}

} // namespace

ReportsPanel::ReportsPanel(WindowManagerController *controller, QWidget *parent, bool compact)
    : ControlPanel(controller, parent)
    , m_compact(compact)
{
    m_game = new QComboBox(this);
    m_game->setMinimumWidth(200);
    m_game->setToolTip("The games with recorded sessions");
    m_compare = Theme::button("Compare picture", this, "accent",
                              "Magpie's picture against a plain stretch of the same frame, on a still screen.\n"
                              "The game gets the focus for 2 s, then both are captured and scored.");
    auto *refreshButton = Theme::button("Refresh", this, nullptr, "Read the reports again");
    auto *folder = Theme::button("Open folder", this, nullptr, "Open this game's reports folder (CSV, JSON, PNG)");
    QPushButton *larger = nullptr;
    if (m_compact)
        larger = Theme::button("Larger view", this, nullptr, "The reports in a window of their own");

    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel("Game", this));
    top->addWidget(m_game);
    top->addWidget(m_compare);
    top->addStretch(1);
    top->addWidget(refreshButton);
    top->addWidget(folder);
    if (larger)
        top->addWidget(larger);

    m_sessionsTable = new QTableWidget(0, 5, this);
    m_sessionsTable->setHorizontalHeaderLabels({"Started", "Played", "Avg FPS", "GPU", "Setups"});
    m_sessionsTable->verticalHeader()->hide();
    m_sessionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sessionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sessionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sessionsTable->horizontalHeader()->setStretchLastSection(true);
    m_sessionsTable->setMinimumWidth(260);

    // Right: the three views, picked with buttons
    auto *viewRow = new QHBoxLayout;
    const QString names[3] = {"Session", "Setups", "Pictures"};
    const QString tips[3] = {"The picked session over time: FPS and GPU, coloured by setup",
                             "This game's setups compared over all its sessions",
                             "The picture comparisons (Compare picture)"};
    for (int i = 0; i < 3; ++i) {
        m_viewButtons[i] = Theme::button(names[i], this, nullptr, tips[i]);
        m_viewButtons[i]->setCheckable(true);
        viewRow->addWidget(m_viewButtons[i]);
        connect(m_viewButtons[i], &QPushButton::clicked, this, [this, i]() { showView(i); });
    }
    viewRow->addStretch(1);
    m_hint = new QLabel(this);
    m_hint->setObjectName("hint");
    viewRow->addWidget(m_hint);

    m_sessionChart = new SessionChart(this);
    m_setupChart = new SetupChart(this);
    auto *pictures = new QWidget(this);
    m_pictureList = new QListWidget(pictures);
    m_pictureList->setMaximumWidth(200);
    m_picture = new ImageView(pictures);
    m_pictureText = new QLabel(pictures);
    m_pictureText->setWordWrap(true);
    auto *pictureSide = new QVBoxLayout;
    pictureSide->addWidget(m_pictureText);
    pictureSide->addWidget(m_picture, 1);
    auto *ph = new QHBoxLayout(pictures);
    ph->setContentsMargins(0, 0, 0, 0);
    ph->addWidget(m_pictureList);
    ph->addLayout(pictureSide, 1);
    m_views = new QStackedWidget(this);
    m_views->addWidget(m_sessionChart);
    m_views->addWidget(m_setupChart);
    m_views->addWidget(pictures);

    auto *right = new QWidget(this);
    auto *rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->addLayout(viewRow);
    rv->addWidget(m_views, 1);

    auto *split = new QSplitter(this);
    split->addWidget(m_sessionsTable);
    split->addWidget(right);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 5);

    auto *v = new QVBoxLayout(this);
    v->addLayout(top);
    v->addWidget(split, 1);
    if (m_compact)
        setMinimumHeight(300);

    connect(m_game, &QComboBox::currentIndexChanged, this, &ReportsPanel::loadGame);
    connect(m_compare, &QPushButton::clicked, this, &ReportsPanel::compareRequested);
    connect(refreshButton, &QPushButton::clicked, this, &ReportsPanel::refresh);
    connect(folder, &QPushButton::clicked, this, [this]() {
        const QString dir = m_dir.isEmpty() ? SessionRecorder::reportsDir() : m_dir;
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    if (larger)
        connect(larger, &QPushButton::clicked, this, &ReportsPanel::openLarger);
    connect(m_sessionsTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        const auto rows = m_sessionsTable->selectionModel()->selectedRows();
        if (!rows.isEmpty())
            showSession(rows.first().row());
    });
    connect(m_pictureList, &QListWidget::currentRowChanged, this, &ReportsPanel::showComparison);
    connect(m_pictureList, &QListWidget::itemDoubleClicked, this, [this]() {
        const int row = m_pictureList->currentRow();
        if (row >= 0 && row < int(m_comparisons.size()))
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_dir + "/" + m_comparisons[size_t(row)].image));
    });

    showView(1);
    refresh();
}

void ReportsPanel::refresh()
{
    loadGames();
}

void ReportsPanel::showGame(const QString &game)
{
    loadGames();
    const int i = m_game->findData(Report::folderName(game));
    if (i >= 0)
        m_game->setCurrentIndex(i);
    loadGame();
}

void ReportsPanel::loadGames()
{
    const QString keep = m_game->currentData().toString();
    QSignalBlocker block(m_game);
    m_game->clear();
    const QDir root(SessionRecorder::reportsDir());
    for (const QString &folder : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase)) {
        const QStringList files = QDir(root.filePath(folder)).entryList({"*.json"}, QDir::Files);
        if (files.isEmpty())
            continue;
        QString name = folder;   // the game's own name, from one of its files
        if (const auto o = readJson(root.filePath(folder) + "/" + files.first()))
            name = o->value("game").toString(folder);
        m_game->addItem(name, folder);
    }
    if (m_game->count() == 0) {
        m_game->addItem("(no reports yet)", QString());
        m_game->setEnabled(false);
    } else {
        m_game->setEnabled(true);
        const int i = m_game->findData(keep);
        m_game->setCurrentIndex(i >= 0 ? i : 0);
    }
    loadGame();
}

void ReportsPanel::loadGame()
{
    m_sessions.clear();
    m_comparisons.clear();
    const QString folder = m_game->currentData().toString();
    m_dir = folder.isEmpty() ? QString() : SessionRecorder::reportsDir() + "/" + folder;
    if (!m_dir.isEmpty()) {
        const QDir dir(m_dir);
        for (const QString &file : dir.entryList({"*.json"}, QDir::Files, QDir::Name | QDir::Reversed)) {
            const auto o = readJson(dir.filePath(file));
            if (!o)
                continue;
            if (file.startsWith("compare-")) {
                if (auto c = Report::comparisonFromJson(*o))
                    m_comparisons.push_back(*c);
            } else if (auto s = Report::sessionFromJson(*o)) {
                m_sessions.push_back(*s);
            }
        }
    }

    const auto setups = Report::compareSetups(m_sessions);
    m_setupOrder.clear();
    std::vector<Report::Segment> compared;
    for (const auto &s : setups) {
        m_setupOrder << s.setup;
        if (!ReportColors::isState(s.setup))
            compared.push_back(s);
    }
    m_setupChart->setSetups(compared, m_setupOrder);

    m_sessionsTable->setRowCount(int(m_sessions.size()));
    for (int r = 0; r < int(m_sessions.size()); ++r) {
        const Report::Session &s = m_sessions[size_t(r)];
        QStringList names;
        for (const auto &seg : s.segments)
            names << seg.setup;
        const QStringList cells{s.start.toString("yyyy-MM-dd HH:mm"), played(s.start.secsTo(s.end)),
                                number(s.overall.fpsAvg), number(s.overall.gpuAvg, " %"), names.join(", ")};
        for (int c = 0; c < cells.size(); ++c) {
            auto *item = new QTableWidgetItem(cells[c]);
            item->setToolTip(c == 4 ? names.join("\n") : QString());
            m_sessionsTable->setItem(r, c, item);
        }
    }
    m_sessionsTable->resizeColumnsToContents();

    m_pictureList->clear();
    for (const auto &c : m_comparisons)
        m_pictureList->addItem(QString("%1  %2%3 %")
                                   .arg(c.time.toString("yyyy-MM-dd HH:mm"))
                                   .arg(c.gainPercent >= 0 ? "+" : "")
                                   .arg(c.gainPercent, 0, 'f', 1));
    m_compare->setEnabled(true);

    m_loading = true;
    if (!m_sessions.empty())
        m_sessionsTable->selectRow(0);
    else
        m_sessionChart->setSession({}, {}, {});
    m_loading = false;
    if (!m_comparisons.empty())
        m_pictureList->setCurrentRow(0);
    else
        showComparison(-1);
    m_hint->setText(m_sessions.empty() ? QString("Sessions are recorded while a game runs (at least %1 s)")
                                                 .arg(Report::kMinSessionSeconds)
                                       : QString("%1 session(s)").arg(m_sessions.size()));
}

void ReportsPanel::showSession(int row)
{
    if (row < 0 || row >= int(m_sessions.size()))
        return;
    const Report::Session &s = m_sessions[size_t(row)];
    QFile f(m_dir + "/" + s.samplesFile);
    std::vector<Report::Sample> samples;
    if (f.open(QIODevice::ReadOnly))
        samples = Report::readSamples(QString::fromUtf8(f.readAll()));
    m_sessionChart->setSession(samples, m_setupOrder, s.start.toString("yyyy-MM-dd HH:mm"));
    if (!m_loading && m_views->currentIndex() == 1)
        showView(0);   // picking a session shows it
}

void ReportsPanel::showView(int index)
{
    m_views->setCurrentIndex(index);
    for (int i = 0; i < 3; ++i)
        m_viewButtons[i]->setChecked(i == index);
}

void ReportsPanel::showComparison(int row)
{
    if (row < 0 || row >= int(m_comparisons.size())) {
        m_pictureText->clear();
        m_picture->setImage({}, "No picture comparisons yet: start the game with Magpie, put it on a still screen "
                                "and press Compare picture.");
        return;
    }
    const Report::Comparison &c = m_comparisons[size_t(row)];
    m_picture->setImage(QPixmap(m_dir + "/" + c.image), "The picture file is missing: " + c.image);
    const QString scaler = c.setup.section(' ', 0, 1);
    const QString verdict = c.gainPercent >= 1    ? QString("<b>%1 is %2 % sharper</b> than a plain stretch").arg(scaler).arg(c.gainPercent, 0, 'f', 1)
                            : c.gainPercent <= -1 ? QString("<b>%1 is %2 % softer</b> than a plain stretch").arg(scaler).arg(-c.gainPercent, 0, 'f', 1)
                                                  : QString("<b>%1 and a plain stretch are about as sharp</b>").arg(scaler);
    m_pictureText->setText(QString("%1 &nbsp;(%2)<br>Sharpness: %3 for %4, %5 for the bilinear stretch, %6 for the game's "
                                   "own frame at %7%8%9. Left to right below: the game's pixels, the plain stretch, %4.")
                               .arg(verdict, c.setup.toHtmlEscaped())
                               .arg(c.sharpScaled, 0, 'f', 1)
                               .arg(scaler)
                               .arg(c.sharpBilinear, 0, 'f', 1)
                               .arg(c.sharpSource, 0, 'f', 1)
                               .arg(c.sourceW)
                               .arg(QString::fromUtf8(" × "))
                               .arg(c.sourceH));
    m_picture->setToolTip(QString("%1\nSharpness: game frame %2, bilinear stretch %3, scaled %4 (%5%6 %)\n"
                                  "Double-click the list entry to open the full image.")
                              .arg(c.setup)
                              .arg(c.sharpSource, 0, 'f', 1)
                              .arg(c.sharpBilinear, 0, 'f', 1)
                              .arg(c.sharpScaled, 0, 'f', 1)
                              .arg(c.gainPercent >= 0 ? "+" : "")
                              .arg(c.gainPercent, 0, 'f', 1));
}

void ReportsPanel::openLarger()
{
    auto *dialog = new QDialog(window());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("Game reports");
    auto *panel = new ReportsPanel(m_controller, dialog, false);
    auto *v = new QVBoxLayout(dialog);
    v->addWidget(panel);
    connect(panel, &ReportsPanel::compareRequested, this, &ReportsPanel::compareRequested);
    dialog->resize(1200, 760);
    dialog->show();
    if (!m_game->currentData().toString().isEmpty())
        panel->showGame(m_game->currentText());
}
