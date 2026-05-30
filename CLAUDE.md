<!-- GSD:project-start source:PROJECT.md -->
## Project

**Logi Options+ for Linux (working title — name TBD)**

A polished Linux desktop application that reconstructs the **Logitech Options+** experience — visually (a faithful clone of its look-and-feel) and, above all, in **features** — for configuring Logitech HID++ devices (mice/keyboards). It is built **on top of the existing `logiops` daemon** in this repo: the GUI talks to the `logid` daemon over D-Bus and edits its configuration, reusing all the HID++ communication that already works. Where the daemon can't yet do what a feature needs (better gestures, action wheel), **the logiops C++ daemon is extended** as part of the same project. Linux only, Debian first.

**Core Value:** Bring the **full Options+ experience to Linux** — the rich configuration UX that today only exists on Windows/macOS — and in doing so fix the real rough edges of raw `logiops` (text-only config, limited/buggy gestures, no action wheel). If everything else fails, a Linux user with a Logitech mouse must be able to configure it through a real GUI instead of hand-editing `/etc/logid.cfg`.

### Constraints

- **Tech stack**: Linux only; Debian-first. GUI tech is open (to be recommended by research — e.g. Tauri / Qt / GTK). Must speak D-Bus to `logid`. Avoid choices that hard-block future Win/macOS ports.
- **Architecture**: GUI sits on top of the existing `logiops` daemon (D-Bus), but the daemon C++ may be extended/forked when a feature requires it (gestures, action wheel). One combined project/repo.
- **Privilege/IPC**: logiops' D-Bus system-bus policy (`pizza.pixl.LogiOps.conf`) currently grants access to `root` only. A normal-user GUI needs an access path — D-Bus policy change, user-bus build (`USE_USER_BUS`), or polkit. Must be solved.
- **Security**: daemon runs as root and parses untrusted HID input (see `.planning/codebase/CONCERNS.md`). Any daemon extension must respect that threat surface.
- **Daemon limitation**: `logid::Device` currently hardcodes HID++ 2.0 (TODO at `src/logid/Device.h:55`); multi-device richness may need work.
<!-- GSD:project-end -->

<!-- GSD:stack-start source:codebase/STACK.md -->
## Technology Stack

## Languages
- C++20 - Entire daemon and IPC library. The C++20 requirement is driven by string-literal template parameters (noted in `src/logid/CMakeLists.txt` line 4). Standard enforced via `CMAKE_CXX_STANDARD 20` / `CMAKE_CXX_STANDARD_REQUIRED ON` in `CMakeLists.txt` and `src/logid/CMakeLists.txt`.
- C (kernel UAPI headers) - Low-level HID/input access uses Linux C headers directly (`<linux/hidraw.h>`, `<linux/input.h>`) in `src/logid/backend/raw/RawDevice.cpp`.
- CMake script - Build configuration across `CMakeLists.txt`, `src/logid/CMakeLists.txt`, `src/ipcgull/CMakeLists.txt`.
## Runtime
- Linux only. Native ELF daemon (`logid`). No managed runtime.
- Runs as a `systemd` system service (`Type=simple`, `User=root`) per `src/logid/logid.service.in`.
- Must run as root in production; a user-bus development mode exists via the `-DUSE_USER_BUS=ON` CMake option (see `CMakeLists.txt` line 15, `src/logid/logid.cpp` line 154).
- None at the language level (no vendored package manager). Dependencies are resolved from the host distribution via `pkg-config` / `pkg_check_modules`.
- Submodules managed by Git (`.gitmodules`): `src/ipcgull` from `https://github.com/PixlOne/ipcgull.git`. CMake auto-runs `git submodule update --init --recursive` (`CMakeLists.txt` lines 33-35).
- Lockfile: Not applicable (system libraries pinned by distro, not by this repo).
## Frameworks
- ipcgull (bundled submodule, `src/ipcgull/`) - D-Bus object/interface abstraction layer used to expose devices and features over IPC. Built as an `OBJECT` library, linked statically into `logid` (`BUILD_STATIC ON` by default in `src/ipcgull/CMakeLists.txt`).
- libconfig (libconfig++) - Configuration file parsing for `/etc/logid.cfg`. Used via `using namespace libconfig;` in `src/logid/Configuration.cpp`. Linked as `config++` (`src/logid/CMakeLists.txt` line 89).
- ipcgull ships an optional standalone server test (`src/ipcgull/tests/server_test/`, gated behind `-DBUILD_TESTS`). The main `logid` daemon has no unit-test framework wired into its build.
- CMake >= 3.12 (top level and `logid`); ipcgull requires >= 3.10 - Build system.
- pkg-config (`PkgConfig` REQUIRED) - Locating system libraries (`libevdev`, `libconfig`, `libudev`, `systemd`, `gio-2.0`, `glib-2.0`).
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` - Generates `compile_commands.json` for tooling.
## Key Dependencies
- libevdev - Creating and writing to the virtual `uinput` device (keyboard/relative axis emulation). Detected via `pkg_check_modules(PC_EVDEV libevdev REQUIRED)`; headers `<libevdev/libevdev.h>`, `<libevdev/libevdev-uinput.h>` used in `src/logid/InputDevice.cpp`.
- libudev - Device hotplug discovery and monitoring of `hidraw` devices. `pkg_check_modules(LIBUDEV libudev REQUIRED)`; used in `src/logid/backend/raw/DeviceMonitor.cpp`.
- libconfig++ (`config++`) - Config parsing (see above).
- ipcgull (static) - D-Bus IPC (see above).
- Threads (`CMAKE_THREAD_LIBS_INIT`, `find_package(Threads REQUIRED)`) - Multithreaded task scheduling (`src/logid/util/task.cpp`).
- glib-2.0 - GLib core used by the GIO D-Bus backend.
- gio-2.0 - GDBus implementation backing ipcgull (`src/ipcgull/src/common_gdbus.cpp`, `src/ipcgull/src/server_gdbus.cpp`).
- systemd - Detected via `pkg_check_modules(SYSTEMD "systemd")` (not REQUIRED). When present, the build installs `logid.service` into the systemd system unit dir resolved from `pkg-config --variable=systemdsystemunitdir systemd` (`src/logid/CMakeLists.txt` lines 94-111).
## Configuration
- Daemon configured by a single libconfig file. Default path `/etc/logid.cfg` (`default_config` in `src/logid/logid.cpp`); override with `-c`/`--config`.
- CLI options parsed manually (no getopt) in `src/logid/logid.cpp`: `-v/--verbose [level]`, `-V/--version`, `-c/--config <path>`, `-h/--help`.
- Example config: `logid.example.cfg` (libconfig syntax: `devices: ( { ... } )` with `smartshift`, `hiresscroll`, `dpi`, `buttons` sub-blocks).
- A working `logid.cfg` is present at repo root (untracked).
- `USE_USER_BUS` (default OFF) - Adds `-DUSE_USER_BUS`; switches D-Bus from system to session bus for non-root dev.
- `CMAKE_BUILD_TYPE` (e.g. `Release`) - Standard CMake.
- `CMAKE_INSTALL_PREFIX` hardcoded to `/usr` in top-level `CMakeLists.txt` (line 3).
- Version baked in via `-DLOGIOPS_VERSION="..."`, derived from `git describe --tags` or `version.txt` (`CMakeLists.txt` lines 17-53).
- Warning flags: `-Wall -Wextra` always; CI additionally builds with `-Werror`.
- `CMakeLists.txt`, `src/logid/CMakeLists.txt`, `src/ipcgull/CMakeLists.txt`
- `.editorconfig` (style), `.gitignore`, `.gitmodules`
- Templated install artifacts: `src/logid/logid.service.in`, `src/logid/logiops-dbus.conf.in`
## Platform Requirements
- C++20 compiler (GCC/Clang), CMake, pkg-config, and dev packages: `libevdev`, `libudev`, `libconfig++`, `glib2`/`gio`. Per `README.md`, distro install commands provided for Arch, Debian/Ubuntu, Fedora, Gentoo, Solus, openSUSE.
- CI build matrix (`.github/workflows/build-test.yml`): `ubuntu:latest`, `ubuntu:20.04`, `fedora:latest`, `archlinux:base-devel`, built with `-DCMAKE_CXX_FLAGS="-Werror"`.
- Linux with `hidraw` and `uinput` kernel support, a running D-Bus system bus, and (typically) systemd. Installs `logid` to `/usr/bin`, a systemd unit, and a D-Bus system policy file.
<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->
## Conventions

## Language & Standard
- Modern **C++20** throughout (`src/logid/` and the vendored `src/ipcgull/`).
- Formatting is governed by `.editorconfig` at the repo root — match its indentation and line-ending rules rather than guessing.
## Naming
| Element | Convention | Example |
|---------|-----------|---------|
| Classes / types | PascalCase | `DeviceManager`, `RawDevice`, `ReceiverMonitor` |
| Files | PascalCase, matching the primary class | `DeviceManager.cpp`, `InputDevice.h` |
| Methods | camelCase | `addDevice()`, `listen()`, `save()` |
| Private members | leading underscore | `_devices`, `_config`, `_data` |
| Free / util functions | snake_case | utilities in `src/logid/util/` |
| Header guards | `#ifndef` / `#define` include guards (not `#pragma once`) | per-header `LOGID_..._H` |
## C++ Patterns
- **Smart pointers** for ownership — `std::shared_ptr` / `std::unique_ptr` rather than raw owning pointers.
- **`make()` factory methods** — objects that need post-construction setup or shared ownership are created through static `make()` factories rather than bare constructors.
- **Concurrency** — `std::shared_mutex` for reader/writer protection of shared state.
- **Modifiers** — `[[nodiscard]]` on value-returning queries and `final` on leaf classes are used deliberately; follow suit.
- **IPC interfaces** — D-Bus-exposed objects use nested `ipcgull` interface classes (the vendored binding in `src/ipcgull/`).
## Error Handling
- **Exception-based.** Errors propagate as thrown exceptions rather than return codes.
- A central handler, `ExceptionHandler::Default` (`src/logid/util/ExceptionHandler.*`), provides default handling. Note: this file currently has a slicing bug (see `CONCERNS.md` #1) — be careful to rethrow with bare `throw;`, not `throw error;`.
## Logging
- Custom `printf`-style logger via `logPrintf` (`src/logid/util/log.h`). Log levels include `WARN`, etc.
- The logger is explicitly flagged for future replacement (`src/logid/util/log.h:24`).
- Avoid passing untrusted/runtime strings as the format argument (format-string hazard — see `CONCERNS.md` #5).
## Build / Structure Conventions
- CMake-based build; each subtree has its own `CMakeLists.txt` (root `CMakeLists.txt`, `src/logid/CMakeLists.txt`, etc.).
- Config schema is declared declaratively in `src/logid/config/schema.h`.
- Backend code is layered by protocol generation under `src/logid/backend/` (`raw/`, `hidpp/`, `hidpp10/`, `hidpp20/`) — see `ARCHITECTURE.md` and `STRUCTURE.md`.
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->
## Architecture

## Pattern Overview
- Strict bottom-up layering: raw HID I/O → HID++ transport → HID++ feature wrappers → high-level device features → actions/gestures.
- Shared-pointer object graph. Almost every long-lived object is heap-allocated through a static `make()` factory that wires up a `std::weak_ptr` self-reference (the `_xWrapper<T>` CRTP idiom appears at every layer: `RawDeviceWrapper`, `_deviceWrapper`, `_deviceMonitorWrapper`, `_receiverMonitorWrapper`, `_featureWrapper`, `DeviceWrapper`, `ButtonWrapper`).
- Event-driven: hardware reports arrive on background I/O threads and are dispatched through registered event-handler callbacks. Deferred/async work is offloaded to a worker thread pool (`util/task.h`).
- Configuration is a compile-time-typed schema (`config/schema.h`) parsed from a libconfig file; the same schema objects are exposed live over D-Bus.
- Single primary device assumption: `logid::Device` currently hardcodes a requirement for HID++ 2.0 (see TODO in `src/logid/Device.h:55`).
## Layers
- Purpose: Talk to the kernel `hidraw` interface and watch for device hotplug.
- Location: `src/logid/backend/raw/`
- Contains: `RawDevice` (one open `hidraw` fd, send/receive raw reports), `DeviceMonitor` (udev enumeration + hotplug, abstract `addDevice`/`removeDevice`), `IOMonitor` (epoll loop on one background thread, dispatches read/hangup/error per fd), `EventHandler`.
- Depends on: libudev, libevdev, the Linux `hidraw` and `epoll`/`eventfd` syscalls.
- Used by: HID++ transport layer and `DeviceManager`.
- Purpose: Protocol-agnostic HID++ framing on top of a `RawDevice`. Builds/parses `Report`s, matches responses to requests, dispatches asynchronous events.
- Location: `src/logid/backend/hidpp/`
- Key files: `Device.h/.cpp` (request/response correlation, `EventHandler` registration, `InvalidDevice` reasons), `Report.h/.cpp`, `defs.h` (`DeviceIndex`, `DeviceConnectionEvent`).
- Depends on: `backend/raw/`.
- Used by: `hidpp10` and `hidpp20`.
- Purpose: Unifying-receiver (DJ) protocol — pairing, device connect/disconnect notifications, enumeration of paired devices.
- Location: `src/logid/backend/hidpp10/`
- Key files: `Receiver.h/.cpp`, `ReceiverMonitor.h/.cpp` (abstract `addDevice(DeviceConnectionEvent)`/`removeDevice`/`pairReady`, runs on the RawDevice thread), `Device.h`, `Error.h`, `defs.h`.
- Depends on: `backend/hidpp/`.
- Used by: `logid::Receiver`.
- Purpose: Modern feature-based protocol. `Device` adds `callFunction`/`callFunctionNoResponse` and response multiplexing; each hardware feature is a `Feature` subclass that resolves its feature index via the `Root` feature and wraps the device's functions.
- Location: `src/logid/backend/hidpp20/` and `backend/hidpp20/features/`
- Key files: `Device.h/.cpp`, `Feature.h/.cpp` (base; throws `UnsupportedFeature` if absent), `EssentialFeature.h`, `feature_defs.h`. Features include `Root`, `FeatureSet`, `AdjustableDPI`, `SmartShift`, `HiresScroll`, `ReprogControls`, `ChangeHost`, `ThumbWheel`, `WirelessDeviceStatus`, `DeviceName`, `Reset`.
- Depends on: `backend/hidpp/`.
- Used by: high-level `features/`.
- Purpose: User-facing device capabilities that combine HID++ 2.0 features with configuration and D-Bus interfaces. Each implements the `DeviceFeature` interface (`configure()`, `listen()`, `setProfile()`).
- Location: `src/logid/features/`
- Key files: `DeviceFeature.h` (abstract base + `make<T>` factory), `DPI`, `SmartShift`, `HiresScroll`, `RemapButton`, `ThumbWheel`, `DeviceStatus`.
- Depends on: `backend/hidpp20/`, `config/`, `actions/`, `ipcgull`.
- Used by: `logid::Device`.
- Purpose: What happens when a remapped button is pressed/released/moved. `Action` is the abstract base (`press`/`release`/`move`/`reprogFlags`); `GestureAction` further delegates to per-direction `Gesture` objects.
- Location: `src/logid/actions/` and `actions/gesture/`
- Key files: `Action.h/.cpp` (factory `makeAction` switches on the config variant), concrete actions (`KeypressAction`, `CycleDPI`, `ChangeDPI`, `ChangeHostAction`, `ChangeProfile`, `ToggleHiresScroll`, `ToggleSmartShift`, `NullAction`, `GestureAction`), and gestures (`Gesture.h/.cpp`, `AxisGesture`, `IntervalGesture`, `ReleaseGesture`, `ThresholdGesture`, `NullGesture`).
- Depends on: `features/RemapButton` (the `Button` that owns them), `InputDevice`, `config/schema.h`, `ipcgull`.
- Used by: `features/RemapButton`'s `Button`.
- Purpose: Tie everything together and own the object graph.
- Key files: `logid.cpp` (entry), `DeviceManager` (extends `raw::DeviceMonitor`), `Device`, `Receiver` (extends `hidpp10::ReceiverMonitor` + `ipcgull::object`), `Configuration`, `InputDevice` (virtual evdev/uinput device for synthesizing input), `ipc_defs.h`.
- Purpose: Typed, reflective config schema that maps libconfig settings to C++ structs and simultaneously exposes them over D-Bus.
- Key files: `schema.h` (the full config tree), `types.h`, `group.h`, `map.h`, `config.cpp` (serialization templates using C++20 string-literal template parameters).
- Purpose: Cross-cutting helpers. `task.h/.cpp` (worker thread pool, `run_task`/`run_task_after`), `log.h/.cpp` (`logPrintf`, global log level), `ExceptionHandler.h/.cpp`.
## Data Flow
- The `ipcgull` server publishes nodes for the config, devices, and receivers. Clients can change DPI, switch profiles, remap actions, start/stop pairing, etc. These calls mutate the live config schema objects and re-invoke `configure()`/`reconfigure()` on the affected device/feature.
- Config state lives in the `config::Config` schema tree owned by `Configuration`; features hold `std::reference_wrapper`s into it so D-Bus edits are visible immediately.
- Per-device runtime state (`_awake`, active `_profile`) is guarded by per-object mutexes (`_state_lock`, `_profile_mutex`). `DeviceManager` guards its device/receiver maps with `_map_lock` and nickname allocation with `_nick_lock`.
## Key Abstractions
- Purpose: Safely create `shared_ptr`-managed objects that need a `weak_ptr` to themselves before construction completes.
- Examples: `src/logid/backend/raw/RawDevice.h:63`, `backend/hidpp/Device.h:168`, `backend/raw/DeviceMonitor.h:61`, `features/DeviceFeature.h:83`.
- Pattern: static `make(...)` builds a derived wrapper, assigns `_self`, then runs a post-construction hook (`_ready()`, `_setupReportsAndInit()`, etc.).
- Purpose: Uniform contract for high-level device capabilities.
- Examples: `src/logid/features/DPI.h`, `features/RemapButton.h`.
- Pattern: pure virtual `configure()` / `listen()` / `setProfile(config::Profile&)`; constructor throws `features::UnsupportedFeature` when the backing HID++ feature is missing.
- Purpose: Map a `std::variant` config node (`config::Action`, `config::Gesture`) to a concrete polymorphic object.
- Examples: `src/logid/actions/Action.h:50`, `actions/gesture/Gesture.h:52`.
- Purpose: Wrap one HID++ 2.0 feature, resolving its runtime feature index and exposing `callFunction`.
- Examples: `src/logid/backend/hidpp20/features/AdjustableDPI.h`, `features/ReprogControls.h`.
- Purpose: RAII-scoped registration of asynchronous event callbacks at the raw and HID++ layers.
- Examples: `src/logid/backend/EventHandlerList.h`, used in `backend/hidpp/Device.h:146` and `features/RemapButton.h:125`.
## Entry Points
- Triggers: process start (typically the `logid.service` systemd unit).
- Responsibilities: parse CLI, load config, start worker pool, start IPC server, create virtual input, create and enumerate the `DeviceManager`, then block in `server->start()`.
- Triggers: udev hotplug + initial enumeration.
- Responsibilities: classify each `hidraw` node as receiver vs. direct device and instantiate the appropriate object.
- Triggers: asynchronous HID++ reports from hardware via the `IOMonitor` epoll thread.
- Responsibilities: dispatch hardware events to subscribed features/actions.
- Triggers: external clients on the system (or user) bus.
- Responsibilities: live control of devices, profiles, and configuration.
## Error Handling
- Probing in `DeviceManager::addDevice` catches `hidpp10::Error`, `hidpp20::Error`, `hidpp::Device::InvalidDevice`, `std::system_error`, and `TimeoutError`; transient cases re-throw `DeviceNotReady()` to trigger a retry/backoff (`max_tries`, `ready_backoff` in `DeviceMonitor.h`).
- `hidpp20::UnsupportedFeature` / `features::UnsupportedFeature` are used as control flow to skip capabilities a device lacks (`_addFeature` silently swallows them).
- `hidpp::Device::InvalidDevice` carries a `Reason` enum (`NoHIDPPReport`, `InvalidRawDevice`, `Asleep`, `VirtualNode`) so callers can distinguish "skip" from "retry".
- Background/worker tasks route exceptions through `util/ExceptionHandler`.
- Fatal startup errors log via `logPrintf(ERROR, ...)` and `return EXIT_FAILURE` from `main`.
## Cross-Cutting Concerns
<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->
## Project Skills

No project skills found. Add skills to any of: `.claude/skills/`, `.agents/skills/`, `.cursor/skills/`, or `.github/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->
## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:
- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->



<!-- GSD:profile-start -->
## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
