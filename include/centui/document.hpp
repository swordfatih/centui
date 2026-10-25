#pragma once

#include <centui/widget.hpp>

namespace centui
{

/// \brief Owns a widget tree and coordinates layout, painting, focus, and input.
struct preparation_statistics
{
    std::size_t layout_nodes_created = 0;
    std::size_t paint_visits = 0;
    std::size_t command_cache_hits = 0;
    std::size_t command_cache_bytes = 0;
};

class document
{
public:
    document();

    explicit document(handle<> root);

    ~document();

    document(const document&) = delete;

    document& operator=(const document&) = delete;

    document(document&&) = delete;

    document& operator=(document&&) = delete;

    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] handle<> root() const noexcept;

    [[nodiscard]] text_service& fonts() noexcept;

    void set_font(const centui::font& face);

    /// \brief Replaces document defaults and immediately resolves mounted widgets.
    void set_style(const style_patch& defaults);

    /// \brief Merges a document override for an exact composed style type.
    template <typename Style>
    void patch_style(const style_patch& patch)
    {
        patch_style_type(typeid(Style), patch);
    }

    /// \brief Removes the document override for an exact composed style type.
    template <typename Style>
    void clear_style_patch()
    {
        clear_style_type(typeid(Style));
    }

    /// \brief Validates layout configuration and requests fresh geometry.
    void set_layout_options(layout_options options);

    [[nodiscard]] const layout_options& options() const noexcept;

    void set_host_services(std::shared_ptr<const host_services> services);

    [[nodiscard]] invalidation pending_changes() const noexcept;

    /// \brief Replaces the mounted tree with a detached root.
    void set_root(handle<> value);

    [[nodiscard]] std::vector<handle<>> children(const handle<>& parent) const;

    [[nodiscard]] handle<> parent(const handle<>& child) const;

    void append(const handle<>& parent, const handle<>& child);

    void append(const handle<>& parent, std::span<const handle<>> children);

    void remove(const handle<>& child);

    /// \brief Returns a borrowed draw list, valid until the next preparation or destruction.
    [[nodiscard]] const draw_list& prepare(centui::viewport viewport);

    /// \brief Delivers one tick to requesting widgets. Duration is nonnegative logical seconds.
    /// Callbacks may edit the tree; edits are committed after the tick batch.
    void advance(frame_duration elapsed);

    /// \brief Sets the retained raster cache budget; live draw lists and scratch storage are additional.
    void set_paint_cache_budget(std::size_t bytes);

    void set_command_cache_budget(std::size_t bytes);

    [[nodiscard]] preparation_statistics statistics() const noexcept;

    void process_event(const input_event& event, centui::viewport viewport);

    /// \brief Dispatches portable events in order, then applies deferred tree changes.
    void process_events(std::span<const input_event> events, centui::viewport viewport);

private:
    friend class widget;
    friend class details::event_access;

    void patch_style_type(details::style_type_id type, const style_patch& patch);

    void clear_style_type(details::style_type_id type);

    using style_updates = std::vector<std::pair<std::shared_ptr<widget>, centui::style>>;

    static void stage_styles(
        style_updates& updates,
        const std::shared_ptr<widget>& root,
        const style_patch& defaults,
        const details::style_patch_map& patches,
        std::optional<details::style_type_id> type = std::nullopt
    );

    static bool commit_styles(style_updates& updates);

    void update_styles(
        style_patch defaults,
        details::style_patch_map patches,
        std::optional<details::style_type_id> type
    );

    void interaction_changed();

    void request_frame(const std::shared_ptr<widget>& value);

    bool request_focus(const handle<>& target);

    bool advance_focus(const handle<>& scope, bool reverse);

    void collect_focus(const std::shared_ptr<widget>& value, std::vector<handle<>>& result) const;

    void sync_host();

    void dispatch_guarded(const std::function<void()>& operation);

    struct phase_guard;
    struct layout_entry;
    struct layout_state;
    struct frame_scheduler;
    struct input_state;

    static void associate(
        const std::shared_ptr<widget>& value,
        const std::shared_ptr<details::document_context>& context
    ) noexcept;

    [[nodiscard]] std::shared_ptr<widget> checked(const handle<>& value) const;

    void validate_root(const handle<>& value) const;

    void validate_append(const handle<>& parent, std::span<const handle<>> children) const;

    void dirty_tree() noexcept;

    void edit(std::function<void()> operation);

    void flush_edits();

    void settle_input(centui::viewport viewport);

    void sync_layout(std::unique_ptr<layout_entry>& entry, const std::shared_ptr<widget>& value, bool inherited_dirty);

    static void check_measurements(layout_entry& entry);

    static void store_bounds(layout_entry& entry, float x, float y, bool force);

    void update_layout(centui::viewport viewport);

    void notify_layout(const std::shared_ptr<widget>& value);

    void paint_tree(
        const widget& value,
        draw_list& output,
        rect clip,
        bool enabled,
        details::visual_state inherited_state = details::visual_state::normal
    );

    void paint_tree_uncached(
        const widget& value,
        draw_list& output,
        rect clip,
        bool enabled,
        details::visual_state inherited_state
    );

    static bool contains(rect box, float x, float y) noexcept;

    [[nodiscard]] bool eligible(const std::shared_ptr<widget>& value) const;

    [[nodiscard]] bool focus_eligible(const std::shared_ptr<widget>& value) const;

    void set_focus(std::shared_ptr<widget> value);

    std::shared_ptr<widget> hit(const std::shared_ptr<widget>& value, float x, float y, rect clip) const;

    std::shared_ptr<widget> hit(float x, float y) const;

    // Uncaptured motion may fall back to an ancestor. Gestures remain target-only.
    void route_motion(std::shared_ptr<widget> target, const input_event& event);

    bool route_event(std::shared_ptr<widget> target, const input_event& event);

    void set_hover(std::shared_ptr<widget> value);

    void cancel_capture();

    void clear_input();

    void synchronize_input();

    void dispatch(const input_event& event);

    struct data;
    std::unique_ptr<data> m_data;
};

} // namespace centui
