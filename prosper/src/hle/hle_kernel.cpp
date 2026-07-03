// hle_kernel.cpp — HLE of libkernel primitives. Threading/sync are backed by host
// pthreads (guest ABI == host SysV ABI). Sony pthread types are opaque pointer
// handles: scePthreadMutexInit(&handle, &attr, name) allocates the object and stores
// the pointer through the caller's handle slot, returning 0 on success.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE   // pthread_getattr_np
#endif
#include "dispatch.hpp"
#include "nid.hpp"
#include <pthread.h>
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace prosper {

#define HLE(name) static uint64_t name(uint64_t a0, uint64_t a1, uint64_t a2, \
                                       uint64_t a3, uint64_t a4, uint64_t a5)

// --- mutex attributes (opaque; we back with host pthread_mutexattr_t) ---
HLE(k_mutexattr_init) {
    if (!a0) return 0x16; // EINVAL-ish
    auto* at = (pthread_mutexattr_t*)calloc(1, sizeof(pthread_mutexattr_t));
    pthread_mutexattr_init(at);
    *(void**)a0 = at;                     // store handle through caller's slot
    return 0;
}
HLE(k_mutexattr_settype)     { return 0; } // type/protocol/pshared ignored for now
HLE(k_mutexattr_setprotocol) { return 0; }
HLE(k_mutexattr_setpshared)  { return 0; }
HLE(k_mutexattr_destroy)     { if (a0 && *(void**)a0) { free(*(void**)a0); *(void**)a0 = nullptr; } return 0; }

// --- mutexes ---
HLE(k_mutex_init) {
    if (!a0) return 0x16;
    auto* m = (pthread_mutex_t*)calloc(1, sizeof(pthread_mutex_t));
    // Default to recursive: game code often locks re-entrantly and Sony's default differs.
    pthread_mutexattr_t at; pthread_mutexattr_init(&at);
    pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &at);
    pthread_mutexattr_destroy(&at);
    *(void**)a0 = m;
    return 0;
}
HLE(k_mutex_destroy) { if (a0 && *(void**)a0) { pthread_mutex_destroy((pthread_mutex_t*)*(void**)a0); free(*(void**)a0); *(void**)a0 = nullptr; } return 0; }
HLE(k_mutex_lock)    { if (a0 && *(void**)a0) pthread_mutex_lock((pthread_mutex_t*)*(void**)a0); return 0; }
HLE(k_mutex_trylock) { return (a0 && *(void**)a0) ? (uint64_t)pthread_mutex_trylock((pthread_mutex_t*)*(void**)a0) : 0x16; }
HLE(k_mutex_unlock)  { if (a0 && *(void**)a0) pthread_mutex_unlock((pthread_mutex_t*)*(void**)a0); return 0; }

// --- condition variables ---
HLE(k_condattr_init)    { if (a0) { auto* c = (pthread_condattr_t*)calloc(1, sizeof(pthread_condattr_t)); pthread_condattr_init(c); *(void**)a0 = c; } return 0; }
HLE(k_condattr_destroy) { if (a0 && *(void**)a0) { free(*(void**)a0); *(void**)a0 = nullptr; } return 0; }
HLE(k_cond_init)      { if (!a0) return 0x16; auto* c = (pthread_cond_t*)calloc(1, sizeof(pthread_cond_t)); pthread_cond_init(c, nullptr); *(void**)a0 = c; return 0; }
HLE(k_cond_destroy)   { if (a0 && *(void**)a0) { pthread_cond_destroy((pthread_cond_t*)*(void**)a0); free(*(void**)a0); *(void**)a0 = nullptr; } return 0; }
HLE(k_cond_signal)    { if (a0 && *(void**)a0) pthread_cond_signal((pthread_cond_t*)*(void**)a0); return 0; }
HLE(k_cond_broadcast) { if (a0 && *(void**)a0) pthread_cond_broadcast((pthread_cond_t*)*(void**)a0); return 0; }
HLE(k_cond_wait)      { if (a0 && *(void**)a0 && a1 && *(void**)a1) pthread_cond_wait((pthread_cond_t*)*(void**)a0, (pthread_mutex_t*)*(void**)a1); return 0; }

// --- thread identity ---
// The guest may read fields at fixed offsets of "its" thread object, so we hand out
// a large zeroed per-process control block (single-thread bring-up). Multi-thread
// TCBs come with scePthreadCreate later.
static uint64_t main_tcb() {
    static void* tcb = calloc(1, 0x4000);   // 16 KiB zeroed, plenty for field reads
    return (uint64_t)(uintptr_t)tcb;
}
HLE(k_pthread_self) { return main_tcb(); }
HLE(k_pthread_equal){ return (uint64_t)(a0 == a1); }
HLE(k_pthread_yield){ sched_yield(); return 0; }

// --- thread attributes ---
HLE(k_attr_init)        { if (a0) { auto* at = (pthread_attr_t*)calloc(1, sizeof(pthread_attr_t)); pthread_attr_init(at); *(void**)a0 = at; } return 0; }
HLE(k_attr_destroy)     { if (a0 && *(void**)a0) { pthread_attr_destroy((pthread_attr_t*)*(void**)a0); free(*(void**)a0); *(void**)a0 = nullptr; } return 0; }
HLE(k_attr_setstacksize){ if (a0 && *(void**)a0 && a1 >= 16384) pthread_attr_setstacksize((pthread_attr_t*)*(void**)a0, a1); return 0; }
HLE(k_attr_noop)        { return 0; }

// Query the CURRENT thread's real attributes into the caller's attr object (the GC needs
// accurate stack bounds to scan roots; bad bounds make IL2CPP's GC init assert).
HLE(k_attr_get) {
    if (a0 && *(void**)a0) pthread_getattr_np(pthread_self(), (pthread_attr_t*)*(void**)a0);
    return 0;
}
// scePthreadAttrGetstackaddr(attr, void** addr) — Sony reports the stack *base* (low addr).
HLE(k_attr_getstackaddr) {
    if (a0 && *(void**)a0 && a1) {
        void* base = nullptr; size_t sz = 0;
        pthread_attr_getstack((pthread_attr_t*)*(void**)a0, &base, &sz);
        *(void**)(uintptr_t)a1 = base;
    }
    return 0;
}
HLE(k_attr_getstacksize) {
    if (a0 && *(void**)a0 && a1) {
        void* base = nullptr; size_t sz = 0;
        pthread_attr_getstack((pthread_attr_t*)*(void**)a0, &base, &sz);
        *(size_t*)(uintptr_t)a1 = sz;
    }
    return 0;
}

// --- thread creation: run the guest entry on a real host thread (ABI matches) ---
HLE(k_pthread_create) {
    pthread_attr_t* at = (a1 && *(void**)a1) ? (pthread_attr_t*)*(void**)a1 : nullptr;
    pthread_t tid;
    int r = pthread_create(&tid, at, (void* (*)(void*))(uintptr_t)a2, (void*)(uintptr_t)a3);
    if (r) return (uint64_t)r;
    if (a0) *(uint64_t*)a0 = (uint64_t)tid;
    return 0;
}
HLE(k_pthread_join)   { void* rv = nullptr; pthread_join((pthread_t)a0, a1 ? &rv : nullptr); if (a1) *(void**)(uintptr_t)a1 = rv; return 0; }
HLE(k_pthread_detach) { pthread_detach((pthread_t)a0); return 0; }
HLE(k_pthread_exit)   { pthread_exit((void*)(uintptr_t)a0); return 0; }

// --- thread-local storage keys (IL2CPP uses these heavily) -> host pthread keys ---
HLE(k_key_create) {
    if (!a0) return 0x16;
    pthread_key_t k;
    int r = pthread_key_create(&k, (void (*)(void*))(uintptr_t)a1);
    if (r) return (uint64_t)r;
    *(uint32_t*)(uintptr_t)a0 = (uint32_t)k;   // hand the guest our host key
    return 0;
}
HLE(k_key_delete)    { pthread_key_delete((pthread_key_t)a0); return 0; }
HLE(k_getspecific)   { return (uint64_t)(uintptr_t)pthread_getspecific((pthread_key_t)a0); }
HLE(k_setspecific)   { return (uint64_t)(int64_t)pthread_setspecific((pthread_key_t)a0, (void*)(uintptr_t)a1); }

void register_kernel_hle() {
    #define R(str, fn) Hle::register_fn(nid_hash(str), (HleFn)(fn), str)
    R("scePthreadMutexattrInit", k_mutexattr_init);
    R("scePthreadMutexattrSettype", k_mutexattr_settype);
    R("scePthreadMutexattrSetprotocol", k_mutexattr_setprotocol);
    R("scePthreadMutexattrSetpshared", k_mutexattr_setpshared);
    R("scePthreadMutexattrDestroy", k_mutexattr_destroy);
    R("scePthreadMutexInit", k_mutex_init);
    R("scePthreadMutexDestroy", k_mutex_destroy);
    R("scePthreadMutexLock", k_mutex_lock);
    R("scePthreadMutexTrylock", k_mutex_trylock);
    R("scePthreadMutexUnlock", k_mutex_unlock);
    R("scePthreadCondattrInit", k_condattr_init);
    R("scePthreadCondattrDestroy", k_condattr_destroy);
    R("scePthreadCondInit", k_cond_init);
    R("scePthreadCondDestroy", k_cond_destroy);
    R("scePthreadCondSignal", k_cond_signal);
    R("scePthreadCondBroadcast", k_cond_broadcast);
    R("scePthreadCondWait", k_cond_wait);
    R("scePthreadSelf", k_pthread_self);
    R("scePthreadEqual", k_pthread_equal);
    R("scePthreadYield", k_pthread_yield);
    R("scePthreadCreate", k_pthread_create);
    R("scePthreadJoin", k_pthread_join);
    R("scePthreadDetach", k_pthread_detach);
    R("scePthreadExit", k_pthread_exit);
    R("scePthreadAttrInit", k_attr_init);
    R("scePthreadAttrDestroy", k_attr_destroy);
    R("scePthreadAttrSetstacksize", k_attr_setstacksize);
    R("scePthreadAttrSetinheritsched", k_attr_noop);
    R("scePthreadAttrSetschedpolicy", k_attr_noop);
    R("scePthreadAttrSetschedparam", k_attr_noop);
    R("scePthreadAttrSetdetachstate", k_attr_noop);
    R("scePthreadAttrGetschedparam", k_attr_noop);
    R("scePthreadAttrGet", k_attr_get);
    R("scePthreadAttrGetstackaddr", k_attr_getstackaddr);
    R("scePthreadAttrGetstacksize", k_attr_getstacksize);
    R("scePthreadGetstack", k_attr_getstackaddr);
    // TLS keys (POSIX + Sony names -> host pthread keys)
    R("pthread_key_create", k_key_create);   R("scePthreadKeyCreate", k_key_create);
    R("pthread_key_delete", k_key_delete);   R("scePthreadKeyDelete", k_key_delete);
    R("pthread_getspecific", k_getspecific); R("scePthreadGetspecific", k_getspecific);
    R("pthread_setspecific", k_setspecific); R("scePthreadSetspecific", k_setspecific);
    R("pthread_self", k_pthread_self);
    #undef R
    register_kernel_mem_hle();    // virtual/direct memory
    register_kernel_time_hle();   // time/clock + C11 threads + stubs
}

} // namespace prosper
