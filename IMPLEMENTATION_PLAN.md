# CentUI architecture implementation plan

This is an ordered implementation brief for Codex. Implement stages sequentially, completing each stage's acceptance checks before continuing. The goal is a document-owned UI with stable typed handles, controlled mutation, incremental preparation, shared CPU text services, and independent graphics renderers.

## Execution rules

- Inspect the current source and applicable repository instructions before editing. This plan describes the intended architecture; adapt names and implementation details to the actual code.
- Preserve user changes. `ARCHITECTURE.md` was already deleted in the working tree when this plan was written; do not restore it automatically.
- Review the recorded baseline below. Existing build artifacts are not evidence that current sources compile; builds/tests remain deferred under the user's instruction.
- Keep each stage buildable. Update tests and examples with the implementation, and record completed tasks and any justified deviations in this file.
- Prefer narrow interfaces and explicit ownership. Do not introduce a general entity-component system, large virtual widget hierarchy, reactive framework, or asynchronous renderer to accomplish these tasks.
- Preserve declarative construction and compile-time style composition. Use temporary adapters where needed, then remove obsolete code once callers migrate.
- Current user instruction: do not build anything; implement code only. Record verification commands and defer builds/tests until the user authorizes them. Do not claim unexecuted checks passed.
- Never use `import std` or `import std.compat`. Include the required standard headers explicitly, in the global module fragment for module units and before imports for ordinary source files. Do not enable CMake standard-library module support.
- Keep CMake organized around one `centui` library with `CONFIGURE_DEPENDS` source globs. Backend/app options select source directories and dependencies; do not introduce separate values/input/glyph/core libraries or enumerate source files individually.
- Always use braces for control flow, including single-statement branches and loops. Keep code concrete and readable, with blank lines separating logical operations. Prefer simple data structures and explicit ownership over generic frameworks.

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

Implementation after the stage 3 refactor: `centui.runtime` owns slot records containing authoritative widget content, a binding, parent ID, ordered child IDs, and pending mutation categories. Layout and prepared text are owned separately by runtime services. `text`, `button`, and `container` are copyable typed handles. Copies alias identity; explicit moves transfer the handle and invalidate the source. Construction and mounting use the same registry and traversal. Before mounting, root handle copies share an explicit construction owner; composing named children transfers their records/services and relinquishes their former owner without moving their handles. Child handles expire if the enclosing construction owner dies. Mounting transfers the complete runtime to the document and relinquishes construction ownership. Mounted handles keep only weak access to runtime lifetime. IDs are runtime-local and may be remapped during construction composition; publicly exposed mounted IDs remain stable. Removal invalidates generations; exhausted slots are retired.

Public operations: `document{root}`, `mount`, `root`, `children`, `parent`, `append`, `insert`, `reparent`, `reorder`, and `remove`. `insert` accepts unmounted trees; `reparent` moves existing nodes. Reparent indices refer to the destination list after removing the child. Removing the root empties the document and permits mounting a new tree. `element::valid()` is safe for expired/moved handles; other access throws `invalid_element`. `element::as<text<>>()` and corresponding widget types check the runtime kind. Containers use an initializer-list child constructor: braces compose children, including a single same-type container, while ordinary copying (`auto alias = child`) aliases the handle. The `from_children` workaround is removed.

Preparation and input have document overloads and always traverse runtime IDs, including before mounting. The raw value-node API remains a separate low-level compatibility API for existing callers/tests; public widget construction no longer creates raw value trees. Preparation became incremental in stage 3; mutable style access was replaced in stage 2. Interaction capabilities stay attached to widget content through reparenting; stage 4 moves hover/focus/capture targets into runtime-owned IDs. Existing examples mount documents; the app example updates a nested title through a captured typed handle.

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

Widgets and documents expose read-only `pending_changes()` snapshots. Layout/paint changes propagate to ancestors; per-widget text invalidation stays local. Successful preparation acknowledges topology/text/layout/paint; input preparation acknowledges geometry and eligibility, with visual interaction transitions marking paint. Exceptions leave unfinished stages pending. Stage 3 consumes these flags to skip clean preparation stages.

Thread contract: documents, draft trees, and their widgets must be accessed and destroyed on their creating thread. Public document/handle access and mutations reject a different thread with `std::logic_error`; document moves are checked before transferring ownership. Identity comparison and plain handle copying/moving do not access widget state. Destruction is a caller obligation because destructors cannot report a recoverable thread-access error. No locking or background mutation queue is introduced.

Updated the public mutation callers and added `tests/mutation.test.cpp` covering invalidation, validation, atomic failure, no-op edits, capture cancellation, topology, and thread rejection. The app example uses typed text/style setters. No builds or tests were run, per user instruction.

Acceptance tests: text changes invalidate measurement/layout/paint; width changes invalidate layout/paint; colors invalidate paint only; enabled changes update interaction eligibility; invalid style edits leave prior values usable.

## 3. Implement an incremental preparation pipeline

- [x] Make the document coordinate topology synchronization, text preparation, layout, and paint generation explicitly.
- [x] Keep Yoga nodes and child links attached between frames. Synchronize only changed topology.
- [x] Calculate layout only when document inputs or viewport dimensions require it, relying on Yoga's incremental behavior where appropriate.
- [x] Cache text preparation by its actual inputs. Introduce an internal text-service boundary now; migrate resource ownership in stage 5.
- [x] Reuse unchanged paint output or rebuild it only when needed. Avoid prematurely implementing complex subtree paint caching.
- [x] Remove recursive full-tree `attach()` from the normal render/event path once all required synchronization is covered.
- [x] Add lightweight test instrumentation for topology synchronization, shaping, and layout so incremental behavior is observable.

Implementation after refactoring: `centui.preparation` coordinates concrete services owned by `document_state`. Elements contain identity and construction ownership only. Geometry preparation checks viewport/font inputs and pending mutation categories, asks the layout engine to apply changed styles/measurements and synchronize changed parent lists, and calculates Yoga layout only when necessary. Idle frames skip geometry traversal. Changed geometry currently walks the tree to inspect service caches; Yoga owns its native layout dirtiness and incremental calculation. Subtree preparation uses the owning runtime's geometry and returns the subtree's range from the same command array in document coordinates.

`centui.text_service` owns each node's current shaping key and immutable prepared text. Keys contain text, exact font size, and font identity. `centui.layout_engine` owns Yoga nodes, applied layout styles, native links, and measurement installations. After stage 4, its measurement interface consumes only intrinsic dimensions, with no text-layout dependency. Identical dimensions reuse the installed callback even if shaping/resource identity changed; failed installation remains retryable. Font identity uses an opaque lifetime token independent of native resources; moving a font preserves it, and replacement/address reuse cannot reuse an old key. No font object/reference is retained between calls. Wrapping and other new shaping options must extend the key in stage 5. Each slot retains only its current key/output.

Topology operations no longer detach the entire document. Reparenting releases the previous parent's native child list; removal detaches the affected parent and destroyed subtree before freeing nodes. Preparation releases all dirty parent lists before linking new owners. Construction composition transfers heap-owned records and service entries without relocating content, prepared text, or native nodes. Mounting transfers the entire runtime without rebuilding geometry or paint. Both construction composition and document insertion share one validated transfer operation; allocations and validation precede ownership changes.

`centui.painter` owns the cached command array and subtree ranges. It consumes a preorder list of content, absolute geometry, and prepared text; it has no Yoga or document-storage dependency. Geometry extraction and range construction are linear in tree/slot storage. Paint emission shares a helper with the legacy raw-node painter. Synchronous UI rendering borrows the cached array; public `prepare()` retains its vector return type and copies command records, not text or glyph vectors. Commands still borrow runtime data until mutation/preparation/destruction; owned frames remain stage 6.

`centui.diagnostics` provides optional counters outside nodes. Call `enable_preparation_diagnostics()` on a document or element to observe subsequent work through `preparation_stats()`. Diagnostics are disabled by default; disabling releases counters, and enabling again starts from zero without invalidating caches. Mounting preserves the runtime's existing diagnostics; enabling on an empty document carries the setting into the mounted runtime. Inserting a subtree preserves its prepared resources but does not merge diagnostic histories. Counters survive removal within their runtime; mounting a replacement runtime uses that runtime's counters.

`tests/preparation.test.cpp` covers idle reuse, stage-specific edits, resize, shared input geometry, topology changes, actual input keys, font lifetime/address reuse, failure recovery, mounting/composition, subtree ranges, optional diagnostics, rejected transfers, and slot cache cleanup. Tests remain source specifications: no builds or tests were run.

Source-only verification: `git diff --check` passed; `clang-format --dry-run --Werror --style=file` passed for the refactored modules and preparation tests. A source scan found 27 unique module declarations, resolved every local `centui.*` import, and found no dependency cycles. The formatter now inserts control-flow braces and expands short functions/blocks; its invalid `SpacesInAnglesStyle` key was corrected to `SpacesInAngles`. These checks do not establish compilation or runtime correctness.

Acceptance tests: repeated idle preparation does not rebuild links or reshape text; a color edit does not perform layout; editing one label does not reshape unrelated labels; resize refreshes geometry; repeated mutation and preparation do not retain stale measurements.

## 4. Unify input dispatch with document geometry

### 4A. Establish the primitive/component boundary first

- [x] Represent runtime content using box and text primitives plus an optional generic interaction capability.
- [x] Compose buttons from an interactive box and a private caption while preserving public construction, setters, styles, and typed conversions.
- [x] Keep widget-kind metadata for public handle conversion; preparation, hit testing, and runtime paint do not dispatch on widget kind.
- [x] Put primitive-specific intrinsic preparation, style resolution, and paint emission in `centui.primitives`.
- [x] Remove text-layout resource types from the layout engine's measurement interface.

Runtime box content retains the existing `container_data` name. A button owns a box with an interaction capability and one text child. Caption parts are hidden from public `document.children()` and cannot be modified through public document topology operations; private children still participate in normal runtime traversal, preparation, and destruction. The button's surface is sealed to public child insertion/reordering. `set_text` forwards to its caption, and the primitive adapter resolves inherited font size, text colors, and text alignment. Surface interaction appearance propagates to passive descendants. Padding now positions the caption through ordinary child layout. Existing text alignment/wrapping limitations remain stage 5.

`centui.layout_engine` receives intrinsic dimensions rather than text resources. Shaping remains keyed by text/font inputs; changing a font can reshape without reinstalling a measurement or calculating layout when the resulting dimensions are identical. The source regression for font replacement reflects this distinction. The legacy raw-node API retains its own button representation and painter adapter for compatibility; it does not participate in runtime input.

### 4B. Document-owned sequential input

- [x] Move interaction tracking to runtime-owned stable IDs for hover, focus, and pointer capture.
- [x] Ensure geometry is current before hit testing through the existing preparation coordinator.
- [x] Invoke activation at the release-event boundary and refresh geometry before a subsequent event requires it.
- [x] Permit callbacks to remove themselves, siblings, subtrees, or replace the root/document without retaining node references across application code.
- [x] Define exception behavior and reject recursive dispatch into the same runtime.
- [x] Cancel invalid targets immediately on removal, disabling, and reparenting beneath disabled ancestors; normalize host focus loss and cancellation.
- [x] Refresh viewport dimensions before input, including zero-sized/minimized hosts, and separate viewport refresh from backbuffer reset.
- [x] Define hit-test order consistently with paint order.

`centui.dispatcher` owns one synchronous dispatch session; `document_state::interaction` owns the persistent target IDs and host-focus state. `ui` is a backend adapter forwarding events and rendering. A batch re-resolves the current document/runtime and viewport before each event. Primary press focuses/captures an eligible surface; release activates only when it hits the captured surface. Capture is released before the callback, whose shared callable is retained without copying mutable captures. Later events see callback changes, including root replacement and new viewport dimensions. Pointer exit clears hover but preserves capture; release outside cancels activation. Focus loss or pointer cancellation clears hover, focus, and capture. Focus gain does not restore previous targets. Secondary buttons do not activate surfaces. Focus is currently a pointer-driven visual state; keyboard traversal/activation and accessibility are not implemented.

Hit testing walks children in reverse paint order before their parent. Noninteractive content is transparent to input; disabled interaction capabilities disable their subtrees. Overflow remains visible/hittable consistently with current painting until clipping is implemented. Subtree input scopes clear targets outside that scope.

Callback exceptions propagate, keep completed mutations, stop the remaining batch, and release the dispatch guard. Retained callables allow self-replacement/removal to finish safely. Runtime retirement invalidates handles immediately when a document or construction owner is destroyed/replaced, even while dispatch temporarily retains that runtime. New input into the same runtime during dispatch throws `std::logic_error`; a later independent dispatch remains usable.

SDL translates focus gain/loss and minimizes into normalized events, releases physical capture on focus loss/minimize/release, and refreshes the backend's borrowed viewport before dispatch. Minimized or unavailable dimensions become zero. Backbuffer dimensions are tracked separately so earlier viewport refresh cannot suppress the required graphics resize in `begin_frame()`.

Added `tests/dispatch.test.cpp` covering generic non-button surfaces, document-owned targets across UI adapters, button composition/privacy/typography/colors, sequential geometry and viewport changes, target removal and replacement, root/document replacement, immediate cancellation, pointer exit/release outside, focus loss, zero viewports, overlap order, callback exceptions, recursive dispatch recovery, and reparenting under disabled ancestors. Existing mutable-capture/self-replacement tests remain applicable. These are source specifications; no build or tests were run under the user's instruction.

Acceptance when execution is authorized: run the new dispatch tests and existing document/mutation/preparation tests, then verify real SDL resize/focus/capture behavior. No graphics or runtime success is claimed from source checks.

## 5. Extract a shared CPU text and font service

- Extend `centui.text_service` with font/resource ownership and constrained shaping. Preserve the existing layout engine and painter boundaries; widget content remains authoritative inputs, not a container for prepared resources.
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

- Evolve `centui.painter` output into the owned frame contract. Keep frame construction outside element handles and keep graphics backend state outside the runtime's layout/text services.
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

The initial box/text/interaction boundary and button composition were pulled forward into stage 4. Application component APIs and new primitive extension remain outstanding.

- [x] Define a small runtime capability boundary for layout participation, intrinsic measurement, paint generation, and input/focus behavior.
- [x] Replace button-specific branching in generic input and painting with generic interactive-surface behavior.
- [x] Implement a button as a component combining label content, an interactive surface, state-dependent styling, and activation behavior.
- [ ] Let application components compose existing elements while exposing typed handles and setters without editing the core node variant.
- [ ] Provide a narrow extension interface for genuinely new measurement/paint primitives, using the document's mutation and lifetime rules.
- [x] Avoid rebuilding a large virtual widget inheritance hierarchy. Choose capability representation based on actual primitive requirements.
- [x] Explicitly document currently supported interaction capabilities; do not imply keyboard accessibility from pointer-only focus state.

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
