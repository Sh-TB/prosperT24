// test_rdna2_decode — validates the RDNA2 instruction-stream walker (src/gpu/rdna2_decode.cpp)
// against instructions assembled by llvm-mc for gfx1030 (authoritative encodings, not hand-rolled).
// The stream mixes every major encoding class + inline literals + S_ENDPGM; the walker must classify
// each instruction's format, compute its length (incl. literals), and terminate at S_ENDPGM.
#include "../src/gpu/rdna2_decode.hpp"
#include <cstdio>
#include <cstdint>

using namespace prosper::gpu;

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  [FAIL] %s\n", m); fails++; } \
                         else       { printf("  [ok]   %s\n", m); } } while (0)

int main() {
    printf("== test_rdna2_decode ==\n");
    // Assembled with: llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx1030 --show-encoding
    //   s_mov_b32 s0,s1 | s_mov_b32 s2,0x12345678 | s_add_u32 s0,s1,s2 | v_mov_b32 v0,v1 |
    //   v_mov_b32 v2,0x12345678 | v_add_f32 v0,v1,v2 | v_fma_f32 v0,v1,v2,v3 |
    //   s_load_dwordx4 s[0:3],s[4:5],0x0 | exp mrt0 v0,v1,v2,v3 | s_endpgm
    const uint32_t code[] = {
        0xBE800301u,                     // SOP1  s_mov_b32 s0,s1
        0xBE8203FFu, 0x12345678u,        // SOP1  s_mov_b32 s2,lit   (+literal)
        0x80000201u,                     // SOP2  s_add_u32
        0x7E000301u,                     // VOP1  v_mov_b32 v0,v1
        0x7E0402FFu, 0x12345678u,        // VOP1  v_mov_b32 v2,lit   (+literal)
        0x06000501u,                     // VOP2  v_add_f32
        0xD54B0000u, 0x040E0501u,        // VOP3  v_fma_f32
        0xF4080002u, 0xFA000000u,        // SMEM  s_load_dwordx4
        0xF800000Fu, 0x03020100u,        // EXP   exp mrt0
        0xBF810000u,                     // SOPP  s_endpgm
    };
    const size_t n = sizeof(code) / sizeof(code[0]);

    std::vector<Rdna2Inst> ins;
    size_t consumed = rdna2_walk(code, n, ins);
    CHECK(consumed == n, "walker consumed the whole stream (15 dwords)");
    CHECK(ins.size() == 10, "decoded 10 instructions");
    if (ins.size() != 10) { printf("== FAIL: got %zu ==\n", ins.size()); return 1; }

    struct Exp { Rdna2Format fmt; uint32_t pc; uint32_t len; bool lit; };
    const Exp exp[] = {
        {Rdna2Format::SOP1, 0,  1, false},
        {Rdna2Format::SOP1, 1,  2, true },
        {Rdna2Format::SOP2, 3,  1, false},
        {Rdna2Format::VOP1, 4,  1, false},
        {Rdna2Format::VOP1, 5,  2, true },
        {Rdna2Format::VOP2, 7,  1, false},
        {Rdna2Format::VOP3, 8,  2, false},
        {Rdna2Format::SMEM, 10, 2, false},
        {Rdna2Format::EXP,  12, 2, false},
        {Rdna2Format::SOPP, 14, 1, false},
    };
    const char* names[] = {"SOP1","SOP1+lit","SOP2","VOP1","VOP1+lit","VOP2","VOP3","SMEM","EXP","SOPP"};
    for (size_t k = 0; k < 10; k++) {
        char msg[64]; snprintf(msg, sizeof msg, "inst %zu = %s (pc=%u len=%u)", k, names[k], exp[k].pc, exp[k].len);
        bool ok = ins[k].fmt == exp[k].fmt && ins[k].pc == exp[k].pc &&
                  ins[k].len_dwords == exp[k].len && ins[k].has_literal == exp[k].lit;
        CHECK(ok, msg);
    }
    CHECK(ins[1].literal == 0x12345678u && ins[4].literal == 0x12345678u, "inline literals captured");
    CHECK(ins[9].is_end, "S_ENDPGM flagged as end");

    // --- operand decode (stage 2) ---
    auto isS = [](const Operand& o, int n){ return o.kind == OperandKind::SGPR && o.value == n; };
    auto isV = [](const Operand& o, int n){ return o.kind == OperandKind::VGPR && o.value == n; };
    // inst0: s_mov_b32 s0, s1  -> dst SGPR0, src0 SGPR1
    CHECK(isS(ins[0].dst, 0) && ins[0].n_src == 1 && isS(ins[0].src[0], 1), "s_mov_b32 s0,s1 operands");
    // inst2: s_add_u32 s0, s1, s2 -> dst SGPR0, src0 SGPR1, src1 SGPR2
    CHECK(isS(ins[2].dst, 0) && isS(ins[2].src[0], 1) && isS(ins[2].src[1], 2), "s_add_u32 s0,s1,s2 operands");
    // inst3: v_mov_b32 v0, v1 -> dst VGPR0, src0 VGPR1
    CHECK(isV(ins[3].dst, 0) && isV(ins[3].src[0], 1), "v_mov_b32 v0,v1 operands");
    // inst4: v_mov_b32 v2, lit -> dst VGPR2, src0 Literal
    CHECK(isV(ins[4].dst, 2) && ins[4].src[0].kind == OperandKind::Literal, "v_mov_b32 v2,lit operands");
    // inst5: v_add_f32 v0, v1, v2 -> dst VGPR0, src0 VGPR1, src1 VGPR2
    CHECK(isV(ins[5].dst, 0) && isV(ins[5].src[0], 1) && isV(ins[5].src[1], 2), "v_add_f32 v0,v1,v2 operands");
    // inst6: v_fma_f32 v0, v1, v2, v3 (VOP3) -> dst VGPR0, srcs VGPR1/2/3
    CHECK(isV(ins[6].dst, 0) && ins[6].n_src == 3 && isV(ins[6].src[0], 1) &&
          isV(ins[6].src[1], 2) && isV(ins[6].src[2], 3), "v_fma_f32 VOP3 operands");
    // inline-constant field decode: SGPR106 special, field 129 -> +1, 193 -> -1, 242 -> 1.0f
    CHECK(decode_src_field(0).kind == OperandKind::SGPR && decode_src_field(0).value == 0, "field 0 -> SGPR0");
    CHECK(decode_src_field(257).kind == OperandKind::VGPR && decode_src_field(257).value == 1, "field 257 -> VGPR1");
    CHECK(decode_src_field(129).kind == OperandKind::InlineInt && decode_src_field(129).value == 1, "field 129 -> int +1");
    CHECK(decode_src_field(193).kind == OperandKind::InlineInt && decode_src_field(193).value == -1, "field 193 -> int -1");
    CHECK(decode_src_field(242).kind == OperandKind::InlineFloat && inline_float_value(242) == 1.0f, "field 242 -> float 1.0");

    if (fails) { printf("== FAIL: %d ==\n", fails); return 1; }
    printf("== PASS ==\n");
    return 0;
}
