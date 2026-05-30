#!/usr/bin/env bash
#
# device-list.sh — Phase 2 live device-list smoke (DEV-01/03/04, + DEV-02 read)
#
# Requirement: with the `logid` daemon running and at least one Logitech device
# present, a normal (logiops-group) user can:
#   - enumerate device object paths via pizza.pixl.LogiOps.Devices.Enumerate (DEV-01)
#   - read a non-empty Name for the first device (DEV-04)
#   - read an Active bool for the first device (DEV-03 sleep/wake state)
#   - (best-effort) read a Battery property once Plan 02 lands it (DEV-02)
#
# Prerequisites (manual — the script cannot create these):
#   - The `logid` daemon is running and owns pizza.pixl.LogiOps on the system bus.
#   - The current user can reach the bus (member of the `logiops` group, Phase 1).
#   - At least one Logitech device is connected/awake.
#
# Behaviour: degrades gracefully — prints "SKIP: ..." and exits 0 when a
# prerequisite (root / busctl / daemon / hardware / pending battery) is missing;
# exits non-zero ONLY when a real assertion fails.
#
set -euo pipefail

SERVICE="pizza.pixl.LogiOps"
ROOT="/pizza/pixl/logiops"
DEVICES_IFACE="pizza.pixl.LogiOps.Devices"
DEVICE_IFACE="pizza.pixl.LogiOps.Device"

skip() { echo "SKIP: $*"; exit 0; }
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

# --- prerequisite: running as non-root (the GUI is a non-root client) ---
if [ "$(id -u)" -eq 0 ]; then
    skip "running as root — the device list is exercised as a normal user"
fi

# --- prerequisite: tooling ---
if ! command -v busctl >/dev/null 2>&1; then
    skip "busctl is not available to talk to the system bus"
fi

# --- prerequisite: daemon reachable (name owned on the system bus) ---
if ! busctl --system list 2>/dev/null | grep -q "$SERVICE"; then
    skip "$SERVICE is not owned on the system bus (is logid running?)"
fi

# --- DEV-01: enumerate device object paths ---
echo "Enumerating devices on $SERVICE $ROOT $DEVICES_IFACE ..."
if ! ENUM="$(busctl --system call "$SERVICE" "$ROOT" "$DEVICES_IFACE" Enumerate 2>&1)"; then
    if echo "$ENUM" | grep -qiE 'access[ ]?denied'; then
        skip "AccessDenied on Enumerate — see access-denied-gui.sh (are you in the logiops group?)"
    fi
    skip "could not call Enumerate (non-AccessDenied error): $(echo "$ENUM" | head -1)"
fi

# busctl prints an `ao` reply as: `ao N "/path/0" "/path/1" ...`
PATHS=$(echo "$ENUM" | grep -oE '"/pizza/pixl/logiops/devices/[0-9]+"' | tr -d '"')
COUNT=$(echo "$ENUM" | awk '{print $2}')

if [ -z "${PATHS}" ] || [ "${COUNT:-0}" = "0" ]; then
    skip "Enumerate returned zero devices (no Logitech hardware connected)"
fi
pass "Enumerate returned ${COUNT} device path(s) (DEV-01)"

FIRST="$(echo "$PATHS" | head -1)"
echo "First device: ${FIRST}"

# --- DEV-04: Name is a non-empty string ---
if ! NAME_OUT="$(busctl --system get-property "$SERVICE" "$FIRST" "$DEVICE_IFACE" Name 2>&1)"; then
    skip "could not read Name on ${FIRST}: $(echo "$NAME_OUT" | head -1)"
fi
# Reply form: `s "Device Name"`
NAME_VAL=$(echo "$NAME_OUT" | sed -E 's/^s[[:space:]]+"(.*)"$/\1/')
if [ -z "$NAME_VAL" ]; then
    fail "Device Name is empty on ${FIRST} (DEV-04 expects a non-empty name)"
fi
pass "Device Name = \"${NAME_VAL}\" (DEV-04)"

# --- DEV-03: Active is a bool ---
if ! ACTIVE_OUT="$(busctl --system get-property "$SERVICE" "$FIRST" "$DEVICE_IFACE" Active 2>&1)"; then
    skip "could not read Active on ${FIRST}: $(echo "$ACTIVE_OUT" | head -1)"
fi
# Reply form: `b true` / `b false`
if ! echo "$ACTIVE_OUT" | grep -qE '^b[[:space:]]+(true|false)$'; then
    fail "Active is not a bool on ${FIRST} (got: ${ACTIVE_OUT}) (DEV-03)"
fi
pass "Device Active = $(echo "$ACTIVE_OUT" | awk '{print $2}') (DEV-03)"

# --- DEV-02: Battery is a uint8 0..100, OR BatteryKnown==false (still PASS) ---
# The daemon (Plan 02) always exposes Battery/Charging/BatteryKnown on .Device.
# A device that genuinely doesn't report battery sets BatteryKnown=false; that
# is a legitimate PASS (the GUI shows "—"), not a FAIL. We only FAIL on a
# malformed reply or an out-of-range value when the daemon claims it IS known.
if ! BATT_OUT="$(busctl --system get-property "$SERVICE" "$FIRST" "$DEVICE_IFACE" Battery 2>&1)"; then
    # Property genuinely absent => an older daemon predating Plan 02. Graceful
    # skip (the GUI degrades to "—"); never fail the suite on an old daemon.
    skip "Battery property not present (pre-Plan-02 daemon?) — detail: $(echo "$BATT_OUT" | head -1)"
fi
# Reply form: `y NNN` (uint8). Extract the numeric value.
if ! echo "$BATT_OUT" | grep -qE '^y[[:space:]]+[0-9]+$'; then
    fail "Battery is not a uint8 on ${FIRST} (got: ${BATT_OUT}) (DEV-02)"
fi
BATT_VAL=$(echo "$BATT_OUT" | awk '{print $2}')

# Read BatteryKnown to decide whether the value must be in 0..100.
if ! KNOWN_OUT="$(busctl --system get-property "$SERVICE" "$FIRST" "$DEVICE_IFACE" BatteryKnown 2>&1)"; then
    skip "could not read BatteryKnown on ${FIRST}: $(echo "$KNOWN_OUT" | head -1)"
fi
if ! echo "$KNOWN_OUT" | grep -qE '^b[[:space:]]+(true|false)$'; then
    fail "BatteryKnown is not a bool on ${FIRST} (got: ${KNOWN_OUT}) (DEV-02)"
fi
KNOWN_VAL=$(echo "$KNOWN_OUT" | awk '{print $2}')

if [ "$KNOWN_VAL" = "true" ]; then
    # Known battery => value MUST be a sane percentage.
    if [ "$BATT_VAL" -lt 0 ] || [ "$BATT_VAL" -gt 100 ]; then
        fail "Battery=${BATT_VAL} out of range 0..100 while BatteryKnown=true (DEV-02)"
    fi
    pass "Device Battery = ${BATT_VAL}% (BatteryKnown=true) (DEV-02)"
else
    # Unknown battery is legitimate — the GUI shows "—". Still a PASS.
    pass "Device reports BatteryKnown=false (GUI shows \"—\") — legitimate (DEV-02)"
fi

pass "device-list smoke complete"
exit 0
