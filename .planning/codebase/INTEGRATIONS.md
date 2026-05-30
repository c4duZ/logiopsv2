# External Integrations

**Analysis Date:** 2026-05-30

This is a local Linux system daemon. It has **no network/cloud integrations** (no HTTP clients, no remote APIs, no SaaS SDKs). All "integrations" are with the Linux kernel, hardware (Logitech HID++ devices), and host system services (udev, D-Bus, systemd).

## APIs & External Services

**Cloud / Remote APIs:**
- None. The daemon does not make outbound network calls and exposes no network sockets.

**Hardware protocol (the core integration):**
- Logitech HID++ protocol (both 1.0 and 2.0) over raw HID.
  - HID++ 1.0 backend: `src/logid/backend/hidpp10/` (`Device.cpp`, `Receiver.cpp`, `ReceiverMonitor.cpp`, `Error.cpp`).
  - HID++ 2.0 backend: `src/logid/backend/hidpp20/` (`Device.cpp`, `Feature.cpp`, plus per-feature handlers under `backend/hidpp20/features/`: `Root`, `FeatureSet`, `DeviceName`, `Reset`, `AdjustableDPI`, `SmartShift`, `ReprogControls`, `HiresScroll`, `ChangeHost`, `WirelessDeviceStatus`, `ThumbWheel`).
  - Shared HID++ report framing: `src/logid/backend/hidpp/Device.cpp`, `src/logid/backend/hidpp/Report.cpp`.
  - Wireless receivers (Unifying-style) handled via `hidpp10::Receiver` / `ReceiverMonitor` and `src/logid/Receiver.cpp`.

## Data Storage

**Databases:**
- None.

**File Storage:**
- Local filesystem only. Single config file read from `/etc/logid.cfg` (override via `-c`), parsed with libconfig in `src/logid/Configuration.cpp` / `src/logid/config/config.cpp`. No data is persisted back by the daemon.

**Caching:**
- None (in-memory device/feature state only).

## Hardware & Kernel Interfaces

**HID (raw device access):**
- Linux `hidraw` via direct file descriptors and `ioctl`. `src/logid/backend/raw/RawDevice.cpp` includes `<linux/hidraw.h>`, `<linux/input.h>`, `<sys/ioctl.h>`, `<fcntl.h>` and issues `HIDIOCGRAWINFO`, `HIDIOCGRAWPHYS`, `HIDIOCGRAWNAME`, `HIDIOCGRDESCSIZE`, `HIDIOCGRDESC`. Reads/writes raw HID reports to `/dev/hidraw*`.

**Input injection (uinput):**
- Virtual input device created with libevdev + uinput. `src/logid/InputDevice.cpp` uses `libevdev_new`, `libevdev_enable_event_type` (`EV_KEY`, `EV_REL`), and `libevdev_uinput_create_from_device`. Virtual device name: `"LogiOps Virtual Input"` (`virtual_input_name` in `src/logid/logid.cpp`). This is how button remaps / keypress / scroll actions reach the OS.

**Device discovery & hotplug (udev):**
- libudev netlink monitor. `src/logid/backend/raw/DeviceMonitor.cpp` calls `udev_new`, `udev_monitor_new_from_netlink(ctx, "udev")`, and `udev_monitor_filter_add_match_subsystem_devtype(monitor, "hidraw", nullptr)` to watch for `hidraw` device add/remove events.

**Event loop (epoll/eventfd):**
- `src/logid/backend/raw/IOMonitor.cpp` uses `epoll_create1`, `<sys/epoll.h>`, and `<sys/eventfd.h>` to multiplex hidraw + udev file descriptors and wake the loop.

**Threading:**
- POSIX threads (linked via CMake `Threads`). Task scheduling in `src/logid/util/task.cpp`; exception handling in `src/logid/util/ExceptionHandler.cpp`.

## Authentication & Identity

- None in the application sense. Privilege model is OS-level: the daemon runs as `root` (`User=root` in `src/logid/logid.service.in`) to access `hidraw`/`uinput` and own its D-Bus name. Access to the D-Bus service is restricted to root by policy (see Webhooks/IPC below).

## Inter-Process Communication (D-Bus)

**Mechanism:**
- D-Bus, implemented through the bundled `ipcgull` library on a GDBus (gio-2.0/glib-2.0) backend (`src/ipcgull/src/common_gdbus.cpp`, `src/ipcgull/src/server_gdbus.cpp`).

**Bus selection:**
- Default: D-Bus **system bus** (`ipcgull::IPCGULL_SYSTEM` -> `G_BUS_TYPE_SYSTEM`).
- With `-DUSE_USER_BUS=ON`: session bus instead (`src/logid/logid.cpp` lines 154-160; bus mapping in `src/ipcgull/src/server_gdbus.cpp`).

**Service identity (`src/logid/ipc_defs.h`):**
- Bus name: `pizza.pixl.LogiOps` (`SERVICE_ROOT_NAME`).
- Root object path: `/pizza/pixl/logiops` (`server_root_node`).
- Server created in `src/logid/logid.cpp` via `ipcgull::make_server(...)`; devices/features register objects under the root node (`src/logid/DeviceManager.cpp`).

**Access policy:**
- Installed D-Bus system policy: `src/logid/logiops-dbus.conf.in` -> installed as `/usr/share/dbus-1/system.d/pizza.pixl.LogiOps.conf`. Policy denies receiving from the service by default and grants own/send/receive only to `user="root"`.

## Monitoring & Observability

**Error Tracking:**
- None (no Sentry/etc.). Errors surface as exceptions and log lines.

**Logs:**
- Custom logger in `src/logid/util/log.cpp` (`logPrintf`, levels DEBUG/INFO/WARN/ERROR; default INFO, set by `-v/--verbose`). Writes to stdout/stderr; `main()` disables stdout buffering so `journald` captures output line-by-line (`src/logid/logid.cpp`). Under systemd, logs flow to the journal for `logid.service`.

## CI/CD & Deployment

**Hosting:**
- Not applicable. Distributed as source / release tarball; users build and install locally (`make install` -> `/usr/bin/logid`, systemd unit, D-Bus policy).

**CI Pipeline (GitHub Actions, `.github/workflows/`):**
- `build-test.yml` - On push/PR to `main`, builds across a container matrix (`ubuntu:latest`, `ubuntu:20.04`, `fedora:latest`, `archlinux:base-devel`) with `-DCMAKE_CXX_FLAGS="-Werror"`. Checks out submodules recursively.
- `make-release.yml` - On `v*.*` tags, writes the tag to `version.txt`, strips `.git`, and publishes a source tarball as a GitHub release asset (`softprops/action-gh-release`).

## Environment Configuration

**Required env vars:**
- None. Configuration is file-based (`/etc/logid.cfg`) and CLI-flag-based; no environment variables are consumed by the daemon. No secrets are involved.

**Secrets location:**
- Not applicable (no credentials, tokens, or keys used or stored).

## Webhooks & Callbacks

**Incoming:**
- D-Bus method calls / property access on `pizza.pixl.LogiOps` at `/pizza/pixl/logiops` (root-only per policy). Used by external controllers/UIs to configure devices at runtime.

**Outgoing:**
- D-Bus signals emitted via ipcgull (e.g. device add/remove, status changes) on the same bus. No network webhooks.

---

*Integration audit: 2026-05-30*
