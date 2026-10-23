#include <centui/layout.hpp>

#include <yoga/Yoga.h>

namespace centui::details
{

YGFlexDirection to_yoga(direction value)
{
    switch(value)
    {
    case direction::column:
    {
        return YGFlexDirectionColumn;
    }

    case direction::column_reverse:
    {
        return YGFlexDirectionColumnReverse;
    }

    case direction::row:
    {
        return YGFlexDirectionRow;
    }

    case direction::row_reverse:
    {
        return YGFlexDirectionRowReverse;
    }
    }

    throw std::invalid_argument("Invalid layout enum value");
}

YGPositionType to_yoga(position value)
{
    switch(value)
    {
    case position::normal:
    {
        return YGPositionTypeStatic;
    }

    case position::relative:
    {
        return YGPositionTypeRelative;
    }

    case position::absolute:
    {
        return YGPositionTypeAbsolute;
    }
    }

    throw std::invalid_argument("Invalid layout enum value");
}

YGAlign to_yoga(alignment value)
{
    switch(value)
    {
    case alignment::automatic:
    {
        return YGAlignAuto;
    }

    case alignment::start:
    {
        return YGAlignFlexStart;
    }

    case alignment::center:
    {
        return YGAlignCenter;
    }

    case alignment::end:
    {
        return YGAlignFlexEnd;
    }

    case alignment::stretch:
    {
        return YGAlignStretch;
    }

    case alignment::baseline:
    {
        return YGAlignBaseline;
    }

    case alignment::space_between:
    {
        return YGAlignSpaceBetween;
    }

    case alignment::space_around:
    {
        return YGAlignSpaceAround;
    }

    case alignment::space_evenly:
    {
        return YGAlignSpaceEvenly;
    }
    }

    throw std::invalid_argument("Invalid layout enum value");
}

YGJustify to_yoga(justification value)
{
    switch(value)
    {
    case justification::start:
    {
        return YGJustifyFlexStart;
    }

    case justification::center:
    {
        return YGJustifyCenter;
    }

    case justification::end:
    {
        return YGJustifyFlexEnd;
    }

    case justification::space_between:
    {
        return YGJustifySpaceBetween;
    }

    case justification::space_around:
    {
        return YGJustifySpaceAround;
    }

    case justification::space_evenly:
    {
        return YGJustifySpaceEvenly;
    }
    }

    throw std::invalid_argument("Invalid layout enum value");
}

template <typename px_function, typename percent_function, typename fit_function, typename... target_types>
void set_length(
    YGNodeRef node,
    length value,
    px_function px,
    percent_function percent,
    fit_function fit,
    target_types... targets
)
{
    switch(value.unit)
    {
    case unit::px:
    {
        std::invoke(px, node, targets..., value.value);
        break;
    }

    case unit::percent:
    {
        std::invoke(percent, node, targets..., value.value);
        break;
    }

    case unit::fit:
    {
        std::invoke(fit, node, targets...);
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
    set_edges(
        node,
        value,
        YGNodeStyleSetPadding,
        YGNodeStyleSetPaddingPercent,
        [](YGNodeRef target, YGEdge edge)
        {
            YGNodeStyleSetPadding(target, edge, 0.0f);
        }
    );
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
    set_length(
        node,
        value,
        YGNodeStyleSetGap,
        YGNodeStyleSetGapPercent,
        [](YGNodeRef target, YGGutter gutter)
        {
            YGNodeStyleSetGap(target, gutter, 0.0f);
        },
        YGGutterAll
    );
}

} // namespace centui::details

namespace centui
{

namespace
{

struct node_deleter
{
    void operator()(YGNode* node) const noexcept
    {
        YGNodeFree(node);
    }
};

struct config_deleter
{
    void operator()(YGConfig* config) const noexcept
    {
        YGConfigFree(config);
    }
};

} // namespace

struct layout::data
{
    struct measurement
    {
        measure_function callback;
        std::exception_ptr failure;
    };

    // The node is destroyed before its callback state and configuration.
    std::unique_ptr<YGConfig, config_deleter> config{YGConfigNew()};
    std::unique_ptr<measurement> measure;
    std::unique_ptr<YGNode, node_deleter> node;
};

measure_function::operator bool() const noexcept
{
    return static_cast<bool>(m_callable);
}

size measure_function::operator()(axis_constraint width, axis_constraint height) const
{
    return m_callable->invoke(width, height);
}

layout::layout()
    : m_data(std::make_unique<data>())
{
    // Text measures and paints in the same fractional coordinate space.
    YGConfigSetPointScaleFactor(m_data->config.get(), 0);
}

layout::~layout() = default;

void layout::update(const style& style)
{
    if(!m_data->node)
    {
        m_data->node.reset(YGNodeNewWithConfig(m_data->config.get()));
    }

    auto* node = m_data->node.get();

    YGNodeStyleSetFlexDirection(node, details::to_yoga(style.flex_direction));
    YGNodeStyleSetPositionType(node, details::to_yoga(style.position));
    YGNodeStyleSetAlignItems(node, details::to_yoga(style.align_items));
    YGNodeStyleSetJustifyContent(node, details::to_yoga(style.justify_content));
    YGNodeStyleSetDisplay(node, style.visibility == visibility::collapsed ? YGDisplayNone : YGDisplayFlex);
    YGNodeStyleSetOverflow(
        node,
        style.overflow == overflow::scroll ? YGOverflowScroll
        : style.overflow == overflow::clip ? YGOverflowHidden
                                           : YGOverflowVisible
    );
    YGNodeStyleSetBorder(node, YGEdgeAll, style.border_width);

    details::set_width(node, style.width);
    details::set_height(node, style.height);
    details::set_padding(node, style.padding);
    details::set_margin(node, style.margin);
    details::set_inset(node, style.inset);
    details::set_gap(node, style.gap);
}

void layout::insert_child(const layout& child)
{
    YGNodeInsertChild(m_data->node.get(), child.m_data->node.get(), YGNodeGetChildCount(m_data->node.get()));
}

void layout::calculate(float width, float height)
{
    YGNodeCalculateLayout(m_data->node.get(), width, height, YGDirectionLTR);
}

rect layout::bounds() const
{
    return {
        .x = YGNodeLayoutGetLeft(m_data->node.get()),
        .y = YGNodeLayoutGetTop(m_data->node.get()),
        .width = YGNodeLayoutGetWidth(m_data->node.get()),
        .height = YGNodeLayoutGetHeight(m_data->node.get()),
    };
}

rect layout::content_bounds() const
{
    const auto box = bounds();
    const auto left =
        YGNodeLayoutGetPadding(m_data->node.get(), YGEdgeLeft) + YGNodeLayoutGetBorder(m_data->node.get(), YGEdgeLeft);
    const auto top =
        YGNodeLayoutGetPadding(m_data->node.get(), YGEdgeTop) + YGNodeLayoutGetBorder(m_data->node.get(), YGEdgeTop);

    return {
        left,
        top,
        std::max(
            0.0f,
            box.width - left - YGNodeLayoutGetPadding(m_data->node.get(), YGEdgeRight) -
                YGNodeLayoutGetBorder(m_data->node.get(), YGEdgeRight)
        ),
        std::max(
            0.0f,
            box.height - top - YGNodeLayoutGetPadding(m_data->node.get(), YGEdgeBottom) -
                YGNodeLayoutGetBorder(m_data->node.get(), YGEdgeBottom)
        )
    };
}

void layout::rethrow_measure_failure()
{
    if(m_data->measure && m_data->measure->failure)
    {
        auto failure = std::exchange(m_data->measure->failure, {});
        YGNodeMarkDirty(m_data->node.get());
        std::rethrow_exception(failure);
    }
}

void layout::set_measure(measure_function measure)
{
    if(!measure)
    {
        throw std::invalid_argument("A measured widget requires a measurement function");
    }

    if(!m_data->node)
    {
        m_data->node.reset(YGNodeNewWithConfig(m_data->config.get()));
    }

    if(!m_data->measure)
    {
        m_data->measure = std::make_unique<data::measurement>();
    }

    m_data->measure->callback = std::move(measure);
    m_data->measure->failure = {};
    YGNodeSetContext(m_data->node.get(), m_data->measure.get());

    if(YGNodeHasMeasureFunc(m_data->node.get()))
    {
        YGNodeMarkDirty(m_data->node.get());

        return;
    }

    YGNodeSetMeasureFunc(
        m_data->node.get(),
        [](YGNodeConstRef node, float width, YGMeasureMode width_mode, float height, YGMeasureMode height_mode)
            -> YGSize
        {
            auto& state = *static_cast<data::measurement*>(YGNodeGetContext(node));

            const auto constraint = [](float value, YGMeasureMode mode)
            {
                return axis_constraint{
                    value,
                    mode == YGMeasureModeUndefined ? constraint_mode::unconstrained
                    : mode == YGMeasureModeExactly ? constraint_mode::exact
                                                   : constraint_mode::maximum
                };
            };

            centui::size size{};

            try
            {
                size = state.callback(constraint(width, width_mode), constraint(height, height_mode));
            }
            catch(...)
            {
                // Never unwind through Yoga's callback boundary.
                state.failure = std::current_exception();

                return {0, 0};
            }

            return {
                width_mode == YGMeasureModeExactly  ? width
                : width_mode == YGMeasureModeAtMost ? std::min(size.width, width)
                                                    : size.width,
                height_mode == YGMeasureModeExactly  ? height
                : height_mode == YGMeasureModeAtMost ? std::min(size.height, height)
                                                     : size.height
            };
        }
    );
}

} // namespace centui
