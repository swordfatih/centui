export module centui.unit;

export namespace centui
{

struct rect
{
    float x;
    float y;
    float width;
    float height;
};

enum class unit
{
    px,
    percent,
    fit,
};

struct length
{
    float value = 0;
    unit  unit = unit::fit;

    constexpr bool operator==(const length&) const = default;
};

constexpr length px(float value)
{
    return {value, unit::px};
}

constexpr length percent(float value)
{
    return {value, unit::percent};
}

constexpr length fit()
{
    return {0, unit::fit};
}

struct edges
{
    length top = fit();
    length right = fit();
    length bottom = fit();
    length left = fit();

    static constexpr edges all(length value)
    {
        return {value, value, value, value};
    }

    constexpr bool operator==(const edges&) const = default;
};

enum class position
{
    fixed,
    relative,
    absolute
};

enum class direction
{
    column,
    column_reverse,
    row,
    row_reverse
};

enum class justification
{
    start,
    center,
    end,
    space_between,
    space_around,
    space_evenly
};

enum class alignment
{
    automatic,
    start,
    center,
    end,
    stretch,
    baseline,
    space_between,
    space_around,
    space_evenly
};

} // namespace centui