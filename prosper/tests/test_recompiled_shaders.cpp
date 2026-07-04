// test_recompiled_shaders — render a frame whose BOTH shaders are recompiled from RDNA2.
// Vertex shader: a fullscreen triangle computed from gl_VertexIndex (pos.x=(vid&1)*4-1,
// pos.y=(vid>>1)*4-1) exported via EXP POS0. Pixel shader: solid green via EXP MRT0. Both are
// assembled by llvm-mc for gfx1030, recompiled to SPIR-V by recompile_vertex/recompile_fragment,
// and run through a real Vulkan pipeline. The fullscreen triangle covers the whole viewport, so we
// assert every sampled pixel is GREEN — proving RDNA2 vertex+pixel -> our SPIR-V -> rendered frame.
#include "../src/gpu/rdna2_to_spirv.hpp"
#include "render_runner.h"
#include <cstdio>
#include <cstdint>
#include <vector>

using namespace prosper::gpu;

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  [FAIL] %s\n", m); fails++; } \
                         else       { printf("  [ok]   %s\n", m); } } while (0)

int main() {
    printf("== test_recompiled_shaders ==\n");
    const uint32_t W = 64, H = 64;

    // Fullscreen-triangle vertex shader (llvm-mc gfx1030): pos = ((vid&1)*4-1, (vid>>1)*4-1, 0, 1).
    const uint32_t vs[] = {
        0x36020081u, 0x2C040081u, 0x7E020D01u, 0x7E040D02u, 0x7E0A02F6u, 0x7E0C02F2u, 0x10020B01u,
        0x08020D01u, 0x10040B02u, 0x08040D02u, 0x7E060280u, 0x7E0802F2u, 0xF80008CFu, 0x04030201u, 0xBF810000u,
    };
    // Green pixel shader: exp mrt0 (0, 1.0, 0, 1.0).
    const uint32_t ps[] = { 0x7E000280u, 0x7E0202F2u, 0x7E040280u, 0x7E0602F2u, 0xF800180Fu, 0x03020100u, 0xBF810000u };

    std::vector<uint32_t> vert = recompile_vertex(vs, sizeof(vs)/sizeof(vs[0]));
    std::vector<uint32_t> frag = recompile_fragment(ps, sizeof(ps)/sizeof(ps[0]));
    CHECK(!vert.empty() && vert[0] == 0x07230203u, "recompiled RDNA2 vertex shader -> SPIR-V");
    CHECK(!frag.empty() && frag[0] == 0x07230203u, "recompiled RDNA2 pixel shader  -> SPIR-V");
    if (vert.empty() || frag.empty()) { printf("== FAIL ==\n"); return 1; }

    std::vector<uint8_t> px = prosper::test::render_triangle_rgba(vert, frag, W, H);
    CHECK(px.size() == (size_t)W * H * 4, "pipeline accepted both recompiled shaders + rendered");
    if (px.size() != (size_t)W * H * 4) { printf("== FAIL: render failed ==\n"); return 1; }

    // The fullscreen triangle covers the whole viewport -> every sampled pixel is green.
    auto isGreen = [&](uint32_t x, uint32_t y) {
        const uint8_t* p = &px[((size_t)y * W + x) * 4];
        return p[1] > 0x80 && p[0] < 0x40 && p[2] < 0x40;
    };
    const uint32_t xs[] = {0, W/2, W-1}, ys[] = {0, H/2, H-1};
    uint32_t green = 0, total = 0;
    for (uint32_t y : ys) for (uint32_t x : xs) { total++; if (isGreen(x, y)) green++; }
    const uint8_t* c = &px[((size_t)(H/2) * W + W/2) * 4];
    printf("  center=(%u,%u,%u,%u)  green samples %u/%u\n", c[0],c[1],c[2],c[3], green, total);
    CHECK(green == total, "every sampled pixel is GREEN (recompiled VS positioned the tri, PS colored it)");

    if (fails) { printf("== FAIL: %d ==\n", fails); return 1; }
    printf("== PASS ==\n");
    return 0;
}
