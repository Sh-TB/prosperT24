// exec_image.hpp — map a built guest image into real host memory + trap imports.
//
// Linux (M2). The image is mapped at its guest base; the import stub region is
// mapped PROT_NONE so any guest call to an unimplemented import faults, and the
// SIGSEGV handler identifies it by name from the faulting address. This is the
// "it's alive, trapped at the first Sony call" mechanism.
#pragma once
#include "../self/module.hpp"
#include <string>

namespace prosper {

// Map the (already relocated) image + a PROT_NONE stub region into host memory.
// `img` must have been built at `base` and bound to `stub_base`/`stub_size`.
// Returns false with *err set on failure.
bool map_image(const Module& m, const LoadedImage& img,
               uint64_t stub_base, uint64_t stub_size, std::string* err);

// Install the fault handler that turns a stub fault into a named-import report.
void install_trap_handler();

// Bring-up / test: transfer control to the idx-th import's stub as if the guest
// called it. Returns: 1 = trapped in stub region (name filled), 2 = other fault,
// 0 = returned without faulting (unexpected). Requires install_trap_handler() first.
int invoke_stub(uint64_t idx, std::string* name_out);

// Result of trying to run the guest entry point.
struct BootResult {
    int          kind = 0;      // 1 = trapped on an import, 2 = other fault, 0 = returned
    std::string  detail;        // import "lib::nid" or fault description
    uint64_t     fault_addr = 0, fault_rip = 0;
};
// Set up a SysV-style initial stack + argc/argv param block, jump to `img.entry`,
// and run until the first fault (expected: the crt's first unimplemented Sony call).
BootResult run_entry(const LoadedImage& img);

} // namespace prosper
