#include "../support/studio.hpp"

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <centui/backends/bgfx.hpp>
#include <centui/platforms/sdl.hpp>

namespace
{

using centui::handle;
using centui::make_widget;
using namespace studio;

handle<centui::container_widget> group(centui::style appearance, std::initializer_list<handle<>> children = {})
{
    return make_widget<centui::container_widget>(std::move(appearance), std::span{children.begin(), children.size()});
}

handle<centui::text_widget> label(std::string value, centui::style appearance = centui::style::resolve<muted>())
{
    return make_widget<centui::text_widget>(std::move(value), std::move(appearance));
}

handle<centui::container_widget> field(std::string title, handle<> control)
{
    return group(centui::style::resolve<stack>(), {label(std::move(title)), control});
}

handle<centui::button_widget> action(std::string title, std::function<void()> callback)
{
    return make_widget<centui::button_widget>(
        std::move(title),
        centui::button_options{.on_click = std::move(callback)},
        centui::style::resolve<primary>()
    );
}

class studio_view
{
public:
    studio_view()
    {
        auto projects = project_panel();
        auto editor = editor_panel();
        auto preview = preview_panel();
        auto dialog = confirmation_dialog();
        m_apply->set_on_click(
            [state = m_state, dialog]
            {
                state->apply();
                dialog->open();
            }
        );

        auto content = group(centui::style::resolve<page>());
        content->append(group(
            centui::style::resolve<stack>(),
            {
                label("NORTHSTAR / PERSONAL WORKSPACE", centui::style::resolve<accent>()),
                label("Make room for your best work.", centui::style::resolve<heading>()),
                label("A quiet place to shape projects, tune your focus, and plan what comes next."),
            }
        ));
        content->append(group(centui::style::resolve<row>(), {projects, editor, preview}));
        content->append(m_state->status);
        content->append(label("Tab to move between controls. Enter to activate. Escape to close the dialog."));

        auto scroll = make_widget<centui::scroll_widget>(centui::style::resolve<screen>());
        scroll->append(content);
        m_root = group(centui::style::resolve<screen>(), {scroll, dialog});
    }

    handle<> root() const
    {
        return m_root;
    }

private:
    std::shared_ptr<settings> m_state = std::make_shared<settings>();
    handle<centui::toggle_widget> m_notifications;
    handle<centui::range_widget> m_duration;
    handle<centui::button_widget> m_apply;
    handle<centui::container_widget> m_root;

    handle<> project_panel()
    {
        auto list = make_widget<centui::selection_widget>(
            projects(),
            centui::selection_options{},
            centui::style::resolve<selection_style>()
        );
        list->set_selected(m_state->project);
        list->set_on_change(
            [state = m_state](std::size_t index)
            {
                state->project = index;
                state->changed();
            }
        );
        auto scroll = make_widget<centui::scroll_widget>(centui::style::resolve<project_list>());
        scroll->append(list);

        return group(
            centui::style::resolve<card, sidebar>(),
            {
                label("01 / PROJECTS", centui::style::resolve<accent>()),
                label("On your desk", centui::style::resolve<subheading>()),
                label("Choose a project to focus on."),
                scroll,
                label("Scroll for more projects.\nUse arrow keys to select."),
            }
        );
    }

    handle<> editor_panel()
    {
        auto name = make_widget<centui::input_widget>(
            m_state->name,
            centui::input_options{},
            centui::style::resolve<control>()
        );
        name->set_on_change(
            [state = m_state](std::string_view value)
            {
                state->name = value;
                state->changed();
            }
        );
        auto notes = make_widget<centui::input_widget>(
            m_state->notes,
            centui::input_options{.multiline = true},
            centui::style::resolve<control, multiline>()
        );
        notes->set_on_change(
            [state = m_state](std::string_view value)
            {
                state->notes = value;
                state->changed();
            }
        );
        m_duration = make_widget<centui::range_widget>(
            centui::range_options{.minimum = 15, .maximum = 90, .step = 5},
            centui::style::resolve<control>()
        );
        m_duration->set_value(m_state->minutes);
        m_duration->set_on_change(
            [state = m_state](float value)
            {
                state->minutes = value;
                state->changed();
            }
        );
        m_notifications = make_widget<centui::toggle_widget>(
            "Notifications",
            centui::toggle_options{.checked = m_state->notifications},
            centui::style::resolve<control>()
        );
        m_notifications->set_on_change(
            [state = m_state](bool value)
            {
                state->notifications = value;
                state->changed();
            }
        );
        m_apply = action("Apply changes", {});

        return group(
            centui::style::resolve<card, editor>(),
            {
                label("02 / PREFERENCES", centui::style::resolve<accent>()),
                label("Your workspace", centui::style::resolve<subheading>()),
                field("Studio name", name),
                field("A note for later", notes),
                group(centui::style::resolve<stack>(), {m_state->duration, m_duration}),
                m_notifications,
                m_apply,
            }
        );
    }

    handle<> preview_panel()
    {
        auto preset_content = group(
            centui::style::resolve<stack>(),
            {
                label("25 min", centui::style::resolve<subheading>()),
                label("Quiet preset\nClick to switch off distractions."),
            }
        );
        auto quiet = make_widget<centui::pressable_widget>(
            preset_content,
            centui::pressable_options{},
            centui::style::resolve<preset>()
        );
        quiet->set_on_click(
            [state = m_state, notifications = m_notifications, duration = m_duration]
            {
                notifications->set_checked(false);
                duration->set_value(25);
                state->notifications = false;
                state->minutes = 25;
                state->changed();
            }
        );
        return group(
            centui::style::resolve<card, preview>(),
            {
                label("03 / LIVE PREVIEW", centui::style::resolve<accent>()),
                cover(),
                m_state->preview_name,
                m_state->details,
                quiet,
            }
        );
    }

    handle<centui::popup_widget> confirmation_dialog()
    {
        auto close = action("Back to studio", {});
        auto content = group(
            centui::style::resolve<card, dialog_size>(),
            {
                label("ALL SET", centui::style::resolve<accent>()),
                label("Your workspace is ready.", centui::style::resolve<subheading>()),
                m_state->confirmation,
                label("These settings are kept in memory for this session."),
                close,
            }
        );
        auto dialog = make_widget<centui::popup_widget>(content, centui::style::resolve<overlay>());
        close->set_on_click(
            [weak = dialog.weak()]
            {
                if(auto popup = weak.lock(); popup.valid())
                {
                    popup->close();
                }
            }
        );
        return dialog;
    }
};

} // namespace

int main()
{
    centui::sdl::platform platform{{.title = "CentUI / Studio / Dynamic tree", .width = 1160, .height = 820}};
    centui::bgfx::backend renderer{platform};
    studio_view view;
    centui::document document{view.root()};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    centui::run(platform, renderer, document);
}
