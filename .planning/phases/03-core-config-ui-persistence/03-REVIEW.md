---
phase: 03-core-config-ui-persistence
reviewed: 2026-05-30T00:00:00Z
depth: standard
files_reviewed: 30
files_reviewed_list:
  - src/logid/actions/ChangeHostAction.cpp
  - src/logid/actions/ChangeHostAction.h
  - src/logid/config/schema.h
  - src/logid/features/DPI.cpp
  - src/logid/features/DPI.h
  - src/logiops-gui/ButtonsModel.cpp
  - src/logiops-gui/ButtonsModel.h
  - src/logiops-gui/ConfigState.cpp
  - src/logiops-gui/ConfigState.h
  - src/logiops-gui/DeviceController.cpp
  - src/logiops-gui/DeviceController.h
  - src/logiops-gui/DeviceControllerFactory.cpp
  - src/logiops-gui/DeviceControllerFactory.h
  - src/logiops-gui/ProfilesModel.cpp
  - src/logiops-gui/ProfilesModel.h
  - src/logiops-gui/KeyNameMapper.cpp
  - src/logiops-gui/KeyNameMapper.h
  - src/logiops-gui/main.cpp
  - src/logiops-gui/qml/config/ButtonsTab.qml
  - src/logiops-gui/qml/config/ReassignPanel.qml
  - src/logiops-gui/qml/config/KeyCaptureField.qml
  - src/logiops-gui/qml/config/DeviceRender.qml
  - src/logiops-gui/qml/config/BindingList.qml
  - src/logiops-gui/qml/config/PointerTab.qml
  - src/logiops-gui/qml/config/DpiCycleEditor.qml
  - src/logiops-gui/qml/config/ScrollTab.qml
  - src/logiops-gui/qml/config/ProfilesTab.qml
  - src/logiops-gui/qml/config/SaveToolbar.qml
  - src/logiops-gui/qml/config/RestoreDialog.qml
  - src/logiops-gui/qml/config/ConfigTabs.qml
  - src/logiops-gui/qml/DetailPane.qml
findings:
  critical: 2
  warning: 5
  info: 6
  total: 13
status: issues_found
---

# Phase 3: Code Review Report

**Reviewed:** 2026-05-30
**Depth:** standard
**Files Reviewed:** 30
**Status:** issues_found

## Summary

This phase widens the root-running `logid` daemon's D-Bus audience to non-root users
and adds the core config UI plus two new daemon entry points (`ChangeHost.GetHostCount`,
`DPI.GetPresets/SetPresets` + the `dpi_presets` schema). The GUI side is well-structured:
async-everywhere D-Bus (no UI-thread blocking through the polkit prompt), a clean
test seam on every model, defensive row/index bounds checks, and capability gating that
mirrors the daemon's `UnsupportedFeature` model. `KeyNameMapper`, `ConfigState`'s
no-retry-storm logic, and the `setChangeHost` client-side validation are all correct.

The serious issues are concentrated exactly where the priority context warned: the
**daemon C++ that now accepts untrusted input from a non-root D-Bus caller**. Two are
genuine remote-crash (DoS) vectors against a root process — `ChangeHostAction::setHost`
runs `std::stoi` on an attacker-controlled string, and `ChangeHostAction::release` does a
modulo by a device-reported `hostCount` that can be `0`. The GUI validates these, but the
D-Bus methods are directly callable and the daemon must not trust the GUI. A handful of
warnings cover an inverted DPI nearest-match comparator, an off-by-one in CycleDPI list
sizing, and a missing `markDirty()` wiring contract that undercuts CONF-01.

## Critical Issues

### CR-01: `ChangeHostAction::setHost` crashes the root daemon on non-numeric D-Bus input (DoS)

**File:** `src/logid/actions/ChangeHostAction.cpp:77-86`
**Issue:** `SetHost` is exposed over D-Bus (line 37) and, after this phase, is reachable by
any non-root member of the Phase-1 group. For any string that is not `next`/`prev`/`previous`,
the handler calls `std::stoi(host)` directly. `std::stoi` throws `std::invalid_argument` on a
non-numeric string (e.g. `""`, `"x"`, `"3a"`) and `std::out_of_range` on an oversized number
(e.g. `"99999999999"`). Nothing in this call path catches it, so the exception propagates out
of the ipcgull dispatch and terminates (or destabilizes) the root process. The GUI's
`ButtonsModel::setChangeHost` validation (ButtonsModel.cpp:172-191) does NOT protect the daemon —
the method is callable directly over the bus. This is the precise threat the phase priority
context calls out ("std::stoi DoS guard expected").
**Fix:** Validate/parse defensively inside the daemon, never trust the caller:
```cpp
void ChangeHostAction::setHost(std::string host) {
    std::transform(host.begin(), host.end(), host.begin(), ::tolower);
    std::unique_lock lock(_config_mutex);
    if (host == "next" || host == "prev" || host == "previous") {
        _config.host = std::move(host);
        return;
    }
    int parsed;
    try {
        size_t consumed = 0;
        parsed = std::stoi(host, &consumed);
        if (consumed != host.size())          // reject "3a", "3 ", etc.
            throw std::invalid_argument(host);
    } catch (const std::exception&) {
        throw std::invalid_argument("ChangeHost: host must be a number or next/prev");
        // or surface an ipcgull error type the bus marshals as a normal D-Bus error
    }
    _config.host = parsed;
}
```
Throwing a *caught/marshalled* D-Bus error is fine; an *uncaught* `std::stoi` exception out of
the worker/dispatch is not.

### CR-02: `ChangeHostAction::release` divides by `hostCount` (zero → crash) and can set a negative host

**File:** `src/logid/actions/ChangeHostAction.cpp:110`
**Issue:** `next_host %= host_info.hostCount;` where `hostCount` is a device-reported `uint8_t`
(`ChangeHost.h:42`). If the device reports `hostCount == 0` (an uncooperative/odd device, or a
transient read), this is integer division by zero — undefined behavior, in practice a SIGFPE that
kills the root daemon. This runs on the worker thread inside `run_task`, triggered by a physical
button press, so it is reachable in normal operation, not just via the bus. Separately, `prev`
from `currentHost == 0` makes `next_host = -1`; `-1 % hostCount` is `-1` in C++ (sign follows the
dividend), so the guard `next_host != currentHost` passes and `setHost(static_cast<uint8_t>(-1))`
sends host `255` to the hardware. The intended wrap-around (prev from host 0 → last host) does not
happen.
**Fix:** Guard the count and use a non-negative modulo:
```cpp
auto host_info = self->_change_host->getHostInfo();
if (host_info.hostCount == 0)
    return;
int next_host = /* ... compute as before ... */;
int count = host_info.hostCount;
next_host = ((next_host % count) + count) % count;   // always in [0, count)
if (next_host != host_info.currentHost)
    self->_change_host->setHost(static_cast<uint8_t>(next_host));
```

## Warnings

### WR-01: `getClosestDPI` nearest-match comparator uses signed distance, not absolute distance

**File:** `src/logid/features/DPI.cpp:47-50`
**Issue:** In the non-range (discrete list) branch, the comparator is
`return (dpi - a) < (dpi - b);`. This is a *signed* difference, not `|dpi - a| < |dpi - b|`. For a
target below some candidates the differences go negative and the "closest" selection is wrong — it
effectively biases toward the largest candidate rather than the nearest. `SetDPI` from the new
Pointer-tab slider routes through here for discrete-DPI sensors, so the daemon can snap a requested
DPI to a visibly wrong preset. (Carried from upstream, but live on this phase's DPI write path.)
**Fix:**
```cpp
auto it = std::min_element(dpi_list.dpis.begin(), dpi_list.dpis.end(),
    [dpi](uint16_t a, uint16_t b) {
        return std::abs(dpi - a) < std::abs(dpi - b);
    });
```
Note `dpi`, `a`, `b` are `uint16_t`; promote to `int` before subtraction (they already promote in
an arithmetic context, but make it explicit to avoid an unsigned wrap: `std::abs(int(dpi) - int(a))`).

### WR-02: `DPI::configure` off-by-one when seeding the CycleDPI list (skips a sensor / truncates)

**File:** `src/logid/features/DPI.cpp:84-93`
**Issue:** For the list-of-DPIs config, `_fillDPILists(dpis.size() - 1)` is called and then the loop
increments `i` only when `dpi != 0`. If `dpis.size()` is `0` (an empty list reaching configure),
`dpis.size() - 1` underflows to `SIZE_MAX` cast into `uint8_t` (becomes 255), forcing 256 sensor-DPI
list reads — slow and wrong. More commonly, because `++i` is *inside* the `if (dpi != 0)` guard, a
`0` entry in the middle of the list shifts every subsequent DPI onto the wrong sensor index. The
index `i` should track list position, not "non-zero count."
**Fix:** Guard the empty list and advance `i` per element:
```cpp
const auto& dpis = std::get<std::list<int>>(config);
if (dpis.empty()) return;
_fillDPILists(dpis.size() - 1);
std::shared_lock dpi_lock(_dpi_list_mutex);
int i = 0;
for (const auto& dpi : dpis) {
    if (dpi != 0)
        _adjustable_dpi->setSensorDPI(i, getClosestDPI(_dpi_lists.at(i), dpi));
    ++i;                       // advance regardless, so sensor index stays aligned
}
```

### WR-03: CONF-01 dirty-tracking is not wired — feature setters never call `markDirty()`

**File:** `src/logiops-gui/DeviceController.cpp:244-418`, `src/logiops-gui/ButtonsModel.cpp:132-226`, `src/logiops-gui/ProfilesModel.cpp:96-149`
**Issue:** `ConfigState`'s contract (ConfigState.h:93-94, "Called by EVERY feature setter across the
tabs after a live-apply") is the spine of CONF-01: the "Unsaved changes" pill only appears if setters
mark the state dirty. But none of the C++ setters reference `ConfigState`, and the QML setter call
sites (e.g. ScrollTab.qml `onToggled: controller.setSmartShiftActive(...)`, PointerTab/DpiCycleEditor
preset mutators, ProfilesTab switch/create/rename) do not call `configState.markDirty()` either. As
written, a user can change DPI, toggle SmartShift, edit presets, and switch profiles with the dirty
pill never appearing — so Save looks unnecessary and unsaved live changes are silently lost on daemon
restart. Only `ConfigState::restoreDefaults` (via `onClearReplied`) sets dirty.
**Fix:** Either have each QML setter call `configState.markDirty()` alongside the controller/model
call, or (cleaner, less error-prone) inject `ConfigState*` into `DeviceController` / `ButtonsModel` /
`ProfilesModel` and call `markDirty()` at the end of each successful optimistic setter. Verify the
phase's test plan actually asserts the pill appears after a representative setter from each tab.

### WR-04: `seedDpiBounds` reads `min/max` over a discrete list but `_dpiStep` defaults to 0 — slider step becomes unusable

**File:** `src/logiops-gui/DeviceController.cpp:196-207`
**Issue:** For a discrete-DPI device (`isRange == false`), `_dpiStep` is set to `0` (line 203:
`isRange ? 50 : 0`). The Pointer-tab slider then falls back to `stepSize: 50` (PointerTab.qml:67-68),
but the device's discrete DPIs may not be multiples of 50 from `dpiMin`. The slider can therefore
emit values the daemon never offers; `getClosestDPI` snaps them, but combined with WR-01's broken
comparator the snapped value can be far from what the user dragged to. Minor on a range device,
confusing on a discrete one.
**Fix:** When `!isRange`, either disable free dragging and present the discrete set as ticks/options,
or compute the smallest gap between sorted discrete DPIs as the step. At minimum, document that
discrete devices are approximated and ensure WR-01 is fixed so the snap is at least *nearest*.

### WR-05: `ProfilesModel::renameProfile` leaves the model showing the new name even if the daemon create/remove fails

**File:** `src/logiops-gui/ProfilesModel.cpp:135-149`
**Issue:** Rename optimistically rewrites `_names[row] = newName` and emits `dataChanged` before the
async `performSetProfile(newName)` / `performRemoveProfile(oldName)` round-trips. `performSetProfile`
ends with `refresh()`, which re-reads `GetProfiles` and re-seeds — so a *failed* create (e.g. polkit
or daemon error) will momentarily show `newName` and then snap back when refresh returns the real
list, but `performRemoveProfile(oldName)` was *also* fired unconditionally. If the create failed but
the old profile still exists, the subsequent remove can delete `oldName` while `newName` was never
created — net data loss of a profile. The two writes are not sequenced (remove does not wait for
create's reply).
**Fix:** Chain `performRemoveProfile(oldName)` inside the `performSetProfile` reply handler, and only
when that reply is non-error and confirms `newName` now exists. Reuse the same
`QDBusPendingCallWatcher::finished` sequencing pattern already used in `ButtonsModel`'s two-step
dispatch.

## Info

### IN-01: `getHostCount()` performs synchronous hardware I/O on a D-Bus getter

**File:** `src/logid/actions/ChangeHostAction.cpp:71-75`
**Issue:** `getHostCount` calls `_change_host->getHostInfo()`, which issues a HID++ request to the
device every time the property is read. The GUI reads it during `enumerate()` (ButtonsModel.cpp:369)
synchronously (`ch.call(...)`). A sleeping/slow device makes that getter block the caller. It is
read-only and bounded, so not a correctness bug, but consider caching `_host_count` (the feature
already stores one) and returning the cached value.

### IN-02: `ButtonsModel::enumerate` makes synchronous blocking D-Bus calls in the async reply handler

**File:** `src/logiops-gui/ButtonsModel.cpp:331-371`
**Issue:** Inside the async `Enumerate` reply, per-button `btn->property(...)`, `props.call("GetAll", ...)`
(looped over every action type), and the `GetHostCount` call are all *synchronous* blocking calls on the
bus. The comment claims these are "cheap cached daemon properties," but `GetAll` on each `.Action.<X>`
interface and `GetHostCount` (see IN-01) can hit the wire. For a device with many buttons this serializes
N×(action-types) blocking round-trips on the UI thread — the exact UI-stall the rest of the code is
careful to avoid. Consider batching/async-ing the readback.

### IN-03: `getClosestDPI` range branch can index empty `dpis` via `min_element`/`max_element`

**File:** `src/logid/features/DPI.cpp:31-32`
**Issue:** `*std::min_element(...)` / `*std::max_element(...)` dereference the result without checking
`dpis.empty()`. A device returning an empty range list would dereference `end()`. Unlikely from real
hardware, but it is untrusted device data feeding a root process; add an `empty()` guard returning the
requested `dpi` unchanged.

### IN-04: `DeviceController::setDpi` ignores its own `_clampDpi` helper

**File:** `src/logiops-gui/DeviceController.cpp:244-263`
**Issue:** The preset mutators clamp via `_clampDpi` (defense-in-depth, T-3-03-01), but the primary
`setDpi` path hands the raw slider value straight to `SetDPI` without clamping. The daemon snaps, so it
is safe, but it is inconsistent with the documented clamp-before-send intent and means the slider and the
preset editor treat bounds differently. Apply `_clampDpi(dpi)` in `setDpi` for parity.

### IN-05: `isAuthDenied` substring match on "authoriz" is broad

**File:** `src/logiops-gui/ConfigState.cpp:41-46`
**Issue:** Mapping any error whose message merely *contains* "authoriz" to the polkit-declined string can
misclassify unrelated daemon errors (e.g. a message like "unauthorized config key"). Low impact (only
selects which of two user-facing strings shows), but prefer matching on the D-Bus error *name*
(`org.freedesktop.DBus.Error.AccessDenied`, polkit's `…NotAuthorized`) rather than free-text message
substrings.

### IN-06: `DeviceRender` hotspot positions are synthesized, can overlap / run off-canvas

**File:** `src/logiops-gui/qml/config/DeviceRender.qml:83-87`
**Issue:** `fy: 0.18 + (index * 0.12)` then `Math.min(fy, 0.9)` clamps every hotspot beyond index ~6 to
`y = 0.9`, stacking them on top of each other (overlapping 44px hit areas → ambiguous clicks). This is an
acknowledged placeholder ("a real artwork pack overrides this"), so it is informational, but a device with
>6 enumerated buttons will have unclickable/overlapping hotspots today. Consider a simple wrap/grid layout
until per-PID artwork exists.

---

_Reviewed: 2026-05-30_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
