// exec_image.hpp — map a built guest image into real host memory + HLE stubs.
//
// Linux (M2/M3). The relocated image is mapped executable at its guest base. The
// import stub region holds one small executable stub per import: implemented imports
// tail-jump to their C handler (args intact); unimplemented ones log and return 0 so
// the boot advances and reveals the next call. A SIGSEGV handler still catches genuine
// guest faults (null derefs from stubbed-out returns, etc.) and reports RIP.
#pragma once
#include "../self/module.hpp"
#include <string>

namespace prosper {

// Map the (already relocated) image at img.base as executable. `false`+*err on failure.
bool map_image(const Module& m, const LoadedImage& img, std::string* err);

// Create the executable stub region at stub_base (must match bind_imports_to_stubs)
// and populate one stub per import (handler tail-call or unimplemented logger).
bool install_stubs(const Module& m, uint64_t stub_base, uint64_t stub_size, std::string* err);

// Install the fault handler for genuine guest faults during a run.
void install_trap_handler();

// Address of the idx-th import's stub (for tests/bring-up).
uint64_t stub_addr(uint64_t idx);

// Test/bring-up: call the idx-th import's stub as the guest would; returns its result.
uint64_t invoke_stub(uint64_t idx);

// Result of running the guest.
struct BootResult {
    int          kind = 0;      // 0 = returned, 2 = faulted (SIGSEGV/BUS), 3 = SIGILL
    std::string  detail;        // fault description
    uint64_t     fault_addr = 0, fault_rip = 0;
    uint64_t     rbp = 0, rsp = 0, rax = 0, rdi = 0, rsi = 0, rdx = 0; // regs at fault
};
// Set up a SysV-style stack + argc/argv, jump to img.entry, run until it returns or
// faults. Unimplemented imports are logged along the way (see dispatch.hpp).
BootResult run_entry(const LoadedImage& img);

} // namespace prosper
