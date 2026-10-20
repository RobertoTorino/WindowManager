#include "DriverModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QXmlStreamReader>

#include <vector>

namespace Driver {

namespace {

struct LookupValue
{
    int     parent{0};
    QString name;
    int     value{0};
};

std::vector<LookupValue> lookupValues(const QByteArray& xml)
{
    std::vector<LookupValue> out;
    QXmlStreamReader reader(xml);
    LookupValue current;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            if (reader.name() == u"LookupValue")
                current = {reader.attributes().value("ParentID").toInt(), {}, 0};
            else if (reader.name() == u"Name")
                current.name = reader.readElementText().trimmed();
            else if (reader.name() == u"Value")
                current.value = reader.readElementText().trimmed().toInt();
        } else if (reader.isEndElement() && reader.name() == u"LookupValue") {
            out.push_back(current);
        }
    }
    return out;
}

} // namespace

bool matchesDevice(const QString& matchingDeviceId, quint32 vendorId, quint32 deviceId)
{
    static const QRegularExpression re("ven_([0-9a-f]{4})&dev_([0-9a-f]{4})", QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(matchingDeviceId);
    return m.hasMatch() && m.captured(1).toUInt(nullptr, 16) == vendorId && m.captured(2).toUInt(nullptr, 16) == deviceId;
}

QDate parseDriverDate(const QString& text)
{
    const QStringList parts = text.trimmed().split('-');
    if (parts.size() != 3)
        return {};
    return QDate(parts[2].toInt(), parts[0].toInt(), parts[1].toInt());
}

QString nvidiaVersion(const QString& windowsVersion)
{
    QString digits = windowsVersion;
    digits.remove('.');
    if (digits.size() < 5)
        return windowsVersion;
    const QString last = digits.right(5);
    return QString::number(last.left(3).toInt()) + '.' + last.right(2);
}

QString displayVersion(quint32 vendorId, const QString& windowsVersion)
{
    return vendorId == kNvidia ? nvidiaVersion(windowsVersion) : windowsVersion;
}

int compareVersions(const QString& a, const QString& b)
{
    const QStringList x = a.split('.'), y = b.split('.');
    for (qsizetype i = 0; i < std::max(x.size(), y.size()); ++i) {
        const int u = i < x.size() ? x[i].toInt() : 0;
        const int v = i < y.size() ? y[i].toInt() : 0;
        if (u != v)
            return u < v ? -1 : 1;
    }
    return 0;
}

QString vendorName(quint32 vendorId)
{
    switch (vendorId) {
    case kNvidia: return "NVIDIA";
    case kAmd:    return "AMD";
    case kIntel:  return "Intel";
    }
    return {};
}

QString driverPage(quint32 vendorId)
{
    switch (vendorId) {
    case kNvidia: return "https://www.nvidia.com/Download/index.aspx";
    case kAmd:    return "https://www.amd.com/en/support/download/drivers.html";
    case kIntel:  return "https://www.intel.com/content/www/us/en/support/detect.html";
    }
    return {};
}

QString nvidiaProduct(const QString& adapterName)
{
    QString s = adapterName.simplified();
    if (s.startsWith("NVIDIA ", Qt::CaseInsensitive))
        s = s.mid(7);
    for (const char* suffix : {" Laptop GPU", " with Max-Q Design", " Max-Q"}) {
        if (s.endsWith(suffix, Qt::CaseInsensitive))
            s.chop(int(qstrlen(suffix)));
    }
    return s.trimmed();
}

std::optional<NvidiaIds> findNvidiaIds(const QByteArray& seriesXml, const QByteArray& productsXml,
                                       const QString& product, bool notebook)
{
    const auto series = lookupValues(seriesXml);
    const auto isNotebookSeries = [&series](int id) {
        for (const auto& s : series) {
            if (s.value == id)
                return s.name.contains("Notebook", Qt::CaseInsensitive);
        }
        return false;
    };
    std::optional<NvidiaIds> fallback;
    for (const auto& p : lookupValues(productsXml)) {
        if (p.name.compare(product, Qt::CaseInsensitive) != 0)
            continue;
        const NvidiaIds ids{p.parent, p.value};
        if (isNotebookSeries(p.parent) == notebook)
            return ids;
        if (!fallback)
            fallback = ids;   // only the other kind lists it: the driver is the same
    }
    return fallback;
}

QString nvidiaLookupUrl(const NvidiaIds& ids)
{
    // osID 57: Windows 10 / 11 64-bit; dch=1: the drivers current Windows uses
    return QString("https://gfwsl.geforce.com/services_toolkit/services/com/nvidia/services/AjaxDriverService.php"
                   "?func=DriverManualLookup&psid=%1&pfid=%2&osID=57&languageCode=1033&isWHQL=1&dch=1"
                   "&upCRD=0&sort1=0&numberOfResults=1")
        .arg(ids.series).arg(ids.product);
}

std::optional<Latest> parseNvidiaLookup(const QByteArray& json)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QJsonArray ids = root.value("IDS").toArray();
    if (root.value("Success").toString() != "1" || ids.isEmpty())
        return std::nullopt;
    const QJsonObject info = ids.first().toObject().value("downloadInfo").toObject();
    Latest latest;
    latest.version    = info.value("Version").toString();
    latest.name       = QUrl::fromPercentEncoding(info.value("Name").toString().toUtf8());
    latest.detailsUrl = info.value("DetailsURL").toString();
    // "Tue Oct 06, 2026"
    latest.date = QDate::fromString(info.value("ReleaseDateTime").toString().section(' ', 1), "MMM dd, yyyy");
    if (latest.version.isEmpty())
        return std::nullopt;
    return latest;
}

} // namespace Driver
