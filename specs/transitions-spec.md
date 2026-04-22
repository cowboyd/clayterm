# Clayterm Transitions Specification

**Version:** 0.1 (draft) **Status:** Design specification for a not-yet-implemented
feature. Normative where it establishes invariants and contract. Descriptive
where surfaces may settle during implementation.

---

## 1. Purpose

A transition smoothly interpolates an element's visual properties over time.
This specification defines how transitions integrate with Clayterm's frame-snapshot
rendering model: how they are declared, how time is supplied, how enter and
exit behaviors are expressed, and how callers observe in-flight animation so
they can drive the render loop.

Transitions are a first-class extension of the rendering contract defined in
the [Clayterm Renderer Specification](renderer-spec.md). They do not change
the architectural model, do not introduce a component tree, and do not require
callers to hold cross-frame identity beyond the stable element identifiers they
already use.

---

## 2. Scope

### In scope (normative)

- The transition model and its relationship to the frame-snapshot rendering contract
- Time handling and the `deltaTime` convention
- The animating signal returned from `render()`
- The declarative enter and exit model (no callbacks across the WASM boundary)
- Element identity requirements for transitions
- Cancellation semantics (as a consequence of the frame-snapshot model)

### In scope (non-normative, descriptive)

- The shape of the `transition` field on the `open()` directive (shorthand and longhand)
- The set of easing functions exposed in the initial surface
- The wire encoding of transition data in the directive buffer
- Interaction with line mode
- Testing strategy

### Out of scope

- Custom (JavaScript-authored) easing functions. Reserved for a future extension;
  the enum space is designed not to preclude them.
- Proportional reversal (CSS-style dynamic shortening of duration when a
  transition is cancelled mid-flight).
- Physics-based animation, spring interpolation, or keyframe sequences.
- Any framework-level concept of "animation groups," "timelines," or choreography
  across multiple elements. Orchestration is a caller concern.
- Input parsing (see [Input Specification](input-spec.md)).

---

## 3. Terminology

**Transition.** A time-based interpolation of one or more of an element's
visual properties between an initial value and a target value.

**Transition property.** A specific visual attribute of an element that can be
interpolated: position (x, y), size (width, height), background color, overlay
color, border color, border width, or corner radius.

**Easing.** A function mapping normalized progress in [0, 1] to an eased value
in [0, 1]. Clayterm exposes a fixed set of built-in easings.

**Transition state.** One of four modes an element can be in with respect to a
given transition: idle, entering (element newly mounted), transitioning
(property target changed on an existing element), or exiting (element removed
from the tree but still being animated out).

**Enter transition.** The animation played when an element first appears in the
directive tree. Its initial state is derived from the element's target state
by applying caller-supplied deltas (e.g., offset position, transparent color).

**Exit transition.** The animation played when an element disappears from the
directive tree. Its final state is derived from the element's last-seen state
by applying caller-supplied deltas. The element is still rendered during its
exit even though it is no longer in the directive tree.

**Delta time (`deltaTime`).** The number of seconds elapsed since the previous
render transaction. Used by the renderer to advance interpolation.

**Animating signal.** A boolean flag in the render result indicating whether
any transition is currently in progress. Callers use it to decide whether to
schedule another frame.

---

## 4. Architectural Model

_This section is normative._

### 4.1 Relationship to the frame-snapshot model

Transitions do not alter the frame-snapshot contract defined in INV-3 of the
renderer specification. The directive array still fully describes the desired
state for its frame. Transitions interpolate between the previous frame's
state and the current frame's target state; they do not reintroduce a
persistent component tree on the caller side.

What transitions add is the requirement that element identifiers remain stable
across frames for any element on which animation is desired. This is not a new
invariant — the existing pointer-event subsystem already relies on stable
identifiers — but it becomes load-bearing for transitions.

### 4.2 Time ownership

The `Term` instance is the sole source of frame-to-frame time. On each
`render()` call, the Term reads a monotonic clock and computes the elapsed
seconds since the previous render. That value is passed to the layout engine
to advance any in-flight transitions.

The caller MAY override the computed delta via an explicit `deltaTime` option
on `render()`. Use cases include deterministic testing, snapshot rendering,
and compute-only renders where the caller is querying bounds without
displaying output.

The Term MUST NOT use a non-monotonic clock (e.g., `Date.now()`). Wall-clock
time can move backward under NTP adjustments or DST, which would produce
negative deltas and corrupt interpolation.

### 4.3 Delta clamping

Clayterm does not clamp `deltaTime`. Long gaps between frames (process
suspension, backgrounded terminal, debugger pause) produce large deltas. The
underlying interpolation is duration-based and naturally clamps at 1.0 of
progress, so a large delta causes in-flight transitions to complete rather
than to overshoot or become unstable.

This differs from physics-based engines, which clamp deltas to prevent
tunneling. Transitions as specified here are not physics-based, so clamping
is unnecessary.

### 4.4 Animation-loop signaling

The render result MUST surface whether any transition is currently active.
Callers use this signal to schedule the next frame. When no transition is
active, callers may stop rendering until the next external event (input,
resize, application state change).

This requirement exists because terminal applications typically render
on-demand rather than at a fixed refresh rate. Without an explicit animating
signal, a caller has no way to know that a transition it triggered is still
in progress.

### 4.5 Boundary preservation

Transitions MUST NOT require function pointers, callbacks, or other
non-serializable values to cross the TS→WASM boundary. Easing and
enter/exit initial-state computation are implemented on the C side using
declarative configuration carried in the directive buffer.

This preserves INV-2 (single transaction per frame): one binary buffer in,
one result struct out.

---

## 5. Core Invariants

_This section is normative._

**INV-T1. Time is driven by delta, not wall clock.** All transition
interpolation advances by `deltaTime`, a per-frame seconds value. The
renderer does not subscribe to an internal timer or schedule work of its own.

**INV-T2. Render remains pure under time override.** When the caller supplies
an explicit `deltaTime`, the render result depends only on the directive
array, the previous frame's cell buffer, and the supplied `deltaTime`. This
makes deterministic rendering possible for tests and snapshots.

**INV-T3. No callbacks across the boundary.** Transition configuration MUST
be fully serializable. No function pointers, closures, or callback registries
cross the TS→WASM boundary during a render transaction.

**INV-T4. Identity is drawn from element IDs.** Transition state is associated
with elements by their declared `id`. Callers using transitions on an element
MUST assign it a stable, unique `id` across frames. Reusing an `id` for a
different logical element in a later frame is a caller error; behavior is
unspecified.

**INV-T5. Animating signal is accurate per transaction.** The `animating`
flag returned by `render()` reflects the state of transitions as of the end
of that transaction. If it is `true`, at least one transition has non-zero
remaining progress and calling `render()` again with positive `deltaTime`
will advance it.

**INV-T6. Cancellation is structural.** There is no imperative `cancel()`
API. Transitions are cancelled by re-describing the previous target in a
later frame; the transition infrastructure re-anchors the interpolation from
the current visible value to the new target.

---

## 6. Rendering Contract Additions

_This section is normative._

### 6.1 `render()` signature

The `render()` method accepts an optional `deltaTime` field in its options
argument:

```
render(ops: Op[], options?: RenderOptions): RenderResult

interface RenderOptions {
  mode?: "line";
  row?: number;
  pointer?: { x, y, down };
  deltaTime?: number;   // seconds; overrides Term's internal clock
}
```

Each `render()` call advances transitions by its `deltaTime`:

- If `deltaTime` is omitted, Term computes it as the monotonic wall-clock
  time elapsed since the previous `render()` call.
- If `deltaTime` is provided, it is used verbatim for that frame.

On every `render()` call, Term captures the current monotonic timestamp as
the reference point for the next implicit delta. The two modes can be
freely mixed, but mixing within a single session is primarily useful for
tests that step time manually and should otherwise be avoided.

### 6.2 `RenderResult` addition

The render result gains one field:

```
interface RenderResult {
  output: Uint8Array;
  events: PointerEvent[];
  info: RenderInfo;
  errors: ClayError[];
  animating: boolean;     // NEW
}
```

`animating` is `true` if and only if at least one element has an in-flight
transition at the end of the transaction.

### 6.3 The `transition` field on `open()`

An element may declare a transition by adding a `transition` field to its
open-element directive. The field is optional. Its absence means the element
has no transitions, which is the default.

The field accepts either shorthand or longhand form (Section 7).

---

## 7. Declarative Transition Surface

_This section is descriptive. The shapes may be revised during implementation,
but the architectural commitments above do not change._

### 7.1 Shorthand form

All listed properties share one duration and one easing:

```ts
open("sidebar", {
  layout: { width: fixed(20) },
  bg: rgba(30, 30, 30, 255),
  transition: {
    duration: 0.2,
    easing: easeOut(),
    properties: ["x", "width", "bg"],
  },
})
```

### 7.2 Longhand form

Each property declares its own duration and easing independently:

```ts
open("sidebar", {
  transition: [
    { property: "x",     duration: 0.3,  easing: easeInOut() },
    { property: "width", duration: 0.3,  easing: easeInOut() },
    { property: "bg",    duration: 0.15, easing: easeOut()   },
  ],
})
```

The shorthand form is expanded to longhand during directive packing. The wire
encoding carries only longhand.

### 7.3 Extended form (enter, exit, interaction handling)

```ts
open("toast", {
  transition: {
    properties: [
      { property: "y",  duration: 0.25, easing: easeOut() },
      { property: "bg", duration: 0.15, easing: linear()  },
    ],
    enter: {
      independently: false,
      from: { y: -2, bg: rgba(0, 0, 0, 0) },
    },
    exit: {
      independently: false,
      to: { y: -2, bg: rgba(0, 0, 0, 0) },
      paintOrder: "natural",
    },
    interactive: false,
  },
})
```

**`enter.from`** declares deltas relative to the element's target state. The
initial state used by the enter transition is `target + from`. A missing
`from` entry for a given property means the enter transition starts at the
target value for that property (no visible animation on that axis).

**`exit.to`** declares deltas relative to the element's last-seen state.
The final state used by the exit transition is `initial + to`.

**`enter.independently` / `exit.independently`** (default `false`) control
whether the element's enter/exit plays when its parent is also entering or
exiting in the same frame. The default couples the element to its parent:
child elements do not play their own enter/exit when the parent is itself
entering or exiting (this prevents cascaded animations when an entire
container mounts or unmounts). Setting `independently: true` opts in to
playing the animation unconditionally.

**`exit.paintOrder`** controls how an exiting element is drawn relative to
its reflowing siblings during the exit animation. One of:

- `"natural"` (default) — paints in the element's natural DOM order.
- `"underSiblings"` — paints beneath siblings; reflowing neighbors cover the
  exiting element.
- `"overSiblings"` — paints on top of siblings; the exiting element remains
  visually prominent until its animation completes.

**`interactive`** (default `false`) — when `false`, pointer interactions
with the element are disabled while a position transition is in progress.
When `true`, pointer interactions remain enabled throughout position
transitions.

### 7.4 Easing helpers

Exported from the top-level module:

```ts
linear()
easeIn()
easeOut()
easeInOut()
cubicBezier(x1: number, y1: number, x2: number, y2: number)
```

Each returns an `Easing` value: a tagged byte with optional parameters. The
easing enum space is deliberately larger than the current surface to allow
future additions (including a potential `custom()` form that bridges to a
JavaScript function) without breaking serialized frames.

### 7.5 Property names

```ts
type TransitionProperty =
  | "x" | "y" | "position"
  | "width" | "height" | "size"
  | "bg" | "overlay" | "borderColor"
  | "cornerRadius" | "borderWidth"
  | "all";
```

Group names (`position`, `size`, `all`) expand to the underlying property
set during packing and are equivalent to listing the constituent properties
explicitly in longhand form.

---

## 8. Wire Encoding

_This section is descriptive._

The transition block is a new optional tagged section on `OP_OPEN_ELEMENT`.
Its presence is indicated in the element's property bitmask (existing
mechanism for optional fields). When present, its layout is:

```
transition_block {
  flags: u8                          // bit 0: enter present
                                     // bit 1: exit present
                                     // bit 2: interactive (0 = disabled, 1 = enabled)
  entry_count: u8                    // number of property_transition entries
  entries: property_transition[]     // entry_count entries, in stable property order
  enter?: transition_side            // present iff flags bit 0
  exit?: transition_side             // present iff flags bit 1
}

property_transition {
  property: u16                      // single-bit mask from Clay's property enum
  duration: f32                      // seconds, non-negative
  easing: u8                         // easing kind
  params: f32[0 or 4]                // 4 floats iff easing == cubicBezier
}

transition_side {
  flags: u8                          // bit 0: independently
                                     // bits 1-2: paintOrder (exit only: 0 natural, 1 under, 2 over)
  mask: u16                          // which properties have deltas
  values: bytes                      // packed in stable property order; widths per property
}
```

Value widths are property-specific: `f32` for position and size, `u32` for
colors, `u8[4]` for border widths, `u8[4]` for corner radii (8-bit
resolution per corner is consistent with the existing cornerRadius
encoding).

The shorthand form is never present on the wire. TS fans shorthand out to
per-property longhand entries before packing. The C side sees only longhand.

### 8.1 Validation

The existing `validate()` utility gains checks:

- `duration >= 0` for every entry.
- `easing` is one of the defined enum values.
- Property names in entries are valid and appear at most once.
- Property names in `enter.from` / `exit.to` are a subset of the entries
  (deltas for a property not being transitioned are ignored or flagged).

---

## 9. Cancellation Semantics

_This section is normative._

A caller cancels an in-flight transition by emitting a new frame whose
directive for that element describes a different target state. The
transition infrastructure re-anchors the interpolation:

- The new `initial` value becomes the element's currently-visible value.
- `elapsedTime` resets to zero.
- The new `target` is the value declared in the current frame.

The transition duration is unchanged. A cancelled-and-reversed transition
takes its full configured duration regardless of how far it had progressed
at the time of cancellation.

There is no `term.cancelTransition(id)` call. The frame-snapshot model
makes cancellation a structural consequence of re-describing the desired
state rather than an imperative operation.

---

## 10. Interaction with Line Mode

_This section is descriptive; the concrete behavior will be finalized
during implementation._

Line mode emits cells as newline-separated rows without absolute cursor
positioning. Position transitions (`x`, `y`) have no meaningful effect in
this mode: the rendering output places each row at the current cursor,
not at absolute coordinates.

Expected behavior in line mode:

- Color and size transitions proceed normally.
- Position transitions are silently skipped (treated as if the property is
  not being transitioned for that frame).
- Enter/exit transitions that declare `from` or `to` deltas on position
  properties have those position deltas dropped; other delta properties
  still apply.

The `animating` signal reports accurately regardless of mode; line-mode
color or size transitions still report as animating.

---

## 11. Testing Strategy

_This section is descriptive._

The `deltaTime` override enables deterministic, snapshot-friendly tests.
A test sequence looks like:

```ts
term.render(opsA, { deltaTime: 0 });
term.render(opsB, { deltaTime: 0 });      // target change, no time elapsed
term.render(opsB, { deltaTime: 0.1 });    // 50% through a 0.2s transition
term.render(opsB, { deltaTime: 0.1 });    // 100%, completed
```

Test coverage should include, at minimum:

- Shorthand and longhand produce identical output for equivalent configs.
- Enter transitions with `independently: true` and `false`.
- Exit transitions with each `paintOrder` value.
- Cancellation: target change mid-flight re-anchors initial to current.
- Re-appearance during an exit transition.
- Transition config present one frame and absent the next.
- Multiple concurrent transitions on a single element (longhand).
- Multiple concurrent transitions on multiple elements.
- Line mode rendering: color and size transitions apply, position transitions
  are silently skipped.

---

## 12. Implementation Notes

_This section is descriptive and may change without affecting contract._

### 12.1 Clay submodule version

clayterm currently pins Clay at commit `76ec363`. The transition API was
introduced upstream in commit `ee192f4`, with follow-up bug fixes. Before
implementing transitions, the Clay submodule must be advanced to a post-
`ee192f4` commit. Non-transition Clay changes introduced between the current
pin and the target pin — notably the `Clay_OnHover` signature change and the
element ID scheme split — require an audit of existing clayterm integration.

Upgrading Clay is a prerequisite and should be treated as its own commit
ahead of transition work.

### 12.2 Handler architecture

Each `Term` registers a single C-side transition handler with Clay.
Per-element transition metadata (per-property duration, easing, easing
params, enter deltas, exit deltas) is stored in a side table keyed by
Clay element ID, owned by the Term's context.

The handler:

1. Resolves the active Term context.
2. Looks up metadata for the element by its Clay ID.
3. For each property in the active bitmask, computes local progress as
   `clamp(elapsedTime / property.duration, 0, 1)`, applies the property's
   easing, writes the interpolated value into the output struct.
4. Increments the Term context's `animating_count`.
5. Returns `true` if any property's local progress is below 1.0.

At the start of each `render()`, the Term resets its `animating_count` to
zero. At the end, the value is copied into the result struct as the
`animating` flag (true if count > 0).

The `setInitialState` and `setFinalState` callbacks Clay expects are
implemented as fixed C functions that apply the per-element `from` / `to`
deltas from the side table to the target / initial state Clay passes in.

### 12.3 Per-element storage lifetime

Metadata is repopulated each frame during directive unpacking. Clay's
handler is invoked synchronously inside `Clay_EndLayout`, so per-frame
metadata remains valid when the handler fires. No metadata needs to persist
across frames on our side; Clay's internal hashmap persists the actual
transition state (elapsed time, current value, state machine phase).

### 12.4 Multiple Term instances

`animating_count` and the metadata side table live on the Term's C-side
context, not as module-level state. Multiple Terms created in the same
process remain isolated.

---

## 13. Open Questions

These items remain undecided and will be resolved during implementation.
They do not affect the contract.

### 13.1 First-frame delta

On the very first `render()` after `createTerm()`, there is no previous
frame to compute a delta against. Clay's own behavior on its first
`Clay_EndLayout(deltaTime)` call (with a non-zero delta) is the source of
truth: clayterm will pass through whatever delta it has computed and adopt
whatever Clay does. Verification and documentation occur during
integration.

### 13.2 Mid-transition target change

The cancellation semantics in Section 9 require that a target change
mid-flight re-anchors `initial` to the current visible value. Clay's
`TRANSITIONING` state machine is expected to handle this, but it must be
verified. If Clay does not re-anchor, our handler adds the logic by
tracking the last-seen target per element.

### 13.3 Element re-appearance mid-exit

If an element is exiting and reappears in the next frame's directives,
the expected behavior is to cancel the exit and interpolate from the
current visible state to the new target. Implementation-dependent on Clay.

### 13.4 Transition removed mid-flight

If an element has a transition one frame and the `transition` field is
absent in the next frame, Clay's behavior for in-flight transitions
determines the outcome. Two reasonable options: (a) in-flight transitions
complete using their original config; (b) they freeze at their current
value. Deferred to Clay's observed behavior. Documented once verified.

### 13.5 Custom easing escape hatch

The easing enum space is deliberately larger than the initial surface. A
future `custom()` easing that bridges to a JavaScript function is
anticipated but not specified here. Its design must preserve INV-T3
(no callbacks across the boundary during a render transaction) — likely
via a pre-sampled lookup table supplied in the directive buffer.

---

## 14. Demos

Two demos accompany the feature:

1. **`demo/transitions.ts`** — a clayterm-native demo meaningfully
   exercising transitions in a terminal context (e.g., a collapsing
   sidebar, a list reorder, or a toast notification). Primary purpose:
   surface real-world sharp edges in the API.

2. **A reproduction of Clay's upstream `raylib-transitions` demo** —
   the example that accompanied the Clay transition-API commit
   (`ee192f4`). Primary purpose: provide a reference implementation
   that can be visually compared to upstream, validating that the
   clayterm integration faithfully exercises the full transition API
   surface.

---

## Appendix A. Relationship to the Renderer Specification

This specification extends, but does not modify, the renderer specification.
Specifically:

- **INV-1 (Zero IO).** Transitions introduce reading of a monotonic clock
  for `deltaTime` computation. A clock read is not terminal IO and does
  not violate this invariant. The renderer still produces bytes only; it
  does not read or write terminals.

- **INV-2 (Single transaction per frame).** Transitions preserve this.
  All transition configuration is serialized into the single directive
  buffer; no additional boundary crossings occur during rendering.

- **INV-3 (Frame-snapshot independence).** Transitions preserve this at
  the API level. Each directive array still fully describes the desired
  state. Element IDs carry more weight (Section 4.1) but callers do not
  acquire new cross-frame bookkeeping responsibilities.

- **INV-4 (ANSI byte output).** Unchanged.

- **INV-5 (Layout/render/diff ownership).** The renderer additionally
  owns transition interpolation. Interpolated values feed into the
  existing layout and diff pipeline at the same pipeline stage that
  resolved values would.

The "Deferred/Future Areas" section of the renderer specification should
be updated to remove transitions from its list and to reference this
specification.
