#include "image_view.hpp"

namespace Morpho::Vulkan {

ImageView::ImageView(): image_view(VK_NULL_HANDLE) { }

ImageView::ImageView(VkImageView image_view): image_view(image_view) { }

VkImageView ImageView::get_image_view() const {
    return image_view;
}

}
