// hle_libc.cpp — HLE implementations of the common libc functions the guest imports.
// The guest ABI == host SysV ABI, so most of these are thin thunks straight to the
// host C library. Registered by NID so the loader binds imports directly to them.
#include "dispatch.hpp"
#include "nid.hpp"
#include <cstring>
#include <cstdlib>
#include <cstdint>

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

// C++/CRT lifecycle: we don't run global destructors, so registration is a no-op.
HLE(h_atexit)      { return 0; }
HLE(h_cxa_atexit)  { return 0; }
HLE(h_cxa_finalize){ return 0; }
// Itanium C++ ABI static-init guards: byte 0 of the guard = "initialized".
HLE(h_guard_acquire) { uint8_t* g = (uint8_t*)P(a0); return (*g) ? 0 : 1; }
HLE(h_guard_release) { uint8_t* g = (uint8_t*)P(a0); *g = 1; return 0; }
HLE(h_guard_abort)   { return 0; }

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
    R("atexit", h_atexit);   R("__cxa_atexit", h_cxa_atexit); R("__cxa_finalize", h_cxa_finalize);
    R("__cxa_guard_acquire", h_guard_acquire);
    R("__cxa_guard_release", h_guard_release);
    R("__cxa_guard_abort",   h_guard_abort);
    #undef R
}

} // namespace prosper
