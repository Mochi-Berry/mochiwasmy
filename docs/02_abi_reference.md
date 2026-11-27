# 02 — ABI Reference (mochiwasmy)

- Native entry point: `mbjs_init_module()`
- ABI version: `MBC1`
- Header ABI:
- `include/mochiwasmy.h`
- `include/mochiwasmy_abi.h`

## Runtime exports (1 — from manifest.json)

- `ping`

Symbols above are exposed by `mochi_modules/src/mochiwasmy.c` through
`MBJS_AddModuleExportList()` inside `mbjs_init_module()`; the values are
recorded by `MBJS_SetModuleExportList()` in the module init func.

## Build variables (no absolute host paths)

- `ENGINE_INC`: directory holding `mbjs.h` + `mbjs_runtime.h` (default
  `../libmochiberry`; override at build time).
- Windows link libs: (none additional).
