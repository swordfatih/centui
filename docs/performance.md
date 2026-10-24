# Preparation and resource caches

CentUI retains Yoga nodes across append/remove operations. Removed layout entries
are destroyed at edit commit, including when the viewport is minimized. Geometry
application uses Yoga's new-layout flag, while ancestor movement and scrolling
force placement updates. Layout callbacks retain their existing notification and
exception-retry behavior.

## Optional paint boundaries

Call `panel->set_paint_boundary(true)` to reuse a panel's prepared commands while
another region changes. This is useful for static panels beside animated controls.
It is disabled by default on every widget. Custom widget setters must call
`invalidate_paint()` or `invalidate_layout()` when their output changes.

The initial implementation deliberately clears boundary caches after geometry,
viewport, or input-state changes. It does not accelerate scrolling or hover-only
updates. It preserves the renderer API and still copies and submits every visible
quad. No offscreen render targets are introduced.

`document.set_command_cache_budget(bytes)` controls retained command storage
(4 MiB by default, zero disables admission). The budget counts command payloads;
container overhead, the final draw list, and referenced image resources are extra.
Nested boundaries may duplicate commands and should be used sparingly.
`document.statistics()` reports cumulative layout allocations, paint visits,
command-cache hits, and currently retained command bytes.

Build with `-DCENTUI_BUILD_BENCHMARKS=ON` and run `centui_benchmark` for a static
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
smoke tests (`CENTUI_BUILD_INTEGRATION_TESTS`) render text/buttons, resize, and exit
after four frames. Linux uses Xvfb and software GL. Core sanitizer coverage is
separate from graphics testing.

Consumer CI configures an independent application using FetchContent SOURCE_DIR,
FetchContent Git (cloning the checked-out revision), and add_subdirectory. It links
only `centui::centui` and executes a prepared-drawing check. Installed
`find_package(centui)` packages are not currently provided.

Layout, input, and frame scheduling now have separate internal state owners. The
scheduler owns request coalescing. Document
continues to own phase guards, deferred edits, and exception ordering. Further subsystem API narrowing and GPU batching/atlases remain separate improvements.
