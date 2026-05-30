---
status: partial
phase: 01-access-path-daemon-hardening
source: [01-VERIFICATION.md]
started: 2026-05-30
updated: 2026-05-30
---

## Current Test

[awaiting human testing — needs a real desktop session with a polkit agent + an attached Logitech device]

## Tests

### 1. polkit DENY leaves /etc/logid.cfg byte-unchanged (ACCESS-02)
expected: With the daemon installed/restarted under the hardened unit, trigger a config save over D-Bus; the polkit prompt appears; CANCEL it; `/etc/logid.cfg` sha256 is unchanged and the log shows "Unauthorized save() denied." Run: `bash test/smoke/polkit-deny.sh`
result: [pending]

### 2. Authorized save writes under the hardened unit (ACCESS-02 / ACCESS-03)
expected: Authenticate the polkit prompt; the write succeeds despite `ProtectSystem=strict`. Decide ReadWritePaths: file-scoped `/etc/logid.cfg` sufficed, OR widen to `ReadWritePaths=/etc` if EROFS is observed (libconfig temp+rename — feeds back to Plan 03 / src/logid/logid.service.in).
result: [pending]

### 3. Non-root logiops-group user reaches the bus and changes a setting (ACCESS-01)
expected: As a user in the `logiops` group (re-login after `usermod -aG logiops $USER`), a non-root D-Bus call to `pizza.pixl.LogiOps` changes a device setting on real hardware. Run: `bash test/smoke/access-path.sh`
result: [pending]

### 4. Hardening doesn't break hidraw/uinput with a device attached (ACCESS-03)
expected: After installing the hardened unit and restarting `logid`, a Logitech device is still detected and a button remap still synthesizes input (uinput works). Also: `systemd-analyze security logid` shows an improved exposure score. Run: `bash test/smoke/hardening.sh`
result: [pending]

## Summary

total: 4
passed: 0
issues: 0
pending: 4
skipped: 0
blocked: 0

## Gaps
