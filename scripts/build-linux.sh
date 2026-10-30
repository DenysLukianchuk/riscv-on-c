#!/bin/sh
# Builds a Linux image for the emulator: the kernel with its user space in an
# initramfs, inside OpenSBI's fw_payload so that the emulator loads a single
# ELF. Everything is built for rv64imac, without floating-point instructions.
#
# Usage: scripts/build-linux.sh [hello|shell] [work-dir]
#   hello  a nolibc /init that greets from U-mode and powers off (default;
#          about a minute)
#   shell  busybox and musl from Buildroot, with a login on the console
#          (about ten minutes the first time, as Buildroot builds its own
#          compiler)
# Result: <work-dir>/fw_payload.elf and the kernel's <work-dir>/linux.config;
# the default work-dir is build/linux-<image>.
set -eu

LINUX_VERSION=6.18.59
LINUX_SHA256=53a55ac9863a5547e292b8e381d4e4c2240dd1127d5428a7c44dbc550be23963
OPENSBI_VERSION=v1.9
BUILDROOT_VERSION=2026.08.1
CROSS=riscv64-linux-gnu-
ISA=rv64imac_zicsr_zifencei
JOBS=$(nproc)

image=${1:-hello}
case $image in
hello | shell) ;;
*)
	echo "usage: $0 [hello|shell] [work-dir]" >&2
	exit 2
	;;
esac
scripts=$(realpath "$(dirname "$0")")
work=$(realpath -m "${2:-build/linux-$image}")
mkdir -p "$work"
cd "$work"

linux=linux-$LINUX_VERSION
if [ ! -d $linux ]; then
	curl -sSfLO https://cdn.kernel.org/pub/linux/kernel/v6.x/$linux.tar.xz
	echo "$LINUX_SHA256  $linux.tar.xz" | sha256sum -c -
	tar xf $linux.tar.xz
fi
if [ ! -d opensbi ]; then
	git clone -q --depth 1 --branch $OPENSBI_VERSION \
		https://github.com/riscv-software-src/opensbi.git
fi

# User space, as a list or an archive for the kernel's initramfs.
if [ $image = hello ]; then
	# nolibc is the small C library that ships with the kernel sources.
	mkdir -p nolibc
	make -s -C $linux/tools/include/nolibc ARCH=riscv headers_standalone \
		OUTPUT="$work/nolibc/"
	make -s -C $linux ARCH=riscv mrproper # the step above leaves files behind
	cat > init.c <<'EOF'
int main(void)
{
	printf("\nHello from user space on riscv-on-c!\n");
	reboot(LINUX_REBOOT_CMD_POWER_OFF);
	return 0;
}
EOF
	${CROSS}gcc -Os -static -fno-pie -no-pie -nostdlib \
		-fno-asynchronous-unwind-tables -march=$ISA -mabi=lp64 \
		-include nolibc/sysroot/include/nolibc.h -Inolibc/sysroot/include \
		init.c -lgcc -o init.elf
	cat > initramfs.list <<EOF
dir /dev 755 0 0
nod /dev/console 600 0 0 c 5 1
file /init $work/init.elf 755 0 0
EOF
	initramfs="$work/initramfs.list"
else
	if [ ! -d buildroot ]; then
		git clone -q --depth 1 --branch $BUILDROOT_VERSION \
			https://gitlab.com/buildroot.org/buildroot.git
	fi
	make -s -C buildroot O="$work/buildroot-build" \
		BR2_DEFCONFIG="$scripts/buildroot_defconfig" defconfig
	make -C buildroot O="$work/buildroot-build" -j"$JOBS" > buildroot.log
	initramfs="$work/buildroot-build/images/rootfs.cpio"
fi

# The kernel: tinyconfig plus the console, the PLIC and the initramfs.
kmake="make -s -C $linux ARCH=riscv CROSS_COMPILE=$CROSS O=$work/kernel"
$kmake tinyconfig
config="$linux/scripts/config --file kernel/.config"
$config \
	-e 64BIT -e NONPORTABLE -d EFI -e RISCV_ISA_C -d FPU -d RISCV_ISA_V \
	-e PRINTK -e PRINTK_TIME -e TTY -e SERIAL_8250 -e SERIAL_8250_CONSOLE \
	-e SERIAL_OF_PLATFORM -e SERIAL_EARLYCON -e SERIAL_EARLYCON_RISCV_SBI \
	-e HVC_RISCV_SBI -e RISCV_SBI_V01 -e SIFIVE_PLIC -e BINFMT_ELF \
	-e BLK_DEV_INITRD --set-str INITRAMFS_SOURCE "$initramfs" \
	--set-str CMDLINE "earlycon=sbi console=ttyS0"
if [ $image = shell ]; then
	# What busybox's init, its scripts and musl expect of the kernel.
	$config \
		-e PROC_FS -e PROC_SYSCTL -e SYSFS -e TMPFS -e DEVTMPFS \
		-e DEVTMPFS_MOUNT -e BINFMT_SCRIPT -e MULTIUSER -e FUTEX -e EPOLL \
		-e SIGNALFD -e TIMERFD -e EVENTFD -e POSIX_TIMERS -e FILE_LOCKING \
		-e SHMEM -e INOTIFY_USER -e UNIX98_PTYS -e NET -e UNIX -e INET
fi
$kmake olddefconfig
$kmake -j"$JOBS" Image
cp kernel/.config linux.config

# OpenSBI with the kernel as its payload, linked at the start of RAM.
make -s -C opensbi -j"$JOBS" CROSS_COMPILE=$CROSS PLATFORM=generic \
	PLATFORM_RISCV_ISA=$ISA PLATFORM_RISCV_ABI=lp64 \
	FW_TEXT_START=0x80000000 \
	FW_PAYLOAD_PATH="$work/kernel/arch/riscv/boot/Image" \
	O="$work/opensbi-build"
cp opensbi-build/platform/generic/firmware/fw_payload.elf fw_payload.elf

echo "$work/fw_payload.elf"
