#pragma once

#include <centui/style.hpp>

namespace centui
{

class widget;

namespace details
{

struct document_context;

enum class change
{
    layout,
    topology,
    placement,
    paint,
    interaction,
    raster,
    geometry
};

struct style_effects
{
    bool layout = false;
    bool paint = false;
    bool interaction = false;
    bool inherited_metrics = false;
};

[[nodiscard]] style_effects classify_style(const style& next, const style& previous) noexcept;

// The sole propagation policy. Consumers acknowledge work only after success.
struct invalidation_access
{
    static void invalidate(widget& source, change reason);
    static void invalidate_document(document_context& context, change reason);
    static void invalidate_paint_subtree(widget& source);
    static void geometry_applied(document_context& context);
};

} // namespace details
} // namespace centui
