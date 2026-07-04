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

} // namespace prosper::gpu
