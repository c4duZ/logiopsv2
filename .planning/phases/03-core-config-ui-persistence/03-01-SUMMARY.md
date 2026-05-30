---
phase: 03-core-config-ui-persistence
plan: 01
subsystem: gui
tags: [qt6, qml, dbus, introspect, capability-gating, tabbar, theme, svg, ctest]

# Dependency graph
requires:
  - phase: 03-core-config-ui-persistence
    plan: 00
    provides: "seven typed D-Bus proxies (.Config/.DPI/.SmartShift/.HiresScroll/.ThumbWheel/.Buttons/.Button) + phase3 CTest harness under tests/phase3/"
  - phase: 02-d-bus-client-device-list
    provides: "logiops-gui-lib + DeviceModel + DaemonConnection + Theme.qml shell + qt_add_qml_module pattern"
provides:
  - "DeviceController: per-device QObject that introspects the daemon for the present feature-interface set, lazily builds typed proxies, and exposes capability flags (hasSmartShift/hasHires/hasThumbwheel/hasButtons/hasDpi/hasTorque, dpiMin/dpiMax/dpiStep/sensorCount/hostCount) + live-value Q_PROPERTYs with optimistic setters"
  - "DeviceControllerFactory: owns + swaps the per-selected-device DeviceController, registered as the deviceControllerFactory QML context property"
  - "Tabbed Options+-style DetailPane (header + sliding TabBar + cross-fading ConfigTabs StackLayout) with whole-tab capability omission (UI-01)"
  - "ConfigTabs host + four placeholder tab files (ButtonsTab/PointerTab/ScrollTab/ProfilesTab) for Wave 2 to fill"
  - "All Phase 3 Theme layout/color tokens + the twelve config SVG glyphs bundled via qrc"
affects: [03-02, 03-03, 03-04]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Capability gating via org.freedesktop.DBus.Introspectable.Introspect() -> parse present-interface set -> build proxies only for present interfaces (mirrors daemon's present-or-absent _addFeature model; T-3-01-03)"
    - "All DeviceController reads/writes async via QDBusPendingCallWatcher; QML never blocks (T-3-01-02)"
    - "Optimistic setters: emit the change signal immediately (idempotent guard), then fire the async D-Bus setter (live-apply)"
    - "Test seam: applyIntrospection() + parseInterfaces() + seed* entry points drive the controller bus-free (Phase 2 DeviceModelTest precedent)"
    - "Single tabKeys list in DetailPane is the one source both the TabBar and the ConfigTabs StackLayout index, so omitted tabs keep indices in lock-step"

key-files:
  created:
    - src/logiops-gui/DeviceController.h
    - src/logiops-gui/DeviceController.cpp
    - src/logiops-gui/DeviceControllerFactory.h
    - src/logiops-gui/DeviceControllerFactory.cpp
    - src/logiops-gui/qml/config/ConfigTabs.qml
    - src/logiops-gui/qml/config/ButtonsTab.qml
    - src/logiops-gui/qml/config/PointerTab.qml
    - src/logiops-gui/qml/config/ScrollTab.qml
    - src/logiops-gui/qml/config/ProfilesTab.qml
    - tests/phase3/DeviceControllerTest.cpp
    - src/logiops-gui/icons/keystroke.svg
    - src/logiops-gui/icons/dpi.svg
    - src/logiops-gui/icons/host.svg
    - src/logiops-gui/icons/profile.svg
    - src/logiops-gui/icons/smartshift.svg
    - src/logiops-gui/icons/hires.svg
    - src/logiops-gui/icons/thumbwheel.svg
    - src/logiops-gui/icons/disabled.svg
    - src/logiops-gui/icons/save.svg
    - src/logiops-gui/icons/restore.svg
    - src/logiops-gui/icons/add.svg
    - src/logiops-gui/icons/remove.svg
  modified:
    - src/logiops-gui/qml/DetailPane.qml
    - src/logiops-gui/qml/Theme.qml
    - src/logiops-gui/main.cpp
    - src/logiops-gui/CMakeLists.txt
    - tests/phase3/CMakeLists.txt

key-decisions:
  - "Added a DeviceControllerFactory (deviceControllerFactory context property) to own/swap the per-device DeviceController on selectDevice(path), instead of registering a bare DeviceController in main.cpp — the controller needs a path+bus at construction and the sidebar selection changes at runtime, so a factory keeps main.cpp's registration a single context property while still building a fresh capability-discovering controller per device (matches the plan's 'created/swapped when the sidebar selection changes' model)"
  - "DeviceController is NOT `final` (the plan's test seam asks for a virtual introspectInterfaces()/parseInterfaces() override point); kept other leaf semantics but dropped final to honor the overridable seam"
  - "Whole-tab omission computed once as DetailPane.tabKeys and passed down to ConfigTabs so the TabBar and the StackLayout share one index space (UI-SPEC: omit, never show-disabled)"
  - "hostCount kept at default 3 behind a Q_PROPERTY + setHostCount() (NOT exposed over D-Bus yet) so Plan 02 can wire the real value without reshaping the controller"

patterns-established:
  - "Per-device controller introspect-then-build-proxies; Wave 2 tabs bind to deviceControllerFactory.controller capability flags + live values"
  - "config/ tab QML files live in the same logiops.gui module (no extra import); placeholders are the Wave-2 replacement points"

requirements-completed: [UI-01, SCR-01, SCR-02, SCR-03, DPI-01]

# Metrics
duration: 8min
completed: 2026-05-30
---

# Phase 3 Plan 01: Config-UI Foundation (DeviceController + Tabbed Shell) Summary

**Built the Phase 3 GUI spine: a per-device `DeviceController` that discovers each device's capabilities + live config over the Plan-00 typed proxies via D-Bus Introspect, and the Options+-style tabbed detail pane (header + sliding TabBar + cross-fading StackLayout) with whole-tab capability omission — plus every Phase 3 Theme token and the twelve config glyphs Wave 2 references.**

## Performance

- **Duration:** ~8 min
- **Started:** 2026-05-30T23:05Z
- **Completed:** 2026-05-30T23:13Z
- **Tasks:** 3
- **Files modified:** 27 (22 created, 5 modified)

## Accomplishments
- **DeviceController (capability + live-value bridge):** async `Introspect()` on the device node → parse present-interface set → lazily build only the present typed proxies (DPI/SmartShift/Hires/ThumbWheel/Buttons), exactly mirroring the daemon's present-or-absent `_addFeature` model (T-3-01-03). Capability Q_PROPERTYs (`hasSmartShift/hasHires/hasThumbwheel/hasButtons/hasDpi/hasTorque`, `dpiMin/dpiMax/dpiStep/sensorCount`, `hostCount` default 3) + live-value Q_PROPERTYs with optimistic `Q_INVOKABLE` setters. All reads/writes async via `QDBusPendingCallWatcher` — the UI thread never blocks (T-3-01-02).
- **DeviceControllerTest GREEN:** asserts (a) absent interface → `hasX==false`, (b) present DPI with isRange dpis `{400,8000}` → `dpiMin==400`/`dpiMax==8000`, (c) `hasTorque` reflects the injected TorqueSupport read, plus XML parse, hostCount default, and the optimistic-setter signal contract — all bus-free via the construction seams.
- **Theme tokens + twelve glyphs:** added the UI-SPEC Phase 3 layout tokens (`tabBarHeight 48`, `controlRowHeight 40`, `actionPanelWidth 360/min 320`, `deviceRenderMax 420`, `hotspotMin 32`, `dialogMaxWidth 440`) and the `destructive`/`destructiveTint` color pair — with NO new typography (4 sizes / 2 weights preserved). Twelve 24×24 monochrome single-path SVG glyphs in the existing `mouse.svg` style, all bundled via qrc.
- **Tabbed DetailPane (UI-01):** persistent header (icon/name/model + live connection-battery chip) above a sliding `TabBar` (2px accent underline animated `motionBase`/`OutCubic`; selected label accent DemiBold, unselected muted→foreground on hover) hosting a cross-fading `ConfigTabs` StackLayout. Whole-tab omission gated on controller capabilities (Buttons↔hasButtons, Pointer↔hasDpi, Scroll↔smartshift‖hires‖thumbwheel). Four placeholder tab bodies for Wave 2.
- Full suite **9/9 CTest GREEN**; daemon + GUI build clean (no `-Wall -Wextra` regressions).

## Task Commits

1. **Task 1: DeviceController capability + live-value bridge + unit test** - `1ebf150` (feat)
2. **Task 2: Phase 3 Theme tokens + twelve config glyphs** - `ce10c13` (feat)
3. **Task 3: Tabbed DetailPane shell + ConfigTabs host + controller registration (UI-01)** - `af2c18e` (feat)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added a DeviceControllerFactory for per-device controller swap**
- **Found during:** Task 3
- **Issue:** The plan says "register a DeviceController per selected device as a QML context property in main.cpp (created/swapped when the sidebar selection changes)". A `DeviceController` needs a device path + bus at construction, but main.cpp registers context properties once at startup before any selection exists, and the sidebar selection changes at runtime in QML — there is no single DeviceController to register.
- **Fix:** Added `DeviceControllerFactory` (registered once as `deviceControllerFactory`) that owns the current `DeviceController` and rebuilds it on `selectDevice(path)`, exposing it via a `controller` Q_PROPERTY. QML calls `selectDevice` on selection change and binds the shell to `deviceControllerFactory.controller`. Net effect is exactly the plan's "created/swapped per selection" model with a single context-property registration.
- **Files modified:** src/logiops-gui/DeviceControllerFactory.{h,cpp}, src/logiops-gui/main.cpp, src/logiops-gui/CMakeLists.txt
- **Verification:** GUI builds clean; `grep DeviceController src/logiops-gui/main.cpp` satisfied; shell renders the controller-bound tabs.
- **Committed in:** `af2c18e`

**2. [Rule 3 - Blocking] Dropped `final` from DeviceController to honor the test seam**
- **Found during:** Task 1
- **Issue:** The plan asks for a "protected/virtual `introspectInterfaces()`" test seam, which is incompatible with the class being `final` (the plan's frontmatter/convention leans `final` on leaf classes).
- **Fix:** Declared `introspectInterfaces()` and `parseInterfaces()` as `virtual` and left `DeviceController` non-`final` so a test/subclass can override the present-set source without a bus. The shipped test drives it via the public `applyIntrospection`/`seed*` seams (no override needed), but the override point exists per the plan.
- **Files modified:** src/logiops-gui/DeviceController.h
- **Verification:** phase3_device_controller GREEN.
- **Committed in:** `1ebf150`

---

**Total deviations:** 2 auto-fixed (both Rule 3 - blocking). No scope change; both were required to land the planned artifacts.

## Known Stubs

The four `config/*Tab.qml` files (`ButtonsTab`, `PointerTab`, `ScrollTab`, `ProfilesTab`) are **intentional placeholders** — Plan 01's deliverable is the tab SHELL + capability spine; the real tab content is the explicit deliverable of the Wave-2 plans (02 Buttons, 03 Pointer+Scroll, 04 Profiles). They render their tab name centered so the shell compiles and renders standalone. This is by design per the plan ("ConfigTabs loads four placeholder Items"), not a gap that blocks this plan's goal (the UI-01 frame + gating spine are complete and live).

## Verification: Build vs. Deferred

Verified by **actual build + test execution** on this machine (Qt 6.4.2):
- `logiops-gui-lib` + `logiops-gui` build clean (no errors/warnings).
- `phase3_device_controller` GREEN; full suite **9/9 GREEN** (no regressions).
- All acceptance greps pass (TabBar / StackLayout / motionBase+OutCubic / DeviceController registration / four tab labels / twelve glyphs in CMake / Theme tokens / typography count unchanged).
- qmllint on the new QML produces only import-path-resolution noise (`Connections`/`sourceSize`/`easing` "not resolved") identical in class + comparable count to the Phase 2 `Main.qml` baseline under the same standalone invocation — not real defects (the module build resolves them).

**Deferred to hardware / human verification (on-device, QML rendering correctness):**
- Visual: the sliding 2px accent underline animation, tab cross-fade, header layout, and glyph rendering at 24×24 — needs the GUI run against a live daemon + a real device (requires `qml6-module-qtquick` per Phase 2 UAT note).
- Live capability gating: that a device genuinely lacking SmartShift/Hires/Thumbwheel omits the Scroll tab, and a keyboard (no DPI) omits Pointer — needs a live daemon Introspect against real hardware. The logic is unit-covered with injected fakes; the live Introspect round-trip is the on-hardware item.
- Live-apply: optimistic setters reaching the daemon (DPI/SmartShift/Hires/Thumb) — Wave 2 wires the actual controls; this plan only ships the controller + setters.

## Next Phase Readiness
- Wave 2 (Plans 02/03/04) bind directly to `deviceControllerFactory.controller`: capability flags gate sections/tabs, live-value Q_PROPERTYs two-way-bind to sliders/toggles, and the four `config/*Tab.qml` placeholders are the drop-in points.
- All Phase 3 Theme tokens + glyphs are in place — the tab plans reference them without re-declaring.
- The capability-discovery + tab-structure spine is built once, so the three Wave-2 executors do not each reinvent it.

---
*Phase: 03-core-config-ui-persistence*
*Completed: 2026-05-30*

## Self-Check: PASSED

All 23 created files present on disk; all three task commits (`1ebf150`, `ce10c13`, `af2c18e`) exist in git history. Full CTest suite 9/9 GREEN.
