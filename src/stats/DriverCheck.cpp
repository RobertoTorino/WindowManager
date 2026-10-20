#include "DriverCheck.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

const char* kSeriesUrl   = "https://www.nvidia.com/Download/API/lookupValueSearch.aspx?TypeID=2";
const char* kProductsUrl = "https://www.nvidia.com/Download/API/lookupValueSearch.aspx?TypeID=3";

} // namespace

DriverCheck::DriverCheck(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

void DriverCheck::get(const QString& url, std::function<void(const QByteArray&, const QString&)> done)
{
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("WindowManager/") + APP_VERSION_STR);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(20000);
    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            done({}, reply->errorString());
        else
            done(reply->readAll(), {});
    });
}

void DriverCheck::check(const Stats::Adapter& adapter, bool notebook)
{
    Result result;
    result.luid = adapter.luid;
    if (adapter.vendorId != Driver::kNvidia) {
        emit finished(result);
        return;
    }
    const QString product = Driver::nvidiaProduct(adapter.name);
    if (!m_series.isEmpty() && !m_products.isEmpty()) {
        lookup(result, product, notebook);
        return;
    }
    get(kSeriesUrl, [this, result, product, notebook](const QByteArray& series, const QString& error) mutable {
        if (!error.isEmpty()) {
            result.error = error;
            emit finished(result);
            return;
        }
        m_series = series;
        get(kProductsUrl, [this, result, product, notebook](const QByteArray& products, const QString& error) mutable {
            if (!error.isEmpty()) {
                result.error = error;
                emit finished(result);
                return;
            }
            m_products = products;
            lookup(result, product, notebook);
        });
    });
}

void DriverCheck::lookup(Result result, const QString& product, bool notebook)
{
    const auto ids = Driver::findNvidiaIds(m_series, m_products, product, notebook);
    if (!ids) {
        result.error = QString("NVIDIA's driver list has no \"%1\".").arg(product);
        emit finished(result);
        return;
    }
    get(Driver::nvidiaLookupUrl(*ids), [this, result](const QByteArray& json, const QString& error) mutable {
        const auto latest = error.isEmpty() ? Driver::parseNvidiaLookup(json) : std::nullopt;
        if (latest) {
            result.checked = true;
            result.latest  = *latest;
        } else {
            result.error = error.isEmpty() ? QString("NVIDIA's answer could not be read.") : error;
        }
        emit finished(result);
    });
}
