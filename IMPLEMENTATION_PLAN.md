# CentUI simplification implementation plan

## Current module architecture (supersedes earlier layout)

The user authorized changing imports and explicitly rejected partitions and `document.cpp`. This supersedes the import-preservation, byte-for-byte, and module-layout requirements below. Preserve application construction, styling, and behavior instead. Use ordinary named modules only: `centui.types`, `centui.style`, `centui.render`, `centui.font`, `centui.layout`, `centui.tree`, `centui.widgets`, and umbrella `centui`, plus separate backends and app/window modules. Widget, handles, document, painter, and UI facade are implemented together in `tree.cppm` with ordinary friendship. `render` depends only on foundational types; font preparation consumes its data types. `style` depends only on types, including the lightweight font identifier. No partitions, compatibility forwarding modules, accessor bridges, or separate document implementation file.

## Objective and authority

Rewrite the unreleased library around one widget tree, one rendering contract, and a small inheritance-based extension API. Keep C++26 reflection and compile-time style composition. Preserve the declarative application API and the visual behavior demonstrated by `examples/main.cpp`.

There are no backward compatibility requirements. Delete obsolete APIs and tests of obsolete contracts. Do not retain parallel implementations or compatibility adapters. However, the API used by `examples/main.cpp` is a deliberate product requirement: keep that file byte-for-byte unchanged, including its imports. This requirement takes priority over module-count targets.

This document is an implementation specification for separately dispatched coding models. Implement the tasks in dependency order. Do not independently redesign the contracts below. If a contract proves impossible with the selected compiler, report the exact compiler error and the smallest proposed adjustment before dependent work proceeds.

No implementation or successful build is claimed by this plan. The current README explicitly records unverified implementation work. Establish a real baseline first.

## Success criteria

- `examples/main.cpp` builds unchanged and retains layout, colors, text, button states, and click-driven updates.
- Public widget values remain copyable typed handles. Capturing `title` by value still updates the displayed title.
- Custom widgets derive from a documented base and require no modifications to core enums, variants, dispatchers, or renderers.
- Both backends inherit the same renderer base and implement exactly the same rendering contract.
- There is one tree representation, one Yoga integration, one CPU text implementation, and one draw-list preparation path.
- Headless tests require no graphics backend or window library.
- Aim for 2,500–3,200 nonblank implementation lines, excluding tests, examples, dependencies, and shaders. This is a stretch estimate, not permission to remove required behavior or compress readable code.
- Aim for approximately 12–16 substantive module units. Required imports in `main.cpp` constrain consolidation. Do not create empty compatibility modules to hit a preconceived architecture.

## Frozen product scope

Keep: container/text/button construction; style packs; nested state-color overrides; setters; `patch_style`; default system font; explicit font selection; flex layout; constrained text wrapping; text clipping; pointer hover/press/release/cancel; host-focus changes; enabled state; optional application loop; SFML and bgfx rendering.

Do not add: ECS, dependency injection framework, runtime widget registration, property-binding framework, serialization, plugin loading, Unicode paragraph bidi, fallback-font chains, editable text, keyboard navigation, accessibility, scrolling, or a general animation system.

Delete: legacy raw-node API, `element_kind` dispatch, construction runtimes and migration, slot/generation storage, backend-owned font discovery, alternate raw-node rendering, vector-versus-frame overload families, retained frame/subtree snapshots, public preparation diagnostics, and obsolete aliases not used by the required example.

## Architectural contracts

### 1. Ownership and handles

Use stable heap-allocated polymorphic widget objects. Public handles and parents share ownership; parent links are weak. A document holds a root handle. No object moves when a tree is composed or mounted.

This intentionally changes the old lifetime contract:

- Copying a handle aliases the same widget.
- Moving a handle empties the source. Access through an empty handle throws a documented exception; `valid()` checks whether it holds an object.
- Removing a widget detaches it. Outstanding handles remain valid and usable.
- Destroying a document detaches its root and releases ownership. Outstanding handles keep their subtrees alive.
- A widget has at most one parent. Reject duplicate children, already-parented children, ancestor cycles, and attempts to attach a mounted root elsewhere. Validate an insertion batch before modifying the tree.
- A root is mounted in at most one document. Detachment clears its document association recursively.
- Structural edits are document operations after mounting. Before mounting, container construction composes the tree.
- Use a weak document-context reference on widgets for invalidation. The context never strongly owns widgets; the document owns the root. No strong parent/context cycles.
- Supply `weak_handle<T>` with `lock()`. Document that callbacks capturing their own widget or an ancestor must use weak handles. The existing button callback capturing its sibling title is valid.
- Widgets, documents, and font services are single-thread confined. State this once in the public contract; avoid pervasive runtime thread-ID checks.

Use an erased base handle for heterogeneous child lists and `handle<T>` for typed extension access. Keep the existing `text<Styles...>`, `button<Styles...>`, and `container<Styles...>` facade names and constructor syntax. These wrappers must share one handle mechanism, not implement separate ownership systems.

### 2. Widget extension

Define one public `widget` base with a virtual destructor and protected virtual hooks:

```cpp
virtual size measure(const measure_context&);
virtual void paint(painter&) const;
virtual void on_event(const input_event&);
```

Task 1 must freeze the exact support-type declarations. Measurement constraints must distinguish unconstrained, exact, and maximum sizes; two floats alone are insufficient for Yoga measurement.

The base owns resolved style, geometry, parent/child relationships, and dirty notification. Provide protected style/bounds access and named `invalidate_layout()` / `invalidate_paint()` helpers. User widgets must not access Yoga types or internal document records.

Framework traversal paints the background, invokes content painting, then visits ordered children. A widget hook does not manually traverse its children. `painter` appends portable commands; it never accesses a graphics backend.

Provide `make_widget<T>(args...)`, constrained to concrete classes derived from `widget`, returning `handle<T>`. The handle gives typed access without unchecked casts. Test both a custom leaf and a composite derived widget. Composite construction uses a protected child-attachment operation subject to the same tree rules.

Built-in facades delegate to ordinary implementations derived from `widget`. Do not template the layout engine, event dispatcher, or renderer on user style types.

### 3. Reflection and styles

Preserve the current style vocabulary and defaults required by `main.cpp`, including partial nested state-color overrides. Resolve style packs at compile time, left to right, with later supplied fields overriding earlier ones.

- Reject unknown property names and incompatible property types at compile time, including nested state fields.
- Produce one ordinary resolved `style` value per widget.
- Preserve `patch_style({.text_color = ...})` using a concrete aggregate patch type.
- Reflection implements repetitive field matching and patch application. Do not require generated aggregate types if compiler support makes that fragile; a short explicit patch declaration is acceptable.
- Keep property-to-dirty-category mapping in one place. Equal writes do no work.
- Validate a replacement completely before publishing it. Failed patches leave style and dirty state unchanged.
- Geometry/font changes invalidate layout and paint; color changes invalidate paint. Text cache keys account for text, font identity, size, and constraints.

### 4. Layout, preparation, and input

One document coordinates layout and drawing. One private Yoga wrapper owns native layout resources through RAII. Keep fractional coordinates consistently between text measurement and painting.

Start with document-level layout/paint dirtiness and per-text layout caches. Do not recreate per-slot service arrays or a multi-stage dependency engine. A dirty layout can update the whole tree initially; optimize only with evidence.

Rendering performs `update_layout(viewport)`, `update_draw_list()`, then `renderer.draw(...)`. Idle rendering reuses geometry, shaping results, and the command buffer. Viewport changes invalidate layout. Zero-sized viewports submit no drawing.

Input rules:

- Resolve current geometry before hit testing. Select the topmost eligible widget using reverse paint order.
- Track hover and pointer capture through weak widget references.
- Only primary-button press/release activates buttons. Release must satisfy the documented captured-button/inside-target rule.
- Cancel, host focus loss, disabling, and subtree removal clear ineligible capture/hover state.
- Event callbacks may change text/style immediately. Queue structural edits during dispatch and apply them before the next event, including events in the same batch.
- Hold the active event target alive until its callback returns. Reject recursive dispatch.
- Apply queued edits when unwinding a callback exception, restore dispatcher state, then propagate the original exception; structural edit validation must occur when requested and again if preceding edits changed topology.
- Measurement and painting cannot mutate the preparing tree or recursively prepare it. Enforce a small scoped phase guard.
- Catch measurement exceptions at the Yoga C boundary and rethrow after returning to C++. Failed preparation stays dirty and is retryable.

### 5. Text and draw-list lifetime

Keep FreeType and HarfBuzz; simplify their integration rather than rewriting font shaping.

- A CPU font service belongs to the document by default. Default system-font discovery happens lazily. Explicit font resources may be shared.
- Preserve UTF-8 byte clusters, LF/CRLF handling, ASCII-space wrapping, line alignment, long-word overflow, and clipping to text content bounds and viewport.
- Store immutable CPU glyph images under stable resource identities; cache eviction cannot invalidate a live prepared draw list.
- Draw lists are document-owned and borrowed synchronously by renderers. They are valid until the next preparation or document destruction. No public retained-frame promise.
- Commands own or retain all referenced CPU resource data. Avoid dangling `string_view` metadata; omit inspection metadata unless needed.
- Use one closed variant for portable drawing primitives, initially rectangles and positioned glyph runs with explicit clip rectangles. A closed drawing vocabulary is acceptable; a closed widget vocabulary is not.
- New widgets must express painting through this vocabulary. New rendering primitives, if later needed, require deliberately extending all backends.
- CPU/GPU cache ownership and bounds must be explicit. A basic bounded cache is sufficient; document that live draw resources can exceed cached ownership budgets.

### 6. Renderer and host interfaces

Define one public non-template base:

```cpp
class renderer {
public:
    virtual ~renderer() = default;
    virtual centui::viewport viewport() const noexcept = 0;
    virtual void draw(const draw_list&) = 0;
};
```

Both `sfml::backend` and `bgfx::backend` derive from it. `draw` consumes the list synchronously; a backend must copy/retain anything required beyond that call. Backends upload CPU images and draw commands. They never choose fonts, shape text, measure widgets, or traverse the tree.

`ui` becomes a non-template facade borrowing `renderer&`, preserving `centui::ui ui{backend};`, `render(document&)`, and event forwarding. Do not use `if constexpr` backend detection or adapter chains.

Hosts own clearing, presentation, graphics contexts, and native event translation. Keep `app` as an optional convenient SDL/bgfx host with `app.run(document)` as used in `main.cpp`. Destroy GPU resources before the graphics context. SFML rendering must restore the borrowed target's view and relevant state.

Coordinates use top-left origin and downward Y. Commands use straight RGBA; each backend must implement the appropriate blending and identical clipping/glyph placement.

## Module layout and deletion policy

Task 1 records the exact acyclic import graph before coding. Suggested substantive boundaries:

- `color`, `unit`: keep these actual foundational modules because `main.cpp` imports them; put viewport/geometry in `unit` or another justified foundation.
- `style`: reflected style resolution, patching, validation.
- `render`: commands, painter, renderer interface; depends on resource data types, not document.
- `font`: font resources, shaping and CPU glyph cache.
- `widget`: base, handles, normalized input types, extension contexts.
- `container`, `text`, `button`: built-in facades and implementations.
- `document`: tree operations, input, preparation, Yoga integration.
- `ui`: small facade; may be exported alongside document if not retaining its standalone import.
- `sfml`, `bgfx`: backend modules.
- `app`: optional host.
- Optional `centui` umbrella exporting public modules without adding behavior.

Native implementation helpers may live in ordinary private source files. File merging is not itself simplification. Keep separate files when they make substantial algorithms easier to understand.

Remove superseded `model`, `runtime`, `node`, `layout_engine`, `preparation`, `primitives`, `text_service`, `glyph`, `painter`, `draw`, `dispatcher`, `diagnostics`, and `canvas` modules after migrating their required behavior. Their responsibilities must be accounted for, not simply deleted. Update all other examples and tests to the new contracts. Preserve `main.cpp` unchanged.

## Ordered implementation tasks

### Task 0 — Baseline and acceptance inventory

Depends on: nothing. Scope: inspection, build setup, baseline evidence.

1. Read repository instructions and this plan. Check existing uncommitted changes; do not overwrite unrelated work.
2. Record compiler, standard library, CMake, and Ninja versions. Verify modules, `<meta>`, reflection expressions, and expansion statements with a small compile probe.
3. Record a SHA-256 hash of `examples/main.cpp`. Save the hash in `docs/rewrite-status.md`.
4. Try the existing headless build/tests. Record exact failures; do not claim a passing baseline if unavailable.
5. Capture the current example visually if it runs. Otherwise record the expected dimensions/colors/order from its source and defer visual comparison until runnable.
6. Inventory existing tests: retained product behavior, replaced lifetime contracts, and obsolete compatibility assertions. Record the mapping rather than blanket-deleting tests.

Done when: status document contains evidence, baseline limitations, and test inventory. A reflection toolchain failure blocks implementation validation; report it explicitly instead of substituting C++20 techniques.

### Task 1 — Freeze declarations and build boundaries

Depends on: Task 0. Scope: public declaration draft, module graph, CMake.

1. Write exact declarations for geometry/constraints, handles, widget hooks, painter, glyph resources, draw list, renderer, document operations, and weak-handle behavior in `docs/rewrite-contracts.md`.
2. Resolve header/module dependency cycles on paper. Resource types cannot depend on widgets; widgets cannot import document implementation.
3. Specify `append`, `remove`, and root-replacement signatures and deferred-operation semantics. Do not expose implementation storage.
4. Document derived-widget setters, typed access, child composition, default measurement, and painter coordinates with short examples.
5. Separate core, backend, and optional host CMake targets with explicit source lists. Retain existing build option names. Core must not link window/graphics libraries.
6. Freeze these declarations before dispatching downstream tasks. Downstream models must not independently rename them.

Done when: a reviewer can implement each subsystem from the declarations without guessing lifetime or ownership. Do not create fake successful stubs for unimplemented behavior.

### Task 2 — Foundation and reflected styles

Depends on: Task 1. Scope: foundation/style modules and focused tests.

Implement style composition, nested overrides, patches, validation, and dirty classification. Keep required enums/defaults/units. Use compile-fail fixtures for unknown fields and incompatible types, plus runtime tests for atomic failed patches and equal writes.

Done when: all four style structs in `main.cpp` compile unchanged; later style packs override earlier ones; partial state overrides preserve defaults.

### Task 3 — Widget ownership and extensibility

Depends on: Task 2. Scope: widget base, handles, tree primitives, container facade.

Implement the frozen lifetime contract and weak document-context notification. Add tests for copied/moved handles, nested temporaries, duplicate/cyclic parenting rejection, detachment with surviving handles, destruction, weak locking, and custom derived/composite objects. Include the single-child nested-container construction case so brace initialization does not accidentally copy a container instead of nesting it.

Done when: composing a tree does not allocate per-widget runtimes or migrate object state; destruction tests prove no framework-created ownership cycles.

### Task 4 — Text resources and portable drawing

Depends on: Tasks 1–2. Scope: font/render modules; may run independently of Task 3 after declarations are frozen.

Implement one shaping/cache service and the command/painter/renderer contracts. Reuse proven FreeType/HarfBuzz algorithms from the existing source where useful, removing legacy ownership wrappers. Test UTF-8 clusters, multiline/wrapping/alignment, clipping data, font identity, and resource survival through cache eviction while the list remains live.

Done when: a fake renderer can consume complete portable commands without accessing fonts or tree internals. No raw-node/span/frame alternate API exists.

### Task 5 — Document, Yoga, built-ins, and input

Depends on: Tasks 3–4. Scope: document, text/button facades, UI facade.

Implement geometry preparation, dirty reuse, text measurement, ordered painting, pointer state, and deferred structural changes. Implement the button using the same public extension hooks as custom widgets; use a private label child only if it simplifies behavior without adding hidden-node machinery.

Tests must cover resize, no-op frames, text edits, paint-only edits, disabled controls, overlap hit order, release outside, cancellation, focus loss, removal during callback, root replacement between events, callback exceptions, recursive dispatch rejection, and preparation exception recovery.

Done when: unchanged `main.cpp` compiles against the new core/app declarations; a headless equivalent reproduces its layout and click-driven updates. No core switch discriminates built-in widget kinds.

### Task 6 — Backends and application host

Depends on: Task 5. Scope: both backends and app.

Port both backends to the exact shared renderer base. Remove backend font ownership and raw-node drawing. Preserve GPU teardown order, bounded upload caching, SFML state restoration, glyph placement, alpha blending, and clipping. Port native event translation and retain the default app constructor/run syntax.

Done when: both backends build, consume identical draw commands, and pass visual smoke checks. Verify resize, overlapping translucent rectangles, clipped glyphs, hover/press/disabled states, and shutdown.

### Task 7 — External examples and deletion

Depends on: Task 6. Scope: examples, stale modules, tests, documentation.

1. Replace `custom_backend.cpp` with a minimal subclass of `renderer` implementing only the required contract.
2. Replace `custom_primitive.cpp` with an external derived widget example showing measurement, painting, and a setter that invalidates the correct work.
3. Update SFML example imports/integration as needed. Do not edit `main.cpp`.
4. Delete replaced modules, stale imports, compatibility tests, and abandoned CMake paths. No second implementation may remain linked or parked in the source tree.
5. Rewrite README around actual supported contracts and short examples. Explain lifetime/weak captures and synchronous draw-list lifetime.

Done when: every old behavior in the Task 0 inventory is either tested under the new design or explicitly marked intentionally removed under this plan.

### Task 8 — Final integration and size review

Depends on: Task 7. Scope: verification and justified fixes only.

1. Run clean headless, SFML-only, bgfx-without-app, and SDL/bgfx-app configurations in separate build directories.
2. Run tests and compile-fail fixtures with the supported reflection compiler. Run lifetime checks with sanitizers where that toolchain supports them.
3. Verify the `main.cpp` hash is unchanged. Run it and record screenshots/observations against the baseline or the documented expected appearance.
4. Measure implementation lines using the same nonblank-line definition as the baseline, including any new `.cpp`/headers so moving code cannot game the count.
5. Inspect the final import graph and source for legacy names, duplicate preparation paths, unchecked widget casts, per-backend text shaping, and undocumented resource lifetimes.
6. Record actual build/test results, line/module counts, and remaining limitations. Never replace missing validation with “should work.”

Done when: success criteria are demonstrated. Missing compiler or graphics access must be reported as a remaining verification blocker, not marked complete.

## Suggested verification commands

Use the verified reflection toolchain file/compiler settings from Task 0 with each configure command. The commands below omit those environment-specific arguments.

```powershell
cmake -S . -B out/rewrite-headless -G Ninja -DCENTUI_BUILD_BGFX=OFF -DCENTUI_BUILD_SFML=OFF -DCENTUI_BUILD_APP=OFF -DCENTUI_BUILD_TESTS=ON
cmake --build out/rewrite-headless
ctest --test-dir out/rewrite-headless --output-on-failure

cmake -S . -B out/rewrite-sfml -G Ninja -DCENTUI_BUILD_BGFX=OFF -DCENTUI_BUILD_SFML=ON -DCENTUI_BUILD_APP=OFF
cmake --build out/rewrite-sfml

cmake -S . -B out/rewrite-bgfx -G Ninja -DCENTUI_BUILD_BGFX=ON -DCENTUI_BUILD_SFML=OFF -DCENTUI_BUILD_APP=OFF
cmake --build out/rewrite-bgfx

cmake -S . -B out/rewrite-app -G Ninja -DCENTUI_BUILD_BGFX=ON -DCENTUI_BUILD_SFML=OFF -DCENTUI_BUILD_APP=ON
cmake --build out/rewrite-app
```

## Dispatch instructions for each implementing model

Copy this template into each task assignment:

> Implement Task N from IMPLEMENTATION_PLAN.md. Read the architectural contracts, docs/rewrite-contracts.md, and docs/rewrite-status.md first. Work only within this task's scope and prerequisites. Preserve examples/main.cpp byte-for-byte. Do not retain compatibility adapters, introduce alternative object models, or rename frozen interfaces. Use clear names and ordinary RAII; do not minimize line count through dense formatting. Run relevant checks. Update docs/rewrite-status.md with changed files, exact commands/results, unresolved issues, and the next task's handoff. If blocked by a toolchain or contract conflict, provide concrete evidence rather than inventing a fallback architecture.

Dispatch sequentially by default. Only Tasks 3 and 4 are independently assignable after Task 2 and the declaration freeze; assign exclusive file ownership if run concurrently. Do not dispatch multiple models to redesign the same interfaces.

Each handoff must list:

- Completed behavior and changed files.
- Tests run and actual outcomes.
- Any contract deviation, with justification; unresolved deviations block dependent tasks.
- Remaining TODOs and which task owns them.
- Confirmation that `main.cpp` remains unchanged.

## Concise architecture summary

Keep the declarative user API. Replace the internals with stable shared widget objects, weak parent links, typed handles, reflected styles, a small virtual widget base, one document preparation pipeline, and one virtual renderer interface. Keep Yoga/FreeType/HarfBuzz. Remove legacy runtime migration and competing drawing contracts. Prove correctness before judging the reduction in code.
