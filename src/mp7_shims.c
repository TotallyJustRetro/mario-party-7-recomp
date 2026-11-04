/* mp7_shims.c — Game-specific shims for Mario Party 7 recomp.
 *
 * orig_OSSaveContext:
 *   The runtime (threads.cpp) declares:
 *     extern "C" void orig_OSSaveContext(CPU* c);
 *   The recompiler emits the body as fn_800A01B0 (cname always uses fn_XXXXXXXX).
 *   This alias satisfies the linker without modifying the recompiler or runtime.
 */
#include "recomp_funcs.h"

void fn_800A01B0(CPU* c);           /* forward decl of recompiled body */
void orig_OSSaveContext(CPU* c) { fn_800A01B0(c); }
