#pragma once

#include "paint_service.hpp"

#include <map>

#include <centui/document.hpp>

namespace centui
{
namespace details
{

struct document_context
{
    explicit document_context(document& value);

    bool layout_dirty = true;
    bool paint_dirty = true;
    bool placement_dirty = true;
    bool preparing = false;
    // The document owns this context; widgets only keep weak references to it.
    document& owner;

    void check_mutation() const;
};

[[nodiscard]] color state_color(const states_color&, visual_state) noexcept;

} // namespace details

struct document::phase_guard
{
    details::document_context& context;

    explicit phase_guard(details::document_context& value);

    ~phase_guard();
};

struct document::layout_entry
{
    explicit layout_entry(const layout_configuration& configuration)
        : native(configuration)
    {
    }

    std::shared_ptr<widget> value;
    centui::layout native;
    std::optional<centui::style> applied_style;
    bool measured = false;
    std::vector<std::unique_ptr<layout_entry>> children;
};

struct document::layout_state
{
    centui::layout_options layout_options{};
    std::unique_ptr<layout_configuration> configuration;
    std::unique_ptr<layout> viewport_layout;
    std::unique_ptr<layout_entry> tree;
    bool measure_all = true;
};

struct document::frame_scheduler
{
    bool frames_dirty = true;
    std::vector<std::weak_ptr<widget>> frames;

    void request(const std::shared_ptr<widget>& value);
};

struct document::input_state
{
    std::weak_ptr<widget> hover;
    std::weak_ptr<widget> capture;
    mouse_button capture_button = mouse_button::primary;
    std::weak_ptr<widget> focus;

    bool host_focused = true;
    bool pointer_known = false;
    bool hit_dirty = true;
    float pointer_x = 0;
    float pointer_y = 0;

    std::shared_ptr<const host_services> host;
    std::optional<rect> text_input_area;
};

struct document::data
{
    handle<> root;
    std::shared_ptr<details::document_context> context;
    text_service fonts;
    details::paint_service drawings;
    std::shared_ptr<const void> font_revision;
    draw_list draw;

    struct command_entry
    {
        std::size_t revision;
        std::vector<draw_quad> commands;
    };

    std::map<std::weak_ptr<const widget>, command_entry, std::owner_less<>> commands;
    std::size_t command_budget = 4 * 1024 * 1024;
    preparation_statistics statistics;

    void clear_commands()
    {
        commands.clear();
        statistics.command_cache_bytes = 0;
    }

    centui::viewport viewport{};
    layout_state geometry;
    frame_scheduler scheduler;

    std::vector<std::function<void()>> edits;
    bool dispatching = false;
    input_state input;

};

} // namespace centui
