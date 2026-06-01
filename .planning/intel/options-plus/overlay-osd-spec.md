# Options+ Overlay / OSD Design Spec — Owned Re-Implementation Target (REF-01 / REF-02)

> **Reference-only design study — see [legal-boundary.md](./legal-boundary.md).**
> No Logitech overlay art bundled. This is OUR OWN radial/OSD interaction + layout
> reference, written from the **readable** `LogiOptionsPlus/data/overlay/` JSON plus a
> *study* of the extracted `app.asar` (mapped by [asar-inventory.md](./asar-inventory.md),
> gitignored, never shipped). The `overlay/icons/` art and `osd_resources.json` are
> **NOT copied into `src/`**; we re-create every overlay visual in our own QML.
>
> This is the design target for **Phase 6 (Action Wheel)** and the **gesture OSD**
> (Phase 4.2). Per the locked architecture: the **daemon detects/highlights from HID++
> deltas and executes; the GUI renders only.**

---

## 1. OSD resource model (distilled from the readable overlay JSON)

Source (readable, gitignored, **not bundled**):
- `LogiOptionsPlus/data/overlay/osd_resources.json` (5122 bytes)
- `LogiOptionsPlus/data/overlay/notification_resources.json` (108 bytes)
- `LogiOptionsPlus/data/overlay/icons/` (22 PNGs — study only, never copied)

Options+ drives small **on-screen feedback notifications** (OSDs) from a flat JSON model.
The shape we adapt (our own re-implementation; we do **not** ship their JSON or icons):

### 1.1 `osd_resources.json` structure

- **`lock_notifications[]`** — toggle-state OSDs. Each entry:
  - `id` (int), optional `platform` (e.g. `"win"` — Windows-only entries we **drop** on Linux),
  - an **`on`** object and an **`off`** object, each `{ title, subtitle, icon }`.
  - Examples in the file: Caps Lock (id 2), Num Lock (id 3, `platform:"win"`), Fn Lock (id 4),
    Scroll Lock (id 5, `platform:"win"`), Media Keys↔F-Keys (id 6), Show Mode (id 10).
  - **Our mapping:** these are key/lock-state feedback toasts. Linux-relevant ones (Caps Lock,
    Fn Lock, Media/F-keys toggle) re-created in our own OSD with our own glyphs; `platform:"win"`
    entries are skipped on Linux.
- **`backlighting_notifications`** — `{ on: [...], off: {...} }`. The `on` array carries either a
  **`level`** (0–7 brightness steps) or a **`backlight_effect`** id (with the other field set to
  the sentinel `-999`), each `{ title, subtitle, icon }`. This is the **Phase 8 backlight OSD**
  model (brightness step + effect-name feedback). Cited here; consumed by Phase 8.
- **`mute_osd`** — `{ title, subtitle, icon: "mute.png" }`. A single mute-state OSD.

### 1.2 `notification_resources.json` structure

A minimal model — currently just **`low_battery`**: `{ title:"", subtitle:"Low battery",
icon:"battery.png" }`. So "notifications" = transient system alerts (low battery), distinct from
the toggle-state `lock_notifications` OSDs. **Our mapping:** a low-battery toast surfaced by the
GUI from the daemon's existing battery feature (we already model battery live, Phase 2) — our own
copy + glyph.

### 1.3 The OSD entry contract we adopt (re-authored)

A generic OSD entry our overlay renderer consumes is: **icon + title + optional subtitle**, with
toggle OSDs carrying an `on`/`off` pair. We re-author the strings (per [vocabulary.md](./vocabulary.md))
and draw our own glyphs — the icon *names* in the JSON (`caps_lock_on.png`, `mute.png`,
`battery.png`, `backlighting_N.png`) tell us **which states need a glyph**, not which art to ship.

---

## 2. Radial / action-wheel interaction (Phase 6 target: WHEEL-01/02/03)

> The `cc.min.js`/`520.min.js`/`46.min.js` chunks are minified; per
> [asar-inventory.md](./asar-inventory.md) **most `osd`/`radial`/`wheel` grep tokens there are
> mangled noise** — they confirm the feature exists but are NOT readable names. So the mechanic
> below is described from the *behavior* (the running-app study + the Options+ product model),
> re-expressed as our own re-implementation target. The `package.json` `fabric-guideline-plugin`
> (canvas) dep is the likely radial/overlay editor — evidence the wheel is a canvas-drawn radial.

The action wheel is a **radial menu** the user pops at the cursor and selects from by direction:

1. **Trigger (WHEEL-02):** a configured button **press** opens the wheel. The wheel is bound as a
   button assignment (same model as any other action), so it is configured from the **Buttons**
   tab's reassign surface, gaining a "Radial / Action Wheel" category.
2. **Layout (WHEEL-01):** **N slices** arranged around a center, each slice **bound to an action**
   (keystroke, system, profile/host switch, etc. — reusing the existing action vocabulary). Slice
   count is user-configurable; slices are evenly distributed around the circle. A center/dead-zone
   represents "no selection / cancel".
3. **Selection mechanic (WHEEL-02):** **flick toward a slice → release to fire.** While the button
   is held, the user moves the pointer toward a slice; the slice under the current **flick angle**
   highlights; **releasing the button fires the highlighted slice's action** (releasing inside the
   dead-zone cancels). This maps to: the **daemon** computes the active slice from raw HID++ XY
   deltas (angle → slice index) and fires on release; it emits an **`ActiveSlice`** signal so the
   **GUI overlay only renders** the highlight (no input logic in the GUI). This is exactly the
   Phase 6 split recorded in ROADMAP `06-02` (daemon flick-angle detection + `ActiveSlice` signal)
   and `06-04` (GUI overlay driven by `ActiveSlice`).
4. **Placement (WHEEL-03):** the overlay is **follow-the-cursor** — it appears centered at the
   pointer position when active, so the flick is relative to where the cursor already is. It must
   be **click-through** (input passes to the daemon's detection, not captured by the overlay
   window). This is the high-risk piece gated by the Phase 6 X11-vs-Wayland overlay spike
   (`06-01`).

**Re-implementation framing (ours):** daemon = detection + execution + `ActiveSlice`; GUI =
a transparent, click-through, follow-cursor `RadialOverlay.qml` that draws N slices and highlights
the active one. The **feel** (snappy open, clear highlight, fire-on-release) we re-create in our
own QML — no Logitech canvas/art reused.

---

## 3. Gesture OSD

During a gesture (held gesture button + move), Options+ shows transient on-screen feedback for the
direction/action being triggered — tied to the gesture vocabulary (`GESTURE_INFO_HOLD_MOVE_*`,
"Hold the button and move the mouse"; see [vocabulary.md](./vocabulary.md) §1.4) and the demo GIFs
in `assets/` that illustrate each direction.

**Our re-implementation (ties to Phase 4.2):** a lightweight gesture OSD — same overlay machinery
as the action wheel (transparent, follow-cursor or centered) — that shows the **active direction +
the plain-language action** as the user holds-and-moves, mirroring the `GestureBuilder` preview
sentence but at runtime. The daemon already knows the active direction; the GUI renders the OSD.
This reuses the Phase 6 overlay platform, so the gesture OSD and the action wheel share one
overlay backend.

---

## 4. Graceful degradation (the X11/Wayland reality Phase 6 must handle)

A **follow-the-pointer, click-through** overlay is not uniformly available across Linux
compositors:

- **X11:** override-redirect / shaped input-transparent windows can follow the cursor reliably.
- **Wayland:** depends on the compositor. `wlr-layer-shell` (wlroots: Sway, etc.) and KWin support
  layer-shell overlays; **GNOME-Mutter** historically lacks third-party layer-shell — a
  follow-cursor click-through overlay may be impossible there. Cursor *position* may also not be
  queryable without a portal.

**Degradation rule (WHEEL-03):** where the compositor **cannot** render a follow-the-pointer
overlay (or expose cursor position), fall back to a **centered pop-up** radial drawn at screen
center instead of at the cursor — the selection mechanic (flick-angle → release) still works
because the daemon computes the slice from relative XY deltas, independent of where the overlay is
drawn. The Phase 6 spike (`06-01`) produces the per-compositor go/no-go matrix and the chosen
technique; this spec mandates the **centered-pop-up fallback** as the floor so the feature degrades
rather than fails. The same fallback applies to the gesture OSD.

---

## 5. Asset boundary (explicit)

We re-create all overlay/OSD visuals — radial slices, OSD cards, glyphs, gesture feedback — in our
**own QML with our own art/strings**. We do **NOT** bundle or copy into `src/`:

- `LogiOptionsPlus/data/overlay/icons/*.png` (the 22 lock/backlight/mute/battery glyphs),
- `LogiOptionsPlus/data/overlay/osd_resources.json` / `notification_resources.json`,
- any extracted `app.asar` overlay code/canvas/art.

The JSON is mined here only as a **structure/model** reference (the schema-vs-content distinction
in [legal-boundary.md](./legal-boundary.md)): we adapt the *shape* (icon + title + subtitle;
on/off toggle pairs; level/effect backlight model; N-slice radial) into our own spec and ship our
own re-authored content. Enforced by the **Phase 9 BLOCKING legal-asset audit**.
