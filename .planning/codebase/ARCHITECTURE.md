# Architecture

**Analysis Date:** 2026-05-30

## Pattern Overview

**Overall:** Layered, event-driven daemon with a hardware abstraction stack. A single long-lived process monitors the system for Logitech HID devices via udev, speaks the HID++ protocol to them over `/dev/hidraw*`, applies user configuration, and exposes runtime control over D-Bus (via the bundled `ipcgull` IPC library).

**Key Characteristics:**
- Strict bottom-up layering: raw HID I/O → HID++ transport → HID++ feature wrappers → high-level device features → actions/gestures.
- Shared-pointer object graph. Almost every long-lived object is heap-allocated through a static `make()` factory that wires up a `std::weak_ptr` self-reference (the `_xWrapper<T>` CRTP idiom appears at every layer: `RawDeviceWrapper`, `_deviceWrapper`, `_deviceMonitorWrapper`, `_receiverMonitorWrapper`, `_featureWrapper`, `DeviceWrapper`, `ButtonWrapper`).
- Event-driven: hardware reports arrive on background I/O threads and are dispatched through registered event-handler callbacks. Deferred/async work is offloaded to a worker thread pool (`util/task.h`).
- Configuration is a compile-time-typed schema (`config/schema.h`) parsed from a libconfig file; the same schema objects are exposed live over D-Bus.
- Single primary device assumption: `logid::Device` currently hardcodes a requirement for HID++ 2.0 (see TODO in `src/logid/Device.h:55`).

## Layers

**Raw HID I/O layer (`backend/raw/`):**
- Purpose: Talk to the kernel `hidraw` interface and watch for device hotplug.
- Location: `src/logid/backend/raw/`
- Contains: `RawDevice` (one open `hidraw` fd, send/receive raw reports), `DeviceMonitor` (udev enumeration + hotplug, abstract `addDevice`/`removeDevice`), `IOMonitor` (epoll loop on one background thread, dispatches read/hangup/error per fd), `EventHandler`.
- Depends on: libudev, libevdev, the Linux `hidraw` and `epoll`/`eventfd` syscalls.
- Used by: HID++ transport layer and `DeviceManager`.

**HID++ transport layer (`backend/hidpp/`):**
- Purpose: Protocol-agnostic HID++ framing on top of a `RawDevice`. Builds/parses `Report`s, matches responses to requests, dispatches asynchronous events.
- Location: `src/logid/backend/hidpp/`
- Key files: `Device.h/.cpp` (request/response correlation, `EventHandler` registration, `InvalidDevice` reasons), `Report.h/.cpp`, `defs.h` (`DeviceIndex`, `DeviceConnectionEvent`).
- Depends on: `backend/raw/`.
- Used by: `hidpp10` and `hidpp20`.

**HID++ 1.0 / receiver layer (`backend/hidpp10/`):**
- Purpose: Unifying-receiver (DJ) protocol — pairing, device connect/disconnect notifications, enumeration of paired devices.
- Location: `src/logid/backend/hidpp10/`
- Key files: `Receiver.h/.cpp`, `ReceiverMonitor.h/.cpp` (abstract `addDevice(DeviceConnectionEvent)`/`removeDevice`/`pairReady`, runs on the RawDevice thread), `Device.h`, `Error.h`, `defs.h`.
- Depends on: `backend/hidpp/`.
- Used by: `logid::Receiver`.

**HID++ 2.0 feature layer (`backend/hidpp20/`):**
- Purpose: Modern feature-based protocol. `Device` adds `callFunction`/`callFunctionNoResponse` and response multiplexing; each hardware feature is a `Feature` subclass that resolves its feature index via the `Root` feature and wraps the device's functions.
- Location: `src/logid/backend/hidpp20/` and `backend/hidpp20/features/`
- Key files: `Device.h/.cpp`, `Feature.h/.cpp` (base; throws `UnsupportedFeature` if absent), `EssentialFeature.h`, `feature_defs.h`. Features include `Root`, `FeatureSet`, `AdjustableDPI`, `SmartShift`, `HiresScroll`, `ReprogControls`, `ChangeHost`, `ThumbWheel`, `WirelessDeviceStatus`, `DeviceName`, `Reset`.
- Depends on: `backend/hidpp/`.
- Used by: high-level `features/`.

**High-level feature layer (`features/`):**
- Purpose: User-facing device capabilities that combine HID++ 2.0 features with configuration and D-Bus interfaces. Each implements the `DeviceFeature` interface (`configure()`, `listen()`, `setProfile()`).
- Location: `src/logid/features/`
- Key files: `DeviceFeature.h` (abstract base + `make<T>` factory), `DPI`, `SmartShift`, `HiresScroll`, `RemapButton`, `ThumbWheel`, `DeviceStatus`.
- Depends on: `backend/hidpp20/`, `config/`, `actions/`, `ipcgull`.
- Used by: `logid::Device`.

**Action / gesture layer (`actions/`):**
- Purpose: What happens when a remapped button is pressed/released/moved. `Action` is the abstract base (`press`/`release`/`move`/`reprogFlags`); `GestureAction` further delegates to per-direction `Gesture` objects.
- Location: `src/logid/actions/` and `actions/gesture/`
- Key files: `Action.h/.cpp` (factory `makeAction` switches on the config variant), concrete actions (`KeypressAction`, `CycleDPI`, `ChangeDPI`, `ChangeHostAction`, `ChangeProfile`, `ToggleHiresScroll`, `ToggleSmartShift`, `NullAction`, `GestureAction`), and gestures (`Gesture.h/.cpp`, `AxisGesture`, `IntervalGesture`, `ReleaseGesture`, `ThresholdGesture`, `NullGesture`).
- Depends on: `features/RemapButton` (the `Button` that owns them), `InputDevice`, `config/schema.h`, `ipcgull`.
- Used by: `features/RemapButton`'s `Button`.

**Orchestration layer (top-level `src/logid/`):**
- Purpose: Tie everything together and own the object graph.
- Key files: `logid.cpp` (entry), `DeviceManager` (extends `raw::DeviceMonitor`), `Device`, `Receiver` (extends `hidpp10::ReceiverMonitor` + `ipcgull::object`), `Configuration`, `InputDevice` (virtual evdev/uinput device for synthesizing input), `ipc_defs.h`.

**Configuration layer (`config/`):**
- Purpose: Typed, reflective config schema that maps libconfig settings to C++ structs and simultaneously exposes them over D-Bus.
- Key files: `schema.h` (the full config tree), `types.h`, `group.h`, `map.h`, `config.cpp` (serialization templates using C++20 string-literal template parameters).

**Utility layer (`util/`):**
- Purpose: Cross-cutting helpers. `task.h/.cpp` (worker thread pool, `run_task`/`run_task_after`), `log.h/.cpp` (`logPrintf`, global log level), `ExceptionHandler.h/.cpp`.

## Data Flow

**Device discovery and configuration:**

1. `main()` (`src/logid/logid.cpp:132`) reads CLI options, loads `Configuration`, calls `init_workers()`, creates the `ipcgull` server, creates the virtual `InputDevice`, then `DeviceManager::make<DeviceManager>(...)` and `device_manager->enumerate()`.
2. `DeviceManager` (subclass of `raw::DeviceMonitor`) enumerates existing `hidraw` nodes and subscribes to udev hotplug. Each appearance calls `DeviceManager::addDevice(path)` (`src/logid/DeviceManager.cpp:63`).
3. `addDevice` first opens a throwaway `RawDevice` to honor the `ignore` PID list, then probes with `hidpp::Device::make(path, DefaultDevice, ...)`. If `version() == {1,0}` it is a Unifying receiver → `Receiver::make(path, ...)`; otherwise a direct `Device::make(path, DefaultDevice|CordedDevice, ...)`.
4. For receivers, `hidpp10::ReceiverMonitor` listens for `DeviceConnectionEvent`s and calls `Receiver::addDevice(event)` per paired wireless device, each producing a `logid::Device`.
5. `Device::_init()` (`src/logid/Device.cpp:140`) constructs the underlying `hidpp20::Device`, then registers high-level features with `_addFeature<T>(name)` (DPI, SmartShift, HiresScroll, RemapButton, DeviceStatus, ThumbWheel). `_addFeature` swallows `UnsupportedFeature`, so only features the hardware actually advertises survive. It then calls `configure()` and `listen()` on each surviving feature.
6. Each feature's `configure()` pushes the active profile's settings to the hardware over HID++ 2.0; `listen()` registers HID++ event handlers for asynchronous hardware events.

**Runtime input event (button remap):**

1. The hardware sends a HID++ report; the kernel delivers it to the open `hidraw` fd, which `IOMonitor`'s epoll thread reads and hands to `RawDevice::_handleEvent`.
2. `RawDevice` notifies its `EventHandlerList`, reaching `hidpp::Device::handleEvent`, which dispatches to matching `EventHandler` callbacks.
3. `features::RemapButton` (registered in `listen()`) receives diverted control events and computes the new pressed-button set in `_buttonEvent` (`src/logid/features/RemapButton.h:104`).
4. The owning `Button` invokes its configured `Action`'s `press()`/`release()`/`move()`. Actions such as `KeypressAction` synthesize input through the shared virtual `InputDevice`; `GestureAction` routes movement to the appropriate directional `Gesture`.
5. Longer or delayed work (e.g. `setProfileDelayed`, threshold timers) is queued onto the worker pool via `run_task` / `run_task_after`.

**Control via D-Bus:**
- The `ipcgull` server publishes nodes for the config, devices, and receivers. Clients can change DPI, switch profiles, remap actions, start/stop pairing, etc. These calls mutate the live config schema objects and re-invoke `configure()`/`reconfigure()` on the affected device/feature.

**State Management:**
- Config state lives in the `config::Config` schema tree owned by `Configuration`; features hold `std::reference_wrapper`s into it so D-Bus edits are visible immediately.
- Per-device runtime state (`_awake`, active `_profile`) is guarded by per-object mutexes (`_state_lock`, `_profile_mutex`). `DeviceManager` guards its device/receiver maps with `_map_lock` and nickname allocation with `_nick_lock`.

## Key Abstractions

**`make()` factory + `_xWrapper<T>` CRTP self-reference:**
- Purpose: Safely create `shared_ptr`-managed objects that need a `weak_ptr` to themselves before construction completes.
- Examples: `src/logid/backend/raw/RawDevice.h:63`, `backend/hidpp/Device.h:168`, `backend/raw/DeviceMonitor.h:61`, `features/DeviceFeature.h:83`.
- Pattern: static `make(...)` builds a derived wrapper, assigns `_self`, then runs a post-construction hook (`_ready()`, `_setupReportsAndInit()`, etc.).

**`DeviceFeature`:**
- Purpose: Uniform contract for high-level device capabilities.
- Examples: `src/logid/features/DPI.h`, `features/RemapButton.h`.
- Pattern: pure virtual `configure()` / `listen()` / `setProfile(config::Profile&)`; constructor throws `features::UnsupportedFeature` when the backing HID++ feature is missing.

**`Action` / `Gesture` factories over config variants:**
- Purpose: Map a `std::variant` config node (`config::Action`, `config::Gesture`) to a concrete polymorphic object.
- Examples: `src/logid/actions/Action.h:50`, `actions/gesture/Gesture.h:52`.

**`hidpp20::Feature`:**
- Purpose: Wrap one HID++ 2.0 feature, resolving its runtime feature index and exposing `callFunction`.
- Examples: `src/logid/backend/hidpp20/features/AdjustableDPI.h`, `features/ReprogControls.h`.

**`EventHandlerList` / `EventHandlerLock`:**
- Purpose: RAII-scoped registration of asynchronous event callbacks at the raw and HID++ layers.
- Examples: `src/logid/backend/EventHandlerList.h`, used in `backend/hidpp/Device.h:146` and `features/RemapButton.h:125`.

## Entry Points

**`main` (`src/logid/logid.cpp:132`):**
- Triggers: process start (typically the `logid.service` systemd unit).
- Responsibilities: parse CLI, load config, start worker pool, start IPC server, create virtual input, create and enumerate the `DeviceManager`, then block in `server->start()`.

**`DeviceManager::addDevice` / `removeDevice` (`src/logid/DeviceManager.cpp:63`):**
- Triggers: udev hotplug + initial enumeration.
- Responsibilities: classify each `hidraw` node as receiver vs. direct device and instantiate the appropriate object.

**`hidpp::Device::handleEvent` and feature event handlers:**
- Triggers: asynchronous HID++ reports from hardware via the `IOMonitor` epoll thread.
- Responsibilities: dispatch hardware events to subscribed features/actions.

**`ipcgull` D-Bus methods:**
- Triggers: external clients on the system (or user) bus.
- Responsibilities: live control of devices, profiles, and configuration.

## Error Handling

**Strategy:** Exceptions for exceptional/transient conditions, with targeted catches at layer boundaries.

**Patterns:**
- Probing in `DeviceManager::addDevice` catches `hidpp10::Error`, `hidpp20::Error`, `hidpp::Device::InvalidDevice`, `std::system_error`, and `TimeoutError`; transient cases re-throw `DeviceNotReady()` to trigger a retry/backoff (`max_tries`, `ready_backoff` in `DeviceMonitor.h`).
- `hidpp20::UnsupportedFeature` / `features::UnsupportedFeature` are used as control flow to skip capabilities a device lacks (`_addFeature` silently swallows them).
- `hidpp::Device::InvalidDevice` carries a `Reason` enum (`NoHIDPPReport`, `InvalidRawDevice`, `Asleep`, `VirtualNode`) so callers can distinguish "skip" from "retry".
- Background/worker tasks route exceptions through `util/ExceptionHandler`.
- Fatal startup errors log via `logPrintf(ERROR, ...)` and `return EXIT_FAILURE` from `main`.

## Cross-Cutting Concerns

**Logging:** `src/logid/util/log.h` — `logPrintf(level, fmt, ...)` with a process-global `global_loglevel` (`logid.cpp:40`), set from `-v/--verbose`. stdout is unbuffered so journald captures output.

**Concurrency:** One epoll I/O thread per `IOMonitor` for hardware reads, plus a fixed worker pool (`config.workers`, default 4) created by `init_workers` for deferred/delayed tasks. Object state is protected by per-object `std::mutex` / `std::shared_mutex`.

**Validation:** Centralized in the `config/` schema — `config_io` templates parse/validate libconfig settings into typed structs and log per-setting errors via `config::logError`.

**IPC / Authentication:** D-Bus through `ipcgull`. Bus selection is compile-time: `USE_USER_BUS` chooses the user bus, otherwise the system bus (`logid.cpp:154`); the system bus is gated by the installed `pizza.pixl.LogiOps.conf` policy.

**Device communication:** All hardware traffic flows through `RawDevice` → `hidpp::Device` (report framing + response matching) → `hidpp20::Device::callFunction`. There is no direct syscall access above the `backend/raw/` layer.

---

*Architecture analysis: 2026-05-30*
