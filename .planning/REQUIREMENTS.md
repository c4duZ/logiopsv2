# Requirements: Logi Options+ for Linux

**Defined:** 2026-05-30
**Core Value:** Bring the full Logitech Options+ experience to Linux — configure a Logitech device through a real GUI instead of hand-editing `/etc/logid.cfg`.

## v1 Requirements

Requirements for the initial milestone. Built as a Qt 6 + QML GUI driving the existing `logid` daemon over D-Bus, extending the daemon's C++ only where a feature requires it. Each maps to a roadmap phase.

### Access & Security

- [x] **ACCESS-01**: A non-root user (in a `logiops` group) can connect to the `logid` D-Bus service and control devices
- [x] **ACCESS-02**: Privileged config persistence (writing `/etc/logid.cfg` via `Configuration::save()`) is gated behind a polkit authorization
- [x] **ACCESS-03**: The daemon is sandboxed with systemd hardening (`NoNewPrivileges`, `ProtectSystem`, capability bounding) so widening D-Bus access does not widen the root attack surface
- [x] **ACCESS-04**: Incoming HID report fields are length-checked before indexing, removing the unbounded-read surface on untrusted hardware input

### Devices

- [x] **DEV-01**: User can see a list of all connected Logitech devices in the app
- [x] **DEV-02**: User can see live battery percentage and charging state per device
- [x] **DEV-03**: User can see live connection status (online/offline, receiver-paired) that updates on hotplug without restarting the app
- [x] **DEV-04**: User can see each device's name/model

### Button Remapping

- [x] **BTN-01**: User can click a button on a visual device representation and reassign its action
- [x] **BTN-02**: User can assign a keystroke/shortcut to a button via a key-capture widget
- [x] **BTN-03**: User can assign built-in actions to a button (DPI change/cycle, host switch, profile switch, SmartShift/hi-res toggles)
- [x] **BTN-04**: User can see at a glance what each button is currently bound to

### Pointer / DPI

- [x] **DPI-01**: User can adjust DPI/sensitivity with a slider bounded by the device's reported min/max
- [x] **DPI-02**: User can define a set of DPI levels to cycle through
- [x] **DPI-03**: User can label DPI presets with names

### Scroll

- [x] **SCR-01**: User can toggle and tune SmartShift (on/off, threshold, torque)
- [x] **SCR-02**: User can toggle hi-res scrolling and invert scroll direction
- [x] **SCR-03**: User can configure thumbwheel behavior (divert/invert, left/right/tap actions) on supported devices

### Multi-Host

- [x] **HOST-01**: User can assign an Easy-Switch (change-host) action to a button to hop between paired computers

### Gestures (priority differentiator)

- [ ] **GEST-01**: User can build a gesture by picking direction → mode → action through a guided UI
- [ ] **GEST-02**: User can control gesture granularity so one gesture maps to one discrete step — fixing "volume steps by 2" and "only 1 desktop switch" — by exposing interval/threshold/multiplier with sane defaults
- [ ] **GEST-03**: User can make a gesture repeat per interval (e.g. switch multiple desktops in one continuous motion)
- [ ] **GEST-04**: The gesture UI explains in plain language when/how often an action will fire
- [ ] **GEST-05**: A gesture configured through the GUI takes effect live on hardware — the action fires, the builder reads back the existing binding, and granularity changes behavior (live-apply correctness; added Phase 4.2 after on-hardware UAT exposed the live-apply defect)

### Profiles

- [x] **PROF-01**: User can create, name, and manually switch between configuration profiles
- [ ] **PROF-02**: User can have a profile auto-activate based on the focused application (per-app profiles)
- [ ] **PROF-03**: User can define app-match rules (window class / executable) for per-app profile switching
- [ ] **PROF-04**: User can export a profile to a shareable file and import one
- [ ] **PROF-05**: Per-application profile switching works on Wayland compositors (not just X11)

### Action Wheel (flagship)

- [ ] **WHEEL-01**: User can configure a radial action wheel with N slices, each bound to an action
- [ ] **WHEEL-02**: User can trigger the wheel with a button, flick toward a slice, and release to fire the chosen action
- [ ] **WHEEL-03**: User sees a radial overlay at the cursor when the wheel is active, with graceful degradation where the compositor cannot render overlays

### Smart Actions / Macros

- [ ] **MACRO-01**: User can build a multi-step Smart Action (ordered keystrokes/text/media steps with inter-step delays) bound to one button
- [ ] **MACRO-02**: User can add "launch app / open URL" steps, executed via a user-session helper (never as root)

### Backlight (greenfield — heaviest/most uncertain)

- [ ] **LIGHT-01**: User can control keyboard backlight/RGB on supported devices (requires a new HID++ 2.0 LED feature wrapper in the daemon)

### Config & Persistence

- [x] **CONF-01**: User can apply changes live and persist them without editing any text file
- [x] **CONF-02**: User can restore a device to default settings
- [x] **CONF-03**: GUI state stays in sync with the daemon's live state (signal-driven; no stale config), with the daemon as single source of truth

### Visual / UX

- [x] **UI-01**: The app presents a faithful Options+-style interface (per-model device renders, tabbed layout, polished and animated)

### Packaging

- [ ] **PKG-01**: The app installs on Debian/Ubuntu as a package that depends on the `logid` daemon and ships its D-Bus policy, polkit action, and systemd unit
- [ ] **PKG-02**: Install / upgrade / purge work cleanly on a fresh system, verified in CI

## v2 Requirements

None — per project owner, the full feature set is committed to v1. Future milestones will be defined after v1 ships.

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Logi Flow (cursor crosses between computers, clipboard/file sync) | Requires cross-machine network/cloud service logiops doesn't have; huge surface. Easy-Switch button (HOST-01) covers the hardware host-switch; point users to input-leap/barrier for cursor-crossing |
| Firmware / DFU updates | No firmware path in logiops; bricking risk. Detect & link to Logitech's tool instead |
| Logitech account / cloud sync / AI Prompt Builder / Smart Actions marketplace | Requires vendor cloud auth; privacy + vendor dependency; offline-Linux users don't want it |
| Telemetry / usage insights / notification center | Privacy-hostile, no value for a local tool |
| Predictive/AI battery estimates | Marketing layer over raw battery %; show raw % + charging state instead |
| App auto-update outside apt | On Debian, apt/`.deb` is the update path |
| Windows / macOS support | logiops is Linux-only and Options+ already exists there; stack chosen to not *block* a future port, but no non-Linux backend is built |
| Reimplementing HID++ from scratch | The logiops daemon already does this; we build on it |

## Traceability

Which phases cover which requirements. Populated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| ACCESS-01 | Phase 1 | Complete |
| ACCESS-02 | Phase 1 | Complete |
| ACCESS-03 | Phase 1 | Complete |
| ACCESS-04 | Phase 1 | Complete |
| DEV-01 | Phase 2 | Complete |
| DEV-02 | Phase 2 | Complete |
| DEV-03 | Phase 2 | Complete |
| DEV-04 | Phase 2 | Complete |
| CONF-03 | Phase 2 | Complete |
| BTN-01 | Phase 3 | Complete |
| BTN-02 | Phase 3 | Complete |
| BTN-03 | Phase 3 | Complete |
| BTN-04 | Phase 3 | Complete |
| DPI-01 | Phase 3 | Complete |
| DPI-02 | Phase 3 | Complete |
| DPI-03 | Phase 3 | Complete |
| SCR-01 | Phase 3 | Complete |
| SCR-02 | Phase 3 | Complete |
| SCR-03 | Phase 3 | Complete |
| HOST-01 | Phase 3 | Complete |
| PROF-01 | Phase 3 | Complete |
| CONF-01 | Phase 3 | Complete |
| CONF-02 | Phase 3 | Complete |
| UI-01 | Phase 3 | Complete |
| GEST-01 | Phase 4 | Pending |
| GEST-02 | Phase 4 | Pending |
| GEST-03 | Phase 4 | Pending |
| GEST-04 | Phase 4 | Pending |
| GEST-05 | Phase 4.2 | Pending |
| PROF-02 | Phase 5 | Pending |
| PROF-03 | Phase 5 | Pending |
| PROF-04 | Phase 5 | Pending |
| PROF-05 | Phase 5 | Pending |
| WHEEL-01 | Phase 6 | Pending |
| WHEEL-02 | Phase 6 | Pending |
| WHEEL-03 | Phase 6 | Pending |
| MACRO-01 | Phase 7 | Pending |
| MACRO-02 | Phase 7 | Pending |
| LIGHT-01 | Phase 8 | Pending |
| PKG-01 | Phase 9 | Pending |
| PKG-02 | Phase 9 | Pending |

**Coverage:**
- v1 requirements: 40 total
- Mapped to phases: 40 ✓
- Unmapped: 0 ✓

*Note: prior drafts referenced "35" requirements; the actual enumerated v1 requirement count is 40 (all committed to v1).*

---
*Requirements defined: 2026-05-30*
*Last updated: 2026-05-30 after roadmap creation (traceability populated)*
