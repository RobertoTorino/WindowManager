#include "PlatformBackend.h"

#include <QDesktopServices>
#include <QImage>
#include <QUrl>

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

QImage PlatformBackend::captureWindow(quintptr window) const
{
    Fit::Rect rect;
    return windowRect(window, rect) ? captureScreen(rect) : QImage();
}

bool PlatformBackend::openUrl(const QString& url)
{
    return QDesktopServices::openUrl(QUrl(url));
}
