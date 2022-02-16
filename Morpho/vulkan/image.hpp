#pragma once
#include <vulkan/vulkan.h>
#include "vma.hpp"

namespace Morpho::Vulkan {

class Image {
public:
    Image();
    Image(VkImage image, VmaAllocation allocation, VmaAllocationInfo allocation_info);

    VkImage get_image() const;
    VmaAllocation get_allocation() const;
private:
    VkImage image;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
};

}
