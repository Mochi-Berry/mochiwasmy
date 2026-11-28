/*
 * mochiwasmy — thin JS binding over the native x86_64 binary.
 * Loads lib/mochiwasmy_x86_64.dll via the engine's native loader and re-exports
 * the C ABI. Prefer native-first: keep heavy work in C, keep this file a
 * 1:1 binding (no object-wrapping layer).
 */
import { loadNative } from '../../mochinode/index.js';

let _m = null;
async function ensure() {
  if (!_m) _m = await loadNative('mochiwasmy.dll');
  return _m;
}

export async function init() { return ensure(); }
export function isNativeReady() { return _m != null; }

export function __exports() {
  return _m ? Object.keys(_m) : [];
}

export default {
  load: () => ensure(),
  exports: () => (_m ? Object.keys(_m) : []),
};
