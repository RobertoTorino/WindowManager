#pragma once

#include <QImage>
#include <QObject>
#include <QString>

class WindowManagerController;

/**
 * Picture comparison (Reports tab): is Magpie's upscaling sharper than a plain stretch?
 *
 * Gives the game the focus (Magpie only draws live frames while it is in front), and 2 s later takes, at the
 * same moment, the game's own frame (PrintWindow of its window, cut to the rectangle Magpie captures) and what
 * Magpie shows (a copy of the screen where it draws). The game's frame is then stretched to the same size with
 * a plain bilinear filter. All three get a sharpness score (Report::sharpness), and a side-by-side zoom of the
 * most detailed part is drawn. Saved in reports/<game>/compare-<time>.* (PNG + JSON). Windows only.
 *
 * Best on a still screen (pause, character select): the two shots are a frame or two apart.
 */
class PictureCompare : public QObject
{
    Q_OBJECT

public:
    explicit PictureCompare(WindowManagerController *controller, QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    void start(const QString &game, const QString &scalerName);   // scalerName: "Magpie FSR"

signals:
    void captureStarting();   // hide whatever covers the game's monitor
    void captureDone();
    void finished(bool ok, const QString &text, const QString &jsonPath);

private:
    void capture();
    void fail(const QString &text);

    WindowManagerController *m_controller;
    bool    m_busy{false};
    QString m_game;
    QString m_scaler;
};

namespace PictureCompareImage {
// The side-by-side zoom: three panels of the same region (chosen where the stretched picture has the most edge
// detail), each enlarged 2x with no smoothing so the pixels show.
QImage sideBySide(const QImage &source, const QImage &bilinear, const QImage &scaled, const QString &scalerName,
                  double sharpSource, double sharpBilinear, double sharpScaled);
std::vector<unsigned char> gray(const QImage &image);
}
