#include "render_pass.hpp"

namespace Morpho::Vulkan {

RenderPass::RenderPass(VkRenderPass render_pass) {
    this->render_pass = render_pass;
}

VkRenderPass RenderPass::get_vulkan_handle() {
    return render_pass;
}

}
