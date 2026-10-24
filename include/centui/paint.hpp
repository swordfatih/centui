#pragma once

#include <memory>
#include <variant>
#include <vector>

#include <centui/types.hpp>

namespace centui
{
namespace details
{

class paint_service;
struct painted_image;

} // namespace details

/// \brief A logical vector path. Copies share storage until edited.
class path
{
public:
    path();

    path& move_to(vec2 point);
    path& line_to(vec2 point);
    path& curve_to(vec2 control1, vec2 control2, vec2 end);
    path& close();
    path& rectangle(rect bounds);
    path& rounded_rectangle(rect bounds, float radius);
    path& circle(vec2 center, float radius);

private:
    friend class details::paint_service;
    struct data;
    data& writable();
    std::shared_ptr<data> m_data;
};

struct gradient_stop
{
    float offset;
    centui::color color;
    bool operator==(const gradient_stop&) const = default;
};

/// \brief Gradient coordinates use the same local logical units as the path.
struct linear_gradient
{
    vec2 start;
    vec2 end;
    std::vector<gradient_stop> stops;
    bool operator==(const linear_gradient&) const = default;
};

struct radial_gradient
{
    vec2 center;
    float radius;
    std::vector<gradient_stop> stops;
    bool operator==(const radial_gradient&) const = default;
};

using brush = std::variant<color, linear_gradient, radial_gradient>;

} // namespace centui
