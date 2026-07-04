// render_state.hpp — interpret a GpuState's register files into a semantic render-state.
//
// Third CommandProcessor stage (after decode + apply): read the well-defined RDNA2 hardware registers
// out of the folded register files and expose them as a small, typed RenderState that a Vulkan
// backend (M4) can turn into a pipeline + draw. Register offsets/masks come from pm4_registers.hpp
// (vendored from Kyty). Address fields use the confirmed RDNA2 convention `addr = (LO<<8) |
// ((HI&0xff)<<40)` (verified against Kyty GraphicsRun.cpp). Fields whose in-word bit layout is not
// yet confirmed are exposed as faithful RAW register values rather than guessed-apart, so nothing
// here is fabricated.
#pragma once
#include "command_processor.hpp"
#include <cstdint>

namespace prosper::gpu {

struct RenderState {
    // Shader program GPU addresses per stage (byte address; 0 if that stage's PGM regs were unset).
    uint64_t ps_addr = 0;   // pixel  (SPI_SHADER_PGM_{LO,HI}_PS)
    uint64_t gs_addr = 0;   // geometry (…_GS)
    uint64_t es_addr = 0;   // export/vertex (…_ES)
    uint64_t hs_addr = 0;   // hull/tess (…_HS)

    // Color MRT 0.
    uint64_t color0_base   = 0;   // byte address (CB_COLOR0_BASE + BASE_EXT)
    uint32_t color0_format = 0;   // CB_COLOR0_INFO.FORMAT  (bits [4:0])

    // Primitive topology (VGT_PRIMITIVE_TYPE.PRIM_TYPE, bits [5:0]).
    uint32_t prim_type = 0;

    // Raw state registers — bit layouts decoded by the Vulkan backend later (kept faithful here).
    uint32_t db_depth_control  = 0;   // DB_DEPTH_CONTROL
    uint32_t cb_color_control  = 0;   // CB_COLOR_CONTROL
    uint32_t cb_blend0_control = 0;   // CB_BLEND0_CONTROL
    uint32_t cb_target_mask    = 0;   // CB_TARGET_MASK (per-MRT write mask)
};

// Extract the render-state from a folded GpuState (pure; reads register files only).
RenderState extract_render_state(const GpuState& st);

} // namespace prosper::gpu
