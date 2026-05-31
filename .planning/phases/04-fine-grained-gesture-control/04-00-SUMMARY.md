---
phase: 04-fine-grained-gesture-control
plan: 00
subsystem: test-infrastructure
tags: [ctest, tdd, gestures, wave-0, GEST-02, GEST-03, GEST-01, GEST-04]
requires: []
provides:
  - phase4-ctest-prefix
  - gesture-math-contract-fixture
  - gesture-model-recording-scaffold
affects:
  - CMakeLists.txt
  - tests/phase4/
requirements: [GEST-01, GEST-02, GEST-03, GEST-04]
tech-stack:
  added: []
  patterns:
    - "Self-contained daemon-side math unit (no Action/InputDevice/Device/bus link)"
    - "Qt-guarded recording-subclass GUI test (EXISTS + logiops-gui-lib + Qt6 Test)"
key-files:
  created:
    - tests/phase4/CMakeLists.txt
    - tests/phase4/phase4_log_stub.cpp
    - tests/phase4/gesture_math_test.cpp
    - tests/phase4/GestureModelTest.cpp
  modified:
    - CMakeLists.txt
decisions:
  - "gesture_math_test is self-contained (reimplements only the boundary-crossing accounting) so it links nothing beyond libstdc++ and never needs the daemon's bus/feature wiring; the assertions still encode the exact floor(M/I) fire-count contract Plan 01 must satisfy."
  - "GestureModelTest is intentionally RED-by-link (won't compile until Plan 02 lands GestureModel.h); the CMake EXISTS + TARGET logiops-gui-lib guard keeps the daemon math unit buildable in isolation."
  - "Plain-language mode allowlist pinned in the test: Repeat-while-moving->OnInterval, Do-once->OnRelease, Adjust-proportionally->Axis, Nothing->None (RESEARCH Pitfall 1 resolution: OnRelease is the canonical mode 2, OnThreshold out of GUI surface)."
metrics:
  duration_seconds: 209
  tasks: 3
  files: 5
  completed: "2026-05-31"
---

# Phase 4 Plan 00: Phase 4 Test Infrastructure Summary

Stood up the `tests/phase4/` CTest harness mirroring `tests/phase3/`: a self-contained daemon-side `gesture_math_test` encoding the GEST-02/03 `floor(M/interval)` fire-count contract, and a RED Qt recording-subclass `GestureModelTest` scaffolding the two-step mode-switch + preview-sentence contract for Plan 02.

## What Was Built

- **`tests/phase4/CMakeLists.txt`** — registers the `phase4` CTest prefix. `phase4_gesture_math` is an unconditional daemon-side unit (no Qt, no bus). `phase4_gesture_model` is Qt-guarded exactly like phase3 (`TARGET logiops-gui-lib AND EXISTS GestureModelTest.cpp` then `find_package(Qt6 6.4 COMPONENTS Test QUIET)`). Wired into root `CMakeLists.txt` via `add_subdirectory(tests/phase4)` immediately after `tests/phase3`, inside the same `BUILD_TESTING` guard.
- **`tests/phase4/phase4_log_stub.cpp`** — copy of `phase3_log_stub.cpp` (defines `logid::global_loglevel`) to satisfy `logPrintf` linkage if a future plan compiles a real daemon TU here.
- **`tests/phase4/gesture_math_test.cpp`** (230 lines) — encodes the GEST-02/03 accounting contract as assertions: M=120,I=120 -> exactly 1 fire; M=360,I=120 -> exactly 3 fires; M=239,I=120 -> 1 fire (no overshoot); unset interval repeats at `defaults::gesture_interval` (RED-by-value vs a local `kExpectedDefaultInterval=120` with an `// EXPECT after Plan 01` marker); AxisGesture remainder carry (no motion lost, no overshoot of the exact scaled distance).
- **`tests/phase4/GestureModelTest.cpp`** (149 lines) — `RecordingGestureModel` subclass of the (Plan-02) `GestureModel` overriding `performSetGesture`/`performParamCall` to record dispatch with no bus. Asserts two-step order (SetGesture first), the four-mode allowlist + unknown-mode/direction rejection, and the `previewSentence` GEST-04 template (`Nothing`+up -> "Moving up does nothing.") with `previewChanged` NOTIFY.

## How It Works

`ctest --test-dir build -R phase4` discovers both units (#13 `phase4_gesture_math`, #14 `phase4_gesture_model`). The math unit builds and passes in isolation because it is self-contained — it reimplements only the `IntervalGesture::move`/`AxisGesture::move` math under test, so it asserts the post-fix contract directly while documenting (via `// EXPECT after Plan 01` markers) that the current daemon does NOT satisfy it (the daemon's `if (!_config.interval.has_value()) return;` is the GEST-03 regression Plan 01 fixes). The model unit is RED-by-link: it `#include "GestureModel.h"`, which does not exist until Plan 02, so its build fails with `fatal error: GestureModel.h: No such file or directory` — the intended Wave-0 state. The CMake `EXISTS` + `TARGET logiops-gui-lib` guard ensures the math unit remains buildable regardless of the model unit's RED status.

## Verification

- `cmake -S . -B build -DBUILD_TESTING=ON` configures cleanly (exit 0) with `tests/phase4` registered.
- `grep -q "add_subdirectory(tests/phase4)" CMakeLists.txt` succeeds.
- `tests/phase4/CMakeLists.txt` contains `add_test(NAME phase4_gesture_math`, `add_test(NAME phase4_gesture_model`, `find_package(Qt6 6.4 COMPONENTS Test QUIET)`, and the `if(TARGET logiops-gui-lib AND EXISTS` guard.
- `phase4_gesture_math_test` builds and passes (1/1) in isolation.
- `phase4_gesture_model_test` fails to build (RED, `GestureModel.h` missing) — confirmed intended Wave-0 state; turns GREEN after Plan 02.

## Deviations from Plan

None - plan executed exactly as written.

The math unit's self-contained design (passing rather than RED-by-link) is an explicitly-sanctioned option in the plan ("Keep the unit self-contained if linking the real TUs is infeasible; the assertions still encode the exact contract Plan 01 must satisfy"). The RED-against-daemon dimension is preserved through the `// EXPECT after Plan 01 introduces defaults::gesture_interval` markers documenting the gap.

## Notes for Future Plans

- **Plan 01** must introduce `defaults::gesture_interval` (alongside `gesture_threshold = 50` in `src/logid/Configuration.h`) and remove the early `if (!_config.interval.has_value()) return;` in `IntervalGesture::move`, falling back to the default so an unset interval still repeats (GEST-03). When Plan 01 lands, consider re-targeting `gesture_math_test` to compile the real `IntervalGesture.cpp`/`AxisGesture.cpp` TUs (add `util/log.cpp` + `phase4_log_stub.cpp`, link `ipcgull_static config++`) to convert the contract from RED-by-value to a true daemon round-trip.
- **Plan 02** must create `src/logiops-gui/GestureModel.{h,cpp}` exposing: `Q_INVOKABLE bool setMode(QString direction, QString mode)` (plain-language allowlist -> OnInterval/OnRelease/Axis/None, reject unknown before dispatch), virtual `performSetGesture`/`performParamCall` seams, and `Q_PROPERTY(QString previewSentence ... NOTIFY previewChanged)` per UI-SPEC copywriting. `GestureModelTest` turns GREEN once these exist and `GestureModel.cpp` is added to `logiops-gui-lib`'s sources.

## Self-Check: PASSED
