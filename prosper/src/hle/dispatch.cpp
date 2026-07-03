#include "dispatch.hpp"
#include <unordered_map>

namespace prosper {

namespace {
    struct Reg { HleFn fn; std::string name; };
    std::unordered_map<std::string, Reg>& registry() {
        static std::unordered_map<std::string, Reg> r; return r;
    }
    const Module* g_mod = nullptr;
    NidDb*        g_db  = nullptr;

    // unimplemented-call bookkeeping
    std::vector<uint32_t>            g_order;      // first-seen import indices
    std::unordered_map<uint32_t, uint64_t> g_count; // index -> call count
}

void Hle::register_fn(const std::string& nid, HleFn fn, const char* name) {
    registry()[nid] = { fn, name ? name : "" };
}
HleFn Hle::lookup(const std::string& nid) {
    auto it = registry().find(nid);
    return it == registry().end() ? nullptr : it->second.fn;
}
const char* Hle::name_of(const std::string& nid) {
    auto it = registry().find(nid);
    return it == registry().end() ? "" : it->second.name.c_str();
}

void dispatch_init(const Module* m, NidDb* db) { g_mod = m; g_db = db; }
void reset_call_log() { g_order.clear(); g_count.clear(); }
const std::vector<uint32_t>& call_order() { return g_order; }

extern "C" uint64_t prosper_on_unimpl(uint64_t import_index) {
    uint32_t idx = (uint32_t)import_index;
    uint64_t c = ++g_count[idx];
    if (c == 1) {
        g_order.push_back(idx);
        if (g_mod && idx < g_mod->imports.size()) {
            const auto& im = g_mod->imports[idx];
            std::string nm = g_db ? g_db->resolve(im.nid) : std::string();
            fprintf(stderr, "[prosper] unimplemented: %s::%s%s%s%s  -> returning 0\n",
                    im.lib_name.c_str(), im.nid.c_str(),
                    nm.empty() ? "" : " [", nm.c_str(), nm.empty() ? "" : "]");
        }
    }
    return 0; // let the guest proceed so we can discover the next call
}

void dump_call_log(FILE* f) {
    fprintf(f, "\n=== unimplemented Sony calls (first-seen order) ===\n");
    for (uint32_t idx : g_order) {
        const char* lib = "?"; std::string nid;
        std::string nm;
        if (g_mod && idx < g_mod->imports.size()) {
            lib = g_mod->imports[idx].lib_name.c_str();
            nid = g_mod->imports[idx].nid;
            if (g_db) nm = g_db->resolve(nid);
        }
        fprintf(f, "  %4u x  %-24s %s%s%s%s\n", (unsigned)g_count[idx], lib,
                nid.c_str(), nm.empty() ? "" : "  [", nm.c_str(), nm.empty() ? "" : "]");
    }
    fprintf(f, "  (%zu distinct unimplemented functions)\n", g_order.size());
}

} // namespace prosper
