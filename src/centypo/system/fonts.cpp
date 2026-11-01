#include "fonts.hpp"

namespace centypo::details::system
{

std::shared_ptr<const font_provider> make_font_provider()
{
    return std::make_shared<font_provider>();
}

} // namespace centypo::details::system
