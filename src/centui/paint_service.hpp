#pragma once

#include <centui/layout.hpp>
#include <centui/paint.hpp>
#include <centui/render.hpp>

namespace centui::details
{

struct painted_image
{
    rect bounds; // Padded bitmap bounds in local logical units.
    rect clip;   // Non-overlapping tile interior.
    std::shared_ptr<const bitmap> image;
};

// Document-owned vector preparation. Positions and tint never enter mask cache keys.
class paint_service
{
public:
    paint_service();
    ~paint_service();

    std::vector<painted_image> prepare(const path& shape, const brush& paint, float stroke, float scale, rect visible);
    std::vector<painted_image> box(size extent, float radius, computed_edges border, float scale, rect visible);

    void set_cache_budget(std::size_t bytes);

private:
    struct data;
    std::unique_ptr<data> m_data;
};

} // namespace centui::details
