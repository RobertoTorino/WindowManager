#pragma once

#include <QString>

#include "build_stamp.h"   // generated at build time (cmake/BuildInfo.cmake)

/** Version, build number and build date, for the window title and the About box. */
namespace BuildInfo {

// "WindowManager 0.1.3 - build 47 - 2026-10-19"
inline QString windowTitle()
{
    return QString("WindowManager %1 - build %2 - %3").arg(APP_VERSION_STR, APP_BUILD_NUMBER, APP_BUILD_DATE);
}

// "0.1.3, build 47 (47296d4), 2026-10-19"
inline QString description()
{
    return QString("%1, build %2 (%3), %4").arg(APP_VERSION_STR, APP_BUILD_NUMBER, APP_BUILD_HASH, APP_BUILD_DATE);
}

} // namespace BuildInfo
