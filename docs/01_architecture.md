# 01 — Architecture

`mochiwasmy` is a **single package repo**. Installed by TGPM/npm it lands whole at
`node_modules/mochiwasmy`. There are no sub-modules inside — one root `manifest.json`
describes the package; each layer carries its own `manifest.json` for the
engine loader.

```
mochiwasmy/
├── package.json / package-lock.json   <- package manager (npm/TGPM)
├── manifest.json                      <- package descriptor (root; replaces berry.json)
├── CMakeLists.txt / Makefile          <- build plumbing (ENGINE_INC overridable)
├── LICENSE / README.md / .gitignore
├── mochiberry/   svg/                 <- asset & documentation diagrams (no mascots)
├── docs/         01_architecture.md · 02_abi_reference.md · 03_sandbox_security.md
├── node_modules/ manifest.json (layer js_face) + src/index.js + native/ + lib/
└── mochi_modules/ manifest.json (layer native_core) + include/ + src/ + native/
                  + script/ + lib/<m>_x86_64.dll
```

## Loader contract (verified from engine source + disassembly)

- a native plugin exports exactly one symbol: `mbjs_init_module`;
- the import specifier must end in `.dll` / `.so` / `.lbrct` for the loader
  to take the native path;
- `MBJS_AddModuleExportList` is called inside `mbjs_init_module`,
  `MBJS_SetModuleExportList` in the module init func;
- the plugin MUST NOT link against `mbjs.exe` (an EXE cannot be a DLL
  dependency → ERROR_MOD_NOT_FOUND 126); all `MBJS_*` entry points are
  resolved from the host image at load time via `mbjs_runtime.h`.

## Status

| field | value |
|---|---|
| status | `stub` |
| runtime exports | 1 |
| native binary | `mochi_modules/lib/mochiwasmy_x86_64.dll` (57039 B) |

## Build

```sh
make ENGINE_INC=/path/to/engine/native-dir    # dir containing mbjs.h + mbjs_runtime.h
# or: cmake -DENGINE_INC=/path/to/engine/native-dir .
```

## Use (JS)

```js
import * as mochiwasmy from 'mochiwasmy';        // -> node_modules/src/index.js
```
