# Testing

How the logiops project is verified. The honest summary: there is **no automated unit/integration test suite for the `logid` daemon**. Verification relies on compile-time CI across distros and manual hardware testing.

## Test Framework

None for the daemon. There is no GoogleTest/Catch2/CTest suite covering `src/logid/`.

## What Exists

| Mechanism | Location | What it covers |
|-----------|----------|----------------|
| Multi-distro compile CI | `.github/workflows/build-test.yml` | Builds the project on several distributions with warnings-as-errors (`-Werror`). Catches compile/portability regressions, not behavior. |
| Release workflow | `.github/workflows/make-release.yml` | Packaging/release automation. |
| Manual IPC demo | `src/ipcgull/tests/server_test/main.cpp` | A hand-run demonstration server for the vendored `ipcgull` D-Bus binding. Not an assertion-based test. |
| Manual hardware tracking | `TESTED.md` | Human-maintained list of which Logitech devices have been confirmed working (e.g. MX Master series). |

## Implications for New Work

- **No safety net for behavior changes.** Logic changes in device handling, HID++ parsing, actions, or config must be validated manually against real hardware.
- **CI only guarantees it compiles** (cleanly, with `-Werror`) on the supported distros. Keep that bar: no new warnings.
- The HID++ report-parsing and backend layers (`src/logid/backend/`) are the highest-risk untested areas — they parse untrusted hardware input (see `CONCERNS.md` #3). Any new test investment should start here.
- The `ipcgull` D-Bus layer can be exercised manually via `src/ipcgull/tests/server_test/main.cpp`.

## Recommendation

If introducing automated tests, the highest-value targets are pure/parsing logic that doesn't require hardware: HID++ `Report` construction/validation (`src/logid/backend/hidpp/Report.cpp`), error-detection helpers (`isError10`/`isError20`), and config schema parsing (`src/logid/config/schema.h`). These can be unit-tested without a device attached.

---
*Mapped: 2026-05-30*
