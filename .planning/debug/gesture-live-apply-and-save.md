---
slug: gesture-live-apply-and-save
status: awaiting_human_verify
created: 2026-05-31
updated: 2026-05-31
component: daemon (logid) + GUI Save path
severity: blocking (Phase 4 on-hardware UAT)
---

## Current Focus (Bug B)

hypothesis: CONFIRMED — the LogiOps D-Bus policy's `<deny receive_sender="pizza.pixl.LogiOps"/>`
  in `context="default"` is too broad. Because it has no destination/type qualifier, it
  denies EVERY connection (incl. polkitd) from RECEIVING any message sent BY logid (which
  owns pizza.pixl.LogiOps). logid's outbound CheckAuthorization to polkitd is thus rejected
  at the bus ("Rejected receive message, 2 matched rules": [1] global allow receive_type=
  method_call, [2] this deny — deny is last-match and wins). Daemon's fail-safe -> DENY.
test: edit policy template to scope the deny to signals only (or remove it), reinstall to
  /usr/share/dbus-1/system.d/, reload dbus, re-test Save on hardware.
expecting: Save succeeds (or pops a polkit prompt) instead of immediate auth-declined.
next_action: apply fix to template + verify mechanism, then human-verify reinstall+retest.

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

## Bug B — Investigation log (resumed 2026-05-31 ~19:00)

**Evidence gathered (system-side all CORRECT):**
- Daemon code path sound: `Configuration::save()` -> `current_caller()` (ipcgull
  thread_local unique bus name `:1.x`, server_gdbus.cpp:327) -> `checkSaveAuthorized`
  -> `polkit_system_bus_name_new(caller)` + `check_authorization_sync` with
  `ALLOW_USER_INTERACTION`. Standard/correct subject construction.
- Throw `"Not authorized to save configuration"` -> ipcgull G_DBUS_ERROR_FAILED with
  that message -> GUI `isAuthDenied` matches "authoriz" -> auth-declined UI string.
  So the UI banner faithfully reflects a real polkit DENY (not a misclassified error).
- Running daemon: /usr/bin/logid PID 3210, started 18:46 today (has polkit code,
  newer than Configuration.cpp).
- Session 3: c4duz, seat0, tty2, **Active=yes**, Type=wayland. Desktop = Zorin (GNOME),
  gnome-shell PID 2591 running.
- polkitd PID 862 running. Action `pizza.pixl.logiops.save-config` IS registered
  (`pkaction --verbose` shows implicit any/inactive/active = auth_admin_keep).
- Policy file installed & loaded.

**Conclusion so far:** Every static/system precondition is correct. The DENY happens
inside the live polkit interaction. Two remaining falsifiable hypotheses requiring
live observation:
  B-i. No polkit AUTH AGENT is registered for session 3 (search for a GUI agent
       process found ONLY polkitd, no gnome/gsd polkit-agent). If gnome-shell's
       agent isn't registered, polkit cannot prompt -> immediate not_authorized.
       TEST: `pkcheck --action-id pizza.pixl.logiops.save-config --process <pid> -u`
       from the GUI's own pid should pop a prompt. If it errors with
       "no authentication agent" -> B-i confirmed.
  B-ii. Agent registered but subject mismatch / prompt routes nowhere. Differentiated
       by the same pkcheck test: if a prompt DOES appear there but Save still fails,
       subject differs between pkcheck (unix-process) and daemon (system-bus-name).

## Bug B — ROOT CAUSE FOUND + FIX (2026-05-31, reframed by on-hardware journal evidence)

**New evidence (journalctl -u logid, GUI launched directly, no `sg`):**
```
[WARN] polkit check failed, denying save: GDBus.Error:org.freedesktop.DBus.Error.AccessDenied:
Rejected receive message, 2 matched rules; type="method_call", sender=":1.86" (uid=0 pid=3210
comm="/usr/bin/logid") interface="org.freedesktop.PolicyKit1.Authority" member="CheckAuthorization"
... destination=":1.7" (uid=990 pid=862 comm=".../polkitd")
```

**ROOT CAUSE:** The LogiOps D-Bus policy template (`src/logid/logiops-dbus.conf.in`,
installed at `/usr/share/dbus-1/system.d/pizza.pixl.LogiOps.conf`) contained, in
`context="default"`, an UNQUALIFIED:
    `<deny receive_sender="pizza.pixl.LogiOps"/>`
`receive_sender` matches by the SENDER's owned name, with no destination/type qualifier.
`logid` owns `pizza.pixl.LogiOps`, so this deny blocks EVERY connection on the bus —
including polkitd — from RECEIVING ANY message originating from logid, including logid's
own OUTBOUND `CheckAuthorization` method_call to polkitd.

The "2 matched rules" on polkitd's receive check:
  [1] `<allow receive_type="method_call"/>` (global system.conf default) — matches.
  [2] this `<deny receive_sender="pizza.pixl.LogiOps"/>` (LogiOps default ctx) — matches,
      and being the later-loaded last match, WINS -> "Rejected receive message".
=> logid's polkit query never reaches polkitd -> daemon's fail-safe DENY path fires
   -> throw "Not authorized to save configuration" -> GUI "authorization declined".

NOT a polkit-agent problem and NOT a subject mismatch (B-i / B-ii both eliminated:
the query never reached polkit at all).

**Intent of the original deny:** keep non-(logiops-group) users from receiving LogiOps
*signals* (DeviceAdded/StatusChanged/BatteryChanged/PairReady). The deny was just too broad.

**FIX (applied to template):** scope the deny to signals only —
    `<deny receive_sender="pizza.pixl.LogiOps" receive_type="signal"/>`
Signals to non-group users stay blocked (privacy intent preserved; the
`group="logiops"` `<allow receive_sender>` re-permits them for the GUI). Method calls
(incl. logid->polkitd, and replies logid->GUI) are no longer blocked by this rule.

**Eliminated hypotheses:**
- B-i (no auth agent): WRONG — query never reached polkit; bus rejected it first.
- B-ii (subject mismatch): WRONG — same reason; subject is irrelevant when bus blocks delivery.
- "installed policy stale vs template": WRONG — diff'd, installed file == template (both had the bug).

## Resolution (Bug B)

root_cause: Unqualified `<deny receive_sender="pizza.pixl.LogiOps"/>` in the default-context
  D-Bus system policy blocked polkitd from receiving logid's outbound CheckAuthorization
  method call, so the daemon's polkit check always failed -> fail-safe DENY -> Save declined.
fix: Narrow the deny to `receive_type="signal"` so it only restricts who may receive LogiOps
  *signals*, not the method calls the root daemon makes to other services.
verification: PENDING human-verify (requires privileged reinstall of policy + dbus reload + Save retest).
files_changed: [src/logid/logiops-dbus.conf.in]

**Next: human-verify checkpoint — user reinstalls policy, reloads dbus, re-tests Save.**

## How to resume

`/gsd-debug` and reference this file, or read it and trace Bug A first (it's the
feature blocker) — but fixing Bug B (Save) gives a guaranteed workaround
(configure → Save → `sudo systemctl restart logid` → cfg-load applies the
gesture), so Bug B may be the higher-leverage first target.

**Build/run reminder:** GUI via `./build/src/logiops-gui/logiops-gui` (build
binary finds the on-disk QML module; the installed `/usr/bin/logiops-gui` has a
Qt-embedded-module bug). After any QML edit: clear `build/src/logiops-gui/.rcc`
+ targeted rebuild (Qt 6.4 stale-qmlcache). `rebuild-gui.sh` automates it.
