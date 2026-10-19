module;

#include <cstddef>
#include <memory>

export module centui.diagnostics;

export namespace centui
{

struct preparation_statistics
{
    std::size_t topology_syncs = 0;
    std::size_t shapings = 0;
    std::size_t measurements = 0;
    std::size_t style_updates = 0;
    std::size_t layout_passes = 0;
    std::size_t paint_builds = 0;

    bool operator==(const preparation_statistics&) const = default;
};

namespace details
{

// Optional instrumentation; no counters or event history are stored in nodes.
class preparation_diagnostics
{
public:
    void enable(bool enabled)
    {
        if(enabled && !_statistics)
        {
            _statistics = std::make_unique<preparation_statistics>();
        }
        else if(!enabled)
        {
            _statistics.reset();
        }
    }

    [[nodiscard]] bool enabled() const noexcept
    {
        return static_cast<bool>(_statistics);
    }

    [[nodiscard]] preparation_statistics snapshot() const noexcept
    {
        return _statistics ? *_statistics : preparation_statistics{};
    }

    void record(std::size_t preparation_statistics::* counter) noexcept
    {
        if(_statistics)
        {
            ++(_statistics.get()->*counter);
        }
    }

private:
    std::unique_ptr<preparation_statistics> _statistics;
};

} // namespace details
} // namespace centui
