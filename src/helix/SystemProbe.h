#pragma once

#include <QString>

#include "helix/HelixSystem.h"

namespace Helix {

/**
 * Asks Windows what this PC offers: every graphics adapter (DXGI), whether a Direct3D 12 device
 * can be created on it and what shader model, 16-bit math and SIMD width it has, plus free disk
 * space where HelixSR is stored and the tools its setup needs. Takes a moment (it creates devices).
 */
SystemInfo probeSystem(const QString& storageDir);

} // namespace Helix
