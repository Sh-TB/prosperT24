// command_processor.hpp — apply a decoded PM4 stream to a GPU state snapshot.
//
// This is the second CommandProcessor stage (after pm4_decode): walk the decoded ops and fold them
// into a `GpuState` — the register files (context/shader/user-config) plus index type and the list of
// draws. A later stage translates a GpuState + its draws into Vulkan pipelines/draw calls (M4).
//
// Register application matches Kyty's CommandProcessor (GraphicsRun.cpp `cp_op_indirect_cx_regs`,
// MIT): a Set{Cx,Sh,Uc}RegistersIndirect packet points at an array of `num_regs` {offset, value}
// pairs (a ShaderRegister[]); each writes `value` into the class's register file at `offset`. Because
// prosper maps the guest image 1:1, the packet's `regs_vaddr` is directly readable as a host pointer.
#pragma once
#include "pm4_decode.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace prosper::gpu {

// A {register offset, value} pair — the element type of a Set*RegistersIndirect array.
struct ShaderReg { uint32_t offset; uint32_t value; };

// Folded GPU state after replaying a command stream.
struct GpuState {
    std::unordered_map<uint32_t, uint32_t> cx, sh, uc;   // register files by offset
    uint32_t index_type = 0;                             // last SetIndexType
    struct Draw { uint32_t index_count; };
    std::vector<Draw> draws;                             // one per DrawIndexAuto

    // Safety cap on an indirect register count (a malformed/huge count won't run away).
    static constexpr uint32_t kMaxRegsPerPacket = 4096;

    // Fold one decoded command into the state. Reads register arrays via their (1:1-mapped) vaddr.
    void apply(const Pm4Command& c);
};

// Decode `dwords` dwords at `buf` and apply every op to `st`. Returns the number of packets applied.
size_t run_command_buffer(const uint32_t* buf, size_t dwords, GpuState& st);

} // namespace prosper::gpu
