---
layout: default
title: Yoga Coverage
nav_exclude: true
---

# Yoga layout coverage

CentUI targets the public layout capabilities of pinned Yoga 3.2.1. Scalar Yoga
`flex` is expressed through the explicit `flex {grow, shrink, basis}` value.
Native allocation, tree cloning, logging hooks, callback context, and cache flags
remain integration responsibilities rather than widget style properties.

The added tests in `tests/layout.test.cpp` are grouped by the following tags. This
matrix describes test intent, not an assertion that the suite has been executed.

| Capability | Coverage |
| --- | --- |
| Flex grow | Weighted free space, maximum-size redistribution, zero factors |
| Flex shrink | Unequal bases, minimum-size redistribution, nonshrinking sibling |
| Flex basis | Point, percentage, auto, undefined; row and column |
| Dimensions | Point/percentage constraints, automatic dimensions, clearing min/max |
| Aspect ratio | Derive either dimension, clear ratio, reject zero/invalid values |
| Box sizing | Border/content box with asymmetric borders and padding |
| Direction | All four main-axis directions, root RTL, inherited and overridden direction |
| Wrapping | No wrap, wrap, reverse wrap, wrapped line alignment |
| Alignment | Every justification/content alignment, self override/inheritance/stretch, font/custom baselines |
| Gaps | Row/column overrides, percentages, fallback after clearing an override |
| Edges | Physical, logical, axis fallback, percentages, negative and automatic margins |
| Position | Static/relative/absolute, percentage/logical insets, containing-block opt-in and reset |
| Display | Flex/none/contents transitions, nested and root contents, hidden/collapsed interaction |
| Rendering and input | Per-edge border drawing, boxless ancestry, focus, pointer targeting, clipping |
| Overflow | Visible/clip/scroll, overflow result, drawing clips and pointer targets |
| Measurement | Flexible content constraints, invalid results, callback exception boundary |
| Baselines | Padding/border offset, font ascenders, reference child, exceptions and retry |
| Configuration | Root direction, fractional rounding, all compatibility flags, experimental basis recalculation |
| Style lifecycle | Composition, whole-value patches, invalidation, unchanged patches, resets, atomic validation |

Existing document, font, input, rendering, and scroll tests remain relevant to
layout integration. New Yoga features must extend both this matrix and their
behavioral tests.
