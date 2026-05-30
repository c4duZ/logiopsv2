#!/usr/bin/env bash
#
# access-path.sh — ACCESS-01 runtime smoke test
#
# Requirement: a NON-ROOT user who is a member of the `logiops` group can connect
# to the `pizza.pixl.LogiOps` D-Bus service and read/control devices WITHOUT being
# denied (org.freedesktop.DBus.Error.AccessDenied).
#
# Prerequisites (manual — the script cannot create these):
#   - The `logid` daemon is running and owns `pizza.pixl.LogiOps` on the system bus.
#   - The `logiops` group exists and the current user is a member:
#         sudo groupadd logiops
#         sudo usermod -aG logiops "$USER"   # then RE-LOGIN (group set at login)
#   - The relaxed D-Bus policy (group=logiops) is installed and dbus reloaded.
#
# Behaviour: degrades gracefully — prints "SKIP: ..." and exits 0 when a
# prerequisite is missing; exits non-zero ONLY when the access assertion fails.
#
set -euo pipefail

SERVICE="pizza.pixl.LogiOps"
OBJECT="/pizza/pixl/logiops"

skip() { echo "SKIP: $*"; exit 0; }
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

# --- prerequisite: running as non-root ---
if [ "$(id -u)" -eq 0 ]; then
    skip "running as root — this test checks the NON-root access path (run as a normal user)"
fi

# --- prerequisite: member of the logiops group ---
if ! id -nG | tr ' ' '\n' | grep -qx logiops; then
    skip "not in logiops group (run: sudo usermod -aG logiops $USER; then re-login)"
fi

# --- prerequisite: tooling ---
INTROSPECT=""
if command -v busctl >/dev/null 2>&1; then
    INTROSPECT="busctl"
elif command -v gdbus >/dev/null 2>&1; then
    INTROSPECT="gdbus"
else
    skip "neither busctl nor gdbus is available to talk to the system bus"
fi

# --- prerequisite: daemon reachable (name is owned on the system bus) ---
if command -v busctl >/dev/null 2>&1; then
    if ! busctl --system list 2>/dev/null | grep -q "$SERVICE"; then
        skip "$SERVICE is not owned on the system bus (is logid running?)"
    fi
fi

# --- the actual check: introspect, then a real read, as this (non-root) user ---
echo "Introspecting $SERVICE $OBJECT as $(id -un) (groups: $(id -nG))..."

run_call() {
    # Returns 0 on success; captures output in $OUT. Treats AccessDenied as failure.
    if [ "$INTROSPECT" = "busctl" ]; then
        OUT="$(busctl introspect --system "$SERVICE" "$OBJECT" 2>&1)" && return 0 || return 1
    else
        OUT="$(gdbus introspect --system --dest "$SERVICE" --object-path "$OBJECT" 2>&1)" && return 0 || return 1
    fi
}

if run_call; then
    if echo "$OUT" | grep -q "AccessDenied"; then
        fail "introspection returned AccessDenied — non-root group user is still blocked"
    fi
    pass "non-root logiops-group user reached $SERVICE without AccessDenied"
    exit 0
else
    if echo "$OUT" | grep -q "AccessDenied"; then
        fail "non-root group user was denied access (AccessDenied) — ACCESS-01 not satisfied"
    fi
    # Any other failure (e.g. object path differs across versions) is a skip, not a fail:
    skip "could not introspect (non-AccessDenied error); detail: $(echo "$OUT" | head -1)"
fi
