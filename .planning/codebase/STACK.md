# Technology Stack

**Analysis Date:** 2026-05-30

## Languages

**Primary:**
- C++20 - Entire daemon and IPC library. The C++20 requirement is driven by string-literal template parameters (noted in `src/logid/CMakeLists.txt` line 4). Standard enforced via `CMAKE_CXX_STANDARD 20` / `CMAKE_CXX_STANDARD_REQUIRED ON` in `CMakeLists.txt` and `src/logid/CMakeLists.txt`.

**Secondary:**
- C (kernel UAPI headers) - Low-level HID/input access uses Linux C headers directly (`<linux/hidraw.h>`, `<linux/input.h>`) in `src/logid/backend/raw/RawDevice.cpp`.
- CMake script - Build configuration across `CMakeLists.txt`, `src/logid/CMakeLists.txt`, `src/ipcgull/CMakeLists.txt`.

Note: The bundled `ipcgull` submodule (`src/ipcgull/`) targets C++17 (`set(CMAKE_CXX_STANDARD 17)` in `src/ipcgull/CMakeLists.txt`), while the top-level project compiles at C++20.

## Runtime

**Environment:**
- Linux only. Native ELF daemon (`logid`). No managed runtime.
- Runs as a `systemd` system service (`Type=simple`, `User=root`) per `src/logid/logid.service.in`.
- Must run as root in production; a user-bus development mode exists via the `-DUSE_USER_BUS=ON` CMake option (see `CMakeLists.txt` line 15, `src/logid/logid.cpp` line 154).

**Package Manager:**
- None at the language level (no vendored package manager). Dependencies are resolved from the host distribution via `pkg-config` / `pkg_check_modules`.
- Submodules managed by Git (`.gitmodules`): `src/ipcgull` from `https://github.com/PixlOne/ipcgull.git`. CMake auto-runs `git submodule update --init --recursive` (`CMakeLists.txt` lines 33-35).
- Lockfile: Not applicable (system libraries pinned by distro, not by this repo).

## Frameworks

**Core:**
- ipcgull (bundled submodule, `src/ipcgull/`) - D-Bus object/interface abstraction layer used to expose devices and features over IPC. Built as an `OBJECT` library, linked statically into `logid` (`BUILD_STATIC ON` by default in `src/ipcgull/CMakeLists.txt`).
- libconfig (libconfig++) - Configuration file parsing for `/etc/logid.cfg`. Used via `using namespace libconfig;` in `src/logid/Configuration.cpp`. Linked as `config++` (`src/logid/CMakeLists.txt` line 89).

**Testing:**
- ipcgull ships an optional standalone server test (`src/ipcgull/tests/server_test/`, gated behind `-DBUILD_TESTS`). The main `logid` daemon has no unit-test framework wired into its build.

**Build/Dev:**
- CMake >= 3.12 (top level and `logid`); ipcgull requires >= 3.10 - Build system.
- pkg-config (`PkgConfig` REQUIRED) - Locating system libraries (`libevdev`, `libconfig`, `libudev`, `systemd`, `gio-2.0`, `glib-2.0`).
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` - Generates `compile_commands.json` for tooling.

## Key Dependencies

**Critical (linked into `logid`, from `src/logid/CMakeLists.txt`):**
- libevdev - Creating and writing to the virtual `uinput` device (keyboard/relative axis emulation). Detected via `pkg_check_modules(PC_EVDEV libevdev REQUIRED)`; headers `<libevdev/libevdev.h>`, `<libevdev/libevdev-uinput.h>` used in `src/logid/InputDevice.cpp`.
- libudev - Device hotplug discovery and monitoring of `hidraw` devices. `pkg_check_modules(LIBUDEV libudev REQUIRED)`; used in `src/logid/backend/raw/DeviceMonitor.cpp`.
- libconfig++ (`config++`) - Config parsing (see above).
- ipcgull (static) - D-Bus IPC (see above).
- Threads (`CMAKE_THREAD_LIBS_INIT`, `find_package(Threads REQUIRED)`) - Multithreaded task scheduling (`src/logid/util/task.cpp`).

**Infrastructure (via ipcgull, `src/ipcgull/CMakeLists.txt`):**
- glib-2.0 - GLib core used by the GIO D-Bus backend.
- gio-2.0 - GDBus implementation backing ipcgull (`src/ipcgull/src/common_gdbus.cpp`, `src/ipcgull/src/server_gdbus.cpp`).

**Optional:**
- systemd - Detected via `pkg_check_modules(SYSTEMD "systemd")` (not REQUIRED). When present, the build installs `logid.service` into the systemd system unit dir resolved from `pkg-config --variable=systemdsystemunitdir systemd` (`src/logid/CMakeLists.txt` lines 94-111).

## Configuration

**Environment:**
- Daemon configured by a single libconfig file. Default path `/etc/logid.cfg` (`default_config` in `src/logid/logid.cpp`); override with `-c`/`--config`.
- CLI options parsed manually (no getopt) in `src/logid/logid.cpp`: `-v/--verbose [level]`, `-V/--version`, `-c/--config <path>`, `-h/--help`.
- Example config: `logid.example.cfg` (libconfig syntax: `devices: ( { ... } )` with `smartshift`, `hiresscroll`, `dpi`, `buttons` sub-blocks).
- A working `logid.cfg` is present at repo root (untracked).

**Build-time options (CMake):**
- `USE_USER_BUS` (default OFF) - Adds `-DUSE_USER_BUS`; switches D-Bus from system to session bus for non-root dev.
- `CMAKE_BUILD_TYPE` (e.g. `Release`) - Standard CMake.
- `CMAKE_INSTALL_PREFIX` hardcoded to `/usr` in top-level `CMakeLists.txt` (line 3).
- Version baked in via `-DLOGIOPS_VERSION="..."`, derived from `git describe --tags` or `version.txt` (`CMakeLists.txt` lines 17-53).
- Warning flags: `-Wall -Wextra` always; CI additionally builds with `-Werror`.

**Build config files:**
- `CMakeLists.txt`, `src/logid/CMakeLists.txt`, `src/ipcgull/CMakeLists.txt`
- `.editorconfig` (style), `.gitignore`, `.gitmodules`
- Templated install artifacts: `src/logid/logid.service.in`, `src/logid/logiops-dbus.conf.in`

## Platform Requirements

**Development:**
- C++20 compiler (GCC/Clang), CMake, pkg-config, and dev packages: `libevdev`, `libudev`, `libconfig++`, `glib2`/`gio`. Per `README.md`, distro install commands provided for Arch, Debian/Ubuntu, Fedora, Gentoo, Solus, openSUSE.
- CI build matrix (`.github/workflows/build-test.yml`): `ubuntu:latest`, `ubuntu:20.04`, `fedora:latest`, `archlinux:base-devel`, built with `-DCMAKE_CXX_FLAGS="-Werror"`.

**Production:**
- Linux with `hidraw` and `uinput` kernel support, a running D-Bus system bus, and (typically) systemd. Installs `logid` to `/usr/bin`, a systemd unit, and a D-Bus system policy file.

---

*Stack analysis: 2026-05-30*
