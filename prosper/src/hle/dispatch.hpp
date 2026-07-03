// dispatch.hpp — HLE call dispatch: registry of implemented Sony functions +
// a logger for unimplemented ones. Because the Linux host ABI == the guest's
// System V AMD64 ABI, an HLE handler is just a C function the guest calls directly
// (args already in rdi/rsi/rdx/rcx/r8/r9); it returns a value in rax and rets to guest.
#pragma once
#include "../self/module.hpp"
#include "nid.hpp"
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace prosper {

// Generic HLE handler signature (up to 6 integer/pointer args, SysV).
using HleFn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// Registry of implemented functions, keyed by NID.
class Hle {
public:
    static void  register_fn(const std::string& nid, HleFn fn, const char* name);
    static HleFn lookup(const std::string& nid);          // nullptr if unimplemented
    static const char* name_of(const std::string& nid);   // registered display name or ""
};

// Wire the unimplemented-call logger to the running module + name DB.
void dispatch_init(const Module* m, NidDb* db);

// Register the built-in HLE implementations (libc thunks, CRT no-ops). Call before install_stubs.
void register_builtin_hle();

// The default target for unimplemented imports: logs (first-seen) and returns 0.
// Called by generated stubs with the import index in the first arg.
extern "C" uint64_t prosper_on_unimpl(uint64_t import_index);

// First-seen order of unimplemented import indices called by the guest.
const std::vector<uint32_t>& call_order();
// Print the accumulated unimplemented-call trace (index, lib::nid [name], count).
void dump_call_log(FILE* f);
void reset_call_log();

} // namespace prosper
