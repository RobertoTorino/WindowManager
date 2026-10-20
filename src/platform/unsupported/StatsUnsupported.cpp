#include "../../stats/StatsBackend.h"

// The stats bar reads Windows' performance counters and event traces. Until there is an
// equivalent for the platform (/proc for CPU and memory, a Vulkan layer for frames), the bar
// stays hidden.

namespace {

class UnsupportedStats final : public StatsBackend
{
public:
    bool supported() const override { return false; }
    std::vector<Stats::Adapter> adapters() override { return {}; }
    Stats::RawSample sample(qint64) override { return {}; }
    void shutdown() override {}

    bool gpuPreferenceSupported() const override { return false; }
    Stats::GpuPreference gpuPreference(const QString&) override { return Stats::GpuPreference::Auto; }
    bool setGpuPreference(const QString&, Stats::GpuPreference, QString* error) override
    {
        if (error)
            *error = "Choosing the graphics card is not available on this system.";
        return false;
    }
    QString adapterFor(Stats::GpuPreference) override { return {}; }

    Stats::Power power() override { return {}; }
    std::vector<Stats::PowerPlan> powerPlans() override { return {}; }
    bool setPowerPlan(const QString&, QString* error) override { return refuse(error); }
    bool setPowerMode(Stats::PowerMode, QString* error) override { return refuse(error); }

private:
    static bool refuse(QString* error)
    {
        if (error)
            *error = "Power settings are not available on this system.";
        return false;
    }
};

} // namespace

StatsBackend& statsBackend()
{
    static UnsupportedStats backend;
    return backend;
}
