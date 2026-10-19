#pragma once

#include <QString>
#include <QStringList>

/**
 * Pure logic: reading and editing HelixSR's helixsr.ini (it sits next to HelixSR's DLL in the game).
 * Editing keeps everything else in the file (comments, order, line endings).
 */
namespace Helix {

struct IniSettings
{
    QString sharpener{"off"};   // off | nvidia | rcas
    double  sharpness{0.1};     // 0.0 to 1.0, used when a sharpener is on
    QString waveSize{"auto"};   // auto | 64 | 32

    bool operator==(const IniSettings&) const = default;
};

inline const QStringList kSharpeners = {"off", "nvidia", "rcas"};
inline const QStringList kWaveSizes = {"auto", "64", "32"};

// Missing keys and values HelixSR would not accept become the defaults above (sharpness is clamped).
IniSettings parseIni(const QString& text);

// `text` with the three settings changed in place; missing keys or sections are added. Empty text
// gives a new file. Line endings of the existing text are kept (a new file uses Windows endings).
QString applyIni(const QString& text, const IniSettings& settings);

} // namespace Helix
