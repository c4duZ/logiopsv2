---
phase: 03-core-config-ui-persistence
plan: 00
subsystem: testing
tags: [ctest, qtest, libconfig, qdbusxml2cpp, dbus, qt6, evdev, ipcgull]

# Dependency graph
requires:
  - phase: 01-access-path-daemon-hardening
    provides: "polkit-gated Configuration::save() + config::set/get round-trip serialization machinery"
  - phase: 02-d-bus-client-device-list
    provides: "logiops-gui-lib + qt_add_dbus_interface proxy pattern + trimmed introspection XML style"
provides:
  - "BLOCKING CONF-01 proof: Configuration::save() serialization round-trips device structure + values with no loss (phase3_save_roundtrip)"
  - "Unknown-key resilience proof: schema-unknown cfg keys are dropped, never crash save() (phase3_save_unknownkey, T-3-00-02)"
  - "KeyNameMapper: Qt key/modifier -> evdev KEY_* string mapping for the BTN-02 key-capture widget"
  - "Seven typed D-Bus proxies covering the full Phase 3 config bus surface (.Config/.DPI/.SmartShift/.HiresScroll/.ThumbWheel/.Buttons/.Button)"
  - "phase3 CTest suite wired under tests/phase3/ (3 units, all GREEN)"
affects: [03-01, 03-02, 03-03, 03-04, gestures, profiles]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Daemon config round-trip tested without a live bus/polkit by compiling config/config.cpp directly + linking ipcgull_static + config++ on a temp cfg file"
    - "Typed proxies for stable interfaces via qt_add_dbus_interface; ai array out-arg needs QtTypeName.Out0=QList<int> annotation"
    - "Algorithmic letter/digit/F-key KEY_* mapping + table for everything else, returning empty for unknown/Escape (never a guessed token)"

key-files:
  created:
    - tests/phase3/save_roundtrip_test.cpp
    - tests/phase3/save_unknownkey_test.cpp
    - tests/phase3/KeyNameMapperTest.cpp
    - tests/phase3/phase3_log_stub.cpp
    - tests/phase3/CMakeLists.txt
    - src/logiops-gui/KeyNameMapper.h
    - src/logiops-gui/KeyNameMapper.cpp
    - src/logiops-gui/dbus/logid_config.xml
    - src/logiops-gui/dbus/logid_dpi.xml
    - src/logiops-gui/dbus/logid_smartshift.xml
    - src/logiops-gui/dbus/logid_hiresscroll.xml
    - src/logiops-gui/dbus/logid_thumbwheel.xml
    - src/logiops-gui/dbus/logid_buttons.xml
    - src/logiops-gui/dbus/logid_button.xml
  modified:
    - CMakeLists.txt
    - src/logiops-gui/CMakeLists.txt

key-decisions:
  - "Created tests/phase3/ (new dir, per plan) rather than reusing test/; root CMakeLists adds it under BUILD_TESTING"
  - "Round-trip test exercises config::set+writeFile directly (the exact code save() runs post-polkit), never the polkit-gated Save() and never /etc/logid.cfg"
  - "Bare modifiers (Ctrl/Alt/Shift/Meta) map to LEFT* evdev variants; F1-F24 and letters/digits mapped algorithmically; Escape and unknown -> empty"

patterns-established:
  - "Daemon-side config tests link ipcgull_static + config++ and compile config.cpp + a tiny global_loglevel stub (avoids dragging in logid main / evdev / udev / polkit)"
  - "New D-Bus introspection XML keeps comment bodies free of stray -- and < so qdbusxml2cpp emits non-empty proxies (Phase 2 pitfall 7)"

requirements-completed: [CONF-01, BTN-02]

# Metrics
duration: 66min
completed: 2026-05-30
---

# Phase 3 Plan 00: Wave-0 Test Infrastructure & Bus Surface Summary

**Proved the BLOCKING CONF-01 assumption — Configuration::save() serialization round-trips device structure and values with no loss — and landed the KeyNameMapper + seven typed D-Bus proxies that make Wave 1/2 pure assembly.**

## Performance

- **Duration:** ~66 min
- **Started:** 2026-05-30T22:51Z
- **Completed:** 2026-05-30T23:02Z
- **Tasks:** 3
- **Files modified:** 16 (14 created, 2 modified)

## Accomplishments
- **CONF-01 BLOCKING gate satisfied:** an automated test loads a 2-device fixture, mutates a DPI value and a smartshift threshold in the in-memory tree, serializes via the exact `config::set` + `writeFile` path `save()` runs after the polkit gate, reloads, and asserts device-block count preserved + mutated values applied + every untouched value (including the whole other device) unchanged.
- **Unknown-key resilience proven** (T-3-00-02): a cfg with a top-level `frobnicate` key and a per-device unsupported key loads, serializes, and reloads with no crash; schema-known values survive.
- **KeyNameMapper (BTN-02):** Qt key/modifier codes map to the exact libevdev `KEY_*` names the daemon's `SetKeys`/`toKeyCode` accepts; Escape and unknown keys return empty (never a guessed token, T-3-00-03).
- **Seven typed D-Bus proxies generated and compiled** into `logiops-gui-lib`, covering the entire Phase 3 config bus surface — Wave 1/2 need no live introspection.
- Full suite GREEN: 8/8 CTest (5 pre-existing + 3 new); daemon and GUI lib build clean.

## Task Commits

1. **Task 1: BLOCKING save() round-trip + unknown-key CTest (CONF-01)** - `542b875` (test)
2. **Task 2: KeyNameMapper (Qt key/modifier -> evdev KEY_*) + unit test (BTN-02)** - `a77bf0e` (feat)
3. **Task 3: Trimmed D-Bus introspection XML + typed proxy generation** - `7aa5b79` (feat)

_Note: Tasks 1 and 2 are TDD; the save() write path and the mapper were implemented green in single commits since the daemon serialization already existed and the mapper was net-new with its test._

## Files Created/Modified
- `tests/phase3/save_roundtrip_test.cpp` - CONF-01 round-trip fidelity assertions
- `tests/phase3/save_unknownkey_test.cpp` - unknown-key no-crash assertions
- `tests/phase3/KeyNameMapperTest.cpp` - QtTest for the Qt->evdev mapping
- `tests/phase3/phase3_log_stub.cpp` - provides `logid::global_loglevel` (lives in logid.cpp, not linked)
- `tests/phase3/CMakeLists.txt` - three CTest units; round-trip units compile config.cpp + link ipcgull_static + config++
- `src/logiops-gui/KeyNameMapper.{h,cpp}` - the mapper (added to logiops-gui-lib)
- `src/logiops-gui/dbus/logid_{config,dpi,smartshift,hiresscroll,thumbwheel,buttons,button}.xml` - trimmed introspection
- `CMakeLists.txt` - `add_subdirectory(tests/phase3)` under BUILD_TESTING
- `src/logiops-gui/CMakeLists.txt` - +7 `qt_add_dbus_interface` (baseline 2 -> 9), KeyNameMapper.cpp source

## Decisions Made
- Round-trip test calls `config::set(root, cfg)` + `writeFile` directly (faithful to `save()` post-polkit) instead of `Save()`, so no live bus/polkit and no `/etc/logid.cfg` access — keeps the test hermetic and privilege-free (T-3-00-04 = accept).
- Bare modifiers map to LEFT* evdev variants; letters/digits/F-keys mapped algorithmically, everything else via a libevdev-verified table.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Provided `logid::global_loglevel` for the round-trip units**
- **Found during:** Task 1
- **Issue:** Linking `config.cpp` + `log.cpp` failed — `global_loglevel` is defined in `logid.cpp` (the daemon main), which the test must not pull in (it drags evdev/udev/polkit and `main`).
- **Fix:** Added `tests/phase3/phase3_log_stub.cpp` defining `logid::global_loglevel = WARN` — the single daemon symbol the config TU needs beyond config.cpp/log.cpp.
- **Files modified:** tests/phase3/phase3_log_stub.cpp, tests/phase3/CMakeLists.txt
- **Verification:** Both round-trip units link and pass.
- **Committed in:** `542b875`

**2. [Rule 3 - Blocking] Added QtTypeName.Out0 annotation for the DPI `ai` out-arg**
- **Found during:** Task 3
- **Issue:** `qdbusxml2cpp` errored on `ai` (array of int32) in `logid_dpi.xml`: "Got unknown type `ai`" — it cannot infer the Qt C++ container for `ai`.
- **Fix:** Added `<annotation name="org.qtproject.QtDBus.QtTypeName.Out0" value="QList&lt;int&gt;"/>` for `GetDPIs`' `dpis` out-arg.
- **Files modified:** src/logiops-gui/dbus/logid_dpi.xml
- **Verification:** All seven proxies generate non-empty and compile into logiops-gui-lib.
- **Committed in:** `7aa5b79`

---

**Total deviations:** 2 auto-fixed (both Rule 3 - blocking)
**Impact on plan:** Both fixes were required to build the planned artifacts; no scope change.

## Issues Encountered
- The plan specified `tests/phase3/` while the repo's existing tests live in `test/` (singular). Followed the plan's path (`tests/phase3/`) verbatim — it satisfies both the plan's grep acceptance (`add_subdirectory(tests/phase3)`) and keeps the new Wave-0 suite isolated from the Phase 1/2 `test/` dir. Both subdirs coexist under `enable_testing()`.

## Verification: Build vs. Deferred

All acceptance criteria for this plan were verified by **actual build + test execution** on this machine (Qt 6.4.2, libconfig++ 1.5, qdbusxml2cpp present):
- `phase3_save_roundtrip`, `phase3_save_unknownkey`, `phase3_keyname_mapper` — all GREEN via `ctest`.
- Seven typed proxies generated (non-empty headers: button=10, smartshift=8, thumbwheel=8, dpi=6, hiresscroll=6, config=2, buttons=2 member decls) and compiled clean.
- Daemon (`logid`) and `logiops-gui-lib` build with no `-Wall -Wextra` regressions.

**Deferred to hardware/environment:** none for this plan. (Live-apply behavior, the polkit Save() prompt, and visual UI are Wave 1/2 / on-hardware concerns, not Wave 0.) The round-trip test deliberately does NOT exercise the polkit-gated `Save()` D-Bus method (needs a live caller); it exercises the identical post-gate serialization path, which is the load-bearing fidelity claim.

## Next Phase Readiness
- The persistence path is de-risked: Wave 1/2 can rely on `Save()` round-tripping structure/values (comment/formatting loss remains expected and accepted per CONTEXT; recommend a one-line Save warning in Plan 04 per research A5).
- The full Phase 3 typed bus surface is available to controllers/models with no live introspection needed.
- KeyNameMapper is ready for the BTN-02 key-capture widget.

---
*Phase: 03-core-config-ui-persistence*
*Completed: 2026-05-30*

## Self-Check: PASSED

All 14 created files present on disk; all three task commits (`542b875`, `a77bf0e`, `7aa5b79`) exist in git history. Full CTest suite 8/8 GREEN.
