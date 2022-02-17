#pragma once
#include <vulkan/vulkan.h>

namespace Morpho::Vulkan {

class ImageView {
public:
    ImageView();
    ImageView(VkImageView image_view);

    VkImageView get_image_view() const;
private:
    VkImageView image_view;
};

}
