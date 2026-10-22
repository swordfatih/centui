module;

#include <memory>
#include <span>
#include <string>

export module centui.platform;
export import centui.types;

export namespace centui
{
struct window_options
{
    std::string title{};
    int width = 800;
    int height = 600;
};

class platform
{
public:
    virtual ~platform() = default;

    [[nodiscard]] virtual bool poll_events() = 0;
    [[nodiscard]] virtual std::span<const input_event> events() const noexcept = 0;
    [[nodiscard]] virtual centui::viewport viewport() const = 0;
    [[nodiscard]] virtual std::shared_ptr<const host_services> services() const noexcept = 0;
};
}
