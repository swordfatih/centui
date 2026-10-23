#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <centui/tree.hpp>
#include <centui/widgets/callback.hpp>

namespace centui
{

/// \brief Selects one text item from a list using pointer or keyboard input.
class selection_widget : public widget
{
public:
    explicit selection_widget(std::vector<std::string> items, centui::style appearance = {});

    void set_items(std::vector<std::string> items);

    void set_selected(std::optional<std::size_t> index);

    [[nodiscard]] std::optional<std::size_t> selected() const noexcept;

    void set_on_change(std::function<void(std::size_t)> callback);

protected:
    size measure(const measure_context& context) override;

    void paint(painter& output) const override;

    bool on_event(const input_event& event) override;

private:
    float row_height() const noexcept;

    std::vector<std::string> m_items;
    std::optional<std::size_t> m_selected;
    details::callback<std::size_t> m_on_change;
};

/// \brief Initial selection and selection-change callback.
struct selection_options
{
    std::optional<std::size_t> selected{};
    std::function<void(std::size_t)> on_change{};
};

/// \brief A selection list handle with compile-time style composition.
template <typename... Styles>
class selection : public handle<selection_widget>
{
public:
    explicit selection(
        std::vector<std::string> items,
        selection_options options = {},
        centui::style appearance = centui::style::resolve<Styles...>()
    )
        : handle<selection_widget>(make_widget<selection_widget>(std::move(items), std::move(appearance)))
    {
        this->get().set_selected(options.selected);
        this->get().set_on_change(std::move(options.on_change));
    }
};

} // namespace centui
