# Phase 04 — Deferred Items

Out-of-scope discoveries logged during execution. NOT fixed here.

## From 04-01 (daemon fix/harden wave)

- **`phase4_gesture_model` test does not build/run** — `tests/phase4/GestureModelTest.cpp`
  includes `GestureModel.h`, which does not exist yet. This is a Wave-0 RED scaffold for the
  GUI `GestureModel` class, which is built by a later GUI plan (Plan 02 per RESEARCH §Recommended
  structure). Not caused by Plan 01's daemon changes (Plan 01 touches only the C++ daemon, no GUI).
  Resolution: the GUI plan that introduces `src/logiops-gui/GestureModel.{h,cpp}` turns this GREEN.
