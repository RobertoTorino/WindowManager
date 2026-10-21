#include "PictureInfo.h"

#include <QStringList>

#include <algorithm>

namespace Picture {

namespace {

const QString kTimes = QString::fromUtf8(" × ");
const QString kArrow = QString::fromUtf8(" → ");
const QString kDot = QString::fromUtf8(" · ");

QString pct(double v) { return QString::number(qRound(v)) + " %"; }

QString biggerProblem(const Facts& f, bool magpie)
{
    const QString start = QString("The game's window (%1) is bigger than its monitor (%2): ")
                              .arg(sizeText(f.window), sizeText(f.monitor));
    if (!magpie)
        return start + "part of the picture is off the screen.";
    return start + "Magpie only scales up, so it shrinks the picture. Give the game a resolution below the "
                   "monitor's (Game config tab; with JConfig, its DX resolution too).";
}

// "JConfigTK7R2.exe is in front", for the text while Magpie waits for the game
QString inFront(const Facts& f)
{
    return f.foreground.isEmpty() ? QString("no window is in front") : f.foreground + " is in front";
}

QString windowMode(const Facts& f)
{
    if (f.fullScreen)
        return "borderless full screen";
    if (f.maximized)
        return "maximized";
    return "windowed";
}

} // namespace

int scalePercent(Size from, Size to)
{
    if (!from.valid() || !to.valid())
        return 100;
    const double ratio = std::min(double(to.w) / from.w, double(to.h) / from.h);
    return qRound(ratio * 100);
}

QString direction(int percent)
{
    if (percent > 100)
        return "upscaling";
    if (percent < 100)
        return "downscaling";
    return "1:1";
}

QString sizeText(Size s)
{
    return QString::number(s.w) + kTimes + QString::number(s.h);
}

Result evaluate(const Facts& f)
{
    Result r;
    r.scalerName = "None";
    if (!f.gameRunning) {
        r.picture = "Game not running";
        return r;
    }
    const bool bigger = f.windowFound && f.monitor.valid() && (f.window.w > f.monitor.w || f.window.h > f.monitor.h);
    bool paused = false;
    bool starting = false;

    if (f.viewRunning && f.viewRender.valid() && f.viewShown.valid()) {
        r.scaler = Scaler::DownscaledView;
        r.scalerName = "Downscaled view";
        r.from = f.viewRender;
        r.to = f.viewShown;
    } else if (!f.magpieMode.isEmpty()) {
        r.scalerName = "Magpie " + f.magpieMode;
        if (f.magpieScaling && f.magpieSource.valid() && f.magpieDest.valid()) {
            r.scaler = Scaler::Magpie;
            r.from = f.magpieSource;
            r.to = f.magpieDest;
            paused = !f.gameInFront;
        } else if (f.magpieStarting) {
            starting = true;
        } else if (!f.magpieRunning) {
            r.problems.push_back("Magpie is not running. It starts when the game is started with Start in this app.");
        } else if (f.maximized || f.fullScreen) {
            r.problems.push_back("Magpie does not scale a maximized or borderless full-screen window: put the game "
                                 "in a window at the size it should render (Windowed, or the game's own setting).");
        } else if (!f.gameInFront) {
            paused = true;
        } else {
            r.problems.push_back("Magpie runs but does not scale the game. Its log is magpie/logs/magpie.log next "
                                 "to this app (a game running as administrator cannot be scaled).");
        }
        if (bigger)
            r.problems.push_back(biggerProblem(f, true));
    } else if (bigger) {
        r.problems.push_back(biggerProblem(f, false));
    }

    r.paused = paused;
    r.starting = starting;
    if (r.scaler != Scaler::None) {
        r.percent = scalePercent(r.from, r.to);
        r.picture = sizeText(r.from) + kArrow + sizeText(r.to) + kDot + QString::number(r.percent) + " %" + kDot
                    + direction(r.percent);
        r.status = QString("%1: %2%3%4 (%5 %, %6)")
                       .arg(r.scalerName, sizeText(r.from), kArrow, sizeText(r.to))
                       .arg(r.percent)
                       .arg(direction(r.percent));
        if (paused) {
            r.picture += kDot + "paused: " + inFront(f) + ", click the game";
            r.status += ", paused (" + inFront(f) + ")";
        }
        return r;
    }

    if (starting)
        r.picture = "starting";
    else if (!f.magpieMode.isEmpty())
        r.picture = paused ? "waiting: Magpie scales only while the game is in front; " + inFront(f) + ", click the game"
                           : QString("not scaling");
    else if (!f.windowFound)
        r.picture = "game window not found";
    else if (f.fullScreen)
        r.picture = "full screen " + sizeText(f.window) + ", not scaled (1:1)";
    else
        r.picture = "window " + sizeText(f.window) + ", not scaled (1:1)";
    r.status = (r.scaler == Scaler::None && f.magpieMode.isEmpty() ? QString("No scaler") : r.scalerName) + ": "
               + r.picture;
    return r;
}

std::vector<Line> overlayLines(const Facts& f, const Result& r, const Stats::GameStats& stats)
{
    std::vector<Line> out;
    out.push_back({"Scaler", r.scalerName});
    out.push_back({"Picture", r.picture});
    if (!f.gameRunning)
        return out;

    if (f.filesChanged && (f.fileResolution.valid() || f.renderScale > 0)) {
        QStringList parts;
        if (f.fileResolution.valid())
            parts << "resolution " + sizeText(f.fileResolution);
        if (f.renderScale > 0)
            parts << QString("render scale %1 %").arg(f.renderScale);
        out.push_back({"Game files", parts.join(kDot) + " (Game config tab)"});
    }
    if (f.renderScale > 0 && f.renderScale != 100) {
        const Size base = f.fileResolution.valid() ? f.fileResolution : f.window;
        if (base.valid()) {
            const Size drawn{qRound(base.w * f.renderScale / 100.0), qRound(base.h * f.renderScale / 100.0)};
            out.push_back({"Render scale",
                           QString("%1 %: the 3D scene is drawn at %2, %3 to %4")
                               .arg(f.renderScale)
                               .arg(sizeText(drawn), f.renderScale > 100 ? "scaled down" : "scaled up",
                                    sizeText(base))});
        }
    }
    out.push_back({"Window", f.windowFound ? sizeText(f.window) + kDot + windowMode(f) : QString("not found")});
    if (f.monitor.valid()) {
        QString monitor = sizeText(f.monitor);
        if (f.monitorIndex > 0)
            monitor = QString::number(f.monitorIndex) + kDot + monitor;
        if (f.monitorHz > 0)
            monitor += kDot + QString::number(f.monitorHz) + " Hz";
        out.push_back({"Monitor", monitor});
    }

    if (stats.running) {
        QString fps;
        if (stats.fps.valid)
            fps = QString("%1%2 1% low %3").arg(qRound(stats.fps.fps)).arg(kDot).arg(qRound(stats.fps.low1Fps));
        else if (stats.fpsSource == Stats::FpsSource::NoPermission)
            fps = "needs administrator rights";
        else if (stats.fpsSource == Stats::FpsSource::Running)
            fps = "no frames seen";
        else
            fps = "not available";
        out.push_back({"FPS", fps});

        QString load = "CPU " + (stats.cpu ? pct(*stats.cpu) : QString("-"));
        load += kDot + "GPU " + (stats.gpu ? pct(*stats.gpu) : QString("n/a"));
        if (!stats.gpuName.isEmpty())
            load += " on " + Stats::shortAdapterName(stats.gpuName);
        if (stats.magpieGpu)
            load += " + Magpie " + pct(*stats.magpieGpu);
        out.push_back({"Load", load});

        QStringList memory;
        if (stats.ram)
            memory << "RAM " + Stats::formatBytes(*stats.ram);
        if (stats.vram)
            memory << "VRAM " + Stats::formatBytes(*stats.vram);
        if (!memory.isEmpty())
            out.push_back({"Memory", memory.join(kDot)});
    }
    const Stats::Power& p = stats.power;
    if (p.known) {
        QString power = p.onBattery ? "On battery" : "Plugged in";
        if (p.batteryPercent >= 0)
            power += kDot + QString::number(p.batteryPercent) + " %";
        if (!p.plan.isEmpty())
            power += kDot + p.plan;
        out.push_back({"Power", power, p.onBattery || p.batterySaver || p.efficiencyMode});
    }

    for (const QString& problem : r.problems)
        out.push_back({{}, problem, true});
    for (const Stats::Warning& w : stats.warnings)
        out.push_back({{}, w.text + ": " + w.detail, true});
    return out;
}

} // namespace Picture
