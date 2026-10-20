#include "../../display/DisplayBackend.h"

// The Display tab changes Windows' display settings. Until there is an equivalent for the
// platform (KDE: kscreen-doctor), the tab says it is not available and every setter fails.

namespace {

class UnsupportedDisplay final : public DisplayBackend
{
public:
    bool supported() const override { return false; }
    std::vector<DisplayModel::Monitor> monitors() const override { return {}; }

    bool setMode(const QString&, const DisplayModel::Mode&, DisplayModel::Orientation, bool, QString* error) override
    { return refuse(error); }
    bool resetMode(const QString&, QString* error) override { return refuse(error); }
    bool setHdr(const QString&, bool, QString* error) override { return refuse(error); }
    bool setScale(const QString&, int, QString* error) override { return refuse(error); }
    bool setBrightness(const QString&, int, QString* error) override { return refuse(error); }
    bool setColorProfile(const QString&, const QString&, QString* error) override { return refuse(error); }

    DisplayModel::Topology topology() const override { return DisplayModel::Topology::Unknown; }
    int  connectedDisplays() const override { return 0; }
    bool setTopology(DisplayModel::Topology, QString* error) override { return refuse(error); }
    QString nightLightUrl() const override { return {}; }

private:
    static bool refuse(QString* error)
    {
        if (error)
            *error = "Display settings are not available on this system yet.";
        return false;
    }
};

} // namespace

DisplayBackend& displayBackend()
{
    static UnsupportedDisplay backend;
    return backend;
}
