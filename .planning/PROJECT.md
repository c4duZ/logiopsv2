# Logi Options+ for Linux (working title — name TBD)

## What This Is

A polished Linux desktop application that reconstructs the **Logitech Options+** experience — visually (a faithful clone of its look-and-feel) and, above all, in **features** — for configuring Logitech HID++ devices (mice/keyboards). It is built **on top of the existing `logiops` daemon** in this repo: the GUI talks to the `logid` daemon over D-Bus and edits its configuration, reusing all the HID++ communication that already works. Where the daemon can't yet do what a feature needs (better gestures, action wheel), **the logiops C++ daemon is extended** as part of the same project. Linux only, Debian first.

## Core Value

Bring the **full Options+ experience to Linux** — the rich configuration UX that today only exists on Windows/macOS — and in doing so fix the real rough edges of raw `logiops` (text-only config, limited/buggy gestures, no action wheel). If everything else fails, a Linux user with a Logitech mouse must be able to configure it through a real GUI instead of hand-editing `/etc/logid.cfg`.

## Requirements

### Validated

<!-- The logiops foundation this product builds on, plus capabilities delivered by completed phases. -->

- ✓ Non-root D-Bus access path: system-bus policy relaxed to a `logiops` group (own=root), `save()` polkit-gated (`auth_admin_keep`, fail-safe deny), systemd hardening, HID report length-checks — Phase 1 (code complete & statically verified; 4 runtime items pending human validation in `01-HUMAN-UAT.md`)
- ✓ Qt 6/QML GUI app shell (optional CMake target `src/logiops-gui/`) with a live, signal-driven device list (name, battery, connection), daemon-as-single-source-of-truth, reconnect, and 4 screen states; plus a new daemon battery feature (HID++ 0x1000 BatteryStatus + 0x1004 UnifiedBattery, prefers 0x1004) exposed over D-Bus — Phase 2 (code complete & statically verified, build clean -Werror, 5/5 tests; 6 runtime items pending human validation in `02-HUMAN-UAT.md`, incl. installing `qml6-module-qtquick` to run the GUI)

- ✓ HID++ 1.0/2.0 communication with Logitech devices over `hidraw` — existing (`src/logid/backend/`)
- ✓ Device discovery + hotplug via udev, incl. Unifying receivers — existing (`src/logid/backend/raw/DeviceMonitor.cpp`, `hidpp10/ReceiverMonitor.cpp`)
- ✓ Button remapping engine (actions: keypress, DPI cycle/change, profile/host change, toggles) — existing (`src/logid/actions/`)
- ✓ Adjustable DPI, SmartShift, Hires/Thumb scroll, ChangeHost, battery status — existing HID++ 2.0 features (`src/logid/features/`, `backend/hidpp20/features/`)
- ✓ Gesture engine (axis/interval/release/threshold gestures) — existing (`src/logid/actions/gesture/`)
- ✓ Live runtime control + config exposure over D-Bus via `ipcgull` (`pizza.pixl.LogiOps` at `/pizza/pixl/logiops`) — existing (`src/logid/ipc_defs.h`, `src/ipcgull/`)
- ✓ Per-profile configuration model in a typed libconfig schema (`/etc/logid.cfg`) — existing (`src/logid/config/schema.h`)

### Active

<!-- New scope for this project. Hypotheses until shipped and validated. -->

- [ ] GUI desktop app for Linux that configures Logitech devices without editing text config
- [ ] Faithful visual clone of Logitech Options+ (layout, flow, polish)
- [ ] Button remapping UI (assign actions to buttons visually)
- [ ] DPI / sensitivity UI (adjust and switch DPI levels)
- [ ] Gestures + SmartShift / scroll UI
- [ ] **Fine-grained gesture control** — fix limitations like "one gesture = only 1 desktop switch" and "volume steps by 2 per action"; expose magnitude/repetition/granularity
- [ ] **Action wheel** (radial action menu, à la Options+) — likely requires daemon extension
- [ ] Per-application profiles (different settings per active app)
- [ ] Device list / multi-device handling with battery and connection status
- [ ] Resolve GUI ↔ daemon privilege/access (D-Bus policy currently root-only)
- [ ] Debian packaging (primary), with portability to other distros kept in mind

### Out of Scope

<!-- Explicit boundaries with reasoning. -->

- Windows / macOS support — `logiops` is Linux-only; Options+ already exists there. Cross-platform was considered and dropped. Stack choices should not actively *prevent* future portability, but no non-Linux backend is built.
- Reimplementing HID++ communication from scratch — the existing `logiops` daemon already does this; we build on it, not replace it.
- Firmware updates / Logitech-cloud features (Flow account sync, etc.) — not supported by logiops and out of v1 scope.
- Wide multi-distro packaging at v1 — Debian first; broader packaging deferred until the app is solid.

## Context

- **Brownfield.** This repo is the `logiops` daemon (C++20, CMake, Linux). Full codebase map in `.planning/codebase/` (`ARCHITECTURE.md`, `STACK.md`, `INTEGRATIONS.md`, `CONVENTIONS.md`, `CONCERNS.md`, `TESTING.md`).
- **The daemon already exposes everything over D-Bus** (`ipcgull`, bus name `pizza.pixl.LogiOps`), which is the natural integration seam for a GUI. Config also lives as a libconfig file at `/etc/logid.cfg`, exposed live over D-Bus; `Configuration::save()` can write it back.
- **Real user pain driving this** (from the project owner, a Linux power user with a Logitech MX-series mouse — MX Master 4 is in `TESTED.md`):
  - No Options+ on Linux; raw logiops config is text-only and clunky.
  - Gestures feel buggy/limited: a gesture switches only one virtual desktop; volume rises 2 steps per action — no fine control.
  - No action wheel (radial menu) like Options+ has.
- **HID++ 2.0 features available today** (per codebase): `AdjustableDPI`, `SmartShift`, `HiresScroll`, `ThumbWheel`, `ReprogControls`, `ChangeHost`, `WirelessDeviceStatus` (battery), `DeviceName`, `Reset`. No native firmware-update or radial-menu support — those are GUI-side and/or daemon extensions.
- Audience evolves: personal use first (Debian), then open-sourced to the Linux community later. Quality of front-end and feature completeness must NOT be skimped even in the personal-use phase.

## Constraints

- **Tech stack**: Linux only; Debian-first. GUI tech is open (to be recommended by research — e.g. Tauri / Qt / GTK). Must speak D-Bus to `logid`. Avoid choices that hard-block future Win/macOS ports.
- **Architecture**: GUI sits on top of the existing `logiops` daemon (D-Bus), but the daemon C++ may be extended/forked when a feature requires it (gestures, action wheel). One combined project/repo.
- **Privilege/IPC**: logiops' D-Bus system-bus policy (`pizza.pixl.LogiOps.conf`) currently grants access to `root` only. A normal-user GUI needs an access path — D-Bus policy change, user-bus build (`USE_USER_BUS`), or polkit. Must be solved.
- **Security**: daemon runs as root and parses untrusted HID input (see `.planning/codebase/CONCERNS.md`). Any daemon extension must respect that threat surface.
- **Daemon limitation**: `logid::Device` currently hardcodes HID++ 2.0 (TODO at `src/logid/Device.h:55`); multi-device richness may need work.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Build GUI on top of existing logiops daemon (D-Bus), not a from-scratch reimplementation | Reuses working HID++ stack; far faster path to real value | — Pending |
| Linux-only, Debian-first; drop cross-platform | logiops is Linux-only; Options+ already exists on Win/macOS | — Pending |
| Extend the logiops C++ daemon when features require it (gestures, action wheel) | User wants real feature parity, and some gaps are daemon-level, not GUI-level | — Pending |
| Faithful visual clone of Options+, high polish from day one | Explicit owner priority — "não economize no front nem nas features" | — Pending |
| GUI stack chosen during research (Tauri/Qt/GTK TBD) | Avoid premature tech lock-in; let domain research inform it | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-30 after Phase 2 (D-Bus Client & Device List) — code complete, human validation pending*
