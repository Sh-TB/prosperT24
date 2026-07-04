// rdna2_decode.cpp — see rdna2_decode.hpp.
#include "rdna2_decode.hpp"

namespace prosper::gpu {

namespace {
constexpr uint32_t LITERAL = 0xFFu;        // operand field value meaning "32-bit literal follows"
constexpr uint32_t S_ENDPGM = 0xBF810000u;

// A source operand == 255 selects an inline literal constant (one extra dword).
bool sop_has_literal(uint32_t w, int nsrc) {
    if ((w & 0xFFu) == LITERAL) return true;                       // ssrc0
    if (nsrc >= 2 && ((w >> 8) & 0xFFu) == LITERAL) return true;   // ssrc1
    return false;
}
bool vop_has_literal(uint32_t w) { return (w & 0x1FFu) == LITERAL; }   // src0 is 9 bits
}  // namespace

Rdna2Inst rdna2_decode_one(const uint32_t* code, size_t max_dwords) {
    Rdna2Inst i;
    if (max_dwords == 0) { i.fmt = Rdna2Format::Unknown; i.len_dwords = 0; return i; }
    const uint32_t w = code[0];
    i.words[0] = w;

    auto two_dword = [&](Rdna2Format f) {
        i.fmt = f;
        i.len_dwords = (max_dwords >= 2) ? 2 : 1;
        if (max_dwords >= 2) i.words[1] = code[1];
    };
    auto one_plus_lit = [&](Rdna2Format f, bool lit) {
        i.fmt = f;
        if (lit && max_dwords >= 2) { i.has_literal = true; i.literal = code[1]; i.len_dwords = 2; }
        else i.len_dwords = 1;
    };

    if ((w & 0x80000000u) == 0u) {
        // Vector ALU group (bit31 == 0): VOP1 (0x7E prefix), VOPC (0x7C prefix), else VOP2.
        if ((w & 0xFE000000u) == 0x7E000000u)      one_plus_lit(Rdna2Format::VOP1, vop_has_literal(w));
        else if ((w & 0xFE000000u) == 0x7C000000u) one_plus_lit(Rdna2Format::VOPC, vop_has_literal(w));
        else                                       one_plus_lit(Rdna2Format::VOP2, vop_has_literal(w));
    } else if ((w & 0xC0000000u) == 0x80000000u) {
        // Scalar group (bits[31:30] == 10). Carve out SOPP/SOPC/SOP1/SOPK before the SOP2 default.
        if      ((w & 0xFF800000u) == 0xBF800000u) { i.fmt = Rdna2Format::SOPP; i.len_dwords = 1;
                                                     i.is_end = (w == S_ENDPGM); }
        else if ((w & 0xFF800000u) == 0xBF000000u) one_plus_lit(Rdna2Format::SOPC, sop_has_literal(w, 2));
        else if ((w & 0xFF800000u) == 0xBE800000u) one_plus_lit(Rdna2Format::SOP1, sop_has_literal(w, 1));
        else if ((w & 0xF0000000u) == 0xB0000000u) { i.fmt = Rdna2Format::SOPK; i.len_dwords = 1; }
        else                                       one_plus_lit(Rdna2Format::SOP2, sop_has_literal(w, 2));
    } else {
        switch (w >> 26u) {
            case 0x32: i.fmt = Rdna2Format::VINTRP; i.len_dwords = 1; break;
            case 0x34: case 0x35: two_dword(Rdna2Format::VOP3);  break;   // 0x34 old-gen, 0x35 RDNA2
            case 0x36: two_dword(Rdna2Format::DS);    break;
            case 0x37: two_dword(Rdna2Format::FLAT);  break;
            case 0x38: two_dword(Rdna2Format::MUBUF); break;
            case 0x3a: two_dword(Rdna2Format::MTBUF); break;
            case 0x3c: two_dword(Rdna2Format::MIMG);  break;
            case 0x3d: two_dword(Rdna2Format::SMEM);  break;
            case 0x3e: two_dword(Rdna2Format::EXP);   break;
            default:   i.fmt = Rdna2Format::Unknown;  i.len_dwords = 1; break;
        }
    }
    if (i.len_dwords > max_dwords) i.len_dwords = (uint32_t)max_dwords;   // clamp at buffer end
    return i;
}

size_t rdna2_walk(const uint32_t* code, size_t dwords, std::vector<Rdna2Inst>& out) {
    size_t pc = 0;
    while (pc < dwords) {
        Rdna2Inst i = rdna2_decode_one(code + pc, dwords - pc);
        i.pc = (uint32_t)pc;
        out.push_back(i);
        if (i.len_dwords == 0) break;               // safety: never advance 0
        pc += i.len_dwords;
        if (i.is_end || i.fmt == Rdna2Format::Unknown) break;
    }
    return pc;
}

} // namespace prosper::gpu
