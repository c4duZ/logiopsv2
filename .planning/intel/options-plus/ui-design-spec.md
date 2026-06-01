# Options+ UI Design Spec — Owned Re-Implementation Target (REF-02)

> **Reference-only design study — see [legal-boundary.md](./legal-boundary.md).**
> This is OUR OWN re-implementation target, written from a *study* of the extracted
> Options+ `app.asar` (mapped by [asar-inventory.md](./asar-inventory.md)) plus the
> readable `LogiOptionsPlus/data/`. The extracted archive is **gitignored, never
> bundled, never copied into `src/`**. No Logitech HTML/CSS/JS/art reaches the shipped
> app. We re-implement everything in our own QML/strings (see [vocabulary.md](./vocabulary.md)).
>
> **Bundles are minified-bundled React** (per the inventory): we can *grep-locate* a
> screen by its surviving identifiers but cannot read its layout from the JS. So this
> spec describes the **tab/layout/interaction model** in our own words — framed as
> alignment deltas against the QML we already shipped in Phases 2–4 — not a pixel copy.

---

## 0. Source basis (what was studied, where it lives)

Paths are into the extracted tree
`.planning/phases/04.1-options-plus-reference-mining/asar-extract/` (STUDY-ONLY, gitignored),
per [asar-inventory.md](./asar-inventory.md).

| Concern | Extracted location | Confirmed readable anchor (grep) |
|---------|--------------------|----------------------------------|
| App shell / windows | `main.js` (1.0M, least-minified — creates the `BrowserWindow`s) | window→entry map: `index.html` (main), `cc.html` (config/overlay companion) |
| Main config window (device UI) | `index.html` + `app.min.js` (10.5M, `<div id="root">`) | `DeviceTab` / `deviceTab`, `buttonsTab` |
| Gesture builder screen | `app.min.js` (also `cc.min.js`) | `GestureConfiguration`, `closeGestureConfiguration` |
| Schema-driven config panels | `package.json` deps: `@rjsf/*` (react-jsonschema-form) | paired with readable `LogiOptionsPlus/data/*.json` |
| Demo animations / device art | `assets/` (538 content-hashed files: 73 gif, 158 svg, 278 png) | hashes — identify by opening, not by name |

Strings/vocabulary come from the **readable** `LogiOptionsPlus/data/strings/*.yaml`, already
distilled in [vocabulary.md](./vocabulary.md) — NOT from the string-split keys in `app.min.js`.

Our current QML (the re-implementation surface this spec aligns to) lives under
`src/logiops-gui/qml/` — notably `Main.qml`, `DeviceList.qml`, `DetailPane.qml`,
`config/ConfigTabs.qml`, the four tabs (`ButtonsTab`, `PointerTab`, `ScrollTab`,
`ProfilesTab`), `config/ReassignPanel.qml`, `config/GestureBuilder.qml`, and `Theme.qml`.

---

## 1. App shell / navigation

Options+ presents a **single primary window** (`index.html` → `app.min.js`) plus a separate
**configuration/overlay companion window** (`cc.html` → `cc.min.js`, which carries the
`#modal-root` + `#toast-top-right-root` layers — i.e. modal + toast surfaces). The marketplace
is a third, out-of-scope window.

The primary window's layout, as the study reveals, is a classic **two-pane master/detail**:

- **Left: device rail.** A list/strip of the user's connected devices (device render thumbnail
  + name). Selecting a device drives the right pane. Logitech also surfaces account/marketplace
  affordances here that are **out of scope** for us (no Flow/cloud — see PROJECT.md).
- **Right: detail pane** for the selected device, organized as a **horizontal tab strip** over
  a content body, with a persistent device header (device render + name + status) above the tabs.

**Our alignment (already built):** `Main.qml` is exactly this two-pane shell —
`DeviceList.qml` (left, `Theme.sidebarWidth` ≈ 280) + `DetailPane.qml` (right). `DetailPane`
already renders a persistent header (icon, name, model sub-line, live connection/battery line)
above a sliding `TabBar` hosting a cross-fading `ConfigTabs` StackLayout. So the shell is a
**match**; the deltas live below in the tab inventory and the gesture interaction.

> **Modal + toast layers note (from `cc.html`):** Options+ uses a dedicated modal root and a
> top-right toast root. We currently use a **non-modal side panel** (`ReassignPanel`) for
> reassignment and confirmation dialogs (`RestoreDialog`). Decision retained: prefer the
> non-modal side panel for assignment (less disruptive than Options+' modal); reserve a
> toast/inline surface for transient save feedback (`SaveToolbar`). This is an intentional
> divergence, not a gap.

---

## 2. Tab model (per-device detail tabs)

Options+ organizes a device's settings into a **horizontal tab strip** whose membership is
**capability-driven** — a device only shows the tabs for features it has. From the study +
the readable data, the recurring Options+ tab families for a mouse/keyboard are:

| Options+ tab family (our wording) | What it contains | Our current tab |
|-----------------------------------|------------------|-----------------|
| **Buttons** | Device render with clickable button hotspots → per-button assignment (incl. **Gestures** as one assignable category). | `ButtonsTab` (`DeviceRender` hotspots + `BindingList` + `ReassignPanel`) ✅ |
| **Point & Scroll** (Options+ groups pointer + scroll; we split into two) | DPI/sensitivity, scroll direction/speed, SmartShift ratchet↔free-spin, hi-res, thumb wheel. | `PointerTab` (DPI + cycle) + `ScrollTab` (SmartShift / hi-res / thumbwheel) ✅ (split into 2) |
| **Gestures** | Options+ exposes gestures as an **assignment category on a button** (`ASSIGNMENT_NAME_GESTURE`), not always a standalone tab. The per-direction HOLD+MOVE editor opens from there. | We embed the gesture builder **inside** `ReassignPanel` (a "Gesture" category) → `GestureBuilder`. Same model: gesture is an assignment of a button. ✅ |
| **Smart Actions / Macros** | Composite if/then step builder (`MACROS_*`). | Not yet (Phase 7). 🔜 |
| **Backlight** | Brightness levels + effects. | Not yet (Phase 8, greenfield). 🔜 |
| **Profiles / per-app** | Profile create/switch/rename + per-application binding. | `ProfilesTab` (manual create/switch/rename, PROF-01). Per-app auto-switch is Phase 5. ✅ (manual) / 🔜 (per-app) |

**Tab ordering** we ship (left→right): **Buttons → Pointer → Scroll → Profiles** (see
`ConfigTabs.qml` `tabKeys: ["buttons","pointer","scroll","profiles"]`), extended later with
**Smart Actions** and **Backlight** when those phases land. This mirrors Options+'
"interaction first (buttons/gestures), then tuning (pointer/scroll), then organization
(profiles)" reading order.

**Capability omission rule (load-bearing):** Both Options+ and our UI **omit** a tab entirely
when the device lacks the capability — they do **not** show a disabled tab. Our
`DetailPane`/`ConfigTabs` already implement this via the `tabKeys` list computed from
`DeviceController` capability flags (UI-01), keeping StackLayout indices in lock-step with the
visible `TabButton`s. **Match.**

---

## 3. Interaction model

The interactions worth re-implementing faithfully (the rest is layout, covered above):

### 3.1 Button render → assignment

Options+ shows a **device render** with clickable **button hotspots**; clicking a hotspot opens
an **assignment surface** offering categorized actions (keystroke, system, gesture, etc.) plus a
preview of the current binding. Confirmed by the `DeviceTab`/`buttonsTab` anchors in `app.min.js`
and the device-render PNGs/SVGs in `assets/`.

**Our alignment:** `DeviceRender.qml` (synced hotspots) + `BindingList.qml` (current bindings) +
non-modal `ReassignPanel.qml` (7 categories, live key-capture, device-driven host slots). The
model is the **same** (button → category → action); we present it as a **side panel** rather than
a modal. ✅ (with the intentional non-modal divergence from §1).

### 3.2 Gesture builder flow (the marquee interaction)

This is the interaction Phase 4.2 must align to. Options+' model (confirmed via the
`GestureConfiguration`/`closeGestureConfiguration` anchors in `app.min.js`, and fully distilled
in [vocabulary.md](./vocabulary.md) §1):

1. A **gesture button** is one assignment category on a button (`ASSIGNMENT_NAME_GESTURE`).
2. The user faces a **preset-or-custom card** (`GESTURE_CARD_DESCRIPTION`): pick a ready-made
   **predefinição** (preset), OR choose **Personalizada/Custom** (`ASSIGNMENT_NAME_CUSTOM_GESTURE`)
   to build their own.
3. Custom opens a **per-direction editor**: each of four **HOLD + MOVE** directions
   (`GESTURE_ACTION_HOLD_MOVE_{UP,DOWN,LEFT,RIGHT}`) **plus a plain CLICK**
   (`GESTURE_ACTION_CLICK`) gets an individually assigned action. This maps 1:1 onto the daemon's
   per-direction `Gesture` map (`src/logid/actions/gesture/`).
4. A custom config can be **saved as the user's own preset** ("criar sua própria predefinição").
5. Inline **help/feedback** describes the mechanic ("Hold the button and move the mouse" /
   `GESTURE_INFO_HOLD_MOVE_DESCRIPTION`); demo **GIFs** in `assets/` illustrate each direction.

**Our alignment (Phase 4 built, Phase 4.2 to align):** `GestureBuilder.qml` already renders a
**4-cardinal direction cross → mode pills → action sub-section → granularity slider with a human
readout → live preview card → Clear** — all driven by the C++ `GestureModel` (QML renders only).
**Deltas to close in Phase 4.2** (also tracked in `.planning/debug/gesture-live-apply-and-save.md`
for the daemon side):
- We do **not yet** present the **preset-or-custom card** before the per-direction editor — we
  drop the user straight into Custom. Add the predefinição/personalizada two-mode framing
  (Phase 4.2 plan `04.2-03`).
- Wording must adopt the mined Options+ vocabulary ("SEGURAR + MOVER PARA CIMA", etc.) from
  vocabulary.md §1.1/§4 — we ship our own re-worded strings, not Logitech's verbatim copy.
- Read-back: the builder must reflect an **already-configured** gesture's mode + bound action
  (GEST-01) instead of prompting "choose what this direction does" on a configured button.

### 3.3 The recurring "preset vs custom" pattern

Beyond gestures, Options+ reuses a **predefinição-or-personalizada** duality across configurable
controls (the user picks a curated preset OR builds a custom one — vocabulary.md §4). Our
re-implementation keeps this two-mode framing wherever a control has presets (gesture presets,
DPI cycle presets via `DpiCycleEditor`), shipping our **own** preset content per legal-boundary.

---

## 4. Alignment deltas vs our current QML (the concrete consumer value)

| Options+ does… | We currently do… | Action / phase |
|----------------|------------------|----------------|
| Two-pane shell: device rail + detail pane w/ header + tabs | `Main.qml` = `DeviceList` + `DetailPane` (header + sliding `TabBar` + `ConfigTabs`) | **Match** — no action |
| Capability-driven tab membership (omit absent) | `DetailPane`/`ConfigTabs` omit via `tabKeys` from capability flags | **Match** — no action |
| Combined "Point & Scroll" tab | We split into **Pointer** + **Scroll** | **Intentional**: keep split (clearer sections); revisit only if it feels heavy |
| Modal assignment + top-right toast (`cc.html` roots) | Non-modal `ReassignPanel` side panel + `SaveToolbar` | **Intentional divergence** — keep non-modal; use toast/inline for save feedback |
| Gesture: **preset-or-custom card** then per-direction editor | We drop straight into the per-direction Custom editor | **Phase 4.2 (`04.2-03`)**: add the predefinição/personalizada card |
| Gesture: reads back existing binding | Builder prompts "choose what this direction does" even when configured | **Phase 4.2 (`04.2-02`)**: GestureModel read-back resolves bound action (GEST-01) |
| Gesture wording: "SEGURAR + MOVER PARA…", HOLD+MOVE/CLICK | Generic mode pills | **Phase 4.2 (`04.2-03`)**: adopt mined vocabulary (our own strings) |
| **Smart Actions** if/then tab | absent | **Phase 7** — schema in [smart-action-schema.md](./smart-action-schema.md) |
| **Backlight** tab | absent | **Phase 8** (greenfield) |
| **Per-app** profile auto-switch | manual profiles only (`ProfilesTab`) | **Phase 5** — model in [app-match-model.md](./app-match-model.md) |
| **Action wheel** radial overlay | absent | **Phase 6** — see [overlay-osd-spec.md](./overlay-osd-spec.md) |

---

## 5. Visual language note (our own words — feel, not pixels)

Described so we can **re-create the feel** in our own QML/`Theme.qml`; no asset is copied, no
class rule lifted (the `app.min.css` rules are mangled — color/spacing reference only, study live).

- **Calm, content-first surface.** A light/neutral canvas with white cards and 1px hairline
  dividers; a single saturated accent (blue family) reserved for selection/focus/CTA. Our
  `Theme.qml` already encodes this (light/dark `dominant`/`secondary`/`hairline`/`accent` pairs,
  12% `accentTint` selection, ~6% `hoverTint`). **Match the restraint** — one accent, no
  competing hues; destructive actions get the only second semantic color.
- **8-point spacing rhythm**, generous padding, large hit targets (≥44px). `Theme.spacing*` +
  `rowHeight: 56`, `tabBarHeight: 48`, `gestureDirCell: 44` already enforce this. **Match.**
- **Restrained typography**: a small size set + two weights (regular/medium). `Theme` ships
  exactly 4 sizes / 2 weights. **Match.**
- **Soft, quick motion** for tab cross-fades, panel slide-in, and the gesture preview — short
  (120–250ms), ease-out, no overshoot/bounce. `Theme.motion*` (`150/200/220ms`, `OutCubic`)
  already encodes this budget. **Match** — keep motion subtle; the gesture demo GIFs in `assets/`
  set the bar for *animation as explanation*, which we re-create with our own preview card and
  (later) our own illustrations, never their GIFs.
- **Animation-as-explanation** is the one Options+ trait we should deliberately invest in: each
  gesture direction is illustrated. We re-create this with our own live preview card
  (`GestureBuilder` already has a first-class preview) and, if desired, our own simple QML/Lottie
  illustrations — authored by us, never the extracted GIFs.

---

## 6. Asset boundary (explicit)

We re-create all UI in our own QML and our own strings/illustrations. We do **NOT** bundle or
copy into `src/`: the extracted `app.asar` HTML/CSS/JS, the content-hashed `assets/` (PNG/SVG/GIF/
fonts), `app.min.css`/`cc.min.css`, or any Options+ string YAML. This spec cites the extracted
tree only as a study reference (gitignored, never shipped) — enforced by the **Phase 9 BLOCKING
legal-asset audit** (see [legal-boundary.md](./legal-boundary.md)).
