#include "IniText.h"

#include <QRegularExpression>
#include <QStringList>

namespace IniText {

QString sectionOf(const QString& line)
{
    const QString t = line.trimmed();
    if (t.size() >= 2 && t.startsWith('[') && t.endsWith(']'))
        return t.mid(1, t.size() - 2).trimmed().toLower();
    return {};
}

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

std::optional<QString> value(const QString& text, const QString& section, const QString& key)
{
    const QString wantSection = section.toLower();
    const QString wantKey = key.toLower();
    QString current;
    bool seen = false;   // the first matching section has been passed
    for (const QString& line : QString(text).replace("\r\n", "\n").split('\n')) {
        const QString header = sectionOf(line);
        if (!header.isEmpty()) {
            if (current == wantSection)
                seen = true;
            current = header;
            continue;
        }
        QString k;
        QString v;
        if (!seen && current == wantSection && entryOf(line, k, v) && k == wantKey)
            return v;
    }
    return std::nullopt;
}

QString apply(const QString& text, const QList<Entry>& entries, const QString& assign)
{
    const QString eol = text.isEmpty() || text.contains("\r\n") ? "\r\n" : "\n";
    QStringList lines = QString(text).replace("\r\n", "\n").split('\n');
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    bool changed = false;

    for (const Entry& e : entries) {
        const QString wantSection = e.section.toLower();
        const QString wantKey = e.key.toLower();

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

        if (!e.value) {
            if (keyLine >= 0) {
                lines.removeAt(keyLine);
                changed = true;
            }
            continue;
        }

        const QString newLine = e.key + assign + *e.value;
        if (keyLine >= 0) {
            const QString& old = lines[keyLine];
            const QString indent = old.left(old.size() - QString(old).remove(QRegularExpression("^\\s+")).size());
            QString oldKey;
            QString oldValue;
            entryOf(old, oldKey, oldValue);
            if (oldValue != *e.value) {   // the same value keeps the line as it was written
                lines[keyLine] = indent + newLine;
                changed = true;
            }
        } else if (sectionStart >= 0) {
            // after the section's last non-empty line, so blank separators stay below it
            int at = sectionEnd;
            while (at > sectionStart + 1 && lines[at - 1].trimmed().isEmpty())
                --at;
            lines.insert(at, newLine);
            changed = true;
        } else {
            if (!lines.isEmpty())
                lines << QString();
            lines << QString("[%1]").arg(e.section) << newLine;
            changed = true;
        }
    }
    if (!changed && !text.isEmpty())
        return text;
    return lines.join(eol) + eol;
}

} // namespace IniText
