#pragma once

#include "DriverModel.h"
#include "StatsModel.h"

#include <QObject>

#include <functional>

class QNetworkAccessManager;

/**
 * Asks NVIDIA for the newest Game Ready driver of a card (the lookup behind nvidia.com/Download:
 * series list, product list, then the driver search). Other vendors have no public lookup, so
 * check() answers at once with `checked == false` and the panel offers the vendor's page.
 */
class DriverCheck : public QObject
{
    Q_OBJECT

public:
    struct Result
    {
        quint64 luid{0};
        bool    checked{false};   // false: not an NVIDIA card, or the lookup failed (see error)
        Driver::Latest latest;
        QString error;
    };

    explicit DriverCheck(QObject* parent = nullptr);

    // notebook: the PC has a battery (NVIDIA lists laptop GPUs in their own series).
    void check(const Stats::Adapter& adapter, bool notebook);

signals:
    void finished(const DriverCheck::Result& result);

private:
    void get(const QString& url, std::function<void(const QByteArray&, const QString&)> done);
    void lookup(Result result, const QString& product, bool notebook);

    QNetworkAccessManager* m_net;
    QByteArray m_series;     // cached for the app's run
    QByteArray m_products;
};
