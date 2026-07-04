// rdna2_to_spirv.cpp — see rdna2_to_spirv.hpp. Internal SpirvCompute builder + the VALU translator.
#include "rdna2_to_spirv.hpp"
#include "rdna2_decode.hpp"
#include <cstring>
#include <unordered_map>

namespace prosper::gpu {
namespace {

enum : uint32_t {
    Op_ExtInstImport=11, Op_ExtInst=12, Op_MemoryModel=14, Op_EntryPoint=15, Op_ExecutionMode=16,
    Op_Capability=17, Op_TypeVoid=19, Op_TypeInt=21, Op_TypeFloat=22, Op_TypeVector=23,
    Op_TypeRuntimeArray=29, Op_TypeStruct=30, Op_TypePointer=32, Op_TypeFunction=33,
    Op_Constant=43, Op_Function=54, Op_FunctionEnd=56, Op_Variable=59,
    Op_Load=61, Op_Store=62, Op_AccessChain=65, Op_Decorate=71, Op_MemberDecorate=72,
    Op_CompositeExtract=81, Op_IAdd=128, Op_FAdd=129, Op_FSub=131, Op_IMul=132, Op_FMul=133,
    Op_FDiv=136, Op_Label=248, Op_Return=253,
};
// GLSL.std.450 extended-instruction numbers.
enum : uint32_t { Glsl_Floor=8, Glsl_Fract=10, Glsl_Sqrt=31, Glsl_InverseSqrt=32, Glsl_FMin=37, Glsl_FMax=40 };
enum : uint32_t {
    Cap_Shader=1, Addr_Logical=0, Mem_GLSL450=1, Exec_GLCompute=5, EM_LocalSize=17,
    SC_Input=1, SC_StorageBuffer=12, FC_None=0,
    Dec_Block=2, Dec_ArrayStride=6, Dec_BuiltIn=11, Dec_Binding=33, Dec_DescriptorSet=34, Dec_Offset=35,
    BI_GlobalInvocationId=28,
};

uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

// A compute-shader SPIR-V builder specialized for "load N floats -> compute over SSA floats ->
// store 1 float", with helpers the VALU translator drives.
struct SpirvCompute {
    std::vector<uint32_t> caps, extimp, mem, entry, exec, deco, types, code;
    std::unordered_map<uint32_t, uint32_t> fconst_cache, uconst_cache;
    uint32_t next_id = 1;
    uint32_t stride = 1;
    // fixed ids (set in begin()):
    uint32_t t_void=0, t_fn=0, t_f32=0, t_u32=0, t_v3u=0, t_ptr_sb_f32=0;
    uint32_t v_gid=0, v_in=0, v_out=0, gidx=0, f_main=0, glsl=0;

    uint32_t id() { return next_id++; }
    static void put(std::vector<uint32_t>& s, uint32_t op, std::initializer_list<uint32_t> o) {
        s.push_back(((uint32_t)(o.size() + 1) << 16) | op); for (uint32_t x : o) s.push_back(x);
    }
    static void putv(std::vector<uint32_t>& s, uint32_t op, const std::vector<uint32_t>& o) {
        s.push_back(((uint32_t)(o.size() + 1) << 16) | op); s.insert(s.end(), o.begin(), o.end());
    }
    void pstr(std::vector<uint32_t>& v, const char* s) {
        size_t len = std::strlen(s);
        for (size_t i = 0; i <= len; i += 4) { uint32_t w = 0;
            for (size_t k = 0; k < 4; k++) { size_t j = i + k; if (j <= len) w |= (uint32_t)(uint8_t)s[j] << (8*k); }
            v.push_back(w); }
    }
    uint32_t fconst(float f) {
        uint32_t b = fbits(f); auto it = fconst_cache.find(b); if (it != fconst_cache.end()) return it->second;
        uint32_t c = id(); put(types, Op_Constant, {t_f32, c, b}); fconst_cache[b] = c; return c;
    }
    uint32_t uconst(uint32_t v) {
        auto it = uconst_cache.find(v); if (it != uconst_cache.end()) return it->second;
        uint32_t c = id(); put(types, Op_Constant, {t_u32, c, v}); uconst_cache[v] = c; return c;
    }
    uint32_t binop(uint32_t op, uint32_t a, uint32_t b) { uint32_t r = id(); put(code, op, {t_f32, r, a, b}); return r; }
    // GLSL.std.450 extended instructions (float result).
    uint32_t ext1(uint32_t inst, uint32_t a) { uint32_t r = id(); putv(code, Op_ExtInst, {t_f32, r, glsl, inst, a}); return r; }
    uint32_t ext2(uint32_t inst, uint32_t a, uint32_t b) { uint32_t r = id(); putv(code, Op_ExtInst, {t_f32, r, glsl, inst, a, b}); return r; }

    // buffer element pointer: base[ gid.x*stride + k ]
    uint32_t elem_ptr(uint32_t bufvar, uint32_t k) {
        uint32_t idx = gidx;
        if (stride != 1) { uint32_t m = id(); put(code, Op_IMul, {t_u32, m, gidx, uconst(stride)}); idx = m; }
        if (k != 0) { uint32_t a = id(); put(code, Op_IAdd, {t_u32, a, idx, uconst(k)}); idx = a; }
        uint32_t p = id(); putv(code, Op_AccessChain, {t_ptr_sb_f32, p, bufvar, uconst(0), idx}); return p;
    }
    uint32_t load_input(uint32_t k)  { uint32_t p = elem_ptr(v_in, k); uint32_t r = id(); put(code, Op_Load, {t_f32, r, p}); return r; }
    // Output is one float per invocation: b[gid.x] (stride 1), independent of the input stride.
    void     store_output(uint32_t val) {
        uint32_t p = id(); putv(code, Op_AccessChain, {t_ptr_sb_f32, p, v_out, uconst(0), gidx});
        put(code, Op_Store, {p, val});
    }

    void begin(uint32_t input_stride) {
        stride = input_stride;
        t_void = id(); t_fn = id(); t_f32 = id(); t_u32 = id(); t_v3u = id();
        uint32_t t_ptr_in_v3u = id(); v_gid = id();
        uint32_t t_rta = id(), t_struct = id(), t_ptr_sb_struct = id();
        v_in = id(); v_out = id(); t_ptr_sb_f32 = id();
        f_main = id(); uint32_t lbl = id(); glsl = id();

        put(caps, Op_Capability, {Cap_Shader});
        { std::vector<uint32_t> o{glsl}; pstr(o, "GLSL.std.450"); putv(extimp, Op_ExtInstImport, o); }
        put(mem, Op_MemoryModel, {Addr_Logical, Mem_GLSL450});
        { std::vector<uint32_t> o{Exec_GLCompute, f_main}; pstr(o, "main"); o.push_back(v_gid); putv(entry, Op_EntryPoint, o); }
        put(exec, Op_ExecutionMode, {f_main, EM_LocalSize, 64, 1, 1});
        put(deco, Op_Decorate, {v_gid, Dec_BuiltIn, BI_GlobalInvocationId});
        put(deco, Op_Decorate, {t_rta, Dec_ArrayStride, 4});
        put(deco, Op_MemberDecorate, {t_struct, 0, Dec_Offset, 0});
        put(deco, Op_Decorate, {t_struct, Dec_Block});
        put(deco, Op_Decorate, {v_in, Dec_DescriptorSet, 0});  put(deco, Op_Decorate, {v_in, Dec_Binding, 0});
        put(deco, Op_Decorate, {v_out, Dec_DescriptorSet, 0}); put(deco, Op_Decorate, {v_out, Dec_Binding, 1});
        put(types, Op_TypeVoid, {t_void});
        put(types, Op_TypeFunction, {t_fn, t_void});
        put(types, Op_TypeFloat, {t_f32, 32});
        put(types, Op_TypeInt, {t_u32, 32, 0});
        put(types, Op_TypeVector, {t_v3u, t_u32, 3});
        put(types, Op_TypePointer, {t_ptr_in_v3u, SC_Input, t_v3u});
        put(types, Op_Variable, {t_ptr_in_v3u, v_gid, SC_Input});
        put(types, Op_TypeRuntimeArray, {t_rta, t_f32});
        put(types, Op_TypeStruct, {t_struct, t_rta});
        put(types, Op_TypePointer, {t_ptr_sb_struct, SC_StorageBuffer, t_struct});
        put(types, Op_Variable, {t_ptr_sb_struct, v_in, SC_StorageBuffer});
        put(types, Op_Variable, {t_ptr_sb_struct, v_out, SC_StorageBuffer});
        put(types, Op_TypePointer, {t_ptr_sb_f32, SC_StorageBuffer, t_f32});
        put(code, Op_Function, {t_void, f_main, FC_None, t_fn});
        put(code, Op_Label, {lbl});
        uint32_t ld = id(); put(code, Op_Load, {t_v3u, ld, v_gid});
        gidx = id(); put(code, Op_CompositeExtract, {t_u32, gidx, ld, 0});
    }
    std::vector<uint32_t> finish() {
        put(code, Op_Return, {}); put(code, Op_FunctionEnd, {});
        std::vector<uint32_t> m{0x07230203u, 0x00010300u, 0u, next_id, 0u};
        for (auto* s : {&caps, &extimp, &mem, &entry, &exec, &deco, &types, &code}) m.insert(m.end(), s->begin(), s->end());
        return m;
    }
};

}  // namespace

std::vector<uint32_t> recompile_valu(const uint32_t* code, size_t dwords,
                                     uint32_t num_inputs, uint32_t out_vgpr) {
    std::vector<Rdna2Inst> ins;
    rdna2_walk(code, dwords, ins);

    SpirvCompute b;
    b.begin(num_inputs ? num_inputs : 1);
    std::unordered_map<int, uint32_t> vreg;                 // VGPR -> current SSA float id
    for (uint32_t k = 0; k < num_inputs; k++) vreg[(int)k] = b.load_input(k);

    auto val = [&](const Rdna2Inst& in, const Operand& o) -> uint32_t {
        switch (o.kind) {
            case OperandKind::VGPR: { auto it = vreg.find(o.value); return it == vreg.end() ? b.fconst(0.f) : it->second; }
            case OperandKind::InlineInt:   return b.fconst((float)o.value);
            case OperandKind::InlineFloat: return b.fconst(inline_float_value((uint32_t)o.value));
            case OperandKind::Literal: { uint32_t bits = in.literal; float f; std::memcpy(&f, &bits, 4); return b.fconst(f); }
            default: return b.fconst(0.f);
        }
    };

    for (const auto& in : ins) {
        if (in.is_end) break;
        switch (in.fmt) {
            case Rdna2Format::VOP1: {
                uint32_t a = val(in, in.src[0]); uint32_t& d = vreg[in.dst.value];
                switch (in.opcode) {
                    case 0x01: d = a; break;                                   // v_mov_b32
                    case 0x20: d = b.ext1(Glsl_Fract, a); break;               // v_fract_f32
                    case 0x24: d = b.ext1(Glsl_Floor, a); break;               // v_floor_f32
                    case 0x2A: d = b.binop(Op_FDiv, b.fconst(1.0f), a); break; // v_rcp_f32
                    case 0x2E: d = b.ext1(Glsl_InverseSqrt, a); break;         // v_rsq_f32
                    case 0x33: d = b.ext1(Glsl_Sqrt, a); break;                // v_sqrt_f32
                    default: return {};
                }
                break;
            }
            case Rdna2Format::VOP2: {
                uint32_t a = val(in, in.src[0]), c = val(in, in.src[1]);
                if      (in.opcode == 0x03) vreg[in.dst.value] = b.binop(Op_FAdd, a, c);   // v_add_f32
                else if (in.opcode == 0x04) vreg[in.dst.value] = b.binop(Op_FSub, a, c);   // v_sub_f32
                else if (in.opcode == 0x08) vreg[in.dst.value] = b.binop(Op_FMul, a, c);   // v_mul_f32
                else if (in.opcode == 0x0F) vreg[in.dst.value] = b.ext2(Glsl_FMin, a, c);  // v_min_f32
                else if (in.opcode == 0x10) vreg[in.dst.value] = b.ext2(Glsl_FMax, a, c);  // v_max_f32
                else return {};
                break;
            }
            case Rdna2Format::VOP3:
                if (in.opcode == 0x14B) {  // v_fma_f32 (gfx10 VOP3 op 0x14B) = src0*src1 + src2
                    uint32_t m = b.binop(Op_FMul, val(in, in.src[0]), val(in, in.src[1]));
                    vreg[in.dst.value] = b.binop(Op_FAdd, m, val(in, in.src[2]));
                } else return {};
                break;
            default: return {};   // scalar / memory / unsupported: not handled at this stage
        }
    }

    auto it = vreg.find((int)out_vgpr);
    b.store_output(it == vreg.end() ? b.fconst(0.f) : it->second);
    return b.finish();
}

} // namespace prosper::gpu
