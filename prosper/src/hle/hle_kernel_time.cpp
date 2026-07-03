// hle_kernel_time.cpp — time/clock sources, C11 thread primitives, and assorted
// libkernel stubs the engine needs during init. Cross-platform (chrono + pthread).
#include "dispatch.hpp"
#include "nid.hpp"
#include <pthread.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <ctime>

namespace prosper {

#define HLE(name) static uint64_t name(uint64_t a0, uint64_t a1, uint64_t a2, \
                                       uint64_t a3, uint64_t a4, uint64_t a5)
#define P(x) ((void*)(uintptr_t)(x))

namespace {
    using clk = std::chrono::steady_clock;
    clk::time_point g_start = clk::now();
    uint64_t ns_now() { return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(clk::now() - g_start).count(); }
    std::atomic<uint64_t> g_module_handle{1};
}

// --- time / clock (return real, advancing time so wait-for-time loops progress) ---
HLE(k_get_ptc)        { return ns_now(); }                 // sceKernelGetProcessTimeCounter
HLE(k_get_ptc_freq)   { return 1000000000ull; }            // counter is in ns -> 1 GHz
HLE(k_get_proc_time)  { return ns_now() / 1000; }          // microseconds
HLE(k_read_tsc)       { return ns_now(); }
HLE(k_tsc_freq)       { return 1000000000ull; }
HLE(k_clock_gettime) {                                     // (clockid, struct timespec*)
    if (!a1) return 0;
    uint64_t ns = ns_now();
    ((int64_t*)P(a1))[0] = (int64_t)(ns / 1000000000ull);   // tv_sec
    ((int64_t*)P(a1))[1] = (int64_t)(ns % 1000000000ull);   // tv_nsec
    return 0;
}
HLE(k_gettimeofday) {                                      // (struct timeval*, tz*)
    if (!a0) return 0;
    uint64_t us = ns_now() / 1000;
    ((int64_t*)P(a0))[0] = (int64_t)(us / 1000000ull);      // tv_sec
    ((int64_t*)P(a0))[1] = (int64_t)(us % 1000000ull);      // tv_usec
    return 0;
}
HLE(k_time) { uint64_t s = ns_now() / 1000000000ull + 1700000000ull; if (a0) *(int64_t*)P(a0) = (int64_t)s; return s; }
HLE(k_clock) { return ns_now() / 1000; }   // clock(): CLOCKS_PER_SEC=1e6 -> microseconds
// real sleeps so timed wait loops actually yield the CPU (and advance real time)
HLE(k_usleep)   { uint64_t us = a0; struct timespec ts{ (time_t)(us / 1000000), (long)((us % 1000000) * 1000) }; nanosleep(&ts, nullptr); return 0; }
HLE(k_sleep_s)  { struct timespec ts{ (time_t)a0, 0 }; nanosleep(&ts, nullptr); return (uint64_t)a0; }
HLE(k_nanosleep){ if (a0) nanosleep((const struct timespec*)P(a0), a1 ? (struct timespec*)P(a1) : nullptr); return 0; }

// --- assorted libkernel stubs ---
HLE(k_ok)              { return 0; }                       // generic success no-op
HLE(k_load_start_mod)  { return g_module_handle++; }       // return a positive module id
HLE(k_uuid_create) {                                       // fill 16 non-zero bytes
    if (a0) { uint8_t* u = (uint8_t*)P(a0); uint64_t t = ns_now(); for (int i = 0; i < 16; i++) u[i] = (uint8_t)(t >> (i * 4)) ^ (0xA5 + i); }
    return 0;
}

// --- C11 threads (used by MSVC STL std::mutex/std::condition_variable) ---
HLE(m_mtx_init)   { if (a0) { auto* m = (pthread_mutex_t*)calloc(1, sizeof(pthread_mutex_t)); pthread_mutexattr_t at; pthread_mutexattr_init(&at); pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE); pthread_mutex_init(m, &at); pthread_mutexattr_destroy(&at); *(void**)P(a0) = m; } return 0; }
HLE(m_mtx_lock)   { if (a0 && *(void**)P(a0)) pthread_mutex_lock((pthread_mutex_t*)*(void**)P(a0)); return 0; }
HLE(m_mtx_unlock) { if (a0 && *(void**)P(a0)) pthread_mutex_unlock((pthread_mutex_t*)*(void**)P(a0)); return 0; }
HLE(m_mtx_destroy){ if (a0 && *(void**)P(a0)) { pthread_mutex_destroy((pthread_mutex_t*)*(void**)P(a0)); free(*(void**)P(a0)); } return 0; }
HLE(m_cnd_init)   { if (a0) { auto* c = (pthread_cond_t*)calloc(1, sizeof(pthread_cond_t)); pthread_cond_init(c, nullptr); *(void**)P(a0) = c; } return 0; }
HLE(m_cnd_signal) { if (a0 && *(void**)P(a0)) pthread_cond_signal((pthread_cond_t*)*(void**)P(a0)); return 0; }
HLE(m_cnd_broadcast){ if (a0 && *(void**)P(a0)) pthread_cond_broadcast((pthread_cond_t*)*(void**)P(a0)); return 0; }
HLE(m_cnd_wait)   { if (a0 && *(void**)P(a0) && a1 && *(void**)P(a1)) pthread_cond_wait((pthread_cond_t*)*(void**)P(a0), (pthread_mutex_t*)*(void**)P(a1)); return 0; }
HLE(m_cnd_destroy){ if (a0 && *(void**)P(a0)) { pthread_cond_destroy((pthread_cond_t*)*(void**)P(a0)); free(*(void**)P(a0)); } return 0; }

void register_kernel_time_hle() {
    #define R(str, fn) Hle::register_fn(nid_hash(str), (HleFn)(fn), str)
    R("sceKernelGetProcessTimeCounter", k_get_ptc);
    R("sceKernelGetProcessTimeCounterFrequency", k_get_ptc_freq);
    R("sceKernelGetProcessTime", k_get_proc_time);
    R("sceKernelReadTsc", k_read_tsc);
    R("sceKernelGetTscFrequency", k_tsc_freq);
    R("sceKernelClockGettime", k_clock_gettime);
    R("sceKernelUsleep", k_usleep);   R("usleep", k_usleep);
    R("sceKernelSleep", k_sleep_s);   R("sleep", k_sleep_s);
    R("sceKernelNanosleep", k_nanosleep);  R("nanosleep", k_nanosleep);
    R("clock_gettime", k_clock_gettime);
    R("sceKernelGettimeofday", k_gettimeofday);
    R("gettimeofday", k_gettimeofday);
    R("time", k_time);
    R("clock", k_clock);
    // module loading (report success; real PRX are already resident in our address space)
    R("sceSysmoduleLoadModule", k_ok);
    R("sceSysmoduleUnloadModule", k_ok);
    R("sceSysmoduleIsLoaded", k_ok);
    R("sceKernelLoadStartModule", k_load_start_mod);
    R("sceKernelStopUnloadModule", k_ok);
    // thread scheduling hints — safe no-ops
    R("scePthreadSetaffinity", k_ok);
    R("scePthreadGetaffinity", k_ok);
    R("scePthreadSetprio", k_ok);
    R("scePthreadGetprio", k_ok);
    R("scePthreadSetschedparam", k_ok);
    R("scePthreadRename", k_ok);
    R("sceKernelUuidCreate", k_uuid_create);
    // C11 threads
    R("_Mtx_init", m_mtx_init);   R("_Mtx_lock", m_mtx_lock);   R("_Mtx_unlock", m_mtx_unlock);
    R("_Mtx_destroy", m_mtx_destroy);
    R("_Cnd_init", m_cnd_init);   R("_Cnd_signal", m_cnd_signal); R("_Cnd_broadcast", m_cnd_broadcast);
    R("_Cnd_wait", m_cnd_wait);   R("_Cnd_destroy", m_cnd_destroy);
    #undef R
}

} // namespace prosper
