# 03 — Sandbox & Security (mochiwasmy)

## Native plugin isolation rules

1. A plugin MUST NOT link/import `mbjs.exe` — all `MBJS_*` symbols are
   resolved at runtime through `mbjs_runtime.h` (prevents the plugin from
   bypassing the engine).
2. A plugin runs inside an engine-bounded realm; the isolation primitives
   live in the core, not the module:
   - `MBJS_SetMemoryLimit` (hard RAM cap per runtime)
   - `MBJS_SetInterruptHandler` (instruction budget — kills `while(true)`)
   - `MBJS_SetMaxStackSize` (stack cap per context)
3. For untrusted code, use `mochisandbox` (sibling repo): a fresh realm per
   sandbox, globals injected explicitly via `setGlobal()`, `eval` neutralized
   in strict mode.

## Export policy for this module

1 runtime exports. The engine loader reads
`mochi_modules/manifest.json` (not `package.json`) to map symbols — sandbox
mode can block specific exports before binding happens.
