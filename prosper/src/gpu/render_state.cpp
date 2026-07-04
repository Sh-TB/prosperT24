// render_state.cpp — see render_state.hpp.
#include "render_state.hpp"
#include "pm4_registers.hpp"

namespace prosper::gpu {

namespace {
namespace P = prosper::agc::Pm4;

// Read a register value from a class file (0 if unset).
uint32_t rd(const std::unordered_map<uint32_t, uint32_t>& file, uint32_t off) {
    auto it = file.find(off);
    return it == file.end() ? 0u : it->second;
}

// RDNA2 base-address pair: LO holds bits [39:8], HI holds bits [47:40]. Verified vs Kyty
// GraphicsRun.cpp (`(lo<<8) | ((hi&0xff)<<40)`).
uint64_t addr_of(uint32_t lo, uint32_t hi) {
    return (static_cast<uint64_t>(lo) << 8) | (static_cast<uint64_t>(hi & 0xffu) << 40);
}
}  // namespace

RenderState extract_render_state(const GpuState& st) {
    RenderState rs;

    // Shader program addresses (SH register file).
    rs.ps_addr = addr_of(rd(st.sh, P::SPI_SHADER_PGM_LO_PS), rd(st.sh, P::SPI_SHADER_PGM_HI_PS));
    rs.gs_addr = addr_of(rd(st.sh, P::SPI_SHADER_PGM_LO_GS), rd(st.sh, P::SPI_SHADER_PGM_HI_GS));
    rs.es_addr = addr_of(rd(st.sh, P::SPI_SHADER_PGM_LO_ES), rd(st.sh, P::SPI_SHADER_PGM_HI_ES));
    rs.hs_addr = addr_of(rd(st.sh, P::SPI_SHADER_PGM_LO_HS), rd(st.sh, P::SPI_SHADER_PGM_HI_HS));

    // Color MRT 0 (context register file).
    rs.color0_base   = addr_of(rd(st.cx, P::CB_COLOR0_BASE), rd(st.cx, P::CB_COLOR0_BASE_EXT));
    rs.color0_format = rd(st.cx, P::CB_COLOR0_INFO) & P::CB_COLOR0_INFO_FORMAT_MASK;   // FORMAT @ bit 0

    // Primitive topology (PRIM_TYPE is bits [5:0] of VGT_PRIMITIVE_TYPE).
    rs.prim_type = rd(st.cx, P::VGT_PRIMITIVE_TYPE) & 0x3fu;

    // Faithful raw state registers.
    rs.db_depth_control  = rd(st.cx, P::DB_DEPTH_CONTROL);
    rs.cb_color_control  = rd(st.cx, P::CB_COLOR_CONTROL);
    rs.cb_blend0_control = rd(st.cx, P::CB_BLEND0_CONTROL);
    rs.cb_target_mask    = rd(st.cx, P::CB_TARGET_MASK);

    return rs;
}

} // namespace prosper::gpu
