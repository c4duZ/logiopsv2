# Pitfalls Research

**Domain:** Linux desktop GUI (Options+ clone) configuring Logitech HID++ devices on top of the `logiops` root daemon via D-Bus; Debian-first; daemon C++ extended when needed.
**Researched:** 2026-05-30
**Confidence:** HIGH for codebase-grounded items (D-Bus policy, system bus, uinput, root daemon, untrusted HID), MEDIUM for Wayland/polkit/packaging ecosystem claims (verified against training + first-party knowledge; WebSearch was unavailable this run, so a few forward-looking compositor-feature claims are flagged for re-validation).

> **Verification note:** WebSearch/Context7 were not reachable during this run. Codebase-anchored pitfalls are cited to exact files in `.planning/codebase/` (CONCERNS.md, INTEGRATIONS.md) and are HIGH confidence. Wayland protocol availability and Debian/polkit specifics should be re-confirmed against current docs before the relevant phase starts (flagged inline).

## Critical Pitfalls

### Pitfall 1: Running the GUI as root to "make D-Bus work"

**What goes wrong:**
The shipped policy (`src/logid/logiops-dbus.conf.in` → `/usr/share/dbus-1/system.d/pizza.pixl.LogiOps.conf`) grants `own/send/receive` **only to `user="root"`** and denies receiving from the service by default (INTEGRATIONS.md:65-66). When a normal-user GUI gets `org.freedesktop.DBus.Error.AccessDenied`, the tempting fix is to launch the whole GUI with `pkexec`/`sudo`. That puts a large GUI toolkit (browser engine if Tauri, full Qt/GTK stack), its IPC, its config files, and the user's whole rendering session under root — a massive attack surface, broken theming, `$HOME`/XDG path corruption (root writes user-owned dotfiles), and a Wayland session that often simply refuses to connect for root.

**Why it happens:**
The path of least resistance. The daemon is root-only by policy, so "just run the client as root too" appears to be the one-line fix.

**How to avoid:**
Never elevate the GUI. Pick ONE supported access path and design for it from day one:
- **Recommended:** widen the D-Bus policy to allow a dedicated group (e.g. `at_console` or a `logiops` group), and gate *mutating* methods (config writes, device resets) behind **polkit** actions so reads are free but writes prompt/authorize. The daemon already runs on the system bus, so polkit integration is the idiomatic answer.
- Alternative: `-DUSE_USER_BUS=ON` session-bus build (INTEGRATIONS.md:58) — simpler permissions but changes the deployment model (per-session daemon instead of one system daemon) and conflicts with a root daemon that needs `hidraw`/`uinput`. Treat as a fallback, not the default.

**Warning signs:**
GUI works only under `sudo`; theming/icons break; files appear in `/root/.config` or get `root:root` ownership in the user's home; Wayland GUI fails to start as root.

**Phase to address:** Foundational "GUI ↔ daemon access" phase — must be the *first* integration phase, before any UI feature is built on top of it.

---

### Pitfall 2: No D-Bus policy / polkit authorization shipped — wrong on both extremes

**What goes wrong:**
Two opposite failures. (a) Leave the root-only policy as-is and the GUI can't talk to the daemon at all. (b) Over-correct by opening the system-bus service to `<allow send_destination="..."/>` for everyone *including config writes and `Configuration::save()`* — now any local process (or a compromised browser tab in a Tauri webview) can rewrite `/etc/logid.cfg` as root and trigger arbitrary remaps/resets. CONCERNS.md flags this exact surface as **High**: "Root-triggered config write over D-Bus … No D-Bus policy file ships in-tree to restrict who may invoke it" (CONCERNS.md:36-38, item #4).

**Why it happens:**
Developers test on their own single-user box where "allow everyone" feels harmless, and D-Bus policy XML is fiddly so people copy a permissive snippet.

**How to avoid:**
Split read vs write. Allow `send`/`receive` for property reads and signals to the user group, but route every state-changing method (`Configuration::save`, device reset, profile write, action-wheel install) through a polkit action with its own policy `.policy` file. Default `auth_admin_keep` for writes, `yes` for reads. Write the policy and polkit actions as code artifacts in the daemon repo, not as install-time documentation.

**Warning signs:**
A `<allow send_destination>` line with no matching `<deny>` for sensitive interfaces; the GUI never prompts for authorization even when changing system config; `busctl --system` shows the service callable by any uid.

**Phase to address:** Same foundational access phase as Pitfall 1; the polkit split is a success-criterion of that phase, not a later hardening pass.

---

### Pitfall 3: Assuming X11 input/overlay tricks work on Wayland (the action wheel trap)

**What goes wrong:**
The "action wheel" (always-on-top radial overlay triggered by a mouse gesture, anywhere on screen) and "global gestures" are the two features most likely to be designed against an X11 mental model and then silently break on Wayland — which is the default on modern Debian/GNOME. On Wayland a normal client **cannot**: grab global input, position its own window at absolute screen coordinates, force always-on-top, or read the pointer when unfocused. There is no `XGrabPointer`, no override-redirect window, no global hotkey API in core Wayland. An overlay built as an ordinary toplevel will appear in the wrong place, lose focus, or not render over fullscreen apps.

**Why it happens:**
Most desktop-overlay tutorials and a lot of training-era knowledge assume X11. The feature "works on my machine" because the dev is testing under XWayland or an X11 session, then fails for GNOME-Wayland users.

**How to avoid:**
- **Render the overlay** via the `wlr-layer-shell` protocol (`zwlr_layer_shell_v1`) for an always-on-top, anchored surface — but note **GNOME's Mutter does not implement layer-shell** (KDE/wlroots compositors do). On GNOME the realistic path is a GNOME Shell extension or accepting reduced behavior. Re-verify Mutter layer-shell status before committing (flagged).
- **Trigger it from the daemon, not a global hotkey in the GUI.** The action wheel should be *invoked by the daemon's existing gesture engine* (which sees raw HID++ events directly, bypassing the compositor entirely), which then asks the GUI to show the overlay. This sidesteps Wayland's "no global input grab" rule because gesture detection never touches the compositor.
- **Detect the session** (`XDG_SESSION_TYPE`, `WAYLAND_DISPLAY`) and degrade gracefully; document X11-only behaviors as such.

**Warning signs:**
Overlay code calls absolute-position APIs; relies on a global hotkey library; works under `XDG_SESSION_TYPE=x11` but the radial menu appears top-left / steals focus / vanishes under Wayland; testing only ever done on KDE or only on GNOME.

**Phase to address:** A dedicated **action-wheel / overlay** phase, explicitly preceded by a Wayland-vs-X11 spike. This phase MUST flag deeper research (compositor matrix: GNOME-Mutter vs KDE-KWin vs wlroots).

---

### Pitfall 4: Synthesizing input — fighting the daemon's uinput path instead of using it

**What goes wrong:**
The team adds a *second* input-injection path in the GUI (e.g. `ydotool`, `xdotool`, RDP/`libei`, or a fresh uinput device) to make gestures/remaps "send keys," not realizing the daemon **already injects input via libevdev+uinput** ("LogiOps Virtual Input", INTEGRATIONS.md:36). Two injectors mean duplicated permissions problems, double-fired events, and on Wayland the GUI's injection silently no-ops (Wayland blocks synthetic input from unprivileged clients; `xdotool` does nothing). Worse, the daemon's existing uinput code is fragile: it **destroys and recreates the uinput device for every new event code** (CONCERNS.md:61, item #7), so piling more event types on it drops events.

**Why it happens:**
GUI developers think of "send a keypress" as a GUI-side action and reach for familiar X11 automation tools.

**How to avoid:**
All input synthesis stays in the **daemon** through its existing uinput device. The GUI only *configures* what the daemon injects; it never injects directly. If the action wheel needs to "type" or run actions, those resolve to daemon actions over D-Bus, not GUI-side automation. When extending the daemon for new actions, **fix the uinput churn first** (pre-enable the needed event codes once at device creation) rather than inheriting the recreate-per-code bug.

**Warning signs:**
`ydotool`/`xdotool`/`libei` appears in GUI dependencies; key synthesis works on X11 but not Wayland; duplicate virtual input devices in `/proc/bus/input/devices`; events fire twice.

**Phase to address:** Daemon-extension phase for gestures/actions; the "single injection path" rule is an architecture constraint set in the foundational phase.

---

### Pitfall 5: Two sources of truth — GUI state vs live daemon vs `/etc/logid.cfg`

**What goes wrong:**
There are three representations of configuration: (a) the GUI's in-memory model, (b) the daemon's **live runtime state** over D-Bus, and (c) the on-disk **`/etc/logid.cfg`** (root-owned libconfig file; `Configuration::save()` writes it, CONCERNS.md:36-38). Teams commonly let the GUI write the file directly or cache config and drift out of sync: the user hand-edits `logid.cfg`, or another tool changes a setting, and the GUI shows stale values; or the GUI saves and clobbers comments/manual edits/unknown keys in the file. Because the file is `root:root`, a user-owned GUI **cannot write it directly anyway** without elevation — so any "GUI writes the file" design is broken from the start.

**Why it happens:**
"Just edit the config file" feels simpler than a live IPC model, and libconfig round-tripping looks easy until you hit comments, formatting, and concurrent edits.

**How to avoid:**
Establish **the daemon as the single source of truth**. GUI reads live state over D-Bus and writes *only* via daemon methods; persistence happens through the daemon's `Configuration::save()` (gated by polkit, Pitfall 2), never by the GUI touching `/etc/logid.cfg`. Subscribe to daemon D-Bus signals (device add/remove, status — INTEGRATIONS.md:99) to keep the UI live. Treat external file edits as a reload event (daemon re-reads; GUI refreshes from daemon). Decide and document round-trip semantics: does `save()` preserve user comments/unknown keys? (libconfig serialization typically does **not** preserve comments — verify and warn users, or the GUI will silently delete their annotations.)

**Warning signs:**
GUI has file-path logic for `/etc/logid.cfg`; settings revert after a daemon restart; user comments disappear after a GUI save; two clients show different values; "permission denied" writing the config from the GUI.

**Phase to address:** Foundational state/sync architecture phase (defines source-of-truth + signal subscription), reinforced in every feature phase that writes config.

---

### Pitfall 6: Device hotplug, multi-device, and receiver-pairing edge cases handled optimistically

**What goes wrong:**
The GUI assumes a stable single device. Real usage: Unifying/Bolt **receivers** present a receiver node plus N paired child devices that connect/disconnect independently; devices sleep and drop off Bluetooth; a mouse can be paired but currently offline; hotplug fires mid-configuration. The daemon discovers devices via udev `hidraw` add/remove (INTEGRATIONS.md:38-39) and handles receivers via `hidpp10::ReceiverMonitor`, but that pairing code carries **numerous open TODOs** (CONCERNS.md:66, item #11) and `logid::Device` **hardcodes HID++ 2.0** (PROJECT.md:67 — `src/loglog/Device.h:55` TODO), so older/1.0-only devices and pairing flows are under-served. A GUI that caches a device list, keys UI state by transient device index, or blocks on an offline device will show ghosts, crash on removal, or hang.

**Why it happens:**
Developers test with one wired or one always-on device and never exercise sleep/wake, receiver re-pair, or unplug-during-edit.

**How to avoid:**
Drive the device list entirely from daemon D-Bus add/remove signals; never cache a static list. Key UI state by a stable device identifier (serial/HID++ device path), not by index/order. Represent "paired but offline" as a first-class state. Make every device operation tolerant of mid-flight disappearance (the device object can vanish between method call and reply). Budget daemon work for the HID++ 1.0 / receiver-pairing TODOs if multi-device richness is in scope. Test the receiver re-pair and sleep/wake matrix explicitly.

**Warning signs:**
Stale entries after unplug; crash/exception when a device is removed during configuration; offline devices missing from the list entirely; UI keyed by array index; only ever tested with one device.

**Phase to address:** Device-list / multi-device phase; flag for deeper research because of the receiver-pairing TODOs and HID++ 2.0 hardcode.

---

### Pitfall 7: Extending the root daemon while inheriting its untrusted-HID attack surface

**What goes wrong:**
New daemon code (gesture engine improvements, action-wheel hooks) adds more parsing of attacker-controlled HID reports inside a process that **runs as root with no sandboxing, no privilege drop, no capability bounding** (CONCERNS.md:24-26, item #2). The existing code already does **unbounded indexing of short/malicious HID reports** before length validation (CONCERNS.md:28-34, item #3) and has a **format-string logging** hole (`logPrintf(WARN, e.what())`, CONCERNS.md:40-42). Adding features without length-checking new report paths means a malicious/malfunctioning USB or Bluetooth device gets an out-of-bounds read (or worse) as root.

**Why it happens:**
Feature pressure ("ship the action wheel") plus an existing codebase that already indexes reports unsafely, so new code copies the unsafe pattern.

**How to avoid:**
For any daemon extension: (1) length-check every HID++ report field before indexing — treat hardware input as untrusted (CONCERNS.md explicitly recommends this); (2) never pass device/exception strings as printf format strings; (3) add systemd hardening to `logid.service` now (`NoNewPrivileges=`, `ProtectSystem=strict`, `RestrictAddressFamilies=AF_UNIX`, capability bounding to just what `hidraw`/`uinput` need); (4) fix the **sliced-exception** bug (`throw error;` → bare `throw;`, CONCERNS.md:46-48) so error handling around new code actually works. The codebase has **effectively zero automated tests** (CONCERNS.md:69-71), so add tests for new parsing paths.

**Warning signs:**
New code indexes `report[...]` without a prior size check; new log calls pass variable strings as the format arg; `logid.service` still has bare `User=root` and no hardening directives; new daemon features ship with no tests.

**Phase to address:** A daemon-hardening phase *before or alongside* the first daemon extension; security checks are success-criteria for every daemon-extension phase thereafter.

---

### Pitfall 8: Debian packaging that assumes the daemon is already installed (or co-installs it wrong)

**What goes wrong:**
The GUI `.deb` is built in isolation and on the maintainer's machine the daemon was installed from source — so the package "works" but for end users the GUI launches against a missing/misconfigured daemon, a missing D-Bus policy file, or a service that isn't enabled. Specific Debian traps: D-Bus policy must land in `/usr/share/dbus-1/system.d/` and the bus must be told to reload; polkit `.policy` files must be installed and registered; the systemd unit needs `dh_installsystemd` (enable + start on install, stop on remove) — done by hand it breaks on upgrade/purge; if the GUI and daemon are one repo but ship as separate packages, the dependency/`Depends:` relationship and version lockstep must be declared or a GUI built for new daemon D-Bus methods talks to an old daemon and fails cryptically.

**Why it happens:**
Source-install dev workflow hides packaging gaps; D-Bus/polkit/systemd integration is exactly the part that never runs during `make install` testing.

**How to avoid:**
Package the daemon and GUI together (or with a strict `Depends:` + versioned dependency so the GUI requires a daemon new enough for its D-Bus surface). Use `debhelper`/`dh` with `dh_installsystemd`, install the D-Bus system policy and polkit actions via the package (not docs), and test on a clean Debian container/VM — install, upgrade, and **purge** — not on the dev box. Verify the D-Bus daemon picks up the new policy and the service is enabled after install. CI already builds on `ubuntu:latest`/`ubuntu:20.04` (INTEGRATIONS.md:82) — extend it to actually install the `.deb` and smoke-test the D-Bus handshake.

**Warning signs:**
Only ever installed via `make install`; no `debian/` rules for systemd/dbus/polkit; GUI package has no `Depends:` on the daemon; works on dev box, "command not found"/`AccessDenied` on a fresh VM; D-Bus method-not-found errors after a partial upgrade.

**Phase to address:** Packaging phase (late), but the GUI↔daemon **version-compatibility contract** must be decided in the foundational access phase so packaging can enforce it.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| GUI writes `/etc/logid.cfg` directly | No D-Bus write plumbing | Breaks (root-owned file), clobbers comments, drifts from live state | **Never** — file is root-owned; must go through daemon |
| Run GUI under `sudo`/`pkexec` to reach the daemon | One-line fix to AccessDenied | Root GUI attack surface, broken theming/XDG, Wayland refuses root | **Never** — use polkit/group policy instead |
| Open the D-Bus service to all local users | GUI "just works" | Any local process rewrites root config / resets devices | **Never** for write methods; OK for read-only props |
| Build/test the action wheel only on X11 (XWayland) | Overlay works immediately | Silently broken for GNOME-Wayland users (the default) | Only as an explicit X11-first MVP with Wayland flagged |
| Cache a static device list in the GUI | Simpler UI code | Ghost devices, crashes on hotplug | Only with hotplug signals wired before merge |
| Add a second input injector (ydotool/xdotool) in the GUI | Quick "send keys" | Double events, no-op on Wayland, perms mess | **Never** — reuse daemon uinput path |
| Skip systemd hardening on the extended daemon | Ship features faster | Root + untrusted HID = high-severity surface | Only if hardening is a same-milestone follow-up, not "later" |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| D-Bus (system bus) | Assuming session bus; running client as root | Talk to system bus; widen policy to a group; polkit-gate writes |
| polkit | Treating it as optional / docs-only | Ship `.policy` actions in-package; `auth_admin_keep` for write actions |
| uinput (input synthesis) | Injecting from the GUI | All injection in daemon's existing uinput device; fix recreate-per-code churn |
| udev hotplug | Polling / caching device list | Subscribe to daemon add/remove D-Bus signals |
| Unifying/Bolt receiver | Modeling receiver as one device | Model receiver + N child devices, each with online/offline state |
| libconfig (`/etc/logid.cfg`) | Round-tripping and losing comments | Daemon owns writes; document/verify comment-preservation behavior |
| Wayland compositor | One overlay impl for "Wayland" | Per-compositor matrix; layer-shell on wlroots/KDE, extension on GNOME |
| ipcgull (vendored, incomplete) | Trusting it as a stable lib | It's in-tree with TODOs (CONCERNS.md:62) — budget for fixes when adding D-Bus surface |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| uinput device recreated per new event code | Dropped/missed synthetic events under rapid actions | Pre-enable all needed event codes at device creation | Action wheel / multi-action gestures firing fast |
| Polling daemon for device/battery state | UI lag, busy daemon, battery churn | Use D-Bus property-changed/status signals | As soon as multiple devices are connected |
| Synchronous D-Bus calls on the GUI main thread | UI freezes when a device is slow/offline/asleep | Async D-Bus calls; tolerate timeouts | First time a BT device is asleep mid-call |
| IOMonitor lock-yield threading under concurrent events | Sporadic hangs/missed events with many devices | Don't pile new event sources on it blindly (CONCERNS.md:60) | Multiple receivers + hotplug storms |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Indexing HID++ reports before length-checking | OOB read as **root** from a malicious USB/BT device | Length-check every field before access (CONCERNS.md:28-34) |
| Format-string log of device/exception strings | Format-string vuln in a root process | Use `"%s"`-style logging, never variable-as-format (CONCERNS.md:40-42) |
| Opening D-Bus write methods to all local UIDs | Any local process rewrites root config / resets devices | polkit-gate all state-changing methods |
| Running the GUI (esp. Tauri webview) as root | Browser-engine RCE → root | Never elevate the GUI; keep it unprivileged |
| No systemd sandboxing on root daemon | A single parser bug = full root compromise | `NoNewPrivileges`, `ProtectSystem=strict`, capability bounding (CONCERNS.md:24-26) |
| `assert`-based read-length checks compiled out in release | Validation silently disappears under `NDEBUG` | Real runtime checks, not `assert` (CONCERNS.md:50-52) |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Action wheel appears top-left / steals focus on Wayland | Core feature looks broken on the default desktop | Compositor-aware overlay + daemon-triggered invocation; degrade gracefully |
| GUI silently fails when daemon absent/old | "App does nothing," no explanation | Detect daemon presence + version; show actionable error/install hint |
| Offline (asleep/unpaired) devices hidden | User thinks device "disappeared" | Show paired-but-offline state explicitly |
| GUI save deletes user's hand-written config comments | Power users lose annotations | Warn, or preserve comments; document daemon-owns-config model |
| Gesture granularity still coarse (the bug being fixed) | "1 gesture = 1 desktop, volume +2" persists | Expose magnitude/repetition; verify daemon supports fine steps before UI promises it |

## "Looks Done But Isn't" Checklist

- [ ] **GUI↔daemon access:** Often missing polkit write-gating — verify a non-root user can read but is prompted/authorized for config writes, and that the GUI never needs `sudo`.
- [ ] **Action wheel:** Often missing Wayland support — verify behavior on **GNOME-Wayland** (default Debian), not just X11/KDE; verify trigger comes from the daemon gesture path, not a global hotkey.
- [ ] **Input synthesis:** Often missing single-path discipline — verify only the daemon's uinput device exists; no GUI-side ydotool/xdotool; works on Wayland.
- [ ] **State sync:** Often missing live signals — verify external `logid.cfg` edits and CLI changes reflect in the GUI without restart; verify GUI never writes the file directly.
- [ ] **Hotplug/multi-device:** Often missing removal handling — verify unplug-during-edit, sleep/wake, receiver re-pair, and paired-but-offline all behave.
- [ ] **Daemon extension:** Often missing input validation — verify every new HID++ report path length-checks before indexing; verify systemd hardening present.
- [ ] **Packaging:** Often missing fresh-system test — verify install/upgrade/**purge** on a clean Debian VM installs D-Bus policy + polkit + enables the service; verify GUI `Depends:` pins a compatible daemon version.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Shipped permissive D-Bus policy | MEDIUM | Tighten policy, add polkit actions, release a security update; audit for abuse |
| GUI built around editing `logid.cfg` directly | HIGH | Re-architect to daemon-as-source-of-truth; rework all write paths through D-Bus |
| Action wheel built X11-only | HIGH | Add compositor matrix + layer-shell/extension paths; may need overlay rewrite |
| Second input injector added | MEDIUM | Remove it; route actions through daemon uinput |
| Root GUI shipped | HIGH | De-elevate; rebuild permission model around polkit; fix corrupted user dotfile perms |
| Unvalidated HID parsing in extension | MEDIUM | Add length checks + tests; add systemd hardening as defense-in-depth |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| GUI as root (P1) | Foundational access phase (first) | GUI runs unprivileged; no `sudo` needed |
| No/over-broad D-Bus+polkit policy (P2) | Foundational access phase | Reads free, writes polkit-authorized; service not callable by arbitrary uid |
| Wayland overlay/input trap (P3) | Action-wheel phase + Wayland spike | Overlay works on GNOME-Wayland & KDE; flag for deeper research |
| Duplicate input injection (P4) | Foundational architecture + daemon-extension phase | Single uinput device; no GUI-side automation |
| Multiple sources of truth (P5) | State/sync architecture phase | Live signals update UI; GUI never writes `logid.cfg` |
| Hotplug/multi-device/receiver (P6) | Device-list phase (flag for research) | Unplug/sleep/re-pair/offline all handled |
| Untrusted-HID root surface (P7) | Daemon-hardening phase (before first extension) | New parsing length-checked; systemd hardening present; tests added |
| Debian packaging gaps (P8) | Packaging phase (contract decided early) | Clean-VM install/upgrade/purge passes; versioned `Depends:` |

## Sources

- `.planning/codebase/CONCERNS.md` — root-no-hardening (#2), unbounded HID indexing (#3), root D-Bus config write w/ no policy (#4), format-string log (#5), IOMonitor threading (#6), uinput churn (#7), release-mode assert (#8), sliced exception (#1), HID++ 1.0 pairing TODOs (#11), zero tests (#12). [HIGH]
- `.planning/codebase/INTEGRATIONS.md` — system-bus default + `USE_USER_BUS`, root-only D-Bus policy file, uinput "LogiOps Virtual Input", udev `hidraw` hotplug, ipcgull vendored, CI matrix. [HIGH]
- `.planning/PROJECT.md` — daemon-on-top architecture, action-wheel/gesture daemon-extension intent, `Device.h:55` HID++ 2.0 hardcode, Debian-first, privilege-resolution requirement. [HIGH]
- Wayland input/overlay model (no global grab, no absolute positioning, no global hotkeys; `wlr-layer-shell` vs Mutter non-support; XWayland masks X11-only behavior), polkit-gating system-bus daemons, Debian `dh_installsystemd`/D-Bus/polkit packaging — training-data + first-party knowledge. [MEDIUM — re-verify compositor layer-shell support and current Debian polkit defaults before the relevant phase; WebSearch was unavailable this run.]

---
*Pitfalls research for: Linux GUI on a root HID++ daemon via D-Bus (Options+ clone)*
*Researched: 2026-05-30*
