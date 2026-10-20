#include "HelixIni.h"

#include "core/IniText.h"

#include <algorithm>

namespace Helix {

namespace {

QList<IniText::Entry> entriesOf(const IniSettings& s)
{
    return {
        {"Sharpening", "Sharpener", s.sharpener},
        {"Sharpening", "Sharpness", QString::number(std::clamp(s.sharpness, 0.0, 1.0), 'g', 3)},
        {"Compatibility", "WaveSize", s.waveSize},
    };
}

} // namespace

IniSettings parseIni(const QString& text)
{
    IniSettings s;
    QString section;
    QString key;
    QString value;
    for (const QString& line : QString(text).replace("\r\n", "\n").split('\n')) {
        const QString header = IniText::sectionOf(line);
        if (!header.isEmpty()) {
            section = header;
            continue;
        }
        if (!IniText::entryOf(line, key, value))
            continue;

        if (section == "sharpening" && key == "sharpener") {
            if (kSharpeners.contains(value.toLower()))
                s.sharpener = value.toLower();
        } else if (section == "sharpening" && key == "sharpness") {
            bool ok = false;
            const double v = value.toDouble(&ok);
            if (ok)
                s.sharpness = std::clamp(v, 0.0, 1.0);
        } else if (section == "compatibility" && key == "wavesize") {
            if (kWaveSizes.contains(value.toLower()))
                s.waveSize = value.toLower();
        }
    }
    return s;
}

QString applyIni(const QString& text, const IniSettings& settings)
{
    return IniText::apply(text, entriesOf(settings), " = ");
}

} // namespace Helix
