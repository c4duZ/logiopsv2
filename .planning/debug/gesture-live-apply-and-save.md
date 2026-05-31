---
slug: gesture-live-apply-and-save
status: open
created: 2026-05-31
component: daemon (logid) + GUI Save path
severity: blocking (Phase 4 on-hardware UAT)
---

# Debug: gesture live-apply + Save (two open daemon-side bugs)

Paused mid-UAT. Both are DAEMON-level (the GUI-side gesture/reassign bugs from
this session are fixed: qmlcache, freeze, responsive panel, config-wipe,
granularity slider, stale-model-on-button-switch, reassign read-back). These two
need fresh, careful tracing.

## Bug A — A newly-defined gesture does not take effect live

**Symptom:** Gestures already in `/etc/logid.cfg` fire on hardware. A gesture
configured via the GUI (correct button now, after the stale-model fix `2e1bd60`)
does NOT fire — the old/native behavior persists. Neither live nor (would-be)
Save+restart works because Save is also broken (Bug B).

**Diagnosis so far:**
- The live dispatch reads the per-direction map: `GestureAction::move/press/release`
  look up `_gestures.find(<direction>)` (`src/logid/actions/GestureAction.cpp:109-202`).
- `GestureAction::setGesture(direction,type)` (`GestureAction.cpp:~81`,`:226`) is
  supposed to release the old gesture, mutate the config variant, reset the
  `shared_ptr` in `_gestures`, and re-register the `.Gesture.<mode>` ipcgull
  interface. RESEARCH (`.planning/quick/260531-lxy-...-RESEARCH.md`) judged the
  swap "sound", but on hardware the new gesture does not dispatch.

**Hypotheses to test next (scientific method):**
1. Does the GUI's `GestureModel::setMode`/`setGestureKeypress` two-step actually
   reach the daemon for an ALREADY-gesture button? (We stopped calling
   `setAction("Gesture")` on open in `a388c68` to avoid the wipe — verify the
   per-direction `SetGesture`/`SetAction("Keypress")`/`SetKeys` calls still land.
   Watch with `sg logiops -c 'busctl --system monitor pizza.pixl.LogiOps'` while
   editing, or daemon `-vvv`.)
2. After a live `SetGesture`, does `_gestures[dir]` actually hold the NEW gesture
   object, and does the gesture's ACTION (the keypress) get wired? Check whether
   the gesture's child action (`.Action.Keypress` + `SetKeys`) updates the live
   gesture's action object or only a config copy.
3. Is the GestureAction object the HID++ handler dispatches the SAME instance the
   D-Bus interface mutates? (Mirror of the question that was the RemapButton
   live-swap — which IS correct. Confirm the gesture node's interface points at
   the live GestureAction's `_gestures`, not a detached copy.)
4. For OnRelease/OnThreshold: confirm the threshold/granularity default lets it
   fire at all (the GEST-02 fix set `defaults::gesture_interval=120`; check the
   threshold default for OnRelease).

**Key files:** `src/logid/actions/GestureAction.cpp` (dispatch + setGesture),
`src/logid/actions/gesture/*.cpp` (ReleaseGesture/IntervalGesture/AxisGesture +
their `setAction`/`setThreshold`), `src/logiops-gui/GestureModel.cpp`
(setMode/setGestureKeypress two-step + the async seedFromDaemon).

## Bug B — Save fails with "authorization declined"

**Symptom:** Clicking Save shows "Couldn't save — authorization was declined."
Earlier ROOT CAUSE (this session): running the GUI via `sg logiops -c` strips
`XDG_SESSION_ID`, so polkit can't tie the caller to the active graphical session
→ auth_admin declines. The user rebooted and runs the build binary directly
(no `sg`), session 3 is Active, GNOME has a polkit agent — yet it STILL declines.

**Hypotheses to test next:**
1. Is the user STILL launching via `sg` (habit)? Confirm the GUI process has a
   non-empty `XDG_SESSION_ID` (`cat /proc/$(pgrep -f logiops-gui)/environ | tr '\0' '\n' | grep XDG_SESSION_ID`).
2. If session is fine: does the polkit prompt actually appear? `pkcheck
   --action-id pizza.pixl.logiops.save-config --process <gui-pid> -u` to test
   interactively. Check the daemon's Save handler — how it passes the caller
   subject to polkit (bus name vs pid vs `polkit.subject`), and whether it uses
   `CheckAuthorization` with `AllowUserInteraction`.
3. Action defaults are `auth_admin_keep` (`/usr/share/polkit-1/actions/pizza.pixl.logiops.policy`).
   The user is in `sudo` (admin) — auth should be possible. Confirm the daemon
   isn't querying with the WRONG subject (e.g. its own root pid instead of the
   caller).

**Key files:** the daemon Config.Save handler (polkit query — find via
`grep -rn "CheckAuthorization\|polkit\|save-config" src/logid/`),
`src/logiops-gui/ConfigState.cpp` (the async Save + error mapping).

## How to resume

`/gsd-debug` and reference this file, or read it and trace Bug A first (it's the
feature blocker) — but fixing Bug B (Save) gives a guaranteed workaround
(configure → Save → `sudo systemctl restart logid` → cfg-load applies the
gesture), so Bug B may be the higher-leverage first target.

**Build/run reminder:** GUI via `./build/src/logiops-gui/logiops-gui` (build
binary finds the on-disk QML module; the installed `/usr/bin/logiops-gui` has a
Qt-embedded-module bug). After any QML edit: clear `build/src/logiops-gui/.rcc`
+ targeted rebuild (Qt 6.4 stale-qmlcache). `rebuild-gui.sh` automates it.
