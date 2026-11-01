#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <utility>

#include <centui/widgets/scroll.hpp>

namespace centui
{

struct list_options
{
    std::size_t item_count = 0;

    float row_height = 32;

    /// Extra rows retained on either side of the viewport.
    std::size_t overscan = 2;
};

/// \brief A vertical, fixed-height list that mounts only rows near its viewport.
/// Factories return fresh detached widgets; keep persistent item state in the application.
/// Rows leaving the rendered range are detached, including any focus they own.
class list_widget : public scroll_widget
{
public:
    using row_factory = std::function<handle<>(std::size_t)>;

    explicit list_widget(row_factory factory, list_options options = {}, centui::style_source appearance = {});

    [[nodiscard]] std::size_t item_count() const noexcept;

    void set_item_count(std::size_t count);

    /// Recreates mounted rows on the next preparation after data changes or reordering.
    void refresh();

    /// Reveals an item after geometry settles, including before the first frame.
    void scroll_to(std::size_t index);

    /// Half-open mounted range, including overscan; updated during document preparation.
    [[nodiscard]] std::pair<std::size_t, std::size_t> rendered_range() const noexcept;

protected:
    void on_geometry() override;

private:
    class content_widget;

    void check_factory_mutation() const;

    row_factory m_factory;
    list_options m_options;
    handle<content_widget> m_content;
    std::map<std::size_t, handle<container_widget>> m_rows;
    std::pair<std::size_t, std::size_t> m_range{};
    std::optional<std::size_t> m_scroll_target;
    bool m_refresh = false;
    bool m_building = false;
};

/// \brief A virtualized list handle with compile-time style composition.
template <typename... Styles>
class list : public handle<list_widget>
{
public:
    explicit list(list_widget::row_factory factory, list_options options = {}, centui::style_source appearance = {})
        : handle<list_widget>(make_widget<list_widget>(
              std::move(factory),
              options,
              centui::style_source::with_composition<Styles...>(std::move(appearance))
          ))
    {
    }
};

} // namespace centui
