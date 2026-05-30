# logiops test harness (Phase 1)

This directory holds the Wave 0 verification harness for Phase 1
(Access Path & Daemon Hardening). The logiops daemon ships with effectively no
automated tests, so this harness gives every later plan in the phase a concrete,
runnable verification target.

## What's here

| Artifact | Covers | Type |
|----------|--------|------|
| `hidpp_bounds_test.cpp` + `CMakeLists.txt` | ACCESS-04 — short/empty HID reports never OOB-read | CTest C++ unit |
| `smoke/access-path.sh` | ACCESS-01 — a non-root `logiops`-group user reaches the D-Bus service | runtime smoke |
| `smoke/polkit-deny.sh` | ACCESS-02 — a denied `Save` leaves `/etc/logid.cfg` byte-unchanged | runtime smoke |
| `smoke/hardening.sh` | ACCESS-03 — systemd sandbox directives are in effect | runtime smoke |

## Quick run (C++ unit)

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

> **RED until Plan 04.** `hidpp_bounds_test.cpp` calls
> `logid::backend::hidpp::hasHidppHeader()`, a helper that **Plan 04 (ACCESS-04)
> adds** to `src/logid/backend/hidpp/Report.h`. Until that lands the
> `hidpp_bounds_test` target **fails to compile** — this is the intentional RED
> baseline; it turns GREEN once the helper exists.

List the registered test without building/running it:

```bash
ctest --test-dir build -N
```

## Smoke scripts (runtime)

The three scripts under `smoke/` are **runtime** black-box checks: they need a
live `logid` daemon and a real session. Each script **skips cleanly (exit 0) with
a clear message** when a prerequisite is missing, and fails (non-zero) only when
an assertion genuinely fails.

```bash
bash test/smoke/access-path.sh
bash test/smoke/polkit-deny.sh
bash test/smoke/hardening.sh
```

### Manual prerequisites

These cannot be created by the scripts themselves:

1. **`logiops` group + membership** (for `access-path.sh`):
   ```bash
   sudo groupadd logiops              # group creation is packaging's job (Phase 9)
   sudo usermod -aG logiops "$USER"   # add yourself
   # then RE-LOGIN (or `newgrp logiops`) — supplementary groups are set at login
   ```
2. **A running `logid` daemon** owning `pizza.pixl.LogiOps` on the system bus
   (for all three scripts).
3. **An interactive polkit authentication agent** in your session
   (for `polkit-deny.sh` — so the auth prompt can appear and be cancelled).
4. **`/etc/logid.cfg` present** (for `polkit-deny.sh`'s sha256 before/after check).

After installing/relaxing the D-Bus policy, pick it up with:

```bash
sudo systemctl reload dbus   # or reboot
```
