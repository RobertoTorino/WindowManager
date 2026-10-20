#pragma once

#include "StatsModel.h"

#include <vector>

/**
 * Reads the game's resource use for the stats bar: CPU time, memory, GPU counters, present
 * events (frame rate) and the power state.
 *
 * One implementation per platform (src/platform/win32/Win32Stats.cpp, and a stand-in under
 * src/platform/unsupported/ that reports supported() == false). sample() runs on a worker
 * thread, one call at a time; the frame counter runs on a thread of its own once started.
 */
class StatsBackend
{
public:
    virtual ~StatsBackend() = default;

    virtual bool supported() const = 0;

    // Graphics adapters (read once, then cached).
    virtual std::vector<Stats::Adapter> adapters() = 0;

    // Everything for one process; pid 0 only reads the power state. The first call starts the
    // frame counter (an event trace), which needs administrator rights.
    virtual Stats::RawSample sample(qint64 pid) = 0;

    // Stops the frame counter. Called when the app closes.
    virtual void shutdown() = 0;

    // The graphics setting Windows keeps for a program (the full exe path), and the adapter Windows
    // picks for each choice (empty: not known). A change applies when the program starts next.
    virtual bool gpuPreferenceSupported() const = 0;
    virtual Stats::GpuPreference gpuPreference(const QString& exePath) = 0;
    virtual bool setGpuPreference(const QString& exePath, Stats::GpuPreference preference, QString* error) = 0;
    virtual QString adapterFor(Stats::GpuPreference preference) = 0;

    // Power plans (also the ones Control Panel hides under "Show additional plans") and the power
    // mode, which Windows only offers with the Balanced plan.
    virtual Stats::Power power() = 0;
    virtual std::vector<Stats::PowerPlan> powerPlans() = 0;
    virtual bool setPowerPlan(const QString& id, QString* error) = 0;
    virtual bool setPowerMode(Stats::PowerMode mode, QString* error) = 0;
};

// The backend for this platform.
StatsBackend& statsBackend();
