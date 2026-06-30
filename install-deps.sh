https://github.com/cu-ecen-aeld/aesd-autotest-docker/blob/master/docker/Dockerfile

apt-get install -y \
    --no-install-recommends \
    ruby cmake git build-essential bsdmainutils valgrind sudo wget \
    gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
    bc u-boot-tools kmod cpio flex bison libssl-dev psmisc \
    qemu-system-arm