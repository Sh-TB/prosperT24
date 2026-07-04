// command_processor.cpp — see command_processor.hpp.
#include "command_processor.hpp"

namespace prosper::gpu {

void GpuState::apply(const Pm4Command& c) {
    using K = Pm4Command::Kind;
    switch (c.kind) {
        case K::SetRegsIndirect: {
            if (c.regs_vaddr == 0 || c.num_regs == 0 || c.num_regs > kMaxRegsPerPacket) return;
            auto* regs = reinterpret_cast<const ShaderReg*>(static_cast<uintptr_t>(c.regs_vaddr));
            auto& file = (c.reg_class == RegClass::Cx) ? cx
                       : (c.reg_class == RegClass::Sh) ? sh : uc;
            for (uint32_t i = 0; i < c.num_regs; i++) file[regs[i].offset] = regs[i].value;
            break;
        }
        case K::SetShRegDirect:
            sh[c.sh_reg_offset] = c.sh_reg_value;
            break;
        case K::SetIndexType:
            index_type = c.index_size;
            break;
        case K::DrawIndexAuto:
            draws.push_back({ c.index_count });
            break;
        default:
            break;   // fences / events / flips / unknown: no register-state effect (handled later)
    }
}

size_t run_command_buffer(const uint32_t* buf, size_t dwords, GpuState& st) {
    std::vector<Pm4Command> ops;
    decode_pm4(buf, dwords, ops);
    for (const auto& c : ops) st.apply(c);
    return ops.size();
}

} // namespace prosper::gpu
