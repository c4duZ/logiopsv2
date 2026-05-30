---
phase: 01-access-path-daemon-hardening
reviewed: 2026-05-30T00:00:00Z
depth: standard
files_reviewed: 15
files_reviewed_list:
  - src/logid/Configuration.cpp
  - src/logid/Configuration.h
  - src/logid/CMakeLists.txt
  - src/logid/backend/hidpp/Device.cpp
  - src/logid/backend/hidpp/Report.cpp
  - src/logid/backend/hidpp/Report.h
  - src/logid/backend/hidpp10/ReceiverMonitor.cpp
  - src/logid/backend/raw/RawDevice.cpp
  - src/logid/logid.cpp
  - src/logid/util/ExceptionHandler.cpp
  - src/logid/logid.service.in
  - src/logid/logiops-dbus.conf.in
  - src/logid/logiops-policy.policy.in
  - src/ipcgull/src/server_gdbus.cpp
  - src/ipcgull/src/include/ipcgull/connection.h
findings:
  critical: 0
  warning: 3
  info: 6
  total: 9
status: issues_found
---

# Phase 1: Code Review Report

**Reviewed:** 2026-05-30
**Depth:** standard
**Files Reviewed:** 15
**Status:** issues_found

## Summary

This is a security-hardening phase on a root daemon. The five hardening goals are largely well-executed:

- **Polkit fail-safe-DENY (Configuration.cpp):** The gating is airtight on correctness grounds. Every error/null/empty-caller branch returns `false` (or `save()` throws) *before* `writeFile()`, GObject refs are released on every path, and `GError` is freed wherever it can be set. No critical TOCTOU exists because the check authorizes the *caller bus name*, and the write target is a fixed path (`/etc/logid.cfg`) writable only by root. Subject construction via `polkit_system_bus_name_new` from the D-Bus unique name is correct.
- **ipcgull caller threading (server_gdbus.cpp):** The `thread_local` current-caller slot is set under `server_lock` and cleared via an RAII guard that fires on *every* exit path including the exception-to-D-Bus-error conversions. Correct.
- **HID bounds checks (Report.h / Device.cpp / ReceiverMonitor.cpp):** `hasHidppHeader()` correctly guards `report.size() >= 4` before the raw `report[Offset::*]` indexing in all three raw event-filter lambdas. The error-parse guards in `Report.cpp` correctly require `size() >= 6` before reading indices 3-5.
- **CONCERNS fixes:** `ExceptionHandler::Default` now uses bare `throw;` (slicing fixed). `logid.cpp` consistently passes user/runtime strings as `%s` arguments, not as the format string. `RawDevice::_readReports` has an explicit length guard that survives `NDEBUG` (no `assert`).
- **D-Bus / systemd hardening:** The policy is least-privilege (`own` restricted to root) and the systemd unit deliberately avoids `/dev` sandboxing that would break hidraw/uinput. Both are sound.

The findings below are real but none are blocking. Three warnings concern robustness of the new code paths under unusual-but-reachable conditions; the rest are informational.

## Warnings

### WR-01: Polkit blocking `_sync` call can stall the GLib dispatch thread (with user interaction) — acknowledged but unbounded

**File:** `src/logid/Configuration.cpp:88-90`
**Issue:** `polkit_authority_check_authorization_sync(...)` is called with `POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION` from inside `save()`, which runs on the GLib main-loop dispatch thread (the same thread `gdbus_method_call` runs on, under `server_lock`). While the polkit agent prompts the user for a password, this thread is fully blocked — it cannot dispatch any other D-Bus method calls, property reads, or process the `name_lost` callback. The in-code NOTE acknowledges this and defers an async fix, which is a reasonable risk decision for a rare save action. The residual concern is that `server_lock` (a `recursive_mutex`) is *held* across the entire blocking prompt: any other thread calling `emit_signal`, `add_interface`, or `drop_interface` will block until the human responds (or the polkit prompt times out). For a multi-device daemon emitting device signals, a hotplug event during an open auth prompt would stall.
**Fix:** Acceptable to defer for v1 per the documented decision. If a stall is observed, move to `polkit_authority_check_authorization` (async) with a deferred D-Bus reply, OR at minimum drop `server_lock` before the blocking call. To make the deferral explicit and safe, document that signal emission may stall during an auth prompt, or capture `_config_file` + caller and release the lock before prompting. No code change required to ship; track as a follow-up.

### WR-02: `get_phys` / `get_name` underflow `len - 1` when ioctl returns 0

**File:** `src/logid/backend/raw/RawDevice.cpp:96` and `:108`
**Issue:** Both functions compute `static_cast<size_t>(len) - 1` for the returned string length. The guard only rejects `len == -1`. If `HIDIOCGRAWPHYS`/`HIDIOCGRAWNAME` returns `0` (empty string — a kernel/driver edge case for some virtual or malformed nodes), `static_cast<size_t>(0) - 1` underflows to `SIZE_MAX`, and the `std::string{buf, SIZE_MAX}` constructor reads far out of bounds (crash / info leak). This is reachable from untrusted/unusual hardware enumeration, which is exactly the threat surface this phase targets. This is pre-existing code but sits squarely in the hardened access path.
**Fix:**
```cpp
std::string get_phys(int fd) {
    ssize_t len;
    char buf[256];
    if (-1 == (len = ::ioctl(fd, HIDIOCGRAWPHYS(sizeof(buf)), buf))) {
        int err = errno;
        ::close(fd);
        throw std::system_error(err, std::system_category(),
                                "RawDevice HIDIOCGRAWPHYS failed");
    }
    if (len <= 0)
        return {};                          // empty / no NUL to strip
    return {buf, static_cast<size_t>(len) - 1};
}
```
Apply the same `if (len <= 0) return {};` guard to `get_name` (line 108).

### WR-03: `_readReports` length guard has an unreachable lower bound and a wrong upper-bound type comparison

**File:** `src/logid/backend/raw/RawDevice.cpp:232-237`
**Issue:** The new guard is `if (len < 0 || len > max_data_length)`. Two problems:
1. `len < 0` is dead: the `while (-1 != (len = ::read(...)))` condition already excludes the only negative value `read` can return (`-1`). Harmless but misleading.
2. `max_data_length` is declared `static constexpr int max_data_length = 32` (RawDevice.h:49). `read(_fd, buf, max_data_length)` is passed an `int` count, so the kernel can never return more than 32 and `len > max_data_length` is also effectively unreachable. The guard therefore never actually rejects anything — it is defensive scaffolding, not an active bound. The *intended* protection (reject reports that cannot hold a HID++ header) is not what this checks. Note `max_data_length` (32) exceeds `Report::MaxDataLength` (20), so an over-20-byte read still flows into `Report(report)` which silently truncates — acceptable, but the guard does not enforce the documented "bogus length" rejection it claims.
**Fix:** Make the guard meaningful and self-documenting against the buffer, not a constant that already bounds the read:
```cpp
while (-1 != (len = ::read(_fd, buf, max_data_length))) {
    // read() into a max_data_length buffer can only return 0..max_data_length,
    // but guard explicitly so the invariant survives future buffer/count changes.
    if (len <= 0 || static_cast<size_t>(len) > sizeof(buf)) {
        logPrintf(WARN, "Ignoring HID read of unexpected length %zd on %s",
                  (ssize_t)len, _path.c_str());
        continue;
    }
    std::vector<uint8_t> report(buf, buf + len);
    ...
```
Using `<= 0` also skips zero-length reads (which would otherwise build an empty vector and index past it downstream). Comparing against `sizeof(buf)` (not the `int` count constant) keeps the bound tied to the actual destination buffer.

## Info

### IN-01: `polkit_authority_get_sync` result is process-cached but not refreshed on error

**File:** `src/logid/Configuration.cpp:69`
**Issue:** `polkit_authority_get_sync` returns a per-process singleton that is ref-counted; calling it per-save and unref-ing each time is correct and leak-free. No action needed — noting only that if the authority ever enters a permanently-broken state, every subsequent save fails closed (the desired fail-safe behavior). This is correct-by-design, documented here so it is not later "fixed" into a fail-open cache.
**Fix:** None. Keep as-is.

### IN-02: `g_debug` logs the caller's unique bus name on every method call

**File:** `src/ipcgull/src/server_gdbus.cpp:332`
**Issue:** `g_debug("ipcgull: method call from %s", ...)` runs for every dispatched method. The unique bus name (`:1.42`) is low-sensitivity, and `g_debug` is suppressed unless `G_MESSAGES_DEBUG` is set, so this is fine. Flagged only so it is a conscious choice on a root daemon.
**Fix:** Optional — gate behind a build flag or remove before release if debug chatter is undesirable.

### IN-03: `save()` re-fetches caller via `current_caller()` rather than receiving it as an argument

**File:** `src/logid/Configuration.cpp:109`
**Issue:** `save()` reads the caller from the `thread_local` slot set by ipcgull. This is correct *only* because the ipcgull dispatch is synchronous on the same thread and the slot is live for the handler duration. If a future refactor offloads method handlers to the worker pool (`run_task`), the `thread_local` would be empty on the worker thread and `caller.empty()` would deny every save (fail-safe — good) but break legitimate saves silently.
**Fix:** None required now. Add a comment at `save()` noting the dependency on synchronous same-thread dispatch, mirroring the comment already in `connection.h`.

### IN-04: `reportFixup` Long-report branch is `assert`-only (drops under NDEBUG)

**File:** `src/logid/backend/hidpp/Device.cpp:311-314`
**Issue:** When a Long report is sent to a device that does not advertise `LongReportSupported`, the code only `assert`s. Under `NDEBUG` (release builds — CI builds Release) this is a no-op and an unsupported-length report is sent anyway. Not introduced by this phase and not in the hardened input path (this is the *outbound* path), but it is the same `assert`-survives-NDEBUG class of issue the phase fixed elsewhere.
**Fix:** Optional consistency improvement — replace with a thrown `InvalidReportID()`/log-and-return if outbound robustness matters. Out of scope for this phase.

### IN-05: D-Bus policy relies on a `logiops` group that the build does not create

**File:** `src/logid/logiops-dbus.conf.in:18`
**Issue:** The `<policy group="logiops">` block grants send/receive to members of a `logiops` system group. Nothing in `CMakeLists.txt` (or a sysusers/postinst) creates that group, so on a fresh install the group does not exist and the policy block is inert until an admin creates it. This is least-privilege-safe (fails closed: no group → no non-root access), but it means the documented "non-root GUI access path" does not work out of the box.
**Fix:** Add a `sysusers.d` entry or packaging step to create the `logiops` group, and document it. Tracked as a follow-up for the access-path goal; not a security defect (the failure mode is deny, not allow).

### IN-06: `ReadWritePaths=/etc/logid.cfg` requires the file to exist at unit start

**File:** `src/logid/logid.service.in:23`
**Issue:** With `ProtectSystem=strict`, `ReadWritePaths=/etc/logid.cfg` makes the single config file writable. systemd requires the path to exist when the unit starts (a missing `ReadWritePaths` target is tolerated with a warning on modern systemd, but on older versions it can fail the unit). The daemon itself tolerates a missing config (Configuration.cpp:49-51 "using empty config"), so a first-run-before-save scenario could mismatch the unit expectation, and a later `save()` would attempt to create `/etc/logid.cfg` which `ProtectSystem=strict` permits only because the parent `/etc` is exposed via this exact rule.
**Fix:** Ensure packaging ships an empty/example `/etc/logid.cfg` (or use `ReadWritePaths=-/etc/logid.cfg` if a tolerant rule is desired on newer systemd). Verify on the oldest supported systemd in the CI matrix (ubuntu:20.04). Documentation/packaging follow-up.

---

_Reviewed: 2026-05-30_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
