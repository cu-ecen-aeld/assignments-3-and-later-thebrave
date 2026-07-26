#!/usr/bin/env sh
set -e

MODULE_NAME="my_module"
KO_FILE="${MODULE_NAME}.ko"

echo "[VM] Cleaning up old module instance..."
if lsmod | grep -q "^${MODULE_NAME}"; then
    rmmod "${MODULE_NAME}"
fi

echo "[VM] Clearing dmesg ring buffer..."
dmesg -c > /dev/null

echo "[VM] Loading ${KO_FILE}..."
insmod "${KO_FILE}"

echo "[VM] --- Kernel Output (dmesg) ---"
dmesg

echo "[VM] Executing user-space tests..."
# Insert your test assertions here (e.g., reading /dev or /proc entries)
# example: cat /dev/my_device_node

echo "[VM] Unloading module..."
rmmod "${MODULE_NAME}"

echo "[VM] SUCCESS: Module test sequence completed cleanly."
