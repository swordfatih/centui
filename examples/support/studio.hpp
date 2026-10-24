#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include <centui/centui.hpp>

namespace studio
{

inline centui::states_color solid(std::uint32_t rgb)
{
    return centui::states_color::all(centui::color::from_hex(rgb));
}

struct screen
{
    centui::length width = centui::percent(100);
    centui::length height = centui::percent(100);
    centui::states_color background_color = solid(0x101722);
};

struct page
{
    centui::edges padding = centui::edges::all(centui::px(28));
    centui::length gap = centui::px(22);
};

struct stack
{
    centui::length gap = centui::px(10);
};

struct row
{
    centui::direction flex_direction = centui::direction::row;
    centui::alignment align_items = centui::alignment::start;
    centui::length gap = centui::px(16);
};

struct card
{
    float corner_radius = 14;
    centui::edges padding = centui::edges::all(centui::px(20));
    centui::length gap = centui::px(16);
    centui::states_color background_color = solid(0x1B2636);
    centui::edges border_width = centui::edges::all(centui::px(1));
    centui::states_color border_color = solid(0x304156);
};

struct sidebar
{
    centui::flex flex = centui::flex::all(6, 1, centui::px(0));
};

struct editor
{
    centui::flex flex = centui::flex::all(11, 1, centui::px(0));
};

struct preview
{
    centui::flex flex = centui::flex::all(7, 1, centui::px(0));
};

struct heading
{
    float font_size = 34;
};

struct subheading
{
    float font_size = 22;
};

struct muted
{
    float font_size = 14;
    centui::states_color text_color = solid(0xA4B3C7);
};

struct accent
{
    float font_size = 13;
    centui::states_color text_color = solid(0x7DE2D1);
};

struct control
{
    float corner_radius = 7;
    centui::length height = centui::px(44);
    centui::edges padding = centui::edges::all(centui::px(10));
    centui::states_color background_color = solid(0x26364A);
    centui::edges border_width = centui::edges::all(centui::px(1));

    struct
    {
        centui::color normal = centui::color::from_hex(0x40536D);
        centui::color hover = centui::color::from_hex(0x6C8CB0);
        centui::color focused = centui::color::from_hex(0x7DE2D1);
    } border_color;
};

struct multiline
{
    centui::length height = centui::px(100);
};

struct project_list
{
    centui::length height = centui::px(210);
};

struct selection_style
{
    float font_size = 17;

    struct
    {
        centui::color normal = centui::color::from_hex(0xE2EAF4);
        centui::color focused = centui::color::from_hex(0x7DE2D1);
    } text_color;
};

struct primary
{
    float corner_radius = 8;
    centui::length height = centui::px(46);
    centui::edges padding = centui::edges::all(centui::px(12));
    centui::text_alignment text_align = centui::text_alignment::center;

    struct
    {
        centui::color normal = centui::color::from_hex(0x176D68);
        centui::color hover = centui::color::from_hex(0x21867E);
        centui::color pressed = centui::color::from_hex(0x11534F);
        centui::color focused = centui::color::from_hex(0x21867E);
    } background_color;
};

struct preset
{
    float corner_radius = 10;
    centui::edges padding = centui::edges::all(centui::px(16));

    struct
    {
        centui::color normal = centui::color::from_hex(0x22394B);
        centui::color hover = centui::color::from_hex(0x2C4B60);
        centui::color pressed = centui::color::from_hex(0x1A2D3D);
        centui::color focused = centui::color::from_hex(0x2C4B60);
    } background_color;
};

struct overlay
{
    centui::alignment align_items = centui::alignment::center;
    centui::justification justify_content = centui::justification::center;
    centui::states_color background_color = centui::states_color::all({4, 10, 18, 210});
};

struct dialog_size
{
    centui::length width = centui::px(440);
};

inline std::vector<std::string> projects()
{
    return {
        "Brand refresh",
        "Website launch",
        "Summer collection",
        "Mobile companion",
        "Photography",
        "Design system",
        "Field notes",
        "Next chapter",
        "Archive"
    };
}

inline centui::handle<centui::image_widget> cover()
{
    static const auto source = centui::image_source::from_svg(R"svg(
        <svg xmlns="http://www.w3.org/2000/svg" width="64" height="40" viewBox="0 0 64 40">
            <defs>
                <pattern id="stripes" width="40" height="40" patternUnits="userSpaceOnUse"
                         patternTransform="skewX(-26.565)">
                    <rect width="40" height="40" fill="#24485D"/>
                    <rect width="18" height="40" fill="#52C7B8"/>
                </pattern>
            </defs>
            <rect width="64" height="40" fill="url(#stripes)"/>
        </svg>
    )svg");

    centui::style appearance;
    appearance.height = centui::px(120);

    return centui::image<>{source, {}, appearance};
}

struct settings
{
    std::string name = "Northstar studio";
    std::string notes = "Make space for focused work.\nKeep the next step small.";
    std::size_t project = 0;
    float minutes = 45;
    bool notifications = true;
    centui::text<subheading> preview_name{name};
    centui::text<muted> details{""};
    centui::text<muted> status{"Ready. Changes stay in this session."};
    centui::text<muted> duration{"Focus session / 45 min"};
    centui::text<> confirmation{""};

    settings()
    {
        refresh();
    }

    void refresh()
    {
        preview_name.set_text(name.empty() ? "Untitled studio" : name);
        details.set_text(
            std::format(
                "{}\n\n{:.0f} minute focus sessions\nNotifications {}",
                projects().at(project),
                minutes,
                notifications ? "on" : "off"
            )
        );
        duration.set_text(std::format("Focus session / {:.0f} min", minutes));
    }

    void changed()
    {
        refresh();
        status.set_text("Unsaved changes");
    }

    void apply()
    {
        refresh();
        status.set_text("Applied for this session");
        confirmation.set_text(
            std::format(
                "{}\n{}\n\nFocus: {:.0f} minutes\nNotifications: {}\nNotes: {} characters",
                name.empty() ? "Untitled studio" : name,
                projects().at(project),
                minutes,
                notifications ? "on" : "off",
                notes.size()
            )
        );
    }
};

} // namespace studio
