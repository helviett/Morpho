#include "vulkan/context.hpp"
#include "application.hpp"
#include <filesystem>
#include <iostream>
#include <vector>
#include "math.hpp"

int main(int argc, char* argv[]) {
    Application app;
    auto context = new Morpho::Vulkan::Context();
    app.set_graphics_context(context);
    if (!app.load_scene(argv[1])) {
        return 1;
    }
    app.run();
    return 0;
}
