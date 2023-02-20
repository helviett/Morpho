#include "image_view.hpp"

namespace Morpho::Vulkan {

ImageView::ImageView(): image_view(VK_NULL_HANDLE) { }

ImageView::ImageView(VkImageView image_view, Image image): image_view(image_view), image(image) { }

VkImageView ImageView::get_image_view() const {
    return image_view;
}

}
