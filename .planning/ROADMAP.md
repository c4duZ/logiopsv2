# Roadmap: Logi Options+ for Linux

## Overview

This project builds a polished, Options+-style Qt 6 + QML GUI on top of the existing
`logiops`/`logid` daemon, extending the C++ daemon only where a feature genuinely requires
it. The journey starts by unblocking the one thing that gates everything — the root-only
D-Bus policy — and hardening the root daemon as a prerequisite of widening its audience.
From there it delivers core value early as pure D-Bus-client work (see-my-mouse device list,
then live button/DPI/scroll/profile configuration without touching `/etc/logid.cfg`). The
first daemon extension is the owner's priority pain point — fine-grained gestures — chosen
deliberately to de-risk the C++ extension path before the highest-risk flagship feature, the
radial action wheel (which needs a Wayland overlay spike). Per-app profiles, macros, and the
greenfield backlight feature follow as increasingly uncertain work, and Debian-first packaging
lands last, tested on clean VMs.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Access Path & Daemon Hardening** - Relax D-Bus policy to a group, polkit-gate writes, sandbox the daemon, length-check HID input so a non-root client can safely reach `pizza.pixl.LogiOps`
- [ ] **Phase 2: D-Bus Client & Device List** - Qt/QML app shell + typed D-Bus proxy showing a live, signal-driven device list with battery and connection status
- [ ] **Phase 3: Core Config UI & Persistence** - Visual button remap, DPI, scroll/thumbwheel, manual profiles, and polkit-gated save — the Options+-style "configure without editing text" core
- [ ] **Phase 4: Fine-Grained Gesture Control** - First daemon extension: a guided gesture builder with magnitude/repetition/granularity that fixes "volume +2" and "one desktop per gesture"
- [ ] **Phase 5: Per-Application Profiles & Profile Sharing** - User-session focus watcher (X11 + Wayland) auto-switching profiles by app, plus profile import/export
- [ ] **Phase 6: Action Wheel** - Flagship radial action menu: new daemon `ActionWheel` feature + GUI overlay (X11 → Wayland layer-shell → fallback), spiked first
- [ ] **Phase 7: Smart Actions / Macros** - Composite multi-step action (keystrokes/text/media/delays) with launch/open-URL steps routed through a non-root session helper
- [ ] **Phase 8: Keyboard Backlight** - Greenfield HID++ 2.0 LED feature wrapper in the daemon plus backlight/RGB UI on supported devices
- [ ] **Phase 9: Debian Packaging** - `.deb` shipping the D-Bus policy, polkit action, and systemd unit with a versioned daemon dependency, verified install/upgrade/purge in CI

## Phase Details

### Phase 1: Access Path & Daemon Hardening
**Goal**: A non-root user can safely reach the `logid` D-Bus service without widening the root attack surface, and privileged config writes are authorization-gated.
**Depends on**: Nothing (first phase)
**Requirements**: ACCESS-01, ACCESS-02, ACCESS-03, ACCESS-04
**Success Criteria** (what must be TRUE):
  1. A user in the `logiops` group can call a method on `pizza.pixl.LogiOps` and change a device setting without running as root (verified by a non-root smoke test)
  2. Triggering a config persist (`Configuration::save()`) prompts a polkit authorization, and denying it leaves `/etc/logid.cfg` unchanged
  3. The running daemon shows systemd hardening in effect (`NoNewPrivileges`, `ProtectSystem`, restricted capabilities) verifiable via `systemctl show`/`systemd-analyze security`
  4. A malformed/short HID report does not cause an out-of-bounds read — report fields are length-checked before indexing
**Plans**: 6 plans
  - [x] 01-01-PLAN.md — Wave 0 test harness: CTest hidpp-bounds unit + ACCESS-01/02/03 smoke scripts
  - [x] 01-02-PLAN.md — D-Bus policy relaxation to group=logiops (ACCESS-01)
  - [x] 01-03-PLAN.md — systemd hardening of logid.service (ACCESS-03)
  - [x] 01-04-PLAN.md — HID length-checks + CONCERNS #1/#5/#8 fixes (ACCESS-04)
  - [x] 01-05-PLAN.md — ipcgull caller-threading: ipcgull::current_caller() (ACCESS-02 foundation)
  - [ ] 01-06-PLAN.md — polkit-gate Configuration::save() + .policy + CMake link (ACCESS-02)
**Research flag**: Re-verify exact D-Bus policy `group=` syntax and current polkit defaults (`auth_admin_keep`) against live `dbus-daemon`/`polkit` docs before hardening.

### Phase 2: D-Bus Client & Device List
**Goal**: The user opens the app and sees their connected Logitech devices with live status, with the daemon as the single source of truth.
**Depends on**: Phase 1
**Requirements**: DEV-01, DEV-02, DEV-03, DEV-04, CONF-03
**Success Criteria** (what must be TRUE):
  1. User sees a list of all connected Logitech devices, each labeled with its name/model
  2. User sees live battery percentage and charging state per device, updating as it changes
  3. User sees connection status update on hotplug (plug/unplug, sleep/wake) without restarting the app
  4. Device state shown in the GUI always reflects the daemon's live state via signals — no stale cached list
**Plans**: TBD
**UI hint**: yes

### Phase 3: Core Config UI & Persistence
**Goal**: The user can fully configure a device's buttons, pointer, scroll, and manual profiles through a faithful Options+-style GUI and persist changes — never editing a text file.
**Depends on**: Phase 2
**Requirements**: BTN-01, BTN-02, BTN-03, BTN-04, DPI-01, DPI-02, DPI-03, SCR-01, SCR-02, SCR-03, HOST-01, PROF-01, CONF-01, CONF-02, UI-01
**Success Criteria** (what must be TRUE):
  1. User can click a button on a per-model device render, reassign it (keystroke, DPI change/cycle, host switch, profile switch, SmartShift/hi-res toggle), and see each button's current binding at a glance
  2. User can adjust DPI with a min/max-bounded slider, define a cycle of DPI levels, and label those presets
  3. User can toggle/tune SmartShift, toggle and invert hi-res scroll, and configure thumbwheel behavior on supported devices
  4. User can create/name/switch manual profiles, apply changes live, and persist them via a polkit-gated save without editing `/etc/logid.cfg`
  5. User can restore a device to default settings, and the app presents a polished, tabbed Options+-style interface
**Plans**: TBD
**UI hint**: yes
**Research flag**: Verify `Configuration::save()` round-trip fidelity (libconfig comment/formatting preservation) before relying on it as the only persistence path.

### Phase 4: Fine-Grained Gesture Control
**Goal**: The user can build gestures that fire exactly once or repeat predictably, fixing the owner's concrete pain ("volume steps by 2", "only one desktop switch").
**Depends on**: Phase 3
**Requirements**: GEST-01, GEST-02, GEST-03, GEST-04
**Success Criteria** (what must be TRUE):
  1. User can build a gesture by picking direction → mode → action through a guided UI
  2. User can set granularity so one gesture maps to exactly one discrete step (one volume tick, one desktop switch)
  3. User can make a gesture repeat per interval (e.g. switch several desktops in one continuous motion)
  4. The gesture UI explains in plain language when and how often the action will fire
**Plans**: TBD
**UI hint**: yes
**Research flag**: This is the first C++ daemon extension — confirm the magnitude/repetition/granularity abstraction over existing `axis_multiplier`/`threshold`/`interval` and fix uinput recreate-per-code churn while in this code.

### Phase 5: Per-Application Profiles & Profile Sharing
**Goal**: The user's active profile follows the focused application automatically, and profiles can be shared as files.
**Depends on**: Phase 3
**Requirements**: PROF-02, PROF-03, PROF-04, PROF-05
**Success Criteria** (what must be TRUE):
  1. User can have a profile auto-activate when a matching application gains focus, and a default profile cover everything else
  2. User can define app-match rules by window class / executable
  3. Per-application switching works on a Wayland compositor, not only X11 (with documented degradation where the compositor cannot expose the foreground app)
  4. User can export a profile to a shareable file and import one back
**Plans**: TBD
**UI hint**: yes
**Research flag**: Wayland foreground-app detection is compositor-specific and possibly partial — verify `wlr-foreign-toplevel` / GNOME-extension / KWin options. Window-watching lives in a non-root user-session agent calling existing `ChangeProfile`.

### Phase 6: Action Wheel
**Goal**: The user can trigger a radial action menu at the cursor, flick toward a slice, and release to fire that action.
**Depends on**: Phase 4
**Requirements**: WHEEL-01, WHEEL-02, WHEEL-03
**Success Criteria** (what must be TRUE):
  1. User can configure a radial wheel with N slices, each bound to an action
  2. User can trigger the wheel with a button, flick toward a slice, and release to fire the chosen action
  3. User sees a radial overlay at the cursor when the wheel is active, degrading gracefully (e.g. centered pop-up) where the compositor cannot render a follow-the-pointer overlay
**Plans**: TBD
**UI hint**: yes
**Research flag**: Highest-risk feature — run a dedicated X11-vs-Wayland overlay spike (GNOME-Mutter vs KDE-KWin vs wlroots; confirm Mutter layer-shell status) BEFORE building. Daemon detects/highlights from HID++ deltas and executes; GUI renders only. Verify Options+ slice-selection mechanic for feel.

### Phase 7: Smart Actions / Macros
**Goal**: The user can bind a multi-step action to one button, with any process-launching steps executed safely outside the root daemon.
**Depends on**: Phase 3
**Requirements**: MACRO-01, MACRO-02
**Success Criteria** (what must be TRUE):
  1. User can build an ordered Smart Action of keystroke/text/media steps with inter-step delays, bound to one button, and it fires in order
  2. User can add "launch app / open URL" steps that execute via a user-session helper and never as root
**Plans**: TBD
**UI hint**: yes
**Research flag**: Verify Options+ Smart Action step vocabulary before locking scope. New composite action reuses the worker pool's `run_task_after` for delays; launch/URL steps route to the non-root session helper introduced in Phase 5.

### Phase 8: Keyboard Backlight
**Goal**: The user can control keyboard backlight/RGB on supported devices.
**Depends on**: Phase 3
**Requirements**: LIGHT-01
**Success Criteria** (what must be TRUE):
  1. On a supported device, the user can change the keyboard backlight/RGB and see the hardware respond
  2. On devices without the LED feature, the backlight controls are hidden or clearly disabled rather than erroring
**Plans**: TBD
**UI hint**: yes
**Research flag**: Greenfield — no LED/Backlight HID++ 2.0 feature wrapper exists in the daemon. Requires reverse-engineering the LED HID++ 2.0 feature per target device; heaviest and most uncertain item. Gate on a confirmed target keyboard.

### Phase 9: Debian Packaging
**Goal**: The app installs cleanly on Debian/Ubuntu as a package that brings its policy, polkit action, and systemd unit, and survives install/upgrade/purge.
**Depends on**: Phase 1
**Requirements**: PKG-01, PKG-02
**Success Criteria** (what must be TRUE):
  1. The `.deb` installs the GUI with a versioned dependency on the `logid` daemon and ships its D-Bus policy, polkit action, and systemd unit
  2. Install, upgrade, and purge all complete cleanly on a fresh system, verified in CI on a clean VM
  3. After a clean install on a fresh VM, a non-root user completes the D-Bus handshake to the daemon (smoke test)
**Plans**: TBD
**Research flag**: Standard `debhelper`/`dh_installsystemd` patterns — main risk is assuming the daemon is pre-installed. Enforce the GUI↔daemon version contract decided in Phase 1; test on clean VMs.

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Access Path & Daemon Hardening | 5/6 | In Progress|  |
| 2. D-Bus Client & Device List | 0/TBD | Not started | - |
| 3. Core Config UI & Persistence | 0/TBD | Not started | - |
| 4. Fine-Grained Gesture Control | 0/TBD | Not started | - |
| 5. Per-Application Profiles & Profile Sharing | 0/TBD | Not started | - |
| 6. Action Wheel | 0/TBD | Not started | - |
| 7. Smart Actions / Macros | 0/TBD | Not started | - |
| 8. Keyboard Backlight | 0/TBD | Not started | - |
| 9. Debian Packaging | 0/TBD | Not started | - |
