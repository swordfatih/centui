#pragma once

#include <centypo/layout.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace centypo::details
{

/// Copies native rows and converts supported pixel modes to straight alpha.
glyph_bitmap copy_glyph_bitmap(const FT_Bitmap& bitmap, int left, int top);

} // namespace centypo::details
