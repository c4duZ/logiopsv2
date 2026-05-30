# Architecture Research

**Domain:** Linux desktop GUI (Options+ clone) on top of a root HID daemon, integrated over D-Bus
**Researched:** 2026-05-30
**Confidence:** HIGH for codebase/D-Bus facts (verified in-tree); MEDIUM for D-Bus/polkit/Wayland ecosystem patterns (training-data-based, web verification unavailable this session — see Sources)

## Executive Recommendation (read first)

**Privilege/IPC:** Keep the daemon on the **system bus** and **relax the D-Bus policy to a group** (e.g. `plugdev` or a dedicated `logiops` group) for `send_destination`/`receive_sender`, while keeping `own=root`. This is the lowest-friction, most-portable path that matches how the daemon is already deployed (root systemd service touching `hidraw`/`uinput`). Layer **polkit on top only for the genuinely privileged write action** (`Configuration::save()` → `/etc/logid.cfg`). Do **not** adopt `USE_USER_BUS` as the integration strategy, and do **not** build a separate helper service. Rationale and security analysis below.

**Config strategy:** Treat **live D-Bus as the source of truth at runtime**; persist via the daemon's own `Configuration::save()` (polkit-gated). The GUI **never** edits `/etc/logid.cfg` directly.

**Action wheel / gestures:** Split responsibility. The **daemon** detects the gesture and owns the radial *model* + emits "wheel opened/segment-highlighted/segment-chosen" over D-Bus and executes the chosen action (it already owns `InputDevice`/uinput). The **GUI renders the overlay**. On **X11** an override-redirect/input-passthrough top-level works. On **Wayland** a faithful always-on-top, follow-the-pointer overlay is **not portable** — use `wlr-layer-shell` where available (wlroots compositors) and degrade to a normal pop-up window elsewhere. This is the single biggest architectural risk; flag it for deep research in its own phase.

## Standard Architecture

### System Overview

```
┌──────────────────────────────────────────────────────────────────────┐
│                         USER SESSION (non-root)                        │
│                                                                        │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │                        GUI Application                          │  │
│  │  ┌───────────────┐  ┌────────────────┐  ┌──────────────────┐    │  │
│  │  │ Options+-style │  │ D-Bus client   │  │ Action-wheel     │    │  │
│  │  │ config UI      │  │ proxy layer    │  │ overlay renderer │    │  │
│  │  │ (devices, DPI, │  │ (typed wrappers│  │ (X11 / Wayland   │    │  │
│  │  │  buttons,      │  │  over          │  │  layer-shell)    │    │  │
│  │  │  gestures)     │  │  pizza.pixl.*) │  │                  │    │  │
│  │  └───────┬────────┘  └───────┬────────┘  └────────┬─────────┘    │  │
│  └──────────┼───────────────────┼────────────────────┼─────────────┘  │
│             │                   │                     │                │
└─────────────┼───────────────────┼─────────────────────┼───────────────┘
              │ method calls       │ signals (live state) │ overlay signals
              │ (DPI, remap,       │ (device add/remove,  │ (wheel open,
              │  profile, save)    │  battery, status)    │  segment chosen)
              ▼                   ▼                     ▼
┌──────────────────────────────────────────────────────────────────────┐
│              D-Bus SYSTEM BUS  (pizza.pixl.LogiOps)                     │
│   policy: own=root; send/receive = group "logiops" (RELAXED)           │
│   privileged writes additionally gated by polkit action                │
└───────────────────────────────────┬──────────────────────────────────┘
                                     │
┌────────────────────────────────────┼──────────────────────────────────┐
│                       logid DAEMON (root, existing)                    │
│  ┌──────────────────────────────────────────────────────────────────┐ │
│  │ ipcgull server  →  DeviceManager / Device / Receiver objects      │ │
│  │                    config schema objects (live, reflective)       │ │
│  ├──────────────────────────────────────────────────────────────────┤ │
│  │ NEW: ActionWheel feature (gesture→model→signal→execute)           │ │
│  │ NEW: richer Gesture params (magnitude/repetition/granularity)     │ │
│  ├──────────────────────────────────────────────────────────────────┤ │
│  │ features/ → hidpp20 → hidpp → raw(hidraw) ;  InputDevice(uinput)  │ │
│  └──────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────┘
                                     │ hidraw / udev / uinput
                                     ▼
                           Logitech HID++ device(s)
```

### Component Responsibilities

| Component | Responsibility | Implementation |
|-----------|----------------|----------------|
| **GUI config UI** | Faithful Options+ look; present device list, DPI, buttons, gestures, per-app profiles; never touches hardware or files directly | Chosen GUI stack (see STACK research); pure D-Bus client |
| **D-Bus client proxy** | Typed, async wrappers over `pizza.pixl.LogiOps` objects/methods/signals; reconnect handling; surfaces daemon state to UI | Generated/hand-written GDBus or platform D-Bus binding |
| **Action-wheel overlay renderer** | Draw the radial menu, track pointer/segment, send selection back to daemon; handle X11 vs Wayland surface differences | Separate window/surface in the GUI process (or a tiny sibling) |
| **logid daemon (existing)** | All HID++ I/O, feature config, profile switching, uinput synthesis, config persistence; owns the bus name | Existing C++20 daemon, root |
| **ActionWheel feature (NEW)** | Detect the bound gesture, own the radial action model, emit open/highlight/choose signals, execute the selected `Action` via existing engine | New `DeviceFeature` + `ipcgull::object` in `src/logid/features/` |
| **Gesture param extensions (NEW)** | Add magnitude/repetition/granularity to gesture config + execution to fix "1 desktop only / volume +2" issues | Extend `actions/gesture/` + `config/schema.h` |
| **polkit policy (NEW)** | Authorize the privileged `save()`-to-`/etc/logid.cfg` action for an admin/active session | `.policy` XML + daemon-side `polkit` check |

## The Privilege / IPC Decision

Four candidate approaches were evaluated against: works for a non-root GUI, security blast radius, portability across distros/desktops, fit with the existing root-daemon design, and implementation cost.

| Approach | Non-root GUI works? | Security blast radius | Portability | Fit with current design | Cost |
|----------|--------------------|-----------------------|-------------|------------------------|------|
| **(a) Relax system-bus policy to a group** | Yes | **Low–Medium** — group members can call *all* methods incl. live remap; mitigate privileged writes with polkit | High (works on every distro/DE; one conf file) | **Excellent** — daemon already on system bus as root | **Low** |
| (b) `USE_USER_BUS` (session bus) | Yes, but… | **High in practice** — a root process owning a name on the *user* session bus is awkward; one daemon instance vs per-session; root reachable by anything in the session | Low (multi-user, headless, "who owns the bus" problems) | Poor — fights the root-service model | Medium |
| (c) polkit-mediated for *everything* | Yes | **Lowest per-action**, but auth prompts on routine reads/DPI tweaks ruin UX | High | Good as a *complement*, bad as the *transport* | High |
| (d) Separate helper service | Yes | Medium — adds a second privileged surface to audit | Medium | Redundant — the daemon *is* already the privileged helper | High |

### Recommended: (a) group-scoped system-bus policy + (c) polkit only for privileged writes

**Why (a) as the transport.** The daemon is, by design, a root system service that must hold `hidraw`/`uinput` and a stable bus name regardless of who is logged in (system bus is the correct bus for that). The *only* thing blocking the GUI today is one over-restrictive policy file (`logiops-dbus.conf.in` grants send/receive to `user="root"` only). Replacing `user="root"` on the `send_destination`/`receive_sender` rules with `group="logiops"` (keeping `own="root"`) is a one-file change, works identically on every distro and every desktop (X11 or Wayland, GNOME/KDE/wlroots), and requires **zero daemon code** to enable basic GUI control. Installation adds the user to the group (or ships a udev/`tmpfiles`-style group setup).

**Why (c) on top, but narrowly.** Group access still hands group members the full method surface — including live button remapping that synthesizes input via uinput, and especially `Configuration::save()` which writes `/etc/logid.cfg` as root (CONCERNS #4, High). Gate **only** that persistence action (and any future system-file-touching action) behind a polkit action checked daemon-side, so a transient group membership can drive the device live but cannot silently rewrite root-owned system config without an authorization decision. Runtime/ephemeral calls (DPI, profile switch, query) stay un-prompted for good UX.

**Why not (b) `USE_USER_BUS`.** Tempting because it "just works" for a single logged-in developer, but it inverts the deployment model: a root daemon owning a name on a *user* session bus is unusual, breaks for multi-user/fast-user-switching and headless setups, and means anything in your session can reach root. Keep `USE_USER_BUS` available as a **developer convenience build flag** only (it already exists at `logid.cpp:154`), not as the product's integration path.

**Why not (d) helper service.** The daemon already *is* the privileged, audited component. A second privileged process duplicates the attack surface (CONCERNS #2: root, no sandboxing) for no new capability.

### Security implications to carry into the roadmap

- **Group = trust boundary.** Anyone in the `logiops` group can drive the device and synthesize input via the root daemon's uinput. Document this; default to a dedicated group, not `wheel`/`sudo`.
- **The existing root surface is the real risk, not the GUI.** CONCERNS #2 (root, no sandboxing) and #3 (unbounded indexing of attacker-controlled HID reports) already exist. **Adding a wider IPC audience raises the value of hardening the daemon.** Recommend, as part of opening the bus: add systemd sandboxing (`NoNewPrivileges`, `ProtectSystem=strict`, `RestrictAddressFamilies=AF_UNIX`, capability bounding) and length-check HID reports before indexing. These are prerequisites, not nice-to-haves, once non-root callers exist.
- **polkit-gate every method that writes root-owned files or changes persistent system state.** Live/ephemeral control can stay open to the group.
- **ipcgull is vendored and incomplete** (CONCERNS #10). Widening its audience increases exposure of any parsing/marshalling bugs in `server_gdbus.cpp`. Budget time to exercise the new method/signal surface.

## D-Bus Interface Surface the GUI Needs

The daemon already publishes a tree under `/pizza/pixl/logiops` (`server_root_node`, `ipc_defs.h`) via ipcgull, with `DeviceManager`, `Device`, `Receiver`, feature, and live config-schema objects (ARCHITECTURE map, lines 90–96, 135–137). The GUI consumes — and the daemon may need to extend — roughly these capabilities:

| Object / area | Methods the GUI needs | Signals the GUI needs |
|---------------|-----------------------|-----------------------|
| **DeviceManager / root** | enumerate devices; start/stop receiver pairing | device added / removed; receiver pair-ready |
| **Device** | get name/PID/capabilities; get/set active profile; (battery query) | connect/disconnect (awake/asleep); battery/status changed |
| **DPI feature** | get DPI list + current; set DPI; cycle | DPI changed |
| **SmartShift / HiresScroll / ThumbWheel** | get/set their settings | setting changed |
| **RemapButton** | enumerate controls; get/set the `Action` bound to each button; enumerate gesture directions | remap applied |
| **Gesture (extended)** | get/set per-direction gesture + **new** magnitude/repetition/granularity params | — |
| **Config (schema objects)** | read/write profile tree live; **`save()`** (polkit-gated) to persist `/etc/logid.cfg` | config changed |
| **ActionWheel (NEW)** | register/get the radial action model per button; (optional) commit a chosen segment if the GUI drives selection | **wheel opened (with model + anchor); segment highlighted; wheel closed/chosen** |

Implementation note: actions are constructed from a `std::variant` config node via `makeAction`/`Gesture` factories (ARCHITECTURE lines 109–112). The GUI sets buttons by writing the corresponding config-variant fields over D-Bus; the daemon re-runs `configure()`/`reconfigure()`. No new "apply" RPC is strictly required for existing features — the live schema *is* the API.

## Config Read/Write Strategy

Three options exist (PROJECT lines 53; ARCHITECTURE lines 90–94): (1) live D-Bus schema edits, (2) the GUI editing `/etc/logid.cfg` directly, (3) the daemon's `Configuration::save()`.

**Decision:**
- **Runtime = live D-Bus.** Config schema objects are exposed live; edits are visible immediately to features holding `reference_wrapper`s into the tree (ARCHITECTURE line 94). The GUI mutates these for instant feedback (DPI, remaps, gestures). This is the source of truth while the daemon runs.
- **Persistence = daemon `Configuration::save()`, polkit-gated.** Only the daemon should write `/etc/logid.cfg` (it runs as root and owns the file; `Configuration.cpp:57–76`). Expose `save()` over D-Bus behind a polkit action so the GUI's "apply/save" triggers an authorized root-side write.
- **GUI MUST NOT edit `/etc/logid.cfg` directly.** A non-root GUI can't write `/etc/` anyway, and a parallel writer would race the daemon's in-memory tree and risk clobbering or reordering the libconfig file. Direct-file editing is an **anti-pattern** here.

Edge case to flag for the roadmap: round-trip fidelity of `Configuration::save()` (comments/formatting/unknown keys in a hand-edited `logid.cfg`) is unverified — power users with existing hand-written configs may lose comments. Validate before relying on save() as the only persistence path.

## Action-Wheel & Gesture Extension Architecture

### Gesture fixes (lower risk, do first)

The "one gesture = one desktop", "volume +2 per action" complaints (PROJECT lines 34, 56) are **daemon-side granularity limits** in `actions/gesture/` (`AxisGesture`, `IntervalGesture`, `ThresholdGesture`). Fix by adding magnitude/repetition/step parameters to the gesture config schema (`config/schema.h`) and honoring them in the gesture execution + `InputDevice` synthesis. This is a contained extension of an existing, well-factored layer — **no new IPC model, no rendering**. It is the natural first daemon extension and de-risks the toolchain.

### Action wheel (higher risk: it needs a rendered, pointer-tracking overlay)

**Responsibility split (recommended):**
- **Daemon owns:** the trigger (a gesture/button binding opens the wheel), the radial **model** (segments → actions, sourced from config), pointer-delta tracking *if* it comes from HID++ diverted input, the **execution** of the chosen action (it already owns `InputDevice`/uinput and `makeAction`).
- **GUI owns:** **rendering only.** On "wheel opened" signal it draws the radial menu at an anchor, highlights segments as the pointer moves, and the daemon (or the GUI, TBD) resolves the final selection. Keep rendering out of the root daemon — never put a GUI toolkit in a root process.

**Why this split:** rendering in a root daemon is a security and dependency disaster; gesture detection and uinput already live in the daemon. The seam is a small set of new D-Bus signals/methods (see ActionWheel row above).

**X11 vs Wayland — the crux:**

| Concern | X11 | Wayland |
|---------|-----|---------|
| Always-on-top overlay at arbitrary screen coords | Override-redirect window, trivial | **No portable API.** Use `wlr-layer-shell` (wlroots: Sway, Hyprland, etc.); GNOME/KDE do **not** support it the same way |
| Position at pointer / follow pointer | Query pointer freely | Compositor controls positioning; absolute placement is restricted by design |
| Read global pointer motion to highlight segments | Possible | Restricted; prefer routing motion **through the daemon** from HID++ deltas rather than reading the cursor |
| Click-through / input region control | `XShapeCombine` input region | `wl_surface.set_input_region` / layer-shell semantics |

**Implication for the roadmap:** the action wheel cannot be assumed to render identically everywhere. Recommended approach: (1) **drive segment highlighting from HID++ pointer deltas via the daemon**, not from reading the OS cursor — this sidesteps Wayland's input-introspection restrictions and works the same on both; (2) render via `wlr-layer-shell` where present, X11 override-redirect on X, and a **plain centered pop-up window fallback** elsewhere (GNOME/KDE Wayland). Treat "faithful follow-the-pointer overlay on all compositors" as **MEDIUM/LOW feasibility** and give the action wheel its **own research + spike phase**.

## Recommended Project Structure (new code only)

```
.                              # existing logiops repo (one combined repo, per PROJECT)
├── src/logid/                 # EXISTING daemon — extend in place
│   ├── features/
│   │   └── ActionWheel.*       # NEW DeviceFeature + ipcgull object (model + signals)
│   ├── actions/gesture/        # EXTEND: magnitude/repetition/granularity params
│   ├── config/schema.h         # EXTEND: gesture params, action-wheel model
│   ├── logiops-dbus.conf.in    # CHANGE: user="root" -> group="logiops" for send/receive
│   ├── logiops.policy.in       # NEW: polkit action for save()/privileged writes
│   └── logid.service.in        # HARDEN: sandboxing directives (prereq for open bus)
└── gui/                        # NEW GUI app (stack TBD in STACK research)
    ├── dbus/                   # typed proxy layer over pizza.pixl.LogiOps
    ├── ui/                     # Options+-clone views (devices/DPI/buttons/gestures/profiles)
    ├── overlay/                # action-wheel renderer: x11/ and wayland/ backends
    └── packaging/              # Debian-first packaging, group setup, polkit + dbus policy install
```

### Structure Rationale

- **Daemon extensions stay in `src/logid/`** following the existing `DeviceFeature` + `ipcgull::object` pattern (ARCHITECTURE lines 104–108) — minimal new idiom, reuses the proven factory/wrapper machinery.
- **GUI is a separate top-level `gui/`** so the GUI toolchain never leaks into the root daemon build; the only contract between them is D-Bus.
- **`overlay/` isolates X11 vs Wayland** behind a backend interface so the rest of the GUI is compositor-agnostic.

## Data Flow

### Live control (DPI / remap / gesture)

```
GUI UI action → D-Bus proxy → method/property write on system bus
   → ipcgull → live config schema object mutated
   → feature configure()/reconfigure() → hidpp20 → device
   ← signal (state changed) ← daemon → GUI updates UI
```

### Persist

```
GUI "Save" → save() method (system bus) → daemon checks polkit action
   → authorized → Configuration::save() writes /etc/logid.cfg (root)
   → result/signal → GUI confirms
```

### Action wheel

```
gesture trigger (HID++) → daemon ActionWheel feature
   → signal "wheel opened" (model + anchor) → GUI overlay renders
HID++ pointer deltas → daemon → signal "segment highlighted" → GUI updates highlight
release → daemon resolves chosen segment → executes Action (uinput) → signal "closed"
```

## Architectural Patterns

### Pattern: live schema *is* the API
**What:** Mutating config-schema objects over D-Bus is the control mechanism; features watch their references. **When:** all existing features (DPI/SmartShift/remap/gesture). **Trade-off:** no extra RPC surface, but the GUI must understand the schema shape; concurrent edits need care.

### Pattern: privileged action behind polkit, ephemeral control behind group policy
**What:** Two-tier authorization — group for runtime, polkit for system-file writes. **When:** `save()` and any future persistent/system-changing method. **Trade-off:** best security/UX balance; small daemon-side polkit integration cost.

### Pattern: daemon detects, GUI renders
**What:** Root daemon owns input/gesture/execution; user-space GUI owns pixels. **When:** action wheel, any future overlay. **Trade-off:** clean security boundary; requires a well-defined signal protocol and tolerating Wayland rendering limits.

## Anti-Patterns

### GUI edits /etc/logid.cfg directly
**Why wrong:** races the daemon's in-memory tree, needs root the GUI doesn't have, risks clobbering hand-written configs. **Instead:** live D-Bus edits + polkit-gated `save()`.

### Adopting USE_USER_BUS as the product integration path
**Why wrong:** inverts the root-service model; breaks multi-user/headless; exposes root to anything in the session. **Instead:** group-scoped system-bus policy. Keep USE_USER_BUS as a dev-only flag.

### Putting any GUI toolkit / rendering in the root daemon
**Why wrong:** massively enlarges the root attack surface (already flagged: root + untrusted HID parsing). **Instead:** daemon emits overlay signals; user-space GUI renders.

### Widening the bus without hardening the daemon
**Why wrong:** opening D-Bus to a group while CONCERNS #2/#3 (root, no sandbox, unbounded HID indexing) stand raises real risk. **Instead:** sandbox + length-check as a prerequisite of opening the bus.

## Suggested Build Order / Dependency Graph

```
P0  Access path (unblocks everything)
    ├─ relax logiops-dbus.conf.in to group="logiops"
    ├─ HARDEN logid.service.in (sandboxing) + length-check HID reports   [security prereq]
    └─ packaging: create group, install policy
         │
P1  D-Bus client + GUI skeleton ─────────────────┐
    proxy layer over existing objects;            │ depends on P0 (can reach the bus)
    device list + battery/status (read-mostly)    │
         │                                         │
P2  Core config UI (live control) ────────────────┤
    DPI / SmartShift / scroll / button remap       │ uses existing methods only
    via live schema edits                          │
         │                                         │
P3  Persistence ──────────────────────────────────┤
    polkit action + save() wiring                  │ depends on P0/P2
         │                                         │
P4  Gesture granularity (daemon extension) ───────┤  FIRST C++ extension; low risk
    magnitude/repetition/granularity in schema     │  de-risks daemon toolchain
    + gesture UI                                    │
         │                                         │
P5  Per-application profiles                        │  GUI + profile switching (exists)
         │                                         │
P6  Action wheel  ◄── RESEARCH/SPIKE FIRST ────────┘  HIGHEST risk
    daemon ActionWheel feature (model + signals)      depends on P4 (gesture trigger),
    + GUI overlay (X11 backend → Wayland layer-shell  P1 (signals/proxy)
      → fallback)
```

**Ordering rationale:**
- **P0 must be first** — nothing else works until a non-root client can reach the bus; and hardening is a *prerequisite* of opening it, not a later cleanup.
- **P1→P3 are pure D-Bus-client work** against the existing surface; they deliver the core "configure without text editing" value with zero daemon C++ changes.
- **P4 (gesture params) is the first and safest daemon extension** — it exercises the C++ build/IPC-extension path on a contained change, de-risking P6.
- **P6 (action wheel) is last and needs its own spike** because of the X11/Wayland overlay uncertainty; build it after the daemon-extension muscle (P4) exists and the signal/proxy plumbing (P1) is proven.

## Scaling Considerations

This is a single-user desktop tool; "scale" means **device count and desktop diversity**, not load.

| Dimension | Note |
|-----------|------|
| Multiple devices | `logid::Device` hardcodes HID++ 2.0 (TODO `Device.h:55`); multi-device richness may need daemon work before the GUI can show heterogeneous devices well |
| Compositor diversity | The action wheel is the only component sensitive to X11/Wayland/compositor; everything else is D-Bus and portable |
| Multi-user / fast-user-switching | System-bus + group policy handles this correctly; this is a key reason to avoid USE_USER_BUS |

## Sources

- In-tree, HIGH confidence (read this session): `src/logid/ipc_defs.h`, `src/logid/logiops-dbus.conf.in`, `src/logid/logid.cpp:140–183`, plus `.planning/codebase/{ARCHITECTURE,INTEGRATIONS,CONCERNS}.md` and `.planning/PROJECT.md`.
- D-Bus system-bus policy semantics (own/send_destination/receive_sender, group scoping) and polkit action gating: D-Bus / polkit conventions — **MEDIUM** confidence (web verification unavailable this session; based on training knowledge, cutoff 2026-01). Verify exact policy syntax against current `dbus-daemon` and `polkit` docs during P0.
- Wayland overlay constraints (no portable always-on-top/positioned surface; `wlr-layer-shell` is wlroots-only; GNOME/KDE differ; input-region/global-motion restrictions): Wayland protocol design — **MEDIUM/LOW** confidence; **flag for dedicated research in P6**.

---
*Architecture research for: Linux GUI over root logiops daemon (D-Bus)*
*Researched: 2026-05-30*
