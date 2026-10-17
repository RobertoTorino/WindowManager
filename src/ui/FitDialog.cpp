#include "FitDialog.h"

#include "Theme.h"
#include "WindowManagerController.h"
#include "window/MonitorHelper.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QString signedPx(int v)
{
    if (v == 0)
        return QStringLiteral("0 px");
    return QString("%1%2 px").arg(v > 0 ? "+" : "").arg(v);
}

QLabel *cellLabel(const QString &text, QWidget *parent, bool header = false)
{
    auto *l = new QLabel(text, parent);
    if (header)
        l->setObjectName("hint");
    l->setAlignment(header ? Qt::AlignCenter : (Qt::AlignRight | Qt::AlignVCenter));
    return l;
}

} // namespace

FitDialog::FitDialog(WindowManagerController *controller, QWidget *parent)
    : QDialog(parent)
    , m_controller(controller)
{
    setWindowTitle("Fit window to monitor");
    setModal(true);
    setMinimumWidth(560);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 12);
    layout->setSpacing(10);

    m_header = new QLabel(this);
    m_header->setWordWrap(true);
    layout->addWidget(m_header);

    // --- options --------------------------------------------------------
    m_monitor = new QComboBox(this);
    for (const auto &m : MonitorHelper::enumerateMonitors())
        m_monitor->addItem(QString("Monitor %1  -  %2 x %3 at (%4, %5)")
                               .arg(m.index).arg(m.w).arg(m.h).arg(m.x).arg(m.y),
                           m.index);
    m_area = new QComboBox(this);
    m_area->addItem("Full monitor (true borderless)", false);
    m_area->addItem("Work area (keeps the taskbar visible)", true);
    m_strip = new QCheckBox("Remove title bar and borders first", this);
    m_strip->setChecked(true);

    auto *form = new QFormLayout;
    form->addRow("Target monitor", m_monitor);
    form->addRow("Fit to", m_area);
    form->addRow(QString(), m_strip);
    layout->addLayout(form);

    // --- measurements ---------------------------------------------------
    auto *measureBox = new QGroupBox("Measured", this);
    m_table = new QGridLayout(measureBox);
    m_table->setHorizontalSpacing(18);
    const QStringList heads{"", "X", "Y", "Width", "Height"};
    for (int c = 0; c < heads.size(); ++c)
        m_table->addWidget(cellLabel(heads[c], measureBox, true), 0, c);
    const QStringList rows{"Window", "Picture (client area)", "Target"};
    for (int r = 0; r < rows.size(); ++r) {
        auto *l = new QLabel(rows[r], measureBox);
        m_table->addWidget(l, r + 1, 0);
    }
    m_table->setColumnStretch(0, 1);
    m_frameInfo = new QLabel(measureBox);
    m_frameInfo->setObjectName("hint");
    m_frameInfo->setWordWrap(true);
    m_table->addWidget(m_frameInfo, rows.size() + 1, 0, 1, heads.size());
    layout->addWidget(measureBox);

    // --- correction -----------------------------------------------------
    auto *fixBox = new QGroupBox("Correction", this);
    auto *fix = new QVBoxLayout(fixBox);
    m_positionLine = new QLabel(fixBox);
    m_positionLine->setObjectName("fitDelta");
    m_sizeLine = new QLabel(fixBox);
    m_sizeLine->setObjectName("fitDelta");
    m_summary = new QLabel(fixBox);
    m_summary->setWordWrap(true);
    fix->addWidget(m_positionLine);
    fix->addWidget(m_sizeLine);
    fix->addWidget(m_summary);
    layout->addWidget(fixBox);

    // --- buttons --------------------------------------------------------
    m_dontAsk = new QCheckBox("Don't open this automatically after launching a game", this);
    m_dontAsk->setToolTip("You can turn it back on in View > Check window fit after launching a game");
    layout->addWidget(m_dontAsk);

    auto *buttons = new QDialogButtonBox(this);
    m_apply = Theme::button("Apply correction", this, "accent");
    m_apply->setDefault(true);
    buttons->addButton(m_apply, QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // First measurement picks the monitor the window is on.
    recompute();
    if (m_last.valid) {
        if (const int idx = m_monitor->findData(m_last.monitorIndex); idx >= 0) {
            QSignalBlocker block(m_monitor);
            m_monitor->setCurrentIndex(idx);
        }
    }

    connect(m_monitor, qOverload<int>(&QComboBox::currentIndexChanged), this, &FitDialog::recompute);
    connect(m_area, qOverload<int>(&QComboBox::currentIndexChanged), this, &FitDialog::recompute);
    connect(m_strip, &QCheckBox::toggled, this, &FitDialog::recompute);
}

WindowManager::FitRequest FitDialog::request() const
{
    WindowManager::FitRequest r;
    r.monitorIndex = m_monitor->currentData().toInt();
    r.workArea = m_area->currentData().toBool();
    r.stripFrame = m_strip->isChecked();
    return r;
}

void FitDialog::setCell(int row, int column, const QString &text)
{
    if (auto *item = m_table->itemAtPosition(row, column))
        if (auto *label = qobject_cast<QLabel *>(item->widget())) {
            label->setText(text);
            return;
        }
    m_table->addWidget(cellLabel(text, nullptr), row, column);
}

void FitDialog::recompute()
{
    auto request = this->request();
    // Before the first measurement the combo has no meaningful selection yet.
    if (m_monitor->currentIndex() < 0 || !m_last.valid)
        request.monitorIndex = 0;

    const bool ok = m_controller->measureFit(request, m_last);
    if (!ok) {
        m_header->setText(QString("<b>Cannot measure the window.</b><br>%1").arg(m_last.error.toHtmlEscaped()));
        m_positionLine->clear();
        m_sizeLine->clear();
        m_summary->clear();
        m_apply->setEnabled(false);
        return;
    }

    m_header->setText(QString("Game window: <b>%1</b>  <span style='color:#8b8f95'>(%2)</span>")
                          .arg(m_last.title.toHtmlEscaped(), m_last.className.toHtmlEscaped()));

    m_strip->setEnabled(m_last.hasFrame);
    if (!m_last.hasFrame)
        m_strip->setToolTip("This window has no title bar or borders.");

    auto fill = [this](int row, const Fit::Rect &r) {
        setCell(row, 1, QString::number(r.left));
        setCell(row, 2, QString::number(r.top));
        setCell(row, 3, QString::number(r.width()));
        setCell(row, 4, QString::number(r.height()));
    };
    fill(1, m_last.outer);
    fill(2, m_last.client);
    fill(3, m_last.target);

    const auto in = Fit::measureInsets(m_last.outer, m_last.client);
    QString frame = m_last.hasFrame
        ? QString("Frame around the picture: left %1, top %2, right %3, bottom %4 px")
              .arg(in.left).arg(in.top).arg(in.right).arg(in.bottom)
        : QString("No title bar or borders (borderless window).");
    const int invL = m_last.visibleFrame.left - m_last.outer.left;
    const int invR = m_last.outer.right - m_last.visibleFrame.right;
    const int invB = m_last.outer.bottom - m_last.visibleFrame.bottom;
    if (invL > 0 || invR > 0 || invB > 0)
        frame += QString("  |  invisible resize border: left %1, right %2, bottom %3 px")
                     .arg(invL).arg(invR).arg(invB);
    m_frameInfo->setText(frame);

    const auto &p = m_last.plan;
    m_positionLine->setText(QString("Position:  X %1     Y %2").arg(signedPx(p.dx), signedPx(p.dy)));
    m_sizeLine->setText(QString("Size:  Width %1     Height %2").arg(signedPx(p.dw), signedPx(p.dh)));

    if (!p.needsChange()) {
        m_summary->setObjectName("fitOk");
        m_summary->setText("The picture already fits the target exactly. Nothing to do.");
    } else {
        m_summary->setObjectName("");
        m_summary->setText(m_last.stripFrame
            ? "The title bar and borders are removed first, so the window rectangle becomes "
              "the picture. Then it is moved and resized by the amounts above."
            : "The window is moved and resized by the amounts above so that its picture "
              "(not the title bar) covers the target.");

        // Keeping the frame pushes the title bar/borders outside the monitor. On a
        // multi-monitor setup that puts part of the window on a neighbour, and
        // Windows may rescale it there (different DPI), so the fit can drift.
        const auto &d = p.desiredOuter;
        const auto &mon = m_last.monitor;
        if (!m_last.stripFrame && m_last.hasFrame
            && (d.left < mon.left || d.top < mon.top || d.right > mon.right || d.bottom > mon.bottom)) {
            m_summary->setText(m_summary->text() +
                "\n\nNote: keeping the title bar places it outside this monitor, possibly on a "
                "neighbouring one where Windows may rescale it. Removing the title bar and "
                "borders is recommended.");
        }
    }
    // object name drives the colour: re-polish
    m_summary->style()->unpolish(m_summary);
    m_summary->style()->polish(m_summary);

    m_apply->setEnabled(p.needsChange());

    // Wrapped labels change height with their text; let the dialog follow.
    layout()->activate();
    resize(width(), qMax(minimumSizeHint().height(), 0));
}

void FitDialog::done(int result)
{
    if (result == QDialog::Accepted && m_last.valid)
        m_controller->applyFit(request());
    if (m_dontAsk->isChecked())
        m_controller->setFitCheckEnabled(false);
    QDialog::done(result);
}
