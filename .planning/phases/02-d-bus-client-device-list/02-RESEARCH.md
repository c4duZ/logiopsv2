# Phase 2: D-Bus Client & Device List - Research

**Researched:** 2026-05-30
**Domain:** Qt 6 + QML app shell + typed QtDBus client over the `logid` daemon's `pizza.pixl.LogiOps` D-Bus surface (Ubuntu 24.04 / Qt 6.4.2)
**Confidence:** HIGH for the daemon D-Bus surface (read directly from source). MEDIUM for external Qt API specifics (web/Context7 unavailable this session — flagged where it matters).

> **Reading note for the planner:** every claim about the *daemon's D-Bus surface* below is `[VERIFIED: source]` — read from the actual `src/logid/**` and `src/ipcgull/**` files, not training data. Claims about *Qt/QtDBus/qdbusxml2cpp* are `[ASSUMED]` (training knowledge, web verification unavailable) and are collected in the Assumptions Log for confirmation against live Qt docs during planning.

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- New CMake target under `src/logiops-gui/` in this same repo (Qt 6 + QML / Qt Quick Controls 2, C++20).
- Single-window layout: device sidebar (list) + detail pane. Detail pane in this phase shows **read-only** device status; configuration UI is Fase 3.
- Generate a typed C++ D-Bus proxy from the daemon's introspection of `pizza.pixl.LogiOps` via `qdbusxml2cpp`; reuse `ipc_defs.h` constants for bus name / root path.
- Expose daemon state to QML through C++ model classes (a `QAbstractListModel` device-list model); **QML renders only — no business logic in QML/JS**.
- Device list item shows: name/model, battery (numeric % + icon + charging state), connection status.
- Device imagery: generic per-kind icon (mouse / keyboard) for v1; per-model renders deferred to Fase 3 (UI-01).
- Battery: numeric % + icon + charging indicator; show "—"/unknown when the device doesn't report `WirelessDeviceStatus`. **No predictive estimate.**
- Offline/sleeping devices stay visible but dimmed with a badge (do not drop from the list).
- **Daemon is the single source of truth (CONF-03). GUI never caches a static device list.** Live updates by subscribing to D-Bus signals; no polling.
- On daemon restart/crash: detect `NameOwnerChanged`, auto-reconnect and re-enumerate (no app restart).
- Access denied (not in `logiops` group): show onboarding explaining `sudo usermod -aG logiops $USER` + re-login — distinct from a generic error.
- States: no-devices empty state; loading spinner/skeleton; daemon-not-running vs no-access vs no-devices as **separate, clear** messages.
- Theme: follow system light/dark via Qt, Options+-flavored own identity.
- Use the **system bus** (daemon default). Do **not** switch to `USE_USER_BUS`.

### Claude's Discretion
- Exact QML component breakdown, model role names, signal-wiring details, and the `qdbusxml2cpp` invocation specifics — guided by the UI-SPEC and Qt/QtDBus conventions.

### Deferred Ideas (OUT OF SCOPE)
- Per-model device artwork / faithful Options+ device renders → Fase 3 (UI-01).
- Any device configuration (buttons, DPI, scroll, profiles, persistence) → Fase 3.
- Battery "predictive estimate" → out of scope (anti-feature); show raw % only.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| DEV-01 | See a list of all connected Logitech devices | `pizza.pixl.LogiOps.Devices.Enumerate` returns `ao` (array of device object paths); per-path `pizza.pixl.LogiOps.Device.Name` property. **Fully supported by the daemon today.** |
| DEV-02 | See live battery % + charging state per device | **NOT supported by the daemon today.** No battery feature wrapper exists; no battery property/signal on any interface. Requires a small daemon addition (see Runtime State Inventory + Risks). This is the phase's biggest carry-forward. |
| DEV-03 | See live connection status that updates on hotplug without restart | `Devices.DeviceAdded(o)` / `DeviceRemoved(o)` signals (hotplug add/remove) + per-device `Device.Active` property and `Device.StatusChanged(b active)` signal (sleep/wake) **exist today.** Supports online/sleeping; "offline last-known" needs a client decision (DeviceRemoved = gone). |
| DEV-04 | See each device's name/model | `Device.Name` (`s`) and `Device.ProductID` (`q`/uint16) properties **exist today.** |
| CONF-03 | GUI stays in sync, signal-driven, daemon = single source of truth | Enumerate-once + subscribe-to-signals architecture; no client-side cache. Add/remove + status signals exist; the **only** gap for full signal-driven sync is battery (DEV-02). |
</phase_requirements>

## Summary

The daemon already exposes a clean, typed D-Bus surface that covers **DEV-01, DEV-03 (connection/sleep), and DEV-04 fully**: a `pizza.pixl.LogiOps.Devices` interface at the root node (`/pizza/pixl/logiops`) with an `Enumerate() → ao` method and `DeviceAdded(o)` / `DeviceRemoved(o)` signals, plus a per-device `pizza.pixl.LogiOps.Device` interface (at `/pizza/pixl/logiops/devices/N`) exposing `Name` (`s`), `ProductID` (`q`), `Active` (`b`, = awake/not-sleeping), and a `StatusChanged(b)` signal. ipcgull marshals `std::shared_ptr<object>` as a **D-Bus object path** (`o`), so the add/remove signals and the Enumerate return value hand the GUI object paths it follows to read the per-device interface. This is exactly the shape `qdbusxml2cpp` consumes.

**The one load-bearing gap: there is no battery anywhere in the daemon today.** `feature_defs.h` declares `BATTERY_STATUS = 0x1000` and `BATTERY_VOLTAGE = 0x1001` as enum constants, but **no `BatteryStatus`/`UnifiedBattery` HID++ feature wrapper class exists**, `features/DeviceStatus` only handles wakeup/sleep (it does *not* read battery), and no D-Bus property or signal carries a battery percentage or charging flag. DEV-02 therefore **cannot** be satisfied by a pure D-Bus client — it requires a small daemon addition (a HID++ 2.0 battery feature wrapper + a `Battery`/`BatteryPercentage`+`Charging` property and a change signal on the `.Device` interface). The roadmap assumed Phases 2-3 were "zero daemon C++ changes"; that assumption is **false for DEV-02** and the planner must decide: (a) add the minimal battery feature to the daemon in this phase, or (b) ship the device list with battery showing "—" and defer DEV-02. The model and UI must support "—"/unknown regardless (the UI-SPEC already mandates it).

**Primary recommendation:** Build the GUI as specified (typed `qdbusxml2cpp` proxy + `QAbstractListModel` driven by Enumerate + DeviceAdded/Removed + StatusChanged), wire the model so it's ready to carry battery roles, and add a **minimal daemon battery feature** (HID++ 0x1000/0x1001/0x1004 wrapper → `Device.Battery` property + change signal) so DEV-02 is real signal-driven data, not a placeholder. Treat the daemon battery addition as the critical-path task of this phase.

## Project Constraints (from CLAUDE.md)

- **GSD workflow enforcement:** do not make direct repo edits outside a GSD workflow (Edit/Write must come through a GSD command). Planner should structure tasks accordingly.
- **C++20 throughout**, enforced via `CMAKE_CXX_STANDARD 20` / `..._REQUIRED ON`. The GUI target must set the same.
- **Formatting governed by `.editorconfig`** at repo root — match it, don't guess.
- **Naming:** Classes PascalCase, files PascalCase matching primary class, methods camelCase, private members `_leadingUnderscore`, header guards `#ifndef LOGID_..._H` (not `#pragma once`).
- **Smart pointers for ownership**; `make()` factories for post-construction setup; `std::shared_mutex` for reader/writer state.
- **`[[nodiscard]]` on value-returning queries, `final` on leaf classes.**
- **Exception-based error handling.** Rethrow with bare `throw;` (never `throw error;` — slicing bug, CONCERNS #1). Avoid passing untrusted runtime strings as `logPrintf` format args (CONCERNS #5).
- **D-Bus-exposed daemon objects use nested `ipcgull` interface classes** (the established pattern — relevant if the battery addition touches the daemon).
- **CI builds with `-Werror`** (`-Wall -Wextra` always). The GUI target must compile clean under these.
- **`USE_USER_BUS` default OFF** → system bus. GUI must use `QDBusConnection::systemBus()`.

---

## The Exact D-Bus Surface (VERIFIED from source)

> All of the following is `[VERIFIED: source]` — read from `src/logid/ipc_defs.h`, `DeviceManager.cpp`, `Device.cpp`, `Receiver.cpp`, and `src/ipcgull/src/**`.

### Identity & object-path layout

| Thing | Value | Source |
|-------|-------|--------|
| Bus name | `pizza.pixl.LogiOps` (`SERVICE_ROOT_NAME`) | `ipc_defs.h:22` |
| Root object path | `/pizza/pixl/logiops` (`server_root_node`) | `ipc_defs.h:23`, `logid.cpp:161` |
| Bus | **system bus** by default (`-DUSE_USER_BUS=ON` → session) | `logid.cpp:154`, CMake `option(USE_USER_BUS OFF)` |
| Devices container node | `/pizza/pixl/logiops/devices` | `DeviceManager.cpp:38` (`make_root("devices")`) + `full_name`/`tree_name` in `server_gdbus.cpp:1105-1118` |
| Receivers container node | `/pizza/pixl/logiops/receivers` | `DeviceManager.cpp:39` |
| Per-device node | `/pizza/pixl/logiops/devices/N` where `N` is the nickname int (0,1,2,…) | `Device.cpp:106` (`devicesNode()->make_child(_nickname)`); nickname is `std::to_string(int)` from `DeviceManager::newDeviceNickname()` |
| Per-receiver node | `/pizza/pixl/logiops/receivers/N` | `Receiver.cpp:60` |

**Path-derivation confirmed:** `node::full_name = server.root_node() + "/" + tree_name()`; `tree_name` walks parent names joined by `/`. Root node is `make_root("")` (empty name → `full_name` returns just the root path). So the absolute paths above are exact.

### Interfaces, properties, methods, signals

**`pizza.pixl.LogiOps.Devices`** — on the **root** node `/pizza/pixl/logiops` (`DeviceManager.cpp:178-193`):

| Member | Kind | D-Bus signature | Notes |
|--------|------|-----------------|-------|
| `Enumerate` | method | `() → ao` (out arg name `devices`) | Returns object paths of **all** devices, including those behind receivers (it flattens `_devices` + each receiver's `devices()`). `[VERIFIED: DeviceManager.cpp:182, listDevices() 195-206]` |
| `DeviceAdded` | signal | `(o device)` | Emitted on hotplug add and on receiver-paired device appear (`deviceAdded`). `[VERIFIED: DeviceManager.cpp:186-188, 216-219]` |
| `DeviceRemoved` | signal | `(o device)` | Emitted on disconnect/unpair (`removeDevice` → `deviceRemoved`). `[VERIFIED: DeviceManager.cpp:189-191, 221-224, 160-176]` |

**`pizza.pixl.LogiOps.Receivers`** — on the root node (`DeviceManager.cpp:226-242`): `Enumerate() → ao`, `ReceiverAdded(o)`, `ReceiverRemoved(o)`. **Phase 2 can ignore receivers** — `Devices.Enumerate` already includes receiver-attached devices, and the UI lists devices not receivers. Receiver interface matters in Fase 3 (pairing) only.

**`pizza.pixl.LogiOps.Device`** — on each `/pizza/pixl/logiops/devices/N` (`Device.cpp:315-335`):

| Member | Kind | D-Bus signature | Notes |
|--------|------|-----------------|-------|
| `Name` | property (RO) | `s` | Device name/model string from HID++ `DeviceName`. **DEV-04.** `[VERIFIED: Device.cpp:325-326]` |
| `ProductID` | property (RO) | `q` (uint16) | PID. Use to pick mouse vs keyboard icon (see deviceKind note). `[VERIFIED: Device.cpp:327-328]` |
| `Active` | property (RO) | `b` | `true` = awake, `false` = sleeping. Backed by `_awake`. **This is the sleep/wake signal of DEV-03.** `[VERIFIED: Device.cpp:329, Device.h:179, Device.cpp:177-197]` |
| `DefaultProfile` | property | `s` | Fase 3 (config). Ignore in Phase 2. |
| `ActiveProfile` | property (RO) | `s` | Fase 3. Ignore in Phase 2. |
| `GetProfiles`/`SetProfile`/`RemoveProfile`/`ClearProfile` | methods | profile mgmt | **Fase 3.** Ignore in Phase 2. |
| `StatusChanged` | signal | `(b active)` | Fired on sleep() and wakeup(). Drives live online/sleeping. `[VERIFIED: Device.cpp:333, 337-339]` |

**There is NO battery member on any interface.** `[VERIFIED: grep over src/ — no battery property/signal; feature_defs.h:48-49 has BATTERY_STATUS/BATTERY_VOLTAGE enum constants only; no feature class file exists]`

### ipcgull → D-Bus type marshalling (so the Qt proxy knows what it sees)

`[VERIFIED: src/ipcgull/src/include/ipcgull/variant.h + common_gdbus.cpp:76-77 + server_gdbus.cpp:205-210]`

| ipcgull C++ type | D-Bus type | QtDBus C++ type |
|------------------|-----------|-----------------|
| `std::string` | `s` | `QString` |
| `uint16_t` | `q` | `quint16` |
| `bool` | `b` | `bool` |
| `std::shared_ptr<object>` (e.g. `shared_ptr<Device>`) | **`o` (object path)** | `QDBusObjectPath` |
| `std::vector<shared_ptr<Device>>` (Enumerate return) | **`ao`** | `QList<QDBusObjectPath>` |
| `int32/uint32/int64/uint64/double/uint8` | `i/u/x/t/d/y` | matching Qt ints/double |
| `std::map<K,V>` | `a{..}` | `QMap` / needs custom type if used |

**Key consequence:** the device add/remove signals and `Enumerate` give the GUI **object paths**, not inline structs. The GUI's flow is: `Enumerate()` → list of paths → for each path, construct a `Device` proxy and read `Name`/`ProductID`/`Active`; subscribe to that path's `StatusChanged`. On `DeviceAdded(path)` → add a row; on `DeviceRemoved(path)` → remove a row. This is the standard "manager emits object paths, client follows them" D-Bus pattern and maps cleanly onto `qdbusxml2cpp`-generated proxies.

### Standard D-Bus property-change signal

ipcgull properties call `notify_change()` on assignment (`property.h:140, 247-252`), which (via the GDBus backend) emits the standard `org.freedesktop.DBus.Properties.PropertiesChanged`. So `Active` changes arrive **both** as the explicit `Device.StatusChanged(b)` signal *and* as a standard `PropertiesChanged` on the `.Device` interface. The GUI can use either; `StatusChanged` is the explicit, typed one. `[VERIFIED: property.h notify_change wiring; PropertiesChanged is the GDBus standard — ASSUMED the backend emits it on notify_change, confirm with busctl]`.

---

## Standard Stack

### Core (verified on target per STACK.md: Ubuntu 24.04 / Qt 6.4.2)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6 Quick / QML | 6.4.2 | Declarative custom-skinned UI | Same C++20 language as daemon; GPU scene graph; custom theming for Options+ identity `[VERIFIED: STACK.md target probe]` |
| QtDBus (`libqt6dbus6`, dev `qt6-base-dev`) | 6.4.2 | D-Bus client | First-class; `qdbusxml2cpp` generates typed proxy; `QDBusConnection::systemBus()` `[VERIFIED: STACK.md]` / `[ASSUMED]` for exact API |
| Qt Quick Controls 2 | 6.4.2 | List/controls primitives | Custom style (Basic base), not Material/Universal `[VERIFIED: STACK.md]` |
| QtSvg (`libqt6svg6-dev`) | 6.4.2 | Crisp mouse/keyboard/battery glyphs | UI-SPEC icon contract `[VERIFIED: STACK.md]` |
| C++20 / CMake ≥ 3.16 | repo: CMake 3.12 min, target 3.28 | Build | Matches daemon; GUI is another target `[VERIFIED: CMakeLists.txt]` |

### Supporting

| Library | Purpose | When to Use |
|---------|---------|-------------|
| `qt6-base-dev-tools` / `qt6-declarative-dev-tools` | ships `qdbusxml2cpp`, `qmllint` | Build-time proxy generation + QML lint |
| `qml6-module-qtquick-layouts` | `RowLayout`/`SplitView` layout | App shell |
| `busctl` (systemd) | Capture introspection XML at dev time | `busctl --system introspect pizza.pixl.LogiOps /pizza/pixl/logiops/devices/0 --xml-interface` |

**Installation (target deps already verified present per STACK.md):**
```bash
sudo apt install qt6-base-dev qt6-base-dev-tools qt6-declarative-dev \
  qt6-declarative-dev-tools qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts libqt6svg6-dev
```

**Version verification flag:** STACK.md verified these via `apt-cache policy` on the actual target (HIGH for that machine). The exact `qdbusxml2cpp` CLI flags and `qt_add_dbus_interface` CMake signature for 6.4.2 are `[ASSUMED]` from training — confirm against the Qt 6.4 docs during planning (collected in Assumptions Log).

---

## Architecture Patterns

### Recommended target structure
```
src/logiops-gui/
├── CMakeLists.txt          # optional Qt target (see CMake section)
├── main.cpp                # QGuiApplication + QQmlApplicationEngine, register C++ types
├── dbus/
│   ├── logid_devices.xml   # captured introspection for .Devices (root)
│   ├── logid_device.xml    # captured introspection for .Device (per-device)
│   └── (generated proxies via qdbusxml2cpp / qt_add_dbus_interface)
├── DeviceModel.{h,cpp}     # QAbstractListModel, owns the device proxies + roles
├── DaemonConnection.{h,cpp}# systemBus, QDBusServiceWatcher, screen-state enum
├── icons/*.svg             # mouse/keyboard/battery/status glyphs (qrc)
└── qml/
    ├── Main.qml            # ApplicationWindow + SplitView shell
    ├── Theme.qml           # pragma Singleton tokens (light/dark pair)
    ├── DeviceList.qml      # ListView bound to DeviceModel
    ├── DeviceDelegate.qml  # 56px row (UI-SPEC anatomy)
    ├── DetailPane.qml      # read-only status
    └── states/ (Empty/Loading/DaemonDown/AccessDenied).qml
```

### Pattern 1: Manager-emits-paths, client-follows (the daemon's actual model)
**What:** The `.Devices` interface is a *manager*; it returns/emits object paths. The client owns a per-path `.Device` proxy.
**When to use:** Always here — it's the daemon's actual surface.
**Flow:**
```
DaemonConnection::enumerate():
  1. systemBus().interface valid? else -> screen-state (down / access-denied)
  2. Devices.Enumerate() -> QList<QDBusObjectPath>
  3. for each path: DeviceModel::addDevice(path)
       - construct OrgPizzaPixlLogiOpsDeviceInterface(service, path, systemBus)
       - read Name, ProductID, Active (synchronously or async)
       - connect that proxy's StatusChanged(bool) -> model updates that row
  4. connect Devices.DeviceAdded(o)   -> DeviceModel::addDevice(path)
     connect Devices.DeviceRemoved(o) -> DeviceModel::removeDevice(path)
```

### Pattern 2: QAbstractListModel with per-role roleNames (the C++↔QML bridge)
**What:** One model row per device; QML delegate binds each role independently so a single field can repaint without touching the row.
**Roles (Claude's discretion — recommended):**
```cpp
enum Roles {
  PathRole = Qt::UserRole + 1, // QDBusObjectPath as QString (stable sort key)
  NameRole,                    // QString  (DEV-04)
  ModelRole,                   // QString  (sub-line; from Name/ProductID lookup)
  DeviceKindRole,              // int enum {Mouse, Keyboard, Unknown} -> icon
  BatteryPercentRole,          // int (0..100) (DEV-02)
  BatteryKnownRole,            // bool (false -> show "—")
  ChargingRole,                // bool (DEV-02)
  ConnectionStateRole,         // int enum {Online, Sleeping, Offline, Unknown} (DEV-03)
};
```
`Active==false` → `Sleeping`; `Active==true` → `Online`; `DeviceRemoved` → row removed (the daemon's only "gone" signal). True "Offline last-known dimmed-in-place" (UI-SPEC) has no daemon signal distinct from removal — see Open Questions.

### Pattern 3: Signal → dataChanged (no-flicker contract, UI-SPEC)
**What:** On a `StatusChanged`/battery signal, emit `dataChanged(index, index, {role})` for **only the changed role on the changed row** — never `beginResetModel`.
```cpp
void DeviceModel::onStatusChanged(const QString& path, bool active) {
    int row = rowForPath(path);              // stable lookup
    if (row < 0) return;
    _rows[row].connectionState = active ? Online : Sleeping;
    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ConnectionStateRole});
}
```
**Add/remove** use `beginInsertRows`/`endInsertRows` and `beginRemoveRows`/`endRemoveRows`. **Sort order is stable by `path` (or nickname int)** — battery/status changes must never re-sort (UI-SPEC forbids it; classic flicker bug).

### Anti-Patterns to Avoid
- **Caching a device list independent of the daemon.** Forbidden by CONF-03. The model rows *are* the cache, but they are only ever mutated by enumerate + signals; never persisted, never reconstructed from a snapshot.
- **Polling properties on a timer.** Forbidden — use signals (`StatusChanged`, `PropertiesChanged`, and the future battery change signal).
- **`beginResetModel` on a value change.** Flickers the whole list. Use targeted `dataChanged`.
- **Re-sorting on battery/status tick.** Rows jump. Sort once on insert by stable key.
- **Using ipcgull from the GUI.** It is the daemon's *server-side* helper. The GUI is a plain QtDBus client (STACK.md "What NOT to Use").
- **Blocking the UI thread on synchronous D-Bus calls.** Prefer `QDBusPendingCall`/async for `Enumerate` and per-device property reads; a slow/asleep device must not freeze the window.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| D-Bus proxy / marshalling | Manual `QDBusMessage` calls | `qdbusxml2cpp`-generated typed proxy (or `qt_add_dbus_interface`) | Typed signatures, signal wiring, less error surface |
| Daemon up/down detection | Custom ping loop | `QDBusServiceWatcher` on `NameOwnerChanged` | The canonical, race-free mechanism |
| List add/remove animation + repaint scoping | Custom diffing | `QAbstractListModel` begin/end{Insert,Remove}Rows + `dataChanged(roles)` | Qt's model/view does minimal repaint and drives ListView transitions |
| HID++ battery reads | New hidraw access from the GUI | A daemon-side HID++ battery feature wrapper exposed over D-Bus | GUI must never touch hardware (CLAUDE.md / CONTEXT.md: all hardware via daemon) |
| Light/dark theme | Hard-coded palette | System palette + `paletteChanged` (Qt 6.4.2 has no `colorScheme` enum) | Native feel; see Risks |

**Key insight:** The only thing that genuinely must be *built new* in this phase is (1) the GUI itself and (2) — if DEV-02 is in scope — a **minimal daemon battery feature**, because that data does not exist yet. Everything else is wiring existing daemon signals into existing Qt model/view machinery.

## Runtime State Inventory

> This phase is **mostly greenfield** (a new GUI target) but it depends on the daemon's existing runtime D-Bus surface, and DEV-02 forces a daemon extension. Inventory of what already exists vs. what must be added:

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Existing daemon D-Bus signals (add/remove) | `Devices.DeviceAdded(o)` / `DeviceRemoved(o)` exist and fire on hotplug + receiver pair/unpair `[VERIFIED]` | **None** — GUI subscribes. Satisfies DEV-03 hotplug + CONF-03 add/remove. |
| Existing daemon per-device status signal | `Device.StatusChanged(b active)` + `Device.Active` property exist and fire on sleep/wake `[VERIFIED]` | **None** — GUI subscribes. Satisfies DEV-03 online/sleeping. |
| Existing name/model surface | `Device.Name` (`s`), `Device.ProductID` (`q`) exist `[VERIFIED]` | **None** — GUI reads. Satisfies DEV-04. |
| **Battery (DEV-02)** | **MISSING.** No battery feature wrapper class; `feature_defs.h` has `BATTERY_STATUS=0x1000`/`BATTERY_VOLTAGE=0x1001` enum constants only; `DeviceStatus` reads wakeup/sleep, not battery; no D-Bus battery property/signal `[VERIFIED]` | **Daemon addition (code, not data migration):** add a HID++ 2.0 battery feature wrapper (0x1000 BatteryStatus and/or 0x1001 BatteryVoltage; modern devices use 0x1004 UnifiedBattery — **0x1004 is not even in the enum**, may need adding) + an `ipcgull::property` for percentage + charging + a change signal on the `.Device` interface, fired from the battery broadcast event handler. **This is the carry-forward.** |
| GUI access to system bus | Phase 1 relaxed D-Bus policy to `group=logiops` for send/receive `[VERIFIED: STATE.md]` | **None** — non-root GUI can reach the bus; access-denied onboarding ties to membership. |
| Build artifacts | No existing `src/logiops-gui/` `[VERIFIED: ls]` | New CMake target; keep optional so daemon still builds without Qt. |
| Secrets/env vars | None — daemon uses no env/secrets `[VERIFIED: INTEGRATIONS.md]` | None. |

**The canonical question — "after every repo file is updated, what runtime state still has old data?":** N/A for a new GUI, **but inverted here:** the runtime *daemon* is missing a capability (battery) the GUI requires. That is a daemon code addition, sequenced as a Phase 2 task.

## Common Pitfalls

### Pitfall 1: Assuming "Phases 2-3 are zero daemon C++ changes" (the roadmap's stated assumption)
**What goes wrong:** Planner scopes Phase 2 as pure client work; DEV-02 (battery) silently becomes impossible or degrades to a permanent "—".
**Why it happens:** STATE.md records "[Roadmap]: Phases 2-3 are pure D-Bus-client work (zero daemon C++ changes)". The daemon has battery *enum constants* but no implementation, which is easy to mistake for support.
**How to avoid:** Decide explicitly: add minimal daemon battery feature this phase, or descope DEV-02. Either way the model carries battery roles and renders "—" when unknown.
**Warning signs:** `busctl introspect .../devices/0` shows **no** `Battery*` member on `.Device`.

### Pitfall 2: object-path proxies vs inline data
**What goes wrong:** Developer expects `Enumerate`/`DeviceAdded` to return device *structs* (name, battery, …); they return only `o` paths.
**Why it happens:** ipcgull marshals `shared_ptr<Device>` as an object path, not a struct `[VERIFIED]`.
**How to avoid:** Each path → a per-device proxy; read properties from `.Device`. Plan an async property-read step per added device.
**Warning signs:** Empty/garbage names because the code read the path string as a name.

### Pitfall 3: Offline-vs-removed conflation
**What goes wrong:** UI-SPEC wants offline devices to **stay dimmed in place**, but the daemon's only "device gone" signal is `DeviceRemoved` (which means *truly gone*), and `Active=false` means *sleeping*, not offline.
**Why it happens:** There's no daemon "went offline but still paired" state distinct from removed.
**How to avoid:** Map `DeviceRemoved` → remove row (UI-SPEC: "only genuine unpair/unplug removes the row"); map `Active=false` → sleeping (dimmed). A separate "offline last-known" state has no daemon source today — treat sleeping as the dim-in-place case and document the offline nuance as deferred. (Open Question.)
**Warning signs:** Rows vanishing on sleep, or rows lingering forever after a real unplug.

### Pitfall 4: Synchronous D-Bus on the UI thread
**What goes wrong:** A sleeping/slow device makes a blocking property read hang the window.
**How to avoid:** Use async (`QDBusPendingCallWatcher`) for Enumerate and per-device reads; populate rows as replies arrive (also gives the UI-SPEC fade-in for free).

### Pitfall 5: Qt 6.4.2 has no `Application.styleHints.colorScheme`
**What goes wrong:** Copy a Qt 6.5+ snippet; it won't compile/resolve on 6.4.2.
**How to avoid:** Detect dark mode from the system palette luminance (`palette.window().color().lightnessF() < 0.5`) and subscribe to `paletteChanged`. (UI-SPEC already specifies this.) `[ASSUMED — confirm 6.4 API]`

## Code Examples

> These are `[ASSUMED]` Qt API shapes from training knowledge (web/Context7 unavailable). They are best-known-correct patterns; confirm exact signatures against Qt 6.4 docs during planning. They are illustrative for the planner, not copy-paste-final.

### Capture introspection XML for the proxy (dev-time)
```bash
# Daemon must be running. Root interface (.Devices/.Receivers):
busctl --system introspect pizza.pixl.LogiOps /pizza/pixl/logiops --xml-interface > logid_root.xml
# A live device interface (.Device):
busctl --system introspect pizza.pixl.LogiOps /pizza/pixl/logiops/devices/0 --xml-interface > logid_device.xml
# Trim each XML to the single interface qdbusxml2cpp should generate.
```

### CMake proxy generation (two options) `[ASSUMED API]`
```cmake
# Option A: qt_add_dbus_interface (preferred, Qt6)
qt_add_dbus_interface(GEN_SRCS dbus/logid_devices.xml logiddevices_proxy)
qt_add_dbus_interface(GEN_SRCS dbus/logid_device.xml  logiddevice_proxy)
# Then add ${GEN_SRCS} to the target sources.

# Option B: explicit qdbusxml2cpp at build (equivalent)
# qdbusxml2cpp -p logiddevice_proxy -c LogidDeviceInterface dbus/logid_device.xml
```

### Connect to the system bus + watch the daemon `[ASSUMED API]`
```cpp
auto bus = QDBusConnection::systemBus();
auto* watcher = new QDBusServiceWatcher(
    QStringLiteral("pizza.pixl.LogiOps"), bus,
    QDBusServiceWatcher::WatchForOwnerChange, this);
connect(watcher, &QDBusServiceWatcher::serviceRegistered,   this, &DaemonConnection::onDaemonUp);
connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &DaemonConnection::onDaemonDown);
// Distinguish access-denied from down on the initial Enumerate reply:
QDBusPendingReply<QList<QDBusObjectPath>> reply = devicesIface->Enumerate();
// on error: reply.error().name() == "org.freedesktop.DBus.Error.AccessDenied"
//           or QDBusError::AccessDenied  -> access-denied onboarding
//           name owner empty / Disconnected / ServiceUnknown -> daemon-down
```

### Per-device proxy + StatusChanged → model `[ASSUMED API]`
```cpp
// path came from Enumerate / DeviceAdded
auto* dev = new LogidDeviceInterface("pizza.pixl.LogiOps", path.path(), bus, this);
connect(dev, &LogidDeviceInterface::StatusChanged, this,
        [this, p = path.path()](bool active){ model->onStatusChanged(p, active); });
// initial read (async preferred):
QString name = dev->name();          // generated property accessor
quint16 pid  = dev->productID();
bool active  = dev->active();
```

## State of the Art

| Old Approach | Current Approach | Why |
|--------------|------------------|-----|
| `qmake` + hand-written D-Bus calls | CMake `qt_standard_project_setup()` + `qt_add_qml_module()` + `qt_add_dbus_interface()` | Qt6 idiom; QML module registration, resource embedding, typed proxies `[ASSUMED — confirm Qt 6.4]` |
| Polling battery/status on a timer | Signal-driven `dataChanged` + `PropertiesChanged` | CONF-03 no-stale, UI-SPEC no-flicker |
| `colorScheme` enum (Qt 6.5+) | palette-luminance + `paletteChanged` | Target is Qt 6.4.2 |

**Deprecated/outdated for this target:**
- `Application.styleHints.colorScheme` — Qt ≥ 6.5 only; **not available on 6.4.2**.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `qt_add_dbus_interface` / `qdbusxml2cpp` flags as shown work on Qt 6.4.2 | CMake, Code Examples | Proxy gen step needs adjusting; low risk (well-established tool) |
| A2 | `QDBusServiceWatcher` + `WatchForOwnerChange` is the right reconnect API | Reconnect, Code Examples | Reconnect wiring differs; low risk (canonical) |
| A3 | `QDBusError::AccessDenied` / `org.freedesktop.DBus.Error.AccessDenied` is what the relaxed policy returns to a non-group user | Reconnect, states | Access-denied detection may need a different error name; MEDIUM — verify by testing as a non-`logiops` user |
| A4 | ipcgull `notify_change()` emits standard `PropertiesChanged` over GDBus | D-Bus surface | If not, rely solely on the explicit `StatusChanged` signal (still fine); LOW |
| A5 | Qt 6.4.2 palette-luminance dark detection + `paletteChanged` works under GNOME/KDE | Theme, Pitfall 5 | Theme may not auto-follow; MEDIUM — verify on target desktop |
| A6 | object path nickname is a plain integer string (`0`,`1`,…) and stable per device lifetime | Path layout, sort | If paths churn, sort key/dedup logic changes; LOW (verified the nickname is an int; reuse-after-removal is possible) |
| A7 | Adding a battery feature uses HID++ 0x1004 UnifiedBattery for modern devices (not in the enum) | DEV-02 daemon addition | Battery wrapper may target the wrong feature ID for newer mice; MEDIUM — the daemon battery task needs its own HID++ verification |

**These are why the phase needs a running-daemon smoke pass and a real-hardware check before DEV-02/DEV-03 are signed off.**

## Open Questions

1. **Is DEV-02 (battery) in scope for Phase 2, requiring a daemon addition?**
   - What we know: no battery exists in the daemon today `[VERIFIED]`; the model/UI already support "—"/unknown.
   - What's unclear: whether to add the minimal daemon battery feature now or defer DEV-02.
   - Recommendation: **Add the minimal daemon battery feature this phase** (new HID++ feature wrapper + `Device.Battery`/`Charging` property + change signal) so DEV-02 is real. If deferred, mark DEV-02 not-met and ship "—". Either way, build the model to carry battery roles.

2. **"Offline (dimmed in place)" vs `DeviceRemoved`.**
   - What we know: daemon emits `DeviceRemoved` (truly gone) and `Active=false` (sleeping). No "offline-but-paired-and-keep-the-row" state.
   - Recommendation: map removed→remove row, sleeping→dim+badge. Treat the distinct "offline last-known dimmed" UI-SPEC state as best-effort/deferred unless a daemon signal is added.

3. **Receiver-attached vs direct device kind/icon.**
   - What we know: `Enumerate` flattens both; `ProductID` is available; no explicit mouse/keyboard "kind" property.
   - Recommendation: derive `deviceKind` from `ProductID` (a small PID→kind table) or from name heuristics for v1; a proper `DeviceKind` daemon property is a possible later add.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Qt 6 base + DBus + tools | GUI build + proxy gen | ✓ (per STACK.md target probe) | 6.4.2 | Qt 6.8/6.9 via aqtinstall if needed |
| Qt Quick Controls 2 / Layouts | App shell | ✓ | 6.4.2 | — |
| QtSvg | icons | ✓ | 6.4.2 | PNG assets |
| `qdbusxml2cpp` / `busctl` | proxy gen / introspection | ✓ (qt6 tools / systemd) | — | hand-write proxy (avoid) |
| Running `logid` daemon (system bus) | smoke tests, introspection XML capture | depends on dev box | — | unit-test the model with a fake proxy / fixture |
| A real Logitech device + receiver | DEV-02/03 hardware verification | hardware-dependent | — | mark hardware checks human-verify; UI states unit-testable without it |

**Missing dependencies with no fallback:** None for build. **DEV-02 real battery data has no software fallback** — it needs the daemon addition + a battery-reporting device.

**Missing dependencies with fallback:** Without a running daemon/hardware, the model and screen-state logic are still unit-testable against a fake/stub proxy; live behavior needs a smoke run.

## Validation Architecture

> nyquist_validation is enabled (`config.json: workflow.nyquist_validation: true`). This section is required.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | **CTest** (already wired at repo root: `enable_testing()` + `add_subdirectory(test)`). C++ unit via `add_executable` + `add_test`, matching the existing Phase 1 `hidpp_bounds_test` pattern. For the model, a small QtTest (`Qt6::Test`) executable, or a plain assertion `main` if avoiding a QtTest dep. |
| Config file | `test/CMakeLists.txt` (extend it) + the GUI target's CMake |
| Quick run command | `ctest --test-dir build -R devicemodel --output-on-failure` |
| Full suite command | `ctest --test-dir build --output-on-failure` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DEV-01 | Enumerate populates one model row per device | unit (model + fake proxy) | `ctest -R devicemodel_enumerate` | ❌ Wave 0 |
| DEV-03 | `DeviceAdded`/`DeviceRemoved` insert/remove exactly one row (no full reset) | unit (model) | `ctest -R devicemodel_addremove` | ❌ Wave 0 |
| DEV-03 | `StatusChanged(active)` emits `dataChanged` for only ConnectionStateRole on one row (no reset, no reorder) | unit (model) | `ctest -R devicemodel_status_noflicker` | ❌ Wave 0 |
| DEV-04 | `Name`/`ProductID` populate Name/DeviceKind roles | unit (model) | `ctest -R devicemodel_name` | ❌ Wave 0 |
| DEV-02 | Battery roles render "—" when unknown; numeric+charging when known | unit (model) | `ctest -R devicemodel_battery_unknown` | ❌ Wave 0 |
| CONF-03 | Model holds no rows except those from enumerate+signals (no cache); clear+re-enumerate on reconnect | unit (model) | `ctest -R devicemodel_reconnect` | ❌ Wave 0 |
| DEV-01/02/03 | End-to-end against the live daemon: list shows, status/battery update live, hotplug without restart | smoke (running daemon + hardware) | `test/smoke/device-list.sh` (graceful-skip if no daemon/device, mirroring Phase 1 smoke pattern) | ❌ Wave 0 |
| Access path | Non-`logiops` user → access-denied state, not generic error | smoke (manual/scripted as wrong user) | `test/smoke/access-denied-gui.sh` | ❌ Wave 0 |

**Unit-testable (the bulk):** `DeviceModel` logic — feed it a fake/stub D-Bus proxy (or call its slots directly with synthetic path/name/active/battery values) and assert `rowCount`, `data(role)`, and the exact `dataChanged`/`rowsInserted`/`rowsRemoved` signals via `QSignalSpy`. This verifies DEV-01/03/04, the battery-unknown path, the no-flicker contract, and CONF-03 reconnect **without** a daemon or hardware.

**Needs running daemon / hardware (smoke):** actual live battery/status updates and hotplug-without-restart (DEV-02/DEV-03 end-to-end), and the access-denied vs daemon-down distinction (A3) — graceful-skip when the daemon/device is absent, same convention as Phase 1's smoke scripts.

### Sampling Rate
- **Per task commit:** `ctest --test-dir build -R devicemodel --output-on-failure`
- **Per wave merge:** `ctest --test-dir build --output-on-failure` (full unit suite) + GUI builds clean under `-Werror`
- **Phase gate:** full unit suite green; smoke run executed (or documented-skipped) against a live daemon before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `test/DeviceModelTest.cpp` (+ a fake/stub proxy or direct-slot driver) — covers DEV-01/02/03/04, CONF-03
- [ ] `test/CMakeLists.txt` extension — register the model test target (link `Qt6::Core`/`Qt6::Test`, or header-include the model)
- [ ] `src/logiops-gui/CMakeLists.txt` — optional Qt target (so daemon still builds without Qt)
- [ ] `test/smoke/device-list.sh` + `test/smoke/access-denied-gui.sh` — graceful-skip smoke scripts (mirror Phase 1 `test/smoke/*.sh`)
- [ ] Qt6 find_package availability guard so unit/daemon build still works where Qt is absent

## Security Domain

> `security_enforcement` not present in config.json. Phase is a local D-Bus client + a possible small daemon battery-read addition. Minimal but non-zero surface.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | D-Bus access is gated by the system-bus policy (group `logiops`) from Phase 1, not by app-level auth |
| V4 Access Control | yes | The GUI is an unprivileged client; access is enforced by the D-Bus policy. GUI must never bypass it (no direct hidraw). |
| V5 Input Validation | yes | If the daemon battery feature is added: length-check HID++ report fields before indexing (CONCERNS #4 / ACCESS-04 pattern — `hasHidppHeader`/bounds) — untrusted hardware input. |
| V6 Cryptography | no | None. |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Untrusted HID battery report over-read (if daemon battery feature added) | Tampering / DoS | Bounds-check report params before indexing (reuse ACCESS-04 `hasHidppHeader` discipline); the daemon already runs hardened (Phase 1) |
| GUI trusting arbitrary object paths from a spoofed bus name | Spoofing | System-bus policy restricts who can own `pizza.pixl.LogiOps` to root; GUI talks only to that well-known name |
| `logPrintf` format-string with device-supplied name | Tampering | Never pass `Name`/device strings as the format arg (CONCERNS #5) — applies if daemon code is touched |

## Sources

### Primary (HIGH confidence — read this session)
- `src/logid/ipc_defs.h` — bus name, root node
- `src/logid/DeviceManager.{h,cpp}` — `.Devices`/`.Receivers` interfaces, Enumerate, DeviceAdded/Removed, node paths
- `src/logid/Device.{h,cpp}` — `.Device` interface: Name/ProductID/Active properties, StatusChanged signal, per-device node `/devices/N`
- `src/logid/Receiver.{h,cpp}` — receiver node layout
- `src/logid/features/DeviceStatus.{h,cpp}` + `backend/hidpp20/features/WirelessDeviceStatus.{h,cpp}` — confirms wakeup/sleep only, **no battery**
- `src/logid/backend/hidpp20/feature_defs.h` — BATTERY_STATUS/BATTERY_VOLTAGE enum constants only (no impl)
- `src/ipcgull/src/include/ipcgull/{node,interface,property,signal,variant}.h` + `src/common_gdbus.cpp` + `src/server_gdbus.cpp` — type marshalling (`shared_ptr<object>` → object path `o`/`ao`), property notify_change, full_name/tree_name path derivation
- `CMakeLists.txt`, `src/logid/CMakeLists.txt`, `test/CMakeLists.txt` — build structure, CTest harness pattern
- `CLAUDE.md`, `.planning/codebase/CONVENTIONS.md` — project constraints
- `.planning/research/STACK.md` — Qt 6.4.2 target verification (apt probe)

### Secondary (MEDIUM)
- `.planning/codebase/ARCHITECTURE.md` / `INTEGRATIONS.md` — daemon layering, bus selection
- STATE.md — roadmap "zero daemon C++ changes" assumption (now contradicted for DEV-02)

### Tertiary (LOW / ASSUMED — web/Context7 unavailable this session)
- Qt 6.4 QtDBus / `qdbusxml2cpp` / `qt_add_dbus_interface` / `QDBusServiceWatcher` / palette dark-mode APIs — training knowledge; collected in Assumptions Log; confirm against live Qt 6.4 docs during planning.

## Metadata

**Confidence breakdown:**
- Daemon D-Bus surface (paths/interfaces/properties/signals/marshalling): **HIGH** — read directly from source.
- "Battery is missing" (DEV-02 gap): **HIGH** — verified across feature_defs, DeviceStatus, and a repo-wide grep.
- Qt/QtDBus client API specifics: **MEDIUM** — established patterns, but exact 6.4.2 signatures unverified (web unavailable).
- Theme detection / access-denied error name: **MEDIUM** — needs on-target verification.

**Research date:** 2026-05-30
**Valid until:** ~2026-06-29 (stable; daemon surface won't drift, Qt API specifics worth re-confirming when web tools return)

## RESEARCH COMPLETE

**Phase:** 2 - D-Bus Client & Device List
**File:** `.planning/phases/02-d-bus-client-device-list/02-RESEARCH.md` (~ 360 lines)
**Confidence:** HIGH on the daemon surface; MEDIUM on Qt client API specifics.

**Single biggest risk:** **DEV-02 battery is not supported by the daemon at all today** — no battery HID++ feature wrapper, no battery D-Bus property/signal (only `BATTERY_STATUS`/`BATTERY_VOLTAGE` enum constants exist). The roadmap's "Phases 2-3 = zero daemon C++ changes" assumption is false for battery. The planner must either add a minimal daemon battery feature (new HID++ wrapper + `Device.Battery`/`Charging` property + change signal) as a Phase 2 critical-path task, or descope DEV-02 to a permanent "—". Everything else (DEV-01/03/04, CONF-03) is fully serviceable by the daemon's existing `Devices.Enumerate`/`DeviceAdded`/`DeviceRemoved` + `Device.Name`/`ProductID`/`Active`/`StatusChanged` surface, marshalled as object paths the typed `qdbusxml2cpp` proxy consumes.
