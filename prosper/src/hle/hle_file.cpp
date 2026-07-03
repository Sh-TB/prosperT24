// hle_file.cpp — HLE file I/O. Guest paths like "/app0/..." (the game's own data) are
// translated to the host dump directory; stdio FILE* and POSIX fd calls thunk to the
// host. Set the app0 root via set_app0_root() or the PROSPER_APP0 env var.
#include "dispatch.hpp"
#include "nid.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

namespace prosper {

#define HLE(name) static uint64_t name(uint64_t a0, uint64_t a1, uint64_t a2, \
                                       uint64_t a3, uint64_t a4, uint64_t a5)
#define P(x) ((void*)(uintptr_t)(x))
#define CS(x) ((const char*)(uintptr_t)(x))

namespace {
    std::string g_app0;   // host directory backing guest "/app0"
    bool filelog() { static int v = getenv("PROSPER_FILELOG") ? 1 : 0; return v; }
    std::string translate(const char* guest) {
        if (!guest) return {};
        std::string p = guest;
        if (g_app0.empty()) { if (const char* e = getenv("PROSPER_APP0")) g_app0 = e; }
        // Map /app0[/...] -> <root>[/...]; leave other absolute paths as-is.
        std::string h = (p.rfind("/app0", 0) == 0) ? g_app0 + p.substr(5) : p;
        if (filelog()) fprintf(stderr, "[file] open '%s' -> '%s'\n", guest, h.c_str());
        return h;
    }
}

void set_app0_root(const std::string& root) { g_app0 = root; }

// --- stdio FILE* ---
HLE(f_fopen)   { std::string h = translate(CS(a0)); return (uint64_t)(uintptr_t)fopen(h.c_str(), CS(a1)); }
HLE(f_fclose)  { return a0 ? (uint64_t)(int64_t)fclose((FILE*)P(a0)) : 0; }
HLE(f_fread)   { return a3 ? (uint64_t)fread(P(a0), a1, a2, (FILE*)P(a3)) : 0; }
HLE(f_fwrite)  { return a3 ? (uint64_t)fwrite(P(a0), a1, a2, (FILE*)P(a3)) : 0; }
HLE(f_fseek)   { return a0 ? (uint64_t)(int64_t)fseek((FILE*)P(a0), (long)a1, (int)a2) : -1; }
HLE(f_ftell)   { return a0 ? (uint64_t)(int64_t)ftell((FILE*)P(a0)) : -1; }
HLE(f_fgets)   { return (uint64_t)(uintptr_t)(a2 ? fgets((char*)P(a0), (int)a1, (FILE*)P(a2)) : nullptr); }
HLE(f_fflush)  { return (uint64_t)(int64_t)fflush(a0 ? (FILE*)P(a0) : nullptr); }
HLE(f_feof)    { return a0 ? (uint64_t)feof((FILE*)P(a0)) : 1; }
HLE(f_ferror)  { return a0 ? (uint64_t)ferror((FILE*)P(a0)) : 0; }
HLE(f_setvbuf) { return 0; }
HLE(f_rewind)  { if (a0) rewind((FILE*)P(a0)); return 0; }
HLE(f_fgetc)   { return a0 ? (uint64_t)(int64_t)fgetc((FILE*)P(a0)) : (uint64_t)-1; }

// --- POSIX fd ---
HLE(f_open)  { std::string h = translate(CS(a0)); return (uint64_t)(int64_t)::open(h.c_str(), (int)a1, (mode_t)a2); }
HLE(f_close) { return (uint64_t)(int64_t)::close((int)a0); }
HLE(f_read)  { return (uint64_t)(int64_t)::read((int)a0, P(a1), (size_t)a2); }
HLE(f_write) { return (uint64_t)(int64_t)::write((int)a0, P(a1), (size_t)a2); }
HLE(f_lseek) { return (uint64_t)(int64_t)::lseek((int)a0, (off_t)a1, (int)a2); }
HLE(f_stat)  { std::string h = translate(CS(a0)); struct stat st; int r = ::stat(h.c_str(), &st); if (r == 0 && a1) memcpy(P(a1), &st, sizeof(st)); return (uint64_t)(int64_t)r; }
HLE(f_fstat) { struct stat st; int r = ::fstat((int)a0, &st); if (r == 0 && a1) memcpy(P(a1), &st, sizeof(st)); return (uint64_t)(int64_t)r; }
HLE(f_access){ std::string h = translate(CS(a0)); return (uint64_t)(int64_t)::access(h.c_str(), (int)a1); }

void register_file_hle() {
    #define R(str, fn) Hle::register_fn(nid_hash(str), (HleFn)(fn), str)
    R("fopen", f_fopen);   R("fclose", f_fclose); R("fread", f_fread);   R("fwrite", f_fwrite);
    R("fseek", f_fseek);   R("ftell", f_ftell);   R("fgets", f_fgets);   R("fflush", f_fflush);
    R("feof", f_feof);     R("ferror", f_ferror); R("setvbuf", f_setvbuf); R("rewind", f_rewind);
    R("fgetc", f_fgetc);   R("getc", f_fgetc);
    R("open", f_open);     R("close", f_close);   R("read", f_read);     R("write", f_write);
    R("lseek", f_lseek);   R("stat", f_stat);     R("fstat", f_fstat);   R("access", f_access);
    R("sceKernelOpen", f_open);   R("sceKernelClose", f_close);  R("sceKernelRead", f_read);
    R("sceKernelWrite", f_write); R("sceKernelLseek", f_lseek);  R("sceKernelStat", f_stat);
    R("sceKernelFstat", f_fstat);
    #undef R
}

} // namespace prosper
