// linker.hpp — multi-module dynamic linker. Loads the main executable plus its
// dependent PRX modules into one guest address space, builds a global export table,
// and resolves every import to either another module's export (real cross-module call)
// or an HLE stub slot (implemented handler or unimplemented logger). Host-agnostic.
#pragma once
#include "../self/module.hpp"
#include "../hle/dispatch.hpp"   // ImportSlot
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace prosper {

struct LinkInput { std::string path; uint64_t base; };

struct Program {
    std::vector<std::unique_ptr<Module>> mods;   // unique_ptr: stable addresses for imports[]
    std::vector<LoadedImage>             imgs;    // parallel to mods
    std::vector<ImportSlot>              slots;   // unresolved imports -> stub slots
    uint64_t entry = 0;                            // main module entry
    uint64_t stub_base = 0, stub_size = 32;

    // Stats for reporting.
    size_t total_imports = 0, resolved_cross_module = 0, stubbed = 0;
};

// Link the given modules (the first is the main executable). Applies relocations.
// Returns false with *err on failure.
bool link_program(const std::vector<LinkInput>& inputs, uint64_t stub_base,
                  Program& out, std::string* err);

} // namespace prosper
