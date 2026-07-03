// hle_graphics.cpp — HEADLESS graphics bring-up.
//
// The PS5 GPU stack — libSceAgc (low-level GPU command building) and libSceVideoOut (display /
// flip) — is not yet translated to the host GPU (the eventual AGC->Vulkan + RDNA2 shader
// recompiler work, milestones M4/M5). Until then, these HLE handlers provide *valid but empty*
// graphics objects so the game's own logic (init, main loop, simulation) executes headlessly.
// This lets us map the real graphics call sequence to translate, and prove the runtime runs.
//
// This is NOT rendering and NOT faking visible output: object getters return zeroed buffers the
// guest can read/write without faulting; action/submit calls are no-ops. Every entry here is a
// placeholder to be replaced by a real translation. Graphics functions live in sparsely-documented
// Sony libs, so they're registered by raw NID with a note on the observed role.
#include "dispatch.hpp"
#include "nid.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace prosper {

#define HLE(name) static uint64_t name(uint64_t a0, uint64_t a1, uint64_t a2, \
                                       uint64_t a3, uint64_t a4, uint64_t a5)

namespace {
// A getter that returns a pointer the guest dereferences: hand back a per-callsite singleton
// zeroed buffer (big enough that field reads/writes at any small offset stay in-bounds). Singleton
// so repeated "get the device/context" calls see a stable object, as the guest expects.
uint64_t agc_singleton(int slot) {
    static void* objs[16] = {nullptr};
    if (slot < 0 || slot >= 16) slot = 0;
    if (!objs[slot]) objs[slot] = calloc(1, 64 * 1024);
    return (uint64_t)(uintptr_t)objs[slot];
}
}

HLE(g_agc_dev)  { return agc_singleton(0); }   // libSceAgc 2JtWUUiYBXs — object dereferenced at +0x38
HLE(g_agc_ctx)  { return agc_singleton(1); }   // libSceAgc wRbq6ZjNop4 — object dereferenced at +0x38

// --- libSceVideoOut (display / frame presentation). Headless: accept opens/flips and simulate
// flip completion so the game's render loop advances (submit -> wait completion -> submit next).
namespace {
    int g_vo_handle = 0;
    uint64_t g_flip_count = 0;   // incremented per SubmitFlip so GetFlipStatus shows progress
}
HLE(g_vo_open)        { return (uint64_t)(int64_t)(++g_vo_handle + 0x1000); }  // positive handle
HLE(g_vo_close)       { return 0; }
HLE(g_vo_submitflip)  { g_flip_count++; return 0; }                            // accept the flip
HLE(g_vo_flippending) { return 0; }                                            // never pending -> can submit next
HLE(g_vo_flipstatus)  { // (handle, SceVideoOutFlipStatus* status): report our simulated flip count.
    // SceVideoOutFlipStatus is exactly 0x40 bytes — writing more smashes the caller's stack canary!
    if (a1) { uint8_t* s = (uint8_t*)(uintptr_t)a1; memset(s, 0, 0x40);
              *(uint64_t*)(s + 0x00) = g_flip_count;          // count
              *(int64_t*) (s + 0x18) = (int64_t)g_flip_count; // flipArg
              *(int32_t*) (s + 0x38) = 0; }                   // currentBuffer
    return 0;
}
HLE(g_vo_resstatus)   { if (a1) memset((void*)(uintptr_t)a1, 0, 0x20); return 0; }  // SceVideoOutResolutionStatus ~0x20

void register_graphics_hle() {
    #define RN(nid, fn) Hle::register_fn(nid, (HleFn)(fn), nid)   // raw NID (graphics libs undocumented)
    #define R(str, fn)  Hle::register_fn(nid_hash(str), (HleFn)(fn), str)
    // libSceAgc getters whose results the guest dereferences → return stable zeroed objects.
    RN("2JtWUUiYBXs", g_agc_dev);
    RN("wRbq6ZjNop4", g_agc_ctx);
    // libSceVideoOut display / flip
    R("sceVideoOutOpen", g_vo_open);            R("sceVideoOutClose", g_vo_close);
    R("sceVideoOutSubmitFlip", g_vo_submitflip);R("sceVideoOutIsFlipPending", g_vo_flippending);
    R("sceVideoOutSetFlipRate", g_vo_close);    R("sceVideoOutAddFlipEvent", g_vo_close);
    R("sceVideoOutGetFlipStatus", g_vo_flipstatus);
    R("sceVideoOutGetResolutionStatus", g_vo_resstatus);
    R("sceVideoOutRegisterBuffers", g_vo_close);R("sceVideoOutSetBufferAttribute", g_vo_close);
    #undef R
    #undef RN
}

} // namespace prosper
