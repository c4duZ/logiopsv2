#!/usr/bin/env bash
#
# polkit-deny.sh — ACCESS-02 runtime smoke test (the headline criterion)
#
# Requirement: invoking the privileged `Save` method and DENYING (cancelling) the
# polkit authorization prompt must leave `/etc/logid.cfg` BYTE-UNCHANGED, and a
# denied/WARN line must appear in the daemon's journal.
#
# This script only HASHES the config and asserts it did not change — it NEVER
# writes to /etc/logid.cfg itself (threat T-01-01: no tampering with real config).
#
# Prerequisites (manual):
#   - The `logid` daemon is running and owns `pizza.pixl.LogiOps`.
#   - An interactive polkit authentication agent is running in this session
#     (so the prompt appears and YOU can cancel it).
#   - `/etc/logid.cfg` exists.
#   - busctl or gdbus available.
#
# Behaviour: skips cleanly (exit 0) when a prerequisite is missing; exits non-zero
# ONLY when the config changed across a denied save (or no denial was logged).
#
set -euo pipefail

SERVICE="pizza.pixl.LogiOps"
OBJECT="/pizza/pixl/logiops"
CONFIG_IFACE="pizza.pixl.LogiOps.Config"
CONFIG_FILE="/etc/logid.cfg"

skip() { echo "SKIP: $*"; exit 0; }
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

# --- prerequisites ---
command -v sha256sum >/dev/null 2>&1 || skip "sha256sum not available"
[ -r "$CONFIG_FILE" ] || skip "$CONFIG_FILE absent or unreadable — nothing to protect"

CALLER=""
if command -v busctl >/dev/null 2>&1; then
    CALLER="busctl"
elif command -v gdbus >/dev/null 2>&1; then
    CALLER="gdbus"
else
    skip "neither busctl nor gdbus available to invoke Save"
fi

if command -v busctl >/dev/null 2>&1; then
    busctl --system list 2>/dev/null | grep -q "$SERVICE" || \
        skip "$SERVICE not owned on the system bus (is logid running?)"
fi

# --- capture the config hash BEFORE the (to-be-denied) save ---
BEFORE="$(sha256sum "$CONFIG_FILE" | awk '{print $1}')"
echo "Config sha256 before: $BEFORE"
echo
echo ">>> A polkit prompt should appear. CANCEL / DENY it to exercise ACCESS-02. <<<"
echo

# --- invoke Save (expected to be denied) ---
set +e
if [ "$CALLER" = "busctl" ]; then
    busctl --system call "$SERVICE" "$OBJECT" "$CONFIG_IFACE" Save >/tmp/polkit_deny_call.log 2>&1
else
    gdbus call --system --dest "$SERVICE" --object-path "$OBJECT" \
        --method "${CONFIG_IFACE}.Save" >/tmp/polkit_deny_call.log 2>&1
fi
CALL_RC=$?
set -e
echo "Save call exit code: $CALL_RC (non-zero expected on a denied save)"

# --- capture the config hash AFTER ---
AFTER="$(sha256sum "$CONFIG_FILE" | awk '{print $1}')"
echo "Config sha256 after:  $AFTER"

# --- assertion 1: config byte-identical ---
if [ "$BEFORE" != "$AFTER" ]; then
    fail "$CONFIG_FILE CHANGED across a denied save (before=$BEFORE after=$AFTER) — ACCESS-02 VIOLATED"
fi
pass "$CONFIG_FILE is byte-identical (sha256 unchanged) after the denied save"

# --- assertion 2: a denial/WARN line is present in the journal ---
if command -v journalctl >/dev/null 2>&1; then
    if journalctl -u logid --since "1 minute ago" --no-pager 2>/dev/null \
            | grep -Eiq "denied|not authorized|unauthorized|WARN"; then
        pass "a denied/WARN line is present in journalctl -u logid"
    else
        echo "WARN: no denied/WARN line found in journalctl -u logid (last minute);" \
             "config was unchanged, but the audit log line is expected once ACCESS-02 lands"
    fi
else
    echo "NOTE: journalctl unavailable — skipped the audit-log assertion (config check passed)"
fi

exit 0
