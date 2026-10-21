#include "PictureCompare.h"

#include "WindowManagerController.h"
#include "core/Logger.h"
#include "magpie/MagpieProcess.h"
#include "stats/SessionReport.h"
#include "ui/SessionRecorder.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QPainter>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr int kFocusWaitMs = 2000;   // Magpie draws live again within ~50 ms; the game may need longer
constexpr int kHideWaitMs = 250;     // for DWM to show the screen without this app
constexpr int kRegionW = 320;        // the zoomed region, in the scaled picture's pixels
constexpr int kRegionH = 180;
constexpr int kZoom = 2;

const QString kTimes = QString::fromUtf8(" × ");

void logInfo(const QString &text) { Logger::instance().info(text.toStdString(), "Report"); }

#ifdef Q_OS_WIN
// A 32-bit top-down DIB the size of w x h, drawn by `draw` into a memory DC, as a QImage.
template <typename Draw>
QImage grab(int w, int h, Draw draw)
{
    if (w <= 0 || h <= 0)
        return {};
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;   // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    QImage out;
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(mem, bmp);
        if (draw(mem, screen))
            out = QImage(static_cast<const uchar *>(bits), w, h, w * 4, QImage::Format_RGB32).copy();
        SelectObject(mem, old);
    }
    if (bmp)
        DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return out;
}
#endif

double meanLuma(const std::vector<unsigned char> &g)
{
    if (g.empty())
        return 0;
    double total = 0;
    for (unsigned char v : g)
        total += v;
    return total / double(g.size());
}

} // namespace

namespace PictureCompareImage {

std::vector<unsigned char> gray(const QImage &image)
{
    const QImage g = image.convertToFormat(QImage::Format_Grayscale8);
    std::vector<unsigned char> out(size_t(g.width()) * size_t(g.height()));
    for (int y = 0; y < g.height(); ++y)
        std::copy_n(g.constScanLine(y), g.width(), out.begin() + qsizetype(y) * g.width());
    return out;
}

QImage sideBySide(const QImage &source, const QImage &bilinear, const QImage &scaled, const QString &scalerName,
                  double sharpSource, double sharpBilinear, double sharpScaled)
{
    // The region with the most edge detail in the stretched picture: a flat sky says nothing
    const int w = std::min(kRegionW, scaled.width());
    const int h = std::min(kRegionH, scaled.height());
    QRect best(0, 0, w, h);
    double bestScore = -1;
    const QImage grayScaled = bilinear.convertToFormat(QImage::Format_Grayscale8);
    for (int y = 0; y + h <= grayScaled.height(); y += h / 4) {
        for (int x = 0; x + w <= grayScaled.width(); x += w / 4) {
            const double score = Report::sharpness(gray(grayScaled.copy(x, y, w, h)), w, h);
            if (score > bestScore) {
                bestScore = score;
                best = QRect(x, y, w, h);
            }
        }
    }
    const double fx = double(source.width()) / scaled.width();
    const double fy = double(source.height()) / scaled.height();
    const QRect inSource(int(best.x() * fx), int(best.y() * fy), std::max(1, int(w * fx)), std::max(1, int(h * fy)));

    const QSize panel(w * kZoom, h * kZoom);
    const QImage panels[3] = {
        source.copy(inSource).scaled(panel, Qt::IgnoreAspectRatio, Qt::FastTransformation),
        bilinear.copy(best).scaled(panel, Qt::IgnoreAspectRatio, Qt::FastTransformation),
        scaled.copy(best).scaled(panel, Qt::IgnoreAspectRatio, Qt::FastTransformation),
    };
    const QString titles[3] = {
        QString("Game frame %1%2%3 (its own pixels)").arg(source.width()).arg(kTimes).arg(source.height()),
        QString("Bilinear stretch to %1%2%3").arg(scaled.width()).arg(kTimes).arg(scaled.height()),
        scalerName,
    };
    const double scores[3] = {sharpSource, sharpBilinear, sharpScaled};

    constexpr int gap = 8, header = 30, footer = 26;
    QImage out(3 * panel.width() + 2 * gap, header + panel.height() + footer, QImage::Format_RGB32);
    out.fill(QColor("#17181a"));
    QPainter p(&out);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont f = p.font();
    f.setPixelSize(15);
    p.setFont(f);
    for (int i = 0; i < 3; ++i) {
        const int x = i * (panel.width() + gap);
        p.setPen(QColor("#e6e6e6"));
        p.drawText(QRect(x + 4, 0, panel.width(), header), Qt::AlignLeft | Qt::AlignVCenter, titles[i]);
        p.drawImage(x, header, panels[i]);
        p.setPen(QColor("#a8b0bc"));
        QString score = QString("sharpness %1").arg(scores[i], 0, 'f', 1);
        if (i == 2)
            score += QString("  (%1%2 % against the bilinear stretch)")
                         .arg(Report::gainPercent(sharpScaled, sharpBilinear) >= 0 ? "+" : "")
                         .arg(Report::gainPercent(sharpScaled, sharpBilinear), 0, 'f', 1);
        p.drawText(QRect(x + 4, header + panel.height(), panel.width(), footer), Qt::AlignLeft | Qt::AlignVCenter,
                   score);
    }
    return out;
}

} // namespace PictureCompareImage

PictureCompare::PictureCompare(WindowManagerController *controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
}

void PictureCompare::start(const QString &game, const QString &scalerName)
{
    if (m_busy)
        return;
#ifdef Q_OS_WIN
    const Magpie::Scaling s = Magpie::scaling();
    const Magpie::WindowFacts gameWindow = Magpie::windowFacts(m_controller->activeWindow());
    if (!s.active || gameWindow.pid == 0 || Magpie::windowFacts(s.source).pid != gameWindow.pid) {
        emit finished(false, "Picture comparison: Magpie is not scaling the game. Start the game with Magpie "
                             "(Magpie tab) and let it scale, then compare.", {});
        return;
    }
    m_busy = true;
    m_game = game;
    m_scaler = scalerName;
    m_controller->focusTarget();   // live frames: Magpie pauses while another window is in front
    QTimer::singleShot(kFocusWaitMs, this, [this]() {
        emit captureStarting();
        QTimer::singleShot(kHideWaitMs, this, &PictureCompare::capture);
    });
#else
    Q_UNUSED(game)
    Q_UNUSED(scalerName)
    emit finished(false, "Picture comparison works on Windows only (it compares against Magpie).", {});
#endif
}

void PictureCompare::fail(const QString &text)
{
    m_busy = false;
    emit captureDone();
    emit finished(false, "Picture comparison: " + text, {});
}

void PictureCompare::capture()
{
#ifdef Q_OS_WIN
    const Magpie::Scaling s = Magpie::scaling();
    if (!s.active) {
        fail("Magpie stopped scaling before the shots were taken.");
        return;
    }
    // Magpie's picture first, then the game's frame: both within a few milliseconds
    const QImage scaled = grab(s.destWidth, s.destHeight, [&](HDC mem, HDC screen) {
        return BitBlt(mem, 0, 0, s.destWidth, s.destHeight, screen, s.destLeft, s.destTop, SRCCOPY | CAPTUREBLT) != 0;
    });
    const HWND src = reinterpret_cast<HWND>(s.source);
    RECT wr{};
    GetWindowRect(src, &wr);
    const QImage window = grab(wr.right - wr.left, wr.bottom - wr.top, [&](HDC mem, HDC) {
        return PrintWindow(src, mem, PW_RENDERFULLCONTENT) != 0;
    });
    emit captureDone();

    if (scaled.isNull() || window.isNull()) {
        fail("the screen or the game window could not be copied.");
        return;
    }
    const QImage source = window.copy(s.srcLeft - wr.left, s.srcTop - wr.top, s.srcWidth, s.srcHeight);
    const auto graySource = PictureCompareImage::gray(source);
    if (meanLuma(graySource) < 2) {
        fail("the game's own frame came back black (Windows could not read this game's window), so there is "
             "nothing to compare with.");
        return;
    }
    const QImage bilinear = source.scaled(scaled.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    Report::Comparison c;
    c.time = QDateTime::currentDateTime();
    c.game = m_game;
    c.setup = Report::setupName(m_scaler, Report::SetupState::Scaling, source.width(), source.height(), scaled.width(),
                                scaled.height(), 0, 0);
    c.sourceW = source.width();
    c.sourceH = source.height();
    c.destW = scaled.width();
    c.destH = scaled.height();
    c.sharpSource = Report::sharpness(graySource, source.width(), source.height());
    c.sharpBilinear = Report::sharpness(PictureCompareImage::gray(bilinear), bilinear.width(), bilinear.height());
    c.sharpScaled = Report::sharpness(PictureCompareImage::gray(scaled), scaled.width(), scaled.height());
    c.gainPercent = Report::gainPercent(c.sharpScaled, c.sharpBilinear);

    const QString &scalerName = m_scaler;
    const QString dir = SessionRecorder::gameDir(m_game);
    QDir().mkpath(dir);
    const QString stamp = "compare-" + c.time.toString("yyyyMMdd-HHmmss");
    c.image = stamp + ".png";
    PictureCompareImage::sideBySide(source, bilinear, scaled, scalerName, c.sharpSource, c.sharpBilinear,
                                    c.sharpScaled)
        .save(dir + "/" + c.image);
    source.save(dir + "/" + stamp + "-game.png");
    bilinear.save(dir + "/" + stamp + "-bilinear.png");
    scaled.save(dir + "/" + stamp + "-scaled.png");
    const QString json = dir + "/" + stamp + ".json";
    QFile f(json);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || f.write(QJsonDocument(Report::toJson(c)).toJson()) <= 0) {
        fail("could not write " + QDir::toNativeSeparators(json));
        return;
    }
    f.close();
    m_busy = false;

    const QString verdict = c.gainPercent >= 1 ? QString("%1 is %2 % sharper than a plain stretch").arg(scalerName).arg(c.gainPercent, 0, 'f', 1)
                            : c.gainPercent <= -1 ? QString("%1 is %2 % softer than a plain stretch").arg(scalerName).arg(-c.gainPercent, 0, 'f', 1)
                                                  : QString("%1 and a plain stretch are about as sharp").arg(scalerName);
    const QString text = QString("Picture comparison (%1): %2 (sharpness %3 against %4; the game's own frame %5).")
                             .arg(c.setup, verdict)
                             .arg(c.sharpScaled, 0, 'f', 1)
                             .arg(c.sharpBilinear, 0, 'f', 1)
                             .arg(c.sharpSource, 0, 'f', 1);
    logInfo(text);
    emit finished(true, text, json);
#endif
}
