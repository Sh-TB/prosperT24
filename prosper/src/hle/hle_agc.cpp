// hle_agc.cpp — libSceAgc "Gen5" graphics-driver HLE: the Draw Command Buffer (Dcb) frontend.
//
// The AGC Dcb functions ARE the driver: each appends a PM4 packet into the game's command buffer and
// RETURNS the allocated dword pointer, which the game (and the indirect-patch helpers) then use. Our
// old stubs returned 0 -> the patch helpers wrote through a null pointer (the eboot+0x3b5ea6 fault).
//
// Ported/adapted from Kyty (../Kyty, MIT-licensed; source/emulator/src/Graphics/Graphics.cpp +
// Pm4.h) — the Dcb struct layout, PM4 encoding, and per-function packet contents are Kyty's, which
// reverse-engineered the real libSceAgc ABI. A later CommandProcessor will decode this PM4 stream to
// Vulkan (see docs/AGC_IMPL_PLAN.md). Registered by raw NID (AGC lib is undocumented).
#include "dispatch.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace prosper {

#define HLE(name) static uint64_t name(uint64_t a0, uint64_t a1, uint64_t a2, \
                                       uint64_t a3, uint64_t a4, uint64_t a5)

namespace {

// --- PM4 encoding (Kyty Pm4.h) ---------------------------------------------------------------
constexpr uint32_t IT_NOP = 0x10, IT_INDEX_TYPE = 0x2A, IT_EVENT_WRITE = 0x46, IT_SET_SH_REG = 0x76;
// Custom sub-opcodes carried inside IT_NOP:
constexpr uint32_t R_DRAW_INDEX_AUTO = 0x04, R_DRAW_RESET = 0x05, R_WAIT_FLIP_DONE = 0x06,
                   R_SH_REGS_INDIRECT = 0x11, R_CX_REGS_INDIRECT = 0x12, R_UC_REGS_INDIRECT = 0x13,
                   R_ACQUIRE_MEM = 0x14, R_WRITE_DATA = 0x15, R_WAIT_MEM_64 = 0x16, R_FLIP = 0x17,
                   R_RELEASE_MEM = 0x18;
constexpr uint32_t R_NUM = 0x40;
inline uint32_t PM4(uint32_t len, uint32_t op, uint32_t r) {
    return 0xC0000000u | (((len - 2u) & 0x3fffu) << 16u) | ((op & 0xffu) << 8u) | ((r & (R_NUM - 1u)) << 2u);
}

// --- The Draw Command Buffer (the game's own struct; we cast the guest pointer). Kyty layout
// (Graphics.cpp:777): a dword ring [bottom,top) with a write cursor and a "buffer full" callback. ---
struct AgcDcb {
    uint32_t* bottom;       // +0x00 buffer base
    uint32_t* top;          // +0x08 buffer end
    uint32_t* cursor_up;    // +0x10 current write ptr
    uint32_t* cursor_down;  // +0x18 reserve limit
    bool (*callback)(AgcDcb*, uint32_t, void*);  // +0x20 buffer-full callback (guest fn)
    void*     user_data;    // +0x28
    uint32_t  reserved_dw;  // +0x30

    uint32_t available_dw() const {
        auto avail = (uint32_t)(cursor_down - cursor_up);
        return avail > reserved_dw ? avail - reserved_dw : 0;
    }
    uint32_t* allocate_dw(uint32_t n) {
        if (n == 0) return nullptr;
        if (n > available_dw()) {
            if (!callback || !callback(this, n + reserved_dw, user_data)) return nullptr;
            if (available_dw() < n) return nullptr;
        }
        uint32_t* r = cursor_up;
        cursor_up += n;
        return r;
    }
};

// Helper: allocate `n` dwords and set the header; returns cmd (or nullptr).
inline uint32_t* begin_packet(uint64_t buf, uint32_t n, uint32_t op, uint32_t r, uint32_t** out) {
    auto* dcb = (AgcDcb*)(uintptr_t)buf;
    if (!dcb) { *out = nullptr; return nullptr; }
    uint32_t* cmd = dcb->allocate_dw(n);
    if (cmd) cmd[0] = PM4(n, op, r);
    *out = cmd;
    return cmd;
}

}  // namespace

// --- Dcb append functions (a0 = Dcb*, return = the allocated packet pointer). ------------------
HLE(agc_dcb_reset_queue) {  // (buf, op=0x3ff, state=0)
    uint32_t* cmd; if (!begin_packet(a0, 2, IT_NOP, R_DRAW_RESET, &cmd)) return 0;
    cmd[1] = 0; return (uint64_t)(uintptr_t)cmd;
}
HLE(agc_dcb_wait_safe_for_rendering) {  // (buf, video_out_handle, display_buffer_index)
    uint32_t* cmd; if (!begin_packet(a0, 7, IT_NOP, R_WAIT_FLIP_DONE, &cmd)) return 0;
    cmd[1] = (uint32_t)a1; cmd[2] = (uint32_t)a2; cmd[3] = cmd[4] = cmd[5] = cmd[6] = 0;
    return (uint64_t)(uintptr_t)cmd;
}
HLE(agc_dcb_set_sh_register_direct) {  // (buf, ShaderRegister reg)  reg packed in a1: offset|value<<32
    uint32_t* cmd; if (!begin_packet(a0, 3, IT_SET_SH_REG, 0, &cmd)) return 0;
    cmd[1] = (uint32_t)(a1 & 0xffffffffu); cmd[2] = (uint32_t)(a1 >> 32u);
    return (uint64_t)(uintptr_t)cmd;
}
static uint64_t set_regs_indirect(uint64_t buf, uint64_t regs, uint64_t num_regs, uint32_t r) {
    uint32_t* cmd; if (!begin_packet(buf, 4, IT_NOP, r, &cmd)) return 0;
    cmd[1] = (uint32_t)num_regs; cmd[2] = (uint32_t)(regs & 0xffffffffu); cmd[3] = (uint32_t)(regs >> 32u);
    return (uint64_t)(uintptr_t)cmd;
}
HLE(agc_dcb_set_cx_regs_indirect) { return set_regs_indirect(a0, a1, a2, R_CX_REGS_INDIRECT); }
HLE(agc_dcb_set_sh_regs_indirect) { return set_regs_indirect(a0, a1, a2, R_SH_REGS_INDIRECT); }
HLE(agc_dcb_set_uc_regs_indirect) { return set_regs_indirect(a0, a1, a2, R_UC_REGS_INDIRECT); }
HLE(agc_dcb_set_index_size) {  // (buf, index_size, cache_policy)
    uint32_t* cmd; if (!begin_packet(a0, 2, IT_INDEX_TYPE, 0, &cmd)) return 0;
    cmd[1] = (uint32_t)a1; return (uint64_t)(uintptr_t)cmd;
}
HLE(agc_dcb_draw_index_auto) {  // (buf, index_count, modifier)
    uint32_t* cmd; if (!begin_packet(a0, 7, IT_NOP, R_DRAW_INDEX_AUTO, &cmd)) return 0;
    cmd[1] = (uint32_t)a1; cmd[2] = 0; return (uint64_t)(uintptr_t)cmd;
}
HLE(agc_dcb_event_write) {  // (buf, event_type, address)
    uint32_t* cmd; if (!begin_packet(a0, 2, IT_EVENT_WRITE, 0, &cmd)) return 0;
    cmd[1] = (uint32_t)(a1 & 0xffu); return (uint64_t)(uintptr_t)cmd;
}
HLE(agc_dcb_acquire_mem) {  // (buf, engine, cb_db_op, gcr_cntl, ...) — 8 dw
    uint32_t* cmd; if (!begin_packet(a0, 8, IT_NOP, R_ACQUIRE_MEM, &cmd)) return 0;
    cmd[1] = (uint32_t)a2; cmd[2] = cmd[3] = cmd[4] = cmd[5] = cmd[6] = 0; cmd[7] = (uint32_t)a3;
    return (uint64_t)(uintptr_t)cmd;
}
HLE(agc_cb_release_mem) {  // GraphicsCbReleaseMem (buf, ...) — 7 dw
    uint32_t* cmd; if (!begin_packet(a0, 7, IT_NOP, R_RELEASE_MEM, &cmd)) return 0;
    cmd[1] = cmd[2] = cmd[3] = cmd[4] = cmd[5] = cmd[6] = 0; return (uint64_t)(uintptr_t)cmd;
}
HLE(agc_dcb_write_data) {  // (buf, dst, cache_policy, address_or_offset, ...) — 4 dw (num_dwords via stack, approx 0)
    uint32_t* cmd; if (!begin_packet(a0, 4, IT_NOP, R_WRITE_DATA, &cmd)) return 0;
    cmd[1] = (uint32_t)a1; cmd[2] = (uint32_t)(a3 & 0xffffffffu); cmd[3] = (uint32_t)(a3 >> 32u);
    return (uint64_t)(uintptr_t)cmd;
}
HLE(agc_dcb_wait_reg_mem) {  // (buf, ...) — 9 dw
    uint32_t* cmd; if (!begin_packet(a0, 9, IT_NOP, R_WAIT_MEM_64, &cmd)) return 0;
    for (int i = 1; i < 9; i++) cmd[i] = 0; return (uint64_t)(uintptr_t)cmd;
}
HLE(agc_dcb_set_flip) {  // (buf, video_out_handle, display_buffer_index, flip_mode, flip_arg) — 6 dw
    uint32_t* cmd; if (!begin_packet(a0, 6, IT_NOP, R_FLIP, &cmd)) return 0;
    cmd[1] = (uint32_t)a1; cmd[2] = (uint32_t)a2; cmd[3] = (uint32_t)a3;
    cmd[4] = (uint32_t)(a4 & 0xffffffffu); cmd[5] = (uint32_t)(a4 >> 32u);
    return (uint64_t)(uintptr_t)cmd;
}

// --- Indirect-register patch helpers: modify a packet returned by a Set*RegsIndirect call.
// cmd = a0 (that returned pointer). Old stub returned 0 for Set*RegsIndirect -> these wrote to null. -
HLE(agc_patch_set_address) {  // (cmd, regs): cmd[2..3] = regs vaddr
    auto* cmd = (uint32_t*)(uintptr_t)a0; if (!cmd) return 0;
    cmd[2] = (uint32_t)(a1 & 0xffffffffu); cmd[3] = (uint32_t)(a1 >> 32u); return 0;
}
HLE(agc_patch_add_registers) {  // (cmd, num_regs): cmd[1] += num_regs
    auto* cmd = (uint32_t*)(uintptr_t)a0; if (!cmd) return 0;
    cmd[1] += (uint32_t)a1; return 0;
}

void register_agc_hle() {
    #define RN(nid, fn) Hle::register_fn(nid, (HleFn)(fn), nid)
    RN("TRO721eVt4g", agc_dcb_reset_queue);
    RN("MWiElSNE8j8", agc_dcb_wait_safe_for_rendering);
    RN("ZvwO9euwYzc", agc_dcb_set_cx_regs_indirect);
    RN("-HOOCn0JY48", agc_dcb_set_sh_regs_indirect);
    RN("hvUfkUIQcOE", agc_dcb_set_uc_regs_indirect);
    RN("GIIW2J37e70", agc_dcb_set_index_size);
    RN("Yw0jKSqop+E", agc_dcb_draw_index_auto);
    RN("aJf+j5yntiU", agc_dcb_event_write);
    RN("57labkp+rSQ", agc_dcb_acquire_mem);
    RN("wr23dPKyWc0", agc_cb_release_mem);
    RN("i1jyy49AjXU", agc_dcb_write_data);
    RN("VmW0Tdpy420", agc_dcb_wait_reg_mem);
    // Indirect-register patch helpers (SetAddress patches cmd[2..3]; AddRegisters patches cmd[1]).
    RN("vcmNN+AAXnY", agc_patch_set_address);   RN("d-6uF9sZDIU", agc_patch_add_registers);   // Cx
    RN("6lNcCp+fxi4", agc_patch_set_address);   RN("vRoArM9zaIk", agc_patch_add_registers);   // Uc
    #undef RN
}

}  // namespace prosper
