// rdna2_decode.hpp — walk an RDNA2 (gfx10.3) shader binary instruction by instruction.
//
// First stage of the RDNA2 -> SPIR-V shader recompiler (the piece that will replace the placeholder
// SPIR-V in test_vulkan_triangle). Before we can translate instructions we must be able to *walk*
// the ISA stream: classify each instruction's encoding format and compute its length in dwords
// (accounting for inline 32-bit literal constants and the 2-dword formats). Opcode semantics /
// operand decode / SPIR-V emission come in later stages.
//
// Encoding classification follows the RDNA2 ("next-gen") dispatch in Kyty ShaderParse.cpp and the
// AMD gfx10 ISA docs. This is pure (no I/O), so it is unit-tested against instructions assembled by
// llvm-mc for gfx1030 (test_rdna2_decode).
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace prosper::gpu {

enum class Rdna2Format : uint8_t {
    SOP2, SOP1, SOPK, SOPC, SOPP,      // scalar ALU / constant / program-flow
    SMEM,                              // scalar memory
    VOP2, VOP1, VOPC, VOP3,           // vector ALU
    VINTRP,                            // interpolation
    DS, MUBUF, MTBUF, MIMG, FLAT,      // LDS / buffer / image / flat memory
    EXP,                               // export (render target / position)
    Unknown,
};

struct Rdna2Inst {
    Rdna2Format fmt = Rdna2Format::Unknown;
    uint32_t    pc = 0;            // dword offset from the start of the stream
    uint32_t    words[2] = {0, 0}; // the (up to 2) instruction dwords (not incl. a trailing literal)
    uint32_t    len_dwords = 1;    // total length incl. any inline literal
    bool        has_literal = false;
    uint32_t    literal = 0;       // the inline 32-bit constant, if has_literal
    bool        is_end = false;    // S_ENDPGM
};

// Decode the single instruction at code[0..]; `max_dwords` bounds the read. On a truncated/unknown
// encoding, returns fmt=Unknown with len_dwords clamped so a walker still terminates.
Rdna2Inst rdna2_decode_one(const uint32_t* code, size_t max_dwords);

// Walk from code[0], appending each decoded instruction to `out`, until S_ENDPGM, an Unknown
// encoding, or the end of the buffer. Returns the number of dwords consumed.
size_t rdna2_walk(const uint32_t* code, size_t dwords, std::vector<Rdna2Inst>& out);

} // namespace prosper::gpu
