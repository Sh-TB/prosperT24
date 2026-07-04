// vk_translate.hpp — map decoded RDNA2 render-state enums to their Vulkan equivalents.
//
// Kept free of <vulkan.h> so the core library needn't link Vulkan: the enum values below are the
// exact numeric values of the corresponding VkPrimitiveTopology enumerators, so the Vulkan backend
// (M4) can `static_cast` directly. Mappings follow the RDNA2 VGT_DI_PRIMITIVE_TYPE enum and Kyty's
// GraphicsRender.cpp topology switch.
#pragma once
#include <cstdint>

namespace prosper::gpu {

// Values == VkPrimitiveTopology enumerators.
enum class VkTopology : uint32_t {
    PointList     = 0,   // VK_PRIMITIVE_TOPOLOGY_POINT_LIST
    LineList      = 1,
    LineStrip     = 2,
    TriangleList  = 3,
    TriangleStrip = 4,
    TriangleFan   = 5,
};

// Map a VGT_PRIMITIVE_TYPE.PRIM_TYPE value to a Vulkan topology. Unknown/unsupported types fall back
// to PointList (a safe, visible default) rather than asserting, so an unexpected stream still runs.
VkTopology vk_topology(uint32_t prim_type);

} // namespace prosper::gpu
