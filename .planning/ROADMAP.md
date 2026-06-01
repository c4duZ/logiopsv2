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

- [x] **Phase 1: Access Path & Daemon Hardening** - Relax D-Bus policy to a group, polkit-gate writes, sandbox the daemon, length-check HID input so a non-root client can safely reach `pizza.pixl.LogiOps` (completed 2026-05-30)
- [x] **Phase 2: D-Bus Client & Device List** - Qt/QML app shell + typed D-Bus proxy showing a live, signal-driven device list with battery and connection status (completed 2026-05-30)
- [x] **Phase 3: Core Config UI & Persistence** - Visual button remap, DPI, scroll/thumbwheel, manual profiles, and polkit-gated save — the Options+-style "configure without editing text" core (completed 2026-05-31; polkit Save fixed + verified on hardware 2026-05-31)
- [x] **Phase 4: Fine-Grained Gesture Control** - First daemon extension: a guided gesture builder with magnitude/repetition/granularity that fixes "volume +2" and "one desktop per gesture" (built 2026-05-31 — **live-apply defect on hardware tracked in Phase 4.2**)
- [ ] **Phase 4.1: Options+ Reference Mining** (INSERTED) - Extract the Logi Options+ install into our own reference specs: study the `app.asar` UI pixel-by-pixel and parse the readable data (strings vocabulary, macro/Smart-Action schema, `applications.json` app-match DB, overlay/OSD resources, per-app integration configs) into `.planning/intel/options-plus/`. Establish the hybrid/reference-only legal boundary (study, don't bundle proprietary assets) and document the encrypted per-device-DB limitation
- [ ] **Phase 4.2: Gesture Live-Apply Fix & Options+ UX Alignment** (INSERTED) - Fix the daemon gesture child-action wiring so a GUI-built gesture actually fires on hardware (Bug A; root cause in `.planning/debug/gesture-live-apply-and-save.md`), fix read-back + granularity, and align the gesture builder vocabulary/flow to the mined Options+ "predefinição" model
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
  - [x] 01-06-PLAN.md — polkit-gate Configuration::save() + .policy + CMake link (ACCESS-02)
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
**Plans**: 5 plans
  - [x] 02-01-PLAN.md — Wave 0 harness: optional logiops-gui CMake target + RED device_model/battery_feature units + graceful-skip smoke scripts (DEV-01..04, CONF-03)
  - [x] 02-02-PLAN.md — Daemon battery feature: HID++ 0x1000 BatteryStatus wrapper + Battery/Charging D-Bus property + BatteryChanged signal (DEV-02)
  - [x] 02-03-PLAN.md — GUI typed D-Bus proxy + signal-driven DeviceModel (no cache, no-flicker) (DEV-01, DEV-03, DEV-04, CONF-03)
  - [x] 02-04-PLAN.md — GUI shell: main + DaemonConnection (reconnect) + QML sidebar/detail + 4 screen states + theme (DEV-01, DEV-03, DEV-04, CONF-03)
  - [x] 02-05-PLAN.md — GUI battery display wiring + live battery smoke (DEV-02)
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
**Plans**: 5 plans
  - [x] 03-00-PLAN.md — Wave 0: BLOCKING save() round-trip + unknown-key CTest, KeyNameMapper, seven typed D-Bus proxies
  - [x] 03-01-PLAN.md — DeviceController capability model + tabbed DetailPane shell + Theme tokens/glyphs (UI-01)
  - [x] 03-02-PLAN.md — Buttons tab: device render + binding list + non-modal reassign panel + key-capture (BTN-01..04, HOST-01)
  - [x] 03-03-PLAN.md — Pointer + Scroll tabs: DPI slider/cycle/labels + SmartShift/hires/thumbwheel (DPI-01..03, SCR-01..03)
  - [x] 03-04-PLAN.md — Profiles tab + unsaved indicator + async polkit Save + restore-defaults (PROF-01, CONF-01, CONF-02)
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
**Plans**: 4 plans
  - [x] 04-00-PLAN.md — Wave 0: tests/phase4 CTest harness + RED gesture_math (GEST-02/03 accounting) + RED GestureModel scaffold (GEST-01/04)
  - [x] 04-01-PLAN.md — Daemon: fix IntervalGesture granularity defect + defaults::gesture_interval + harden setGesture against untrusted input (GEST-02, GEST-03)
  - [x] 04-02-PLAN.md — GUI brain: GestureModel two-step mode-switch + mode allowlist + preview sentence + granularity readout + typed Action.Gesture proxy (GEST-01..04)
  - [x] 04-03-PLAN.md — GUI: GestureBuilder.qml (direction cross/mode pills/action/granularity/preview) + Theme tokens/glyphs + ReassignPanel category + on-hardware UAT (GEST-01..04)
**UI hint**: yes
**Research flag**: This is the first C++ daemon extension — confirm the magnitude/repetition/granularity abstraction over existing `axis_multiplier`/`threshold`/`interval` and fix uinput recreate-per-code churn while in this code.

### Phase 4.1: Options+ Reference Mining (INSERTED)
**Goal**: We have our own reference specs distilled from the real Logi Options+ install, so every forward phase adapts from how Options+ actually behaves instead of guessing — without bundling Logitech's proprietary assets.
**Depends on**: Nothing new (uses the local `LogiOptionsPlus/` install, gitignored)
**Requirements**: REF-01, REF-02, REF-03 (new — reference-fidelity)
**Success Criteria** (what must be TRUE):
  1. The readable Options+ data is parsed into `.planning/intel/options-plus/` specs we own: gesture/Smart-Action/scroll **vocabulary** (from `data/strings/*.yaml`, incl. pt-BR), the **macro/Smart-Action schema** (`data/macros/predefined_*.json`, `integrations/plugin_*/config.json`), the **app-match DB** (`data/applications.json`), and **overlay/OSD resources** (`data/overlay/*resources.json`)
  2. The `resources/app.asar` UI is extracted and studied; a written design spec captures the tab/layout/interaction model we will re-implement (our own QML, our own strings) — pixel study, not asset bundling
  3. The legal boundary is documented: **hybrid/reference-only** — we adapt behavior and vocabulary, we do NOT ship Logitech assets (matters for Phase 9 packaging)
  4. The encrypted/signed per-device descriptor DB (`data/devices/devices_*.json`) is documented as **not usable**; device capability continues to come from live HID++ enumeration (+ public DBs like Solaar/libratbag)
**Plans**: 4 plans
  - [ ] 04.1-01-PLAN.md — vocabulary.md (gesture/scroll/smart-action terms, pt-BR + en) + legal-boundary.md + device-db-limitation.md (REF-01/03)
  - [ ] 04.1-02-PLAN.md — smart-action-schema.md (macro step/card vocabulary) + app-match-model.md (window-class/executable match rules) (REF-01)
  - [ ] 04.1-03-PLAN.md — extract app.asar to gitignored scratch dir + asar-inventory.md UI navigation map (REF-02)
  - [ ] 04.1-04-PLAN.md — ui-design-spec.md (tab/layout/interaction re-implementation target) + overlay-osd-spec.md (radial/OSD model) (REF-01/02)
**Notes**: Pure analysis/spec phase — no daemon or GUI code ships here; output is intel + design specs that 4.2 and 5–8 consume. Decision (2026-05-31): hybrid adaptation depth.

### Phase 4.2: Gesture Live-Apply Fix & Options+ UX Alignment (INSERTED)
**Goal**: A gesture built in the GUI actually fires on hardware, reads back correctly, and respects granularity — and the gesture builder speaks the Options+ vocabulary/flow.
**Depends on**: Phase 4.1 (vocabulary/UX spec), Phase 4
**Requirements**: GEST-01..04 (rework), GEST-05 (live-apply correctness)
**Success Criteria** (what must be TRUE):
  1. A gesture configured via the GUI on a non-cfg button fires its action on hardware (fixes Bug A: the live-created gesture child action is now wired into the dispatched object, not a detached copy)
  2. The GUI reads back an existing gesture's mode + action (no more "choose what this direction does" on an already-configured button)
  3. Granularity/threshold set from the GUI changes on-hardware behavior
  4. The gesture builder's wording/flow matches the mined Options+ model ("SEGURAR + MOVER PARA…", predefinição vs personalizada)
**Plans**: 4 plans
  - [ ] 04.2-01-PLAN.md — Daemon: converge live Gesture::setAction onto the cfg-load wiring so the child action is introspectable + dispatched, confirm SetThreshold/SetInterval mutate the live gesture, fix the ReleaseGesture/ThresholdGesture "OnRelease" name collision + daemon CTest (GEST-05, GEST-02, GEST-03)
  - [ ] 04.2-02-PLAN.md — GUI: extend GestureModel read-back to resolve the bound action (not just mode/granularity) so the builder reflects existing bindings (GEST-01)
  - [ ] 04.2-03-PLAN.md — UX: align builder copy/flow to the Options+ hold-and-move + preset-vs-custom vocabulary while keeping the GEST-04 plain-language preview (GEST-04, GEST-01)
  - [ ] 04.2-04-PLAN.md — Build/install + on-hardware UAT checkpoint on the MX Master 4 (GEST-05, GEST-01, GEST-02, GEST-03, GEST-04)
**Root cause on file**: `.planning/debug/gesture-live-apply-and-save.md` — daemon dispatch verified correct (Test 1: live `SetKeys` on a cfg-wired action changed hardware behavior); the defect is that a **live** `SetAction("<type>")` on a gesture node creates an action reachable for calls but NOT introspectable/dispatched (Introspect omits `Action.Keypress` after a successful `SetAction`). Bug B (Save/polkit) already fixed + committed (`70e9457`).

### Phase 5: Per-Application Profiles & Profile Sharing
**Goal**: The user's active profile follows the focused application automatically, and profiles can be shared as files.
**Depends on**: Phase 3
**Requirements**: PROF-02, PROF-03, PROF-04, PROF-05
**Success Criteria** (what must be TRUE):
  1. User can have a profile auto-activate when a matching application gains focus, and a default profile cover everything else
  2. User can define app-match rules by window class / executable
  3. Per-application switching works on a Wayland compositor, not only X11 (with documented degradation where the compositor cannot expose the foreground app)
  4. User can export a profile to a shareable file and import one back
**Plans**: 5 plans
  - [ ] 05-00-PLAN.md — Wave 0: phase5 CTest harness + RED match-engine (PROF-03) + RED profile-IO (PROF-04) contracts
  - [ ] 05-01-PLAN.md — Focus-detection spike (X11/wlroots/KWin/Mutter) + degradation matrix + FocusWatcher abstraction + runtime session detection (PROF-05)
  - [ ] 05-02-PLAN.md — Non-root session helper: AppMatchRule/MatchEngine + RuleStore + X11/Wayland watchers + SetProfile wiring (PROF-02, PROF-03, PROF-05)
  - [ ] 05-03-PLAN.md — Profile export/import: ProfileIO versioned-envelope round-trip + validation (PROF-04)
  - [ ] 05-04-PLAN.md — GUI App Rules tab + AppRulesModel + export/import bridge + on-hardware UAT (PROF-02, PROF-03, PROF-04)
**UI hint**: yes
**Research flag**: Wayland foreground-app detection is compositor-specific and possibly partial — verify `wlr-foreign-toplevel` / GNOME-extension / KWin options. Window-watching lives in a non-root user-session agent calling existing `ChangeProfile`. **Adapt from 4.1**: reuse the Options+ `data/applications.json` app-match DB + its window-class/executable match model as the reference for our own rule schema (do not bundle the file).

### Phase 6: Action Wheel
**Goal**: The user can trigger a radial action menu at the cursor, flick toward a slice, and release to fire that action.
**Depends on**: Phase 4
**Requirements**: WHEEL-01, WHEEL-02, WHEEL-03
**Success Criteria** (what must be TRUE):
  1. User can configure a radial wheel with N slices, each bound to an action
  2. User can trigger the wheel with a button, flick toward a slice, and release to fire the chosen action
  3. User sees a radial overlay at the cursor when the wheel is active, degrading gracefully (e.g. centered pop-up) where the compositor cannot render a follow-the-pointer overlay
**Plans**: 4 plans
  - [ ] 06-01-PLAN.md — X11-vs-Wayland overlay spike (click-through follow-cursor proof) → 06-SPIKE.md go/no-go + chosen technique per compositor (WHEEL-03)
  - [ ] 06-02-PLAN.md — Daemon ActionWheel feature/action: N-slice config schema + flick-angle detection from raw XY + fire-on-release + ActiveSlice signal + RED geometry test (WHEEL-01, WHEEL-02)
  - [ ] 06-03-PLAN.md — GUI config: WheelModel two-step write + typed Action.ActionWheel proxy + WheelEditor.qml + ReassignPanel category (WHEEL-01)
  - [ ] 06-04-PLAN.md — GUI overlay: OverlayPlatform (spike technique) + WheelOverlay + RadialOverlay.qml driven by ActiveSlice + on-hardware UAT (WHEEL-03)
**UI hint**: yes
**Research flag**: Highest-risk feature — run a dedicated X11-vs-Wayland overlay spike (GNOME-Mutter vs KDE-KWin vs wlroots; confirm Mutter layer-shell status) BEFORE building. Daemon detects/highlights from HID++ deltas and executes; GUI renders only. **Adapt from 4.1**: the slice-selection mechanic, radial layout, and OSD feel come from the mined `app.asar` study + `data/overlay/osd_resources.json` — re-implement in our own overlay, don't reuse Logitech art.

### Phase 7: Smart Actions / Macros
**Goal**: The user can bind a multi-step action to one button, with any process-launching steps executed safely outside the root daemon.
**Depends on**: Phase 3
**Requirements**: MACRO-01, MACRO-02
**Success Criteria** (what must be TRUE):
  1. User can build an ordered Smart Action of keystroke/text/media steps with inter-step delays, bound to one button, and it fires in order
  2. User can add "launch app / open URL" steps that execute via a user-session helper and never as root
**Plans**: 4 plans
  - [ ] 07-00-PLAN.md — Wave 0 harness: tests/phase7 CTest + RED daemon step-sequencer accounting (ordering + cumulative delay + launch-delegation) + RED SmartActionModel scaffold (MACRO-01, MACRO-02)
  - [ ] 07-01-PLAN.md — Daemon: SmartAction composite action + config::SmartAction step schema + run_task_after sequencer + Launch routed to the documented session-helper IPC contract, never spawned by root (MACRO-01, MACRO-02)
  - [ ] 07-02-PLAN.md — GUI brain: SmartActionModel two-step step-CRUD dispatch + typed Action.SmartAction proxy + Launch isDelegated (MACRO-01, MACRO-02)
  - [ ] 07-03-PLAN.md — GUI: SmartActionBuilder.qml ordered step editor + ReassignPanel Smart Action category + Theme + on-hardware UAT (MACRO-01, MACRO-02)
**UI hint**: yes
**Research flag**: Verify Options+ Smart Action step vocabulary before locking scope. New composite action reuses the worker pool's `run_task_after` for delays; launch/URL steps route to the non-root session helper introduced in Phase 5. **Adapt from 4.1**: the step vocabulary + card model are now concrete — `data/macros/predefined_*.json` (e.g. `APP_WINDOWS_MANAGEMENT`/`BRING_TO_FOREGROUND`, categories) and `integrations/plugin_*/config.json` (per-app cards) define our schema; re-implement, don't bundle.

### Phase 8: Keyboard Backlight
**Goal**: The user can control keyboard backlight/RGB on supported devices.
**Depends on**: Phase 3
**Requirements**: LIGHT-01
**Success Criteria** (what must be TRUE):
  1. On a supported device, the user can change the keyboard backlight/RGB and see the hardware respond
  2. On devices without the LED feature, the backlight controls are hidden or clearly disabled rather than erroring
**Plans**: 3 plans
  - [ ] 08-01-PLAN.md — Reverse-engineering spike: confirm LED/backlight HID++ 2.0 feature ID + function map + target device (08-SPIKE.md) [GATE]
  - [ ] 08-02-PLAN.md — Daemon: hidpp20 Backlight wrapper + high-level DeviceFeature + IPC interface + schema + capability gating (LIGHT-01)
  - [ ] 08-03-PLAN.md — GUI: .Backlight proxy + DeviceController capability + BacklightTab (capability-gated) + on-hardware UAT (LIGHT-01)
**UI hint**: yes
**Research flag**: Greenfield — no LED/Backlight HID++ 2.0 feature wrapper exists in the daemon. Requires reverse-engineering the LED HID++ 2.0 feature per target device; heaviest and most uncertain item. Gate on a confirmed target keyboard. **Adapt from 4.1**: Options+ gives UI vocabulary/strings for backlight controls only; the HID++ LED protocol itself is NOT in the (encrypted) Options+ data — still needs live reverse-engineering (+ OpenRGB/libratbag references).

### Phase 9: Debian Packaging
**Goal**: The app installs cleanly on Debian/Ubuntu as a package that brings its policy, polkit action, and systemd unit, and survives install/upgrade/purge.
**Depends on**: Phase 1
**Requirements**: PKG-01, PKG-02
**Success Criteria** (what must be TRUE):
  1. The `.deb` installs the GUI with a versioned dependency on the `logid` daemon and ships its D-Bus policy, polkit action, and systemd unit
  2. Install, upgrade, and purge all complete cleanly on a fresh system, verified in CI on a clean VM
  3. After a clean install on a fresh VM, a non-root user completes the D-Bus handshake to the daemon (smoke test)
**Plans**: 2 plans
  - [ ] 09-01-PLAN.md — debian/ source tree: two-package split (logiops-daemon ships policy/polkit/unit; logiops-gui + .desktop) with versioned daemon Depends (PKG-01)
  - [ ] 09-02-PLAN.md — package-deb CI: build .deb on clean Debian/Ubuntu + install/upgrade/purge + non-root D-Bus handshake smoke + BLOCKING legal-asset audit (PKG-02)
**Research flag**: Standard `debhelper`/`dh_installsystemd` patterns — main risk is assuming the daemon is pre-installed. Enforce the GUI↔daemon version contract decided in Phase 1; test on clean VMs. **Legal gate (from 4.1)**: the `.deb` ships ONLY our own assets/strings/UI — verify no Logitech proprietary asset (app.asar content, Options+ strings, icons, device data) is bundled. Hybrid/reference-only boundary is a release blocker here.

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 4.1 → 4.2 → 5 → 6 → 7 → 8 → 9

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Access Path & Daemon Hardening | 6/6 | Complete    | 2026-05-30 |
| 2. D-Bus Client & Device List | 5/5 | Complete    | 2026-05-30 |
| 3. Core Config UI & Persistence | 5/5 | Complete    | 2026-05-31 |
| 4. Fine-Grained Gesture Control | 4/4 | Built (live-apply defect → 4.2) | 2026-05-31 |
| 4.1 Options+ Reference Mining (INSERTED) | 0/4 | Not started | - |
| 4.2 Gesture Live-Apply Fix & UX Alignment (INSERTED) | 0/4 | Planned | - |
| 5. Per-Application Profiles & Profile Sharing | 0/5 | Planned | - |
| 6. Action Wheel | 0/4 | Planned | - |
| 7. Smart Actions / Macros | 0/4 | Not started | - |
| 8. Keyboard Backlight | 0/3 | Not started | - |
| 9. Debian Packaging | 0/2 | Not started | - |
