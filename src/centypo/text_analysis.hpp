#pragma once

#include "layout_data.hpp"

#include <centypo/engine.hpp>

namespace centypo::details
{

/// Owns source bytes and resolved paragraph/script state across width reflow.
class text_analysis : public text_boundaries
{
public:
    struct script_run
    {
        std::size_t begin;
        std::size_t end;
        std::uint32_t script;
    };

    struct bidi_run
    {
        std::size_t offset;
        std::size_t length;
        unsigned level;
    };

    text_analysis(std::string_view text, std::string_view language, text_direction direction);
    ~text_analysis();

    const std::vector<script_run>& scripts() const noexcept;
    std::vector<bidi_run> line_runs(std::size_t begin, std::size_t end) const;
    bool rtl(std::size_t offset) const;

    std::size_t storage_bytes() const;

private:
    struct data;

    std::unique_ptr<data> m_data;
};

} // namespace centypo::details
