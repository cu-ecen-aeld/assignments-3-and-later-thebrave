#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

# Use ccache if available so it's only used when installed
if command -v ccache >/dev/null 2>&1; then
    CCACHE="ccache "
    echo "ccache found - will use ccache for compilation"
else
    CCACHE=""
    echo "ccache not found - building without ccache"
fi

CC="${CCACHE}${CROSS_COMPILE}gcc"

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    make distclean KBUILD_BUILD_TIMESTAMP=""
    make defconfig CROSS_COMPILE=${CROSS_COMPILE} ARCH=${ARCH} KBUILD_BUILD_TIMESTAMP="" CC="${CC}"
    make -j4 CROSS_COMPILE=${CROSS_COMPILE} ARCH=${ARCH} KBUILD_BUILD_TIMESTAMP="" CC="${CC}"
fi

echo "Adding the Image in outdir"
cp -v ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

mkdir -p "${OUTDIR}/rootfs/bin"
mkdir -p "${OUTDIR}/rootfs/dev"
mkdir -p "${OUTDIR}/rootfs/etc"
mkdir -p "${OUTDIR}/rootfs/lib"
mkdir -p "${OUTDIR}/rootfs/lib64"
mkdir -p "${OUTDIR}/rootfs/proc"
mkdir -p "${OUTDIR}/rootfs/sys"
mkdir -p "${OUTDIR}/rootfs/sbin"
mkdir -p "${OUTDIR}/rootfs/home/conf"

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    # Had issue with main busybox repo, so using Github mirror
    git clone https://github.com/mirror/busybox.git # git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
else
    cd busybox
fi

# TODO: Make and install busybox
make defconfig ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CC="${CC}"
make -j4 install ARCH=${ARCH} CONFIG_PREFIX="${OUTDIR}/rootfs" CROSS_COMPILE=${CROSS_COMPILE} CC="${CC}"

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cp -v /usr/local/arm-cross-compiler/install/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
cp -v /usr/local/arm-cross-compiler/install/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
cp -v /usr/local/arm-cross-compiler/install/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
cp -v /usr/local/arm-cross-compiler/install/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CC="${CC}"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CC="${CC}"

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -v ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home
cp -v ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home    
cp -v ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home
cp -v ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home
cp -v ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home
cp -v ${FINDER_APP_DIR}/conf/username.txt ${OUTDIR}/rootfs/home/conf
cp -v ${FINDER_APP_DIR}/conf/assignment.txt ${OUTDIR}/rootfs/home/conf

# TODO: Chown the root directory
chown root:root ${OUTDIR}/rootfs/

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio
