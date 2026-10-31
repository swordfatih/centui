#pragma once

#include "text_engine.hpp"

namespace centypo::details
{

void place_text(layout_data& layout, float width, text_alignment alignment);

[[nodiscard]] layout_data layout_paragraphs(
    text_engine& engine,
    std::shared_ptr<font_collection> collection,
    const font& request,
    std::string_view value,
    float size,
    float width,
    const text_options& options
);

} // namespace centypo::details
