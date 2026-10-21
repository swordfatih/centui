#pragma once

#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>

// An optional override; ordinary tests continue to use system font defaults.
inline std::filesystem::path test_font_path()
{
    const auto* path = std::getenv("CENTUI_TEST_FONT");
    if(path && *path)
    {
        return path;
    }
#ifdef _WIN32
    const auto* windows = std::getenv("WINDIR");
    return (windows ? std::filesystem::path{windows} : std::filesystem::path{"C:\\Windows"}) / "Fonts" / "segoeui.ttf";
#elif defined(__APPLE__)
    for(const auto* candidate: {"/System/Library/Fonts/SFNS.ttf", "/System/Library/Fonts/Helvetica.ttc"})
#else
    for(const auto* candidate: {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                               "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
                               "/usr/share/fonts/TTF/DejaVuSans.ttf"})
#endif
#ifndef _WIN32
    {
        if(std::filesystem::exists(candidate))
        {
            return candidate;
        }
    }
    throw std::runtime_error("No system test font found; optionally set CENTUI_TEST_FONT");
#endif
}
