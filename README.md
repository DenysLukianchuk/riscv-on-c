# riscv-on-c

[![CI](https://github.com/DenysLukianchuk/riscv-on-c/actions/workflows/ci.yml/badge.svg)](https://github.com/DenysLukianchuk/riscv-on-c/actions/workflows/ci.yml)

A RISC-V emulator in C (RV32 and RV64). This is a learning project: the aim
is to understand how a processor works by building one. The emulator follows
the RISC-V specifications rather than taking emulator shortcuts.

It is written with test-driven development (TDD). The tests were written with
[Claude Code](https://claude.com/claude-code), from the specifications; the
emulator was then written against them. Each feature starts as a failing
test, then gets the smallest code that makes it pass, then a refactoring with
the tests kept green. The unit tests take their architectural numbers (CSR
addresses, bit positions, trap causes) from their own header rather than from
the emulator's, so a wrong constant in the emulator fails a test instead of
agreeing with it.

## Status

- RV32I / RV64I base ISA, M, A (LR/SC, AMOs), F and D (single and double
  precision floating point) and C (compressed instructions) extensions
- F and D: all five rounding modes, the accrued exception flags in `fcsr`,
  NaN-boxing and the canonical NaN, and `mstatus.FS` / `SD`, by which the
  kernel saves the registers only when a process has changed them. The
  host FPU does the arithmetic; where its results differ from what RISC-V
  requires (NaN payloads, round-to-nearest-max-magnitude, `fcvt` saturation,
  `fmin`/`fmax` and the zeros) the emulator corrects them
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
- Time follows the host's clock at the 10 MHz the device tree announces, and
  a hart waiting in `wfi` lets the host sleep until its timer is due or a key
  arrives, so an idle guest costs no host CPU
- ELF loader; the board describes itself to the guest in a flattened device
  tree, and the hart starts by the RISC-V boot protocol: M-mode, hart ID in
  `a0`, device tree address in `a1`
- Virtual memory: Sv39 on RV64 and Sv32 on RV32 with superpages, R/W/X and
  U permissions, `mstatus` SUM, MXR, MPRV and TVM, A/D bits managed by
  software (a page fault when they need setting), and `sfence.vma`

Boots [OpenSBI](https://github.com/riscv-software-src/opensbi) v1.9: the
banner, its test payload in S-mode and a clean power-off.

Boots Linux 6.18 to a busybox shell in about five seconds: OpenSBI hands
over to the kernel, which brings up paging, the timer, the PLIC and the
16550 console, then busybox's init starts its services and a login on the
console. Keys typed at the host terminal reach the guest through the UART.

```
Welcome to Linux on riscv-on-c
riscv-on-c login: root
# uname -a
Linux riscv-on-c 6.18.59 #1 riscv64 GNU/Linux
# cat /proc/cpuinfo
processor	: 0
hart		: 0
isa		: rv64imac_zicntr_zicsr_zifencei_zaamo_zalrsc_zca_svade
mmu		: sv39
```

Runs [Alpine Linux](https://alpinelinux.org) 3.24, a distribution built for
rv64gc, whose musl and busybox use the floating-point instructions:

```
Welcome to Alpine Linux 3.24
Kernel 6.18.59 on riscv64 (/dev/ttyS0)

riscv-on-c login: root
riscv-on-c:~# cat /proc/cpuinfo | grep ^isa
isa		: rv64imafdc_zicntr_zicsr_zifencei_zaamo_zalrsc_zca_zcd_svade
riscv-on-c:~# awk 'BEGIN { printf "%.10f\n", sqrt(2) * atan2(0, -1) }'
4.4428829382
```

Passes the official [riscv-tests](https://github.com/riscv-software-src/riscv-tests)
`ui`, `um`, `ua`, `uf`, `ud` and `uc` suites on both XLENs, in physical memory and again in
U-mode under paging, along with 16 of 18 `mi` and all 7 `si` tests.

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
     PLATFORM_RISCV_ISA=rv64imac_zicsr_zifencei PLATFORM_RISCV_ABI=lp64 \
     FW_TEXT_START=0x80000000
```

Then run its firmware with the test payload on an optimised build of the
emulator:

```sh
cmake --preset rv64-release && cmake --build --preset rv64-release
build/rv64-release/riscv path/to/opensbi/build/platform/generic/firmware/fw_payload.elf
```

## Running Linux

`scripts/build-linux.sh` builds a Linux image for the emulator. The kernel
(6.18, from `tinyconfig` plus what the board and user space need) travels
inside OpenSBI's `fw_payload`, so the emulator loads a single ELF, and its
user space sits in an initramfs. The script downloads pinned
versions of its sources and needs `riscv64-linux-gnu-gcc`, `flex`, `bison`
and `bc`; the shell image also needs what Buildroot needs, such as `g++` and
a few Perl modules.

There are three images:

- `alpine`: the [Alpine Linux](https://alpinelinux.org) mini root filesystem,
  built by Alpine for rv64gc, with a login on the console. The kernel saves
  and restores the floating-point registers. There is no disk yet, so the
  whole system lives in RAM and changes are lost at power-off. About a minute.
- `shell`: busybox and musl from [Buildroot](https://buildroot.org), built for
  rv64imac without floating point, with a login on the console. About ten
  minutes the first time, as Buildroot builds its own compiler.
- `hello`: a `/init` built against nolibc, the small C library in the kernel
  sources, that greets from U-mode and powers the board off. About a minute.

```sh
cmake --preset rv64-release && cmake --build --preset rv64-release
scripts/build-linux.sh alpine           # result in build/linux-alpine
build/rv64-release/riscv build/linux-alpine/fw_payload.elf
```

The board has 128 MiB of RAM; `-m` changes that, in megabytes or with a K,
M or G suffix, as in `riscv -m 1G image.elf`. On RV32 RAM can be up to 2 GiB,
as it starts at `0x80000000` and has to end below 4 GiB.

Log in as `root` without a password. The terminal is in raw mode while the
emulator runs, so Ctrl-C goes to the guest; press Ctrl-A then x to quit the
emulator.

To skip the build, take `riscv` and `alpine.elf` from a
[release](https://github.com/DenysLukianchuk/riscv-on-c/releases) and run
`./riscv alpine.elf`.

## License

MIT
