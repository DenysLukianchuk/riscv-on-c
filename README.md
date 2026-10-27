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
- Privilege modes M, S and U: traps, delegation (`medeleg`), `mret` / `sret`,
  `ecall` / `ebreak` / `wfi`, `misa`, `mcounteren` / `scounteren`
- Sdtrig debug triggers: mcontrol and mcontrol6 address and data triggers with
  match modes, access sizes and chains; exception (etrigger) and instruction
  count (icount) triggers
- ELF loader

Passes the official [riscv-tests](https://github.com/riscv-software-src/riscv-tests)
`ui`, `um` and `ua` suites on both XLENs, and 16 of 18 `mi` and 4 of 7 `si`
tests.

Next: interrupts and the CLINT timer, UART, PLIC and a device tree, to boot
OpenSBI and then Linux.

## Build and test

Requires CMake 3.25+, Ninja and GCC or Clang. The official tests are built
when a RISC-V cross compiler (`riscv64-linux-gnu-gcc`) is installed.

```sh
cmake --workflow --preset rv64
cmake --workflow --preset rv32
```

## License

MIT
