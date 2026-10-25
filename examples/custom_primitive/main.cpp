#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <centui/backends/bgfx.hpp>
#include <centui/centui.hpp>
#include <centui/platforms/sdl.hpp>

class meter final : public centui::widget
{
public:
    explicit meter(float value = 0)
    {
        centui::style appearance;
        appearance.height = centui::px(24);
        set_style(appearance);
        set_value(value);
        m_value = m_target;
    }

    void set_value(float value)
    {
        check_mutation();
        if(!std::isfinite(value))
        {
            throw std::invalid_argument("Meter values must be finite");
        }
        value = std::clamp(value, 0.0f, 1.0f);

        if(value != m_target)
        {
            request_frame();
            m_target = value;
        }
    }

protected:
    centui::size measure(const centui::measure_context&) override
    {
        return {120, 24};
    }

    void on_layout() override
    {
        const auto bounds = content_bounds();
        m_extent = {bounds.width, bounds.height};
        m_track = {};
        m_track.rounded_rectangle({0, 0, m_extent.width, m_extent.height}, m_extent.height / 2);
        m_brush = centui::linear_gradient{
            {0, 0},
            {m_extent.width, 0},
            {{0, centui::color::from_hex(0x38BDF8)}, {1, centui::color::from_hex(0x5EEAD4)}}
        };
        rebuild();
    }

    void on_frame(centui::frame_duration elapsed) override
    {
        m_value += (m_target - m_value) * (1 - std::exp(-10 * elapsed.count()));
        if(std::abs(m_target - m_value) < 0.001f)
        {
            m_value = m_target;
        }
        else
        {
            request_frame();
        }
        rebuild();
        invalidate_paint();
    }

    void paint(centui::painter& output) const override
    {
        output.fill(m_track, centui::color::from_hex(0x334155));
        if(m_value > 0)
        {
            output.fill(m_progress, m_brush);
        }
    }

private:
    void rebuild()
    {
        m_progress = {};
        m_progress.rounded_rectangle({0, 0, m_extent.width * m_value, m_extent.height}, m_extent.height / 2);
    }

    float m_value = 0;
    float m_target = 0;
    centui::size m_extent{};
    centui::path m_track;
    centui::path m_progress;
    centui::brush m_brush = centui::color::from_hex(0x38BDF8);
};

int main()
{
    auto progress = centui::make_widget<meter>(0.25f);
    centui::sdl::platform window{{.title = "CentUI / Custom vector widget", .width = 640, .height = 420}};
    centui::bgfx::backend renderer{window};
    centui::style card;
    card.width = centui::percent(85);
    card.max_width = centui::px(460);
    card.padding = centui::edges::all(centui::px(28));
    card.gap = centui::px(20);
    card.corner_radius = 18;
    card.background_color = centui::states_color::all(centui::color::from_hex(0x1E293B));
    auto slider = centui::range{centui::range_options{
        .minimum = 0,
        .maximum = 100,
        .step = 1,
        .on_change = [progress](float value)
        {
            progress->set_value(value / 100);
        }
    }};
    slider->set_value(25);
    auto content = centui::container{
        centui::text{"Custom vector widget"},
        centui::text{"Move the slider to animate the gradient. Resize to rebuild the paths."},
        progress,
        slider
    };
    content.set_style(card);
    auto root = centui::container{content};
    centui::style screen;
    screen.width = centui::percent(100);
    screen.height = centui::percent(100);
    screen.align_items = centui::alignment::center;
    screen.justify_content = centui::justification::center;
    screen.background_color = centui::states_color::all(centui::color::from_hex(0x0F172A));
    root.set_style(screen);
    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    centui::ui ui{window, renderer};

    while(window.poll_events())
    {
        ui.frame(document);
    }
}
