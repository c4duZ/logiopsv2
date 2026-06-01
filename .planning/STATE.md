---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: executing
stopped_at: Completed 04.1-03-PLAN.md
last_updated: "2026-06-01T03:45:39.877Z"
last_activity: 2026-06-01
progress:
  total_phases: 11
  completed_phases: 4
  total_plans: 46
  completed_plans: 23
  percent: 50
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-30)

**Core value:** Bring the full Logitech Options+ experience to Linux — configure a Logitech device through a real GUI instead of hand-editing `/etc/logid.cfg`.
**Current focus:** Phase 04.1 — options-plus-reference-mining

## Current Position

Phase: 04.1 (options-plus-reference-mining) — EXECUTING
Plan: 4 of 4
Status: Ready to execute
Last activity: 2026-06-01

Progress: [███████▌░░] 75%

## Performance Metrics

**Velocity:**

- Total plans completed: 15
- Average duration: — min
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 6 | - | - |
| 2 | 5 | - | - |
| 4 | 4 | - | - |

**Recent Trend:**

- Last 5 plans: —
- Trend: —

*Updated after each plan completion*
| Phase 1 P01 | 3 | 3 tasks | 7 files |
| Phase 01 P02 | 1 | 1 tasks | 1 files |
| Phase 01 P03 | 4 | 1 tasks | 1 files |
| Phase 01 P04 | 12 | 3 tasks | 7 files |
| Phase 01 P05 | 5 | 2 tasks | 2 files |
| Phase 01-access-path-daemon-hardening P06 | 3 | 3 tasks | 4 files |
| Phase 02-d-bus-client-device-list P02 | 25 | 2 tasks | 8 files |
| Phase 02 P03 | 18 | 2 tasks | 5 files |
| Phase 02-d-bus-client-device-list P04 | 40 | 2 tasks | 15 files |
| Phase 02-d-bus-client-device-list P05 | 18 | 2 tasks | 9 files |
| Phase 03-core-config-ui-persistence P00 | 66 | 3 tasks | 16 files |
| Phase 03-core-config-ui-persistence P01 | 8 | 3 tasks | 27 files |
| Phase 03-core-config-ui-persistence P02 | 18 | 2 tasks | 16 files |
| Phase 03 P03 | 20 | 3 tasks | 14 files |
| Phase 03-core-config-ui-persistence P04 | 38 | 2 tasks | 16 files |
| Phase 04.1 P01 | 9 | 2 tasks | 3 files |
| Phase 04.1 P02 | 3 | 2 tasks | 2 files |
| Phase 04.1 P03 | 3 | 2 tasks | 3 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Roadmap]: Phase 1 (access path) gates the entire product — daemon hardening is a prerequisite of opening the bus, not later cleanup.
- [Roadmap]: Phases 2-3 are pure D-Bus-client work (zero daemon C++ changes); Phase 4 (gestures) is the first and safest daemon extension, de-risking the action wheel.
- [Roadmap]: Action wheel (Phase 6) sequenced late with a mandatory X11/Wayland overlay spike; daemon detects/executes, GUI renders only.
- [Roadmap]: Macros (Phase 7) and backlight (Phase 8) isolated as later, higher-uncertainty phases; backlight is greenfield (no LED HID++ feature exists).
- [Phase 1]: Wave 0 harness: header-only RED CTest unit (RED until Plan 04 adds hasHidppHeader) + graceful-skip ACCESS-01/02/03 smoke scripts
- [Phase 01]: [Phase 1]: D-Bus policy relaxed to group=logiops for send/receive on pizza.pixl.LogiOps; own= kept root-only (least-privilege, ACCESS-01)
- [Phase 01]: [Phase 1]: logid.service hardened (NoNewPrivileges, ProtectSystem=strict, empty CapabilityBoundingSet, AF_UNIX+AF_NETLINK only); User=root kept and PrivateDevices omitted to preserve hidraw/uinput; ReadWritePaths=/etc/logid.cfg file-scoped pending Plan 06 EROFS check (ACCESS-03)
- [Phase 01]: [Phase 1]: HID reports length-checked before indexing via shared hasHidppHeader() at the 3 raw filter lambdas + isError10/20 belt-and-suspenders; hidpp_bounds CTest GREEN (ACCESS-04). CONCERNS #1 (bare throw; preserves dynamic type), #5 ("%s" format), #8 (explicit read-length guard surviving NDEBUG) fixed.
- [Phase 01]: [Phase 1]: ipcgull threads the D-Bus caller's unique bus name to handlers via thread_local current_caller() (set from gdbus_method_call sender under server_lock, cleared via RAII); g_debug proves threading. Property callbacks untouched. Unblocks polkit-gating save() (ACCESS-02 foundation, Plan 06).
- [Phase 01-access-path-daemon-hardening]: [Phase 1]: Configuration::save() polkit-gated (action pizza.pixl.logiops.save-config, auth_admin_keep) via ipcgull::current_caller()+polkit_system_bus_name subject; fail-safe-DENY on null authority/subject/result/error/empty caller, throw before writeFile so /etc/logid.cfg stays byte-unchanged on denial; polkit-gobject-1 v124 linked, -Werror clean. Task 3 interactive verify (deny-unchanged + authorized-write-under-hardened-unit + ReadWritePaths file-vs-/etc) deferred to operator (ACCESS-02).
- [Phase 02-d-bus-client-device-list]: [Phase 2]: Daemon battery added (HID++ 0x1000 wrapper + DeviceBattery feature + Battery/Charging/BatteryKnown property and BatteryChanged signal on .Device) — DEV-02 is now real signal-driven data, no polling (CONF-03). Scope held to 0x1000; 0x1004 UnifiedBattery deferred (newer mice may need it). Contradicts the earlier 'Phases 2-3 = zero daemon C++ changes' roadmap assumption.
- [Phase 02-d-bus-client-device-list]: [Phase 2]: Typed qdbusxml2cpp proxies (PizzaPixlLogiOps{Devices,Device}Interface) + signal-driven DeviceModel (QAbstractListModel) landed; rows come ONLY from Enumerate+DeviceAdded/Removed/StatusChanged (CONF-03, no cache), StatusChanged emits per-role dataChanged for ConnectionStateRole only (no-flicker, stable nickname sort), beginResetModel reserved for clear()/reconnect. device_model+device_model_noflicker GREEN, -Werror clean. Battery slot/roles present-but-unwired (Plan 05). Caught: '--' and '<' in XML comments silently broke qdbusxml2cpp into empty proxies.
- [Phase 02-d-bus-client-device-list]: [Phase 2]: logiops-gui running app landed — DaemonConnection (system-bus connect + QDBusServiceWatcher NameOwnerChanged reconnect + async Enumerate/per-device Properties.GetAll, no UI block) drives DeviceModel; ScreenState enum maps AccessDenied (group policy) vs DaemonDown (no owner) vs Empty vs Populated; QML StackLayout indexed by screenState gives 4 distinct full-window states; SystemPalette-luminance dark theme (Qt 6.4.2, no colorScheme). -Werror clean, 5/5 unit tests green. Battery is a '—' stub (Plan 05). Caught: loadFromModule is 6.5+ (load via qrc prefer-prefix); runtime needs base qml6-module-qtquick installed.
- [Phase 02-d-bus-client-device-list]: [Phase 2]: Live battery wired end-to-end (DEV-02) — .Device proxy XML gains Battery/Charging/BatteryKnown+BatteryChanged; DaemonConnection subscribes BatteryChanged -> DeviceModel.onBatteryChanged (no polling, CONF-03) + seeds initial from the GetAll snapshot; DeviceDelegate draws glyph+fill+% with >20% green/<=20% amber thresholds, always-green charging bolt, '—' when BatteryKnown=false, per-role bindings (test_battery_noflicker green). Caught: font.features is Qt 6.7+ (target 6.4.2) -> stable % width via TextMetrics instead; battery glyph drawn in QML primitives to tint without Qt5Compat.GraphicalEffects.
- [Phase 03-core-config-ui-persistence]: [Phase 3]: CONF-01 BLOCKING gate satisfied — phase3_save_roundtrip proves Configuration::save() serialization (config::set+writeFile) round-trips device-block count + values with no loss; comment/formatting loss accepted. phase3_save_unknownkey proves schema-unknown keys drop without crash (T-3-00-02). Tests link ipcgull_static+config++, compile config.cpp directly on a temp cfg (no bus/polkit, never /etc).
- [Phase 03-core-config-ui-persistence]: [Phase 3]: KeyNameMapper (BTN-02) maps Qt key/modifier->libevdev KEY_* (modifiers->LEFT*, letters/digits/F-keys algorithmic, table for rest); Escape+unknown->empty (T-3-00-03 no guessed token). Seven typed D-Bus proxies generated (qt_add 2->9) covering full Phase 3 config surface; ai out-arg needs QtTypeName.Out0=QList<int> annotation. Wave 1/2 are pure assembly.
- [Phase 03-core-config-ui-persistence]: DeviceController per-device QObject discovers capabilities via D-Bus Introspect (present-interface set -> lazy typed proxies, mirroring daemon _addFeature), exposes capability flags + live-value Q_PROPERTYs with optimistic async setters; DeviceControllerFactory owns/swaps it on selection (single context property). Tabbed DetailPane: header + sliding accent TabBar + cross-fading ConfigTabs StackLayout with whole-tab capability omission (UI-01). Phase 3 Theme tokens + 12 glyphs landed. phase3_device_controller + full suite 9/9 GREEN.
- [Phase 03-core-config-ui-persistence]: [Phase 3]: Buttons tab landed (BTN-01..04, HOST-01) — ButtonsModel does the strict two-step async reassign (SetAction then .Action.<type> param setter) over .Buttons/.Button, CID-keyed; setChangeHost pre-validates 1..N/next/prev before the D-Bus call (T-3-02-02 stoi DoS guard, unit-asserted). HOST-01 via option-a: exposed read-only daemon ChangeHost.GetHostCount (-Werror clean); GUI seeds host slots from it. QML: synced DeviceRender+BindingList + non-modal ReassignPanel (7 categories, live key-capture, device-driven host slots). ButtonsModel owned by DeviceControllerFactory (per-device swap); KeyNameMapper exposed to QML via keyNames bridge. 10/10 CTest GREEN. On-hardware reassign/key-capture/host-switch deferred to phase gate.
- [Phase 03]: [Phase 3]: DPI-cycle persistence resolved option-a (device-scoped) — config::DpiPreset {value,label} on Profile.dpi_presets + .DPI GetPresets/SetPresets (parallel au/as arrays); extended phase3_save_roundtrip proves values AND labels survive Save. Pointer/Scroll tabs landed (DPI-01..03/SCR-01..03), Scroll sections capability-gated; thumbwheel tap via .ThumbWheel.SetTap (no daemon change). Daemon -Werror clean, phase3 6/6 GREEN.
- [Phase 03-core-config-ui-persistence]: [Phase 3]: Persistence loop closed (PROF-01/CONF-01/CONF-02) — ProfilesModel (create/switch/remove/rename over .Device.SetProfile/RemoveProfile) + global ConfigState (markDirty from every tab setter; async polkit-gated .Config.Save via QDBusPendingCallWatcher, no UI freeze/no retry-storm; AccessDenied->auth-declined / ServiceUnknown->daemon-down copy with dirty held on error per T-3-04-03; .Device.ClearProfile restore behind a confirmation dialog). Single PizzaPixlLogiOpsDeviceInterface extended (a 2nd XML for the same interface name collides). phase3 7/7 + full 12/12 GREEN, -Werror clean, QML loads clean. Task 3 on-hardware polkit/persistence/restore verify pending.
- [Phase 04.1]: Hybrid/reference-only is the standing rule (adapt behavior+vocabulary, ship only our own QML/strings); enforced as Phase 9 BLOCKING legal-asset audit. Encrypted device DB (devices_*.json, key-id/file-sha+binary) is unusable, no decryption; capability comes from live HID++ enumeration + public DBs.
- [Phase 04.1]: applications.json detection is Windows-only (no X11 WM_CLASS); PROF-03 window-class/executable matching is OUR Linux rule layer, seeded by the Options+ applicationId taxonomy as a catalog
- [Phase 04.1]: Smart-action step model unifies predefined_win.json + applications.json macro.type vocab; process-spawn/window steps (OPEN_FILE_FOLDER, APP_WINDOWS_MANAGEMENT) route to a non-root session helper (MACRO-02)
- [Phase 04.1]: [Phase 04.1]: app.asar extracted (565 files, ~81M) to a gitignored phase scratch dir (never src/); only a .extracted marker + an owned asar-inventory.md are committed. The Options+ renderer is minified-bundled React, so the Plan-04 design study must use the running app + readable LogiOptionsPlus/data/ (strings/defaults/overlay/macros), not the JS bundles.

### Pending Todos

None yet.

### Blockers/Concerns

- [Research] Web verification was unavailable during research — re-verify per phase: D-Bus/polkit syntax (P1), Wayland foreground detection (P5), Wayland overlay matrix (P6), Options+ Smart Action vocabulary (P7), backlight HID++ feature (P8).
- [Codebase] Daemon runs as root parsing untrusted HID with no sandboxing and unbounded report indexing (CONCERNS #2/#3) — addressed in Phase 1 before any daemon extension.
- [Codebase] `logid::Device` hardcodes HID++ 2.0 (`Device.h:55`) — may need work for heterogeneous multi-device richness (watch in Phase 2).
- [Codebase] `ipcgull` is vendored and incomplete (CONCERNS #10) — widening its audience may expose parsing/marshalling bugs.

## Session Continuity

Last session: 2026-06-01T03:45:33.819Z
Stopped at: Completed 04.1-03-PLAN.md
Resume file: None
