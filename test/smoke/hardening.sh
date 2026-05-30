#!/usr/bin/env bash
#
# hardening.sh — ACCESS-03 runtime smoke test
#
# Requirement: the running `logid` systemd unit shows the Phase 1 sandbox
# directives in effect — NoNewPrivileges=yes, ProtectSystem=strict, an empty
# CapabilityBoundingSet, and RestrictAddressFamilies covering AF_UNIX + AF_NETLINK
# — and `systemd-analyze security` reports an exposure score.
#
# Prerequisites (manual):
#   - The `logid` systemd unit is installed (and ideally running) on this host.
#   - systemctl / systemd-analyze available.
#
# Behaviour: skips cleanly (exit 0) when the unit isn't installed; exits non-zero
# ONLY when an installed unit is missing a required hardening directive.
#
set -euo pipefail

UNIT="logid"

skip() { echo "SKIP: $*"; exit 0; }
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

command -v systemctl >/dev/null 2>&1 || skip "systemctl not available (no systemd)"

# --- prerequisite: the unit is known to systemd ---
if ! systemctl show "$UNIT" >/dev/null 2>&1; then
    skip "unit '$UNIT' is not installed/known to systemd"
fi
if ! systemctl cat "$UNIT" >/dev/null 2>&1; then
    skip "unit '$UNIT' has no unit file installed yet (hardening not applied)"
fi

# --- read the relevant properties ---
PROPS="$(systemctl show "$UNIT" \
    -p NoNewPrivileges \
    -p ProtectSystem \
    -p CapabilityBoundingSet \
    -p RestrictAddressFamilies 2>/dev/null)"
echo "Hardening properties for $UNIT:"
echo "$PROPS" | sed 's/^/  /'
echo

get() { echo "$PROPS" | grep -E "^$1=" | head -1 | cut -d= -f2-; }

RC=0

NNP="$(get NoNewPrivileges)"
if [ "$NNP" = "yes" ] || [ "$NNP" = "true" ]; then
    pass "NoNewPrivileges=$NNP"
else
    echo "FAIL: NoNewPrivileges expected yes, got '${NNP:-<unset>}'" >&2; RC=1
fi

PS="$(get ProtectSystem)"
if [ "$PS" = "strict" ]; then
    pass "ProtectSystem=strict"
else
    echo "FAIL: ProtectSystem expected strict, got '${PS:-<unset>}'" >&2; RC=1
fi

CBS="$(get CapabilityBoundingSet)"
if [ -z "$CBS" ]; then
    pass "CapabilityBoundingSet is empty (all caps dropped)"
else
    echo "FAIL: CapabilityBoundingSet expected empty, got '$CBS'" >&2; RC=1
fi

RAF="$(get RestrictAddressFamilies)"
if echo "$RAF" | grep -q "AF_UNIX" && echo "$RAF" | grep -q "AF_NETLINK"; then
    pass "RestrictAddressFamilies covers AF_UNIX and AF_NETLINK ($RAF)"
else
    echo "FAIL: RestrictAddressFamilies must include AF_UNIX and AF_NETLINK, got '${RAF:-<unset>}'" >&2; RC=1
fi

# --- exposure score (informational) ---
if command -v systemd-analyze >/dev/null 2>&1; then
    echo
    echo "systemd-analyze security $UNIT (exposure score):"
    systemd-analyze security "$UNIT" 2>/dev/null | tail -1 | sed 's/^/  /' \
        || echo "  (could not compute exposure score)"
fi

if [ "$RC" -ne 0 ]; then
    fail "one or more hardening directives are missing — ACCESS-03 not satisfied"
fi
echo
pass "all required hardening directives are in effect"
exit 0
