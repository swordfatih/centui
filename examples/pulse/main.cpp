// PULSE / CentUI — dark analytics dashboard
// Drop into centui/examples/pulse/main.cpp; see README.md.
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <centui/backends/bgfx.hpp>
#include <centui/centui.hpp>
#include <centui/platforms/sdl.hpp>

namespace ui
{

using namespace centui;

inline states_color solid(unsigned rgb)
{
    return states_color::all(color::from_hex(rgb));
}

struct screen
{
    length width = percent(100), height = percent(100);
    states_color background_color = solid(0x0B0D12);
};

struct shell
{
    direction flex_direction = direction::row;
    length width = percent(100), height = percent(100);
};

struct side
{
    centui::flex flex = centui::flex::all(0, 0, px(248));
    length height = percent(100);
    edges padding = edges::all(px(24));
    length gap = px(22);
    states_color background_color = solid(0x101219);
    centui::edges border_width = centui::edges::all(centui::px(1));
    states_color border_color = solid(0x252932);
};

struct dashboard_viewport
{
    centui::flex flex = centui::flex::all(1, 1, px(0));
    length height = percent(100);
};

struct body
{
    length width = percent(100);
    edges padding = edges::all(px(30));
    length gap = px(24);
};

struct stack
{
    length gap = px(9);
};

struct tight
{
    length gap = px(5);
};

struct row
{
    direction flex_direction = direction::row;
    alignment align_items = alignment::center;
    length gap = px(14);
};

struct spread
{
    direction flex_direction = direction::row;
    justification justify_content = justification::space_between;
    alignment align_items = alignment::center;
    length gap = px(12);
};

struct stats
{
    direction flex_direction = direction::row;
    length gap = px(14);
};

struct columns
{
    direction flex_direction = direction::row;
    length gap = px(16);
};

struct panel
{
    float corner_radius = 14;
    edges padding = edges::all(px(20));
    length gap = px(16);
    states_color background_color = solid(0x141820);
    centui::edges border_width = centui::edges::all(centui::px(1));
    states_color border_color = solid(0x262C37);
};

struct stat
{
    float corner_radius = 14;
    centui::flex flex = centui::flex::all(1, 1, px(0));
    edges padding = edges::all(px(20));
    length gap = px(11);
    states_color background_color = solid(0x141820);
    centui::edges border_width = centui::edges::all(centui::px(1));
    states_color border_color = solid(0x262C37);
};

struct wide
{
    centui::flex flex = centui::flex::all(2, 1, px(0));
};

struct narrow
{
    centui::flex flex = centui::flex::all(1, 1, px(0));
};

struct fill
{
    length width = percent(100);
};

struct separator
{
    length height = px(1);
    states_color background_color = solid(0x2A303C);
};

struct spacer
{
    length height = px(10);
};

struct logo
{
    float font_size = 25;
    states_color text_color = solid(0xF3F5FA);
};

struct eyebrow
{
    float font_size = 11;
    states_color text_color = solid(0x9C86FF);
};

struct heading
{
    float font_size = 31;
    states_color text_color = solid(0xF6F6FB);
};

struct title
{
    float font_size = 18;
    states_color text_color = solid(0xF1F2F7);
};

struct metric
{
    float font_size = 31;
    states_color text_color = solid(0xFFFFFF);
};

struct white
{
    float font_size = 14;
    states_color text_color = solid(0xE5E7EE);
};

struct muted
{
    float font_size = 13;
    states_color text_color = solid(0x9299AB);
};

struct faint
{
    float font_size = 11;
    states_color text_color = solid(0x737D91);
};

struct green
{
    float font_size = 12;
    states_color text_color = solid(0x62D9B0);
};

struct purple
{
    float font_size = 13;
    states_color text_color = solid(0xAE9DFF);
};

struct nav
{
    length height = px(46);
    edges padding = {px(0), px(11), px(0), px(11)};
    justification justify_content = justification::center;
    alignment align_items = alignment::start;
    text_alignment text_align = text_alignment::left;
    float font_size = 14;
    states_color text_color = solid(0xA7AEC0);

    struct
    {
        color normal = color::from_hex(0x101219), hover = color::from_hex(0x202331),
              pressed = color::from_hex(0x282342), focused = color::from_hex(0x282342);
    } background_color;
};

struct nav_active
{
    length height = px(46);
    edges padding = {px(0), px(11), px(0), px(11)};
    justification justify_content = justification::center;
    alignment align_items = alignment::start;
    text_alignment text_align = text_alignment::left;
    float font_size = 14;
    states_color text_color = solid(0xE7DFFF);
    states_color background_color = solid(0x29233F);
};

struct action
{
    float corner_radius = 8;
    length height = px(46);
    edges padding = {px(0), px(12), px(0), px(12)};
    justification justify_content = justification::center;
    alignment align_items = alignment::center;
    float font_size = 14;
    text_alignment text_align = text_alignment::center;
    states_color text_color = solid(0xECECF5);

    struct
    {
        color normal = color::from_hex(0x242934), hover = color::from_hex(0x33394A),
              pressed = color::from_hex(0x202532), focused = color::from_hex(0x33394A);
    } background_color;
};

struct primary
{
    float corner_radius = 8;
    length height = px(46);
    edges padding = {px(0), px(14), px(0), px(14)};
    justification justify_content = justification::center;
    alignment align_items = alignment::center;
    float font_size = 14;
    text_alignment text_align = text_alignment::center;
    states_color text_color = solid(0xFFFFFF);

    struct
    {
        color normal = color::from_hex(0x7862DD), hover = color::from_hex(0x927EF1),
              pressed = color::from_hex(0x6652C6), focused = color::from_hex(0x927EF1);
    } background_color;
};

struct inputstyle
{
    float corner_radius = 6;
    length height = px(38);
    edges padding = edges::all(px(10));
    states_color background_color = solid(0x1D222D);
    centui::edges border_width = centui::edges::all(centui::px(1));
    states_color border_color = solid(0x353C49);
};

struct chartstyle
{
    length width = percent(100);
    length height = px(228);
    states_color background_color = solid(0x141820);
    states_color border_color = solid(0x303644);
};

struct progress_bg
{
    length height = px(7);
    states_color background_color = solid(0x2B3040);
};

struct progress_fg
{
    length height = px(7);
    states_color background_color = solid(0x9B8AF9);
};

class chart_widget : public widget
{
public:
    explicit chart_widget(centui::style_source appearance)
        : widget(std::move(appearance))
    {
    }

protected:
    size measure(const measure_context&) override
    {
        return {900, 260};
    }

    void on_layout() override
    {
        const auto bounds = content_bounds();
        const auto scale = std::min(bounds.width / 900, bounds.height / 260);
        m_offset = {(bounds.width - 900 * scale) / 2, (bounds.height - 260 * scale) / 2};
        if(scale == m_scale)
        {
            return;
        }
        m_scale = scale;
        m_grid = {};
        m_primary = {};
        m_secondary = {};
        m_area = {};
        m_area.move_to({16 * scale, 260 * scale});
        for(int y = 36; y < 240; y += 49)
        {
            for(int x = 8; x < 890; x += 8)
            {
                m_grid.rectangle({x * scale, y * scale, scale, scale});
            }
        }
        constexpr int values[] = {174, 164, 180, 152, 159, 141, 150, 114, 128, 112, 118, 93, 105,
                                  88,  103, 78,  90,  62,  73,  55,  64,  44,  56,  39,  47, 27};
        for(int index = 0; index < 26; ++index)
        {
            const vec2 primary{(16 + index * 34) * scale, (values[index] + 34) * scale};
            const vec2 secondary{primary.x, (values[index] + 55) * scale};
            m_area.line_to(primary);
            if(index == 0)
            {
                m_primary.move_to(primary);
                m_secondary.move_to(secondary);
            }
            else
            {
                m_primary.line_to(primary);
                m_secondary.line_to(secondary);
            }
        }
        m_area.line_to({866 * scale, 260 * scale}).close();
        m_area_brush =
            linear_gradient{{0, 40 * scale}, {0, 260 * scale}, {{0, {154, 131, 250, 72}}, {1, {154, 131, 250, 0}}}};
    }

    void paint(painter& output) const override
    {
        if(m_scale <= 0)
        {
            return;
        }
        const auto bounds = output.bounds();
        output.rectangle(
            {bounds.x + m_offset.x, bounds.y + m_offset.y, 900 * m_scale, 260 * m_scale},
            output.resolve(style().background_color)
        );
        output.fill(m_grid, output.resolve(style().border_color), m_offset);
        output.fill(m_area, m_area_brush, m_offset);
        output.stroke(m_primary, color::from_hex(0x9A83FA), 3 * m_scale, m_offset);
        output.stroke(m_secondary, color::from_hex(0x325E73), 3 * m_scale, m_offset);
    }

private:
    path m_grid;
    path m_primary;
    path m_secondary;
    path m_area;
    brush m_area_brush = color{};
    vec2 m_offset{};
    float m_scale = -1;
};

inline handle<chart_widget> chart()
{
    return make_widget<chart_widget>(style_source::composed<chartstyle>());
}

inline auto statcard(std::string label, std::string number, std::string delta, std::string note)
{
    return container<stat>{
        text<muted>{std::move(label)},
        text<metric>{std::move(number)},
        container<row>{text<green>{std::move(delta)}, text<faint>{std::move(note)}}
    };
}

inline auto traffic_bar(float percentage)
{
    auto fill = container<progress_fg>{};
    fill->patch_style({.width = percent(percentage), .corner_radius = 3.5f});
    auto track = container<progress_bg>{fill};
    track->patch_style({.corner_radius = 3.5f});
    return track;
}

inline auto activity(std::string dot, std::string titletext, std::string subtitle, std::string time)
{
    return container<stack>{
        container<spread>{text<white>{std::move(dot) + "  " + std::move(titletext)}, text<faint>{std::move(time)}},
        text<muted>{std::move(subtitle)}
    };
}

// Dark colors live in the style types; light mode overrides those definitions.
template <typename... Styles>
void theme_style(document& document, bool dark, const style_patch& light)
{
    if(dark)
    {
        (document.clear_style_patch<Styles>(), ...);
    }
    else
    {
        (document.patch_style<Styles>(light), ...);
    }
}

void apply_theme(document& document, bool dark)
{
    document.set_style({.text_color = solid(dark ? 0xE5E7EE : 0x202638)});
    theme_style<screen>(document, dark, {.background_color = solid(0xF3F5FA)});
    theme_style<side>(document, dark, {.background_color = solid(0xFFFFFF), .border_color = solid(0xDDE2EC)});
    theme_style<panel, stat>(document, dark, {.background_color = solid(0xFFFFFF), .border_color = solid(0xDDE2EC)});
    theme_style<logo, heading, title, metric, white>(document, dark, {.text_color = solid(0x202638)});
    theme_style<muted>(document, dark, {.text_color = solid(0x59657A)});
    theme_style<faint>(document, dark, {.text_color = solid(0x67738A)});
    theme_style<green>(document, dark, {.text_color = solid(0x147D5B)});
    theme_style<purple, eyebrow>(document, dark, {.text_color = solid(0x6950C8)});
    auto neutral = solid(0xFFFFFF);
    neutral.hover = color::from_hex(0xE8ECF4);
    neutral.pressed = color::from_hex(0xDDE2EC);
    neutral.focused = neutral.hover;
    theme_style<nav, action>(document, dark, {.background_color = neutral, .text_color = solid(0x344158)});
    theme_style<nav_active>(document, dark, {.background_color = solid(0xE9E2FC), .text_color = solid(0x593DB1)});
    theme_style<inputstyle>(document, dark, {.background_color = solid(0xF3F5FA), .border_color = solid(0xCCD4E2)});
    theme_style<separator, progress_bg>(document, dark, {.background_color = solid(0xDDE2EC)});
    theme_style<chartstyle>(document, dark, {.background_color = solid(0xFFFFFF), .border_color = solid(0xCCD4E2)});
}

} // namespace ui

int main()
{
    using namespace ui;

    centui::sdl::platform window{{.title = "PULSE — CentUI Analytics", .width = 1440, .height = 920}};
    centui::bgfx::backend renderer{window};

    auto selected = std::make_shared<std::string>("Overview");
    auto toast = centui::text<muted>{"All systems operational  /  Last synced just now"};
    auto period = centui::text<purple>{"Last 30 days"};
    auto short_period = std::make_shared<bool>(false);
    auto search = std::make_shared<std::string>();

    auto sidebar = centui::container<side>{
        centui::container<row>{centui::text<purple>{"◆"}, centui::text<logo>{"pulse."}},
        centui::text<faint>{"WORKSPACE"},
        centui::button<nav_active>{
            "◈   Overview",
            {.on_click =
                 [selected, toast]() mutable
             {
                 *selected = "Overview";
                 toast.set_text("Viewing Overview");
             }}
        },
        centui::button<nav>{
            "▥   Analytics",
            {.on_click =
                 [selected, toast]() mutable
             {
                 *selected = "Analytics";
                 toast.set_text("Analytics selected");
             }}
        },
        centui::button<nav>{
            "◇   Audience",
            {.on_click =
                 [selected, toast]() mutable
             {
                 *selected = "Audience";
                 toast.set_text("Audience selected");
             }}
        },
        centui::button<nav>{
            "▤   Campaigns",
            {.on_click =
                 [selected, toast]() mutable
             {
                 *selected = "Campaigns";
                 toast.set_text("Campaigns selected");
             }}
        },
        centui::button<nav>{
            "◷   Activity",
            {.on_click =
                 [selected, toast]() mutable
             {
                 *selected = "Activity";
                 toast.set_text("Activity selected");
             }}
        },
        centui::container<spacer>{},
        centui::text<faint>{"PREFERENCES"},
        centui::toggle<nav>{
            "Dark mode",
            {
                .checked = true,
                .on_change =
                    [](centui::event& event, bool dark)
                {
                    apply_theme(event.document(), dark);
                },
            }
        },
        centui::button<nav>{
            "⚙   Settings",
            {.on_click =
                 [toast]() mutable
             {
                 toast.set_text("Settings selected");
             }}
        },
        centui::button<nav>{
            "?   Help & support",
            {.on_click =
                 [toast]() mutable
             {
                 toast.set_text("Support: hello@pulse.example");
             }}
        },
        centui::container<spacer>{},
        centui::container<separator>{},
        centui::container<stack>{centui::text<white>{"●  Acme Studio"}, centui::text<faint>{"Pro workspace  ·  Active"}}
    };

    auto header = centui::container<spread>{
        centui::container<stack>{
            centui::text<eyebrow>{"WORKSPACE / OVERVIEW"},
            centui::text<heading>{"Good morning, Fatih  ✦"},
            centui::text<muted>{"Here's what's happening with your business today."}
        },
        centui::container<row>{
            centui::button<action>{
                "↻  Refresh",
                {.on_click =
                     [toast]() mutable
                 {
                     toast.set_text("Dashboard refreshed successfully");
                 }}
            },
            centui::button<primary>{
                "↗  Export report",
                {.on_click = [toast]() mutable
                 {
                     toast.set_text("Report export is a demo action");
                 }}
            }
        }
    };

    auto overview = centui::container<spread>{
        centui::text<title>{"Performance overview"},
        centui::button<action>{
            "◷  Last 30 days   ▾",
            {.on_click = [period, short_period]() mutable
             {
                 *short_period = !*short_period;
                 period.set_text(*short_period ? "Last 7 days" : "Last 30 days");
             }}
        }
    };

    auto metrics = centui::container<stats>{
        statcard("Total revenue", "$128,430", "↗  12.8%", "vs. previous period"),
        statcard("Active users", "24,892", "↗  8.2%", "vs. previous period"),
        statcard("Conversion rate", "4.86%", "↗  2.4%", "vs. previous period"),
        statcard("Total orders", "1,429", "↗  16.3%", "vs. previous period")
    };

    auto analytics = centui::container<columns>{
        centui::container<panel, wide>{
            centui::container<spread>{
                centui::container<tight>{
                    centui::text<title>{"Revenue analytics"},
                    centui::text<muted>{"Track your earnings over time"}
                },
                centui::text<green>{"● +12.8% this month"}
            },
            centui::container<row>{centui::text<heading>{"$128,430"}, centui::text<muted>{"USD   /   +$14,590"}},
            chart(),
            centui::container<spread>{
                centui::text<faint>{"SEP 25"},
                centui::text<faint>{"OCT 01"},
                centui::text<faint>{"OCT 08"},
                centui::text<faint>{"OCT 15"},
                centui::text<faint>{"OCT 24"}
            },
            centui::container<row>{centui::text<purple>{"● Revenue"}, centui::text<muted>{"● Previous period"}}
        },
        centui::container<panel, narrow>{
            centui::container<stack>{
                centui::text<title>{"Traffic sources"},
                centui::text<muted>{"Where visitors come from"}
            },
            centui::container<stack>{
                centui::container<spread>{centui::text<white>{"●  Direct"}, centui::text<white>{"42.6%"}},
                traffic_bar(42.6f)
            },
            centui::container<stack>{
                centui::container<spread>{centui::text<white>{"●  Organic search"}, centui::text<white>{"28.4%"}},
                traffic_bar(28.4f)
            },
            centui::container<stack>{
                centui::container<spread>{centui::text<white>{"●  Social media"}, centui::text<white>{"18.2%"}},
                traffic_bar(18.2f)
            },
            centui::container<stack>{
                centui::container<spread>{centui::text<white>{"●  Referral"}, centui::text<white>{"10.8%"}},
                traffic_bar(10.8f)
            },
            centui::container<separator>{},
            centui::text<green>{"↗  Traffic is up 18.6%"},
            centui::text<faint>{"Compared to the previous 30 days"}
        }
    };

    auto transactions = centui::container<columns>{
        centui::container<panel, wide>{
            centui::container<spread>{
                centui::text<title>{"Recent transactions"},
                centui::button<action>{
                    "View all  →",
                    {.on_click =
                         [toast]() mutable
                     {
                         toast.set_text("Transactions: showing the latest four");
                     }}
                }
            },
            centui::container<spread>{
                centui::text<faint>{"CUSTOMER / REFERENCE"},
                centui::text<faint>{"AMOUNT / STATUS"}
            },
            centui::container<separator>{},
            activity("◉", "Olivia Martin", "Order #PL-2048  ·  Premium plan", "+$2,499   ✓ Paid"),
            centui::container<separator>{},
            activity("◉", "Jackson Lee", "Order #PL-2047  ·  Team plan", "+$1,290   ✓ Paid"),
            centui::container<separator>{},
            activity("◉", "Sophia Brown", "Order #PL-2046  ·  Starter plan", "+$399   ✓ Paid")
        },
        centui::container<panel, narrow>{
            centui::text<title>{"Quick actions"},
            centui::text<muted>{"Manage your workspace"},
            centui::input<inputstyle>{
                "Search workspace",
                {.on_change =
                     [search, toast](std::string_view value) mutable
                 {
                     *search = value;
                     toast.set_text(value.empty() ? "Search cleared" : "Searching: " + std::string(value));
                 }}
            },
            centui::button<primary>{
                "+  New campaign",
                {.on_click =
                     [toast]() mutable
                 {
                     toast.set_text("New campaign clicked");
                 }}
            },
            centui::button<action>{
                "↗  Open analytics",
                {.on_click =
                     [toast]() mutable
                 {
                     toast.set_text("Analytics selected");
                 }}
            },
            centui::container<separator>{},
            centui::text<green>{"●  Everything looks good"},
            centui::text<faint>{"Your workspace is running smoothly."}
        }
    };

    auto root = centui::container<screen>{centui::container<shell>{
        sidebar,
        centui::scroll<dashboard_viewport>{
            centui::container<body>{header, overview, period, metrics, analytics, transactions, toast}
        }
    }};

    centui::document document{root};
    apply_theme(document, true);
    document.set_font(centui::font{CENTUI_FONT_PATH});
    centui::ui ui{window, renderer};

    while(window.poll_events())
    {
        ui.frame(document);
    }
}
