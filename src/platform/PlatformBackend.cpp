#include "PlatformBackend.h"

namespace {
PlatformBackend* g_override = nullptr;
}

PlatformBackend& platform()
{
    return g_override ? *g_override : nativePlatformBackend();
}

void setPlatformBackendForTesting(PlatformBackend* backend)
{
    g_override = backend;
}
