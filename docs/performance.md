# Preparation and resource caches

CentUI retains Yoga nodes across append/remove operations. Removed layout entries
are destroyed at edit commit, including when the viewport is minimized. Geometry
application uses Yoga's new-layout flag, while ancestor movement and scrolling
force placement updates. Layout synchronization follows dirty ancestor paths and
skips clean sibling subtrees. Direct child flags may be scanned along those paths;
this is not a strict O(depth) guarantee. A synchronized node is one whose native
inputs are processed, not a clean child flag inspected while traversing.

Yoga still calculates from the root to resolve flex, wrapping, percentage and
baseline dependencies. Every node reporting new layout is processed, including
dependent siblings. Inherited font/text metrics conservatively synchronize a
subtree; default font replacement retains a document-wide measurement fallback.
Layout-option replacement may rebuild native entries. Failed measurement,
baseline evaluation or synchronization retains pending layout for a conservative
full measurement retry.

`on_geometry()` rebuilds geometry from current allocated bounds. An explicit
`invalidate_geometry()` coalesces the hook and invalidates its paint subtree,
without Yoga calculation or unrelated geometry callbacks. Layout and placement
conservatively schedule geometry hooks across the tree, including fixed-size
widgets whose content metrics or child extents changed. Hooks must converge;
offset changes settle through another placement pass. Geometry requests live on
widgets, so scanning the mounted tree does not retain removed widgets or dispatch
requests into their previous document. `pending_changes().paint` includes pending
geometry and placement preparation. Zero-sized surfaces retain that work.

Custom widgets use `invalidate_layout()` for content/measurement/baseline changes,
`invalidate_geometry()` for derived geometry (paint included), and
`invalidate_paint()` for drawing alone. Framework setters do their own propagation.
Measurement, baseline evaluation, and painting may retain derived caches but may
not mutate or request work. External state dependencies require explicit
invalidation. A frame request only schedules a callback; the callback invalidates
what it changes. See [Extending CentUI](extending.md).

## Optional paint boundaries

Call `panel->set_paint_boundary(true)` to reuse a panel's prepared commands while
another region changes. This is useful for static panels beside animated controls.
It is disabled by default on every widget. Custom widget setters must call
`invalidate_paint()`, `invalidate_geometry()` or `invalidate_layout()` when their output changes.

Cache validity requires unchanged subtree drawing, inherited appearance and
interaction state, allocated geometry/effective clipping, raster scale and resource
content identity. Local appearance, explicit geometry requests, and hover/focus/
press transitions increment source/descendant revisions and containing ancestor
revisions. Unrelated sibling boundaries remain reusable, including beneath nested
boundaries. Allocated bounds, placement, topology and raster-scale changes
conservatively clear all command entries. It still copies and submits every
visible quad. No offscreen render targets are introduced.

Logical viewport resizing calculates Yoga with new constraints without inherently
reapplying unchanged native inputs. A scale-only change with the same logical size
refreshes raster commands and host coordinates without intrinsic measurement or
geometry callbacks. Logical text caches remain reusable, while glyph images use
the new raster scale. Explicit Yoga point-scale configuration remains independent.
Retained pointer positions preserve the existing physical-position adjustment on
scale changes; newly delivered pointer events use adapter-provided logical units.

Failed painting leaves the published draw list intact and paint pending. Completed
subtree cache entries remain valid under the same dependency contract on retry.
Eviction and budget changes affect storage, not widget content or layout; live
draw lists retain immutable resources.

`document.set_command_cache_budget(bytes)` controls retained command storage
(4 MiB by default, zero disables admission). The budget counts command payloads;
container overhead, the final draw list, and referenced image resources are extra.
Nested boundaries may duplicate commands and should be used sparingly.
`document.statistics()` reports cumulative layout allocations, native input
synchronizations (`layout_nodes_synchronized`), actual measurement invocations
(`measurement_calls`), root calculation attempts (`layout_calculations`), actual
geometry hook invocations (`geometry_callbacks`, including failures/retries), paint
visits, command-cache hits, and currently retained command bytes. Requests alone
do not increment counters. Idle preparation and equal effective mutations do no
synchronization, measurement, calculation or paint work.

Build with `-DCENTUI_BUILD_EXAMPLES=ON` and run `centui_example_benchmark` from `build/examples/benchmark/` for a static
panel beside an invalidating widget, with and without boundaries. This synthetic
CPU preparation benchmark is not a GPU or application frame-time measurement.
Use representative scrolling, resize, text-input, and topology workloads before
enabling boundaries broadly.

## GPU texture caches

SDL, SFML, and bgfx expose `set_cache_budget(bytes)` (32 MiB default), the existing
`set_cache_limit(images)`, and cumulative `cache_statistics()`. Accounting uses
RGBA upload bytes, excluding driver allocation overhead. Eviction happens at the
next draw, removes expired resources first, then least-recently-used resources
that are absent from the incoming list. Active-list resources can exceed the
budget; this prevents eviction and repeated upload during the same draw.
`clear_cache()` explicitly releases resources. Backend-native destruction handles
queued graphics work; callers must obey the existing renderer/thread lifetime rules.

## Validation

CI builds all adapters and examples on Linux, Windows, and macOS. Optional native
integration tests (`CENTUI_BUILD_TESTS`, with the corresponding adapters enabled) render text/buttons and exit
after four frames. Linux uses Xvfb and software GL. Core sanitizer coverage is
separate from graphics testing.

Consumer CI configures an independent application using FetchContent SOURCE_DIR,
FetchContent Git (cloning the checked-out revision), and add_subdirectory. It links
only `centui::centui` and executes a prepared-drawing check. Installed
`find_package(centui)` packages are not currently provided.

Layout, input, and frame scheduling now have separate internal state owners. The
scheduler owns request coalescing. Document
continues to own phase guards, deferred edits, and exception ordering. Further subsystem API narrowing and GPU batching/atlases remain separate improvements.
