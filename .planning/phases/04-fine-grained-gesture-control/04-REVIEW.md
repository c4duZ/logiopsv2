---
phase: 04-fine-grained-gesture-control
reviewed: 2026-05-31T00:00:00Z
depth: standard
files_reviewed: 18
files_reviewed_list:
  - src/logid/Configuration.h
  - src/logid/actions/GestureAction.cpp
  - src/logid/actions/gesture/AxisGesture.cpp
  - src/logid/actions/gesture/IntervalGesture.cpp
  - src/logiops-gui/GestureModel.h
  - src/logiops-gui/GestureModel.cpp
  - src/logiops-gui/DeviceController.h
  - src/logiops-gui/DeviceController.cpp
  - src/logiops-gui/ButtonsModel.h
  - src/logiops-gui/ButtonsModel.cpp
  - src/logiops-gui/qml/config/GestureBuilder.qml
  - src/logiops-gui/qml/config/ReassignPanel.qml
  - src/logiops-gui/qml/Theme.qml
  - src/logiops-gui/dbus/logid_action_gesture.xml
  - src/logiops-gui/CMakeLists.txt
  - tests/phase4/gesture_math_test.cpp
  - tests/phase4/GestureModelTest.cpp
  - tests/phase4/CMakeLists.txt
findings:
  critical: 0
  warning: 5
  info: 5
  total: 10
status: issues_found
---

# Phase 4: Code Review Report

**Reviewed:** 2026-05-31
**Depth:** standard
**Files Reviewed:** 18
**Status:** issues_found

## Summary

Phase 4 (Fine-Grained Gesture Control) extends the root daemon's gesture path
(`GestureAction`, `IntervalGesture`, `AxisGesture`) and adds the GUI gesture
builder (`GestureModel` + `GestureBuilder.qml`, wired through `DeviceController`
and `ReassignPanel.qml`).

The security lenses called out for this phase are in good shape. The two main
untrusted-D-Bus entry points on the root daemon are handled correctly:
`GestureAction::setGesture` allowlists the direction via `toDirection` (throws
`std::invalid_argument`, no `std::stoi`), guards the `std::optional` gestures map
against `bad_optional_access` (line 252), and restores a valid interface on an
`InvalidGesture` type before rethrowing a clean D-Bus error. The GEST-02/03
interval fix is correct and matches the `gesture_math_test` contract:
`interval.value_or(defaults::gesture_interval)` with an `interval <= 0` divide
guard, and the `(_axis - threshold) / interval` accounting fires exactly
`floor(M/I)` times per motion with no overshoot. The GUI mode allowlist is
correct ("OnRelease" canonical, "None" not "NoPress", "OnThreshold" never sent).
`logPrintf` calls all use `%s` with the runtime string as an argument, not as the
format (CONCERNS #5 respected). No critical issues found.

The findings below are correctness/robustness gaps, not vulnerabilities. The
highest-signal ones are WR-01 (a real lost-update race in the two-step dispatch),
WR-02 (a QML stale-binding `modeOf()` instance — the exact recurring defect class
flagged for this phase), and WR-03 (model state never seeded from the daemon, so
the builder always opens blank).

## Warnings

### WR-01: Two-step param call can race ahead of `SetGesture` (lost mode + param)

**File:** `src/logiops-gui/GestureModel.cpp:310-354`
**Issue:** The two-step sequencing relies on a `Properties.GetAll` "probe hop"
on a *fresh* `QDBusInterface` (`performParamCall`) to guarantee the preceding
`SetGesture` (issued on a *different* fresh `QDBusInterface` in
`performSetGesture`) has been processed. The comment claims "both calls are
queued on the same ordered connection," but `SetGesture` targets `_buttonPath`
(the Action.Gesture interface) and the probe targets
`_buttonPath + "/gestures/" + direction` (a different object path that does not
exist yet until `SetGesture` rebuilds the node). On the live path,
`setMode()` calls `performSetGesture` and returns; the per-mode param call only
happens on a *separate later* user action (`setGranularity`/`setGestureAction`),
so in practice ordering usually holds. But nothing actually sequences the param
call behind the `SetGesture` *reply* — unlike `ButtonsModel`, which at least
probes the *same* path. If `setGranularity` is invoked programmatically right
after `setMode` (e.g. a future "apply preset" path, or fast UI), the
`GetAll` on the not-yet-created child node returns an error and the code fires
the setter anyway (line 345 runs unconditionally in the `finished` lambda),
hitting a non-existent interface — the param is silently lost.
**Fix:** Sequence `performParamCall` behind the actual `SetGesture`
`QDBusPendingCallWatcher::finished` for that direction (chain it, as the header
comment at GestureModel.h:128-130 promises), or have `setMode` remember a
pending param and flush it from the `SetGesture` reply lambda. At minimum, only
fire the setter when the probe reply is *not* an error, and retry/defer
otherwise, so a param call to a missing child node is never issued blind.

### WR-02: `GestureBuilder.qml` binds `modeOf()` (stale method call, no NOTIFY)

**File:** `src/logiops-gui/qml/config/GestureBuilder.qml:37`
**Issue:** `readonly property string activeMode: gestureModel ? gestureModel.modeOf(activeDirection) : ""`
binds to the non-NOTIFYable `Q_INVOKABLE modeOf()` method. This is the exact
recurring defect class flagged for this phase (a binding to a bare method call
that goes stale). `activeMode` re-evaluates only when `activeDirection` changes
(its sole tracked dependency). When the user changes the *mode* of the current
direction via `setMode()`, `modeOf(activeDirection)` returns a new value but
QML never re-runs the binding — `previewChanged` is emitted but `activeMode` is
not subscribed to it. Result: the progressive-disclosure sections
(`showsAction`/`showsGranularity` at lines 159-162, derived from `activeMode`)
do not appear/update after picking a mode until the user switches directions and
back.
**Fix:** Drive `activeMode` off a NOTIFYable property. Either add a
`Q_PROPERTY(QString activeMode READ activeMode NOTIFY previewChanged)` to
`GestureModel` (computed from `_active`'s `DirectionState.plainMode`) and bind
that, or add `previewChanged` as an explicit dependency. The model already
emits `previewChanged` on `setMode` when `direction == _active`
(GestureModel.cpp:125-127), so a NOTIFYable `activeMode` would update correctly.

### WR-03: GestureModel state is never seeded from the daemon — builder always opens blank

**File:** `src/logiops-gui/GestureModel.cpp:42-44`, `src/logiops-gui/DeviceController.cpp:431-455`
**Issue:** The live `GestureModel` constructor stores `_buttonPath`/`_bus` but
never reads the button's *existing* gesture configuration. `_dirs` starts empty,
so `modeOf`/`isConfigured`/`previewSentence`/`granularityReadout` all report the
unconfigured default for every direction, even when the daemon already has
gestures bound on that button (e.g. a config loaded from `/etc/logid.cfg`, or a
button previously configured this session). Opening the gesture builder on an
already-configured button shows "Moving up does nothing." and no configured
dots, contradicting the live-state premise the rest of the GUI follows
(`DeviceController`/`ButtonsModel` both seed from `Get*`/`Enumerate`). Every
edit then blind-overwrites whatever was there.
**Fix:** On live construction, introspect `_buttonPath + "/gestures/{dir}"` for
each cardinal direction (which `.Gesture.<type>` interface is present → mode) and
read its params via the per-mode `GetConfig`/`GetThreshold`, then populate
`_dirs` and emit `previewChanged`/`configuredChanged` — mirroring
`ButtonsModel::enumerate`'s present-interface readback (BTN-04).

### WR-04: `ButtonsModel::enumerate` does blocking sync D-Bus calls inside the async reply

**File:** `src/logiops-gui/ButtonsModel.cpp:334-380`
**Issue:** Inside the async `Enumerate` reply lambda, for every button the code
issues *synchronous* blocking calls: `btn->property(...)` (×4, each a blocking
`Get`), `props.call("GetAll", iface)` in a loop over 8 action types, and a
`ch.call("GetHostCount")`. The comment claims these are "cheap cached daemon
properties (no hardware I/O)," but `GetAll` probing 8 interfaces per button × N
buttons is a serial blocking round-trip storm on the UI thread, and any asleep
or slow device (the exact case `introspectInterfaces` was made async to avoid,
T-3-01-02) will freeze the UI. This is pre-existing Phase 3 code but is on the
gesture-entry path (the `GestureSupport`/`ButtonPath` roles the builder needs).
**Fix:** Use `org.freedesktop.DBus.Introspectable.Introspect` once per button
(one call, parse the present `.Action.<X>` interface from XML, as
`DeviceController::parseInterfaces` already does) instead of 8 blocking `GetAll`
probes, and read identity props from the introspection or via async watchers.
Out of strict Phase 4 scope but flagged because the gesture flow depends on it.

### WR-05: `IntervalGesture::setAction` can throw `InvalidAction` and abort the root daemon

**File:** `src/logid/actions/gesture/IntervalGesture.cpp:118-122`
**Issue:** `setAction(const std::string& type)` is a D-Bus-exposed setter on the
root daemon taking an untrusted `type` string. It calls
`Action::makeAction(_device, type, _config.action, _node)` with no try/catch.
`makeAction` throws `InvalidAction` on an unrecognized type (parallel to
`makeGesture` throwing `InvalidGesture`). The constructor path *does* guard this
(lines 41-47 catch `InvalidAction`), but the setter does not, so a bad action
type from a `logiops`-group client propagates out. Whether that aborts the
daemon depends on ipcgull marshalling the exception to a D-Bus error; the
sibling `GestureAction::setGesture` deliberately catches+rethrows-as-clean for
exactly this reason (GestureAction.cpp:262-270), so this setter is inconsistent
with the established safe pattern. Note also `_config.action` is left modified
by a partial `makeAction` on the throwing path.
**Fix:** Wrap in try/catch like the constructor and `GestureAction::setGesture`:
on `InvalidAction`, restore a valid action from the existing config (or reset
`_action`) and rethrow `std::invalid_argument("Invalid action type")` so the
node keeps a valid interface and the error marshals cleanly. The same applies to
`ReleaseGesture::setAction` (ReleaseGesture.cpp:86-90), which has the identical
unguarded pattern.

## Info

### IN-01: GestureModel granularity for OnRelease/Axis is semantically lossy

**File:** `src/logiops-gui/GestureModel.cpp:154-168`
**Issue:** A single `setGranularity(rawValue)` slider (range 0-200 in
`GestureBuilder.qml:202`) maps to `SetInterval` (OnInterval), `SetThreshold`
(OnRelease), or `SetMultiplier` as a `double` (Axis). For Axis the slider's
0-200 integer becomes a multiplier of 0..200× (line 164), which is an enormous
and non-intuitive scroll multiplier; and for OnRelease the same 0-200 maps to a
threshold. One slider with three incompatible unit meanings is hard to calibrate
and the preview phrase buckets (granularityPhrase) only model the interval/flick
semantics. Consider per-mode ranges or a multiplier sub-range mapping. Not a
bug; a UX/calibration gap.

### IN-02: `previewSentence()` copies the whole DirectionState by value

**File:** `src/logiops-gui/GestureModel.cpp:231-235`
**Issue:** `DirectionState st = (it == _dirs.constEnd()) ? DirectionState{} : *it;`
copies the struct to handle the not-found case. Minor; a `const DirectionState`
default-static + pointer, or composing directly from `*it`, avoids the copy.
Negligible cost, noted for consistency only.

### IN-03: `GestureBuilder.qml` action picker hook is a stub

**File:** `src/logiops-gui/qml/config/ReassignPanel.qml:258-260`
**Issue:** `onChooseActionRequested` only sets `root.expanded = "Keypress"`; it
does not scope the captured keystroke back to the gesture direction — the
`KeyCaptureField.onKeysCaptured` (lines 119-122) still calls
`buttonsModel.setKeypress(row, ...)`, which reassigns the *button*, not the
gesture direction's action. So choosing an action for a gesture direction
silently rebinds the whole button instead of calling
`gestureModel.setGestureAction(direction, ...)`. Likely an incomplete wiring
rather than a Phase-4-final behavior; flagging so it is not mistaken for done.
The discrete-mode action sub-section (GEST-03) is effectively non-functional
until this is connected.

### IN-04: Duplicated mode-label string literals across C++ and QML

**File:** `src/logiops-gui/qml/config/GestureBuilder.qml:159-162`
**Issue:** `showsAction`/`showsGranularity` compare `activeMode` against
`qsTr("Repeat while moving")` / `qsTr("Do once when moved far enough")` /
`qsTr("Adjust proportionally")` — the same plain-language strings that are the
single source of truth in `GestureModel::plainModes()` (GestureModel.cpp:81-88).
The header comment claims "QML never hardcodes mode strings," but here it does,
and a `qsTr()` translation would break the comparison (the model side is not
`tr()`-wrapped). Expose the mode→needs-action/needs-granularity mapping from the
model (e.g. `Q_INVOKABLE bool modeNeedsAction(plainMode)`) so the disclosure
logic is not duplicated and translation-fragile.

### IN-05: `defaults::io_timeout` / `gesture_threshold` declared `static constexpr` in header

**File:** `src/logid/Configuration.h:32-38`
**Issue:** `static constexpr` namespace-scope constants in a header give each TU
internal linkage / its own copy. Harmless for these values, but `inline
constexpr` is the C++17+ idiom for header constants shared across TUs and avoids
potential ODR surprises if an address is ever taken. Style/consistency note only.

---

_Reviewed: 2026-05-31_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
