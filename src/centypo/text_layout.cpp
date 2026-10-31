#include "text_layout.hpp"

#include "unicode_analysis.hpp"

#include <iterator>
#include <span>

namespace centypo
{

void text_options::validate() const
{
    if(wrap < text_wrap::word || wrap > text_wrap::none || direction < text_direction::automatic ||
       direction > text_direction::rtl || !std::isfinite(line_spacing) || line_spacing < 0)
    {
        throw std::invalid_argument("Invalid paragraph options");
    }

    validate_text(language);

    float previous = 0;
    for(auto stop: tab_stops)
    {
        if(!std::isfinite(stop) || stop <= previous)
        {
            throw std::invalid_argument("Tab stops must be finite, positive and increasing");
        }
        previous = stop;
    }
}

namespace details
{
namespace
{

struct visual_piece
{
    std::size_t begin;
    std::size_t end;
    bool rtl;
    std::uint32_t script;
};

// Hard separators are removed from shaping, never from retained source offsets.
std::size_t content_end(std::string_view value, std::size_t begin, std::size_t end)
{
    if(end > begin && value[end - 1] == '\n')
    {
        --end;
        if(end > begin && value[end - 1] == '\r')
        {
            --end;
        }
    }
    else if(end > begin && (value[end - 1] == '\r' || value[end - 1] == '\v' || value[end - 1] == '\f'))
    {
        --end;
    }
    else if(end >= begin + 2 && value.substr(end - 2, 2) == "\xc2\x85")
    {
        end -= 2;
    }
    else if(end >= begin + 3 &&
            (value.substr(end - 3, 3) == "\xe2\x80\xa8" || value.substr(end - 3, 3) == "\xe2\x80\xa9"))
    {
        end -= 3;
    }

    return end;
}

bool supported_sequence(
    const layout_data& shaped,
    std::string_view text,
    const font_face& face,
    std::size_t cluster_base = 0
)
{
    if(std::ranges::any_of(
           shaped.glyphs,
           [](const auto& glyph)
           {
               return glyph.id == 0;
           }
       ))
    {
        return false;
    }

    if(std::none_of(
           text.begin(),
           text.end(),
           [](unsigned char byte)
           {
               return byte == 0xe2 || byte == 0xef || byte == 0xf0 || byte == 0xf3;
           }
       ))
    {
        return true;
    }

    const auto boundaries = analyze_text(text);
    const auto scalars = decode_scalars(text);
    for(std::size_t g = 0; g + 1 < boundaries.graphemes.size(); ++g)
    {
        const auto begin = boundaries.graphemes[g], end = boundaries.graphemes[g + 1];
        bool joined = false, emoji = false;
        std::size_t regional = 0;

        for(std::size_t i = 0; i < scalars.size(); ++i)
        {
            if(scalars[i].begin < begin || scalars[i].begin >= end)
            {
                continue;
            }

            const auto value = scalars[i].value;
            joined |= value == 0x200d || value == 0x20e3;
            emoji |= value >= 0x1f000 || value == 0x20e3;
            regional += value >= 0x1f1e6 && value <= 0x1f1ff;

            if(((value >= 0xfe00 && value <= 0xfe0f) || (value >= 0xe0100 && value <= 0xe01ef)) && i > 0)
            {
                if(!face.variant(scalars[i - 1].value, value) && value != 0xfe0e && !(value == 0xfe0f && face.color()))
                {
                    return false;
                }
            }
        }

        if((joined && emoji) || regional == 2)
        {
            const auto visible = std::count_if(
                shaped.glyphs.begin(),
                shaped.glyphs.end(),
                [&](const auto& glyph)
                {
                    return glyph.cluster >= cluster_base + begin && glyph.cluster < cluster_base + end &&
                           (glyph.advance != 0 || glyph.ink.width != 0);
                }
            );
            if(visible != 1)
            {
                return false;
            }
        }
    }

    return true;
}

layout_data shape_fallback(
    text_engine& engine,
    const font_collection& collection,
    const std::vector<std::size_t>& candidates,
    const font& request,
    const std::shared_ptr<font_face>& primary,
    std::string_view text,
    float size,
    bool rtl,
    std::uint32_t script,
    std::string_view language
)
{
    // Try the full script run first. This preserves ligatures, cursive context
    // and conjuncts whenever one face supports the complete span.
    for(const auto index: candidates)
    {
        auto face = collection.face(index, request);
        if(!face)
        {
            continue;
        }

        const auto shaped = engine.shape(face, text, size, rtl, script, language);
        if(supported_sequence(*shaped, text, *face))
        {
            return *shaped;
        }
    }

    const auto analysis = engine.analyze(text, language);

    struct font_span
    {
        std::size_t begin;
        std::size_t end;
        std::shared_ptr<font_face> face;
    };

    std::vector<font_span> spans;
    for(std::size_t g = 0; g + 1 < analysis->graphemes.size(); ++g)
    {
        const auto begin = analysis->graphemes[g], end = analysis->graphemes[g + 1];
        std::shared_ptr<font_face> selected;
        for(const auto index: candidates)
        {
            auto face = collection.face(index, request);
            if(!face)
            {
                continue;
            }

            const auto shaped = engine.shape(face, text, size, rtl, script, language, begin, end - begin);
            if(supported_sequence(*shaped, text.substr(begin, end - begin), *face, begin))
            {
                selected = face;
                break;
            }
        }

        if(!spans.empty() && spans.back().face == selected)
        {
            spans.back().end = end;
        }
        else
        {
            spans.push_back({begin, end, std::move(selected)});
        }
    }

    if(rtl)
    {
        std::ranges::reverse(spans);
    }

    layout_data result;
    float x = 0, descent = 0;
    for(const auto& span: spans)
    {
        const auto face = span.face ? span.face : primary;
        auto append = [&](const layout_data& shaped, std::size_t cluster)
        {
            result.ascender = std::max(result.ascender, shaped.ascender);
            descent = std::max(descent, shaped.bounds.height - shaped.ascender);
            for(auto glyph: shaped.glyphs)
            {
                glyph.x += x;
                glyph.origin += x;
                if(!span.face)
                {
                    glyph.cluster = static_cast<std::uint32_t>(cluster);
                }
                result.glyphs.push_back(std::move(glyph));
            }
            x += shaped.bounds.width;
        };

        if(span.face)
        {
            append(
                *engine.shape(face, text, size, rtl, script, language, span.begin, span.end - span.begin),
                span.begin
            );
        }
        else
        {
            // Exhausted coverage has one stable .notdef cell per grapheme.
            const auto missing = engine.shape(face, "\xf4\x8f\xbf\xbf", size, rtl, script, language);
            std::vector<std::size_t> offsets;
            for(const auto at: analysis->graphemes)
            {
                if(at >= span.begin && at < span.end)
                {
                    offsets.push_back(at);
                }
            }

            if(rtl)
            {
                std::ranges::reverse(offsets);
            }
            for(const auto at: offsets)
            {
                append(*missing, at);
            }
        }
    }

    result.bounds = {0, 0, x, result.ascender + descent};

    return result;
}

} // namespace

layout_data layout_paragraphs(
    text_engine& engine,
    std::shared_ptr<font_collection> collection,
    const font& request,
    std::string_view value,
    float size,
    float width,
    const text_options& options
)
{
    options.validate();

    const auto analysis = engine.paragraphs(value, options.language, options.direction);
    const auto& boundaries = *analysis;
    const auto candidates = collection->resolve(request, value, options.language);
    std::shared_ptr<font_face> primary;
    for(const auto index: candidates)
    {
        if((primary = collection->face(index, request)))
        {
            break;
        }
    }
    if(!primary)
    {
        throw std::runtime_error("No usable font; enable system discovery or register a font with the engine");
    }
    if(std::isnan(width) || width < 0)
    {
        throw std::invalid_argument("Text width must be nonnegative");
    }

    const auto metrics = *engine.shape(primary, {}, size, false, 0, {});
    const auto tab_interval = std::max(1.0f, engine.shape(primary, " ", size, false, 0, {})->bounds.width * 8);

    layout_data result;
    result.font_resource = collection;
    result.font_size = size;
    result.ascender = metrics.ascender;
    result.line_height = metrics.bounds.height;

    const auto& scripts = analysis->scripts();
    std::vector<std::size_t> paragraph_ends = boundaries.hard_breaks;

    // Always append: a terminal hard break needs a trailing empty line.
    paragraph_ends.push_back(value.size());

    std::size_t paragraph_begin = 0;
    float y = 0;
    for(const auto paragraph_end: paragraph_ends)
    {
        const auto end = content_end(value, paragraph_begin, paragraph_end);
        const auto shape_line = [&](std::size_t begin, std::size_t finish)
        {
            layout_data line;
            if(begin == finish)
            {
                line.ascender = metrics.ascender;
                line.line_height = metrics.bounds.height;
                line.carets.push_back({begin, {0, 0}});

                return line;
            }

            float x = 0;

            const auto next_tab = [&](float at)
            {
                for(const auto stop: options.tab_stops)
                {
                    if(stop > at)
                    {
                        return stop;
                    }
                }

                const auto origin = options.tab_stops.empty() ? 0 : options.tab_stops.back();

                return origin + (std::floor((at - origin) / tab_interval) + 1) * tab_interval;
            };

            for(const auto& run: analysis->line_runs(begin, finish))
            {
                std::vector<visual_piece> pieces;
                for(const auto& script: scripts)
                {
                    const auto a = std::max<std::size_t>(run.offset, script.begin);
                    const auto b = std::min<std::size_t>(run.offset + run.length, script.end);
                    if(a >= b)
                    {
                        continue;
                    }

                    auto cursor = a;
                    while(cursor < b)
                    {
                        const auto tab = value.find('\t', cursor);
                        const auto stop = std::min(b, tab == std::string_view::npos ? b : tab);
                        if(cursor < stop)
                        {
                            pieces.push_back({cursor, stop, (run.level & 1) != 0, script.script});
                        }
                        if(stop < b)
                        {
                            pieces.push_back({stop, stop + 1, (run.level & 1) != 0, script.script});
                        }

                        cursor = stop < b ? stop + 1 : b;
                    }
                }

                if(run.level & 1)
                {
                    std::ranges::reverse(pieces);
                }

                for(const auto& piece: pieces)
                {
                    const auto first_glyph = line.glyphs.size();
                    const auto start_x = x;

                    if(value[piece.begin] == '\t')
                    {
                        x = next_tab(x);
                        const auto descent =
                            std::max(line.line_height - line.ascender, metrics.bounds.height - metrics.ascender);
                        line.ascender = std::max(line.ascender, metrics.ascender);
                        line.line_height = line.ascender + descent;
                    }
                    else
                    {
                        auto shaped = shape_fallback(
                            engine,
                            *collection,
                            candidates,
                            request,
                            primary,
                            value.substr(piece.begin, piece.end - piece.begin),
                            size,
                            piece.rtl,
                            piece.script,
                            options.language
                        );
                        const auto descent =
                            std::max(line.line_height - line.ascender, shaped.bounds.height - shaped.ascender);
                        line.ascender = std::max(line.ascender, shaped.ascender);
                        line.line_height = line.ascender + descent;
                        for(auto glyph: shaped.glyphs)
                        {
                            glyph.cluster += static_cast<std::uint32_t>(piece.begin);
                            glyph.x += x;
                            glyph.origin += x;
                            line.glyphs.push_back(std::move(glyph));
                        }

                        x += shaped.bounds.width;
                    }

                    // Distinguish graphemes from shaping clusters. When the font supplies
                    // no ligature caret data, subdivide a cluster advance equally among
                    // its graphemes. This gives deterministic internal ligature positions.
                    std::vector<std::size_t> clusters{piece.begin, piece.end};
                    for(auto i = first_glyph; i < line.glyphs.size(); ++i)
                    {
                        clusters.push_back(line.glyphs[i].cluster);
                    }

                    std::ranges::sort(clusters);
                    clusters.erase(std::unique(clusters.begin(), clusters.end()), clusters.end());

                    struct cluster_geometry
                    {
                        float left;
                        float right;
                        std::size_t first;
                    };
                    std::vector<cluster_geometry> geometry(clusters.size(), {x, start_x, line.glyphs.size()});
                    for(auto i = first_glyph; i < line.glyphs.size(); ++i)
                    {
                        const auto& glyph = line.glyphs[i];
                        auto& cluster = geometry
                            [std::lower_bound(clusters.begin(), clusters.end(), glyph.cluster) - clusters.begin()];
                        cluster.left = std::min(cluster.left, glyph.origin);
                        cluster.right = std::max(cluster.right, glyph.origin + glyph.advance);
                        cluster.first = std::min(cluster.first, i);
                    }

                    for(std::size_t c = 0; c + 1 < clusters.size(); ++c)
                    {
                        auto [left, right, first] = geometry[c];
                        if(first_glyph == line.glyphs.size())
                        {
                            left = start_x;
                            right = x;
                        }
                        if(right < left)
                        {
                            right = left;
                        }

                        auto a =
                            std::lower_bound(boundaries.graphemes.begin(), boundaries.graphemes.end(), clusters[c]);
                        auto b = std::upper_bound(a, boundaries.graphemes.end(), clusters[c + 1]);
                        if(b - a < 2)
                        {
                            continue;
                        }

                        const auto count = b - a - 1;
                        std::vector<float> stops;
                        if(count > 1 && first != line.glyphs.size() && glyph_access::face(line.glyphs[first]))
                        {
                            stops = glyph_access::face(line.glyphs[first])
                                        ->ligature_carets(line.glyphs[first].id, size, piece.rtl);
                            if(stops.size() != static_cast<std::size_t>(count - 1) || std::ranges::any_of(
                                                                                          stops,
                                                                                          [&](float at)
                                                                                          {
                                                                                              return at <= 0 ||
                                                                                                     at >= right - left;
                                                                                          }
                                                                                      ))
                            {
                                stops.clear();
                            }
                        }

                        const auto position = [&](std::ptrdiff_t index)
                        {
                            if(stops.empty())
                            {
                                return piece.rtl ? right - (right - left) * index / count
                                                 : left + (right - left) * index / count;
                            }

                            const auto visual = piece.rtl ? count - index : index;

                            return visual == 0 ? left : visual == count ? right : left + stops[visual - 1];
                        };

                        for(std::ptrdiff_t g = 0; g <= count; ++g)
                        {
                            const auto at = position(g);
                            const auto affinity = g == count ? text_affinity::upstream : text_affinity::downstream;
                            line.carets.push_back({a[g], {at, 0}, affinity});

                            if(g < count)
                            {
                                const auto next = position(g + 1);
                                line.selection_cells.push_back(
                                    {a[g],
                                     a[g + 1],
                                     {std::min(at, next), 0, std::abs(next - at), metrics.bounds.height}}
                                );
                            }
                        }
                    }
                }
            }

            line.bounds.width = x;

            if(line.line_height <= 0)
            {
                line.ascender = metrics.ascender;
                line.line_height = metrics.bounds.height;
            }

            return line;
        };

        std::size_t begin = paragraph_begin;
        std::vector<std::size_t> shaping_boundaries{paragraph_begin, end};
        if(options.wrap != text_wrap::none && std::isfinite(width) && begin < end)
        {
            const auto paragraph_shape = shape_line(begin, end);
            for(const auto& glyph: paragraph_shape.glyphs)
            {
                shaping_boundaries.push_back(glyph.cluster);
            }
            for(auto at = begin; at < end; ++at)
            {
                if(value[at] == '\t')
                {
                    shaping_boundaries.push_back(at);
                    shaping_boundaries.push_back(at + 1);
                }
            }
            std::ranges::sort(shaping_boundaries);
            shaping_boundaries.erase(
                std::unique(shaping_boundaries.begin(), shaping_boundaries.end()),
                shaping_boundaries.end()
            );
        }

        const auto legal_breaks = [&](const std::vector<std::size_t>& opportunities)
        {
            std::vector<std::size_t> breaks;
            std::set_intersection(
                std::lower_bound(opportunities.begin(), opportunities.end(), paragraph_begin),
                std::upper_bound(opportunities.begin(), opportunities.end(), end),
                shaping_boundaries.begin(),
                shaping_boundaries.end(),
                std::back_inserter(breaks)
            );
            // The content ends before a hard separator, which is not itself
            // necessarily a Unicode break opportunity.
            if(breaks.empty() || breaks.back() != end)
            {
                breaks.push_back(end);
            }
            return breaks;
        };
        const auto word_breaks = legal_breaks(boundaries.lines);
        const auto grapheme_breaks = legal_breaks(boundaries.graphemes);

        do
        {
            auto finish = end;
            if(options.wrap != text_wrap::none && std::isfinite(width) && begin < end)
            {
                const auto fit = [&](const std::vector<std::size_t>& breaks, std::size_t limit)
                {
                    const auto first = std::upper_bound(breaks.begin(), breaks.end(), begin);
                    const auto last = std::upper_bound(first, breaks.end(), limit);
                    const auto count = static_cast<std::size_t>(last - first);
                    std::size_t accepted = 0;
                    std::size_t probe = 1;
                    // Probe increasing prefixes, then refine only the bracket
                    // around the line end. Never reshape the remaining suffix.
                    while(shape_line(begin, first[probe - 1]).bounds.width <= width)
                    {
                        accepted = probe;
                        if(probe == count)
                        {
                            return first[probe - 1];
                        }
                        probe += std::min(probe, count - probe);
                    }
                    auto rejected = probe;
                    while(accepted + 1 < rejected)
                    {
                        const auto middle = accepted + (rejected - accepted) / 2;
                        if(shape_line(begin, first[middle - 1]).bounds.width <= width)
                        {
                            accepted = middle;
                        }
                        else
                        {
                            rejected = middle;
                        }
                    }
                    // An oversized word/cluster still has to make progress.
                    return first[accepted == 0 ? 0 : accepted - 1];
                };
                finish = fit(options.wrap == text_wrap::grapheme ? grapheme_breaks : word_breaks, end);
                if(options.wrap == text_wrap::word_grapheme && shape_line(begin, finish).bounds.width > width)
                {
                    finish = fit(grapheme_breaks, finish);
                }
            }

            auto line = shape_line(begin, finish);
            if(result.lines.empty())
            {
                result.ascender = line.ascender;
            }

            const auto first = result.glyphs.size();
            const auto line_index = result.lines.size();
            for(auto glyph: line.glyphs)
            {
                glyph.y -= y + line.ascender - result.ascender;
                result.glyphs.push_back(std::move(glyph));
            }

            for(auto caret: line.carets)
            {
                caret.position.y = y;
                caret.line = line_index;
                result.carets.push_back(caret);
            }

            for(auto cell: line.selection_cells)
            {
                cell.bounds.y = y;
                cell.bounds.height = line.line_height;
                result.selection_cells.push_back(cell);
            }

            result.lines.push_back(
                {first,
                 result.glyphs.size(),
                 line.bounds.width,
                 begin,
                 finish,
                 y,
                 line.line_height,
                 y + line.ascender,
                 0,
                 analysis->rtl(begin)}
            );

            result.bounds.width = std::max(result.bounds.width, line.bounds.width);
            y += line.line_height + options.line_spacing;
            begin = finish;
        } while(begin < end);

        paragraph_begin = paragraph_end;
    }

    result.bounds.height = y - options.line_spacing;

    if(options.max_lines > 0 && !result.lines.empty())
    {
        const auto count = std::min(options.max_lines, result.lines.size());
        const auto original_last = result.lines[count - 1];
        const bool hidden = count < result.lines.size();
        const bool overflow = std::isfinite(width) && original_last.width > width;

        result.lines.resize(count);
        result.glyphs.resize(original_last.end);
        std::erase_if(
            result.carets,
            [&](const auto& caret)
            {
                return caret.line >= count;
            }
        );
        std::erase_if(
            result.selection_cells,
            [&](const auto& cell)
            {
                return cell.bounds.y > original_last.y;
            }
        );

        if(options.ellipsis && (hidden || overflow))
        {
            auto final_options = options;
            final_options.max_lines = 0;
            final_options.ellipsis = false;
            final_options.wrap = text_wrap::none;
            final_options.direction = original_last.rtl ? text_direction::rtl : text_direction::ltr;

            auto cut = original_last.text_end;
            layout_data final;
            for(;;)
            {
                auto display = std::string(value.substr(original_last.text_begin, cut - original_last.text_begin));
                display += "\xe2\x80\xa6";
                final = layout_paragraphs(
                    engine,
                    collection,
                    request,
                    display,
                    size,
                    std::numeric_limits<float>::infinity(),
                    final_options
                );
                if(!std::isfinite(width) || final.bounds.width <= width || cut == original_last.text_begin)
                {
                    break;
                }

                const auto previous = std::lower_bound(boundaries.graphemes.begin(), boundaries.graphemes.end(), cut);
                cut = previous == boundaries.graphemes.begin() ? original_last.text_begin : *std::prev(previous);
            }

            if(count == 1)
            {
                result.ascender = final.ascender;
            }

            result.glyphs.resize(original_last.begin);
            std::erase_if(
                result.carets,
                [&](const auto& caret)
                {
                    return caret.line == count - 1;
                }
            );
            std::erase_if(
                result.selection_cells,
                [&](const auto& cell)
                {
                    return cell.bounds.y == original_last.y;
                }
            );

            for(auto glyph: final.glyphs)
            {
                glyph.cluster += static_cast<std::uint32_t>(original_last.text_begin);
                glyph.y -= original_last.y + final.ascender - result.ascender;
                result.glyphs.push_back(std::move(glyph));
            }

            for(auto caret: final.carets)
            {
                caret.offset += original_last.text_begin;
                if(caret.offset > cut)
                {
                    continue;
                }

                caret.line = count - 1;
                caret.position.y = original_last.y;
                result.carets.push_back(caret);
            }

            for(auto cell: final.selection_cells)
            {
                cell.begin += original_last.text_begin;
                cell.end += original_last.text_begin;
                if(cell.end > cut)
                {
                    continue;
                }

                cell.bounds.y = original_last.y;
                result.selection_cells.push_back(cell);
            }

            auto& last = result.lines.back();
            last.end = result.glyphs.size();
            last.text_end = cut;
            last.width = final.bounds.width;
            last.height = final.bounds.height;
            last.baseline = original_last.y + final.ascender;
        }

        result.bounds.width = 0;
        for(const auto& line: result.lines)
        {
            result.bounds.width = std::max(result.bounds.width, line.width);
        }
        result.bounds.height = result.lines.back().y + result.lines.back().height;
    }

    // Placement is part of logical geometry, including finite no-wrap widths.
    place_text(result, width, text_alignment::left);

    return result;
}

void place_text(layout_data& result, float width, text_alignment alignment)
{
    if(alignment < text_alignment::left || alignment > text_alignment::end)
    {
        throw std::invalid_argument("Invalid text alignment");
    }

    if(std::isfinite(width))
    {
        for(std::size_t index = 0; index < result.lines.size(); ++index)
        {
            auto& line = result.lines[index];
            const auto spare = std::max(0.0f, width - line.width);
            const auto previous = line.offset;
            const bool right = alignment == text_alignment::right || (alignment == text_alignment::start && line.rtl) ||
                               (alignment == text_alignment::end && !line.rtl);
            line.offset = alignment == text_alignment::center ? spare / 2 : right ? spare : 0;
            const auto delta = line.offset - previous;

            for(auto i = line.begin; i < line.end; ++i)
            {
                result.glyphs[i].x += delta;
                result.glyphs[i].origin += delta;
            }

            for(auto& caret: result.carets)
            {
                if(caret.line == index)
                {
                    caret.position.x += delta;
                }
            }

            for(auto& cell: result.selection_cells)
            {
                if(cell.bounds.y == line.y)
                {
                    cell.bounds.x += delta;
                }
            }
        }
    }
    result.ink_bounds = {};
    bool has_ink = false;
    for(const auto& glyph: result.glyphs)
    {
        if(glyph.ink.width <= 0 || glyph.ink.height <= 0)
        {
            continue;
        }
        const rect
            ink{glyph.x + glyph.ink.x, result.ascender - glyph.y + glyph.ink.y, glyph.ink.width, glyph.ink.height};
        if(!has_ink)
        {
            result.ink_bounds = ink;
            has_ink = true;
        }
        else
        {
            const auto right = std::max(result.ink_bounds.x + result.ink_bounds.width, ink.x + ink.width);
            const auto bottom = std::max(result.ink_bounds.y + result.ink_bounds.height, ink.y + ink.height);
            result.ink_bounds.x = std::min(result.ink_bounds.x, ink.x);
            result.ink_bounds.y = std::min(result.ink_bounds.y, ink.y);
            result.ink_bounds.width = right - result.ink_bounds.x;
            result.ink_bounds.height = bottom - result.ink_bounds.y;
        }
    }
}

} // namespace details
} // namespace centypo
