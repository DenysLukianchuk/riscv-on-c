# riscv-on-c

[![CI](https://github.com/DenysLukianchuk/riscv-on-c/actions/workflows/ci.yml/badge.svg)](https://github.com/DenysLukianchuk/riscv-on-c/actions/workflows/ci.yml)

A RISC-V emulator in C (RV32 and RV64), built as a learning project: the aim
is to understand how a processor works by building one. The emulator follows
the RISC-V specifications rather than taking emulator shortcuts.

It is written with test-driven development (TDD). Each feature starts as a
failing test written from the specifications, then gets the smallest code
that makes it pass, then a refactoring with the tests kept green. The unit
tests take their architectural numbers (CSR addresses, bit positions, trap
causes) from their own header rather than from the emulator's, so a wrong
constant in the emulator fails a test instead of agreeing with it.

## Status

- RV32I / RV64I base ISA, M and A (LR/SC, AMOs) extensions
- Zicsr; `cycle` / `instret` counters, 64-bit on RV32
- Privilege modes M, S and U: traps, delegation (`medeleg`, `mideleg`),
  `mret` / `sret`, `ecall` / `ebreak` / `wfi`, `misa`, `mcounteren` /
  `scounteren`
- Interrupts: `mip` / `mie` and their S-mode views, priorities, delegation to
  S-mode, vectored `mtvec` / `stvec`; external interrupts through a PLIC, with
  `mip.SEIP` read as the software bit ORed with the PLIC's line
- Sdtrig debug triggers: mcontrol and mcontrol6 address and data triggers with
  match modes, access sizes and chains; exception (etrigger) and instruction
  count (icount) triggers
- A board laid out like QEMU's `virt`, on a bus that maps address windows to
  devices: RAM at `0x80000000`, a CLINT timer at `0x02000000`, a PLIC at
  `0x0c000000`, an NS16550A UART with interrupts at `0x10000000`, and a test
  finisher at `0x00100000` through which the guest powers the board off with
  an exit code
- ELF loader; the board describes itself to the guest in a flattened device
  tree, and the hart starts by the RISC-V boot protocol: M-mode, hart ID in
  `a0`, device tree address in `a1`
- Virtual memory: Sv39 on RV64 and Sv32 on RV32 with superpages, R/W/X and
  U permissions, `mstatus` SUM, MXR, MPRV and TVM, A/D bits managed by
  software (a page fault when they need setting), and `sfence.vma`

Boots [OpenSBI](https://github.com/riscv-software-src/opensbi) v1.9: the
banner, its test payload in S-mode and a clean power-off.

Boots Linux 6.18 to user space: OpenSBI hands over to the kernel, which
brings up paging, the timer, the PLIC and the 16550 console, then runs an
`/init` from its initramfs that prints a greeting in U-mode and powers the
board off. There is no shell yet.

Passes the official [riscv-tests](https://github.com/riscv-software-src/riscv-tests)
`ui`, `um` and `ua` suites on both XLENs, in physical memory and again in
U-mode under paging, along with 16 of 18 `mi` and all 7 `si` tests.

Next: a Linux shell.

## Build and test

Requires CMake 3.25+, Ninja and GCC or Clang. The official tests are built
when a RISC-V cross compiler (`riscv64-linux-gnu-gcc`) is installed.

```sh
cmake --workflow --preset rv64
cmake --workflow --preset rv32
```

## Running OpenSBI

Build OpenSBI for the emulator's ISA, linked at the start of RAM:

```sh
git clone --branch v1.9 https://github.com/riscv-software-src/opensbi.git
cd opensbi
make CROSS_COMPILE=riscv64-linux-gnu- PLATFORM=generic \
     PLATFORM_RISCV_ISA=rv64ima_zicsr_zifencei PLATFORM_RISCV_ABI=lp64 \
     FW_TEXT_START=0x80000000
```

Then run its firmware with the test payload on an emulator built without
sanitizers:

```sh
cmake --preset rv64-nosan && cmake --build --preset rv64-nosan
build/rv64-nosan/riscv path/to/opensbi/build/platform/generic/firmware/fw_payload.elf
```

## Running Linux

The kernel is built without compressed and floating-point instructions,
which the emulator does not implement yet, and travels inside OpenSBI's
`fw_payload` image, so the emulator still loads a single ELF.

A first `/init`, built against nolibc, the small C library that ships with
the kernel sources:

```sh
# in the Linux source tree (6.18)
mkdir -p ../nolibc
make -C tools/include/nolibc ARCH=riscv headers_standalone OUTPUT=$PWD/../nolibc/
make ARCH=riscv mrproper   # the step above leaves files in the tree

cat > ../init.c <<'EOF'
int main(void)
{
	printf("\nHello from user space on riscv-on-c!\n");
	reboot(LINUX_REBOOT_CMD_POWER_OFF);
	return 0;
}
EOF
riscv64-linux-gnu-gcc -Os -static -nostdlib -fno-asynchronous-unwind-tables \
    -march=rv64ima_zicsr_zifencei -mabi=lp64 \
    -include ../nolibc/sysroot/include/nolibc.h -I../nolibc/sysroot/include \
    ../init.c -lgcc -o ../init.elf

cat > ../initramfs.list <<EOF
dir /dev 755 0 0
nod /dev/console 600 0 0 c 5 1
file /init $PWD/../init.elf 755 0 0
EOF
```

A minimal kernel for the board: `tinyconfig` plus the console, the PLIC and
the initramfs. EFI has to go, as it forces compressed instructions on.

```sh
K="make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- O=../build"
$K tinyconfig
scripts/config --file ../build/.config \
    -e 64BIT -e NONPORTABLE -d EFI -d RISCV_ISA_C -d FPU -d RISCV_ISA_V \
    -e PRINTK -e PRINTK_TIME -e TTY -e SERIAL_8250 -e SERIAL_8250_CONSOLE \
    -e SERIAL_OF_PLATFORM -e SERIAL_EARLYCON -e SERIAL_EARLYCON_RISCV_SBI \
    -e HVC_RISCV_SBI -e RISCV_SBI_V01 -e SIFIVE_PLIC -e BINFMT_ELF \
    -e BLK_DEV_INITRD --set-str INITRAMFS_SOURCE $PWD/../initramfs.list \
    --set-str CMDLINE "earlycon=sbi console=ttyS0"
$K olddefconfig
$K -j$(nproc) Image
```

OpenSBI with the kernel as its payload, then the run:

```sh
# in the OpenSBI tree
make CROSS_COMPILE=riscv64-linux-gnu- PLATFORM=generic \
     PLATFORM_RISCV_ISA=rv64ima_zicsr_zifencei PLATFORM_RISCV_ABI=lp64 \
     FW_TEXT_START=0x80000000 \
     FW_PAYLOAD_PATH=path/to/build/arch/riscv/boot/Image O=build-linux

build/rv64-nosan/riscv path/to/opensbi/build-linux/platform/generic/firmware/fw_payload.elf
```

The boot takes a few seconds and ends with the greeting and
`reboot: Power down`.

## License

MIT
