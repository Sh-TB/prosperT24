// vk_translate.cpp — see vk_translate.hpp.
#include "vk_translate.hpp"

namespace prosper::gpu {

VkTopology vk_topology(uint32_t prim_type) {
    // RDNA2 VGT_DI_PRIMITIVE_TYPE (kPrimitiveType*): 1=point, 2=line list, 3=line strip,
    // 4=triangle list, 5=triangle fan, 6=triangle strip.
    switch (prim_type) {
        case 1:  return VkTopology::PointList;
        case 2:  return VkTopology::LineList;
        case 3:  return VkTopology::LineStrip;
        case 4:  return VkTopology::TriangleList;
        case 5:  return VkTopology::TriangleFan;
        case 6:  return VkTopology::TriangleStrip;
        default: return VkTopology::PointList;
    }
}

VkFormat vk_color_format(uint32_t format, uint32_t number_type, uint32_t comp_swap) {
    // format 0xA = COLOR_8_8_8_8; number_type 0 = UNORM, 6 = SRGB; comp_swap 0 = standard (RGBA),
    // 1 = alt (BGRA). (Kyty GraphicsRender.cpp RenderTextureFormat decode.)
    if (format == 0xAu) {
        const bool srgb = (number_type == 6u);
        const bool bgra = (comp_swap == 1u);
        if (!bgra) return srgb ? VkFormat::R8G8B8A8_SRGB : VkFormat::R8G8B8A8_UNORM;
        else       return srgb ? VkFormat::B8G8R8A8_SRGB : VkFormat::B8G8R8A8_UNORM;
    }
    return VkFormat::Undefined;   // surface not yet mapped
}

} // namespace prosper::gpu
