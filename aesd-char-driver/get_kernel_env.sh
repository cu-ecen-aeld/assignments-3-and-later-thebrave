#!/usr/bin/env bash
# get_kernel_env.sh
# Usage: ./get_kernel_env.sh [/path/to/yocto/build_dir]

set -e

BUILD_DIR="${1:-$PWD}"

if [ ! -f "$BUILD_DIR/conf/local.conf" ]; then
    echo "Error: '$BUILD_DIR' does not appear to be a valid Yocto build directory."
    echo "Usage: $0 [/path/to/yocto/build_dir]"
    exit 1
fi

echo "==> Sourcing Yocto build environment..."
# Find poky/oe root directory relative to build dir
YOCTO_ROOT=$(cd "$BUILD_DIR/.." && pwd)

if [ -f "$YOCTO_ROOT/oe-init-build-env" ]; then
    source "$YOCTO_ROOT/oe-init-build-env" "$BUILD_DIR" > /dev/null
else
    echo "Error: Could not find oe-init-build-env in $YOCTO_ROOT"
    exit 1
fi

echo "==> Extracting environment variables from 'virtual/kernel'..."
ENV_DUMP=$(bitbake -e virtual/kernel)

# Parse relevant variables
eval "$(echo "$ENV_DUMP" | grep -E '^(ARCH|TARGET_PREFIX|STAGING_BINDIR_TOOLCHAIN|STAGING_KERNEL_BUILDDIR|STAGING_KERNEL_DIR)=')"

# Fallback for CROSS_COMPILE if not explicitly set
CROSS_COMPILE_PREFIX=$(echo "$ENV_DUMP" | grep '^CROSS_COMPILE=' | cut -d'"' -f2)
if [ -z "$CROSS_COMPILE_PREFIX" ]; then
    CROSS_COMPILE_PREFIX="$TARGET_PREFIX"
fi

OUTPUT_ENV_FILE="$BUILD_DIR/kernel-env.sh"

echo "==> Writing environment setup file to: $OUTPUT_ENV_FILE"

cat << EOF > "$OUTPUT_ENV_FILE"
# Auto-generated kernel cross-compilation environment
export ARCH="$ARCH"
export CROSS_COMPILE="$CROSS_COMPILE_PREFIX"
export PATH="$STAGING_BINDIR_TOOLCHAIN:\$PATH"
export KDIR="$STAGING_KERNEL_BUILDDIR"
export KERNEL_SRC="$STAGING_KERNEL_DIR"
export KERNEL_BUILD="$STAGING_KERNEL_BUILDDIR"

echo "Kernel Cross-Compile Environment Loaded:"
echo "  ARCH           = \$ARCH"
echo "  CROSS_COMPILE  = \$CROSS_COMPILE"
echo "  KDIR           = \$KDIR"
EOF

chmod +x "$OUTPUT_ENV_FILE"

echo ""
echo "Done! To start cross-compiling your kernel module, run:"
echo "  source $OUTPUT_ENV_FILE"
echo "  make -C \$KDIR M=/path/to/your/module/source modules"
