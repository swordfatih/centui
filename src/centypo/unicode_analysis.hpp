#pragma once

#include <centypo/unicode.hpp>

namespace centypo::details
{

struct unicode_scalar
{
    std::uint32_t value;
    std::size_t begin;
    std::size_t end;
};

std::vector<unicode_scalar> decode_scalars(std::string_view text);

} // namespace centypo::details
