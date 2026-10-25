#pragma once

#include <memory>
#include <vector>

namespace centui
{

class document;
class widget;

namespace details
{

/// Document-owned policy; all observers are weak and all commits run under its guard.
class focus_controller
{
public:
    explicit focus_controller(document& owner);

    [[nodiscard]] std::shared_ptr<widget> current() const;
    [[nodiscard]] std::shared_ptr<widget> modal() const;
    [[nodiscard]] bool active() const noexcept;

    /// \brief Reports the document's interaction-based preference for focus indication.
    /// Key presses, text entry, and explicit traversal enable it; mouse presses suppress it.
    /// Hover, scrolling, releases, and window activation preserve the preference. Programmatic
    /// focus and modal restoration inherit it, with indication initially enabled.
    /// This describes presentation, not the available input devices or permission to receive input.
    /// Future touch adapters can suppress indication on touch presses using the same policy.
    /// widget::focus_visible() additionally requires active focus; caret/text activity is unaffected.
    [[nodiscard]] bool show_focus_indication() const noexcept;
    [[nodiscard]] bool allowed(const std::shared_ptr<widget>& value) const;
    [[nodiscard]] bool eligible(const std::shared_ptr<widget>& value) const;
    [[nodiscard]] bool suppressed() const noexcept;

    bool request(const std::shared_ptr<widget>& value);
    bool next(bool reverse);
    void clear();
    void enter(const std::shared_ptr<widget>& root, const std::shared_ptr<widget>& initial);
    void leave(const std::shared_ptr<widget>& root);
    void activate(bool value);

    /// \brief Updates the indication preference and invalidates the active target's paint only.
    /// Does not transfer focus, change activity, emit focus notifications, or request reveal.
    void set_show_focus_indication(bool value);
    void reconcile();
    void reset_transaction() noexcept;
    void abort(bool cancel_interaction = true) noexcept;

private:
    struct modal_entry
    {
        std::weak_ptr<widget> root;
        std::weak_ptr<widget> restore;
    };

    [[nodiscard]] bool mounted_visible(const std::shared_ptr<widget>& value) const;
    static bool descendant(const std::shared_ptr<widget>& value, const std::shared_ptr<widget>& root);
    void validate_modal(const std::shared_ptr<widget>& root) const;
    void collect(const std::shared_ptr<widget>& root, std::vector<std::shared_ptr<widget>>& result) const;
    std::shared_ptr<widget> first(const std::shared_ptr<widget>& root) const;
    void transfer(std::shared_ptr<widget> value);
    void activity(const std::shared_ptr<widget>& value, bool active);
    void unwind(std::size_t index);
    void count_transition();
    void cancel_outside();

    document& m_owner;
    std::weak_ptr<widget> m_current;
    std::vector<modal_entry> m_modals;
    bool m_active = true;
    // Retained independently of logical ownership and window activation.
    bool m_show_focus_indication = true;
    bool m_suppressed = false;
    bool m_notifying = false;
    unsigned m_transitions = 0;
};

} // namespace details
} // namespace centui
