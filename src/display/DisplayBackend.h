#pragma once

#include "DisplayModel.h"

#include <QString>

#include <vector>

/**
 * The system's display settings (what Windows shows under Settings > System > Display).
 *
 * Kept apart from PlatformBackend: those are the game's windows, this is the PC's monitors.
 * One implementation per platform (src/platform/win32/Win32Display.cpp, and a stand-in
 * under src/platform/unsupported/ that reports supported() == false). Monitors are named
 * by DisplayModel::Monitor::id. Every setter returns false and fills `error` on failure.
 */
class DisplayBackend
{
public:
    virtual ~DisplayBackend() = default;

    virtual bool supported() const = 0;

    // Active monitors with their current settings. Reading the brightness of an external
    // monitor asks the monitor itself (DDC/CI), which takes a moment.
    virtual std::vector<DisplayModel::Monitor> monitors() const = 0;

    // temporary: for a game; not stored, and resetMode() goes back to the stored mode.
    virtual bool setMode(const QString& id, const DisplayModel::Mode& mode, DisplayModel::Orientation orientation,
                         bool temporary, QString* error) = 0;
    virtual bool resetMode(const QString& id, QString* error) = 0;
    virtual bool setHdr(const QString& id, bool on, QString* error) = 0;
    virtual bool setScale(const QString& id, int percent, QString* error) = 0;
    virtual bool setBrightness(const QString& id, int percent, QString* error) = 0;
    virtual bool setColorProfile(const QString& id, const QString& profile, QString* error) = 0;

    virtual DisplayModel::Topology topology() const = 0;
    virtual int  connectedDisplays() const = 0;   // also the ones switched off by the topology
    virtual bool setTopology(DisplayModel::Topology topology, QString* error) = 0;

    // Settings page for what cannot be set directly (night light). Empty: none.
    virtual QString nightLightUrl() const = 0;
};

// The backend for this platform.
DisplayBackend& displayBackend();
