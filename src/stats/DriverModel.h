#pragma once

#include <QDate>
#include <QString>

#include <optional>

/**
 * Pure logic for the graphics driver line on the GPU & Power tab (tst_stats): which installed
 * driver belongs to which card, NVIDIA's own version number, and NVIDIA's public driver lookup
 * (the one behind nvidia.com/Download), which needs a series id and a product id.
 */
namespace Driver {

constexpr quint32 kNvidia = 0x10DE, kAmd = 0x1002, kIntel = 0x8086;

struct Installed
{
    QString version;    // Windows' driver version, "32.0.16.1742"
    QDate   date;
    QString provider;   // "NVIDIA"
};

// MatchingDeviceId from the display class key ("pci\ven_10de&dev_1f99&subsys_86681558") against a card.
bool matchesDevice(const QString& matchingDeviceId, quint32 vendorId, quint32 deviceId);

// DriverDate as Windows stores it: "9-30-2026" (month-day-year).
QDate parseDriverDate(const QString& text);

// NVIDIA's number from Windows' version: the last five digits, "32.0.16.1742" -> "617.42".
QString nvidiaVersion(const QString& windowsVersion);

// The version users know: NVIDIA's for NVIDIA, Windows' otherwise.
QString displayVersion(quint32 vendorId, const QString& windowsVersion);

// -1, 0, 1 for dotted numbers ("610.47" < "617.42").
int compareVersions(const QString& a, const QString& b);

QString vendorName(quint32 vendorId);
QString driverPage(quint32 vendorId);   // the vendor's download / detect page

// "NVIDIA GeForce RTX 4060 Laptop GPU" -> "GeForce RTX 4060"
QString nvidiaProduct(const QString& adapterName);

// From lookupValueSearch.aspx?TypeID=2 (series) and TypeID=3 (products): the ids of a product.
// A product can be in a desktop and a notebook series; `notebook` picks the "(Notebooks)" one.
struct NvidiaIds
{
    int series{0};    // psid
    int product{0};   // pfid
};
std::optional<NvidiaIds> findNvidiaIds(const QByteArray& seriesXml, const QByteArray& productsXml,
                                       const QString& product, bool notebook);

QString nvidiaLookupUrl(const NvidiaIds& ids);

// The newest driver from AjaxDriverService.php?func=DriverManualLookup.
struct Latest
{
    QString version;      // "617.42"
    QDate   date;
    QString name;         // "GeForce Game Ready Driver"
    QString detailsUrl;
};
std::optional<Latest> parseNvidiaLookup(const QByteArray& json);

} // namespace Driver
