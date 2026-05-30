---
phase: 01-access-path-daemon-hardening
plan: 05
subsystem: infra
tags: [ipcgull, dbus, gdbus, polkit-foundation, caller-identity, thread_local]

# Dependency graph
requires:
  - phase: 01-access-path-daemon-hardening (Plans 01-04)
    provides: D-Bus policy relaxed to group=logiops, daemon hardened, HID length-checks + CONCERNS fixes
provides:
  - "ipcgull::current_caller() — public accessor returning the dispatching D-Bus caller's unique bus name, empty outside a method call"
  - "gdbus_method_call threads its sender into a thread_local slot under the dispatch lock, cleared via RAII"
  - "g_debug() trace of the caller on every method call (verbose: G_MESSAGES_DEBUG)"
affects: [01-06-polkit-gating-save, ACCESS-02]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "thread_local current-caller slot set under server_lock, cleared via RAII CallerGuard (safe single- or multi-threaded GMainContext dispatch)"
    - "Vendored ipcgull submodule modified on a local branch (logiops-access-path); superproject gitlink advanced per task"

key-files:
  created: []
  modified:
    - src/ipcgull/src/include/ipcgull/connection.h
    - src/ipcgull/src/server_gdbus.cpp

key-decisions:
  - "Used a thread_local g_current_caller slot (RESEARCH Option A / Open Question 2) — correct under either single- or multi-threaded GMainContext dispatch, so no need to resolve the threading model definitively"
  - "Debug log left in as a g_debug() (silent unless G_MESSAGES_DEBUG enabled) rather than fprintf(stderr) — proves threading without spamming the journal; Plan 06 consumes current_caller() for the real polkit check"
  - "current_caller() qualified-access expression (ipcgull::g_current_caller = ...) kept verbatim per the plan's acceptance contract even though unqualified would compile inside namespace ipcgull"

patterns-established:
  - "Caller identity is reachable from a method handler via ipcgull::current_caller() for the duration of the dispatch; empty outside it (fail-safe input for Plan 06's deny-on-empty)"

requirements-completed: [ACCESS-02]

# Metrics
duration: 5min
completed: 2026-05-30
---

# Phase 1 Plan 05: Thread D-Bus Caller Through ipcgull Summary

**`ipcgull::current_caller()` now returns the dispatching D-Bus caller's unique bus name (e.g. `:1.42`), populated from `gdbus_method_call`'s `sender` into a `thread_local` slot under the dispatch lock and cleared via RAII — the load-bearing prerequisite for polkit-gating `save()` in Plan 06.**

## Performance

- **Duration:** 5 min
- **Started:** 2026-05-30T18:05:56Z
- **Completed:** 2026-05-30T18:10:51Z
- **Tasks:** 2
- **Files modified:** 2 (both in the vendored ipcgull submodule)

## Accomplishments
- Declared a generic, polkit-free `ipcgull::current_caller()` accessor in the public header `connection.h` (`<string>` added, `#ifndef`/`#define` guard kept, no logid/polkit symbols — ipcgull stays standalone per CONCERNS #10).
- Defined `current_caller()` backed by a `thread_local std::string g_current_caller` slot in `server_gdbus.cpp`.
- In `gdbus_method_call`, dropped `[[maybe_unused]]` on `sender`, set the slot under the existing `server_lock`, and installed a `CallerGuard` RAII that clears the slot on every exit path (including the throw → D-Bus-error path).
- Added a `g_debug("ipcgull: method call from %s", ...)` trace proving the caller is threaded to the handler (the plan's headline acceptance signal).
- The two property callbacks (`gdbus_get_property`, `gdbus_set_property`) keep their `[[maybe_unused]] const gchar* sender` untouched, as required.
- Builds clean in the standard `build/` dir and warning-clean under a dedicated `-Werror` build (CI condition).

## Task Commits

Each task was committed atomically. Because `src/ipcgull` is a git submodule, each task is one commit inside the submodule plus a superproject commit advancing the gitlink.

1. **Task 1: Declare `ipcgull::current_caller()`** — submodule `997c266`, superproject `eddedd1` (feat)
2. **Task 2: Populate the caller slot + define `current_caller()`** — submodule `faf764e`, superproject `dc02044` (feat)

Submodule commits live on the local branch `logiops-access-path` in `src/ipcgull` (base `cd0f9a8`).

**Plan metadata:** (docs commit below — see final commit)

## Files Created/Modified
- `src/ipcgull/src/include/ipcgull/connection.h` — Public `ipcgull::current_caller()` declaration; `<string>` include added.
- `src/ipcgull/src/server_gdbus.cpp` — `thread_local g_current_caller` slot + `current_caller()` definition; `gdbus_method_call` sets the slot from `sender` under `server_lock`, clears via RAII `CallerGuard`, and `g_debug`-logs the sender.

## Decisions Made
- **thread_local over a struct-member slot:** Resolves RESEARCH Open Question 2 by construction — correct under either single- or multi-threaded GMainContext dispatch (A8), so the threading-model question no longer blocks Plan 06.
- **Debug log retained as `g_debug()`:** Quiet by default (only visible with `G_MESSAGES_DEBUG=all`/`ipcgull` in `journalctl -u logid`), so it proves threading without journal spam. It is harmless to leave in once Plan 06 consumes `current_caller()`; Plan 06 may remove it if desired.
- **Qualified assignment expression kept verbatim** (`ipcgull::g_current_caller = (sender ? sender : "")`) to satisfy the plan's acceptance/verify contract.

## GMainContext Threading Observation (RESEARCH A8)
ipcgull dispatches GDBus method calls under `g_main_loop_run` on the thread that called `server::start()` (a single `GMainContext`/main-loop thread in the logid daemon — `logid.cpp` blocks in `server->start()`). The `thread_local` slot is therefore set and read on the same thread for the whole dispatch, and would remain race-free even if a future ipcgull change dispatched on multiple threads. No reliance on the single-thread assumption is baked into the design.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Committed ipcgull changes through the submodule, not the superproject**
- **Found during:** Task 1 (commit step)
- **Issue:** `src/ipcgull` is a git submodule; a superproject `git add src/ipcgull/...` fails with "is in submodule". The plan assumed a normal in-repo commit.
- **Fix:** Created a local branch `logiops-access-path` in the submodule (it was in detached HEAD at upstream `cd0f9a8`), committed each task inside the submodule, then committed the advanced gitlink in the superproject per task. No upstream push (remote is read-only `PixlOne/ipcgull`); the vendored change rides as a local submodule commit so the daemon builds against the modified ipcgull.
- **Files modified:** `src/ipcgull/src/include/ipcgull/connection.h`, `src/ipcgull/src/server_gdbus.cpp`, plus the superproject `.gitmodules`-tracked gitlink for `src/ipcgull`.
- **Verification:** `cmake --build build` and a fresh `-Werror` build both succeed against the new submodule commits; `git ls-tree HEAD src/ipcgull` points at `faf764e`.
- **Committed in:** submodule `997c266`/`faf764e`, superproject `eddedd1`/`dc02044`.

---

**Total deviations:** 1 auto-fixed (1 blocking).
**Impact on plan:** Mechanical commit-routing only (vendored submodule). No code-scope change; the implementation matches the plan exactly. Note for Plan 06 and packaging: the ipcgull submodule now tracks a local branch with commits not present upstream — packaging/CI must build the submodule at this gitlink, and a future upstreaming or pin of these commits should be considered.

## Issues Encountered
- The plan's Task 2 verify command asserts the literal qualified expression `ipcgull::g_current_caller = (sender ? sender`. The first attempt used the unqualified `g_current_caller = ...` (valid inside `namespace ipcgull`); the verify's fixed-string check failed. Resolved by qualifying the assignment and the RAII clear with `ipcgull::`, which compiles identically and satisfies the contract. Verify then returned `OK`.

## User Setup Required
None - no external service configuration required. (The debug trace is visible via `journalctl -u logid` with `G_MESSAGES_DEBUG` set, or a `gdbus call --system` against `pizza.pixl.LogiOps` will emit a non-empty `:1.NN` sender line — the plan's manual acceptance check, deferred to runtime/hardware per RESEARCH Open Question 4.)

## Next Phase Readiness
- **Plan 06 (polkit gating of `save()`) is unblocked:** `Configuration::save()` can now call `ipcgull::current_caller()` to obtain the caller bus name, build a `polkit_system_bus_name_new(sender)` subject, and fail-safe-deny on an empty caller.
- **Carry-forward for Plan 06 / packaging:** the ipcgull submodule is on a local branch (`logiops-access-path`) with commits not upstream; CI and Debian packaging must honor the advanced gitlink. `libpolkit-gobject-1-dev` is still NOT installed on this machine (RESEARCH Environment Availability) — a blocking build dep for Plan 06.

---
*Phase: 01-access-path-daemon-hardening*
*Completed: 2026-05-30*

## Self-Check: PASSED

- FOUND: `.planning/phases/01-access-path-daemon-hardening/01-05-SUMMARY.md`
- FOUND: `src/ipcgull/src/include/ipcgull/connection.h`
- FOUND: `src/ipcgull/src/server_gdbus.cpp`
- FOUND (superproject): `eddedd1`, `dc02044`
- FOUND (submodule): `997c266`, `faf764e`
