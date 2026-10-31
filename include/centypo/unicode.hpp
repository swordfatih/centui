#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace centypo
{

/// \brief Rejects malformed UTF-8 and NUL, without normalizing valid source bytes.
void validate_text(std::string_view value);

/// \brief Unicode boundaries expressed as half-open UTF-8 byte offsets.
/// Includes zero and the end of the text. CRLF is one grapheme/break.
/// Word boundaries follow their own rules and may fall inside a grapheme.
struct text_boundaries
{
    std::vector<std::size_t> graphemes;
    std::vector<std::size_t> words;
    std::vector<std::size_t> lines;
    std::vector<std::size_t> hard_breaks;
};

[[nodiscard]] text_boundaries analyze_text(std::string_view value, std::string_view language = {});

/// \brief Converts host UTF-16 offsets to UTF-8 bytes, clamping inside a surrogate pair backwards.
[[nodiscard]] std::size_t text_byte_offset(std::string_view value, std::size_t utf16_offset);

/// \brief Converts a UTF-8 byte offset to host UTF-16 units, clamping inside a scalar backwards.
[[nodiscard]] std::size_t text_utf16_offset(std::string_view value, std::size_t byte_offset);

/// \brief Converts host scalar indexes (for example SDL IME offsets) to UTF-8 bytes.
[[nodiscard]] std::size_t text_byte_offset_from_codepoints(std::string_view value, std::size_t count);

} // namespace centypo
