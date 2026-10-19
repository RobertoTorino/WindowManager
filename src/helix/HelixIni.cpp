#include "HelixIni.h"

#include <QRegularExpression>

#include <algorithm>

namespace Helix {

namespace {

struct Entry
{
    const char* section;
    const char* key;
    QString     value;
};

QList<Entry> entriesOf(const IniSettings& s)
{
    return {
        {"Sharpening", "Sharpener", s.sharpener},
        {"Sharpening", "Sharpness", QString::number(std::clamp(s.sharpness, 0.0, 1.0), 'g', 3)},
        {"Compatibility", "WaveSize", s.waveSize},
    };
}

// "[Section]" -> "section"; empty if the line is not a section header
QString sectionOf(const QString& line)
{
    const QString t = line.trimmed();
    if (t.size() >= 2 && t.startsWith('[') && t.endsWith(']'))
        return t.mid(1, t.size() - 2).trimmed().toLower();
    return {};
}

// "Key = value" -> true with key (lower case) and value; comments and other lines are not entries
bool entryOf(const QString& line, QString& key, QString& value)
{
    const QString t = line.trimmed();
    if (t.isEmpty() || t.startsWith(';') || t.startsWith('#') || t.startsWith('['))
        return false;
    const int eq = t.indexOf('=');
    if (eq <= 0)
        return false;
    key = t.left(eq).trimmed().toLower();
    value = t.mid(eq + 1).trimmed();
    return true;
}

} // namespace

IniSettings parseIni(const QString& text)
{
    IniSettings s;
    QString section;
    QString key;
    QString value;
    for (const QString& line : QString(text).replace("\r\n", "\n").split('\n')) {
        const QString header = sectionOf(line);
        if (!header.isEmpty()) {
            section = header;
            continue;
        }
        if (!entryOf(line, key, value))
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
    const QString eol = text.isEmpty() || text.contains("\r\n") ? "\r\n" : "\n";
    QStringList lines = QString(text).replace("\r\n", "\n").split('\n');
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();

    for (const Entry& e : entriesOf(settings)) {
        const QString wantSection = QString(e.section).toLower();
        const QString wantKey = QString(e.key).toLower();
        const QString newLine = QString("%1 = %2").arg(e.key, e.value);

        // find the section's lines and the key inside them
        int sectionStart = -1;
        int sectionEnd = lines.size();   // one past the last line of the section
        int keyLine = -1;
        QString current;
        for (int i = 0; i < lines.size(); ++i) {
            const QString header = sectionOf(lines[i]);
            if (!header.isEmpty()) {
                if (sectionStart >= 0 && sectionEnd == lines.size())
                    sectionEnd = i;
                current = header;
                if (current == wantSection && sectionStart < 0)
                    sectionStart = i;
                continue;
            }
            QString key;
            QString value;
            if (current == wantSection && sectionEnd == lines.size() && entryOf(lines[i], key, value) && key == wantKey
                && keyLine < 0)
                keyLine = i;
        }

        if (keyLine >= 0) {
            const QString& old = lines[keyLine];
            const QString indent = old.left(old.size() - QString(old).remove(QRegularExpression("^\\s+")).size());
            lines[keyLine] = indent + newLine;
        } else if (sectionStart >= 0) {
            // after the section's last non-empty line, so blank separators stay below it
            int at = sectionEnd;
            while (at > sectionStart + 1 && lines[at - 1].trimmed().isEmpty())
                --at;
            lines.insert(at, newLine);
        } else {
            if (!lines.isEmpty())
                lines << QString();
            lines << QString("[%1]").arg(e.section) << newLine;
        }
    }
    return lines.join(eol) + eol;
}

} // namespace Helix
