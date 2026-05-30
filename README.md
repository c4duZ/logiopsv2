# LogiOps

A Linux driver for Logitech mice and keyboards — with a graphical front-end.

This is our adapted version of [logiops](https://github.com/PixlOne/logiops), maintained by c4duZ at <https://github.com/c4duZ/logiopsv2>. The original project is a powerful but config-file-only daemon. **Our goal is to bring a polished, Logi Options+-style experience to Linux**: a Qt 6 + QML desktop app on top of the existing `logid` daemon, so you can configure your device visually instead of editing `/etc/logid.cfg` by hand.

Distributed under GPL-3.0. Currently only compatible with HID++ \>2.0 devices.

## What we're building

We keep the proven `logid` daemon as the source of truth and extend it only where a feature genuinely needs it. The work is staged so real value lands early (a working device list and visual config) before the higher-risk features.

| # | Feature | What it gives you |
|---|---------|-------------------|
| 1 | **Access & daemon hardening** | Use the daemon as a non-root user (via a `logiops` group), with privileged saves gated by polkit and the daemon sandboxed |
| 2 | **Device list** | A live GUI list of your connected Logitech devices with battery and connection status, updating on hotplug |
| 3 | **Core config UI** | Visual button remapping, DPI, scroll/SmartShift/thumbwheel, and manual profiles — saved without ever editing a text file |
| 4 | **Fine-grained gestures** | A guided gesture builder that fires *exactly once* or repeats predictably (no more "volume jumps by 2") |
| 5 | **Per-app profiles** | Profiles that auto-switch with the focused app (X11 + Wayland), plus profile import/export |
| 6 | **Action wheel** | A radial action menu at the cursor — flick toward a slice and release to fire it |
| 7 | **Smart actions / macros** | Multi-step actions (keystrokes, text, media, delays, launch app/URL) bound to one button |
| 8 | **Keyboard backlight** | Backlight/RGB control on supported keyboards |
| 9 | **Debian packaging** | A clean `.deb` that ships the policy, polkit action, and systemd unit |

Detailed planning lives in [`.planning/`](./.planning/) (roadmap, requirements, and per-phase plans).

## Configuration

Until the GUI lands, configuration is done through the daemon's config file. See [logid.example.cfg](./logid.example.cfg) for an example.

The default location is `/etc/logid.cfg`, but another can be specified with the `-c` flag. For protocol and option details, the upstream [logiops wiki](https://github.com/PixlOne/logiops/wiki/Configuration) is still a useful reference.

## Dependencies

This project requires a C++20 compiler, `cmake`, `libevdev`, `libudev`, `glib`, and `libconfig`.
Commands for popular distributions:

**Arch Linux:** `sudo pacman -S base-devel cmake libevdev libconfig systemd-libs glib2`

**Debian/Ubuntu:** `sudo apt install build-essential cmake pkg-config libevdev-dev libudev-dev libconfig++-dev libglib2.0-dev`

**Fedora:** `sudo dnf install cmake libevdev-devel systemd-devel libconfig-devel gcc-c++ glib2-devel`

**Gentoo Linux:** `sudo emerge dev-libs/libconfig dev-libs/libevdev dev-libs/glib dev-util/cmake virtual/libudev`

**Solus:** `sudo eopkg install cmake libevdev-devel libconfig-devel libgudev-devel glib2-devel`

**openSUSE:** `sudo zypper install cmake libevdev-devel systemd-devel libconfig-devel gcc-c++ libconfig++-devel libudev-devel glib2-devel`

## Building

To build this project, run:

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

To install, run `sudo make install` after building. You can set the daemon to start at boot by running `sudo systemctl enable logid` or `sudo systemctl enable --now logid` to enable and start the daemon.

## Development

The daemon normally runs as root, but for development you may find it convenient
to run as non-root on the user bus. Compile with the CMake flag
`-DUSE_USER_BUS=ON` to use the user bus.

## Compatible Devices

[For a list of tested devices, check TESTED.md](TESTED.md)

## Credits

This project is an adaptation of [logiops](https://github.com/PixlOne/logiops) by PixlOne and its contributors. All of their original work remains under GPL-3.0.

Logitech, Logi, and their logos are trademarks or registered trademarks of Logitech Europe S.A. and/or its affiliates in the United States and/or other countries. This software is an independent product that is not endorsed or created by Logitech.

Thanks to everyone who contributed to the upstream project, and in particular:

- [Clément Vuchener & contributors for the old HID++ library](https://github.com/cvuchener/hidpp)
- [The Solaar developers for providing information on HID++](https://github.com/pwr-Solaar/Solaar)
- [Nestor Lopez Casado for providing Logitech documentation on the HID++ protocol](http://drive.google.com/folderview?id=0BxbRzx7vEV7eWmgwazJ3NUFfQ28)
- Everyone listed in the upstream contributors page
