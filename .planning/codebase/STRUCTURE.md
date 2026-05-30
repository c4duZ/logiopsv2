# Codebase Structure

**Analysis Date:** 2026-05-30

## Directory Layout

```
logiops/
├── CMakeLists.txt                # Top-level build: version, options, submodules
├── src/
│   ├── ipcgull/                  # Git submodule: D-Bus IPC library (own CMakeLists)
│   │   └── src/include/ipcgull/  # Public headers (node, interface, server, property, ...)
│   └── logid/                    # The daemon — all first-party code lives here
│       ├── CMakeLists.txt        # Builds the `logid` executable, links deps, installs units
│       ├── logid.cpp             # main() entry point
│       ├── logid.service.in      # systemd unit template (configure_file)
│       ├── logiops-dbus.conf.in  # D-Bus system policy template
│       ├── ipc_defs.h            # IPC service/root-node name constants
│       ├── DeviceManager.{h,cpp} # Top orchestrator (udev monitor + device/receiver maps)
│       ├── Device.{h,cpp}        # High-level device, owns features & profiles
│       ├── Receiver.{h,cpp}      # Unifying receiver wrapper
│       ├── Configuration.{h,cpp} # Loads/saves config file, defaults
│       ├── InputDevice.{h,cpp}   # Virtual evdev/uinput device for synthesized input
│       ├── backend/              # Hardware abstraction stack (see below)
│       ├── features/             # High-level user-facing device capabilities
│       ├── actions/              # Button actions + gestures
│       ├── config/               # Typed config schema + (de)serialization
│       └── util/                 # task pool, logging, exception handling
├── .github/workflows/            # CI
├── .planning/codebase/           # GSD analysis docs (this directory)
└── build/                        # Out-of-source CMake build output (generated, not committed)
```

## Directory Purposes

**`src/logid/backend/`:**
- Purpose: Layered hardware abstraction. Higher subdirectories depend only on lower ones.
- Contains:
  - `raw/` — `RawDevice`, `DeviceMonitor`, `IOMonitor` (epoll), `EventHandler` — kernel `hidraw`/udev access.
  - `hidpp/` — protocol-agnostic HID++ transport: `Device`, `Report`, `defs.h`.
  - `hidpp10/` — Unifying receiver protocol: `Receiver`, `ReceiverMonitor`, `Device`, `Error`, `defs.h`.
  - `hidpp20/` — feature-based protocol: `Device`, `Feature`, `EssentialFeature`, `feature_defs.h`, and `features/` (one file per hardware feature: `AdjustableDPI`, `SmartShift`, `HiresScroll`, `ReprogControls`, `ChangeHost`, `ThumbWheel`, `Root`, `FeatureSet`, `DeviceName`, `Reset`, `WirelessDeviceStatus`).
  - `Error.{h,cpp}`, `EventHandlerList.h` — shared backend types.

**`src/logid/features/`:**
- Purpose: User-facing capabilities implementing the `DeviceFeature` interface, combining a HID++ 2.0 feature + config + D-Bus.
- Key files: `DeviceFeature.h` (abstract base), `DPI`, `SmartShift`, `HiresScroll`, `RemapButton`, `ThumbWheel`, `DeviceStatus`.

**`src/logid/actions/`:**
- Purpose: What a remapped control does. Base `Action` plus concrete actions; `gesture/` holds the `Gesture` hierarchy for `GestureAction`.
- Key files: `Action.{h,cpp}`, `KeypressAction`, `CycleDPI`, `ChangeDPI`, `ChangeHostAction`, `ChangeProfile`, `ToggleHiresScroll`, `ToggleSmartShift`, `NullAction`, `GestureAction`; `gesture/Gesture.{h,cpp}`, `AxisGesture`, `IntervalGesture`, `ReleaseGesture`, `ThresholdGesture`, `NullGesture`.

**`src/logid/config/`:**
- Purpose: Compile-time-typed config schema mapped to libconfig and exposed over D-Bus.
- Key files: `schema.h` (the whole config tree), `types.h`, `group.h`, `map.h`, `config.cpp`.

**`src/logid/util/`:**
- Purpose: Cross-cutting helpers.
- Key files: `task.{h,cpp}` (worker pool), `log.{h,cpp}`, `ExceptionHandler.{h,cpp}`.

**`src/ipcgull/`:**
- Purpose: Bundled D-Bus IPC library (git submodule). Treat as a dependency; not edited as part of normal daemon work.

## Key File Locations

**Entry Points:**
- `src/logid/logid.cpp`: `main()` at line 132, CLI parsing above it.

**Configuration:**
- `src/logid/config/schema.h`: full typed config schema (devices, profiles, actions, gestures).
- `src/logid/Configuration.{h,cpp}`: file load/save and `defaults` namespace.
- Default config path: `/etc/logid.cfg` (`logid.cpp:32`). A sample `logid.cfg` exists at the repo root (untracked).

**Core Logic:**
- `src/logid/DeviceManager.cpp`: device discovery/classification (`addDevice` at line 63).
- `src/logid/Device.cpp`: feature registration in `_init()` (line 140), profile handling.
- `src/logid/backend/hidpp/Device.cpp`: HID++ request/response correlation.
- `src/logid/backend/hidpp20/Device.cpp`: `callFunction` feature calls.

**Build:**
- `CMakeLists.txt` (root): version detection, `USE_USER_BUS` option, submodule init.
- `src/logid/CMakeLists.txt`: full source list, dependency discovery (libevdev, libconfig, libudev, systemd), install rules.

**Deployment templates:**
- `src/logid/logid.service.in`, `src/logid/logiops-dbus.conf.in` (processed by `configure_file`).

## Naming Conventions

**Files:**
- One class per file; filename matches the class in `PascalCase` (e.g. `DeviceManager.h`, `AdjustableDPI.cpp`).
- Headers `.h`, sources `.cpp`, paired together in the same directory.
- Lowercase filenames for non-class modules and schema/util files (`schema.h`, `types.h`, `group.h`, `map.h`, `defs.h`, `feature_defs.h`, `task.h`, `log.h`, `ipc_defs.h`, `config.cpp`, `logid.cpp`).
- Templates for generated files use the `.in` suffix.

**Directories:**
- Lowercase, layered by protocol/concern (`backend/raw`, `backend/hidpp`, `backend/hidpp10`, `backend/hidpp20`, `backend/hidpp20/features`, `features`, `actions`, `actions/gesture`, `config`, `util`).

**Code identifiers (from headers):**
- Namespaces mirror directories: `logid`, `logid::backend::raw`, `logid::backend::hidpp`, `logid::backend::hidpp20`, `logid::features`, `logid::actions`, `logid::config`.
- Types: `PascalCase`. Methods: `camelCase`. Private members: `_leading_underscore` `snake_case`.
- Include guards: `LOGID_<PATH>_<NAME>_H` (e.g. `LOGID_BACKEND_RAW_DEVICEMONITOR_H`).
- Includes use angle-bracket paths rooted at `src/logid` (e.g. `#include <backend/hidpp20/Device.h>`), enabled by `include_directories(.)` in `src/logid/CMakeLists.txt`.

## Where to Add New Code

**New high-level device feature (DPI/SmartShift-style):**
- Implementation: `src/logid/features/<Name>.{h,cpp}` extending `features::DeviceFeature`; implement `configure()`, `listen()`, `setProfile()`; throw `features::UnsupportedFeature` if the backing HID++ feature is absent.
- Register it: add `_addFeature<features::<Name>>("<key>")` in `Device::_init()` (`src/logid/Device.cpp:152`).
- Config: add a field to `config::Profile` (and any new structs) in `src/logid/config/schema.h`.
- Build: append both `.cpp` files to the source list in `src/logid/CMakeLists.txt`.

**New HID++ 2.0 feature wrapper:**
- Implementation: `src/logid/backend/hidpp20/features/<Name>.{h,cpp}` extending `hidpp20::Feature`; declare its feature ID and `callFunction` wrappers.
- Build: append to `src/logid/CMakeLists.txt`.

**New button action:**
- Implementation: `src/logid/actions/<Name>Action.{h,cpp}` extending `actions::Action`.
- Wire it into the factory in `src/logid/actions/Action.cpp` (`makeAction`) and add a matching `config::` struct + variant entry in `src/logid/config/schema.h` (`BasicAction` / `Action`).
- Build: append to `src/logid/CMakeLists.txt`.

**New gesture:**
- Implementation: `src/logid/actions/gesture/<Name>Gesture.{h,cpp}` extending `actions::Gesture`; register in `Gesture::makeGesture` and add to the `config::Gesture` variant in `schema.h`.

**Utilities / shared helpers:**
- `src/logid/util/`.

## Special Directories

**`build/`:**
- Purpose: CMake out-of-source build output and generated unit/conf files.
- Generated: Yes. Committed: No.

**`src/ipcgull/`:**
- Purpose: D-Bus IPC library.
- Generated: No (git submodule, initialized by the root `CMakeLists.txt`). Committed: as a submodule reference; required (`FATAL_ERROR` if missing).

**`.planning/codebase/`:**
- Purpose: GSD analysis documents (STACK.md, INTEGRATIONS.md, ARCHITECTURE.md, STRUCTURE.md, ...).
- Generated: Yes (by GSD tooling). Committed: per project preference.

---

*Structure analysis: 2026-05-30*
