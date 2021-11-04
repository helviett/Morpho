#include "vulkan/context.hpp"
#include "application.hpp"

int main() {
    Application app;
    auto context = new Morpho::Vulkan::Context();
    app.set_graphics_context(context);
    app.run();
    return 0;
}
