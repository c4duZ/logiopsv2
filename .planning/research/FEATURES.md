# Feature Research

**Domain:** Linux desktop GUI configurator for Logitech HID++ mice/keyboards (a faithful clone of Logitech Options+), built on top of the `logiops` daemon.
**Researched:** 2026-05-30
**Confidence:** MEDIUM overall.

> **Research method note (read this first):**
> External research tools (`WebSearch`, `WebFetch`, Brave, Exa, Firecrawl) were **all unavailable in this environment** (permission denied / no API key). The Options+ feature catalog below is therefore reconstructed from **model knowledge of Logitech Options+ as of the Jan 2026 training cutoff**, cross-checked against **verified facts read directly from this repository's codebase and `.planning/` maps**. Every Options+ feature claim is rated:
> - **[HIGH]** — corroborated by verified codebase facts (the logiops side of the mapping is read from source).
> - **[MEDIUM]** — well-established Options+ behavior, stable across versions, but not web-verified this session.
> - **[LOW]** — Options+ detail that may have changed in a recent release; **flag for web re-verification** before it drives a hard requirement.
> The **logiops mapping column** (already-in-logiops / needs-daemon-work / GUI-only) is **HIGH confidence** throughout — it is read from `src/logid/config/schema.h`, `src/logid/actions/`, `src/logid/features/`, and `backend/hidpp20/features/`.

---

## Verified logiops baseline (ground truth for the mapping)

Read directly from source this session. This is what the GUI can expose **today** with zero daemon work:

| Capability | Where in daemon | Config knobs exposed |
|---|---|---|
| Button remap → action | `actions/`, `config::Button`/`RemapButton` (keyed by control ID `cid`) | one `Action` per button |
| Actions available | `actions/Action.h` | `None`, `Keypress` (key list), `ToggleSmartShift`, `ToggleHiresScroll`, `CycleDPI`, `ChangeDPI` (inc), `ChangeHost`, `ChangeProfile`, `GestureAction` |
| Gestures (per direction on a button or scroll/thumb) | `actions/gesture/`, `config::Gesture` | modes `NoPress`, `Axis`, `OnInterval`/`OnFewPixels`, `OnRelease`, `OnThreshold`; each has `threshold`; Axis adds `axis` + `axis_multiplier`; Interval adds `interval` + nested `action` |
| DPI | `features/DPI`, `AdjustableDPI` HID++ feature | per-profile `dpi` (single int **or list for cycling**); multi-sensor index |
| SmartShift | `features/SmartShift` | `on`, `threshold`, `torque` |
| Hi-res / Hi-res-invert / "target" scroll + scroll gestures up/down | `features/HiresScroll` | `hires`, `invert`, `target`, `up`/`down` gestures |
| Thumb wheel | `features/ThumbWheel`, `ThumbWheel` HID++ | `divert`, `invert`, `left`/`right` gestures, `proxy`/`touch`/`tap` actions |
| Multi-host switch (Easy-Switch) | `actions/ChangeHostAction`, `ChangeHost` HID++ | `ChangeHost` action (target host index/`next`) |
| Battery / connection status | `features/DeviceStatus`, `WirelessDeviceStatus` HID++ | read-only status (signal over D-Bus) |
| Profiles | `config::Profile`, `config::Device.profiles` + `default_profile` | **named** config sets (dpi/smartshift/hiresscroll/thumbwheel/buttons), switched **manually** via `ChangeProfile` or D-Bus |
| Device naming / reset | `DeviceName`, `Reset` HID++ | name read; reset |
| Live control + config exposure | `ipcgull` D-Bus `pizza.pixl.LogiOps`; `Configuration::save()` writes `/etc/logid.cfg` | full config tree is live-editable over D-Bus |

**Two hard gaps confirmed by source inspection (HIGH confidence):**
1. **No application/window awareness anywhere in the daemon.** Grepping `src/logid/` for `application|window|focus|x11|wayland|active.*app` returns nothing. Profiles are *named*, not *triggered by foreground app*. Per-application profiles need a **new activation layer** (the question is whether it lives in the GUI or the daemon — see that feature).
2. **No backlight / LED feature exists.** No `Backlight`/`LED` HID++ 2.0 feature wrapper in `backend/hidpp20/features/`. Any lighting control is greenfield daemon work.

---

## Feature Landscape

### Table Stakes (Users Expect These)

A configurator that calls itself "Options+ for Linux" feels broken without these.

| Feature | Why Expected | logiops mapping | Complexity | Notes |
|---|---|---|---|---|
| **Device list with live battery + connection status** [HIGH] | First screen of Options+; users open the app to "see my mouse" | **already-in-logiops** (DeviceStatus/WirelessDeviceStatus) + **GUI** | LOW–MED | Battery % + charging + online/offline over D-Bus signals. Multi-device list works but `Device.h:55` hardcodes HID++2.0 — corded/older edge cases may need work. |
| **Visual button remapping** (click a button on a device picture, pick an action) [HIGH] | The single most-used Options+ screen | **already-in-logiops** (RemapButton/actions) + **GUI** | MED | GUI must map control IDs (`cid`) to positions on a per-model device image. Action picker exposes the existing action set. |
| **Keystroke / shortcut assignment** [HIGH] | "Assign Ctrl+C to a button" | **already-in-logiops** (`KeypressAction`, key list) | LOW | GUI needs a key-capture widget → evdev key names. |
| **DPI / pointer-speed slider** [HIGH] | Core mouse setting; Options+ has a sensitivity slider | **already-in-logiops** (DPI feature) + **GUI** | LOW–MED | Daemon supports single DPI or a cycle list. GUI should show a slider + min/max from `AdjustableDPI`. |
| **Scroll settings: SmartShift, hi-res toggle, invert direction** [HIGH] | Options+ "Point & Scroll" tab | **already-in-logiops** (SmartShift, HiresScroll) | LOW–MED | All knobs already in schema (`on`/`threshold`/`torque`, `hires`/`invert`). |
| **Thumbwheel config (MX Master)** [HIGH] | Expected on MX Master hardware | **already-in-logiops** (ThumbWheel) | LOW | Expose divert/invert + left/right/tap actions. |
| **Multi-computer switch button (Easy-Switch / "Flow"-lite)** [MEDIUM] | Easy-Switch button to hop hosts is standard | **already-in-logiops** (ChangeHost) for the *button*; full **Flow** is an anti-feature (below) | LOW (button) | The *host-switch button* works today. Cursor-crossing Flow does not (see anti-features). |
| **Persisting settings / apply without editing text** [HIGH] | Whole reason the project exists | **already-in-logiops** (`Configuration::save()` + live D-Bus) + **GUI** | MED | GUI edits live config then triggers save. Privilege path (root-only D-Bus policy) **must** be solved — see Dependencies. |
| **Per-button action labels / current-binding overview** [HIGH] | Users expect to see what each button does at a glance | **GUI-only** | LOW | Pure presentation over existing config. |
| **Restore-to-default / reset device** [MEDIUM] | Standard escape hatch | **already-in-logiops** (`Reset`) + **GUI** | LOW | |

### Differentiators (Competitive Advantage)

Where this product earns its "+" and directly answers the owner's pain points. These are the reasons to build it instead of hand-editing `logid.cfg`.

| Feature | Value Proposition | logiops mapping | Complexity | Notes |
|---|---|---|---|---|
| **Fine-grained gesture control** [HIGH] | Directly fixes owner pain: "one gesture = only 1 desktop switch", "volume steps by 2". Expose magnitude / repetition / granularity per gesture. | **mostly already-in-logiops, under-exposed** + likely **small daemon polish** | MED | The schema *already* has `axis_multiplier`, `threshold`, and `interval`. The "volume by 2" problem is the chosen action firing twice / coarse multiplier; "only 1 desktop" is a single `OnThreshold`/`OnRelease` fire vs a repeating `OnInterval`. **Much of the fix is GUI-exposing existing knobs + better defaults**, but expect daemon tuning (per-repeat magnitude, sane default thresholds, maybe a "repeat N times" / "steps per trigger" field) to make it feel right. **Single most important differentiator.** |
| **Gesture builder UX** (pick direction → mode → action, with a live "this will fire every X px / once past threshold" explanation) [HIGH] | logiops' gesture model is powerful but opaque in text; a good UI is the value-add | **GUI-only** over existing gesture schema | MED | Translates `Axis`/`Interval`/`Release`/`Threshold` modes into plain language. |
| **Action Ring / radial action wheel** [MEDIUM for Options+ behavior, HIGH for "not in logiops"] | Owner explicitly wants it; flagship Options+ feature; no Linux tool has it | **needs daemon extension** (new) + heavy **GUI** | **HIGH** | See deep-dive below. The daemon has *no* concept of "show an overlay and capture a directional choice." Needs: a new action type that (a) signals the GUI/an overlay to appear, (b) tracks pointer/scroll direction or a second input to choose a slice, (c) fires the chosen sub-action. Overlay is a GUI/compositor concern (X11/Wayland layer-shell). The directional capture is daemon-side (it owns the diverted input). **This is the highest-risk, highest-signature feature.** |
| **Per-application profiles** [MEDIUM for Options+ behavior, HIGH for gap] | Different bindings in Firefox vs an IDE vs a video editor — a headline Options+ capability | **needs new activation layer** (greenfield) + **GUI** | **HIGH** | See deep-dive below. logiops has *named* profiles but **no auto-switch on focus change**. Needs an app-focus watcher (X11 `_NET_ACTIVE_WINDOW` / Wayland — hard, compositor-dependent) that calls the existing `ChangeProfile`. Could live GUI-side (a user-session agent that calls D-Bus `ChangeProfile`) to avoid putting window-watching in the root daemon — **recommended split**. |
| **Smart Actions / macros (multi-step)** [LOW for exact Options+ semantics, HIGH for gap] | Options+ Smart Actions chain keystrokes/launches/delays into one trigger | **needs daemon extension** + **GUI** | **HIGH** | logiops actions are single-shot; there is no "sequence of steps with delays / launch app / open URL." Needs a new composite action (ordered list of sub-actions + delays) and possibly a "launch process / open URL" action (security-sensitive in a root daemon — prefer routing launches to a user-session helper). **Verify exact Options+ Smart Action step types via web before locking scope.** |
| **DPI presets with labels + per-profile sensitivity** [MEDIUM] | Options+ lets you define named DPI stops; logiops only has an unlabeled cycle list | **partly already-in-logiops** (cycle list) + **GUI** (+ maybe schema label field) | MED | Cycling exists (`CycleDPI`); *labels/names* per stop are GUI-side metadata, possibly needing a schema addition if persisted. |
| **Faithful Options+ visual clone / device renders** [MEDIUM] | Explicit owner priority ("não economize no front"); makes it feel native, not a hack | **GUI-only** | MED–HIGH | Per-model device artwork + callout hotspots; the polish budget. |
| **Import/export & shareable profiles** [HIGH] | Power-user delight; trivial given config is a file | **GUI-only** (config already a file) | LOW | |
| **Onboarding that solves the privilege problem invisibly** [HIGH] | If a normal user can't reach the root D-Bus service, nothing works | **needs daemon/policy work** + **GUI** | MED | polkit action or D-Bus policy relax or `USE_USER_BUS`. This is a *requirement*, surfaced here because doing it gracefully is a differentiator vs raw logiops. |

### Anti-Features (Commonly Requested, Often Problematic on Linux)

| Feature | Why Requested | Why Problematic | Alternative |
|---|---|---|---|
| **Logi Flow** (cursor crosses between multiple computers, with clipboard/file transfer) [MEDIUM] | It's a beloved Options+ feature | Requires a cross-machine network service, clipboard/file sync, and a cloud/LAN handshake — none of which logiops does; huge surface, out of v1 scope per PROJECT.md | Ship the **Easy-Switch button** (`ChangeHost`) which covers the hardware host-switch. Point users to `barrier`/`input-leap` for cursor-crossing. |
| **Firmware updates** [HIGH gap] | Options+ updates device firmware | logiops has no firmware/DFU path; bricking risk; explicitly out of scope in PROJECT.md | Detect & link to Logitech's tool; never attempt in-app. |
| **Logitech account / cloud sync / "Logi AI Prompt Builder" / Smart Actions marketplace** [LOW] | Parity completionism | Requires Logitech cloud auth; privacy + dependency on a vendor service; offline Linux users won't want it | Local-only profiles + file import/export. |
| **Backlight / per-key RGB control** [HIGH gap] | Keyboards have it; users ask | **No LED/Backlight HID++ feature exists in the daemon** — greenfield reverse-engineering per device; large effort for a mouse-first audience (owner uses MX Master) | Defer to v2+. If pursued, it's a *new HID++ 2.0 feature wrapper* (`needs-daemon-work`), gated on a target keyboard. Mark as out-of-scope for v1. |
| **Telemetry / "usage insights" / notification center** [MEDIUM] | Options+ shows tips/usage | Privacy-hostile; no value for a local tool; scope creep | Skip entirely. |
| **Auto-update of the app via vendor channel** [LOW] | Convenience | On Debian, packaging (apt) is the update path | Rely on `.deb`/repo per PROJECT.md packaging plan. |
| **Predictive/AI battery estimates** [LOW] | Options+ markets "predictive battery" | Marketing layer over the same raw battery %; not worth modeling | Show raw % + charging state from `WirelessDeviceStatus`; optionally a simple linear estimate GUI-side. |

---

## Deep dives on the four flagged features

### Action Ring / radial action wheel — `needs-daemon-work` + heavy `GUI`, **HIGH complexity**
**What Options+ does [MEDIUM]:** A button press pops a circular on-screen menu of N actions; the user flicks the pointer toward a slice (or scrolls) to pick, release to fire. Slices are user-configured actions (same vocabulary as button actions).
**Why it's hard here:** Two halves that live on opposite sides of the D-Bus seam.
- **Directional capture** must be daemon-side: when the trigger button is held, the daemon already diverts that control and receives motion (cf. `GestureAction` routing `move()` to directional `Gesture`s). A new action type can reuse this to compute a chosen slice (angle bucket) and, on release, dispatch the slice's `BasicAction`. This is a *modest* extension of the existing gesture machinery.
- **The visible ring overlay** must be GUI/compositor-side: a frameless always-on-top window at the cursor. On X11 this is straightforward (override-redirect window); on **Wayland it needs `wlr-layer-shell` / compositor support** and is the real risk. The daemon would emit a D-Bus signal ("ring opened, current slice = k") that a user-session GUI helper renders.
**Recommended decomposition:** (1) daemon: new `RingAction` reusing gesture direction logic + D-Bus signals; (2) session helper: overlay renderer subscribing to those signals. Can ship a **headless v1** (ring works, picks by direction, no visual) then add the overlay — de-risks the Wayland part.
**Dependency:** requires the gesture-direction infrastructure and the privilege/session-helper path.

### Per-application profiles — `needs new activation layer` + `GUI`, **HIGH complexity**
**What Options+ does [MEDIUM]:** Detects the foreground application and silently swaps the active button/scroll/DPI profile; a "default" profile covers everything else.
**logiops reality [HIGH]:** `ChangeProfile` action + named `profiles` + `default_profile` exist, switchable over D-Bus. **The missing piece is purely "who decides which profile is active."** No window/focus awareness exists in the daemon.
**Recommended split:** Put the **app-focus watcher in a user-session agent** (part of the GUI package), not the root daemon — it then calls existing D-Bus `ChangeProfile`. Keeps window-introspection out of the root threat surface and sidesteps the daemon's single-user assumptions.
- **X11:** watch `_NET_ACTIVE_WINDOW` + `WM_CLASS` — reliable, easy. [HIGH]
- **Wayland:** **no portable foreground-app API**; compositor-specific (GNOME extension, KWin script, or `wlr-foreign-toplevel`). This is the hard, possibly-partial case. [MEDIUM] — **flag for phase-level research.**
**Schema impact:** likely a new per-profile field mapping profile → app-match rule (window class / executable / glob). That's a `config/schema.h` addition (small daemon work) **if** persisted there, or kept in GUI-side config if the session agent owns matching.

### Fine-grained gesture control — mostly `already-in-logiops (under-exposed)` + small `daemon polish`, **MEDIUM complexity** — *the priority differentiator*
**Owner pain → root cause → fix (HIGH, from source):**
- *"One gesture = only 1 virtual-desktop switch":* the binding is a single-fire mode (`OnThreshold`/`OnRelease`) so the desktop-switch keystroke fires once per gesture. **Fix:** use/offer `OnInterval` so it repeats every *interval* pixels, or add an explicit "repeat / steps-per-gesture" concept. Mostly GUI-exposure of `interval` + better defaults; possibly a new "N times" knob.
- *"Volume steps by 2":* the volume keypress fires twice (coarse `interval`/threshold or a doubled multiplier). **Fix:** expose `interval` and `axis_multiplier` so one trigger = one step; set saner defaults. The knobs exist (`AxisGesture::axis_multiplier`, `IntervalGesture::interval`, `threshold`).
**Net:** ~70% GUI work (surface + explain the existing knobs, fix defaults), ~30% daemon polish (a clean "magnitude/repeats per trigger" abstraction so users don't reason in raw pixels). Low rewrite risk.

### Smart Actions / macros — `needs-daemon-work` + `GUI`, **HIGH complexity**
**What Options+ does [LOW — verify exact step types on web before locking]:** Build a named "Smart Action" = ordered sequence (keystrokes, text, app launch, open URL, delays, media keys) triggered by one button; ships presets + custom builder.
**logiops reality [HIGH]:** Actions are single-shot; no sequence, no delay-between-steps, no "launch app / open URL" action.
**Work needed:** a new composite action = ordered list of steps + inter-step delays (the worker pool `run_task_after` already supports delays). **Security:** "launch process / open URL" inside a **root** daemon is dangerous — route those step types to a **user-session helper** over D-Bus rather than executing as root. Keystroke/text/media steps can stay in the daemon (it already synthesizes input via uinput).

---

## Feature Dependencies

```
Privilege/session path (polkit or USE_USER_BUS or D-Bus policy)
    └──required by──> EVERYTHING (a normal-user GUI must reach logid)

Device list + live status
    └──required by──> Button remap UI ──> Gesture builder
                                      └──> DPI / Scroll / Thumbwheel UI

Existing gesture-direction infra (GestureAction.move → directional Gesture)
    └──enables──> Fine-grained gesture control (expose knobs)
    └──enables──> Action Ring (reuse direction capture)

Named profiles + ChangeProfile (exist)
    └──+ NEW app-focus watcher (session agent)──> Per-application profiles

Worker-pool delayed tasks (run_task_after, exists)
    └──enables──> Smart Actions (inter-step delays)

User-session helper (for safe launches/overlays)
    ├──enables──> Action Ring overlay rendering
    ├──enables──> Smart Actions "launch app / open URL" steps
    └──enables──> Per-app focus watching (Wayland-safe placement)

Flow ──conflicts/excluded──> (out of scope; Easy-Switch button covers the hardware part)
Backlight ──blocked on──> new LED HID++ feature wrapper (greenfield; v2+)
```

### Dependency notes
- **Privilege path gates the entire product.** Until a non-root GUI can call `pizza.pixl.LogiOps`, nothing else is demonstrable. Must be Phase-1.
- **A user-session helper process keeps recurring** (overlay, safe launches, Wayland focus). Designing it early pays off across Action Ring, Smart Actions, and per-app profiles — treat it as shared infrastructure, not per-feature glue.
- **Action Ring and fine-grained gestures share the daemon's direction-capture code** — sequence gestures first; the ring builds on what you learn.
- **Per-app profiles depend only on a watcher**, not on daemon changes (if the session agent owns matching) — relatively cheap *on X11*, risky on Wayland.

---

## MVP Definition

### Launch With (v1) — "a real GUI that beats editing logid.cfg"
- [ ] **Privilege/session access to logid over D-Bus** — gates everything.
- [ ] **Device list + live battery/connection** — the home screen.
- [ ] **Visual button remapping** with the existing action set (keypress, DPI, host, profile, toggles).
- [ ] **DPI slider/levels, SmartShift, hi-res/invert scroll, thumbwheel** — all already in the daemon.
- [ ] **Fine-grained gesture builder** — *the* differentiator; fixes the owner's concrete pain; mostly GUI over existing knobs.
- [ ] **Settings persistence + restore defaults** (`Configuration::save()`, `Reset`).
- [ ] **Profiles UI** (create/switch named profiles manually).

### Add After Validation (v1.x)
- [ ] **Per-application profiles** — once a session agent + X11 focus watcher exist (Wayland: partial, flagged).
- [ ] **Action Ring** — headless/direction-only first, then the overlay; flagship signature feature.
- [ ] **Import/export & shareable profiles** — cheap, high delight.
- [ ] **DPI presets with labels.**

### Future Consideration (v2+)
- [ ] **Smart Actions / macros** — needs composite-action daemon work + safe-launch helper; verify Options+ semantics first.
- [ ] **Backlight / RGB** — only if a target keyboard justifies a new LED HID++ feature; out of scope for the mouse-first owner.
- [ ] **Wayland-robust foreground detection** — track compositor APIs.

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---|---|---|---|
| Privilege/session access path | HIGH | MED | P1 |
| Device list + battery/status | HIGH | LOW–MED | P1 |
| Visual button remapping | HIGH | MED | P1 |
| DPI / SmartShift / scroll / thumbwheel UI | HIGH | LOW–MED | P1 |
| Fine-grained gesture control | HIGH | MED | P1 |
| Settings persistence + reset | HIGH | LOW–MED | P1 |
| Manual named profiles UI | MED | LOW | P1 |
| Per-application profiles | HIGH | HIGH | P2 |
| Action Ring (radial wheel) | HIGH | HIGH | P2 |
| Import/export profiles | MED | LOW | P2 |
| DPI presets with labels | MED | MED | P2 |
| Smart Actions / macros | MED–HIGH | HIGH | P3 |
| Easy-Switch host button (already works) | MED | LOW | P1 (exposure) |
| Backlight / RGB | LOW (this audience) | HIGH | P3 |
| Logi Flow | — | — | Excluded (anti-feature) |
| Firmware updates | — | — | Excluded (anti-feature) |
| Cloud/account/AI/telemetry | — | — | Excluded (anti-feature) |

**Priority key:** P1 = must have for launch · P2 = should have, add when possible · P3 = future.

---

## Competitor / source analysis

- **Logitech Options+** (the cloned product) — feature set reconstructed from model knowledge (Jan 2026 cutoff). **Not web-verified this session** — see method note. Items most at risk of having changed: exact **Smart Actions** step vocabulary [LOW], **Action Ring** interaction details [MEDIUM], **predictive battery** specifics [LOW].
- **logiops codebase** (this repo) — **verified by direct source reading**: `src/logid/config/schema.h`, `src/logid/actions/` (+`gesture/`), `src/logid/features/`, `src/logid/backend/hidpp20/features/`, and `.planning/codebase/{ARCHITECTURE,INTEGRATIONS}.md`. HIGH confidence for the entire mapping column.
- **Adjacent Linux tools** (context, not web-verified [LOW]): raw `logiops`/`logid` (text config — the thing being replaced), Solaar (GUI for HID++ but different UX, weaker gesture/ring story), `input-leap`/`barrier` (the realistic Flow alternative).

### Open items to web-verify before they harden into requirements
1. **Smart Actions** exact step types & whether presets need cloud. [LOW]
2. **Action Ring** precise selection mechanic (flick-to-slice vs scroll-to-rotate) to match feel. [MEDIUM]
3. Current **Options+ UI/IA** (tab names, screen order) for the "faithful clone" visual target. [MEDIUM]
4. Whether recent Options+ added device features that map to **HID++ 2.0 features logiops lacks** (e.g. gesture-pad, specific scroll modes). [LOW]

---
*Feature research for: Logitech Options+ clone on Linux over the logiops daemon*
*Researched: 2026-05-30 — Options+ catalog from model knowledge (web tools unavailable); logiops mapping verified from source.*
