// hle_libc.cpp — HLE implementations of the common libc functions the guest imports.
// The guest ABI == host SysV ABI, so most of these are thin thunks straight to the
// host C library. Registered by NID so the loader binds imports directly to them.
#include "dispatch.hpp"
#include "nid.hpp"
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cctype>
#ifdef _WIN32
#include <malloc.h>   // _aligned_malloc
#endif

namespace prosper {
// Portable aligned allocation (POSIX posix_memalign / Windows _aligned_malloc).
// NOTE: on Windows these need _aligned_free; the guest doesn't run on Windows yet,
// so h_free's plain free() is fine for now (Linux is the runtime target).
static void* aligned_alloc_portable(size_t align, size_t size) {
#ifdef _WIN32
    return _aligned_malloc(size, align);
#else
    void* p = nullptr;
    return posix_memalign(&p, align, size) == 0 ? p : nullptr;
#endif
}
} // namespace prosper

namespace prosper {

// All handlers use the full 6-arg HLE signature; extras are ignored (SysV-safe).
#define HLE(name) static uint64_t name(uint64_t a0, uint64_t a1, uint64_t a2, \
                                       uint64_t a3, uint64_t a4, uint64_t a5)
#define P(x) ((void*)(uintptr_t)(x))
#define CP(x) ((const void*)(uintptr_t)(x))
#define CS(x) ((const char*)(uintptr_t)(x))

HLE(h_memcpy)  { return (uint64_t)(uintptr_t)memcpy(P(a0), CP(a1), a2); }
HLE(h_memmove) { return (uint64_t)(uintptr_t)memmove(P(a0), CP(a1), a2); }
HLE(h_memset)  { return (uint64_t)(uintptr_t)memset(P(a0), (int)a1, a2); }
HLE(h_memcmp)  { return (uint64_t)(int64_t)memcmp(CP(a0), CP(a1), a2); }
HLE(h_memchr)  { return (uint64_t)(uintptr_t)memchr(CP(a0), (int)a1, a2); }
HLE(h_strlen)  { return (uint64_t)strlen(CS(a0)); }
HLE(h_strnlen) { return (uint64_t)strnlen(CS(a0), a1); }
HLE(h_strcmp)  { return (uint64_t)(int64_t)strcmp(CS(a0), CS(a1)); }
HLE(h_strncmp) { return (uint64_t)(int64_t)strncmp(CS(a0), CS(a1), a2); }
HLE(h_strcpy)  { return (uint64_t)(uintptr_t)strcpy((char*)P(a0), CS(a1)); }
HLE(h_strncpy) { return (uint64_t)(uintptr_t)strncpy((char*)P(a0), CS(a1), a2); }
HLE(h_strcat)  { return (uint64_t)(uintptr_t)strcat((char*)P(a0), CS(a1)); }
HLE(h_strncat) { return (uint64_t)(uintptr_t)strncat((char*)P(a0), CS(a1), a2); }
HLE(h_strchr)  { return (uint64_t)(uintptr_t)strchr(CS(a0), (int)a1); }
HLE(h_strrchr) { return (uint64_t)(uintptr_t)strrchr(CS(a0), (int)a1); }
HLE(h_strstr)  { return (uint64_t)(uintptr_t)strstr(CS(a0), CS(a1)); }

HLE(h_malloc)  { return (uint64_t)(uintptr_t)malloc(a0); }
HLE(h_calloc)  { return (uint64_t)(uintptr_t)calloc(a0, a1); }
HLE(h_realloc) { return (uint64_t)(uintptr_t)realloc(P(a0), a1); }
HLE(h_free)    { free(P(a0)); return 0; }
// memalign(alignment, size): aligned allocation. Normalize alignment to a valid
// power-of-two >= sizeof(void*) for posix_memalign.
HLE(h_memalign) {
    uint64_t al = a0 < sizeof(void*) ? sizeof(void*) : a0;
    if (al & (al - 1)) { uint64_t p = sizeof(void*); while (p < al) p <<= 1; al = p; } // round up to pow2
    return (uint64_t)(uintptr_t)aligned_alloc_portable(al, a1);
}
HLE(h_posix_memalign) {
    void* p = aligned_alloc_portable(a1, a2);
    if (!p) return 12; // ENOMEM
    *(void**)P(a0) = p;
    return 0;
}
HLE(h_aligned_alloc)  { return (uint64_t)(uintptr_t)aligned_alloc_portable(a0, a1); }
// C++ operators new/delete (the whole IL2CPP game is C++). new -> malloc; the aligned
// forms take (size, align); nothrow forms take an extra tag arg we ignore.
HLE(h_new)         { return (uint64_t)(uintptr_t)malloc(a0 ? a0 : 1); }
HLE(h_new_align)   { return (uint64_t)(uintptr_t)aligned_alloc_portable(a1 ? a1 : 16, a0 ? a0 : 1); }
HLE(h_delete)      { free(P(a0)); return 0; }

// --- stdio ---
// v*printf receive a guest-built va_list (a pointer to __va_list_tag under the SysV
// ABI, which the host shares) — we can forward it directly.
HLE(h_vsnprintf) { va_list ap; if (a3) memcpy(&ap, P(a3), sizeof(va_list)); return (uint64_t)(int64_t)vsnprintf((char*)P(a0), (size_t)a1, (const char*)P(a2), ap); }
HLE(h_vsprintf)  { va_list ap; if (a2) memcpy(&ap, P(a2), sizeof(va_list)); return (uint64_t)(int64_t)vsprintf((char*)P(a0), (const char*)P(a1), ap); }
// Variadic forms: forward the register args best-effort (handles the common
// integer/pointer/≤4-arg case; float/stack args are a later refinement).
HLE(h_snprintf)  { return (uint64_t)(int64_t)snprintf((char*)P(a0), (size_t)a1, (const char*)P(a2), a3, a4, a5); }
HLE(h_sprintf)   { return (uint64_t)(int64_t)sprintf((char*)P(a0), (const char*)P(a1), a2, a3, a4, a5); }
HLE(h_printf)    { return (uint64_t)(int64_t)printf((const char*)P(a0), a1, a2, a3, a4, a5); }
HLE(h_puts)      { int r = fputs((const char*)P(a0), stdout); fputc('\n', stdout); return (uint64_t)(int64_t)r; }
HLE(h_putchar)   { return (uint64_t)(int64_t)putchar((int)a0); }
HLE(h_fputs)     { return (uint64_t)(int64_t)fputs((const char*)P(a0), a1 ? (FILE*)P(a1) : stdout); }

// --- locale / ctype (Dinkumware CRT: _Getpctype/_Getpt{o,}lower return table ptrs) ---
// Tables have 257 entries; element [-1] is the EOF slot, so we return base+1 and the
// guest indexes [c] for c in 0..255 (and [-1] for EOF). Classification bits follow the
// common MSVCRT/Dinkumware layout; built from the host's ctype so they're correct.
namespace {
    short g_ctype[257], g_tolow[257], g_toup[257];
    bool  g_ctype_built = false;
    void build_ctype() {
        if (g_ctype_built) return;
        g_ctype[0] = g_tolow[0] = g_toup[0] = 0;   // EOF slot
        for (int c = 0; c < 256; c++) {
            short m = 0;
            if (isupper(c)) m |= 0x01; if (islower(c)) m |= 0x02; if (isdigit(c)) m |= 0x04;
            if (isspace(c)) m |= 0x08; if (ispunct(c)) m |= 0x10; if (iscntrl(c)) m |= 0x20;
            if (c == ' ' || c == '\t') m |= 0x40; if (isxdigit(c)) m |= 0x80;
            g_ctype[c + 1] = m;
            g_tolow[c + 1] = (short)tolower(c);
            g_toup[c + 1]  = (short)toupper(c);
        }
        g_ctype_built = true;
    }
}
HLE(h_getpctype)  { build_ctype(); return (uint64_t)(uintptr_t)(g_ctype + 1); }
HLE(h_getptolow)  { build_ctype(); return (uint64_t)(uintptr_t)(g_tolow + 1); }
HLE(h_getptoup)   { build_ctype(); return (uint64_t)(uintptr_t)(g_toup + 1); }
HLE(h_mbcurmax)   { static int one = 1; return (uint64_t)(uintptr_t)&one; }   // __ctype_get_mb_cur_max ptr/val
HLE(h_setlocale)  { static char c[] = "C"; return (uint64_t)(uintptr_t)c; }

// C++/CRT lifecycle: we don't run global destructors, so registration is a no-op.
HLE(h_atexit)      { return 0; }
HLE(h_cxa_atexit)  { return 0; }
HLE(h_cxa_finalize){ return 0; }
// Itanium C++ ABI static-init guards: byte 0 of the guard = "initialized".
HLE(h_guard_acquire) { uint8_t* g = (uint8_t*)P(a0); return (*g) ? 0 : 1; }
HLE(h_guard_release) { uint8_t* g = (uint8_t*)P(a0); *g = 1; return 0; }
HLE(h_guard_abort)   { return 0; }

// std::_Execute_once(once_flag&, int(*cb)(void*,void*,void**), void* arg) — the guts
// of std::call_once. It MUST invoke the callback (which runs the real one-time init),
// exactly once per flag, and return nonzero on success (call_once throws on 0).
HLE(h_execute_once) {
    uint32_t* flag = (uint32_t*)P(a0);
    if (flag && *flag) return 1;                 // already executed
    auto cb = (int (*)(void*, void*, void**))P(a1);
    int r = 1;
    if (cb) { void* ctx = nullptr; r = cb((void*)P(a0), (void*)P(a2), &ctx); }
    if (flag) *flag = 1;
    return (uint64_t)(r ? 1 : 0);
}
// C++ exception refcounting — no-op is safe for the non-throwing boot path.
HLE(h_cxa_dec_refcount) { return 0; }
HLE(h_cxa_inc_refcount) { return 0; }

void register_builtin_hle() {
    #define R(str, fn) Hle::register_fn(nid_hash(str), (HleFn)(fn), str)
    R("memcpy", h_memcpy);   R("memmove", h_memmove); R("memset", h_memset);
    R("memcmp", h_memcmp);   R("memchr", h_memchr);
    R("strlen", h_strlen);   R("strnlen", h_strnlen);
    R("strcmp", h_strcmp);   R("strncmp", h_strncmp);
    R("strcpy", h_strcpy);   R("strncpy", h_strncpy);
    R("strcat", h_strcat);   R("strncat", h_strncat);
    R("strchr", h_strchr);   R("strrchr", h_strrchr); R("strstr", h_strstr);
    R("malloc", h_malloc);   R("calloc", h_calloc);   R("realloc", h_realloc); R("free", h_free);
    R("memalign", h_memalign); R("posix_memalign", h_posix_memalign); R("aligned_alloc", h_aligned_alloc);
    // operator new / new[] (+ nothrow), and aligned variants
    R("_Znwm", h_new); R("_Znam", h_new);
    R("_ZnwmRKSt9nothrow_t", h_new); R("_ZnamRKSt9nothrow_t", h_new);
    R("_ZnwmSt11align_val_t", h_new_align); R("_ZnamSt11align_val_t", h_new_align);
    R("_ZnwmSt11align_val_tRKSt9nothrow_t", h_new_align); R("_ZnamSt11align_val_tRKSt9nothrow_t", h_new_align);
    // operator delete / delete[] (+ sized, aligned, nothrow) -> free
    R("_ZdlPv", h_delete); R("_ZdaPv", h_delete);
    R("_ZdlPvm", h_delete); R("_ZdaPvm", h_delete);
    R("_ZdlPvSt11align_val_t", h_delete); R("_ZdaPvSt11align_val_t", h_delete);
    R("_ZdlPvmSt11align_val_t", h_delete); R("_ZdaPvmSt11align_val_t", h_delete);
    R("_ZdlPvRKSt9nothrow_t", h_delete); R("_ZdaPvRKSt9nothrow_t", h_delete);
    // stdio
    R("vsnprintf", h_vsnprintf); R("vsprintf", h_vsprintf);
    R("snprintf", h_snprintf);   R("sprintf", h_sprintf);
    R("printf", h_printf);       R("puts", h_puts);
    R("putchar", h_putchar);     R("fputs", h_fputs);
    // locale / ctype
    R("_Getpctype", h_getpctype); R("_Getptolower", h_getptolow); R("_Getptoupper", h_getptoup);
    R("__ctype_get_mb_cur_max", h_mbcurmax); R("setlocale", h_setlocale);
    R("atexit", h_atexit);   R("__cxa_atexit", h_cxa_atexit); R("__cxa_finalize", h_cxa_finalize);
    R("__cxa_guard_acquire", h_guard_acquire);
    R("__cxa_guard_release", h_guard_release);
    R("__cxa_guard_abort",   h_guard_abort);
    R("_ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1_", h_execute_once);  // std::call_once core
    R("__cxa_decrement_exception_refcount", h_cxa_dec_refcount);
    R("__cxa_increment_exception_refcount", h_cxa_inc_refcount);
    #undef R
    register_file_hle();     // file I/O (stdio + POSIX, /app0 translation)
    register_kernel_hle();   // libkernel primitives (pthread/sync/...)
}

} // namespace prosper
