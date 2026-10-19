# CentUI architecture implementation plan

This is an ordered implementation brief for Codex. Implement stages sequentially, completing each stage's acceptance checks before continuing. The goal is a document-owned UI with stable typed handles, controlled mutation, incremental preparation, shared CPU text services, and independent graphics renderers.

## Execution rules

- Inspect the current source and applicable repository instructions before editing. This plan describes the intended architecture; adapt names and implementation details to the actual code.
- Preserve user changes. `ARCHITECTURE.md` was already deleted in the working tree when this plan was written; do not restore it automatically.
- Start with the verification baseline below. Existing build artifacts are not evidence that current sources compile.
- Keep each stage buildable. Update tests and examples with the implementation, and record completed tasks and any justified deviations in this file.
- Prefer narrow interfaces and explicit ownership. Do not introduce a general entity-component system, large virtual widget hierarchy, reactive framework, or asynchronous renderer to accomplish these tasks.
- Preserve declarative construction and compile-time style composition. Use temporary adapters where needed, then remove obsolete code once callers migrate.
- Current user instruction: do not build anything; implement code only. Record verification commands and defer builds/tests until the user authorizes them. Do not claim unexecuted checks passed.
- Never use `import std` or `import std.compat`. Include the required standard headers explicitly, in the global module fragment for module units and before imports for ordinary source files. Do not enable CMake standard-library module support.
- Keep CMake organized around one `centui` library with `CONFIGURE_DEPENDS` source globs. Backend/app options select source directories and dependencies; do not introduce separate values/input/glyph/core libraries or enumerate source files individually.

## Target architecture

```text
Declarative builders + typed handles
                 |
              Document
     ownership / mutation / identity
        /          |           \
   Text service   Layout      Input dispatcher
        \          |           /
             Paint frame
                 |
       bgfx / SFML / custom renderer

Host adapter -> viewport and normalized events
App -> assembles host, document, services, and renderer
```

The document owns the mounted UI. Handles identify widgets without owning their lifetime. Layout and input use the same current geometry. A CPU text service owns font and glyph resources. A paint frame owns its command data and retains referenced immutable resources. Renderers own graphics resources and consume frames.

## 0. Establish a trustworthy baseline

- [x] Inspect modules, public APIs, dependencies, examples, and test sources.
- [x] Remove the missing test include from `tests/node.test.cpp`. The replacement `.inc` tests were removed at the user's request.
- [x] Configure and build in a fresh build directory. The headless build completed before the user's instruction to stop building; tests were not run.
- [ ] Verify headless core, bgfx/app, and SFML configurations where the installed environment supports them. State any unavailable configuration explicitly.

Status: the broken test include is removed. The replacement `.inc` tests, build baseline documentation, and CMake presets were removed at the user's request. Runtime tests and graphics verification are deferred under the user's no-build instruction. Both graphics configurations encountered a network failure fetching SFML's SheenBidi dependency; no graphics build success is claimed.

Acceptance when verification is authorized: a clean source build and its discovered tests agree.

## 1. Introduce stable identity and document ownership

- [x] Add a `document` that owns mounted nodes and their lifetimes.
- [x] Introduce node IDs containing an index and generation, backed by a slot map or equivalent storage. Reused storage must not resurrect expired handles.
- [x] Represent parent and child relationships through stable IDs rather than addresses in child vectors.
- [x] Introduce typed widget handles. Define how unmounted construction state becomes document-owned while existing handles retain identity. Keep this transition explicit and avoid ownership cycles.
- [x] Define invalid-handle behavior, document destruction behavior, and wrong-widget-type access. Prefer explicit checked failure over undefined behavior.
- [x] Preserve nested declarative syntax. Specify whether builders are consumed and handles are copied; do not silently move from ordinary lvalue widget objects.
- [x] Support subtree destruction, child insertion/removal/reordering, and reparenting through the document.
- [x] Reject cycles, duplicate child ownership, and cross-document parenting before changing topology.
- [x] Associate Yoga state and interaction identity with stable node IDs. Add adapters so current preparation can operate during migration.

Implementation: `centui.document` owns slot records, each containing a node payload, layout, binding, parent ID, and ordered child IDs. `text`, `button`, and `container` are copyable typed handles. Copies alias identity; explicit moves transfer the handle and invalidate the source. Before mounting, the top-level construction handle owns the value tree; composing named children transfers construction ownership without moving their handles. Child handles expire if that construction owner dies. Mounting transfers ownership to the document; handles then retain only a binding with a weak document reference. IDs are local to a document. Removal invalidates generations; exhausted slots are retired.

Public operations: `document{root}`, `mount`, `root`, `children`, `parent`, `append`, `insert`, `reparent`, `reorder`, and `remove`. `insert` accepts unmounted trees; `reparent` moves existing nodes. Reparent indices refer to the destination list after removing the child. Removing the root empties the document and permits mounting a new tree. `element::valid()` is safe for expired/moved handles; other access throws `invalid_element`. `element::as<text<>>()` and corresponding widget types check the runtime kind. Containers use an initializer-list child constructor: braces compose children, including a single same-type container, while ordinary copying (`auto alias = child`) aliases the handle. The `from_children` workaround is removed.

Preparation and input have document overloads and traverse mounted IDs. The raw value-node API remains a construction/test adapter; it is not mounted topology. Full preparation remains until stage 3; mutable style access was replaced in stage 2. Pointer interaction state stays attached to its slot payload through reparenting; the document dispatch redesign remains stage 4. Existing examples mount documents; the app example updates a nested title through a captured typed handle.

Added `tests/document.test.cpp` for ownership, handle expiry, generation reuse, move semantics, topology validation, preparation, reparented input, and callback removal. Container regressions cover same-type single children, several identical styled nesting levels, named and temporary children, class template argument deduction, empty/mixed trees, ordinary handle copying, and duplicate rejection. No builds or tests were run for these changes, per user instruction; acceptance behavior is specified in source tests but remains unverified at runtime.

Acceptance tests: nested text can be edited through its original handle; a composed button can be disabled; reorder and storage growth preserve handles; removal and document destruction invalidate handles; reparenting preserves identity; invalid topology changes leave the tree intact.

## 2. Centralize mutation and validation

- [x] Route public changes through typed setters and document operations, including text, enabled state, style, and topology.
- [x] Replace unrestricted mutable `style&` exposure with validated style patches or another explicit edit operation.
- [x] Keep compile-time style resolution as an initial-value facility. Runtime changes must use the same validation and invalidation rules regardless of how initial styles were supplied.
- [x] Validate a mutation before publishing it. Rejected changes must leave document state and dirty tracking consistent.
- [x] Define and track separate invalidation for topology, text shaping/measurement, layout, paint, and interaction eligibility.
- [x] Centralize mappings from changed properties to affected stages. Color-only edits must not reshape text or rebuild topology.
- [x] Define the document's thread ownership contract. Keep the implementation single-threaded unless an existing requirement demands more.

Implementation: `style()` returns a const reference. `patch_style({.width = px(100)})` applies supplied properties to a detached candidate; `set_style(style)` replaces the full value. A patch replaces an entire supplied edges/state-colors property. Unspecified properties are preserved. Both paths validate before publication, using the same `style::validate()` invoked after compile-time style composition and during node construction/adoption. Lengths must be finite with valid units, dimensions/padding/gap nonnegative, fit values zero, font sizes positive/finite/representable, and enum values in their declared domains. Margins and insets may be negative. Invalid patches preserve values, prepared data, and pending changes.

Text and button labels expose `set_text` (the existing `value(string)` delegates to it). Buttons retain `set_enabled` and add `set_on_click`. Equal property writes do not invalidate; disabling clears interaction state. `style::changes_from()` centralizes style-to-stage mapping. `invalidation` tracks topology, text, layout, paint, and interaction eligibility. Text changes mark text/layout/paint; geometry marks layout/paint; colors and alignment mark paint; enabled state marks eligibility/paint; callbacks mark eligibility. Topology operations mark their parent and invalidate ancestor layout/paint, including both paths on reparenting. Reordering an unchanged sequence is a no-op.

Widgets and documents expose read-only `pending_changes()` snapshots. Layout/paint changes propagate to ancestors; per-widget text invalidation stays local. Successful preparation acknowledges topology/text/layout/paint; input preparation acknowledges geometry and eligibility, with visual interaction transitions marking paint. Exceptions leave unfinished stages pending. These flags are the foundation for stage 3: the existing preparation path still performs full preparation and does not yet skip clean stages.

Thread contract: documents, draft trees, and their widgets must be accessed and destroyed on their creating thread. Public document/handle access and mutations reject a different thread with `std::logic_error`; document moves are checked before transferring ownership. Identity comparison and plain handle copying/moving do not access widget state. Destruction is a caller obligation because destructors cannot report a recoverable thread-access error. No locking or background mutation queue is introduced.

Updated the public mutation callers and added `tests/mutation.test.cpp` covering invalidation, validation, atomic failure, no-op edits, capture cancellation, topology, and thread rejection. The app example uses typed text/style setters. No builds or tests were run, per user instruction.

Acceptance tests: text changes invalidate measurement/layout/paint; width changes invalidate layout/paint; colors invalidate paint only; enabled changes update interaction eligibility; invalid style edits leave prior values usable.

## 3. Implement an incremental preparation pipeline

- [ ] Make the document coordinate topology synchronization, text preparation, layout, and paint generation explicitly.
- [ ] Keep Yoga nodes and child links attached between frames. Synchronize only changed topology.
- [ ] Calculate layout only when document inputs or viewport dimensions require it, relying on Yoga's incremental behavior where appropriate.
- [ ] Cache text preparation by its actual inputs. Introduce an internal text-service boundary now; migrate resource ownership in stage 5.
- [ ] Reuse unchanged paint output or rebuild it only when needed. Avoid prematurely implementing complex subtree paint caching.
- [ ] Remove recursive full-tree `attach()` from the normal render/event path once all required synchronization is covered.
- [ ] Add lightweight test instrumentation for topology synchronization, shaping, and layout so incremental behavior is observable.

Acceptance tests: repeated idle preparation does not rebuild links or reshape text; a color edit does not perform layout; editing one label does not reshape unrelated labels; resize refreshes geometry; repeated mutation and preparation do not retain stale measurements.

## 4. Unify input dispatch with document geometry

- [ ] Move interaction tracking to document-owned stable IDs for hover, focus, and pointer capture.
- [ ] Ensure geometry is current before hit testing, using the preparation pipeline rather than a separate layout path.
- [ ] Process events in order. Invoke an activation callback at its defined event boundary; apply resulting mutations and refresh affected geometry before the next event needs it.
- [ ] Make callbacks safe when they remove their own node, another target, or an entire subtree. Do not retain node references across callbacks.
- [ ] Define callback exception behavior and ensure dispatch guards and document state recover consistently.
- [ ] Clear invalid focus/capture/hover on removal, disabling, host focus loss, and cancellation. Add normalized cancellation/focus events where required.
- [ ] Refresh host viewport dimensions before event dispatch, including resize and zero-sized/minimized windows.
- [ ] Specify hit-test order and overlap behavior consistently with paint order.

Acceptance tests: the second event in a batch sees geometry changed by the first callback; self-removal is safe; removed nodes receive no later activation; release outside cancels activation; focus loss clears capture; resize uses current dimensions.

## 5. Extract a shared CPU text and font service

- [ ] Move font ownership, shaping, measurement, and CPU glyph rasterization out of graphics backends into a text service.
- [ ] Introduce explicit font handles and immutable resource identities or revisions. Define replacement and destruction semantics.
- [ ] Make styles reference a font handle, with a configurable default supplied by the host/application. Avoid requiring Windows font discovery for headless or non-Windows use.
- [ ] Key shaping caches by font identity/revision, text, size, and relevant shaping options and constraints.
- [ ] Key glyph images by font identity/revision, glyph ID, and rasterization parameters. Never assume glyph IDs are globally meaningful.
- [ ] Define constrained measurement, wrapping, newline handling, and alignment consistently between layout and paint. Implement the advertised `text_align` behavior.
- [ ] Retain enough shaping metadata for wrapping correctness; do not infer character boundaries from glyph indices.
- [ ] Define a bounded CPU cache policy. Eviction must release cache ownership without invalidating resources retained by live frames.
- [ ] Remove mutable backend font access and backend text-shaping fallbacks after callers migrate.

Acceptance tests: fonts sharing glyph IDs never share incorrect images; replacing a font cannot reuse stale images; measurement and paint agree for constrained text; newline/wrapping/alignment behavior is covered; explicit fonts work without system discovery.

## 6. Establish an owned paint-frame contract

- [ ] Add an immutable or read-only frame type owning its command data and retaining referenced resources.
- [ ] Emit positioned glyph-image primitives from prepared text. Renderers must not shape text or choose fonts.
- [ ] Store stable glyph-image identity and immutable image data/resources in a form each renderer can upload.
- [ ] Define frame coordinate space, paint order, alpha behavior, viewport dimensions, and clipping semantics.
- [ ] Add clipping commands or per-primitive clip bounds where needed so constrained text paints consistently across backends.
- [ ] Ensure a prepared frame remains usable after document mutation and destruction, and after text-service cache eviction or destruction.
- [ ] Replace borrowed string/layout command references at the public boundary. Share immutable resources rather than copying glyph bitmaps per frame.
- [ ] Keep frame inspection possible for headless tests and logging tools. Do not add asynchronous rendering as part of this stage.

Acceptance tests: frames survive originating tree/service destruction; old frames preserve old content after edits; glyph images remain pinned while referenced; paint and clipping order are deterministic; frame inspection requires no graphics initialization.

## 7. Reduce backends to graphics renderers

- [ ] Migrate bgfx, SFML, and the custom logging example to consume the same frame contract.
- [ ] Limit renderer responsibilities to resource upload/cache, graphics state, drawing, and documented host integration.
- [ ] Key GPU caches by immutable resource identity and define renderer cache cleanup/budget behavior. Document which resources may be released when a frame is no longer retained.
- [ ] Preserve correct graphics-resource destruction before host graphics shutdown.
- [ ] Share CPU-side placement and rasterization logic through frames/text services; retain API-specific upload and drawing code in each backend.
- [ ] Remove renderer dependence on nodes, widget kinds, layout engines, and mutable font objects.
- [ ] Add batching only where measurement demonstrates a need. Preserve paint order and clipping when batching.

Acceptance: both backends render equivalent layout and text placement; custom backends need only the frame contract; font replacement and multiple fonts render correctly; graphics lifecycle checks pass. Visually inspect overlap, alpha, clipping, and text in available real backends.

## 8. Separate components from runtime primitives

- [ ] Define a small runtime capability boundary for layout participation, intrinsic measurement, paint generation, and input/focus behavior.
- [ ] Replace button-specific branching in generic input and painting with generic interactive-surface behavior.
- [ ] Implement a button as a component combining label content, an interactive surface, state-dependent styling, and activation behavior.
- [ ] Let application components compose existing elements while exposing typed handles and setters without editing the core node variant.
- [ ] Provide a narrow extension interface for genuinely new measurement/paint primitives, using the document's mutation and lifetime rules.
- [ ] Avoid rebuilding a large virtual widget inheritance hierarchy. Choose capability representation based on actual primitive requirements.
- [ ] Explicitly document currently supported interaction capabilities; do not imply keyboard accessibility from pointer-only focus state.

Acceptance tests/examples: an application-defined composite widget uses ordinary public APIs; a custom primitive participates in measurement/paint without core switch edits; button activation and visual states use the generic runtime.

## 9. Clarify host integration and build boundaries

- [ ] Keep SDL event translation, window sizing, mouse capture, and graphics initialization in host integration.
- [ ] Keep `app` as a convenience assembly of host, document, text service, and renderer with explicit destruction order.
- [ ] Support embedding through a host-owned frame loop. Publish the order: update viewport, dispatch events, prepare frame, draw, present.
- [x] Gate `src/app` and SDL dependency configuration on `CENTUI_BUILD_APP`, independently of whether bgfx is enabled.
- [ ] Keep optional source directories and dependencies correctly gated within the single `centui` library. Do not make document/text APIs depend on window libraries.
- [ ] Document the required C++26/reflection/module toolchain and provide reproducible configure/build commands.
- [ ] Audit public naming and semantics. In particular, implement genuine fixed positioning or replace the current `position::fixed` mapping to Yoga static positioning with accurately named behavior.

Acceptance: fresh headless configuration needs neither SDL nor graphics libraries; bgfx with app disabled excludes SDL/app; available backend configurations compile; the embedding example uses public APIs only.

## 10. Complete migration and document the resulting API

- [ ] Migrate examples to stable handles, controlled setters, document preparation, and owned frames.
- [ ] Include a small dynamic example where a button updates a nested label and changes child membership.
- [ ] Remove obsolete adapters, borrowed public commands, button-specific runtime paths, and superseded synchronization methods.
- [ ] Document ownership, handle invalidation, mutation rules, frame/resource lifetime, thread confinement, and backend requirements.
- [ ] Run the source-derived test suite and supported configuration matrix from fresh build directories.
- [ ] Summarize completed architecture, verified configurations, remaining limitations, and any plan deviations.

Final acceptance: an application can build a nested UI declaratively, keep typed references, mutate it safely during callbacks, reuse idle preparation, select fonts, retain prepared frames independently, and render through either backend without relying on internal node access.
