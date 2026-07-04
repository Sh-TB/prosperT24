// agc_reg_defaults.cpp — the RegisterDefaults tables that libSceAgc's
// sceAgcGetRegisterDefaults2 (NID 2JtWUUiYBXs) / GetRegisterDefaults (wRbq6ZjNop4) return.
//
// The tables (register hashes, PM4 register offsets, default values) and the PM4 register
// constants are VENDORED VERBATIM from Kyty (MIT License, Copyright (c) 2021 Ivan Chikhradze),
// files source/emulator/src/Graphics/Graphics.cpp and include/Emulator/Graphics/Pm4.h. Kyty
// reverse-engineered the exact layout the PS5 AGC SDK expects. prosper reuses it so the game's
// Gen5 graphics init (Unity GfxDevicePS5) can build its internal register-offset table from a
// real, non-empty RegisterDefaults instead of the previous count=0 placeholder.
//
// Consumed by hle_graphics.cpp's GetRegisterDefaults2 thunk via prosper_agc_reg_defaults().
#include <cstddef>
#include <cstdint>

namespace prosper { namespace agc {

// PM4 register offsets + packet constants (Kyty Pm4.h).
namespace Pm4 {
constexpr uint32_t IT_NOP                       = 0x10;
constexpr uint32_t IT_SET_BASE                  = 0x11;
constexpr uint32_t IT_CLEAR_STATE               = 0x12;
constexpr uint32_t IT_INDEX_BUFFER_SIZE         = 0x13;
constexpr uint32_t IT_DISPATCH_DIRECT           = 0x15;
constexpr uint32_t IT_DISPATCH_INDIRECT         = 0x16;
constexpr uint32_t IT_SET_PREDICATION           = 0x20;
constexpr uint32_t IT_COND_EXEC                 = 0x22;
constexpr uint32_t IT_DRAW_INDIRECT             = 0x24;
constexpr uint32_t IT_DRAW_INDEX_INDIRECT       = 0x25;
constexpr uint32_t IT_INDEX_BASE                = 0x26;
constexpr uint32_t IT_DRAW_INDEX_2              = 0x27;
constexpr uint32_t IT_CONTEXT_CONTROL           = 0x28;
constexpr uint32_t IT_INDEX_TYPE                = 0x2A;
constexpr uint32_t IT_DRAW_INDIRECT_MULTI       = 0x2C;
constexpr uint32_t IT_DRAW_INDEX_AUTO           = 0x2D;
constexpr uint32_t IT_NUM_INSTANCES             = 0x2F;
constexpr uint32_t IT_INDIRECT_BUFFER_CNST      = 0x33;
constexpr uint32_t IT_DRAW_INDEX_OFFSET_2       = 0x35;
constexpr uint32_t IT_WRITE_DATA                = 0x37;
constexpr uint32_t IT_MEM_SEMAPHORE             = 0x39;
constexpr uint32_t IT_DRAW_INDEX_INDIRECT_MULTI = 0x38;
constexpr uint32_t IT_WAIT_REG_MEM              = 0x3C;
constexpr uint32_t IT_INDIRECT_BUFFER           = 0x3F;
constexpr uint32_t IT_COPY_DATA                 = 0x40;
constexpr uint32_t IT_CP_DMA                    = 0x41;
constexpr uint32_t IT_PFP_SYNC_ME               = 0x42;
constexpr uint32_t IT_SURFACE_SYNC              = 0x43;
constexpr uint32_t IT_EVENT_WRITE               = 0x46;
constexpr uint32_t IT_EVENT_WRITE_EOP           = 0x47;
constexpr uint32_t IT_EVENT_WRITE_EOS           = 0x48;
constexpr uint32_t IT_RELEASE_MEM               = 0x49;
constexpr uint32_t IT_DMA_DATA                  = 0x50;
constexpr uint32_t IT_ACQUIRE_MEM               = 0x58;
constexpr uint32_t IT_REWIND                    = 0x59;
constexpr uint32_t IT_SET_CONFIG_REG            = 0x68;
constexpr uint32_t IT_SET_CONTEXT_REG           = 0x69;
constexpr uint32_t IT_SET_SH_REG                = 0x76;
constexpr uint32_t IT_SET_QUEUE_REG             = 0x78;
constexpr uint32_t IT_SET_UCONFIG_REG           = 0x79;
constexpr uint32_t IT_WRITE_CONST_RAM           = 0x81;
constexpr uint32_t IT_DUMP_CONST_RAM            = 0x83;
constexpr uint32_t IT_INCREMENT_CE_COUNTER      = 0x84;
constexpr uint32_t IT_INCREMENT_DE_COUNTER      = 0x85;
constexpr uint32_t IT_WAIT_ON_CE_COUNTER        = 0x86;
constexpr uint32_t IT_WAIT_ON_DE_COUNTER_DIFF   = 0x88;
constexpr uint32_t IT_DISPATCH_DRAW_PREAMBLE    = 0x8C;
constexpr uint32_t IT_DISPATCH_DRAW             = 0x8D;
constexpr uint32_t R_ZERO             = 0x00;
constexpr uint32_t R_VS               = 0x01;
constexpr uint32_t R_PS               = 0x02;
constexpr uint32_t R_DRAW_INDEX       = 0x03;
constexpr uint32_t R_DRAW_INDEX_AUTO  = 0x04;
constexpr uint32_t R_DRAW_RESET       = 0x05;
constexpr uint32_t R_WAIT_FLIP_DONE   = 0x06;
constexpr uint32_t R_CS               = 0x07;
constexpr uint32_t R_DISPATCH_DIRECT  = 0x08;
constexpr uint32_t R_DISPATCH_RESET   = 0x09;
constexpr uint32_t R_WAIT_MEM_32      = 0x0A;
constexpr uint32_t R_PUSH_MARKER      = 0x0B;
constexpr uint32_t R_POP_MARKER       = 0x0C;
constexpr uint32_t R_VS_EMBEDDED      = 0x0D;
constexpr uint32_t R_PS_EMBEDDED      = 0x0E;
constexpr uint32_t R_VS_UPDATE        = 0x0F;
constexpr uint32_t R_PS_UPDATE        = 0x10;
constexpr uint32_t R_SH_REGS_INDIRECT = 0x11;
constexpr uint32_t R_CX_REGS_INDIRECT = 0x12;
constexpr uint32_t R_UC_REGS_INDIRECT = 0x13;
constexpr uint32_t R_ACQUIRE_MEM      = 0x14;
constexpr uint32_t R_WRITE_DATA       = 0x15;
constexpr uint32_t R_WAIT_MEM_64      = 0x16;
constexpr uint32_t R_FLIP             = 0x17;
constexpr uint32_t R_RELEASE_MEM      = 0x18;
constexpr uint32_t DB_RENDER_CONTROL                                = 0x0;
constexpr uint32_t DB_RENDER_CONTROL_DEPTH_CLEAR_ENABLE_MASK        = 0x1;
constexpr uint32_t DB_RENDER_CONTROL_STENCIL_CLEAR_ENABLE_MASK      = 0x1;
constexpr uint32_t DB_RENDER_CONTROL_RESUMMARIZE_ENABLE_MASK        = 0x1;
constexpr uint32_t DB_RENDER_CONTROL_STENCIL_COMPRESS_DISABLE_MASK  = 0x1;
constexpr uint32_t DB_RENDER_CONTROL_DEPTH_COMPRESS_DISABLE_MASK    = 0x1;
constexpr uint32_t DB_RENDER_CONTROL_COPY_CENTROID_MASK             = 0x1;
constexpr uint32_t DB_RENDER_CONTROL_COPY_SAMPLE_MASK               = 0xF;
constexpr uint32_t DB_COUNT_CONTROL = 0x1;
constexpr uint32_t DB_DEPTH_VIEW                         = 0x2;
constexpr uint32_t DB_DEPTH_VIEW_SLICE_START_MASK        = 0x7FF;
constexpr uint32_t DB_DEPTH_VIEW_SLICE_START_HI_MASK     = 0x3;
constexpr uint32_t DB_DEPTH_VIEW_SLICE_MAX_MASK          = 0x7FF;
constexpr uint32_t DB_DEPTH_VIEW_Z_READ_ONLY_MASK        = 0x1;
constexpr uint32_t DB_DEPTH_VIEW_STENCIL_READ_ONLY_MASK  = 0x1;
constexpr uint32_t DB_DEPTH_VIEW_MIPID_MASK              = 0xF;
constexpr uint32_t DB_DEPTH_VIEW_SLICE_MAX_HI_MASK       = 0x3;
constexpr uint32_t DB_RENDER_OVERRIDE              = 0x3;
constexpr uint32_t DB_RENDER_OVERRIDE2             = 0x4;
constexpr uint32_t DB_HTILE_DATA_BASE              = 0x5;
constexpr uint32_t PS_SHADER_SAMPLE_EXCLUSION_MASK = 0x6;
constexpr uint32_t DB_DEPTH_SIZE_XY             = 0x7;
constexpr uint32_t DB_DEPTH_SIZE_XY_X_MAX_MASK  = 0x3FFF;
constexpr uint32_t DB_DEPTH_SIZE_XY_Y_MAX_MASK  = 0x3FFF;
constexpr uint32_t DB_DEPTH_BOUNDS_MIN = 0x8;
constexpr uint32_t DB_DEPTH_BOUNDS_MAX = 0x9;
constexpr uint32_t DB_STENCIL_CLEAR             = 0xA;
constexpr uint32_t DB_STENCIL_CLEAR_CLEAR_MASK  = 0xFF;
constexpr uint32_t DB_DEPTH_CLEAR                   = 0xB;
constexpr uint32_t DB_DEPTH_CLEAR_DEPTH_CLEAR_MASK  = 0xFFFFFFFF;
constexpr uint32_t PA_SC_SCREEN_SCISSOR_TL            = 0xC;
constexpr uint32_t PA_SC_SCREEN_SCISSOR_TL_TL_X_MASK  = 0xFFFF;
constexpr uint32_t PA_SC_SCREEN_SCISSOR_TL_TL_Y_MASK  = 0xFFFF;
constexpr uint32_t PA_SC_SCREEN_SCISSOR_BR            = 0xD;
constexpr uint32_t PA_SC_SCREEN_SCISSOR_BR_BR_X_MASK  = 0xFFFF;
constexpr uint32_t PA_SC_SCREEN_SCISSOR_BR_BR_Y_MASK  = 0xFFFF;
constexpr uint32_t DB_DFSM_CONTROL = 0xE;
constexpr uint32_t DB_DEPTH_INFO                          = 0xF;
constexpr uint32_t DB_DEPTH_INFO_ADDR5_SWIZZLE_MASK_MASK  = 0xF;
constexpr uint32_t DB_DEPTH_INFO_ARRAY_MODE_MASK          = 0xF;
constexpr uint32_t DB_DEPTH_INFO_PIPE_CONFIG_MASK         = 0x1F;
constexpr uint32_t DB_DEPTH_INFO_BANK_WIDTH_MASK          = 0x3;
constexpr uint32_t DB_DEPTH_INFO_BANK_HEIGHT_MASK         = 0x3;
constexpr uint32_t DB_DEPTH_INFO_MACRO_TILE_ASPECT_MASK   = 0x3;
constexpr uint32_t DB_DEPTH_INFO_NUM_BANKS_MASK           = 0x3;
constexpr uint32_t DB_Z_INFO                               = 0x10;
constexpr uint32_t DB_Z_INFO_FORMAT_MASK                   = 0x3;
constexpr uint32_t DB_Z_INFO_NUM_SAMPLES_MASK              = 0x3;
constexpr uint32_t DB_Z_INFO_ITERATE_FLUSH_MASK            = 0x1;
constexpr uint32_t DB_Z_INFO_PARTIALLY_RESIDENT_MASK       = 0x1;
constexpr uint32_t DB_Z_INFO_MAXMIP_MASK                   = 0xF;
constexpr uint32_t DB_Z_INFO_TILE_MODE_INDEX_MASK          = 0x7;
constexpr uint32_t DB_Z_INFO_DECOMPRESS_ON_N_ZPLANES_MASK  = 0xF;
constexpr uint32_t DB_Z_INFO_ALLOW_EXPCLEAR_MASK           = 0x1;
constexpr uint32_t DB_Z_INFO_TILE_SURFACE_ENABLE_MASK      = 0x1;
constexpr uint32_t DB_Z_INFO_ZRANGE_PRECISION_MASK         = 0x1;
constexpr uint32_t DB_STENCIL_INFO                            = 0x11;
constexpr uint32_t DB_STENCIL_INFO_FORMAT_MASK                = 0x1;
constexpr uint32_t DB_STENCIL_INFO_ITERATE_FLUSH_MASK         = 0x1;
constexpr uint32_t DB_STENCIL_INFO_PARTIALLY_RESIDENT_MASK    = 0x1;
constexpr uint32_t DB_STENCIL_INFO_RESERVED_FIELD_1_MASK      = 0x7;
constexpr uint32_t DB_STENCIL_INFO_TILE_MODE_INDEX_MASK       = 0x7;
constexpr uint32_t DB_STENCIL_INFO_ALLOW_EXPCLEAR_MASK        = 0x1;
constexpr uint32_t DB_STENCIL_INFO_TILE_STENCIL_DISABLE_MASK  = 0x1;
constexpr uint32_t DB_Z_READ_BASE        = 0x12;
constexpr uint32_t DB_STENCIL_READ_BASE  = 0x13;
constexpr uint32_t DB_Z_WRITE_BASE       = 0x14;
constexpr uint32_t DB_STENCIL_WRITE_BASE = 0x15;
constexpr uint32_t DB_DEPTH_SIZE                       = 0x16;
constexpr uint32_t DB_DEPTH_SIZE_PITCH_TILE_MAX_MASK   = 0x7FF;
constexpr uint32_t DB_DEPTH_SIZE_HEIGHT_TILE_MAX_MASK  = 0x7FF;
constexpr uint32_t DB_DEPTH_SLICE                      = 0x17;
constexpr uint32_t DB_DEPTH_SLICE_SLICE_TILE_MAX_MASK  = 0x3FFFFF;
constexpr uint32_t DB_Z_READ_BASE_HI        = 0x1A;
constexpr uint32_t DB_STENCIL_READ_BASE_HI  = 0x1B;
constexpr uint32_t DB_Z_WRITE_BASE_HI       = 0x1C;
constexpr uint32_t DB_STENCIL_WRITE_BASE_HI = 0x1D;
constexpr uint32_t DB_HTILE_DATA_BASE_HI    = 0x1E;
constexpr uint32_t DB_RMI_L2_CACHE_CONTROL  = 0x1F;
constexpr uint32_t TA_BC_BASE_ADDR          = 0x20;
constexpr uint32_t TA_BC_BASE_ADDR_HI       = 0x21;
constexpr uint32_t PA_SC_WINDOW_OFFSET      = 0x80;
constexpr uint32_t PA_SC_WINDOW_SCISSOR_TL  = 0x81;
constexpr uint32_t PA_SC_WINDOW_SCISSOR_BR  = 0x82;
constexpr uint32_t PA_SC_CLIPRECT_RULE      = 0x83;
constexpr uint32_t PA_SC_CLIPRECT_0_TL      = 0x84;
constexpr uint32_t PA_SC_CLIPRECT_0_BR      = 0x85;
constexpr uint32_t PA_SU_HARDWARE_SCREEN_OFFSET                          = 0x8D;
constexpr uint32_t PA_SU_HARDWARE_SCREEN_OFFSET_HW_SCREEN_OFFSET_X_MASK  = 0x1FF;
constexpr uint32_t PA_SU_HARDWARE_SCREEN_OFFSET_HW_SCREEN_OFFSET_Y_MASK  = 0x1FF;
constexpr uint32_t CB_TARGET_MASK = 0x8E;
constexpr uint32_t CB_SHADER_MASK = 0x8F;
constexpr uint32_t PA_SC_GENERIC_SCISSOR_TL                             = 0x90;
constexpr uint32_t PA_SC_GENERIC_SCISSOR_TL_TL_X_MASK                   = 0x7FFF;
constexpr uint32_t PA_SC_GENERIC_SCISSOR_TL_TL_Y_MASK                   = 0x7FFF;
constexpr uint32_t PA_SC_GENERIC_SCISSOR_TL_WINDOW_OFFSET_DISABLE_MASK  = 0x1;
constexpr uint32_t PA_SC_GENERIC_SCISSOR_BR            = 0x91;
constexpr uint32_t PA_SC_GENERIC_SCISSOR_BR_BR_X_MASK  = 0x7FFF;
constexpr uint32_t PA_SC_GENERIC_SCISSOR_BR_BR_Y_MASK  = 0x7FFF;
constexpr uint32_t PA_SC_VPORT_SCISSOR_0_TL                             = 0x94;
constexpr uint32_t PA_SC_VPORT_SCISSOR_0_TL_TL_X_MASK                   = 0x7FFF;
constexpr uint32_t PA_SC_VPORT_SCISSOR_0_TL_TL_Y_MASK                   = 0x7FFF;
constexpr uint32_t PA_SC_VPORT_SCISSOR_0_TL_WINDOW_OFFSET_DISABLE_MASK  = 0x1;
constexpr uint32_t PA_SC_VPORT_SCISSOR_0_BR            = 0x95;
constexpr uint32_t PA_SC_VPORT_SCISSOR_0_BR_BR_X_MASK  = 0x7FFF;
constexpr uint32_t PA_SC_VPORT_SCISSOR_0_BR_BR_Y_MASK  = 0x7FFF;
constexpr uint32_t PA_SC_VPORT_SCISSOR_15_TL = 0xB2;
constexpr uint32_t PA_SC_VPORT_SCISSOR_15_BR = 0xB3;
constexpr uint32_t PA_SC_VPORT_ZMIN_0        = 0xB4;
constexpr uint32_t PA_SC_VPORT_ZMAX_0        = 0xB5;
constexpr uint32_t PA_SC_VPORT_ZMIN_15       = 0xD2;
constexpr uint32_t PA_SC_VPORT_ZMAX_15       = 0xD3;
constexpr uint32_t PA_SC_RIGHT_VERT_GRID     = 0xE8;
constexpr uint32_t PA_SC_LEFT_VERT_GRID      = 0xE9;
constexpr uint32_t PA_SC_HORIZ_GRID          = 0xEA;
constexpr uint32_t PA_SC_FOV_WINDOW_LR       = 0xEB;
constexpr uint32_t PA_SC_FOV_WINDOW_TB       = 0xEC;
constexpr uint32_t CB_RMI_GL2_CACHE_CONTROL  = 0x104;
constexpr uint32_t CB_BLEND_RED              = 0x105;
constexpr uint32_t CB_BLEND_GREEN            = 0x106;
constexpr uint32_t CB_BLEND_BLUE             = 0x107;
constexpr uint32_t CB_BLEND_ALPHA            = 0x108;
constexpr uint32_t CB_DCC_CONTROL            = 0x109;
constexpr uint32_t DB_STENCIL_CONTROL                       = 0x10B;
constexpr uint32_t DB_STENCIL_CONTROL_STENCILFAIL_MASK      = 0xF;
constexpr uint32_t DB_STENCIL_CONTROL_STENCILZPASS_MASK     = 0xF;
constexpr uint32_t DB_STENCIL_CONTROL_STENCILZFAIL_MASK     = 0xF;
constexpr uint32_t DB_STENCIL_CONTROL_STENCILFAIL_BF_MASK   = 0xF;
constexpr uint32_t DB_STENCIL_CONTROL_STENCILZPASS_BF_MASK  = 0xF;
constexpr uint32_t DB_STENCIL_CONTROL_STENCILZFAIL_BF_MASK  = 0xF;
constexpr uint32_t DB_STENCILREFMASK                        = 0x10C;
constexpr uint32_t DB_STENCILREFMASK_STENCILTESTVAL_MASK    = 0xFF;
constexpr uint32_t DB_STENCILREFMASK_STENCILMASK_MASK       = 0xFF;
constexpr uint32_t DB_STENCILREFMASK_STENCILWRITEMASK_MASK  = 0xFF;
constexpr uint32_t DB_STENCILREFMASK_STENCILOPVAL_MASK      = 0xFF;
constexpr uint32_t DB_STENCILREFMASK_BF                           = 0x10D;
constexpr uint32_t DB_STENCILREFMASK_BF_STENCILTESTVAL_BF_MASK    = 0xFF;
constexpr uint32_t DB_STENCILREFMASK_BF_STENCILMASK_BF_MASK       = 0xFF;
constexpr uint32_t DB_STENCILREFMASK_BF_STENCILWRITEMASK_BF_MASK  = 0xFF;
constexpr uint32_t DB_STENCILREFMASK_BF_STENCILOPVAL_BF_MASK      = 0xFF;
constexpr uint32_t PA_CL_VPORT_XSCALE     = 0x10F;
constexpr uint32_t PA_CL_VPORT_XOFFSET    = 0x110;
constexpr uint32_t PA_CL_VPORT_YSCALE     = 0x111;
constexpr uint32_t PA_CL_VPORT_YOFFSET    = 0x112;
constexpr uint32_t PA_CL_VPORT_ZSCALE     = 0x113;
constexpr uint32_t PA_CL_VPORT_ZOFFSET    = 0x114;
constexpr uint32_t PA_CL_VPORT_XSCALE_15  = 0x169;
constexpr uint32_t PA_CL_VPORT_XOFFSET_15 = 0x16A;
constexpr uint32_t PA_CL_VPORT_YSCALE_15  = 0x16B;
constexpr uint32_t PA_CL_VPORT_YOFFSET_15 = 0x16C;
constexpr uint32_t PA_CL_VPORT_ZSCALE_15  = 0x16D;
constexpr uint32_t PA_CL_VPORT_ZOFFSET_15 = 0x16E;
constexpr uint32_t PA_CL_UCP_0_X          = 0x16F;
constexpr uint32_t PA_CL_UCP_0_Y          = 0x170;
constexpr uint32_t PA_CL_UCP_0_Z          = 0x171;
constexpr uint32_t PA_CL_UCP_0_W          = 0x172;
constexpr uint32_t SPI_PS_INPUT_CNTL_0    = 0x191;
constexpr uint32_t SPI_PS_INPUT_CNTL_31   = 0x1B0;
constexpr uint32_t SPI_VS_OUT_CONFIG      = 0x1B1;
constexpr uint32_t SPI_PS_INPUT_ENA       = 0x1B3;
constexpr uint32_t SPI_PS_INPUT_ADDR      = 0x1B4;
constexpr uint32_t SPI_INTERP_CONTROL_0   = 0x1B5;
constexpr uint32_t SPI_PS_IN_CONTROL      = 0x1B6;
constexpr uint32_t SPI_BARYC_CNTL         = 0x1B8;
constexpr uint32_t SPI_TMPRING_SIZE       = 0x1BA;
constexpr uint32_t SPI_SHADER_IDX_FORMAT  = 0x1C2;
constexpr uint32_t SPI_SHADER_POS_FORMAT  = 0x1C3;
constexpr uint32_t SPI_SHADER_Z_FORMAT    = 0x1C4;
constexpr uint32_t SPI_SHADER_COL_FORMAT  = 0x1C5;
constexpr uint32_t CB_BLEND0_CONTROL                            = 0x1E0;
constexpr uint32_t CB_BLEND0_CONTROL_COLOR_SRCBLEND_MASK        = 0x1F;
constexpr uint32_t CB_BLEND0_CONTROL_COLOR_COMB_FCN_MASK        = 0x7;
constexpr uint32_t CB_BLEND0_CONTROL_COLOR_DESTBLEND_MASK       = 0x1F;
constexpr uint32_t CB_BLEND0_CONTROL_ALPHA_SRCBLEND_MASK        = 0x1F;
constexpr uint32_t CB_BLEND0_CONTROL_ALPHA_COMB_FCN_MASK        = 0x7;
constexpr uint32_t CB_BLEND0_CONTROL_ALPHA_DESTBLEND_MASK       = 0x1F;
constexpr uint32_t CB_BLEND0_CONTROL_SEPARATE_ALPHA_BLEND_MASK  = 0x1;
constexpr uint32_t CB_BLEND0_CONTROL_ENABLE_MASK                = 0x1;
constexpr uint32_t GE_MAX_OUTPUT_PER_SUBGROUP = 0x1FF;
constexpr uint32_t DB_DEPTH_CONTROL                                          = 0x200;
constexpr uint32_t DB_DEPTH_CONTROL_STENCIL_ENABLE_MASK                      = 0x1;
constexpr uint32_t DB_DEPTH_CONTROL_Z_ENABLE_MASK                            = 0x1;
constexpr uint32_t DB_DEPTH_CONTROL_Z_WRITE_ENABLE_MASK                      = 0x1;
constexpr uint32_t DB_DEPTH_CONTROL_DEPTH_BOUNDS_ENABLE_MASK                 = 0x1;
constexpr uint32_t DB_DEPTH_CONTROL_ZFUNC_MASK                               = 0x7;
constexpr uint32_t DB_DEPTH_CONTROL_BACKFACE_ENABLE_MASK                     = 0x1;
constexpr uint32_t DB_DEPTH_CONTROL_STENCILFUNC_MASK                         = 0x7;
constexpr uint32_t DB_DEPTH_CONTROL_STENCILFUNC_BF_MASK                      = 0x7;
constexpr uint32_t DB_DEPTH_CONTROL_ENABLE_COLOR_WRITES_ON_DEPTH_FAIL_MASK   = 0x1;
constexpr uint32_t DB_DEPTH_CONTROL_DISABLE_COLOR_WRITES_ON_DEPTH_PASS_MASK  = 0x1;
constexpr uint32_t DB_EQAA                                  = 0x201;
constexpr uint32_t DB_EQAA_MAX_ANCHOR_SAMPLES_MASK          = 0x7;
constexpr uint32_t DB_EQAA_PS_ITER_SAMPLES_MASK             = 0x7;
constexpr uint32_t DB_EQAA_MASK_EXPORT_NUM_SAMPLES_MASK     = 0x7;
constexpr uint32_t DB_EQAA_ALPHA_TO_MASK_NUM_SAMPLES_MASK   = 0x7;
constexpr uint32_t DB_EQAA_HIGH_QUALITY_INTERSECTIONS_MASK  = 0x1;
constexpr uint32_t DB_EQAA_INCOHERENT_EQAA_READS_MASK       = 0x1;
constexpr uint32_t DB_EQAA_INTERPOLATE_COMP_Z_MASK          = 0x1;
constexpr uint32_t DB_EQAA_STATIC_ANCHOR_ASSOCIATIONS_MASK  = 0x1;
constexpr uint32_t DB_SHADER_CONTROL                             = 0x203;
constexpr uint32_t DB_SHADER_CONTROL_Z_EXPORT_ENABLE_MASK        = 0x1;
constexpr uint32_t DB_SHADER_CONTROL_Z_ORDER_MASK                = 0x3;
constexpr uint32_t DB_SHADER_CONTROL_KILL_ENABLE_MASK            = 0x1;
constexpr uint32_t DB_SHADER_CONTROL_EXEC_ON_NOOP_MASK           = 0x1;
constexpr uint32_t DB_SHADER_CONTROL_CONSERVATIVE_Z_EXPORT_MASK  = 0x3;
constexpr uint32_t CB_COLOR_CONTROL            = 0x202;
constexpr uint32_t CB_COLOR_CONTROL_MODE_MASK  = 0x7;
constexpr uint32_t CB_COLOR_CONTROL_ROP3_MASK  = 0xFF;
constexpr uint32_t PA_CL_CLIP_CNTL                                 = 0x204;
constexpr uint32_t PA_CL_CLIP_CNTL_UCP_ENA_MASK                    = 0x3F;
constexpr uint32_t PA_CL_CLIP_CNTL_PS_UCP_Y_SCALE_NEG_MASK         = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_PS_UCP_MODE_MASK                = 0x3;
constexpr uint32_t PA_CL_CLIP_CNTL_CLIP_DISABLE_MASK               = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_UCP_CULL_ONLY_ENA_MASK          = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_BOUNDARY_EDGE_FLAG_ENA_MASK     = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_DX_CLIP_SPACE_DEF_MASK          = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_DIS_CLIP_ERR_DETECT_MASK        = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_VTX_KILL_OR_MASK                = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_DX_RASTERIZATION_KILL_MASK      = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_DX_LINEAR_ATTR_CLIP_ENA_MASK    = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_VTE_VPORT_PROVOKE_DISABLE_MASK  = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_ZCLIP_NEAR_DISABLE_MASK         = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_ZCLIP_FAR_DISABLE_MASK          = 0x1;
constexpr uint32_t PA_CL_CLIP_CNTL_ZCLIP_PROG_NEAR_ENA_MASK        = 0x1;
constexpr uint32_t PA_SU_SC_MODE_CNTL                                = 0x205;
constexpr uint32_t PA_SU_SC_MODE_CNTL_CULL_FRONT_MASK                = 0x1;
constexpr uint32_t PA_SU_SC_MODE_CNTL_CULL_BACK_MASK                 = 0x1;
constexpr uint32_t PA_SU_SC_MODE_CNTL_FACE_MASK                      = 0x1;
constexpr uint32_t PA_SU_SC_MODE_CNTL_POLY_MODE_MASK                 = 0x3;
constexpr uint32_t PA_SU_SC_MODE_CNTL_POLYMODE_FRONT_PTYPE_MASK      = 0x7;
constexpr uint32_t PA_SU_SC_MODE_CNTL_POLYMODE_BACK_PTYPE_MASK       = 0x7;
constexpr uint32_t PA_SU_SC_MODE_CNTL_POLY_OFFSET_FRONT_ENABLE_MASK  = 0x1;
constexpr uint32_t PA_SU_SC_MODE_CNTL_POLY_OFFSET_BACK_ENABLE_MASK   = 0x1;
constexpr uint32_t PA_SU_SC_MODE_CNTL_VTX_WINDOW_OFFSET_ENABLE_MASK  = 0x1;
constexpr uint32_t PA_SU_SC_MODE_CNTL_PROVOKING_VTX_LAST_MASK        = 0x1;
constexpr uint32_t PA_SU_SC_MODE_CNTL_PERSP_CORR_DIS_MASK            = 0x1;
constexpr uint32_t PA_CL_VTE_CNTL               = 0x206;
constexpr uint32_t PA_CL_VS_OUT_CNTL            = 0x207;
constexpr uint32_t PA_SU_SMALL_PRIM_FILTER_CNTL = 0x20C;
constexpr uint32_t PA_CL_OBJPRIM_ID_CNTL        = 0x20D;
constexpr uint32_t PA_STEREO_CNTL               = 0x210;
constexpr uint32_t PA_STATE_STEREO_X            = 0x211;
constexpr uint32_t PA_SU_POINT_SIZE             = 0x280;
constexpr uint32_t PA_SU_POINT_MINMAX           = 0x281;
constexpr uint32_t PA_SU_LINE_CNTL             = 0x282;
constexpr uint32_t PA_SU_LINE_CNTL_WIDTH_MASK  = 0xFFFF;
constexpr uint32_t VGT_GS_ONCHIP_CNTL = 0x291;
constexpr uint32_t PA_SC_MODE_CNTL_0                            = 0x292;
constexpr uint32_t PA_SC_MODE_CNTL_0_MSAA_ENABLE_MASK           = 0x1;
constexpr uint32_t PA_SC_MODE_CNTL_0_VPORT_SCISSOR_ENABLE_MASK  = 0x1;
constexpr uint32_t PA_SC_MODE_CNTL_0_LINE_STIPPLE_ENABLE_MASK   = 0x1;
constexpr uint32_t PA_SC_MODE_CNTL_1      = 0x293;
constexpr uint32_t VGT_GS_OUT_PRIM_TYPE   = 0x29B;
constexpr uint32_t VGT_PRIMITIVEID_EN     = 0x2A1;
constexpr uint32_t VGT_PRIMITIVEID_RESET  = 0x2A3;
constexpr uint32_t VGT_DRAW_PAYLOAD_CNTL  = 0x2A6;
constexpr uint32_t VGT_ESGS_RING_ITEMSIZE = 0x2AB;
constexpr uint32_t VGT_REUSE_OFF          = 0x2AD;
constexpr uint32_t DB_HTILE_SURFACE                               = 0x2AF;
constexpr uint32_t DB_HTILE_SURFACE_LINEAR_MASK                   = 0x1;
constexpr uint32_t DB_HTILE_SURFACE_FULL_CACHE_MASK               = 0x1;
constexpr uint32_t DB_HTILE_SURFACE_HTILE_USES_PRELOAD_WIN_MASK   = 0x1;
constexpr uint32_t DB_HTILE_SURFACE_PRELOAD_MASK                  = 0x1;
constexpr uint32_t DB_HTILE_SURFACE_PREFETCH_WIDTH_MASK           = 0x3F;
constexpr uint32_t DB_HTILE_SURFACE_PREFETCH_HEIGHT_MASK          = 0x3F;
constexpr uint32_t DB_HTILE_SURFACE_DST_OUTSIDE_ZERO_TO_ONE_MASK  = 0x1;
constexpr uint32_t DB_SRESULTS_COMPARE_STATE0     = 0x2B0;
constexpr uint32_t DB_SRESULTS_COMPARE_STATE1     = 0x2B1;
constexpr uint32_t VGT_GS_MAX_VERT_OUT            = 0x2CE;
constexpr uint32_t GE_NGG_SUBGRP_CNTL             = 0x2D3;
constexpr uint32_t VGT_TESS_DISTRIBUTION          = 0x2D4;
constexpr uint32_t VGT_SHADER_STAGES_EN           = 0x2D5;
constexpr uint32_t VGT_LS_HS_CONFIG               = 0x2D6;
constexpr uint32_t VGT_TF_PARAM                   = 0x2DB;
constexpr uint32_t DB_ALPHA_TO_MASK               = 0x2DC;
constexpr uint32_t PA_SU_POLY_OFFSET_DB_FMT_CNTL  = 0x2DE;
constexpr uint32_t PA_SU_POLY_OFFSET_CLAMP        = 0x2DF;
constexpr uint32_t PA_SU_POLY_OFFSET_FRONT_SCALE  = 0x2E0;
constexpr uint32_t PA_SU_POLY_OFFSET_FRONT_OFFSET = 0x2E1;
constexpr uint32_t PA_SU_POLY_OFFSET_BACK_SCALE   = 0x2E2;
constexpr uint32_t PA_SU_POLY_OFFSET_BACK_OFFSET  = 0x2E3;
constexpr uint32_t VGT_GS_INSTANCE_CNT            = 0x2E4;
constexpr uint32_t PA_SC_CENTROID_PRIORITY_0      = 0x2F5;
constexpr uint32_t PA_SC_CENTROID_PRIORITY_1      = 0x2F6;
constexpr uint32_t PA_SC_AA_CONFIG                             = 0x2F8;
constexpr uint32_t PA_SC_AA_CONFIG_MSAA_NUM_SAMPLES_MASK       = 0x7;
constexpr uint32_t PA_SC_AA_CONFIG_AA_MASK_CENTROID_DTMN_MASK  = 0x1;
constexpr uint32_t PA_SC_AA_CONFIG_MAX_SAMPLE_DIST_MASK        = 0xF;
constexpr uint32_t PA_SC_AA_CONFIG_MSAA_EXPOSED_SAMPLES_MASK   = 0x7;
constexpr uint32_t PA_SU_VTX_CNTL                        = 0x2F9;
constexpr uint32_t PA_CL_GB_VERT_CLIP_ADJ                = 0x2FA;
constexpr uint32_t PA_CL_GB_VERT_DISC_ADJ                = 0x2FB;
constexpr uint32_t PA_CL_GB_HORZ_CLIP_ADJ                = 0x2FC;
constexpr uint32_t PA_CL_GB_HORZ_DISC_ADJ                = 0x2FD;
constexpr uint32_t PA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0     = 0x2FE;
constexpr uint32_t PA_SC_AA_MASK_X0Y0_X1Y0               = 0x30E;
constexpr uint32_t PA_SC_AA_MASK_X0Y1_X1Y1               = 0x30F;
constexpr uint32_t PA_SC_SHADER_CONTROL                  = 0x310;
constexpr uint32_t PA_SC_BINNER_CNTL_0                   = 0x311;
constexpr uint32_t PA_SC_BINNER_CNTL_1                   = 0x312;
constexpr uint32_t PA_SC_CONSERVATIVE_RASTERIZATION_CNTL = 0x313;
constexpr uint32_t PA_SC_NGG_MODE_CNTL                   = 0x314;
constexpr uint32_t CB_COLOR0_BASE                        = 0x318;
constexpr uint32_t CB_COLOR0_VIEW                   = 0x31B;
constexpr uint32_t CB_COLOR0_VIEW_SLICE_START_MASK  = 0x1FFF;
constexpr uint32_t CB_COLOR0_VIEW_SLICE_MAX_MASK    = 0x1FFF;
constexpr uint32_t CB_COLOR0_VIEW_MIP_LEVEL_MASK    = 0xF;
constexpr uint32_t CB_COLOR0_INFO                                 = 0x31C;
constexpr uint32_t CB_COLOR0_INFO_FORMAT_MASK                     = 0x1F;
constexpr uint32_t CB_COLOR0_INFO_NUMBER_TYPE_MASK                = 0x7;
constexpr uint32_t CB_COLOR0_INFO_COMP_SWAP_MASK                  = 0x3;
constexpr uint32_t CB_COLOR0_INFO_FAST_CLEAR_MASK                 = 0x1;
constexpr uint32_t CB_COLOR0_INFO_COMPRESSION_MASK                = 0x1;
constexpr uint32_t CB_COLOR0_INFO_BLEND_CLAMP_MASK                = 0x1;
constexpr uint32_t CB_COLOR0_INFO_BLEND_BYPASS_MASK               = 0x1;
constexpr uint32_t CB_COLOR0_INFO_ROUND_MODE_MASK                 = 0x1;
constexpr uint32_t CB_COLOR0_INFO_CMASK_IS_LINEAR_MASK            = 0x1;
constexpr uint32_t CB_COLOR0_INFO_FMASK_COMPRESSION_DISABLE_MASK  = 0x1;
constexpr uint32_t CB_COLOR0_INFO_FMASK_COMPRESS_1FRAG_ONLY_MASK  = 0x1;
constexpr uint32_t CB_COLOR0_INFO_DCC_ENABLE_MASK                 = 0x1;
constexpr uint32_t CB_COLOR0_INFO_CMASK_ADDR_TYPE_MASK            = 0x3;
constexpr uint32_t CB_COLOR0_INFO_ALT_TILE_MODE_MASK              = 0x1;
constexpr uint32_t CB_COLOR0_ATTRIB                             = 0x31D;
constexpr uint32_t CB_COLOR0_ATTRIB_TILE_MODE_INDEX_MASK        = 0x1F;
constexpr uint32_t CB_COLOR0_ATTRIB_FMASK_TILE_MODE_INDEX_MASK  = 0x1F;
constexpr uint32_t CB_COLOR0_ATTRIB_NUM_SAMPLES_MASK            = 0x7;
constexpr uint32_t CB_COLOR0_ATTRIB_NUM_FRAGMENTS_MASK          = 0x3;
constexpr uint32_t CB_COLOR0_ATTRIB_FORCE_DST_ALPHA_1_MASK      = 0x1;
constexpr uint32_t CB_COLOR0_DCC_CONTROL                                        = 0x31E;
constexpr uint32_t CB_COLOR0_DCC_CONTROL_OVERWRITE_COMBINER_DISABLE_MASK        = 0x1;
constexpr uint32_t CB_COLOR0_DCC_CONTROL_KEY_CLEAR_ENABLE_MASK                  = 0x1;
constexpr uint32_t CB_COLOR0_DCC_CONTROL_MAX_UNCOMPRESSED_BLOCK_SIZE_MASK       = 0x3;
constexpr uint32_t CB_COLOR0_DCC_CONTROL_MIN_COMPRESSED_BLOCK_SIZE_MASK         = 0x1;
constexpr uint32_t CB_COLOR0_DCC_CONTROL_MAX_COMPRESSED_BLOCK_SIZE_MASK         = 0x3;
constexpr uint32_t CB_COLOR0_DCC_CONTROL_COLOR_TRANSFORM_MASK                   = 0x3;
constexpr uint32_t CB_COLOR0_DCC_CONTROL_INDEPENDENT_64B_BLOCKS_MASK            = 0x1;
constexpr uint32_t CB_COLOR0_DCC_CONTROL_ENABLE_CONSTANT_ENCODE_REG_WRITE_MASK  = 0x1;
constexpr uint32_t CB_COLOR0_DCC_CONTROL_INDEPENDENT_128B_BLOCKS_MASK           = 0x1;
constexpr uint32_t CB_COLOR0_CMASK          = 0x31F;
constexpr uint32_t CB_COLOR0_FMASK          = 0x321;
constexpr uint32_t CB_COLOR0_CLEAR_WORD0    = 0x323;
constexpr uint32_t CB_COLOR0_CLEAR_WORD1    = 0x324;
constexpr uint32_t CB_COLOR0_DCC_BASE       = 0x325;
constexpr uint32_t CB_COLOR7_BASE           = 0x381;
constexpr uint32_t CB_COLOR7_VIEW           = 0x384;
constexpr uint32_t CB_COLOR7_INFO           = 0x385;
constexpr uint32_t CB_COLOR7_ATTRIB         = 0x386;
constexpr uint32_t CB_COLOR7_DCC_CONTROL    = 0x387;
constexpr uint32_t CB_COLOR7_CMASK          = 0x388;
constexpr uint32_t CB_COLOR7_FMASK          = 0x38A;
constexpr uint32_t CB_COLOR7_CLEAR_WORD0    = 0x38C;
constexpr uint32_t CB_COLOR7_CLEAR_WORD1    = 0x38D;
constexpr uint32_t CB_COLOR7_DCC_BASE       = 0x38E;
constexpr uint32_t CB_COLOR0_BASE_EXT       = 0x390;
constexpr uint32_t CB_COLOR7_BASE_EXT       = 0x397;
constexpr uint32_t CB_COLOR0_CMASK_BASE_EXT = 0x398;
constexpr uint32_t CB_COLOR7_CMASK_BASE_EXT = 0x39F;
constexpr uint32_t CB_COLOR0_FMASK_BASE_EXT = 0x3A0;
constexpr uint32_t CB_COLOR7_FMASK_BASE_EXT = 0x3A7;
constexpr uint32_t CB_COLOR0_DCC_BASE_EXT   = 0x3A8;
constexpr uint32_t CB_COLOR7_DCC_BASE_EXT   = 0x3AF;
constexpr uint32_t CB_COLOR0_ATTRIB2                   = 0x3B0;
constexpr uint32_t CB_COLOR0_ATTRIB2_MIP0_HEIGHT_MASK  = 0x3FFF;
constexpr uint32_t CB_COLOR0_ATTRIB2_MIP0_WIDTH_MASK   = 0x3FFF;
constexpr uint32_t CB_COLOR0_ATTRIB2_MAX_MIP_MASK      = 0xF;
constexpr uint32_t CB_COLOR7_ATTRIB2 = 0x3B7;
constexpr uint32_t CB_COLOR0_ATTRIB3                          = 0x3B8;
constexpr uint32_t CB_COLOR0_ATTRIB3_MIP0_DEPTH_MASK          = 0x1FFF;
constexpr uint32_t CB_COLOR0_ATTRIB3_COLOR_SW_MODE_MASK       = 0x1F;
constexpr uint32_t CB_COLOR0_ATTRIB3_RESOURCE_TYPE_MASK       = 0x3;
constexpr uint32_t CB_COLOR0_ATTRIB3_CMASK_PIPE_ALIGNED_MASK  = 0x1;
constexpr uint32_t CB_COLOR0_ATTRIB3_DCC_PIPE_ALIGNED_MASK    = 0x1;
constexpr uint32_t CB_COLOR7_ATTRIB3 = 0x3BF;
constexpr uint32_t FSR_RECURSIONS0  = 0x800003FC;
constexpr uint32_t FSR_RECURSIONS1  = 0x800003FD;
constexpr uint32_t PA_SC_FSR_ENABLE = 0x800003FE;
constexpr uint32_t CX_NOP           = 0x800003FF;
constexpr uint32_t SPI_SHADER_PGM_RSRC4_PS  = 0x1;
constexpr uint32_t SPI_SHADER_PGM_CHKSUM_PS = 0x6;
constexpr uint32_t SPI_SHADER_PGM_RSRC3_PS  = 0x7;
constexpr uint32_t SPI_SHADER_PGM_LO_PS     = 0x8;
constexpr uint32_t SPI_SHADER_PGM_HI_PS     = 0x9;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_PS                        = 0xA;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_PS_VGPRS_MASK             = 0x3F;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_PS_SGPRS_MASK             = 0xF;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_PS_PRIORITY_MASK          = 0x3;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_PS_FLOAT_MODE_MASK        = 0xFF;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_PS_DX10_CLAMP_MASK        = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_PS_DEBUG_MODE_MASK        = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_PS_IEEE_MODE_MASK         = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_PS_CU_GROUP_DISABLE_MASK  = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_PS_FWD_PROGRESS_MASK      = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_PS_FP16_OVFL_MASK         = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_PS                                = 0xB;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_PS_SCRATCH_EN_MASK                = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_PS_USER_SGPR_MASK                 = 0x1F;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_PS_WAVE_CNT_EN_MASK               = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_PS_EXTRA_LDS_SIZE_MASK            = 0xFF;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_PS_LOAD_INTRAWAVE_COLLISION_MASK  = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_PS_USER_SGPR_MSB_MASK             = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_PS_SHARED_VGPR_CNT_MASK           = 0xF;
constexpr uint32_t SPI_SHADER_USER_DATA_PS_0  = 0xC;
constexpr uint32_t SPI_SHADER_USER_DATA_PS_15 = 0x1B;
constexpr uint32_t SPI_SHADER_USER_ACCUM_PS_0 = 0x32;
constexpr uint32_t SPI_SHADER_PGM_LO_VS       = 0x48;
constexpr uint32_t SPI_SHADER_PGM_HI_VS       = 0x49;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_VS                       = 0x4A;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_VS_VGPRS_MASK            = 0x3F;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_VS_SGPRS_MASK            = 0xF;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_VS_PRIORITY_MASK         = 0x3;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_VS_FLOAT_MODE_MASK       = 0xFF;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_VS_DX10_CLAMP_MASK       = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_VS_IEEE_MODE_MASK        = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_VS_VGPR_COMP_CNT_MASK    = 0x3;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_VS_CU_GROUP_ENABLE_MASK  = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_VS_FWD_PROGRESS_MASK     = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_VS_FP16_OVFL_MASK        = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_VS                       = 0x4B;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_VS_SCRATCH_EN_MASK       = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_VS_USER_SGPR_MASK        = 0x1F;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_VS_OC_LDS_EN_MASK        = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_VS_SO_EN_MASK            = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_VS_USER_SGPR_MSB_MASK    = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_VS_SHARED_VGPR_CNT_MASK  = 0xF;
constexpr uint32_t SPI_SHADER_USER_DATA_VS_0       = 0x4C;
constexpr uint32_t SPI_SHADER_USER_DATA_VS_15      = 0x5B;
constexpr uint32_t SPI_SHADER_PGM_CHKSUM_GS        = 0x80;
constexpr uint32_t SPI_SHADER_PGM_RSRC4_GS         = 0x81;
constexpr uint32_t SPI_SHADER_USER_DATA_ADDR_LO_GS = 0x82;
constexpr uint32_t SPI_SHADER_USER_DATA_ADDR_HI_GS = 0x83;
constexpr uint32_t SPI_SHADER_PGM_RSRC3_GS         = 0x87;
constexpr uint32_t SPI_SHADER_PGM_LO_GS            = 0x88;
constexpr uint32_t SPI_SHADER_PGM_HI_GS            = 0x89;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS                        = 0x8A;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_VGPRS_MASK             = 0x3F;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_SGPRS_MASK             = 0xF;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_PRIORITY_MASK          = 0x3;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_FLOAT_MODE_MASK        = 0xFF;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_DX10_CLAMP_MASK        = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_DEBUG_MODE_MASK        = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_IEEE_MODE_MASK         = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_CU_GROUP_ENABLE_MASK   = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_FWD_PROGRESS_MASK      = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_WGP_MODE_MASK          = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_GS_VGPR_COMP_CNT_MASK  = 0x3;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_GS_FP16_OVFL_MASK         = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_GS                        = 0x8B;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_GS_SCRATCH_EN_MASK        = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_GS_USER_SGPR_MASK         = 0x1F;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_GS_ES_VGPR_COMP_CNT_MASK  = 0x3;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_GS_OC_LDS_EN_MASK         = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_GS_LDS_SIZE_MASK          = 0xFF;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_GS_USER_SGPR_MSB_MASK     = 0x1;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_GS_SHARED_VGPR_CNT_MASK   = 0xF;
constexpr uint32_t SPI_SHADER_USER_DATA_GS_0       = 0x8C;
constexpr uint32_t SPI_SHADER_USER_DATA_GS_15      = 0x9B;
constexpr uint32_t SPI_SHADER_USER_ACCUM_ESGS_0    = 0xB2;
constexpr uint32_t SPI_SHADER_PGM_LO_ES            = 0xC8;
constexpr uint32_t SPI_SHADER_PGM_HI_ES            = 0xC9;
constexpr uint32_t SPI_SHADER_PGM_CHKSUM_HS        = 0x100;
constexpr uint32_t SPI_SHADER_PGM_RSRC4_HS         = 0x101;
constexpr uint32_t SPI_SHADER_USER_DATA_ADDR_LO_HS = 0x102;
constexpr uint32_t SPI_SHADER_USER_DATA_ADDR_HI_HS = 0x103;
constexpr uint32_t SPI_SHADER_PGM_RSRC3_HS         = 0x107;
constexpr uint32_t SPI_SHADER_PGM_LO_HS            = 0x108;
constexpr uint32_t SPI_SHADER_PGM_HI_HS            = 0x109;
constexpr uint32_t SPI_SHADER_PGM_RSRC1_HS         = 0x10A;
constexpr uint32_t SPI_SHADER_PGM_RSRC2_HS         = 0x10B;
constexpr uint32_t SPI_SHADER_USER_DATA_HS_0       = 0x10C;
constexpr uint32_t SPI_SHADER_USER_ACCUM_LSHS_0    = 0x132;
constexpr uint32_t SPI_SHADER_PGM_LO_LS            = 0x148;
constexpr uint32_t SPI_SHADER_PGM_HI_LS            = 0x149;
constexpr uint32_t COMPUTE_START_X                 = 0x204;
constexpr uint32_t COMPUTE_START_Y                 = 0x205;
constexpr uint32_t COMPUTE_START_Z                 = 0x206;
constexpr uint32_t COMPUTE_PGM_LO                  = 0x20C;
constexpr uint32_t COMPUTE_PGM_HI                  = 0x20D;
constexpr uint32_t COMPUTE_PGM_RSRC1             = 0x212;
constexpr uint32_t COMPUTE_PGM_RSRC1_VGPRS_MASK  = 0x3F;
constexpr uint32_t COMPUTE_PGM_RSRC1_SGPRS_MASK  = 0xF;
constexpr uint32_t COMPUTE_PGM_RSRC1_BULKY_MASK  = 0x1;
constexpr uint32_t COMPUTE_PGM_RSRC2                      = 0x213;
constexpr uint32_t COMPUTE_PGM_RSRC2_SCRATCH_EN_MASK      = 0x1;
constexpr uint32_t COMPUTE_PGM_RSRC2_USER_SGPR_MASK       = 0x1F;
constexpr uint32_t COMPUTE_PGM_RSRC2_TGID_X_EN_MASK       = 0x1;
constexpr uint32_t COMPUTE_PGM_RSRC2_TGID_Y_EN_MASK       = 0x1;
constexpr uint32_t COMPUTE_PGM_RSRC2_TGID_Z_EN_MASK       = 0x1;
constexpr uint32_t COMPUTE_PGM_RSRC2_TG_SIZE_EN_MASK      = 0x1;
constexpr uint32_t COMPUTE_PGM_RSRC2_TIDIG_COMP_CNT_MASK  = 0x3;
constexpr uint32_t COMPUTE_PGM_RSRC2_LDS_SIZE_MASK        = 0x1FF;
constexpr uint32_t COMPUTE_RESOURCE_LIMITS    = 0x215;
constexpr uint32_t COMPUTE_DESTINATION_EN_SE0 = 0x216;
constexpr uint32_t COMPUTE_DESTINATION_EN_SE1 = 0x217;
constexpr uint32_t COMPUTE_TMPRING_SIZE       = 0x218;
constexpr uint32_t COMPUTE_DESTINATION_EN_SE2 = 0x219;
constexpr uint32_t COMPUTE_DESTINATION_EN_SE3 = 0x21A;
constexpr uint32_t COMPUTE_USER_ACCUM_0       = 0x224;
constexpr uint32_t COMPUTE_PGM_RSRC3          = 0x228;
constexpr uint32_t COMPUTE_SHADER_CHKSUM      = 0x22A;
constexpr uint32_t COMPUTE_USER_DATA_0        = 0x240;
constexpr uint32_t COMPUTE_USER_DATA_15       = 0x24F;
constexpr uint32_t COMPUTE_DISPATCH_TUNNEL    = 0x27D;
constexpr uint32_t SH_NOP = 0x800002FF;
constexpr uint32_t VGT_PRIMITIVE_TYPE                 = 0x242;
constexpr uint32_t VGT_PRIMITIVE_TYPE_PRIM_TYPE_MASK  = 0x3F;
constexpr uint32_t VGT_OBJECT_ID             = 0x248;
constexpr uint32_t GE_INDX_OFFSET            = 0x24A;
constexpr uint32_t GE_MULTI_PRIM_IB_RESET_EN = 0x24B;
constexpr uint32_t VGT_HS_OFFCHIP_PARAM      = 0x24F;
constexpr uint32_t VGT_TF_MEMORY_BASE        = 0x250;
constexpr uint32_t GE_CNTL                     = 0x25B;
constexpr uint32_t GE_CNTL_PRIM_GRP_SIZE_MASK  = 0x1FF;
constexpr uint32_t GE_CNTL_VERT_GRP_SIZE_MASK  = 0x1FF;
constexpr uint32_t GE_USER_VGPR1  = 0x25C;
constexpr uint32_t GE_USER_VGPR2  = 0x25D;
constexpr uint32_t GE_USER_VGPR3  = 0x25E;
constexpr uint32_t GE_STEREO_CNTL = 0x25F;
constexpr uint32_t GE_USER_VGPR_EN                     = 0x262;
constexpr uint32_t GE_USER_VGPR_EN_EN_USER_VGPR1_MASK  = 0x1;
constexpr uint32_t GE_USER_VGPR_EN_EN_USER_VGPR2_MASK  = 0x1;
constexpr uint32_t GE_USER_VGPR_EN_EN_USER_VGPR3_MASK  = 0x1;
constexpr uint32_t TA_CS_BC_BASE_ADDR       = 0x380;
constexpr uint32_t TA_CS_BC_BASE_ADDR_HI    = 0x381;
constexpr uint32_t TEXTURE_GRADIENT_FACTORS = 0x382;
constexpr uint32_t GDS_OA_CNTL              = 0x41D;
constexpr uint32_t GDS_OA_COUNTER           = 0x41E;
constexpr uint32_t GDS_OA_ADDRESS           = 0x41F;
constexpr uint32_t FSR_EXTEND_SUBPIXEL_ROUNDING = 0x80003FF4;
constexpr uint32_t FSR_ALPHA_VALUE0             = 0x80003FF5;
constexpr uint32_t FSR_ALPHA_VALUE1             = 0x80003FF6;
constexpr uint32_t FSR_CONTROL_POINT0           = 0x80003FF7;
constexpr uint32_t FSR_CONTROL_POINT1           = 0x80003FF8;
constexpr uint32_t FSR_CONTROL_POINT2           = 0x80003FF9;
constexpr uint32_t FSR_CONTROL_POINT3           = 0x80003FFA;
constexpr uint32_t FSR_WINDOW0                  = 0x80003FFB;
constexpr uint32_t FSR_WINDOW1                  = 0x80003FFC;
constexpr uint32_t TEXTURE_GRADIENT_CONTROL     = 0x80003FFD;
constexpr uint32_t MEMORY_MAPPING_MASK          = 0x80003FFE;
constexpr uint32_t UC_NOP                       = 0x80003FFF;
} // namespace Pm4

struct ShaderRegister { uint32_t offset; uint32_t value; };
struct RegisterDefaultInfo { uint32_t type; ShaderRegister reg[16]; };
struct RegisterDefaults {
    ShaderRegister** tbl0       = nullptr;
    ShaderRegister** tbl1       = nullptr;
    ShaderRegister** tbl2       = nullptr;
    ShaderRegister** tbl3       = nullptr;
    uint64_t         unknown[2] = {};
    uint32_t*        types      = nullptr;
    uint32_t         count      = 0;
};
static_assert(offsetof(RegisterDefaults, count) == 0x38, "AGC RegisterDefaults layout must match SDK");

static RegisterDefaultInfo g_cx_reg_info1[] = {
    /* 0 */ {0xE24F806D, {{Pm4::CB_COLOR_CONTROL, 0x00cc0010}}},
    /* 1 */ {0xF6C28182, {{Pm4::CB_DCC_CONTROL, 0x00000000}}},
    /* 2 */ {0x6F6E55A5, {{Pm4::CB_RMI_GL2_CACHE_CONTROL, 0x00000000}}},
    /* 3 */ {0x0BC65DA4, {{Pm4::CB_SHADER_MASK, 0x00000000}}},
    /* 4 */ {0x9E5AD592, {{Pm4::CB_TARGET_MASK, 0x00000000}}},
    /* 5 */ {0xBB513B98, {{Pm4::DB_ALPHA_TO_MASK, 0x0000aa00}}},
    /* 6 */ {0xAB64B23B, {{Pm4::DB_COUNT_CONTROL, 0x00000000}}},
    /* 7 */ {0x53C39964, {{Pm4::DB_DEPTH_CONTROL, 0x00000000}}},
    /* 8 */ {0x01396B11, {{Pm4::DB_EQAA, 0x00000000}}},
    /* 9 */ {0x7D42019A, {{Pm4::DB_RENDER_CONTROL, 0x00000000}}},
    /* 10 */ {0x3548F523, {{Pm4::PS_SHADER_SAMPLE_EXCLUSION_MASK, 0x00000000}}},
    /* 11 */ {0xF43AD28A, {{Pm4::DB_RMI_L2_CACHE_CONTROL, 0x00000000}}},
    /* 12 */ {0x6DE4C312, {{Pm4::DB_SHADER_CONTROL, 0x00000000}}},
    /* 13 */ {0x00A77AE0, {{Pm4::DB_SRESULTS_COMPARE_STATE0, 0x00000000}}},
    /* 14 */ {0x00A779B7, {{Pm4::DB_SRESULTS_COMPARE_STATE1, 0x00000000}}},
    /* 15 */ {0x5100100C, {{Pm4::DB_STENCILREFMASK, 0x00000000}}},
    /* 16 */ {0x59958BBA, {{Pm4::DB_STENCILREFMASK_BF, 0x00000000}}},
    /* 17 */ {0x0C06F17C, {{Pm4::DB_STENCIL_CONTROL, 0x00000000}}},
    /* 18 */ {0x6F104B72, {{Pm4::GE_MAX_OUTPUT_PER_SUBGROUP, 0x00000000}}},
    /* 19 */ {0x25C70D9C, {{Pm4::PA_CL_CLIP_CNTL, 0x00000000}}},
    /* 20 */ {0x3881201E, {{Pm4::PA_CL_OBJPRIM_ID_CNTL, 0x00000000}}},
    /* 21 */ {0x09AFDDAF, {{Pm4::PA_CL_VTE_CNTL, 0x0000043f}}},
    /* 22 */ {0x367D63CF, {{Pm4::PA_SC_AA_CONFIG, 0x00000000}}},
    /* 23 */ {0x43707DB8, {{Pm4::PA_SC_CLIPRECT_RULE, 0x0000ffff}}},
    /* 24 */ {0xF6AE26BA, {{Pm4::PA_SC_CONSERVATIVE_RASTERIZATION_CNTL, 0x00000000}}},
    /* 25 */ {0x1B917652, {{Pm4::PA_SC_FSR_ENABLE, 0x00000000}}},
    /* 26 */ {0x94B1E4F7, {{Pm4::PA_SC_HORIZ_GRID, 0x00000000}}},
    /* 27 */ {0xE3661B6C, {{Pm4::PA_SC_LEFT_VERT_GRID, 0x00000000}}},
    /* 28 */ {0x1EB8D73A, {{Pm4::PA_SC_MODE_CNTL_0, 0x00000002}}},
    /* 29 */ {0x15051FA3, {{Pm4::PA_SC_MODE_CNTL_1, 0x00000000}}},
    /* 30 */ {0x9C51A7F1, {{Pm4::PA_SC_RIGHT_VERT_GRID, 0x00000000}}},
    /* 31 */ {0xA20EFC70, {{Pm4::PA_SC_WINDOW_OFFSET, 0x00000000}}},
    /* 32 */ {0x0EC09F6E, {{Pm4::PA_STATE_STEREO_X, 0x00000000}}},
    /* 33 */ {0x34A7D6D3, {{Pm4::PA_STEREO_CNTL, 0x00000000}}},
    /* 34 */ {0xCE831B94, {{Pm4::PA_SU_HARDWARE_SCREEN_OFFSET, 0x00000000}}},
    /* 35 */ {0x5CC72A74, {{Pm4::PA_SU_LINE_CNTL, 0x00000008}}},
    /* 36 */ {0x3B77713C, {{Pm4::PA_SU_POINT_MINMAX, 0xffff0000}}},
    /* 37 */ {0x40F64410, {{Pm4::PA_SU_POINT_SIZE, 0x00080008}}},
    /* 38 */ {0x69441268, {{Pm4::PA_SU_POLY_OFFSET_CLAMP, 0x00000000}}},
    /* 39 */ {0x2E418B83, {{Pm4::PA_SU_POLY_OFFSET_DB_FMT_CNTL, 0x000001e9}}},
    /* 40 */ {0xA00D0C8D, {{Pm4::PA_SU_SC_MODE_CNTL, 0x00000240}}},
    /* 41 */ {0xB1289FB3, {{Pm4::PA_SU_SMALL_PRIM_FILTER_CNTL, 0x00000001}}},
    /* 42 */ {0x144832FB, {{Pm4::PA_SU_VTX_CNTL, 0x0000002d}}},
    /* 43 */ {0x9890D9FA, {{Pm4::SPI_TMPRING_SIZE, 0x00000000}}},
    /* 44 */ {0x9016FAF1, {{Pm4::VGT_DRAW_PAYLOAD_CNTL, 0x00000000}}},
    /* 45 */ {0x4B73CE27, {{Pm4::VGT_GS_MAX_VERT_OUT, 0x00000400}}},
    /* 46 */ {0x5F5A3E7B, {{Pm4::VGT_GS_OUT_PRIM_TYPE, 0x00000002}}},
    /* 47 */ {0xD4AF3A51, {{Pm4::VGT_LS_HS_CONFIG, 0x00000000}}},
    /* 48 */ {0x6CF4F543, {{Pm4::VGT_PRIMITIVEID_RESET, 0xffffffff}}},
    /* 49 */ {0x5FB86CCB, {{Pm4::VGT_PRIMITIVEID_EN, 0x00000000}}},
    /* 50 */ {0xEDEFA188, {{Pm4::VGT_REUSE_OFF, 0x00000000}}},
    /* 51 */ {0xD0DE9EE6, {{Pm4::VGT_SHADER_STAGES_EN, 0x00000000}}},
    /* 52 */ {0xC5831803, {{Pm4::VGT_TESS_DISTRIBUTION, 0x88101000}}},
    /* 53 */ {0x8E6DE84B, {{Pm4::VGT_TF_PARAM, 0x00000000}}},
    /* 54 */
    {0xD0771662,
     {
         {Pm4::PA_SC_CENTROID_PRIORITY_0, 0x00000000},
         {Pm4::PA_SC_CENTROID_PRIORITY_1, 0x00000000},
     }},
    /* 55 */ {0x569F7444, {{Pm4::PA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0, 0x00000000}}},
    /* 56 */
    {0x5C6637CD,
     {
         {Pm4::PA_SC_AA_MASK_X0Y0_X1Y0, 0xffffffff},
         {Pm4::PA_SC_AA_MASK_X0Y1_X1Y1, 0xffffffff},
     }},
    /* 57 */
    {0xCAE3E690,
     {
         {Pm4::PA_SC_BINNER_CNTL_0, 0x00000002},
         {Pm4::PA_SC_BINNER_CNTL_1, 0x03ff0080},
     }},
    /* 58 */
    {0x43FBD769,
     {
         {Pm4::CB_BLEND_RED, 0x00000000},
         {Pm4::CB_BLEND_BLUE, 0x00000000},
         {Pm4::CB_BLEND_GREEN, 0x00000000},
         {Pm4::CB_BLEND_ALPHA, 0x00000000},
     }},
    /* 59 */ {0xEF550356, {{Pm4::CB_BLEND0_CONTROL, 0x20010001}}},
    /* 60 */
    {0x8F52E279,
     {
         {Pm4::TA_BC_BASE_ADDR, 0x00000000},
         {Pm4::TA_BC_BASE_ADDR_HI, 0x00000000},
     }},
    /* 61 */
    {0x1F2D8149,
     {
         {Pm4::PA_SC_CLIPRECT_0_TL, 0x00000000},
         {Pm4::PA_SC_CLIPRECT_0_BR, 0x20002000},
     }},
    /* 62 */ {0x853D0614, {{Pm4::CX_NOP, 0x00000000}}},
    /* 63 */
    {0x4413C6F9,
     {
         {Pm4::DB_DEPTH_BOUNDS_MIN, 0x00000000},
         {Pm4::DB_DEPTH_BOUNDS_MAX, 0x00000000},
     }},
    /* 64 */
    {0x67096014,
     {
         {Pm4::DB_Z_INFO, 0x80000000},
         {Pm4::DB_STENCIL_INFO, 0x20000000},
         {Pm4::DB_Z_READ_BASE, 0x00000000},
         {Pm4::DB_STENCIL_READ_BASE, 0x00000000},
         {Pm4::DB_Z_WRITE_BASE, 0x00000000},
         {Pm4::DB_STENCIL_WRITE_BASE, 0x00000000},
         {Pm4::DB_Z_READ_BASE_HI, 0x00000000},
         {Pm4::DB_STENCIL_READ_BASE_HI, 0x00000000},
         {Pm4::DB_Z_WRITE_BASE_HI, 0x00000000},
         {Pm4::DB_STENCIL_WRITE_BASE_HI, 0x00000000},
         {Pm4::DB_HTILE_DATA_BASE_HI, 0x00000000},
         {Pm4::DB_DEPTH_VIEW, 0x00000000},
         {Pm4::DB_HTILE_DATA_BASE, 0x00000000},
         {Pm4::DB_DEPTH_SIZE_XY, 0x00000000},
         {Pm4::DB_DEPTH_CLEAR, 0x00000000},
         {Pm4::DB_STENCIL_CLEAR, 0x00000000},
     }},
    /* 65 */
    {0x88F5E915,
     {
         {Pm4::PA_SC_FOV_WINDOW_LR, 0xff00ff00},
         {Pm4::PA_SC_FOV_WINDOW_TB, 0x00000000},
     }},
    /* 66 */
    {0x033F1EFF,
     {
         {Pm4::FSR_RECURSIONS0, 0x00000000},
         {Pm4::FSR_RECURSIONS1, 0x00000000},
     }},
    /* 67 */
    {0x918106BB,
     {
         {Pm4::PA_SC_GENERIC_SCISSOR_TL, 0x80000000},
         {Pm4::PA_SC_GENERIC_SCISSOR_BR, 0x40004000},
     }},
    /* 68 */
    {0x95F0E7AC,
     {
         {Pm4::PA_CL_GB_VERT_CLIP_ADJ, 0x4e7e0000},
         {Pm4::PA_CL_GB_VERT_DISC_ADJ, 0x4e7e0000},
         {Pm4::PA_CL_GB_HORZ_CLIP_ADJ, 0x4e7e0000},
         {Pm4::PA_CL_GB_HORZ_DISC_ADJ, 0x4e7e0000},
     }},
    /* 69 */
    {0xB48CBAB2,
     {
         {Pm4::PA_SU_POLY_OFFSET_BACK_SCALE, 0x00000000},
         {Pm4::PA_SU_POLY_OFFSET_BACK_OFFSET, 0x00000000},
     }},
    /* 70 */
    {0x05BB3BC6,
     {
         {Pm4::PA_SU_POLY_OFFSET_FRONT_SCALE, 0x00000000},
         {Pm4::PA_SU_POLY_OFFSET_FRONT_OFFSET, 0x00000000},
     }},
    /* 71 */
    {0x94FABA07,
     {
         {Pm4::DB_RENDER_OVERRIDE, 0x00000000},
         {Pm4::DB_RENDER_OVERRIDE2, 0x00000000},
     }},
    /* 72 */
    {0x38E92C91,
     {
         {Pm4::CB_COLOR0_BASE, 0x00000000},
         {Pm4::CB_COLOR0_VIEW, 0x00000000},
         {Pm4::CB_COLOR0_INFO, 0x00000000},
         {Pm4::CB_COLOR0_ATTRIB, 0x00000000},
         {Pm4::CB_COLOR0_DCC_CONTROL, 0x00000048},
         {Pm4::CB_COLOR0_CMASK, 0x00000000},
         {Pm4::CB_COLOR0_FMASK, 0x00000000},
         {Pm4::CB_COLOR0_CLEAR_WORD0, 0x00000000},
         {Pm4::CB_COLOR0_CLEAR_WORD1, 0x00000000},
         {Pm4::CB_COLOR0_DCC_BASE, 0x00000000},
         {Pm4::CB_COLOR0_BASE_EXT, 0x00000000},
         {Pm4::CB_COLOR0_CMASK_BASE_EXT, 0x00000000},
         {Pm4::CB_COLOR0_FMASK_BASE_EXT, 0x00000000},
         {Pm4::CB_COLOR0_DCC_BASE_EXT, 0x00000000},
         {Pm4::CB_COLOR0_ATTRIB2, 0x00000000},
         {Pm4::CB_COLOR0_ATTRIB3, 0x0006c000},
     }},
    /* 73 */
    {0x0B177B43,
     {
         {Pm4::PA_SC_SCREEN_SCISSOR_TL, 0x00000000},
         {Pm4::PA_SC_SCREEN_SCISSOR_BR, 0x40004000},
     }},
    /* 74 */ {0x48531062, {{Pm4::SPI_PS_INPUT_CNTL_0, 0x00000000}}},
    /* 75 */
    {0xAAA964B9,
     {
         {Pm4::PA_CL_UCP_0_X, 0x00000000},
         {Pm4::PA_CL_UCP_0_Y, 0x00000000},
         {Pm4::PA_CL_UCP_0_Z, 0x00000000},
         {Pm4::PA_CL_UCP_0_W, 0x00000000},
     }},
    /* 76 */
    {0x7690AF6F,
     {
         {Pm4::PA_CL_VPORT_XSCALE, 0x4e7e0000},
         {Pm4::PA_CL_VPORT_YSCALE, 0x4e7e0000},
         {Pm4::PA_CL_VPORT_ZSCALE, 0x4e7e0000},
         {Pm4::PA_CL_VPORT_XOFFSET, 0x00000000},
         {Pm4::PA_CL_VPORT_YOFFSET, 0x00000000},
         {Pm4::PA_CL_VPORT_ZOFFSET, 0x00000000},
         {Pm4::PA_SC_VPORT_SCISSOR_0_TL, 0x80000000},
         {Pm4::PA_SC_VPORT_SCISSOR_0_BR, 0x40004000},
         {Pm4::PA_SC_VPORT_ZMIN_0, 0x00000000},
         {Pm4::PA_SC_VPORT_ZMAX_0, 0x00000000},
     }},
    /* 77 */
    {0x078D7060,
     {
         {Pm4::PA_SC_WINDOW_SCISSOR_TL, 0x80000000},
         {Pm4::PA_SC_WINDOW_SCISSOR_BR, 0x40004000},
     }},

};

static RegisterDefaultInfo g_sh_reg_info1[] = {
    /* 0 */ {0x5D6E3EC7, {{Pm4::COMPUTE_PGM_RSRC1, 0x00000000}}},
    /* 1 */ {0x57E7079A, {{Pm4::COMPUTE_PGM_RSRC2, 0x00000000}}},
    /* 2 */ {0x7467FAFD, {{Pm4::COMPUTE_PGM_RSRC3, 0x00000000}}},
    /* 3 */ {0x9E826B50, {{Pm4::COMPUTE_RESOURCE_LIMITS, 0x00000000}}},
    /* 4 */ {0xDC484F18, {{Pm4::COMPUTE_TMPRING_SIZE, 0x00000000}}},
    /* 5 */ {0x5DA8BCA3, {{Pm4::SPI_SHADER_PGM_RSRC1_GS, 0x00000000}}},
    /* 6 */ {0x5CA726D8, {{Pm4::SPI_SHADER_PGM_RSRC1_HS, 0x00000000}}},
    /* 7 */ {0x5DD28360, {{Pm4::SPI_SHADER_PGM_RSRC1_PS, 0x00000000}}},
    /* 8 */ {0x57EFA0BE, {{Pm4::SPI_SHADER_PGM_RSRC2_GS, 0x00000000}}},
    /* 9 */ {0x502363D5, {{Pm4::SPI_SHADER_PGM_RSRC2_HS, 0x00000000}}},
    /* 10 */ {0x506D14BD, {{Pm4::SPI_SHADER_PGM_RSRC2_PS, 0x00000000}}},
    /* 11 */ {0xB2609506, {{Pm4::COMPUTE_USER_ACCUM_0, 0x00000000}}},
    /* 12 */
    {0x9E5CFB8A,
     {
         {Pm4::SPI_SHADER_PGM_RSRC3_HS, 0x00000000},
         {Pm4::SPI_SHADER_PGM_RSRC3_GS, 0x00000000},
         {Pm4::SPI_SHADER_PGM_RSRC3_PS, 0x00000000},
     }},
    /* 13 */
    {0xC918DF3E,
     {
         {Pm4::COMPUTE_PGM_LO, 0x00000000},
         {Pm4::COMPUTE_PGM_HI, 0x00000000},
     }},
    /* 14 */
    {0xC9751C9C,
     {
         {Pm4::SPI_SHADER_PGM_LO_ES, 0x00000000},
         {Pm4::SPI_SHADER_PGM_HI_ES, 0x00000000},
     }},
    /* 15 */
    {0xC97EF77A,
     {
         {Pm4::SPI_SHADER_PGM_LO_GS, 0x00000000},
         {Pm4::SPI_SHADER_PGM_HI_GS, 0x00000000},
     }},
    /* 16 */
    {0xC927C6B9,
     {
         {Pm4::SPI_SHADER_PGM_LO_HS, 0x00000000},
         {Pm4::SPI_SHADER_PGM_HI_HS, 0x00000000},
     }},
    /* 17 */
    {0xC92A1EC5,
     {
         {Pm4::SPI_SHADER_PGM_LO_LS, 0x00000000},
         {Pm4::SPI_SHADER_PGM_HI_LS, 0x00000000},
     }},
    /* 18 */
    {0xC9E01B31,
     {
         {Pm4::SPI_SHADER_PGM_LO_PS, 0x00000000},
         {Pm4::SPI_SHADER_PGM_HI_PS, 0x00000000},
     }},
    /* 19 */ {0x50685F29, {{Pm4::SH_NOP, 0x00000000}}},
    /* 20 */ {0xB26219CA, {{Pm4::SPI_SHADER_USER_ACCUM_ESGS_0, 0x00000000}}},
    /* 21 */ {0xB25B6CF9, {{Pm4::SPI_SHADER_USER_ACCUM_LSHS_0, 0x00000000}}},
    /* 22 */ {0xB2F86101, {{Pm4::SPI_SHADER_USER_ACCUM_PS_0, 0x00000000}}},
    /* 23 */
    {0x07E3B155,
     {
         {Pm4::SPI_SHADER_USER_DATA_ADDR_LO_GS, 0x00000000},
         {Pm4::SPI_SHADER_USER_DATA_ADDR_HI_GS, 0x00000000},
     }},
    /* 24 */
    {0x07E383C6,
     {
         {Pm4::SPI_SHADER_USER_DATA_ADDR_LO_HS, 0x00000000},
         {Pm4::SPI_SHADER_USER_DATA_ADDR_HI_HS, 0x00000000},
     }},
    /* 25 */ {0xBDA98653, {{Pm4::COMPUTE_USER_DATA_0, 0x00000000}}},
    /* 26 */ {0xBDBD1D0F, {{Pm4::SPI_SHADER_USER_DATA_GS_0, 0x00000000}}},
    /* 27 */ {0xBD946FD4, {{Pm4::SPI_SHADER_USER_DATA_HS_0, 0x00000000}}},
    /* 28 */ {0xBDF02A4C, {{Pm4::SPI_SHADER_USER_DATA_PS_0, 0x00000000}}},
};

static RegisterDefaultInfo g_uc_reg_info1[] = {
    /* 0 */ {0x19E93E85, {{Pm4::GDS_OA_ADDRESS, 0x00000000}}},
    /* 1 */ {0x3B5C2AF3, {{Pm4::GDS_OA_CNTL, 0x00000000}}},
    /* 2 */ {0x47974A35, {{Pm4::GDS_OA_COUNTER, 0x00000000}}},
    /* 3 */ {0x105971C2, {{Pm4::GE_CNTL, 0x00000000}}},
    /* 4 */ {0x7D137765, {{Pm4::GE_INDX_OFFSET, 0x00000000}}},
    /* 5 */ {0xD187FEBC, {{Pm4::GE_MULTI_PRIM_IB_RESET_EN, 0x00000000}}},
    /* 6 */ {0x12F854AC, {{Pm4::GE_STEREO_CNTL, 0x00000000}}},
    /* 7 */ {0x40D49AD1, {{Pm4::GE_USER_VGPR_EN, 0x00000000}}},
    /* 8 */ {0x8C0923DA, {{Pm4::FSR_EXTEND_SUBPIXEL_ROUNDING, 0x00000000}}},
    /* 9 */ {0xBB8DF494, {{Pm4::TEXTURE_GRADIENT_CONTROL, 0x00000000}}},
    /* 10 */ {0xF6D8A76E, {{Pm4::TEXTURE_GRADIENT_FACTORS, 0x40000040}}},
    /* 11 */ {0x7620F1E9, {{Pm4::VGT_OBJECT_ID, 0x00000000}}},
    /* 12 */ {0x9EBFAB10, {{Pm4::VGT_PRIMITIVE_TYPE, 0x00000000}}},
    /* 13 */
    {0x98A09D0E,
     {
         {Pm4::TA_CS_BC_BASE_ADDR, 0x00000000},
         {Pm4::TA_CS_BC_BASE_ADDR_HI, 0x00000000},
     }},
    /* 14 */
    {0x195D37D2,
     {
         {Pm4::FSR_ALPHA_VALUE0, 0x00000000},
         {Pm4::FSR_ALPHA_VALUE1, 0x00000000},
     }},
    /* 15 */
    {0xF9EC4F85,
     {
         {Pm4::FSR_CONTROL_POINT0, 0x00000000},
         {Pm4::FSR_CONTROL_POINT1, 0x00000000},
         {Pm4::FSR_CONTROL_POINT2, 0x00000000},
         {Pm4::FSR_CONTROL_POINT3, 0x00000000},
     }},
    /* 16 */
    {0x4626B750,
     {
         {Pm4::FSR_WINDOW0, 0x00000000},
         {Pm4::FSR_WINDOW1, 0x00000000},
     }},
    /* 17 */ {0x4CC673A0, {{Pm4::MEMORY_MAPPING_MASK, 0x00000000}}},
    /* 18 */ {0xDE5B3431, {{Pm4::UC_NOP, 0x00000000}}},
    /* 19 */ {0x036AC8A6, {{Pm4::GE_USER_VGPR1, 0x00000000}}}};

static RegisterDefaultInfo g_cx_reg_info2[] = {
    /* 0 */ {0x8FB4EDB5, {{Pm4::DB_DFSM_CONTROL, 0x00000000}}},
    /* 1 */ {0xB994AD29, {{Pm4::DB_HTILE_SURFACE, 0x00000000}}},
    /* 2 */ {0xD427322F, {{Pm4::PA_SC_NGG_MODE_CNTL, 0x00000000}}},
    /* 3 */ {0xF58FEA31, {{Pm4::SPI_INTERP_CONTROL_0, 0x00000000}}},
};

static RegisterDefaultInfo g_sh_reg_info2[] = {
    /* 0 */ {0x6AC156EF, {{Pm4::COMPUTE_DESTINATION_EN_SE0, 0x00000000}}},
    /* 1 */ {0x6AC15610, {{Pm4::COMPUTE_DESTINATION_EN_SE1, 0x00000000}}},
    /* 2 */ {0x6AC15009, {{Pm4::COMPUTE_DESTINATION_EN_SE2, 0x00000000}}},
    /* 3 */ {0x6AC153BA, {{Pm4::COMPUTE_DESTINATION_EN_SE3, 0x00000000}}},
    /* 4 */ {0xBE7DCD73, {{Pm4::COMPUTE_DISPATCH_TUNNEL, 0x00000000}}},
    /* 5 */ {0x0C4B1438, {{Pm4::COMPUTE_SHADER_CHKSUM, 0x00000000}}},
    /* 6 */ {0xDB00D71A, {{Pm4::COMPUTE_START_X, 0x00000000}}},
    /* 7 */ {0xDB00D249, {{Pm4::COMPUTE_START_Y, 0x00000000}}},
    /* 8 */ {0xDB00EC60, {{Pm4::COMPUTE_START_Z, 0x00000000}}},
    /* 9 */ {0x0C4D6FE4, {{Pm4::SPI_SHADER_PGM_CHKSUM_GS, 0x00000000}}},
    /* 10 */ {0x0C4A80EF, {{Pm4::SPI_SHADER_PGM_CHKSUM_HS, 0x00000000}}},
    /* 11 */ {0x0DD283E7, {{Pm4::SPI_SHADER_PGM_CHKSUM_PS, 0x00000000}}},
    /* 12 */ {0xC620E68C, {{Pm4::SPI_SHADER_PGM_RSRC4_GS, 0x00000000}}},
    /* 13 */ {0xC67EFACF, {{Pm4::SPI_SHADER_PGM_RSRC4_HS, 0x00000000}}},
    /* 14 */ {0xD9E6D9F7, {{Pm4::SPI_SHADER_PGM_RSRC4_PS, 0x00000000}}},
};

static RegisterDefaultInfo g_uc_reg_info2[] = {
    /* 0 */ {0x31F34B9F, {{Pm4::VGT_HS_OFFCHIP_PARAM, 0x00000000}}},
    /* 1 */ {0xAC0F9E76, {{Pm4::UC_NOP, 0x00000000}}},
    /* 2 */ {0x929FD95D, {{Pm4::VGT_TF_MEMORY_BASE, 0x00000000}}},
};

#define KYTY_ID(id, tbl)   ((id)*4 + (tbl))
#define KYTY_INDEX_CX1(id) g_cx_reg_info1[id].type, KYTY_ID(id, 0), 0
#define KYTY_INDEX_SH1(id) g_sh_reg_info1[id].type, KYTY_ID(id, 1), 0
#define KYTY_INDEX_UC1(id) g_uc_reg_info1[id].type, KYTY_ID(id, 2), 0
#define KYTY_INDEX_CX2(id) g_cx_reg_info2[id].type, KYTY_ID(id, 0), 0
#define KYTY_INDEX_SH2(id) g_sh_reg_info2[id].type, KYTY_ID(id, 1), 0
#define KYTY_INDEX_UC2(id) g_uc_reg_info2[id].type, KYTY_ID(id, 2), 0
#define KYTY_REG_CX1(id)   &g_cx_reg_info1[id].reg[0]
#define KYTY_REG_SH1(id)   &g_sh_reg_info1[id].reg[0]
#define KYTY_REG_UC1(id)   &g_uc_reg_info1[id].reg[0]
#define KYTY_REG_CX2(id)   &g_cx_reg_info2[id].reg[0]
#define KYTY_REG_SH2(id)   &g_sh_reg_info2[id].reg[0]
#define KYTY_REG_UC2(id)   &g_uc_reg_info2[id].reg[0]

static ShaderRegister* g_tbl_cx1[] = {
    KYTY_REG_CX1(0),  KYTY_REG_CX1(1),  KYTY_REG_CX1(2),  KYTY_REG_CX1(3),  KYTY_REG_CX1(4),  KYTY_REG_CX1(5),  KYTY_REG_CX1(6),
    KYTY_REG_CX1(7),  KYTY_REG_CX1(8),  KYTY_REG_CX1(9),  KYTY_REG_CX1(10), KYTY_REG_CX1(11), KYTY_REG_CX1(12), KYTY_REG_CX1(13),
    KYTY_REG_CX1(14), KYTY_REG_CX1(15), KYTY_REG_CX1(16), KYTY_REG_CX1(17), KYTY_REG_CX1(18), KYTY_REG_CX1(19), KYTY_REG_CX1(20),
    KYTY_REG_CX1(21), KYTY_REG_CX1(22), KYTY_REG_CX1(23), KYTY_REG_CX1(24), KYTY_REG_CX1(25), KYTY_REG_CX1(26), KYTY_REG_CX1(27),
    KYTY_REG_CX1(28), KYTY_REG_CX1(29), KYTY_REG_CX1(30), KYTY_REG_CX1(31), KYTY_REG_CX1(32), KYTY_REG_CX1(33), KYTY_REG_CX1(34),
    KYTY_REG_CX1(35), KYTY_REG_CX1(36), KYTY_REG_CX1(37), KYTY_REG_CX1(38), KYTY_REG_CX1(39), KYTY_REG_CX1(40), KYTY_REG_CX1(41),
    KYTY_REG_CX1(42), KYTY_REG_CX1(43), KYTY_REG_CX1(44), KYTY_REG_CX1(45), KYTY_REG_CX1(46), KYTY_REG_CX1(47), KYTY_REG_CX1(48),
    KYTY_REG_CX1(49), KYTY_REG_CX1(50), KYTY_REG_CX1(51), KYTY_REG_CX1(52), KYTY_REG_CX1(53), KYTY_REG_CX1(54), KYTY_REG_CX1(55),
    KYTY_REG_CX1(56), KYTY_REG_CX1(57), KYTY_REG_CX1(58), KYTY_REG_CX1(59), KYTY_REG_CX1(60), KYTY_REG_CX1(61), KYTY_REG_CX1(62),
    KYTY_REG_CX1(63), KYTY_REG_CX1(64), KYTY_REG_CX1(65), KYTY_REG_CX1(66), KYTY_REG_CX1(67), KYTY_REG_CX1(68), KYTY_REG_CX1(69),
    KYTY_REG_CX1(70), KYTY_REG_CX1(71), KYTY_REG_CX1(72), KYTY_REG_CX1(73), KYTY_REG_CX1(74), KYTY_REG_CX1(75), KYTY_REG_CX1(76),
    KYTY_REG_CX1(77)};

static ShaderRegister* g_tbl_sh1[]    = {KYTY_REG_SH1(0),  KYTY_REG_SH1(1),  KYTY_REG_SH1(2),  KYTY_REG_SH1(3),  KYTY_REG_SH1(4),
                                         KYTY_REG_SH1(5),  KYTY_REG_SH1(6),  KYTY_REG_SH1(7),  KYTY_REG_SH1(8),  KYTY_REG_SH1(9),
                                         KYTY_REG_SH1(10), KYTY_REG_SH1(11), KYTY_REG_SH1(12), KYTY_REG_SH1(13), KYTY_REG_SH1(14),
                                         KYTY_REG_SH1(15), KYTY_REG_SH1(16), KYTY_REG_SH1(17), KYTY_REG_SH1(18), KYTY_REG_SH1(19),
                                         KYTY_REG_SH1(20), KYTY_REG_SH1(21), KYTY_REG_SH1(22), KYTY_REG_SH1(23), KYTY_REG_SH1(24),
                                         KYTY_REG_SH1(25), KYTY_REG_SH1(26), KYTY_REG_SH1(27), KYTY_REG_SH1(28)};
static ShaderRegister* g_tbl_uc1[]    = {KYTY_REG_UC1(0),  KYTY_REG_UC1(1),  KYTY_REG_UC1(2),  KYTY_REG_UC1(3),  KYTY_REG_UC1(4),
                                         KYTY_REG_UC1(5),  KYTY_REG_UC1(6),  KYTY_REG_UC1(7),  KYTY_REG_UC1(8),  KYTY_REG_UC1(9),
                                         KYTY_REG_UC1(10), KYTY_REG_UC1(11), KYTY_REG_UC1(12), KYTY_REG_UC1(13), KYTY_REG_UC1(14),
                                         KYTY_REG_UC1(15), KYTY_REG_UC1(16), KYTY_REG_UC1(17), KYTY_REG_UC1(18), KYTY_REG_UC1(19)};
static uint32_t        g_tbl_index1[] = {
           KYTY_INDEX_CX1(0),  KYTY_INDEX_CX1(1),  KYTY_INDEX_CX1(2),  KYTY_INDEX_CX1(3),  KYTY_INDEX_CX1(4),  KYTY_INDEX_CX1(5),
           KYTY_INDEX_CX1(6),  KYTY_INDEX_CX1(7),  KYTY_INDEX_CX1(8),  KYTY_INDEX_CX1(9),  KYTY_INDEX_CX1(10), KYTY_INDEX_CX1(11),
           KYTY_INDEX_CX1(12), KYTY_INDEX_CX1(13), KYTY_INDEX_CX1(14), KYTY_INDEX_CX1(15), KYTY_INDEX_CX1(16), KYTY_INDEX_CX1(17),
           KYTY_INDEX_CX1(18), KYTY_INDEX_CX1(19), KYTY_INDEX_CX1(20), KYTY_INDEX_CX1(21), KYTY_INDEX_CX1(22), KYTY_INDEX_CX1(23),
           KYTY_INDEX_CX1(24), KYTY_INDEX_CX1(25), KYTY_INDEX_CX1(26), KYTY_INDEX_CX1(27), KYTY_INDEX_CX1(28), KYTY_INDEX_CX1(29),
           KYTY_INDEX_CX1(30), KYTY_INDEX_CX1(31), KYTY_INDEX_CX1(32), KYTY_INDEX_CX1(33), KYTY_INDEX_CX1(34), KYTY_INDEX_CX1(35),
           KYTY_INDEX_CX1(36), KYTY_INDEX_CX1(37), KYTY_INDEX_CX1(38), KYTY_INDEX_CX1(39), KYTY_INDEX_CX1(40), KYTY_INDEX_CX1(41),
           KYTY_INDEX_CX1(42), KYTY_INDEX_CX1(43), KYTY_INDEX_CX1(44), KYTY_INDEX_CX1(45), KYTY_INDEX_CX1(46), KYTY_INDEX_CX1(47),
           KYTY_INDEX_CX1(48), KYTY_INDEX_CX1(49), KYTY_INDEX_CX1(50), KYTY_INDEX_CX1(51), KYTY_INDEX_CX1(52), KYTY_INDEX_CX1(53),
           KYTY_INDEX_CX1(54), KYTY_INDEX_CX1(55), KYTY_INDEX_CX1(56), KYTY_INDEX_CX1(57), KYTY_INDEX_CX1(58), KYTY_INDEX_CX1(59),
           KYTY_INDEX_CX1(60), KYTY_INDEX_CX1(61), KYTY_INDEX_CX1(62), KYTY_INDEX_CX1(63), KYTY_INDEX_CX1(64), KYTY_INDEX_CX1(65),
           KYTY_INDEX_CX1(66), KYTY_INDEX_CX1(67), KYTY_INDEX_CX1(68), KYTY_INDEX_CX1(69), KYTY_INDEX_CX1(70), KYTY_INDEX_CX1(71),
           KYTY_INDEX_CX1(72), KYTY_INDEX_CX1(73), KYTY_INDEX_CX1(74), KYTY_INDEX_CX1(75), KYTY_INDEX_CX1(76), KYTY_INDEX_CX1(77),
           KYTY_INDEX_SH1(0),  KYTY_INDEX_SH1(1),  KYTY_INDEX_SH1(2),  KYTY_INDEX_SH1(3),  KYTY_INDEX_SH1(4),  KYTY_INDEX_SH1(5),
           KYTY_INDEX_SH1(6),  KYTY_INDEX_SH1(7),  KYTY_INDEX_SH1(8),  KYTY_INDEX_SH1(9),  KYTY_INDEX_SH1(10), KYTY_INDEX_SH1(11),
           KYTY_INDEX_SH1(12), KYTY_INDEX_SH1(13), KYTY_INDEX_SH1(14), KYTY_INDEX_SH1(15), KYTY_INDEX_SH1(16), KYTY_INDEX_SH1(17),
           KYTY_INDEX_SH1(18), KYTY_INDEX_SH1(19), KYTY_INDEX_SH1(20), KYTY_INDEX_SH1(21), KYTY_INDEX_SH1(22), KYTY_INDEX_SH1(23),
           KYTY_INDEX_SH1(24), KYTY_INDEX_SH1(25), KYTY_INDEX_SH1(26), KYTY_INDEX_SH1(27), KYTY_INDEX_SH1(28), KYTY_INDEX_UC1(0),
           KYTY_INDEX_UC1(1),  KYTY_INDEX_UC1(2),  KYTY_INDEX_UC1(3),  KYTY_INDEX_UC1(4),  KYTY_INDEX_UC1(5),  KYTY_INDEX_UC1(6),
           KYTY_INDEX_UC1(7),  KYTY_INDEX_UC1(8),  KYTY_INDEX_UC1(9),  KYTY_INDEX_UC1(10), KYTY_INDEX_UC1(11), KYTY_INDEX_UC1(12),
           KYTY_INDEX_UC1(13), KYTY_INDEX_UC1(14), KYTY_INDEX_UC1(15), KYTY_INDEX_UC1(16), KYTY_INDEX_UC1(17), KYTY_INDEX_UC1(18),
           KYTY_INDEX_UC1(19)};

static ShaderRegister* g_tbl_cx2[]    = {KYTY_REG_CX2(0), KYTY_REG_CX2(1), KYTY_REG_CX2(2), KYTY_REG_CX2(3)};
static ShaderRegister* g_tbl_sh2[]    = {KYTY_REG_SH2(0),  KYTY_REG_SH2(1),  KYTY_REG_SH2(2),  KYTY_REG_SH2(3),  KYTY_REG_SH2(4),
                                         KYTY_REG_SH2(5),  KYTY_REG_SH2(6),  KYTY_REG_SH2(7),  KYTY_REG_SH2(8),  KYTY_REG_SH2(9),
                                         KYTY_REG_SH2(10), KYTY_REG_SH2(11), KYTY_REG_SH2(12), KYTY_REG_SH2(13), KYTY_REG_SH2(14)};
static ShaderRegister* g_tbl_uc2[]    = {KYTY_REG_UC2(0), KYTY_REG_UC2(1), KYTY_REG_UC2(2)};
static uint32_t        g_tbl_index2[] = {KYTY_INDEX_CX2(0),  KYTY_INDEX_CX2(1),  KYTY_INDEX_CX2(2),  KYTY_INDEX_CX2(3),  KYTY_INDEX_SH2(0),
                                         KYTY_INDEX_SH2(1),  KYTY_INDEX_SH2(2),  KYTY_INDEX_SH2(3),  KYTY_INDEX_SH2(4),  KYTY_INDEX_SH2(5),
                                         KYTY_INDEX_SH2(6),  KYTY_INDEX_SH2(7),  KYTY_INDEX_SH2(8),  KYTY_INDEX_SH2(9),  KYTY_INDEX_SH2(10),
                                         KYTY_INDEX_SH2(11), KYTY_INDEX_SH2(12), KYTY_INDEX_SH2(13), KYTY_INDEX_SH2(14), KYTY_INDEX_UC2(0),
                                         KYTY_INDEX_UC2(1),  KYTY_INDEX_UC2(2)};

static RegisterDefaults g_reg_defaults1 = { // @suppress("Invalid arguments")
    g_tbl_cx1, g_tbl_sh1, g_tbl_uc1, nullptr, {0, 0}, g_tbl_index1, sizeof(g_tbl_index1) / 12};
static RegisterDefaults g_reg_defaults2 = { // @suppress("Invalid arguments")
    g_tbl_cx2, g_tbl_sh2, g_tbl_uc2, nullptr, {0, 0}, g_tbl_index2, sizeof(g_tbl_index2) / 12};


} } // namespace prosper::agc

// Returned by the sceAgcGetRegisterDefaults2 HLE thunk (ver is the SDK version, always 8).
extern "C" void* prosper_agc_reg_defaults(unsigned int /*ver*/) {
    return &prosper::agc::g_reg_defaults1;
}
extern "C" void* prosper_agc_reg_defaults_internal(unsigned int /*ver*/) {
    return &prosper::agc::g_reg_defaults2;
}
