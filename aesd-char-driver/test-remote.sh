#!/usr/bin/env bash
set -xeuo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- Configuration ---
QEMU_HOST="localhost"
QEMU_PORT="10022"               # Ensure your QEMU command forwards port 2222 -> 22
QEMU_USER="root"
REMOTE_DIR="/tmp/mod_test"
MODULE_FILE="aesdchar.ko"
PASSWORD="root"

# Path to your Yocto SDK environment script (adjust if you use SDK)
SDK_ENV_SCRIPT="${SDK_ENV_SCRIPT:-/workspaces/Workspace/assignment-8-thebrave/poky/oe-init-build-env}"

# SSH/SCP options tailored for transient QEMU instances
SSH_OPTS=(
    -o StrictHostKeyChecking=no
    -o UserKnownHostsFile=/dev/null
    -o LogLevel=ERROR
    -p "${QEMU_PORT}"
)

# 1. Environment Setup
if [ -f "${SDK_ENV_SCRIPT}" ]; then
    # Disable nounset temporarily as Yocto SDK scripts sometimes rely on unset vars
    set +u
    source "${SDK_ENV_SCRIPT}"
    set -u
else
    echo "Error: Yocto SDK environment script not found at ${SDK_ENV_SCRIPT}"
    exit 1
fi

# 2. Build
echo "==> Cross-compiling kernel module..."
cd "${script_dir}" || exit 1
make -j"$(nproc)" modules

# 3. Sync & Execute
echo "==> Deploying to QEMU VM (${QEMU_HOST}:${QEMU_PORT})..."
ssh "${SSH_OPTS[@]}" "${QEMU_USER}@${QEMU_HOST}" "mkdir -p ${REMOTE_DIR}"

scp "${SSH_OPTS[@]}" "${MODULE_FILE}" .run_tests_guest.sh "${QEMU_USER}@${QEMU_HOST}:${REMOTE_DIR}/run_tests_guest.sh"

echo "==> Running guest tests..."
sshpass -p "${PASSWORD}" ssh "${SSH_OPTS[@]}" "${QEMU_USER}@${QEMU_HOST}" \
    "cd ${REMOTE_DIR} && chmod +x run_tests_guest.sh && ./run_tests_guest.sh"
