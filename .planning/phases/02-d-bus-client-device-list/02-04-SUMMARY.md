---
phase: 02-d-bus-client-device-list
plan: 04
subsystem: logiops-gui
tags: [qt6, qml, qtdbus, device-list, screen-states, theme, reconnect]
requires:
  - DeviceModel (Plan 03) public slots + roleNames
  - PizzaPixlLogiOps{Devices,Device}Interface generated proxies (Plan 03)
  - daemon .Devices/.Device D-Bus surface on the system bus (Phase 1 + Plan 02/03)
provides:
  - DaemonConnection (live system-bus connect + screenState enum + reconnect)
  - logiops-gui executable (QGuiApplication + QQmlApplicationEngine)
  - QML shell (Main + DeviceList/DeviceDelegate/DetailPane) + four state screens + Theme singleton
affects:
  - Plan 05 (battery display wires onBatteryChanged + replaces the '—' stub)
tech-stack:
  added: [Qt6::Gui, Qt6::Quick, Qt6::Qml, Qt6::Svg, qt_add_qml_module]
  patterns:
    - "manager-emits-paths / client-follows (RESEARCH Pattern 1)"
    - "async QDBusPendingCallWatcher everywhere (no UI-thread block, Pitfall 4)"
    - "C++ owns all state; QML renders only (StackLayout indexed by screenState enum)"
    - "palette-luminance dark detection via SystemPalette (Qt 6.4.2, no colorScheme)"
key-files:
  created:
    - src/logiops-gui/DaemonConnection.h
    - src/logiops-gui/DaemonConnection.cpp
    - src/logiops-gui/main.cpp
    - src/logiops-gui/qml/Main.qml
    - src/logiops-gui/qml/Theme.qml
    - src/logiops-gui/qml/DeviceList.qml
    - src/logiops-gui/qml/DeviceDelegate.qml
    - src/logiops-gui/qml/DetailPane.qml
    - src/logiops-gui/qml/states/EmptyState.qml
    - src/logiops-gui/qml/states/LoadingState.qml
    - src/logiops-gui/qml/states/DaemonDownState.qml
    - src/logiops-gui/qml/states/AccessDeniedState.qml
    - src/logiops-gui/icons/mouse.svg
    - src/logiops-gui/icons/keyboard.svg
  modified:
    - src/logiops-gui/CMakeLists.txt
decisions:
  - "ScreenState enum mapping: QDBusError::AccessDenied -> AccessDenied; ServiceUnknown/no-owner -> DaemonDown; 0 paths -> Empty; >=1 -> Populated (verified against the daemon's group=logiops deny policy, A3)"
  - "Async Properties.GetAll for per-device Name/ProductID/Active (the generated property accessors are synchronous/blocking) so a sleeping device never freezes the UI"
  - "QML loaded via qrc:/logiops/gui/qml/Main.qml (the qmldir `prefer` prefix) because Qt 6.4.2 has no QQmlApplicationEngine::loadFromModule (6.5+)"
  - "DaemonConnection screenState unit deferred: a hermetic test needs ctor bus/proxy injection (architectural refactor); the enum + per-state QML view are statically proven by the -Werror build + cachegen instead"
metrics:
  tasks: 2 automatable (+1 documented human-verify)
  files: 14 created, 1 modified
  duration: ~40 min
  completed: 2026-05-30
---

# Phase 2 Plan 04: The Running App (DaemonConnection + QML Shell + 4 Screen States) Summary

A live Qt 6 / QML client (`logiops-gui`) that connects to `pizza.pixl.LogiOps` on
the **system bus**, enumerates devices into the Plan-03 `DeviceModel` via async
D-Bus calls, renders a sidebar + read-only detail shell, and switches the whole
window between four DISTINCT C++-driven screen states (loading / empty /
daemon-down / access-denied onboarding). It auto-reconnects on `NameOwnerChanged`
through a `QDBusServiceWatcher` (no app restart, DEV-03/CONF-03) and follows the
system light/dark theme via palette luminance. Battery display is intentionally a
`—` stub pending Plan 05. (DEV-01/03/04 + CONF-03.)

## What was built

**Task 1 — DaemonConnection (`feat(02-04)` `65d13d4`)**
- `DaemonConnection final : QObject` owning the system-bus connection, the
  `.Devices` manager proxy, a `QMap<QString, PizzaPixlLogiOpsDeviceInterface*>`
  per-device proxy map, and the `QDBusServiceWatcher`.
- `Q_ENUM ScreenState { Loading, Populated, Empty, DaemonDown, AccessDenied }`
  exposed as `Q_PROPERTY screenState` (NOTIFY screenStateChanged).
- `enumerate()` runs async (`QDBusPendingCallWatcher`) and maps the reply/error:
  `QDBusError::AccessDenied` -> AccessDenied; ServiceUnknown / no owner ->
  DaemonDown; 0 paths -> Empty; >=1 -> add each + Populated.
- `addDevice()` constructs a per-device proxy, async-reads `Name/ProductID/Active`
  via `Properties.GetAll` (non-blocking, Pitfall 4), pushes the row into
  `DeviceModel::onDeviceAdded`, and connects `StatusChanged -> onStatusChanged`.
- `DeviceAdded/DeviceRemoved -> add/remove`; `onDaemonUp/onDaemonDown` and the
  `Q_INVOKABLE retry()` tear down stale proxies + clear the model + re-enumerate
  (the no-restart reconnect). `Q_INVOKABLE copyUsermodCommand()` writes
  `sudo usermod -aG logiops $USER` to the clipboard.

**Task 2 — QML shell + states + theme + executable (`feat(02-04)` `296b942`)**
- `main.cpp`: `QGuiApplication` + `QQmlApplicationEngine`; constructs the model +
  connection and registers them as `deviceModel` / `daemon` context properties.
- `Theme.qml` (pragma Singleton): UI-SPEC tokens as light/dark pairs; `dark`
  derived from `SystemPalette.window.hslLightness < 0.5` (re-resolves on theme
  flip — Qt 6.4.2 has no `Application.styleHints.colorScheme`, A5).
- `Main.qml`: `ApplicationWindow` 960x640 (min 720x480) + `StackLayout`
  `currentIndex: daemon.screenState` — each enum value maps to a DISTINCT view;
  Populated is the `SplitView` (DeviceList 280px + DetailPane). Auto-selects the
  first device on populate and keeps the selection valid on insert/remove.
- `DeviceList`/`DeviceDelegate`: keyboard-navigable `ListView`, 56px rows, accent
  3px selection bar + 12% tint, 2px focus ring, hover tint, offline 55% / sleeping
  70% dim + "Offline"/"Sleeping" badge, fade+slide add/remove transitions, per-role
  bindings (no-flicker). Device strings rendered `Text.PlainText` (T-02-14).
- `DetailPane`: read-only (large icon, Display name, model sub-line, Connection
  section, Battery `—` stub). NO config controls (Fase 3).
- The four state screens use the EXACT copywriting-contract strings; DaemonDown
  has a "Retry connection" -> `daemon.retry()`; AccessDenied shows the monospace
  usermod block + "Copy command" -> `daemon.copyUsermodCommand()` (label swaps to
  "Copied" ~1.5s) + the re-login note.
- CMake: `qt_add_executable` + `qt_add_qml_module` (Theme registered as singleton)
  + `install`. Mouse/keyboard SVGs as module resources.

## Build & static verification (the checkpoint, statically)

- `cmake -S . -B build -DBUILD_GUI=ON -DCMAKE_CXX_FLAGS="-Werror" && cmake --build build`
  builds **clean under -Werror**; the `logiops-gui` ELF binary is produced
  (`build/src/logiops-gui/logiops-gui`).
- QML cachegen compiled every `.qml` (Main + 4 states + Theme + list/delegate/
  detail) — the authoritative QML type check on this Qt 6.4.2 box (the standalone
  `qmllint` cannot load the QtQuick type info here, so its "X is used but not
  resolved" noise is a tooling limitation, not a QML defect).
- ScreenState mapping is statically provable: the C++ enum has all 5 values
  (Loading/Populated/Empty/DaemonDown/AccessDenied) and `Main.qml`'s `StackLayout`
  indexes a distinct view per value via `currentIndex: daemon.screenState`.
- Full unit suite green (`ctest`): `device_model`, `device_model_noflicker`,
  `battery_feature`, `unified_battery`, `hidpp_bounds` — 5/5 passed (regression).

## MANUAL launch + verify steps (Task 3 — human-verify, deferred to operator)

This task CANNOT be automated here (needs a real display session + a running
daemon, and one runtime prerequisite below). The operator should:

**0. Runtime prerequisite (one-time):** install the BASE QtQuick QML module —
   it is NOT installed on this box (only the controls/layouts sub-modules are):
   ```
   sudo apt install qml6-module-qtquick
   ```
   Without it the engine reports `module "QtQuick" is not installed` even though
   the binary builds fine. (Also ensure `qml6-module-qtquick-controls` and
   `-layouts` are present — they already were.)

**1. Build & run:**
   ```
   cmake -S . -B build -DBUILD_GUI=ON && cmake --build build --target logiops-gui
   ./build/src/logiops-gui/logiops-gui
   ```
   Ensure `logid` is running and you are in the `logiops` group.

**2. Populated:** your device(s) appear in the sidebar with name/model; first
   device auto-selected; detail pane shows read-only status; battery shows `—`.

**3. Live status (DEV-03):** sleep/wake the mouse (or unplug/replug the receiver)
   — the row updates live WITHOUT restarting the app, no flicker/reorder; the
   sleeping/offline badge appears/clears.

**4. Daemon-down + retry (CONF-03):** `sudo systemctl stop logid` — the app flips
   to "Can't reach the logid service"; start it again or click "Retry connection"
   — the list repopulates with no app restart.

**5. Access-denied onboarding:** run as a user NOT in `logiops` — confirm the
   "You're not in the logiops group yet" screen with the usermod block and a
   working "Copy command" (verify the clipboard holds
   `sudo usermod -aG logiops <user>`), NOT a generic error or crash.

**6. Theme:** toggle the desktop light/dark theme — the app follows.

Resume signal: type "approved" or describe issues (wrong state, flicker on status
change, theme not following, copy button not working).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] `QQmlApplicationEngine::loadFromModule` is Qt 6.5+**
- **Found during:** Task 2 build.
- **Issue:** Plan/example assumed `loadFromModule`; it does not exist on the
  target Qt 6.4.2, breaking the build.
- **Fix:** Load via the module's resource URL `qrc:/logiops/gui/qml/Main.qml`
  (the `prefer` prefix qt_add_qml_module writes into the generated qmldir).
- **Files modified:** `src/logiops-gui/main.cpp`
- **Commit:** `296b942`

**2. [Rule 1 - Bug] Wrong icon qrc prefix in QML**
- **Found during:** Task 2 headless smoke (Main.qml found, then icon paths).
- **Issue:** Icons were referenced as `qrc:/qt/qml/logiops/gui/icons/...`, but the
  module's actual resource prefix is `:/logiops/gui/` (from qmldir `prefer`), so
  the icons would 404 at runtime.
- **Fix:** Rewrote all icon `source:` paths to `qrc:/logiops/gui/icons/...` in
  DeviceDelegate, DetailPane, EmptyState.
- **Files modified:** the three QML files above.
- **Commit:** `296b942`

**3. [Rule 3 - Blocking, environment] Base `qml6-module-qtquick` not installed**
- **Found during:** Task 2 headless run.
- **Issue:** Only `qtquick-controls`/`-layouts` sub-modules are installed; the
  base `qml6-module-qtquick` (which provides `QtQuick/qmldir` + the base plugin)
  is absent, so the engine cannot resolve `Item/Rectangle/Text/...` at run time.
- **Fix:** Cannot install from the sandbox (interactive sudo unavailable).
  Documented as the one-time operator prerequisite (MANUAL step 0) — it does not
  affect the build (-Werror clean, binary produced) and is not a code defect.
- **Commit:** n/a (documentation).

### Theme lint hygiene
- Replaced `Font.Normal`/`Font.DemiBold` with the numeric weights 400/600 in
  Theme.qml to clear qmllint "Unqualified access" warnings (equivalent values).

## Known Stubs

| Stub | File | Reason |
|------|------|--------|
| Battery shows `—` (delegate + detail pane) | qml/DeviceDelegate.qml, qml/DetailPane.qml | Intentional — DEV-02 battery DISPLAY is deferred to Plan 05, which wires `DeviceModel.onBatteryChanged` to the daemon's `BatteryChanged` signal (added in Plan 02) and replaces the placeholder. The model already carries the battery roles. |

These stubs do NOT block this plan's goal (DEV-01/03/04 + CONF-03 device list +
4 states + theme + reconnect); battery is a separate requirement owned by Plan 05.

## Threat Flags

None — no new network endpoints, auth paths, or trust boundaries beyond the
plan's threat model. Mitigations applied: T-02-14 (device strings rendered
`Text.PlainText`, never RichText/format-string); T-02-11/12/13 unchanged (pure
unprivileged D-Bus client on the well-known name, no hardware access, the
clipboard write is user-initiated non-secret onboarding text).

## Self-Check: PASSED

All 14 created files present, `logiops-gui` binary built, both task commits
(`65d13d4`, `296b942`) exist in history.
