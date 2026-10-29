# riscv-on-c

[![CI](https://github.com/DenysLukianchuk/riscv-on-c/actions/workflows/ci.yml/badge.svg)](https://github.com/DenysLukianchuk/riscv-on-c/actions/workflows/ci.yml)

A RISC-V emulator in C (RV32 and RV64), built as a learning project: the aim
is to understand how a processor works by building one, test-first. Each
feature starts as a failing test written from the RISC-V specifications, and
the emulator follows the specifications rather than taking emulator
shortcuts.

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
- `satp` in Bare mode only, no address translation yet

Boots [OpenSBI](https://github.com/riscv-software-src/opensbi) v1.9: the
banner, its test payload in S-mode and a clean power-off.

Passes the official [riscv-tests](https://github.com/riscv-software-src/riscv-tests)
`ui`, `um` and `ua` suites on both XLENs, and 16 of 18 `mi` and 4 of 7 `si`
tests.

Next: the Sv39 MMU, then Linux.

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

## License

MIT
