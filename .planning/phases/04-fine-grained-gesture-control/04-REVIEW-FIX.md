---
phase: 04-fine-grained-gesture-control
fixed_at: 2026-05-31T00:00:00Z
review_path: .planning/phases/04-fine-grained-gesture-control/04-REVIEW.md
iteration: 1
findings_in_scope: 5
fixed: 4
skipped: 1
status: partial
---

# Phase 4: Code Review Fix Report

**Fixed at:** 2026-05-31
**Source review:** .planning/phases/04-fine-grained-gesture-control/04-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 5 (5 warnings; 0 critical)
- Fixed: 4
- Skipped: 1

All fixes were verified against the full build (`cmake --build build`, -Werror
clean, exit 0), the full test suite (`ctest --test-dir build`, 14/14 passing),
and — for the QML-affecting fix — a cleared-qmlcache rebuild plus an offscreen
smoke test grepping `not a type|Unknown method|Error:` (clean: "QML OK").

## Fixed Issues

### WR-05: `IntervalGesture::setAction` / `ReleaseGesture::setAction` can throw `InvalidAction` and abort the root daemon

**Files modified:** `src/logid/actions/gesture/IntervalGesture.cpp`, `src/logid/actions/gesture/ReleaseGesture.cpp`
**Commit:** c77b080
**Applied fix:** Wrapped both D-Bus-exposed `setAction(const std::string& type)`
setters in try/catch, mirroring `GestureAction::setGesture`. The untrusted `type`
is passed to `Action::makeAction`; on `InvalidAction` the prior `_config.action`
snapshot is restored, a VALID `_action` is rebuilt from the restored config (or
left reset if none), and a clean `std::invalid_argument("Invalid action type")`
is rethrown so ipcgull marshals a D-Bus error instead of the daemon aborting and
the node is never left with a partially-mutated config or missing action. Added
explicit `<stdexcept>` includes to both TUs rather than relying on transitive
inclusion. Verified: daemon target builds -Werror clean; 14/14 tests pass.

### WR-02: `GestureBuilder.qml` binds `modeOf()` (stale method call, no NOTIFY)

**Files modified:** `src/logiops-gui/GestureModel.h`, `src/logiops-gui/qml/config/GestureBuilder.qml`
**Commit:** 004574c
**Applied fix:** Added a NOTIFYable `Q_PROPERTY(QString activeMode READ activeMode
NOTIFY previewChanged)` to `GestureModel` (getter returns `modeOf(_active)`) and
rebound the QML `activeMode` to `gestureModel.activeMode` instead of the bare
`gestureModel.modeOf(activeDirection)` method call. `previewChanged` already fires
on both `setMode` (when editing the active direction) and `setActiveDirection`, so
the progressive-disclosure sections (`showsAction`/`showsGranularity`) now re-bind
when the mode of the current direction changes — not only on a direction switch.
Verified: cleared qmlcache, rebuilt logiops-gui clean, offscreen smoke test "QML
OK", 14/14 tests pass.

### WR-01: Two-step param call can race ahead of `SetGesture` (lost mode + param)

**Files modified:** `src/logiops-gui/GestureModel.h`, `src/logiops-gui/GestureModel.cpp`
**Commit:** d7faa6b
**Applied fix:** Added a per-direction in-flight `SetGesture` watcher map
(`_pendingSetGesture`). The live `performSetGesture` now records its
`QDBusPendingCallWatcher` keyed by direction (kept alive until its reply lands).
The live `performParamCall` chains the param setter strictly behind that watcher's
`finished` signal when a `SetGesture` is still in flight for the direction, and
fires the setter ONLY when the `SetGesture` did not error (a failed mode switch
means the `.Gesture.<type>` child node was never created, so the param is no longer
issued blind). When no `SetGesture` is in flight (adjusting a param on an already-
set mode), it probes the child node and fires only if the interface is present.
The recording test seam is unaffected (overrides record call order; live wiring is
not exercised). Verified: builds clean, 14/14 tests pass, smoke test "QML OK".

**Note (requires human verification):** the async sequencing/ordering is correct
by construction but is not covered by an automated live-bus test (the model's test
harness records calls via subclass overrides). A developer should confirm the
chained behavior against a live daemon — issue a granularity change immediately
after a mode switch and confirm the param lands on the rebuilt node.

### WR-03: GestureModel state is never seeded from the daemon — builder always opens blank

**Files modified:** `src/logiops-gui/GestureModel.h`, `src/logiops-gui/GestureModel.cpp`
**Commit:** 6f5f724
**Applied fix:** Added `seedFromDaemon()`, called from the live constructor (no-op
on the test path where `_live == false`). For each cardinal direction it probes
which `.Gesture.<type>` interface is present at `.../gestures/{dir}` (first present
wins, mirroring `ButtonsModel::enumerate`'s BTN-04 present-interface readback),
maps the daemon type back to the plain-language mode, and reads the granularity
param via the per-mode getter (`GetConfig` for OnInterval/Axis, `GetThreshold` for
OnRelease). It then emits `configuredChanged(direction)` per seeded direction and a
single `previewChanged`, so the builder opens reflecting the button's existing
gestures (configured dots, mode pills, preview sentence) instead of blank and no
longer blind-overwrites existing config. Verified: builds clean, 14/14 tests pass
(test path untouched since seeding is gated on `_live`), smoke test "QML OK".

**Note (requires human verification):** the readback is best-effort and uses
bounded synchronous `GetAll`/getter probes (4 directions, lazily, once per builder
open — consistent with the existing `ButtonsModel::enumerate` pattern the review
pointed to). The gesture action TYPE is not seeded (the discrete gesture
interfaces expose no `GetAction` getter), so the preview's action phrase opens at
its "doing what you choose" default until the user re-picks an action; mode and
granularity are restored. Confirm against a live daemon with a pre-configured
button.

## Skipped Issues

### WR-04: `ButtonsModel::enumerate` does blocking sync D-Bus calls inside the async reply

**File:** `src/logiops-gui/ButtonsModel.cpp:334-380`
**Reason:** skipped — out of strict Phase 4 scope (pre-existing Phase 3 code). The
review itself flags this as "pre-existing Phase 3 code" and "Out of strict Phase 4
scope," and the fix guidance directs to document-and-skip rather than risk a
regression. The recommended fix (replace the 8 per-button blocking `GetAll` probes
with a single `Introspectable.Introspect` + XML parse, as
`DeviceController::parseInterfaces` does, plus async identity-prop watchers) is a
non-trivial refactor of the Phase 3 button-list readback that the gesture-entry
path (`GestureSupport`/`ButtonPath` roles) depends on. Changing it carries a real
risk of regressing the button enumeration that all of Phase 3 and the gesture flow
rely on, with no Phase 4 test coverage to catch a regression. Deferred to a
dedicated Phase 3 performance task.
**Original issue:** Inside the async `Enumerate` reply lambda, for every button the
code issues synchronous blocking calls (`btn->property(...)` ×4, `GetAll` over 8
action types, `GetHostCount`), a serial blocking round-trip storm on the UI thread
that can freeze the UI on an asleep/slow device.

---

_Fixed: 2026-05-31_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
