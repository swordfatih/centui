#include "../support/studio.hpp"

#include <centui/backends/bgfx.hpp>
#include <centui/platforms/sdl.hpp>

int main()
{
    using namespace studio;
    centui::sdl::platform platform{{.title = "CentUI / Studio / Static tree", .width = 1160, .height = 820}};
    centui::bgfx::backend renderer{platform};
    auto state = std::make_shared<settings>();
    auto notifications = centui::toggle<control>{
        "Notifications",
        {
            .checked = state->notifications,
            .on_change = [state](bool checked)
            {
                state->notifications = checked;
                state->changed();
            },
        }
    };
    auto duration = centui::range<control>{{
        .minimum = 15,
        .maximum = 90,
        .step = 5,
        .value = state->minutes,
        .on_change = [state](float value)
        {
            state->minutes = value;
            state->changed();
        },
    }};
    auto dismiss = centui::button<primary>{"Back to studio"};
    auto dialog = centui::popup<overlay>{
        centui::container<card, dialog_size>{
            centui::text<accent>{"ALL SET"},
            centui::text<subheading>{"Your workspace is ready."},
            state->confirmation,
            centui::text<muted>{"These settings are kept in memory for this session."},
            dismiss,
        },
    };
    dismiss.set_on_click(
        [weak = dialog.weak()]
        {
            if(auto popup = weak.lock(); popup.valid())
            {
                popup->close();
            }
        }
    );
    auto root = centui::container<screen>{
        centui::scroll<screen>{
            centui::container<page>{
                centui::container<stack>{
                    centui::text<accent>{"NORTHSTAR / PERSONAL WORKSPACE"},
                    centui::text<heading>{"Make room for your best work."},
                    centui::text<muted>{"A quiet place to shape projects, tune your focus, and plan what comes next."},
                },
                centui::container<row>{
                    centui::container<card, sidebar>{
                        centui::text<accent>{"01 / PROJECTS"},
                        centui::text<subheading>{"On your desk"},
                        centui::text<muted>{"Choose a project to focus on."},
                        centui::scroll<project_list>{
                            centui::selection<selection_style>{projects(), {
                                .selected = state->project,
                                .on_change = [state](std::size_t index)
                                {
                                    state->project = index;
                                    state->changed();
                                },
                            }},
                        },
                        centui::text<muted>{"Scroll for more projects.\nUse arrow keys to select."},
                    },
                    centui::container<card, editor>{
                        centui::text<accent>{"02 / PREFERENCES"},
                        centui::text<subheading>{"Your workspace"},
                        centui::container<stack>{
                            centui::text<muted>{"Studio name"},
                            centui::input<control>{state->name, {
                                .on_change = [state](std::string_view value)
                                {
                                    state->name = value;
                                    state->changed();
                                },
                            }},
                        },
                        centui::container<stack>{
                            centui::text<muted>{"A note for later"},
                            centui::input<control, multiline>{state->notes, {
                                .multiline = true,
                                .on_change = [state](std::string_view value)
                                {
                                    state->notes = value;
                                    state->changed();
                                },
                            }},
                        },
                        centui::container<stack>{state->duration, duration},
                        notifications,
                        centui::button<primary>{"Apply changes", {
                            .on_click = [state, dialog]
                            {
                                state->apply();
                                dialog->open();
                            },
                        }},
                    },
                    centui::container<card, preview>{
                        centui::text<accent>{"03 / LIVE PREVIEW"},
                        cover(),
                        state->preview_name,
                        state->details,
                        centui::pressable<preset>{
                            centui::container<stack>{
                                centui::text<subheading>{"25 min"},
                                centui::text<muted>{"Quiet preset\nClick to switch off distractions."},
                            },
                            {
                                .on_click = [state, notifications, duration]() mutable
                                {
                                    notifications->set_checked(false);
                                    duration->set_value(25);
                                    state->notifications = false;
                                    state->minutes = 25;
                                    state->changed();
                                },
                            },
                        },
                    },
                },
                state->status,
                centui::text<muted>{"Tab to move between controls. Enter to activate. Escape to close the dialog."},
            },
        },
        dialog,
    };

    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    centui::run(platform, renderer, document);
}
