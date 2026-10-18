
module;

#include <yoga/Yoga.h>

export module centui.layout;

import std;
import centui.style;
import centui.unit;

namespace centui::details
{

template <typename Px, typename Percent, typename Fit = std::nullptr_t, typename Target = std::nullptr_t>
void set_length(YGNodeRef node, length value, Px px, Percent percent, Fit fit = nullptr, Target target = nullptr)
{
    if constexpr(std::is_same_v<Target, std::nullptr_t>)
    {
        switch(value.unit)
        {
        case unit::px:
            px(node, value.value);
            break;
        case unit::percent:
            percent(node, value.value);
            break;
        case unit::fit:
            fit(node);
            break;
        }
    }
    else
    {
        switch(value.unit)
        {
        case unit::px:
            px(node, target, value.value);
            break;
        case unit::percent:
            percent(node, target, value.value);
            break;
        case unit::fit:
            if constexpr(std::is_same_v<Fit, std::nullptr_t>)
            {
                px(node, target, 0.0f);
            }
            else
            {
                fit(node, target);
            }
            break;
        }
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

template <typename Px, typename Percent, typename Fit = std::nullptr_t>
void set_edges(YGNodeRef node, const edges& value, Px px, Percent percent, Fit fit = nullptr)
{
    set_length(node, value.top, px, percent, fit, YGEdgeTop);
    set_length(node, value.right, px, percent, fit, YGEdgeRight);
    set_length(node, value.bottom, px, percent, fit, YGEdgeBottom);
    set_length(node, value.left, px, percent, fit, YGEdgeLeft);
}

void set_padding(YGNodeRef node, const edges& value)
{
    set_edges(node, value, YGNodeStyleSetPadding, YGNodeStyleSetPaddingPercent);
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
    set_length(node, value, YGNodeStyleSetGap, YGNodeStyleSetGapPercent, nullptr, YGGutterAll);
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

        YGNodeStyleSetFlexDirection(node, static_cast<YGFlexDirection>(style.flex_direction));
        YGNodeStyleSetPositionType(node, static_cast<YGPositionType>(style.position));
        YGNodeStyleSetAlignItems(node, static_cast<YGAlign>(style.align_items));
        YGNodeStyleSetJustifyContent(node, static_cast<YGJustify>(style.justify_content));

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

private:
    std::unique_ptr<YGNode, node_deleter> _internal;
};

} // namespace centui
