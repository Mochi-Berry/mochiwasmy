#ifndef MB_MOCHIWASMY_H
#define MB_MOCHIWASMY_H
/*
 * mochiwasmy — MochiBerry native-first module.
 *
 * ABI contract (see mochiberry/module-loader-contract.md):
 *   - exports the single entry point mbjs_init_module(MBJSContext*, const char*)
 *   - registers C functions via NewCModule / AddModuleExportList / SetModuleExportList
 *   - C functions that throw MUST return MBJS_EXCEPTION (never MBJS_UNDEFINED)
 *
 * Runtime symbols (MBJS_*) are resolved through mbjs_runtime_resolve() from
 * <root>/native/mbjs_runtime.h at load time; do NOT link against mbjs.exe.
 *
 * Exported symbols (measured): ping, load, call
 */
#include <stdint.h>

typedef struct MBJSContext MBJSContext;
typedef struct MBJSModuleDef MBJSModuleDef;

MBJSModuleDef *mbjs_init_module(MBJSContext *ctx, const char *name);

#endif /* MB_MOCHIWASMY_H */
