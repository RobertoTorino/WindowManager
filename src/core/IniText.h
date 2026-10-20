#pragma once

#include <QList>
#include <QString>

#include <optional>

/**
 * Pure logic: reading and editing ini text in place (HelixSR's helixsr.ini, Unreal Engine config files).
 * Editing keeps everything else in the file: comments, order, unknown keys and the line endings.
 * Section and key lookup is case-insensitive.
 */
namespace IniText {

struct Entry
{
    QString section;                // without the brackets
    QString key;
    std::optional<QString> value;   // nullopt removes the key
};

// "[Section]" -> "section" (lower case); empty if the line is not a section header
QString sectionOf(const QString& line);

// "Key = value" -> true with key (lower case) and value; comments and other lines are not entries
bool entryOf(const QString& line, QString& key, QString& value);

// The value of the first `key` in the first `section`; nullopt if there is none.
std::optional<QString> value(const QString& text, const QString& section, const QString& key);

// `text` with the entries set: an existing key is changed in place (its indentation kept), a missing one is
// added at the end of its section, a missing section at the end of the file. `assign` joins key and value
// ("Key = value" or Unreal's "Key=value"). When nothing changes, `text` comes back as it was; otherwise
// trailing blank lines are dropped and the text ends with one line ending (Windows endings for a new file).
QString apply(const QString& text, const QList<Entry>& entries, const QString& assign = " = ");

} // namespace IniText
