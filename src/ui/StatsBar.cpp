#include "StatsBar.h"

#include "WindowManagerController.h"
#include "stats/StatsBackend.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStringList>

#include <algorithm>

namespace {

const char *kFpsTip = "Frames per second over the last second; the 1 % low is the frame rate of the "
                      "slowest 1 % of frames in the last 5 seconds.";

} // namespace

StatsBar::StatsBar(WindowManagerController *controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(4, 0, 4, 0);
    h->setSpacing(18);

    m_idle = new QLabel(this);
    m_idle->setObjectName("hint");
    h->addWidget(m_idle);

    m_fps  = addItem("FPS", kFpsTip);
    m_cpu  = addItem("CPU", {});
    m_gpu  = addItem("GPU", {});
    m_ram  = addItem("RAM", "The game's working set: the memory it has in RAM now.");
    m_vram = addItem("VRAM", "Video memory the game has on its own GPU (dedicated memory).");
    for (const Item &item : {m_fps, m_cpu, m_gpu, m_ram, m_vram})
        h->addWidget(item.box);
    h->addStretch(1);

    m_warning = new QLabel(this);
    m_warning->setObjectName("statWarn");
    m_power = new QLabel(this);
    m_power->setObjectName("statValue");
    h->addWidget(m_warning);
    h->addWidget(m_power);

    display(Stats::GameStats{});
}

bool StatsBar::available()
{
    return statsBackend().supported();
}

StatsBar::Item StatsBar::addItem(const QString &caption, const QString &tooltip)
{
    Item item;
    item.box = new QWidget(this);
    auto *h = new QHBoxLayout(item.box);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(5);
    auto *label = new QLabel(caption, item.box);
    label->setObjectName("hint");
    item.value = new QLabel(item.box);
    item.value->setObjectName("statValue");
    h->addWidget(label);
    h->addWidget(item.value);
    item.box->setToolTip(tooltip);
    return item;
}

void StatsBar::display(const Stats::GameStats &stats)
{
    using Stats::FpsSource;
    const bool running = stats.running;
    m_idle->setVisible(!running);
    m_idle->setText(m_controller->targetExecutable().isEmpty() ? "No game selected" : "Game not running");
    for (const Item &item : {m_fps, m_cpu, m_gpu, m_ram, m_vram})
        item.box->setVisible(running);

    const auto hasWarning = [&stats](const char *text) {
        return std::any_of(stats.warnings.begin(), stats.warnings.end(),
                           [text](const Stats::Warning &w) { return w.text == text; });
    };
    const auto pct = [](double v) { return QString::number(qRound(v)) + " %"; };

    if (running) {
        QString fps, fpsTip = kFpsTip;
        if (stats.fps.valid)
            fps = QString("%1 · 1% low %2").arg(qRound(stats.fps.fps)).arg(qRound(stats.fps.low1Fps));
        else if (stats.fpsSource == FpsSource::NoPermission)
            fps = "needs admin";
        else if (stats.fpsSource == FpsSource::Running)
            fps = "-";
        else
            fps = "n/a";
        if (stats.fps.valid)
            fpsTip += QString("\nWorst frame: %1 ms.").arg(stats.fps.worstFrameMs, 0, 'f', 1);
        else if (stats.fpsSource == FpsSource::Running)
            fpsTip += "\nNo frames seen yet. Only DirectX games are counted (not Vulkan or OpenGL).";
        if (!stats.fpsError.isEmpty())
            fpsTip += "\n" + stats.fpsError;
        m_fps.value->setText(fps);
        m_fps.box->setToolTip(fpsTip);

        m_cpu.value->setText(stats.cpu ? pct(*stats.cpu) : QString("-"));
        m_cpu.box->setToolTip(QString("The game's share of the whole CPU (all cores together).%1")
                                  .arg(stats.systemCpu ? "\nWhole system: " + pct(*stats.systemCpu) : QString()));
        Theme::setWarning(m_cpu.value, hasWarning("CPU fully used"));

        QString gpu = stats.gpu ? pct(*stats.gpu) : QString("n/a");
        if (!stats.gpuName.isEmpty())
            gpu += " · " + Stats::shortAdapterName(stats.gpuName);
        m_gpu.value->setText(gpu);
        m_gpu.box->setToolTip(QString("The game's busiest GPU engine, as in Task Manager.%1")
                                  .arg(stats.gpuName.isEmpty() ? QString() : "\nRenders on: " + stats.gpuName));
        Theme::setWarning(m_gpu.value, hasWarning("Integrated GPU"));

        m_ram.value->setText(stats.ram ? Stats::formatBytes(*stats.ram) : QString("-"));
        m_vram.box->setVisible(stats.vram.has_value());
        if (stats.vram)
            m_vram.value->setText(Stats::formatBytes(*stats.vram));
    }

    m_warning->setVisible(!stats.warnings.empty());
    if (!stats.warnings.empty()) {
        QString text = QString::fromUtf8("⚠ ") + stats.warnings.front().text;
        if (stats.warnings.size() > 1)
            text += QString(" (+%1)").arg(stats.warnings.size() - 1);
        QStringList details;
        for (const auto &w : stats.warnings)
            details << w.detail;
        m_warning->setText(text);
        m_warning->setToolTip(details.join("\n\n"));
    }

    const Stats::Power &p = stats.power;
    m_power->setVisible(p.known);
    if (p.known) {
        QString text = p.onBattery ? "On battery" : "Plugged in";
        if (p.batteryPercent >= 0)
            text += QString(" · %1 %").arg(p.batteryPercent);
        m_power->setText(text);
        QStringList tip;
        if (!p.plan.isEmpty())
            tip << "Power plan: " + p.plan;
        if (!p.mode.isEmpty())
            tip << "Power mode: " + p.mode;
        if (p.batterySaver)
            tip << "Battery saver is on.";
        m_power->setToolTip(tip.join("\n"));
        Theme::setWarning(m_power, running && (p.onBattery || p.batterySaver || p.efficiencyMode));
    }
}
