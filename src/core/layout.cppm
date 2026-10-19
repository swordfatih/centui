
module;

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include <yoga/Yoga.h>

export module centui.layout;

import centui.style;
import centui.unit;

namespace centui::details
{

YGFlexDirection to_yoga(direction value)
{
    switch(value)
    {
    case direction::column:
        return YGFlexDirectionColumn;
    case direction::column_reverse:
        return YGFlexDirectionColumnReverse;
    case direction::row:
        return YGFlexDirectionRow;
    case direction::row_reverse:
        return YGFlexDirectionRowReverse;
    }

    throw std::invalid_argument("Invalid layout enum value");
}

YGPositionType to_yoga(position value)
{
    switch(value)
    {
    case position::fixed:
        return YGPositionTypeStatic;
    case position::relative:
        return YGPositionTypeRelative;
    case position::absolute:
        return YGPositionTypeAbsolute;
    }

    throw std::invalid_argument("Invalid layout enum value");
}

YGAlign to_yoga(alignment value)
{
    switch(value)
    {
    case alignment::automatic:
        return YGAlignAuto;
    case alignment::start:
        return YGAlignFlexStart;
    case alignment::center:
        return YGAlignCenter;
    case alignment::end:
        return YGAlignFlexEnd;
    case alignment::stretch:
        return YGAlignStretch;
    case alignment::baseline:
        return YGAlignBaseline;
    case alignment::space_between:
        return YGAlignSpaceBetween;
    case alignment::space_around:
        return YGAlignSpaceAround;
    case alignment::space_evenly:
        return YGAlignSpaceEvenly;
    }

    throw std::invalid_argument("Invalid layout enum value");
}

YGJustify to_yoga(justification value)
{
    switch(value)
    {
    case justification::start:
        return YGJustifyFlexStart;
    case justification::center:
        return YGJustifyCenter;
    case justification::end:
        return YGJustifyFlexEnd;
    case justification::space_between:
        return YGJustifySpaceBetween;
    case justification::space_around:
        return YGJustifySpaceAround;
    case justification::space_evenly:
        return YGJustifySpaceEvenly;
    }

    throw std::invalid_argument("Invalid layout enum value");
}

template <typename px_function, typename percent_function, typename fit_function, typename... target_types>
void set_length(YGNodeRef node, length value, px_function px, percent_function percent, fit_function fit, target_types... targets)
{
    switch(value.unit)
    {
    case unit::px:
        std::invoke(px, node, targets..., value.value);
        break;
    case unit::percent:
        std::invoke(percent, node, targets..., value.value);
        break;
    case unit::fit:
        std::invoke(fit, node, targets...);
        break;
    }
}

void set_width(YGNodeRef node, length value)
{
    set_length(node, value, YGNodeStyleSetWidth, YGNodeStyleSetWidthPercent, YGNodeStyleSetWidthAuto);
}

void set_height(YGNodeRef node, length value)
{
    set_length(node, value, YGNodeStyleSetHeight, YGNodeStyleSetHeightPercent, YGNodeStyleSetHeightAuto);
}

template <typename px_function, typename percent_function, typename fit_function>
void set_edges(YGNodeRef node, const edges& value, px_function px, percent_function percent, fit_function fit)
{
    set_length(node, value.top, px, percent, fit, YGEdgeTop);
    set_length(node, value.right, px, percent, fit, YGEdgeRight);
    set_length(node, value.bottom, px, percent, fit, YGEdgeBottom);
    set_length(node, value.left, px, percent, fit, YGEdgeLeft);
}

void set_padding(YGNodeRef node, const edges& value)
{
    set_edges(node, value, YGNodeStyleSetPadding, YGNodeStyleSetPaddingPercent, [](YGNodeRef target, YGEdge edge)
    {
        YGNodeStyleSetPadding(target, edge, 0.0f);
    });
}

void set_margin(YGNodeRef node, const edges& value)
{
    set_edges(node, value, YGNodeStyleSetMargin, YGNodeStyleSetMarginPercent, YGNodeStyleSetMarginAuto);
}

void set_inset(YGNodeRef node, const edges& value)
{
    set_edges(node, value, YGNodeStyleSetPosition, YGNodeStyleSetPositionPercent, YGNodeStyleSetPositionAuto);
}

void set_gap(YGNodeRef node, length value)
{
    set_length(node, value, YGNodeStyleSetGap, YGNodeStyleSetGapPercent, [](YGNodeRef target, YGGutter gutter)
    {
        YGNodeStyleSetGap(target, gutter, 0.0f);
    }, YGGutterAll);
}

} // namespace centui::details

export namespace centui
{

struct node_deleter
{
    void operator()(YGNode* node) const noexcept
    {
        YGNodeFree(node);
    }
};

using measure_function = std::move_only_function<rect(float, float) const>;

class layout
{
public:
    void update(const style& style)
    {
        if(!_internal)
        {
            _internal.reset(YGNodeNew());
        }

        auto* node = _internal.get();

        YGNodeStyleSetFlexDirection(node, details::to_yoga(style.flex_direction));
        YGNodeStyleSetPositionType(node, details::to_yoga(style.position));
        YGNodeStyleSetAlignItems(node, details::to_yoga(style.align_items));
        YGNodeStyleSetJustifyContent(node, details::to_yoga(style.justify_content));

        details::set_width(node, style.width);
        details::set_height(node, style.height);
        details::set_padding(node, style.padding);
        details::set_margin(node, style.margin);
        details::set_inset(node, style.inset);
        details::set_gap(node, style.gap);
    }

    void insert_child(const layout& child)
    {
        YGNodeInsertChild(_internal.get(), child._internal.get(), YGNodeGetChildCount(_internal.get()));
    }

    void clear_children()
    {
        if(_internal)
        {
            YGNodeRemoveAllChildren(_internal.get());
        }
    }

    void clear_measure()
    {
        if(_internal && YGNodeHasMeasureFunc(_internal.get()))
        {
            YGNodeMarkDirty(_internal.get());
            YGNodeSetMeasureFunc(_internal.get(), nullptr);
            YGNodeSetContext(_internal.get(), nullptr);
        }

        _measure.reset();
    }

    void calculate(float width, float height)
    {
        YGNodeCalculateLayout(_internal.get(), width, height, YGDirectionLTR);
    }

    [[nodiscard]] rect bounds() const
    {
        return {
            .x = YGNodeLayoutGetLeft(_internal.get()),
            .y = YGNodeLayoutGetTop(_internal.get()),
            .width = YGNodeLayoutGetWidth(_internal.get()),
            .height = YGNodeLayoutGetHeight(_internal.get())};
    }

    void set_measure(measure_function measure)
    {
        if(!measure)
        {
            clear_measure();
            return;
        }

        if(!_internal)
        {
            _internal.reset(YGNodeNew());
        }

        if(!_measure)
        {
            _measure = std::make_unique<measure_function>();
        }

        *_measure = std::move(measure);
        YGNodeSetContext(_internal.get(), _measure.get());

        if(YGNodeHasMeasureFunc(_internal.get()))
        {
            YGNodeMarkDirty(_internal.get());
            return;
        }

        YGNodeSetMeasureFunc(_internal.get(), [](YGNodeConstRef node, float width, YGMeasureMode width_mode, float height, YGMeasureMode height_mode) -> YGSize
        {
            const auto& measure = *static_cast<measure_function*>(YGNodeGetContext(node));

            const auto available_width = width_mode == YGMeasureModeUndefined ? std::numeric_limits<float>::infinity() : width;
            const auto available_height = height_mode == YGMeasureModeUndefined ? std::numeric_limits<float>::infinity() : height;

            const auto size = measure(available_width, available_height);

            return {
                width_mode == YGMeasureModeExactly ? width : width_mode == YGMeasureModeAtMost ? std::min(size.width, width)
                                                                                               : size.width,
                height_mode == YGMeasureModeExactly ? height : height_mode == YGMeasureModeAtMost ? std::min(size.height, height)
                                                                                                  : size.height};
        });
    }

private:
    std::unique_ptr<measure_function>    _measure;
    std::unique_ptr<YGNode, node_deleter> _internal;
};

} // namespace centui
