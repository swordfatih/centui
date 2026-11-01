#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <centypo/engine.hpp>

namespace centypo::details
{

struct font_source
{
    std::filesystem::path path;
    long index = 0;
    std::string family;
    int weight = 400;
    font_slant slant = font_slant::normal;
};

[[nodiscard]] std::shared_ptr<const std::vector<unsigned char>> read_font(const std::filesystem::path& path);

} // namespace centypo::details
