#pragma once
#include <vulkan/vulkan.h>
#include "image.hpp"

namespace Morpho::Vulkan {

class ImageView {
public:
    ImageView();
    ImageView(VkImageView image_view, Image image = Image());

    VkImageView get_image_view() const;
    Image image;
private:
    VkImageView image_view;
};

}
