#include "ReportCharts.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace {

// The data-viz reference palette, dark steps (the app is dark): fixed order, never cycled.
const QColor kSeries[] = {QColor("#3987e5"), QColor("#d95926"), QColor("#199e70"), QColor("#c98500"),
                          QColor("#d55181"), QColor("#008300"), QColor("#9085e9"), QColor("#e66767")};
const QColor kNeutral("#6b6f75");   // paused / not scaling / past the eighth setup

constexpr int kLegendH = 40;   // two rows: the setups, then what each line style means
constexpr int kAxisW = 44;     // value labels on the left
constexpr int kTimeH = 18;     // time labels under the lower plot

const QString kDot = QString::fromUtf8(" · ");

QString clock(int seconds)
{
    return QString("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QChar('0'));
}

QString value(double v, const QString &unit = {})
{
    return v < 0 ? QString("-") : QString::number(qRound(v)) + unit;
}

// A round top for an axis: 60 -> 60, 61 -> 80, 145 -> 150
double niceMax(double v)
{
    if (v <= 60)
        return 60;
    const double step = v <= 200 ? 20 : v <= 500 ? 50 : 100;
    return std::ceil(v / step) * step;
}

bool isNeutral(const QString &setup)
{
    return setup.endsWith(" paused") || setup.endsWith(" starting") || setup.contains(" not scaling")
           || setup.isEmpty();
}

} // namespace

namespace ReportColors {

QColor forSetup(const QString &setup, const QStringList &order)
{
    if (isNeutral(setup))
        return kNeutral;
    int slot = 0;
    for (const QString &s : order) {   // the place among the coloured setups
        if (s == setup)
            return slot < 8 ? kSeries[slot] : kNeutral;
        if (!isNeutral(s))
            ++slot;
    }
    return kNeutral;
}

bool isState(const QString &setup) { return isNeutral(setup); }

QColor text() { return QColor("#e6e6e6"); }
QColor muted() { return QColor("#8b8f95"); }
QColor grid() { return QColor("#2c2e32"); }

} // namespace ReportColors

// ---------------------------------------------------------------------------
// SessionChart

SessionChart::SessionChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SessionChart::setSession(const std::vector<Report::Sample> &samples, const QStringList &setupOrder,
                              const QString &title)
{
    m_samples = samples;
    m_order = setupOrder;
    m_title = title;
    double top = 0;
    for (const auto &s : m_samples)
        top = std::max({top, s.fps, s.low1});
    m_fpsMax = niceMax(top);
    update();
}

QRect SessionChart::plotArea(int index) const
{
    const int top = kLegendH + 6;
    const int usable = height() - top - kTimeH - 10;
    const int fpsH = usable * 55 / 100;
    const QRect fps(kAxisW, top, width() - kAxisW - 8, fpsH);
    const QRect gpu(kAxisW, fps.bottom() + 16, fps.width(), usable - fpsH - 10);
    return index == 0 ? fps : gpu;
}

int SessionChart::sampleAt(int x) const
{
    if (m_samples.empty())
        return -1;
    const QRect a = plotArea(0);
    const int last = std::max(1, m_samples.back().t);
    const double t = double(x - a.left()) / a.width() * last;
    const auto it = std::lower_bound(m_samples.begin(), m_samples.end(), t,
                                     [](const Report::Sample &s, double v) { return s.t < v; });
    if (it == m_samples.end())
        return int(m_samples.size()) - 1;
    return int(it - m_samples.begin());
}

void SessionChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#17181a"));
    if (m_samples.size() < 2) {
        p.setPen(ReportColors::muted());
        p.drawText(rect(), Qt::AlignCenter, "Pick a session on the left");
        return;
    }

    // Legend, row 1: the setups in this session (colour = setup), the grey states as one entry
    QStringList present;
    bool neutral = false;
    for (const auto &s : m_samples) {
        if (isNeutral(s.setup))
            neutral = true;
        else if (!present.contains(s.setup))
            present << s.setup;
    }
    if (neutral)
        present << QString();
    int x = kAxisW;
    for (const QString &setup : present) {
        const QString label = setup.isEmpty() ? QString("starting / paused / not scaling") : setup;
        const int w = std::min(p.fontMetrics().horizontalAdvance(label) + 2, width() - x - 24);   // +2: rounding
        if (w < 40)
            break;
        p.fillRect(QRect(x, 6, 12, 8), ReportColors::forSetup(setup, m_order));
        p.setPen(setup.isEmpty() ? ReportColors::muted() : ReportColors::text());
        p.drawText(QRect(x + 16, 0, w, 20), Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(label, Qt::ElideRight, w));
        x += 16 + w + 14;
    }
    // row 2: the line styles
    p.setPen(ReportColors::muted());
    p.drawText(QRect(kAxisW, 20, width() - kAxisW - 8, 18), Qt::AlignLeft | Qt::AlignVCenter,
               QString::fromUtf8("solid: FPS and the game's GPU · dotted: 1 % low · dashed: Magpie's GPU"));

    const int last = std::max(1, m_samples.back().t);
    const auto xOf = [&](const QRect &a, int t) { return a.left() + double(t) / last * a.width(); };

    // Plots: recessive grid, one axis each
    struct Plot { QRect area; double max; QString name; QString unit; };
    const Plot plots[2] = {{plotArea(0), m_fpsMax, "FPS", ""}, {plotArea(1), 100, "GPU", " %"}};
    for (const Plot &plot : plots) {
        const QRect &a = plot.area;
        for (int i = 0; i <= 4; ++i) {
            const double v = plot.max * i / 4;
            const int y = a.bottom() - int(a.height() * i / 4.0);
            p.setPen(QPen(ReportColors::grid(), 1));
            p.drawLine(a.left(), y, a.right(), y);
            p.setPen(ReportColors::muted());
            p.drawText(QRect(0, y - 8, kAxisW - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(qRound(v)) + plot.unit);
        }
        p.setPen(ReportColors::muted());
        p.drawText(a.adjusted(6, 2, 0, 0), Qt::AlignLeft | Qt::AlignTop, plot.name);
    }

    // A series as line pieces, broken where there is no value, each piece in its setup's colour
    const auto drawSeries = [&](const Plot &plot, auto valueOf, Qt::PenStyle style, qreal width, bool ownColour) {
        const QRect &a = plot.area;
        QPainterPath path;
        QString setup;
        bool open = false;
        const auto flush = [&]() {
            if (!path.isEmpty()) {
                QPen pen(ownColour ? ReportColors::forSetup(setup, m_order) : ReportColors::muted(), width, style);
                pen.setCapStyle(Qt::RoundCap);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawPath(path);
            }
            path = QPainterPath();
            open = false;
        };
        for (const auto &s : m_samples) {
            const double v = valueOf(s);
            if (v < 0 || (ownColour && s.setup != setup)) {
                const QPointF lastPoint = open ? path.currentPosition() : QPointF();
                flush();
                setup = s.setup;
                if (v >= 0 && !lastPoint.isNull()) {   // keep the line joined across a setup change
                    path.moveTo(lastPoint);
                    open = true;
                }
            }
            if (v < 0)
                continue;
            const QPointF pt(xOf(a, s.t), a.bottom() - std::min(v, plot.max) / plot.max * a.height());
            if (open)
                path.lineTo(pt);
            else
                path.moveTo(pt);
            open = true;
        }
        flush();
    };
    if (std::none_of(m_samples.begin(), m_samples.end(), [](const Report::Sample &s) { return s.fps >= 0; })) {
        p.setPen(ReportColors::muted());
        p.drawText(plots[0].area, Qt::AlignCenter,
                   "No frames counted (DirectX games only; needs the frame counter, see Game info overlay)");
    }
    drawSeries(plots[0], [](const Report::Sample &s) { return s.low1; }, Qt::DotLine, 1.2, true);
    drawSeries(plots[0], [](const Report::Sample &s) { return s.fps; }, Qt::SolidLine, 2, true);
    drawSeries(plots[1], [](const Report::Sample &s) { return s.magpieGpu; }, Qt::DashLine, 1.5, false);
    drawSeries(plots[1], [](const Report::Sample &s) { return s.gpu; }, Qt::SolidLine, 2, true);

    // Time axis under the lower plot
    const QRect g = plots[1].area;
    p.setPen(ReportColors::muted());
    for (int i = 0; i <= 4; ++i) {
        const int t = last * i / 4;
        const int px = int(xOf(g, t));
        const Qt::Alignment align = i == 0 ? Qt::AlignLeft : i == 4 ? Qt::AlignRight : Qt::AlignHCenter;
        const QRect box = i == 0 ? QRect(px, g.bottom() + 2, 60, kTimeH)
                          : i == 4 ? QRect(px - 60, g.bottom() + 2, 60, kTimeH)
                                   : QRect(px - 30, g.bottom() + 2, 60, kTimeH);
        p.drawText(box, align | Qt::AlignTop, clock(t));   // the ends inside the plot
    }

    // Crosshair
    if (m_hoverX >= g.left() && m_hoverX <= g.right()) {
        p.setPen(QPen(ReportColors::text(), 1, Qt::DashLine));
        p.drawLine(m_hoverX, plots[0].area.top(), m_hoverX, g.bottom());
    }
}

void SessionChart::mouseMoveEvent(QMouseEvent *event)
{
    const int i = sampleAt(event->position().toPoint().x());
    const QRect a = plotArea(0);
    if (i < 0 || event->position().x() < a.left() || event->position().x() > a.right()) {
        leaveEvent(event);
        return;
    }
    m_hoverX = event->position().toPoint().x();
    const Report::Sample &s = m_samples[size_t(i)];
    QString tip = clock(s.t) + kDot + s.setup + "\nFPS " + value(s.fps);
    if (s.low1 >= 0)
        tip += " (1 % low " + value(s.low1) + ")";
    tip += "\nGPU " + value(s.gpu, " %");
    if (s.magpieGpu >= 0)
        tip += " + Magpie " + value(s.magpieGpu, " %");
    tip += kDot + "CPU " + value(s.cpu, " %");
    QToolTip::showText(event->globalPosition().toPoint(), tip, this);
    update();
}

void SessionChart::leaveEvent(QEvent *)
{
    m_hoverX = -1;
    QToolTip::hideText();
    update();
}

// ---------------------------------------------------------------------------
// SetupChart

namespace {
constexpr int kRowH = 30;
constexpr int kHeaderH = 22;
}

SetupChart::SetupChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SetupChart::setSetups(const std::vector<Report::Segment> &setups, const QStringList &setupOrder)
{
    m_setups = setups;
    m_order = setupOrder;
    setMinimumHeight(kHeaderH + kRowH * int(std::max<size_t>(1, m_setups.size())) + 8);
    update();
}

int SetupChart::rowAt(int y) const
{
    const int row = (y - kHeaderH) / kRowH;
    return y >= kHeaderH && row < int(m_setups.size()) ? row : -1;
}

void SetupChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#17181a"));
    if (m_setups.empty()) {
        p.setPen(ReportColors::muted());
        p.drawText(rect(), Qt::AlignCenter, "No sessions recorded for this game yet");
        return;
    }

    // Columns: setup name | Avg FPS | 1 % low | GPU (game + Magpie) | time
    const int nameW = std::min(320, width() * 34 / 100);
    const int timeW = 64;
    const int barsW = width() - nameW - timeW - 16;
    const int colW = barsW / 3;
    const int x0 = nameW + 8;
    double fpsMax = 60;
    for (const auto &s : m_setups)
        fpsMax = std::max({fpsMax, s.fpsAvg, s.low1Avg});
    fpsMax = niceMax(fpsMax);

    p.setPen(ReportColors::muted());
    const QStringList headers{"Avg FPS", "1 % low", "GPU % (+ Magpie)"};
    for (int c = 0; c < 3; ++c)
        p.drawText(QRect(x0 + c * colW, 0, colW - 8, kHeaderH), Qt::AlignLeft | Qt::AlignVCenter, headers[c]);
    p.drawText(QRect(width() - timeW - 8, 0, timeW, kHeaderH), Qt::AlignRight | Qt::AlignVCenter, "Recorded");

    const auto bar = [&](int col, int y, double v, double max, const QColor &colour, double extra = -1) {
        const int left = x0 + col * colW;
        const int maxW = colW - 52;   // room for the value
        p.setPen(QPen(ReportColors::grid(), 1));
        p.drawLine(left, y + 4, left, y + kRowH - 6);   // the baseline
        if (v < 0) {
            p.setPen(ReportColors::muted());
            p.drawText(QRect(left + 6, y, 40, kRowH), Qt::AlignLeft | Qt::AlignVCenter, "-");
            return;
        }
        const int w = std::max(2, int(std::min(v, max) / max * maxW));
        QPainterPath path;
        path.addRoundedRect(QRectF(left, y + 8, w, kRowH - 16), 4, 4);
        p.fillPath(path, colour);
        int end = left + w;
        if (extra > 0) {   // Magpie's share, after a 2 px gap, lighter
            const int w2 = std::max(2, int(std::min(extra, max) / max * maxW));
            QPainterPath path2;
            path2.addRoundedRect(QRectF(end + 2, y + 8, w2, kRowH - 16), 4, 4);
            QColor light = colour;
            light.setAlpha(110);
            p.fillPath(path2, light);
            end += 2 + w2;
        }
        p.setPen(ReportColors::text());
        const QString label = extra > 0 ? value(v) + " + " + value(extra) : value(v);
        p.drawText(QRect(end + 6, y, 80, kRowH), Qt::AlignLeft | Qt::AlignVCenter, label);
    };

    for (size_t i = 0; i < m_setups.size(); ++i) {
        const Report::Segment &s = m_setups[i];
        const int y = kHeaderH + int(i) * kRowH;
        const QColor colour = ReportColors::forSetup(s.setup, m_order);
        p.fillRect(QRect(4, y + 10, 10, 10), colour);
        p.setPen(ReportColors::text());
        p.drawText(QRect(20, y, nameW - 20, kRowH), Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(s.setup, Qt::ElideRight, nameW - 20));
        bar(0, y, s.fpsAvg, fpsMax, colour);
        bar(1, y, s.low1Avg, fpsMax, colour);
        bar(2, y, s.gpuAvg, 100, colour, s.magpieGpuAvg);
        p.setPen(ReportColors::muted());
        const QString time = s.seconds >= 120 ? QString("%1 min").arg(s.seconds / 60) : QString("%1 s").arg(s.seconds);
        p.drawText(QRect(width() - timeW - 8, y, timeW, kRowH), Qt::AlignRight | Qt::AlignVCenter, time);
    }
}

void SetupChart::mouseMoveEvent(QMouseEvent *event)
{
    const int row = rowAt(event->position().toPoint().y());
    if (row < 0) {
        QToolTip::hideText();
        return;
    }
    const Report::Segment &s = m_setups[size_t(row)];
    QString tip = s.setup + "\nAvg FPS " + value(s.fpsAvg) + ", 1 % low " + value(s.low1Avg) + "\nGPU " +
                  value(s.gpuAvg, " %") + " (peak " + value(s.gpuPeak, " %") + ")";
    if (s.magpieGpuAvg >= 0)
        tip += " + Magpie " + value(s.magpieGpuAvg, " %");
    tip += "\nCPU " + value(s.cpuAvg, " %") + ", RAM peak " + value(s.ramPeakMb, " MB") + ", VRAM peak " +
           value(s.vramPeakMb, " MB") + QString("\nRecorded %1").arg(clock(s.seconds));
    QToolTip::showText(event->globalPosition().toPoint(), tip, this);
}

// ---------------------------------------------------------------------------
// ImageView

ImageView::ImageView(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(200, 100);
}

void ImageView::setImage(const QPixmap &image, const QString &emptyText)
{
    m_image = image;
    m_empty = emptyText;
    update();
}

void ImageView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (m_image.isNull()) {
        p.setPen(ReportColors::muted());
        p.drawText(rect().adjusted(12, 0, -12, 0), Qt::AlignCenter | Qt::TextWordWrap, m_empty);
        return;
    }
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QSize size = m_image.size().scaled(this->size(), Qt::KeepAspectRatio);
    p.drawPixmap(QRect(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size), m_image);
}
