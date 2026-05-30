# Project Research Summary

**Project:** Logi Options+ for Linux (working title) — a faithful Options+ clone GUI on top of the `logiops`/`logid` daemon
**Domain:** Linux desktop GUI configurator for Logitech HID++ devices, integrated over D-Bus to a root daemon; Debian-first
**Researched:** 2026-05-30
**Confidence:** MEDIUM-HIGH (HIGH on everything read from this repo's source and the target machine; MEDIUM on the Options+ feature catalog and Wayland/polkit specifics — web tools were unavailable this session)

## Executive Summary

This is a brownfield GUI project: the `logiops` daemon already does all HID++ communication, device discovery, button/gesture/DPI/scroll configuration, and exposes its entire live config tree over D-Bus (`pizza.pixl.LogiOps` on the system bus). The product is therefore **not a reimplementation** — it is a polished Options+-style GUI client that drives the existing daemon, extended with C++ daemon work only where a feature genuinely requires it (richer gestures, the action wheel). The research converges hard on one structural truth: the daemon ships a **root-only D-Bus policy**, so a normal-user GUI cannot talk to it at all today. Resolving that access path is the single gate the entire product hangs on and must be the first thing built.

The recommended stack is **Qt 6 + QML + QtDBus** (C++20, CMake). This wins because the daemon is already C++20/CMake and will be extended in the same repo — Qt gives one language, one build system, and one D-Bus mental model across daemon and GUI, plus a fully skinnable QML scene graph for a faithful Options+ look and a natural radial action-wheel overlay (`QtQuick.Shapes` + frameless window / `layer-shell-qt`). Tauri 2 + `zbus` is a credible runner-up (easiest pixel-faithful clone) but adds a second toolchain and webview-fidelity drift. All versions were verified against the actual target machine (Zorin OS 18.1 / Ubuntu 24.04 "noble", Qt 6.4.2).

The key risks are concentrated and well-understood. (1) **Access/privilege**: the recommended path is to relax the system-bus policy to a dedicated group for live control, and gate only privileged writes (`Configuration::save()` -> `/etc/logid.cfg`) behind polkit — never run the GUI as root, never let it edit the config file directly. (2) **Daemon security**: the root daemon parses untrusted HID with no sandboxing and does unbounded report indexing (CONCERNS #2/#3); widening the bus audience raises the value of hardening, so hardening must precede the first daemon extension. (3) **Wayland**: the action wheel and per-app focus detection have no portable Wayland implementation and need a dedicated spike. (4) **Single source of truth**: the daemon owns config; the GUI reads/writes only via D-Bus. Note that the Options+ feature catalog and the Wayland/polkit specifics are MEDIUM confidence (web verification was unavailable) and are flagged for per-phase re-verification.

## Key Findings

### Recommended Stack

**Qt 6 + QML (Qt Quick Controls 2) + QtDBus**, C++20, CMake — see STACK.md. The decisive factor is single-toolchain cohesion with the C++20/CMake daemon: the GUI becomes another CMake target, reuses `ipc_defs.h` constants, and `qdbusxml2cpp` turns the daemon's D-Bus introspection into a typed C++ proxy. QML provides the custom-skinnable, animated UI an Options+ clone demands and the natural surface for the radial action-wheel overlay. All deps are stock `noble` packages (verified on target: Qt 6.4.2). Qt also keeps a future Win/macOS port possible (a stated constraint). Runner-up: Tauri 2 + `zbus` (verify crate versions before locking). Explicitly avoid GTK4/libadwaita (Linux-only, fights custom skin), Electron (heavy), Flutter-linux (weak D-Bus), and running the GUI as root.

**Core technologies:**
- **Qt 6 / QML (Qt Quick Controls 2)** 6.4.2: GUI + declarative custom-skinned UI — same C++ language as the daemon, GPU scene graph, themeable for Options+ fidelity
- **QtDBus** 6.4.2: D-Bus client to `logid` — first-class, `systemBus()` + `qdbusxml2cpp` typed proxy over `pizza.pixl.LogiOps`
- **QtQuick.Shapes + layer-shell-qt**: radial action-wheel overlay — vector arcs for slices; layer-shell for correct Wayland always-on-top (frameless+top-hint on X11)
- **C++20 / CMake** (already in repo): GUI builds as another target; `debhelper` (compat 13) + `dh --buildsystem=cmake` for Debian packaging

### Expected Features

See FEATURES.md. The logiops mapping (already-in-daemon / needs-daemon-work / GUI-only) is HIGH confidence (read from source); the Options+ catalog itself is MEDIUM. Crucial finding: **~70% of fine-grained gestures already exist in the schema** (`axis_multiplier`, `threshold`, `interval`) — the owner's pain ("1 desktop per gesture", "volume steps by 2") is mostly GUI-exposure of existing knobs plus small daemon polish, not a rewrite.

**Must have (table stakes):**
- Device list with live battery + connection status — already-in-logiops (DeviceStatus) + GUI
- Visual button remapping with the existing action set — already-in-logiops (RemapButton/actions) + GUI
- DPI slider/levels, SmartShift, hi-res/invert scroll, thumbwheel — all already in the daemon
- Settings persistence (`Configuration::save()`) + restore defaults (`Reset`)
- Manual named profiles UI — already-in-logiops (`ChangeProfile`)

**Should have (competitive):**
- **Fine-grained gesture control** — THE priority differentiator; fixes owner pain; ~70% GUI over existing knobs, ~30% daemon polish
- **Action wheel (radial menu)** — flagship signature feature; needs daemon extension + GUI overlay; highest risk (Wayland)
- **Per-application profiles** — needs a user-session focus watcher (X11 easy, Wayland hard) calling existing `ChangeProfile`
- Import/export shareable profiles (cheap — config is a file); DPI presets with labels

**Defer (v2+):**
- Smart Actions / macros — composite-action daemon work + safe-launch helper; verify Options+ semantics first
- Backlight / RGB — no LED HID++ feature exists; greenfield, mouse-first audience doesn't need it
- Excluded anti-features: Logi Flow (point to input-leap), firmware updates, cloud/account/AI/telemetry

### Architecture Approach

See ARCHITECTURE.md. Keep the daemon on the **system bus**; relax its D-Bus policy to a dedicated group (`own=root` stays) for live control, and layer **polkit only on the privileged `save()` write**. Treat **live D-Bus as the runtime source of truth**; the GUI never edits `/etc/logid.cfg` directly. For the action wheel, **the daemon detects and executes, the GUI renders** — the daemon owns the gesture trigger, the radial model, pointer-delta tracking (from HID++, sidestepping Wayland input restrictions) and action execution; the GUI subscribes to signals and draws the overlay. A recurring theme is a **user-session helper** for overlay rendering, safe app launches, and Wayland-safe focus watching — design it early as shared infrastructure.

**Major components:**
1. **GUI config UI (Qt/QML, non-root)** — Options+ views (devices/DPI/buttons/gestures/profiles); pure D-Bus client, never touches hardware or files
2. **D-Bus client proxy** — typed wrappers over `pizza.pixl.LogiOps`, reconnect handling, signal-driven live state
3. **Action-wheel overlay renderer + session helper** — X11/Wayland overlay backends; safe launches; Wayland focus watching
4. **logid daemon (existing, root)** — all HID++ I/O, config, uinput; extended with a new `ActionWheel` feature + richer gesture params + polkit-gated `save()` + systemd hardening

### Critical Pitfalls

Top items from PITFALLS.md:

1. **Running the GUI as root to "make D-Bus work"** — massive attack surface, broken theming/XDG, Wayland refuses root. Avoid: never elevate the GUI; widen policy to a group + polkit-gate writes.
2. **No / over-broad D-Bus + polkit policy** — root-only blocks everything; allow-everyone lets any local process rewrite root config (CONCERNS #4). Avoid: split read (group, free) vs write (polkit `auth_admin_keep`), shipped as code artifacts.
3. **Assuming X11 overlay/input tricks work on Wayland** — the action-wheel trap. No global grab, no absolute positioning, no global hotkey; Mutter doesn't implement layer-shell. Avoid: trigger from the daemon's gesture engine (bypasses compositor), render via layer-shell where present, degrade gracefully; dedicated Wayland spike.
4. **Extending the root daemon while inheriting its untrusted-HID surface** (CONCERNS #2/#3) — new parsing of attacker-controlled HID as root. Avoid: length-check every report field before indexing, add systemd hardening (`NoNewPrivileges`, `ProtectSystem=strict`, capability bounding) before the first extension, add tests (the codebase has effectively none).
5. **Two/three sources of truth (GUI vs live daemon vs `/etc/logid.cfg`)** — drift, clobbered comments, a user-owned GUI can't write the root file anyway. Avoid: daemon is the single source of truth; GUI reads live + writes via D-Bus; subscribe to signals; verify `save()` comment-preservation.

## Implications for Roadmap

Based on combined research, the suggested phase structure (the ARCHITECTURE build order and PITFALLS phase mapping agree strongly):

### Phase 1: GUI <-> Daemon Access Path (foundational)
**Rationale:** The root-only D-Bus policy blocks the entire product — every researcher independently flagged this as the gate. Nothing is demonstrable until a non-root client can reach `pizza.pixl.LogiOps`. Daemon hardening is a *prerequisite* of opening the bus, not later cleanup.
**Delivers:** Relaxed `logiops-dbus.conf.in` (group-scoped send/receive, `own=root`); polkit `.policy` for `save()`/privileged writes; systemd hardening + HID report length-checks; packaging group setup. A non-root smoke test calling the daemon.
**Addresses:** Privilege/session access (gates everything in FEATURES.md).
**Avoids:** Pitfalls 1, 2, 7 (GUI-as-root, bad policy, untrusted-HID root surface).

### Phase 2: D-Bus Client + GUI Skeleton + Device List
**Rationale:** Pure D-Bus-client work against the existing surface; delivers the "see my mouse" home screen with zero daemon C++ changes.
**Delivers:** Qt/QML app shell, typed D-Bus proxy, signal-driven device list with live battery/connection status.
**Uses:** Qt 6 / QML / QtDBus, `qdbusxml2cpp` (STACK.md).
**Implements:** GUI config UI + D-Bus proxy components.
**Avoids:** Pitfalls 5, 6 (single source of truth via live signals; hotplug/offline-device handling — never cache a static list).

### Phase 3: Core Config UI (live control) + Persistence
**Rationale:** All knobs (DPI/SmartShift/scroll/thumbwheel/button remap, manual profiles) already exist in the daemon — this is live schema editing over D-Bus plus polkit-gated `save()`. Delivers the core "configure without text editing" value.
**Delivers:** Button remap UI, DPI/scroll/thumbwheel UI, named-profiles UI, persistence via polkit-gated `save()`.
**Addresses:** Table-stakes features (FEATURES.md P1).
**Avoids:** Pitfall 5 (GUI never writes `/etc/logid.cfg`); verify `save()` comment-preservation.

### Phase 4: Fine-Grained Gesture Control (first daemon extension)
**Rationale:** THE priority differentiator and the safest first C++ daemon extension — it exercises the build/IPC-extension path on a contained change (~70% GUI exposure, ~30% daemon polish), de-risking the action wheel. Directly fixes the owner's concrete pain.
**Delivers:** Gesture builder UX over existing `axis_multiplier`/`threshold`/`interval`; daemon magnitude/repetition/granularity params + better defaults.
**Addresses:** Fine-grained gesture control (FEATURES.md top differentiator).
**Avoids:** Pitfall 4 (all input synthesis stays in the daemon's uinput path; fix uinput recreate-per-code churn while here).

### Phase 5: Per-Application Profiles
**Rationale:** Named profiles + `ChangeProfile` already exist; only a focus watcher is missing. Cheap on X11. Builds on the user-session helper.
**Delivers:** User-session focus watcher (X11 `_NET_ACTIVE_WINDOW`/`WM_CLASS`) calling existing `ChangeProfile`; app-match rules.
**Research flag:** Wayland foreground detection is compositor-specific and possibly partial.

### Phase 6: Action Wheel (highest risk — spike first)
**Rationale:** Flagship signature feature with no portable Wayland overlay path. Build last, after daemon-extension muscle (P4) and signal/proxy plumbing (P2) exist. Ship a headless/direction-only version first, then the overlay.
**Delivers:** New daemon `ActionWheel` feature (model + open/highlight/choose signals, direction capture reusing gesture infra); GUI overlay renderer (X11 -> Wayland layer-shell -> fallback).
**Avoids:** Pitfall 3 (daemon-triggered, not global hotkey; HID-delta highlighting, not cursor reading; no GUI toolkit in the root daemon).

### Phase 7: Packaging Polish (Debian-first)
**Rationale:** The GUI<->daemon version-compatibility contract is decided in Phase 1; this phase enforces it. Late but must test on clean VMs.
**Delivers:** `debhelper`/`dh` packaging shipping D-Bus policy + polkit + systemd unit; versioned `Depends:`; clean-VM install/upgrade/**purge** + D-Bus handshake smoke test in CI.
**Avoids:** Pitfall 8 (packaging that assumes the daemon is already installed).

### Phase Ordering Rationale

- **Access path is non-negotiably first** — it gates every other phase, and hardening must accompany (not follow) opening the bus.
- **Phases 2-3 are pure D-Bus-client work** against the existing surface — they deliver core value with zero daemon C++ risk.
- **Phase 4 (gestures) is the first and safest daemon extension** — it de-risks the C++ extension toolchain before the high-risk action wheel, and it's the priority differentiator.
- **A user-session helper recurs** across action wheel, safe launches, and Wayland focus — introduce it as shared infrastructure around Phase 5.
- **Phase 6 (action wheel) is last and needs its own spike** because of X11/Wayland overlay uncertainty.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 1:** Re-verify exact D-Bus policy `group=` syntax and current polkit defaults against live `dbus-daemon`/`polkit` docs (MEDIUM — web unavailable this session).
- **Phase 5:** Wayland foreground-app detection — compositor-specific, possibly partial; verify `wlr-foreign-toplevel`/GNOME-extension/KWin options.
- **Phase 6:** Dedicated Wayland-vs-X11 overlay spike (compositor matrix: GNOME-Mutter vs KDE-KWin vs wlroots; confirm Mutter layer-shell status). Also verify Options+ action-wheel selection mechanic for feel.
- **Phase 4 (light):** Confirm Smart Actions step vocabulary only if/when macros enter scope (v2+).

Phases with standard patterns (lighter research):
- **Phases 2-3:** Well-trodden Qt/QML + D-Bus-client patterns against an existing, source-verified interface.
- **Phase 7:** Standard `debhelper`/`dh_installsystemd` Debian packaging (just test on clean VMs).

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Versions verified on the actual target machine (Zorin 18.1 / Ubuntu 24.04, Qt 6.4.2). Tauri runner-up crate versions are MEDIUM (from training data — verify). |
| Features | MEDIUM-HIGH | logiops mapping column is HIGH (read from source); Options+ catalog is MEDIUM (model knowledge, web tools unavailable). |
| Architecture | HIGH / MEDIUM | Codebase + D-Bus facts HIGH (verified in-tree); D-Bus-policy/polkit/Wayland ecosystem patterns MEDIUM (training data). |
| Pitfalls | HIGH / MEDIUM | Codebase-grounded pitfalls (policy, uinput, root, untrusted HID) HIGH; Wayland/polkit/packaging specifics MEDIUM. |

**Overall confidence:** MEDIUM-HIGH — strong on everything verifiable from this repo and the target system; the soft spots are the external Options+ catalog and Wayland/polkit specifics, both flagged for per-phase re-verification.

### Gaps to Address

- **Web verification was unavailable this entire research session.** The Options+ feature catalog, exact Wayland compositor capabilities (Mutter layer-shell status), polkit/D-Bus policy syntax, and Tauri/`zbus` crate versions are MEDIUM confidence. Re-verify against live docs at the start of the relevant phase before any of these harden into requirements.
- **Wayland overlay + foreground detection** (Phases 5-6): no portable API; needs a dedicated spike with a compositor matrix; treat "faithful follow-the-pointer overlay everywhere" as MEDIUM/LOW feasibility.
- **`Configuration::save()` round-trip fidelity** (Phase 3): libconfig typically does not preserve comments — verify and warn users, or power users lose hand-written annotations.
- **HID++ 1.0 / receiver-pairing TODOs and the HID++ 2.0 hardcode** (`Device.h:55`) (Phase 2): budget daemon work if heterogeneous/multi-device richness is in scope.
- **ipcgull is vendored and incomplete** (CONCERNS #10): widening its audience exposes parsing/marshalling bugs — budget time to exercise the new method/signal surface.

## Sources

### Primary (HIGH confidence)
- Target system probe — `apt-cache policy` / `pkg-config` / `dpkg -l` on Zorin OS 18.1 / Ubuntu 24.04 `noble`: confirmed Qt 6.4.2 stack, GLib/GIO 2.80, layer-shell-qt, debhelper 13, cmake 3.28
- This repo's source — `src/logid/config/schema.h`, `src/logid/actions/` (+`gesture/`), `src/logid/features/`, `backend/hidpp20/features/`, `ipc_defs.h`, `logiops-dbus.conf.in`, `logid.cpp`
- `.planning/codebase/` maps — `ARCHITECTURE.md`, `INTEGRATIONS.md`, `CONCERNS.md` (#2 root/no-sandbox, #3 unbounded HID indexing, #4 root D-Bus config write, #7 uinput churn, #11 pairing TODOs, #12 zero tests); `.planning/PROJECT.md`
- Detailed research: `.planning/research/{STACK,FEATURES,ARCHITECTURE,PITFALLS}.md`

### Secondary (MEDIUM confidence)
- Qt DBus / QML / QtQuick.Shapes capabilities; Tauri 2 + `zbus` — model knowledge (verify crate versions on crates.io)
- D-Bus system-bus policy (own/send/receive, group scoping) + polkit action gating — training-data conventions (verify syntax against live docs in Phase 1)
- Logitech Options+ feature catalog — model knowledge as of Jan 2026 cutoff (not web-verified)

### Tertiary (LOW confidence)
- Wayland overlay/input constraints, `wlr-layer-shell` vs Mutter non-support, Wayland foreground-app APIs — needs validation in a dedicated Phase 6 spike
- Options+ Smart Actions exact step types and Action Ring selection mechanic — verify before they drive requirements

---
*Research completed: 2026-05-30*
*Ready for roadmap: yes*
