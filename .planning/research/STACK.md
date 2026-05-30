# Stack Research

**Domain:** Linux desktop GUI (Options+ clone) on top of the `logiops`/`logid` daemon, talking D-Bus, Debian-first
**Researched:** 2026-05-30
**Confidence:** HIGH for the recommended (Qt) path — versions verified against the actual target system's apt repos (Zorin OS 18 / Ubuntu 24.04 "noble"). MEDIUM for the Tauri runner-up (D-Bus binding versions are from training data; web verification tools were unavailable this session — flagged below).

> **Verification note:** WebSearch / WebFetch / Brave / Context7 were all unavailable in this research session. Concrete version numbers below were obtained by querying the **actual target machine** (`apt-cache policy`, `pkg-config`, `dpkg -l`) on Zorin OS 18.1 (Ubuntu 24.04 base, codename `noble`). Those are HIGH confidence for *this* environment. Anything marked "(training data — verify)" should be re-confirmed against crates.io / upstream before locking.

---

## TL;DR Recommendation

- **Primary: Qt 6 + QML (Qt Quick Controls 2)**, C++ backend, talking to `logid` via **Qt D-Bus** (`QtDBus`). This is the single best fit because the project *already extends a C++20 daemon*, Qt D-Bus is first-class and trivially maps onto the existing `pizza.pixl.LogiOps` interface, QML gives you the high-fidelity, animated, custom-skinned look-and-feel an Options+ clone demands, the **radial action-wheel overlay is natural in QML** (`QtQuick.Shapes` + a frameless/`layer-shell-qt` window), Debian packaging is clean (all deps are stock `noble` packages), and **it does not block a future Windows/macOS port** (Qt is the most portable of all candidates).
- **Runner-up: Tauri 2 (Rust core + web frontend)** via **`zbus`** for D-Bus. Best-in-class for *pixel-faithful* Options+ cloning (it's literally HTML/CSS), small binaries, good `.deb` story. Loses to Qt on: an extra language/runtime in a C++ shop, webview-fidelity drift across distros (webkit2gtk), and a clumsier always-on-top/transparent radial overlay.

Everything else (GTK4/libadwaita, Electron, Flutter-linux) is explicitly *not* recommended — reasons in "What NOT to Use."

---

## Options Compared

Scored against the five axes from the brief. ✅ strong / ⚠️ workable-with-effort / ❌ weak.

| Stack | D-Bus story | Options+ look-fidelity | Debian packaging | Radial overlay | Maintenance burden | Portability (future Win/macOS) |
|---|---|---|---|---|---|---|
| **Qt6 + QML** (primary) | ✅ `QtDBus` first-class, generates proxy from introspection | ✅ QML = fully custom skin, animations, shaders | ✅ all deps in `noble`; CMake already in repo | ✅ `QtQuick.Shapes` + frameless/`layer-shell-qt` | ✅ one language (C++) shared with daemon | ✅ best-in-class (Win/macOS/Linux) |
| **Tauri 2** (runner-up) | ✅ `zbus` (pure-Rust, async, excellent) | ✅ HTML/CSS — easiest pixel clone | ✅ `cargo-deb` / bundler emits `.deb` | ⚠️ transparent always-on-top window OK; layer-shell weaker | ⚠️ adds Rust + web toolchain to a C++ project | ✅ good (webview per-OS) |
| GTK4 / libadwaita | ✅ GIO `GDBus` (same lib the daemon already uses) | ❌ libadwaita = GNOME HIG, fights a custom Options+ skin | ✅ native Debian citizen | ⚠️ Cairo/Snapshot custom draw + `gtk-layer-shell` | ✅ low | ❌ effectively Linux-only |
| Electron | ✅ via node `dbus-next`/`dbus` | ✅ HTML/CSS | ⚠️ large `.deb`, bundles Chromium | ⚠️ heavy transparent window | ❌ Chromium update treadmill, ~150MB | ✅ portable but heavy |
| Flutter-linux | ⚠️ no first-class D-Bus; `dbus.dart` pkg over method channels | ✅ custom canvas UI | ⚠️ bundler immature, large | ⚠️ custom paint OK, overlay weak | ⚠️ Dart + niche Linux target | ⚠️ Linux target least-mature |

**Why Qt wins the tie-break over Tauri:** the daemon is C++20/CMake and *will be extended in the same repo* (gestures, action-wheel daemon support). Choosing Qt means **one language, one build system, one D-Bus mental model** across daemon + GUI. Tauri forces a second toolchain (Rust) and a third surface (web) into a C++ codebase. Qt also gives a *deterministic* native look across distros, whereas a Tauri/web UI renders through whatever `webkit2gtk` the host ships (2.52 on noble here) — fidelity drifts.

---

## Recommended Stack (Primary: Qt 6 + QML)

### Core Technologies

| Technology | Version (target: noble/Zorin 18) | Purpose | Why Recommended |
|---|---|---|---|
| **Qt 6 (Quick / QML)** | **6.4.2** in Ubuntu 24.04 repos (6.8 LTS / 6.9 from upstream Qt installer if newer needed) | GUI framework + declarative UI | Custom-skinnable (essential for an Options+ clone), animated, GPU-accelerated scene graph; same C++ language as the daemon |
| **QtDBus** (`libqt6dbus6`) | **6.4.2** (installed) | D-Bus client to `logid` | First-class. `qdbusxml2cpp` turns the daemon's introspection into a typed C++ proxy; `QDBusConnection::systemBus()` connects to `pizza.pixl.LogiOps` |
| **Qt Quick Controls 2** (`qml6-module-qtquick-controls`) | **6.4.2** | Buttons, sliders, lists, switches | Themeable controls; use a **custom style** (not Material/Universal defaults) to match Options+ |
| **C++20** | (project already) | GUI backend / daemon glue | Matches daemon's `CMAKE_CXX_STANDARD 20`; reuse `ipc_defs.h` constants |
| **CMake** | **3.28.3** (noble) / repo requires ≥3.12 | Build system | Already the project's build system — GUI becomes another CMake target, single `cmake --build` |

### Supporting Libraries

| Library | Version (noble) | Purpose | When to Use |
|---|---|---|---|
| **QtQuick.Shapes** (`qml6-module-qtquick-shapes`) | 6.4.2 | Vector arcs/paths for the **radial action wheel** | Drawing pie/donut segments, hover highlights, smooth animated wheel |
| **QtQuick.Window** (`qml6-module-qtquick-window`) | 6.4.2 | Frameless / transparent overlay window | The action-wheel pops as a borderless, translucent, always-on-top window |
| **layer-shell-qt** (`liblayershellqtinterface5` / `-dev`, plugin `layer-shell-qt`) | in noble repos | Proper Wayland overlay positioning for the wheel | **Wayland** sessions — gives true always-on-top/anchored overlay (frameless `Qt::WindowStaysOnTopHint` is the X11/fallback path) |
| **QtQuick.Layouts / Dialogs** (`qml6-module-qtquick-layouts`, `-dialogs`) | 6.4.2 | Responsive layout, native file dialogs | General UI assembly |
| **QtSvg** | 6.4.2 | Crisp device/icon artwork | Rendering mouse renders / Options+-style iconography |
| **(optional) QCoro** (`libqcoro6dbus`, `libqcoro6qml`) | in noble repos | C++20 coroutines over QtDBus/QML | Cleaner `co_await` on async D-Bus calls instead of signal/slot callbacks — nice-to-have |

### Development Tools

| Tool | Purpose | Notes |
|---|---|---|
| `qdbusxml2cpp` | Generate typed C++ proxy from the daemon's D-Bus introspection XML | Ships with `qt6-tools-dev` / `qt6-base-dev-tools`. Capture XML at runtime with `busctl introspect pizza.pixl.LogiOps /pizza/pixl/logiops` |
| `busctl` / `d-feet` / `gdbus` | Inspect the live `logid` interface while developing | `busctl --system tree pizza.pixl.LogiOps` to enumerate the object tree the daemon exports |
| Qt Creator / `qmllint` / `qmlformat` | QML authoring + linting | `qmllint` ships in `qt6-declarative-dev-tools` |
| CMake + `debhelper` (13.x) + `dpkg-buildpackage` | Debian packaging | See Debian section |

### D-Bus binding — concrete

- **Library: `QtDBus` (`libqt6dbus6t64` 6.4.2, dev: `qt6-base-dev`).** Verified installed on target.
- Connect: `QDBusConnection::systemBus()` (the daemon defaults to the **system bus** unless built `-DUSE_USER_BUS=ON`).
- Generate proxy: `qdbusxml2cpp -p logidproxy logid_interface.xml` → typed wrapper for the `pizza.pixl.LogiOps` interfaces at `/pizza/pixl/logiops` and the per-device/feature child objects.
- Subscribe to the daemon's add/remove/status **signals** via `QDBusConnection::connect(...)` to drive live device-list updates.

---

## Installation

This is a system-library / native stack, not npm. Debian/Ubuntu-noble dev packages:

```bash
# Core Qt6 + QML + D-Bus (target: Ubuntu 24.04 / Zorin 18)
sudo apt install \
  qt6-base-dev qt6-base-dev-tools qt6-tools-dev \
  qt6-declarative-dev qt6-declarative-dev-tools \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-shapes qml6-module-qtquick-window \
  qml6-module-qtquick-dialogs \
  libqt6dbus6 libqt6svg6-dev

# Wayland overlay (radial action wheel) — proper layered surface
sudo apt install qml6-module-org-kde-layershell  # provides layer-shell-qt plugin
sudo apt install liblayershellqtinterface-dev    # if linking from C++

# Optional: C++20 coroutines over D-Bus/QML
sudo apt install libqcoro6-dev  # qcoro6 dbus/qml/quick

# Debian packaging toolchain
sudo apt install debhelper dpkg-dev cmake build-essential pkg-config
```

> If a newer Qt (6.8 LTS / 6.9) is wanted for the latest QML niceties, install via the official Qt online installer or `aqtinstall`; **6.4.2 is sufficient** for everything this project needs (Shapes, Controls 2, DBus all present).

### Runner-up install (Tauri 2)

```bash
# Rust toolchain
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
# Tauri 2 Linux deps on noble
sudo apt install libwebkit2gtk-4.1-dev libgtk-3-dev librsvg2-dev \
  build-essential curl wget file libssl-dev
cargo install create-tauri-app cargo-deb
# Cargo deps: tauri = "2", zbus = "5"   (zbus version: training data — verify on crates.io)
```

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|---|---|---|
| Qt6+QML | **Tauri 2 + zbus** | If the team is Rust/web-native, wants the *easiest* pixel-faithful clone (HTML/CSS), and accepts a second toolchain. Strong, viable second choice. |
| Qt6+QML | **GTK4 + Cairo custom drawing (no libadwaita)** | If you commit to Linux-only forever AND want to reuse the daemon's existing GLib/GIO `GDBus` stack directly. Lose libadwaita to avoid GNOME-HIG styling fights. |
| QtDBus | **`sdbus-c++`** (`libsdbus-c++-dev`, in noble) | If you wrote a *non-Qt* C++ GUI and needed a modern C++ D-Bus binding. Not needed inside Qt (QtDBus covers it); listed because it's the best standalone C++ D-Bus lib. |
| Qt custom style | **Qt Quick Controls Material/Universal** | Quick prototyping before the bespoke Options+ skin is built. Replace with a custom style for the real clone. |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|---|---|---|
| **GTK4 + libadwaita** | libadwaita hard-codes the GNOME HIG (rounded headerbars, specific spacing, no real theming). A *faithful Options+ clone* fights it the whole way, and it's **effectively Linux-only** — directly conflicts with the "don't block Win/macOS" constraint. | Qt6+QML (themeable + portable) |
| **Electron** | ~150 MB bundled Chromium per app, fat `.deb`, perpetual security-update treadmill, heavy RAM for a settings utility. Overkill for a device config panel. | Tauri 2 (same web UI, native webview, tiny binary) |
| **Flutter-linux** | Linux is Flutter's least-mature target; **no first-class D-Bus** (you bolt on the `dbus.dart` package), packaging/`.deb` tooling is immature, and it injects Dart into a C++ project for no payoff here. | Qt6+QML or Tauri 2 |
| **`ipcgull` as the GUI's D-Bus client** | It's the daemon's *server-side* IPC helper (GDBus wrapper bundled in `src/ipcgull/`). The GUI is a plain D-Bus *client*; use the toolkit's native binding, not ipcgull. | QtDBus (Qt path) / zbus (Tauri path) |
| **Raw `libdbus` (dbus-1) in the GUI** | Low-level, manual marshalling, error-prone. No reason to hand-roll it. | QtDBus / GDBus / zbus |
| **System-bus + root-only policy left as-is** | The shipped policy (`pizza.pixl.LogiOps.conf`) grants own/send/receive to `root` only — a normal-user GUI **cannot talk to the daemon** out of the box. This is a hard blocker, not a stack choice. | Add a D-Bus policy entry allowing a `logid`/`plugdev` group (or polkit-gated methods); see Pitfalls/Architecture research. |

---

## Stack Patterns by Variant

**If the priority is single-toolchain cohesion with the C++ daemon (the actual situation):**
- Use **Qt6 + QML + QtDBus**.
- Because daemon + GUI share C++20/CMake; one build, one D-Bus model, reuse of `ipc_defs.h`. This is the recommended default.

**If the team is web/Rust-first and wants the most literal CSS-level Options+ clone:**
- Use **Tauri 2 + `zbus`** (zbus ~5.x — training data, verify).
- Because HTML/CSS is the easiest path to pixel parity and the `.deb` bundler is good; accept the second toolchain.

**If the project decides Linux-only is acceptable forever (it currently is NOT — Win/macOS must stay possible):**
- Then **GTK4 + GDBus** (reuse the daemon's existing GLib stack) becomes reasonable — but skip libadwaita to keep styling control.

**Radial action-wheel overlay (any Qt variant):**
- Frameless, `WA_TranslucentBackground`/`color: "transparent"` `Window`, `Qt::WindowStaysOnTopHint`.
- Draw segments with **`QtQuick.Shapes`** (`PathArc`), animate with QML `Behavior`/`NumberAnimation`.
- On **Wayland**, use **`layer-shell-qt`** for correct always-on-top anchored positioning (plain stay-on-top hints are unreliable under Wayland compositors). On X11 the frameless+top-hint window suffices.

---

## Version Compatibility

| Package A | Compatible With | Notes |
|---|---|---|
| Qt 6.4.2 (noble) | QtDBus 6.4.2, QtQuick.Shapes 6.4.2, Controls 2 6.4.2 | All from the same Ubuntu 24.04 release — coherent set, verified present on target |
| `logid` D-Bus (system bus, `G_BUS_TYPE_SYSTEM`) | `QDBusConnection::systemBus()` | Match the daemon's bus; if daemon built `-DUSE_USER_BUS=ON`, use `sessionBus()` instead |
| GLib/GIO 2.80 (daemon side) | QtDBus (GUI side) | Both speak the wire D-Bus protocol — no GLib/Qt ABI coupling; they only share the bus |
| layer-shell-qt | Qt 6.x + Wayland compositor (wlroots/KWin/Mutter) | Overlay only needed on Wayland; X11 path uses window flags |
| `libsdbus-c++` 2.x (noble) | standalone C++ (non-Qt) only | Don't mix with QtDBus in the same process |
| Tauri 2 + webkit2gtk-4.1 2.52 (noble) | zbus 5.x (verify) | Tauri 2 requires `webkit2gtk-4.1` (present on noble) — Tauri 1's `-4.0` is EOL |

---

## Debian Packaging Approach

- **Primary (Qt path): `debhelper` (compat 13) + CMake buildsystem + `dpkg-buildpackage`.** The repo is already CMake; add a `debian/` dir (`control`, `rules` using `dh $@ --buildsystem=cmake`, `changelog`, `install`). Runtime deps resolve to stock `noble` packages (`libqt6quick6`, `libqt6dbus6`, `qml6-module-*`) via `${shlibs:Depends}` + explicit `Depends` on the QML modules (which shlibs can't auto-detect).
- Ship **one source package** producing two binaries if desired (`logid` daemon + `logid-gui`), or fold the GUI into the existing package. Daemon already installs the systemd unit and D-Bus policy; the GUI package should ship the **amended D-Bus policy** (or a polkit rule) that lets a normal user reach the daemon.
- **Runner-up (Tauri): `cargo-deb`** or Tauri's built-in bundler (`tauri build` → `.deb`). Simpler one-shot `.deb`, but produces a less "native Debian-citizen" package (vendored Rust deps, manual dependency declarations for webkit2gtk).
- Both are Debian-clean; the Qt/CMake/`debhelper` route is the more idiomatic, lintian-friendly path and reuses the existing build.

---

## Sources

- **Target system probe** (HIGH) — `apt-cache policy`, `pkg-config --modversion`, `dpkg -l` on Zorin OS 18.1 / Ubuntu 24.04 `noble`: confirmed Qt 6.4.2 (core/dbus/qml/quick/wayland), GLib/GIO 2.80, `qml6-module-qtquick-shapes/-window/-controls`, `libsdbus-c++-dev` 2.x, `liblayershellqtinterface`/`layer-shell-qt`, `libgtk-4-dev` 4.14, `libadwaita-1-dev` 1.5, `libwebkit2gtk-4.1-dev` 2.52, `debhelper` 13.14, `cmake` 3.28.
- **Repo codebase maps** (HIGH) — `.planning/codebase/INTEGRATIONS.md` (bus name `pizza.pixl.LogiOps`, system-bus default, root-only policy, `ipcgull`/GDBus backend), `STACK.md` (C++20/CMake, GLib/GIO deps), `PROJECT.md` (Debian-first, don't-block-Win/macOS, action-wheel requirement).
- **Qt DBus / QML / Tauri / zbus capabilities** (MEDIUM, from model knowledge — web/Context7 verification unavailable this session): QtDBus `systemBus()` + `qdbusxml2cpp`; QtQuick.Shapes for vector arcs; Tauri 2 requiring webkit2gtk-4.1 and pairing with `zbus` for D-Bus. **Verify `zbus`/`tauri` exact crate versions on crates.io before locking the runner-up.**

---
*Stack research for: Linux Options+-clone GUI on the logiops/logid D-Bus daemon (Debian-first)*
*Researched: 2026-05-30*
