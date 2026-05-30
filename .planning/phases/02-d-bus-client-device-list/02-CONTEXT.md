# Phase 2: D-Bus Client & Device List - Context

**Gathered:** 2026-05-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Build the Qt 6 + QML application shell and a typed D-Bus client that opens to a live, signal-driven list of connected Logitech devices with battery and connection status — the daemon as single source of truth (DEV-01..04, CONF-03). This is the foundation every later GUI phase builds on (the app target, the D-Bus proxy, the C++↔QML model bridge, reconnection, and access onboarding).

In scope: a new GUI CMake target, the generated/typed D-Bus proxy over `pizza.pixl.LogiOps`, a device-list model + sidebar/detail shell, live status via D-Bus signals, hotplug/sleep handling, daemon reconnect, and access/empty/error states. Out of scope: per-model device renders (Fase 3, UI-01), any device CONFIGURATION (buttons/DPI/scroll/profiles — Fase 3), gestures/action-wheel/etc.
</domain>

<decisions>
## Implementation Decisions

### App Shell & Project Structure
- New CMake target under `src/logiops-gui/` in this same repo (same build system as the daemon; Qt 6 + QML / Qt Quick Controls 2, C++20 — per research STACK.md).
- Single-window layout: a device sidebar (list) + a detail pane (Options+-style). Detail pane in this phase shows read-only device status; configuration UI lands in Fase 3.
- Generate a typed C++ D-Bus proxy from the daemon's introspection of `pizza.pixl.LogiOps` via `qdbusxml2cpp` (reuse `ipc_defs.h` constants for bus name / root path).
- Expose daemon state to QML through C++ model classes (a `QAbstractListModel` device-list model); QML renders only — no business logic in QML/JS.

### Device List Presentation
- List item shows: device name/model, battery (numeric % + icon + charging state), and connection status.
- Device imagery: generic per-kind icon (mouse / keyboard) for v1; per-model renders are deferred to Fase 3 (UI-01).
- Battery: numeric % + icon + charging indicator; show "—"/unknown when the device doesn't report `WirelessDeviceStatus`.
- Offline/sleeping devices stay visible but dimmed with an "offline"/"sleeping" badge (do not drop from the list).

### D-Bus Client Architecture
- Daemon is the single source of truth (CONF-03). The GUI never caches a static device list.
- Live updates by subscribing to D-Bus signals (device add/remove, status/battery change) → update the model; no polling.
- On daemon restart/crash: detect `NameOwnerChanged` on the bus, then auto-reconnect and re-enumerate (no app restart required).
- Access denied (caller not in `logiops` group): show an onboarding screen explaining `sudo usermod -aG logiops $USER` + re-login — distinct from a generic error.

### States & Theme
- No devices: friendly empty state ("connect a Logitech device").
- Loading: spinner/skeleton while enumerating.
- Theme: follow the system light/dark theme via Qt, with an Options+-flavored own identity (custom-skinned, per research).
- Distinguish daemon-not-running vs no-access vs no-devices with clear, separate messages.

### Claude's Discretion
- Exact QML component breakdown, model role names, signal-wiring details, and the qdbusxml2cpp invocation specifics are at Claude's discretion, guided by the UI-SPEC (generated next) and Qt/QtDBus conventions.
</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- D-Bus identity: `src/logid/ipc_defs.h` (`SERVICE_ROOT_NAME = pizza.pixl.LogiOps`, root node `/pizza/pixl/logiops`). The GUI proxy should reuse these constants.
- Phase 1 delivered the non-root access path (group `logiops` + relaxed D-Bus policy), so a non-root GUI can now reach the bus. The access-denied onboarding ties to that group.
- The daemon exposes devices/receivers/config as ipcgull objects under the root node (see `.planning/codebase/ARCHITECTURE.md` / `INTEGRATIONS.md`); battery/connection come from the `DeviceStatus`/`WirelessDeviceStatus` surface.

### Established Patterns
- Repo is C++20 + CMake; the GUI is another CMake target. Qt 6.4.2 verified available on the target (Ubuntu 24.04 noble) per research STACK.md, with QtDBus (`libqt6dbus6`) and `qml6-module-qtquick-shapes`.
- Follow `.planning/codebase/CONVENTIONS.md` for C++ style.

### Integration Points
- All daemon communication flows through the generated D-Bus proxy. No direct hidraw/hardware access from the GUI — ever.
- `NameOwnerChanged` on the system bus is the reconnect trigger.
</code_context>

<specifics>
## Specific Ideas

- The "no stale cached list" criterion (CONF-03) is load-bearing: the model must be driven entirely by enumeration + signals, never a snapshot the GUI holds independently.
- Use the system bus (the daemon's default), matching Phase 1's policy work; do not switch to USE_USER_BUS.
- Research flag: confirm the daemon actually emits the device-add/remove and status-change D-Bus signals the live model needs; if a needed signal is missing on the ipcgull surface, note it (may require a tiny daemon signal addition) rather than polling.
</specifics>

<deferred>
## Deferred Ideas

- Per-model device artwork / faithful Options+ device renders → Fase 3 (UI-01).
- Any device configuration (buttons, DPI, scroll, profiles, persistence) → Fase 3.
- Battery "predictive estimate" → out of scope (anti-feature); show raw % only.
</deferred>
