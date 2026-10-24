#include <centui/layout.hpp>

#include <yoga/Yoga.h>

namespace centui::details
{

void invalidate_node(YGNodeRef node)
{
    // Yoga's public MarkDirty only accepts measured leaves. Metadata and baseline
    // callback setters do not dirty containers; this setter propagates dirtiness
    // without leaving a different reference-baseline value on the node.
    const auto reference = YGNodeIsReferenceBaseline(node);
    YGNodeSetIsReferenceBaseline(node, !reference);
    YGNodeSetIsReferenceBaseline(node, reference);
}

YGDirection to_yoga(layout_direction value)
{
    switch(value)
    {
    case layout_direction::inherit:
        return YGDirectionInherit;
    case layout_direction::ltr:
        return YGDirectionLTR;
    case layout_direction::rtl:
        return YGDirectionRTL;
    }

    throw std::invalid_argument("Invalid layout direction");
}

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

template <typename px_function, typename percent_function, typename automatic_function, typename... target_types>
void set_length(
    YGNodeRef node,
    length value,
    px_function px,
    percent_function percent,
    automatic_function automatic,
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

    case unit::automatic:
    {
        std::invoke(automatic, node, targets...);
        break;
    }
    case unit::undefined:
    {
        std::invoke(px, node, targets..., YGUndefined);
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

template <typename px_function, typename percent_function, typename automatic_function>
void set_edges(
    YGNodeRef node,
    const edges& value,
    px_function px,
    percent_function percent,
    automatic_function automatic
)
{
    set_length(node, value.top, px, percent, automatic, YGEdgeTop);
    set_length(node, value.right, px, percent, automatic, YGEdgeRight);
    set_length(node, value.bottom, px, percent, automatic, YGEdgeBottom);
    set_length(node, value.left, px, percent, automatic, YGEdgeLeft);
    set_length(node, value.start, px, percent, automatic, YGEdgeStart);
    set_length(node, value.end, px, percent, automatic, YGEdgeEnd);
    set_length(node, value.horizontal, px, percent, automatic, YGEdgeHorizontal);
    set_length(node, value.vertical, px, percent, automatic, YGEdgeVertical);
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

void set_gap(YGNodeRef node, length value, YGGutter gutter)
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
        gutter
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

struct layout_configuration::data
{
    std::unique_ptr<YGConfig, config_deleter> config{YGConfigNew()};
    layout_options options;
};

struct layout::data
{
    struct measurement
    {
        measure_function callback;
        baseline_function baseline;
        std::exception_ptr failure;
    };

    // The node is destroyed before its callback state and configuration.
    std::shared_ptr<layout_configuration::data> configuration;
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

void layout_options::validate() const
{
    if(root_direction != layout_direction::ltr && root_direction != layout_direction::rtl)
    {
        throw std::invalid_argument("Root layout direction must be LTR or RTL");
    }

    if(!std::isfinite(point_scale_factor) || point_scale_factor < 0)
    {
        throw std::invalid_argument("Layout point scale must be finite and nonnegative");
    }
}

layout_configuration::layout_configuration(layout_options options)
    : m_data(std::make_shared<data>())
{
    options.validate();
    m_data->options = options;
    YGConfigSetPointScaleFactor(m_data->config.get(), options.point_scale_factor);
    auto errata = YGErrataNone;

    if(options.stretch_flex_basis)
    {
        errata |= YGErrataStretchFlexBasis;
    }
    if(options.absolute_position_without_insets_excludes_padding)
    {
        errata |= YGErrataAbsolutePositionWithoutInsetsExcludesPadding;
    }
    if(options.absolute_percent_against_inner_size)
    {
        errata |= YGErrataAbsolutePercentAgainstInnerSize;
    }

    YGConfigSetErrata(m_data->config.get(), errata);
    YGConfigSetExperimentalFeatureEnabled(
        m_data->config.get(),
        YGExperimentalFeatureWebFlexBasis,
        options.experimental_web_flex_basis
    );
}

layout::layout(layout_options options)
    : layout(layout_configuration{options})
{
}

layout::layout(const layout_configuration& configuration)
    : m_data(std::make_unique<data>())
{
    m_data->configuration = configuration.m_data;
    m_data->node.reset(YGNodeNewWithConfig(m_data->configuration->config.get()));
}

layout::~layout() = default;

void layout::update(const style& style)
{
    style.validate();

    if(!m_data->node)
    {
        m_data->node.reset(YGNodeNewWithConfig(m_data->configuration->config.get()));
    }

    auto* node = m_data->node.get();

    YGNodeStyleSetFlexDirection(node, details::to_yoga(style.flex_direction));
    YGNodeStyleSetPositionType(node, details::to_yoga(style.position));
    YGNodeStyleSetAlignItems(node, details::to_yoga(style.align_items));
    YGNodeStyleSetAlignSelf(node, details::to_yoga(style.align_self));
    YGNodeStyleSetAlignContent(node, details::to_yoga(style.align_content));
    YGNodeStyleSetDirection(node, details::to_yoga(style.layout_direction));
    YGNodeStyleSetFlexGrow(node, style.flex.grow);
    YGNodeStyleSetFlexShrink(node, style.flex.shrink);
    details::set_length(
        node,
        style.flex.basis,
        YGNodeStyleSetFlexBasis,
        YGNodeStyleSetFlexBasisPercent,
        YGNodeStyleSetFlexBasisAuto
    );
    YGNodeStyleSetFlexWrap(
        node,
        style.flex_wrap == wrapping::none   ? YGWrapNoWrap
        : style.flex_wrap == wrapping::wrap ? YGWrapWrap
                                            : YGWrapWrapReverse
    );
    YGNodeStyleSetBoxSizing(
        node,
        style.box_sizing == box_sizing::border_box ? YGBoxSizingBorderBox : YGBoxSizingContentBox
    );
    YGNodeStyleSetAspectRatio(node, style.aspect_ratio.value_or(YGUndefined));
    YGNodeSetIsReferenceBaseline(node, style.reference_baseline);
    if(YGNodeGetAlwaysFormsContainingBlock(node) != style.forms_containing_block)
    {
        details::invalidate_node(node);
    }
    YGNodeSetAlwaysFormsContainingBlock(node, style.forms_containing_block);
    YGNodeStyleSetJustifyContent(node, details::to_yoga(style.justify_content));
    YGNodeStyleSetDisplay(
        node,
        style.visibility == visibility::collapsed || style.display == display::none ? YGDisplayNone
        : style.display == display::contents                                        ? YGDisplayContents
                                                                                    : YGDisplayFlex
    );
    YGNodeStyleSetOverflow(
        node,
        style.overflow == overflow::scroll ? YGOverflowScroll
        : style.overflow == overflow::clip ? YGOverflowHidden
                                           : YGOverflowVisible
    );
    details::set_edges(
        node,
        style.border_width,
        YGNodeStyleSetBorder,
        YGNodeStyleSetBorder,
        [](YGNodeRef target, YGEdge edge)
        {
            YGNodeStyleSetBorder(target, edge, YGUndefined);
        }
    );

    const auto no_auto = [](YGNodeRef) {};
    details::set_length(node, style.min_width, YGNodeStyleSetMinWidth, YGNodeStyleSetMinWidthPercent, no_auto);
    details::set_length(node, style.min_height, YGNodeStyleSetMinHeight, YGNodeStyleSetMinHeightPercent, no_auto);
    details::set_length(node, style.max_width, YGNodeStyleSetMaxWidth, YGNodeStyleSetMaxWidthPercent, no_auto);
    details::set_length(node, style.max_height, YGNodeStyleSetMaxHeight, YGNodeStyleSetMaxHeightPercent, no_auto);

    details::set_width(node, style.width);
    details::set_height(node, style.height);
    details::set_padding(node, style.padding);
    details::set_margin(node, style.margin);
    details::set_inset(node, style.inset);
    details::set_gap(node, style.gap, YGGutterAll);
    details::set_gap(node, style.row_gap, YGGutterRow);
    details::set_gap(node, style.column_gap, YGGutterColumn);
}

void layout::insert_child(const layout& child)
{
    YGNodeInsertChild(m_data->node.get(), child.m_data->node.get(), YGNodeGetChildCount(m_data->node.get()));
}

void layout::remove_children()
{
    YGNodeRemoveAllChildren(m_data->node.get());
}

void layout::invalidate_measure()
{
    if(YGNodeHasMeasureFunc(m_data->node.get()))
    {
        YGNodeMarkDirty(m_data->node.get());
    }
    else
    {
        details::invalidate_node(m_data->node.get());
    }
}

void layout::calculate(float width, float height)
{
    YGNodeCalculateLayout(
        m_data->node.get(),
        width,
        height,
        details::to_yoga(m_data->configuration->options.root_direction)
    );
}

layout_metrics layout::metrics() const
{
    const auto* node = m_data->node.get();
    const auto read = [node](auto getter)
    {
        return computed_edges{
            getter(node, YGEdgeTop),
            getter(node, YGEdgeRight),
            getter(node, YGEdgeBottom),
            getter(node, YGEdgeLeft)
        };
    };

    return {
        read(YGNodeLayoutGetMargin),
        read(YGNodeLayoutGetPadding),
        read(YGNodeLayoutGetBorder),
        YGNodeLayoutGetDirection(node) == YGDirectionRTL ? layout_direction::rtl : layout_direction::ltr,
        YGNodeLayoutGetHadOverflow(node)
    };
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
        details::invalidate_node(m_data->node.get());
        std::rethrow_exception(failure);
    }
}

void layout::set_baseline(baseline_function baseline)
{
    if(!m_data->node)
    {
        m_data->node.reset(YGNodeNewWithConfig(m_data->configuration->config.get()));
    }
    if(!m_data->measure)
    {
        m_data->measure = std::make_unique<data::measurement>();
    }

    m_data->measure->baseline = std::move(baseline);
    m_data->measure->failure = {};
    YGNodeSetContext(m_data->node.get(), m_data->measure.get());
    details::invalidate_node(m_data->node.get());

    if(!m_data->measure->baseline)
    {
        YGNodeSetBaselineFunc(m_data->node.get(), nullptr);
        return;
    }

    YGNodeSetBaselineFunc(
        m_data->node.get(),
        [](YGNodeConstRef node, float width, float height)
        {
            auto& state = *static_cast<data::measurement*>(YGNodeGetContext(node));
            const auto inset = [node](YGEdge edge)
            {
                return YGNodeLayoutGetPadding(node, edge) + YGNodeLayoutGetBorder(node, edge);
            };

            try
            {
                const auto top = inset(YGEdgeTop);
                const auto value = state.baseline(
                    {std::max(0.0f, width - inset(YGEdgeLeft) - inset(YGEdgeRight)),
                     std::max(0.0f, height - top - inset(YGEdgeBottom))}
                );
                if(!std::isfinite(value))
                {
                    throw std::invalid_argument("Baseline must be finite");
                }
                return top + value;
            }
            catch(...)
            {
                state.failure = std::current_exception();
                return height;
            }
        }
    );
}

void layout::set_measure(measure_function measure)
{
    if(!measure)
    {
        YGNodeSetMeasureFunc(m_data->node.get(), nullptr);
        if(m_data->measure)
        {
            m_data->measure->callback = {};
        }
        return;
    }

    if(!m_data->node)
    {
        m_data->node.reset(YGNodeNewWithConfig(m_data->configuration->config.get()));
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
                if(!std::isfinite(size.width) || !std::isfinite(size.height) || size.width < 0 || size.height < 0)
                {
                    throw std::invalid_argument("Measurement must be finite and nonnegative");
                }
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
