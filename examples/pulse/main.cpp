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
    edges padding = edges::all(px(20));
    length gap = px(16);
    states_color background_color = solid(0x141820);
    centui::edges border_width = centui::edges::all(centui::px(1));
    states_color border_color = solid(0x262C37);
};

struct stat
{
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

// A small RGBA bitmap is a portable way of drawing charts with CentUI's image widget.
inline void pixel(bitmap& image, int x, int y, std::uint32_t rgb, unsigned alpha = 255)
{
    if(x < 0 || y < 0 || x >= image.width || y >= image.height)
    {
        return;
    }

    const auto offset = static_cast<std::size_t>((y * image.width + x) * 4);
    image.pixels[offset] = static_cast<std::uint8_t>((rgb >> 16) & 255);
    image.pixels[offset + 1] = static_cast<std::uint8_t>((rgb >> 8) & 255);
    image.pixels[offset + 2] = static_cast<std::uint8_t>(rgb & 255);
    image.pixels[offset + 3] = static_cast<std::uint8_t>(alpha);
}

inline void line(bitmap& image, int x0, int y0, int x1, int y1, std::uint32_t rgb, int width = 2)
{
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx - dy;

    while(true)
    {
        for(int oy = -width / 2; oy <= width / 2; ++oy)
        {
            for(int ox = -width / 2; ox <= width / 2; ++ox)
            {
                pixel(image, x0 + ox, y0 + oy, rgb);
            }
        }

        if(x0 == x1 && y0 == y1)
        {
            break;
        }

        const int twice_error = error * 2;
        if(twice_error > -dy)
        {
            error -= dy;
            x0 += sx;
        }
        if(twice_error < dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}

inline handle<image_widget> chart()
{
    bitmap image{900, 260, std::vector<std::uint8_t>(900 * 260 * 4, 0)};

    for(int y = 36; y < 240; y += 49)
    {
        for(int x = 8; x < 890; x += 8)
        {
            pixel(image, x, y, 0x303644);
        }
    }

    const std::vector<int> values = {174, 164, 180, 152, 159, 141, 150, 114, 128, 112, 118, 93, 105,
                                     88,  103, 78,  90,  62,  73,  55,  64,  44,  56,  39,  47, 27};

    for(int index = 0; index < 25; ++index)
    {
        const int x0 = 16 + index * 34;
        const int x1 = 16 + (index + 1) * 34;
        const int y0 = values[index];
        const int y1 = values[index + 1];

        line(image, x0, y0 + 34, x1, y1 + 34, 0x9A83FA, 3);
        line(image, x0, y0 + 55, x1, y1 + 55, 0x325E73, 2);
    }

    for(int y = 0; y < 260; ++y)
    {
        for(int x = 0; x < 900; ++x)
        {
            const auto offset = static_cast<std::size_t>((y * 900 + x) * 4);
            if(image.pixels[offset + 3] == 0)
            {
                image.pixels[offset] = 0x14;
                image.pixels[offset + 1] = 0x18;
                image.pixels[offset + 2] = 0x20;
                image.pixels[offset + 3] = 255;
            }
        }
    }

    style image_style;
    image_style.width = percent(100);
    image_style.height = px(228);
    return make_widget<image_widget>(std::move(image), image_style);
}

inline auto statcard(std::string label, std::string number, std::string delta, std::string note)
{
    return container<stat>{
        text<muted>{std::move(label)},
        text<metric>{std::move(number)},
        container<row>{text<green>{std::move(delta)}, text<faint>{std::move(note)}}
    };
}

inline auto activity(std::string dot, std::string titletext, std::string subtitle, std::string time)
{
    return container<stack>{
        container<spread>{text<white>{std::move(dot) + "  " + std::move(titletext)}, text<faint>{std::move(time)}},
        text<muted>{std::move(subtitle)}
    };
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
    auto search = std::make_shared<std::string>();

    auto root = centui::container<screen>{
        centui::container<shell>{
            centui::container<side>{
                centui::container<row>{
                    centui::text<purple>{"◆"},
                    centui::text<logo>{"pulse."}
                },
                centui::text<faint>{"WORKSPACE"},
                centui::button<nav_active>{
                    "◈   Overview",
                    {
                        .on_click = [selected, toast]() mutable
                        {
                            *selected = "Overview";
                            toast.set_text("Viewing Overview");
                        }
                    }
                },
                centui::button<nav>{
                    "▥   Analytics",
                    {
                        .on_click = [selected, toast]() mutable
                        {
                            *selected = "Analytics";
                            toast.set_text("Analytics selected");
                        }
                    }
                },
                centui::button<nav>{
                    "◇   Audience",
                    {
                        .on_click = [selected, toast]() mutable
                        {
                            *selected = "Audience";
                            toast.set_text("Audience selected");
                        }
                    }
                },
                centui::button<nav>{
                    "▤   Campaigns",
                    {
                        .on_click = [selected, toast]() mutable
                        {
                            *selected = "Campaigns";
                            toast.set_text("Campaigns selected");
                        }
                    }
                },
                centui::button<nav>{
                    "◷   Activity",
                    {
                        .on_click = [selected, toast]() mutable
                        {
                            *selected = "Activity";
                            toast.set_text("Activity selected");
                        }
                    }
                },
                centui::container<spacer>{},
                centui::text<faint>{"PREFERENCES"},
                centui::button<nav>{
                    "⚙   Settings",
                    {
                        .on_click = [toast]() mutable
                        {
                            toast.set_text("Settings selected");
                        }
                    }
                },
                centui::button<nav>{
                    "?   Help & support",
                    {
                        .on_click = [toast]() mutable
                        {
                            toast.set_text("Support: hello@pulse.example");
                        }
                    }
                },
                centui::container<spacer>{},
                centui::container<separator>{},
                centui::container<stack>{
                    centui::text<white>{"●  Acme Studio"},
                    centui::text<faint>{"Pro workspace  ·  Active"}
                }
            },
            centui::scroll<dashboard_viewport>{
                centui::container<body>{
                    centui::container<spread>{
                        centui::container<stack>{
                            centui::text<eyebrow>{"WORKSPACE / OVERVIEW"},
                            centui::text<heading>{"Good morning, Fatih  ✦"},
                            centui::text<muted>{"Here's what's happening with your business today."}
                        },
                        centui::container<row>{
                            centui::button<action>{
                                "↻  Refresh",
                                {
                                    .on_click = [toast]() mutable
                                    {
                                        toast.set_text("Dashboard refreshed successfully");
                                    }
                                }
                            },
                            centui::button<primary>{
                                "↗  Export report",
                                {
                                    .on_click = [toast]() mutable
                                    {
                                        toast.set_text("Report export is a demo action");
                                    }
                                }
                            }
                        }
                    },
                    centui::container<spread>{
                        centui::text<title>{"Performance overview"},
                        centui::button<action>{
                            "◷  Last 30 days   ▾",
                            {
                                .on_click = [period]() mutable
                                {
                                    period.set_text("Last 7 days (demo)");
                                }
                            }
                        }
                    },
                    centui::container<stats>{
                        statcard("Total revenue", "$128,430", "↗  12.8%", "vs. previous period"),
                        statcard("Active users", "24,892", "↗  8.2%", "vs. previous period"),
                        statcard("Conversion rate", "4.86%", "↗  2.4%", "vs. previous period"),
                        statcard("Total orders", "1,429", "↗  16.3%", "vs. previous period")
                    },
                    centui::container<columns>{
                        centui::container<panel, wide>{
                            centui::container<spread>{
                                centui::container<tight>{
                                    centui::text<title>{"Revenue analytics"},
                                    centui::text<muted>{"Track your earnings over time"}
                                },
                                centui::text<green>{"● +12.8% this month"}
                            },
                            centui::container<row>{
                                centui::text<heading>{"$128,430"},
                                centui::text<muted>{"USD   /   +$14,590"}
                            },
                            chart(),
                            centui::container<spread>{
                                centui::text<faint>{"SEP 25"},
                                centui::text<faint>{"OCT 01"},
                                centui::text<faint>{"OCT 08"},
                                centui::text<faint>{"OCT 15"},
                                centui::text<faint>{"OCT 24"}
                            },
                            centui::container<row>{
                                centui::text<purple>{"● Revenue"},
                                centui::text<muted>{"● Previous period"}
                            }
                        },
                        centui::container<panel, narrow>{
                            centui::container<stack>{
                                centui::text<title>{"Traffic sources"},
                                centui::text<muted>{"Where visitors come from"}
                            },
                            centui::container<stack>{
                                centui::container<spread>{
                                    centui::text<white>{"●  Direct"},
                                    centui::text<white>{"42.6%"}
                                },
                                centui::container<progress_bg>{
                                    centui::container<progress_fg>{}
                                }
                            },
                            centui::container<stack>{
                                centui::container<spread>{
                                    centui::text<white>{"●  Organic search"},
                                    centui::text<white>{"28.4%"}
                                },
                                centui::text<purple>{"━━━━━━━━━━━━━━"}
                            },
                            centui::container<stack>{
                                centui::container<spread>{
                                    centui::text<white>{"●  Social media"},
                                    centui::text<white>{"18.2%"}
                                },
                                centui::text<purple>{"━━━━━━━━━"}
                            },
                            centui::container<stack>{
                                centui::container<spread>{
                                    centui::text<white>{"●  Referral"},
                                    centui::text<white>{"10.8%"}
                                },
                                centui::text<purple>{"━━━━━━"}
                            },
                            centui::container<separator>{},
                            centui::text<green>{"↗  Traffic is up 18.6%"},
                            centui::text<faint>{"Compared to the previous 30 days"}
                        }
                    },
                    centui::container<columns>{
                        centui::container<panel, wide>{
                            centui::container<spread>{
                                centui::text<title>{"Recent transactions"},
                                centui::button<action>{
                                    "View all  →",
                                    {
                                        .on_click = [toast]() mutable
                                        {
                                            toast.set_text("Transactions: showing the latest four");
                                        }
                                    }
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
                                {
                                    .on_change = [search, toast](std::string_view value) mutable
                                    {
                                        *search = value;
                                        toast.set_text(value.empty() ? "Search cleared" : "Searching: " + std::string(value));
                                    }
                                }
                            },
                            centui::button<primary>{
                                "+  New campaign",
                                {
                                    .on_click = [toast]() mutable
                                    {
                                        toast.set_text("New campaign clicked");
                                    }
                                }
                            },
                            centui::button<action>{
                                "↗  Open analytics",
                                {
                                    .on_click = [toast]() mutable
                                    {
                                        toast.set_text("Analytics selected");
                                    }
                                }
                            },
                            centui::container<separator>{},
                            centui::text<green>{"●  Everything looks good"},
                            centui::text<faint>{"Your workspace is running smoothly."}
                        }
                    },
                    toast
                }
            }
        }
    };

    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    centui::run(window, renderer, document);
}
