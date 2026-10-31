#pragma once

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <centui/widget.hpp>

namespace centui
{

/// \brief Measures and draws text with cached glyph layouts.
class text_widget : public widget
{
public:
    explicit text_widget(std::string value, centui::style_source appearance = {}, bool inherit_text_style = false);

    void set_text(std::string value);

    [[nodiscard]] std::string_view value() const noexcept;

protected:
    size measure(const measure_context& context) override;

    baseline_function baseline() override;

    void on_geometry() override;

    void paint(painter& output) const override;

private:
    centui::style text_style() const;

    centypo::layout prepared(const centypo::engine& engine, float width) const;

    std::string m_value;
    bool m_inherit_text_style = false;
    centypo::layout m_layout;
};

/// \brief A text label handle with compile-time style composition.
template <typename... Styles>
class text : public handle<text_widget>
{
public:
    explicit text(std::string value, centui::style_source appearance = {})
        : handle<text_widget>(make_widget<text_widget>(
              std::move(value),
              centui::style_source::with_composition<Styles...>(std::move(appearance))
          ))
    {
    }

    void set_text(std::string value)
    {
        this->get().set_text(std::move(value));
    }

    void value(std::string value)
    {
        set_text(std::move(value));
    }

    [[nodiscard]] std::string_view value() const
    {
        return this->get().value();
    }
};

} // namespace centui
