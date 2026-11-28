# mochiwasmy (mochi_modules) — native core

Native-first half of this module package: the C source, the prebuilt binary,
the public ABI headers, and the build file. The JS face lives in
`../node_modules/` (see its README.md).

## Layout

```text
mochi_modules/
├── README.md          this file
├── manifest.json      {"layer": "native_core"} — targets, exports, sources
├── CMakeLists.txt     builds lib/mochiwasmy_x86_64.dll from src/
├── include/           public ABI headers
├── src/               C sources (engine plugin)
├── native/            extra native sources, if any
├── script/            shim/inspection helpers, if any
└── lib/               prebuilt mochiwasmy_x86_64.dll (ready to load)
```

## ABI contract

* The plugin exports exactly one entry point: `mbjs_init_module`.
* Runtime `MBJS_*` symbols are resolved from the host engine at load time via
  `mbjs_runtime_resolve()` — never link against `mbjs.exe` (importing it
  fails with error 126 on Windows).
* C functions that throw MUST return `MBJS_EXCEPTION`, never `MBJS_UNDEFINED`.
* Specifier imports must end in `.dll`/`.so`/`.dylib` (arch-suffixed name).

## Build

```bash
cmake -S . -B build -DMBJS_INC=E:/bcf/mochiberry   # engine headers
cmake --build build
# output: lib/mochiwasmy_x86_64.dll
```

Requires `bcrypt` and `winhttp` on Windows (linked automatically).
