#include "application.hpp"
#include <fstream>
#include <stb_image.h>

namespace fs = std::filesystem;

void Application::run() {
    init_window();
    initialize_key_map();
    context->init(window);
    context->set_frame_context_count(2);
    auto swapchain_extent = context->get_swapchain_extent();
    camera = Camera(
        -90.0f,
        0.0f,
        glm::vec3(0, 1, 0),
        glm::radians(45.0f),
        swapchain_extent.width / (float)swapchain_extent.height,
        0.01f,
        1000.0f
    );
    camera.set_position(glm::vec3(0.0f, 0.0f, 10.0f));
    main_loop();
}

void Application::init_window() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(1200, 1000, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, process_keyboard_input);
    glfwSetCursorPosCallback(window, process_cursor_position);
    glfwSetMouseButtonCallback(window, process_mouse_button_input);
}

void Application::main_loop() {
    auto last_frame = 0.0;
    while (!glfwWindowShouldClose(window)) {
        auto current_frame = glfwGetTime();
        auto delta = current_frame - last_frame;
        last_frame = current_frame;
        input.update();
        glfwPollEvents();
        update((float)delta);
        render_frame();
    }
}

void Application::set_graphics_context(Morpho::Vulkan::Context* context) {
    this->context = context;
}

void Application::render_frame() {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    context->begin_frame();
    auto cmd = context->acquire_command_buffer();
    if (is_first_update) {
        initialize_static_resources(cmd);
        is_first_update = false;
    }
    cmd.add_shader(vert_shader);
    cmd.add_shader(frag_shader);
    cmd.set_depth_state(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
    Morpho::Vulkan::RenderPassInfo render_pass;
    render_pass.color_attachment_clear_value = { 0.0f, 0.0f, 0.0f, };
    render_pass.color_attachment_image_view = context->get_swapchain_image_view();
    auto extent = context->get_swapchain_extent();
    auto depth_image = context->acquire_temporary_image(
        { extent.width, extent.height, 1 },
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    render_pass.depth_attachment_image_view = context->create_temporary_image_view(
        VK_FORMAT_D32_SFLOAT,
        depth_image,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
    render_pass.depth_attachment_clear_value.depthStencil = {1.0f, 0};
    cmd.begin_render_pass(render_pass);
    draw_model(model, cmd);
    cmd.end_render_pass();
    context->submit(cmd);
    context->end_frame();
}

std::vector<char> Application::read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file: " + filename);
    }
    auto file_size = (size_t)file.tellg();
    std::vector<char> data(file_size);
    file.seekg(0);
    file.read(data.data(), file_size);
    file.close();

    return data;
}

void Application::initialize_static_resources(Morpho::Vulkan::CommandBuffer& cmd) {
    auto vert_code = read_file("./assets/shaders/triangle.vert.spv");
    auto frag_code = read_file("./assets/shaders/triangle.frag.spv");
    vert_shader = context->acquire_shader(vert_code.data(), (uint32_t)vert_code.size());
    vert_shader.set_stage(Morpho::Vulkan::ShaderStage::Vertex);
    vert_shader.set_entry_point("main");
    frag_shader = context->acquire_shader(frag_code.data(), (uint32_t)frag_code.size());
    frag_shader.set_stage(Morpho::Vulkan::ShaderStage::Fragment);
    frag_shader.set_entry_point("main");
    default_sampler = context->acquire_sampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_FILTER_LINEAR);
    uint32_t white_pixel = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> white_pixels(1 * 1, white_pixel);
    white_image = context->acquire_image(
        { 1, 1, 1 },
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    auto white_staging_buffer = context->acquire_staging_buffer(
        1 * 1 * 4,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );
    white_staging_buffer.update(white_pixels.data(), 1 * 1 * 4);
    cmd.image_barrier(
        white_image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT
    );
    cmd.copy_buffer_to_image(white_staging_buffer, white_image, { (uint32_t)1, (uint32_t)1, (uint32_t)1 });
    cmd.image_barrier(
        white_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT
    );
    white_image_view = context->create_image_view(VK_FORMAT_R8G8B8A8_UNORM, white_image, VK_IMAGE_ASPECT_COLOR_BIT);
    create_scene_resources(cmd);
}

void Application::process_keyboard_input(GLFWwindow* window, int key_code, int scancode, int action, int mods) {
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    auto key = app->glfw_key_code_to_key(key_code);
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        app->input.press_key(key);
    } else if (action == GLFW_RELEASE) {
        app->input.release_key(key);
    }
}

void Application::process_cursor_position(GLFWwindow* window, double xpos, double ypos) {
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    app->input.set_mouse_position((float)xpos, (float)ypos);
}

void Application::process_mouse_button_input(GLFWwindow* window, int button, int action, int mods)
{
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    Key keys[GLFW_MOUSE_BUTTON_LAST] = { Key::UNDEFINED, };
    keys[GLFW_MOUSE_BUTTON_LEFT] = Key::MOUSE_BUTTON_LEFT;
    keys[GLFW_MOUSE_BUTTON_RIGHT] = Key::MOUSE_BUTTON_RIGHT;
    keys[GLFW_MOUSE_BUTTON_MIDDLE] = Key::MOUSE_BUTTON_MIDDLE;
    if (keys[button] == Key::UNDEFINED) {
        return;
    }
    if (action == GLFW_PRESS) {
        app->input.press_key(keys[button]);
    } else {
        app->input.release_key(keys[button]);
    }
}


void Application::initialize_key_map() {
    for (uint32_t i = 0; i < Input::MAX_KEY_COUNT; i++) {
        key_map[i] = Key::UNDEFINED;
    }
    key_map[GLFW_KEY_SPACE] = Key::SPACE;
    key_map[GLFW_KEY_APOSTROPHE] = Key::APOSTROPHE;
    key_map[GLFW_KEY_COMMA] = Key::COMMA;
    key_map[GLFW_KEY_MINUS] = Key::MINUS;
    key_map[GLFW_KEY_PERIOD] = Key::PERIOD;
    key_map[GLFW_KEY_SLASH] = Key::SLASH;
    key_map[GLFW_KEY_0] = Key::NUMBER0;
    key_map[GLFW_KEY_1] = Key::NUMBER1;
    key_map[GLFW_KEY_2] = Key::NUMBER2;
    key_map[GLFW_KEY_3] = Key::NUMBER3;
    key_map[GLFW_KEY_4] = Key::NUMBER4;
    key_map[GLFW_KEY_5] = Key::NUMBER5;
    key_map[GLFW_KEY_6] = Key::NUMBER6;
    key_map[GLFW_KEY_7] = Key::NUMBER7;
    key_map[GLFW_KEY_8] = Key::NUMBER8;
    key_map[GLFW_KEY_9] = Key::NUMBER9;
    key_map[GLFW_KEY_SEMICOLON] = Key::SEMICOLON;
    key_map[GLFW_KEY_EQUAL] = Key::EQUAL;
    key_map[GLFW_KEY_A] = Key::A;
    key_map[GLFW_KEY_B] = Key::B;
    key_map[GLFW_KEY_C] = Key::C;
    key_map[GLFW_KEY_D] = Key::D;
    key_map[GLFW_KEY_E] = Key::E;
    key_map[GLFW_KEY_F] = Key::F;
    key_map[GLFW_KEY_G] = Key::G;
    key_map[GLFW_KEY_H] = Key::H;
    key_map[GLFW_KEY_I] = Key::I;
    key_map[GLFW_KEY_J] = Key::J;
    key_map[GLFW_KEY_K] = Key::K;
    key_map[GLFW_KEY_L] = Key::L;
    key_map[GLFW_KEY_M] = Key::M;
    key_map[GLFW_KEY_N] = Key::N;
    key_map[GLFW_KEY_O] = Key::O;
    key_map[GLFW_KEY_P] = Key::P;
    key_map[GLFW_KEY_Q] = Key::Q;
    key_map[GLFW_KEY_R] = Key::R;
    key_map[GLFW_KEY_S] = Key::S;
    key_map[GLFW_KEY_T] = Key::T;
    key_map[GLFW_KEY_U] = Key::U;
    key_map[GLFW_KEY_V] = Key::V;
    key_map[GLFW_KEY_W] = Key::W;
    key_map[GLFW_KEY_X] = Key::X;
    key_map[GLFW_KEY_Y] = Key::Y;
    key_map[GLFW_KEY_Z] = Key::Z;
    key_map[GLFW_KEY_LEFT_BRACKET] = Key::LEFT_BRACKET;
    key_map[GLFW_KEY_BACKSLASH] = Key::BACKSLASH;
    key_map[GLFW_KEY_RIGHT_BRACKET] = Key::RIGHT_BRACKET;
    key_map[GLFW_KEY_GRAVE_ACCENT] = Key::GRAVE_ACCENT;
    key_map[GLFW_KEY_WORLD_1] = Key::WORLD_1;
    key_map[GLFW_KEY_WORLD_2] = Key::WORLD_2;
    key_map[GLFW_KEY_ESCAPE] = Key::ESCAPE;
    key_map[GLFW_KEY_ENTER] = Key::ENTER;
    key_map[GLFW_KEY_TAB] = Key::TAB;
    key_map[GLFW_KEY_BACKSPACE] = Key::BACKSPACE;
    key_map[GLFW_KEY_INSERT] = Key::INSERT;
    key_map[GLFW_KEY_DELETE] = Key::DEL;
    key_map[GLFW_KEY_RIGHT] = Key::RIGHT;
    key_map[GLFW_KEY_LEFT] = Key::LEFT;
    key_map[GLFW_KEY_DOWN] = Key::DOWN;
    key_map[GLFW_KEY_UP] = Key::UP;
    key_map[GLFW_KEY_PAGE_UP] = Key::PAGE_UP;
    key_map[GLFW_KEY_PAGE_DOWN] = Key::PAGE_DOWN;
    key_map[GLFW_KEY_HOME] = Key::HOME;
    key_map[GLFW_KEY_END] = Key::END;
    key_map[GLFW_KEY_CAPS_LOCK] = Key::CAPS_LOCK;
    key_map[GLFW_KEY_SCROLL_LOCK] = Key::SCROLL_LOCK;
    key_map[GLFW_KEY_NUM_LOCK] = Key::NUM_LOCK;
    key_map[GLFW_KEY_PRINT_SCREEN] = Key::PRINT_SCREEN;
    key_map[GLFW_KEY_PAUSE] = Key::PAUSE;
    key_map[GLFW_KEY_F1] = Key::F1;
    key_map[GLFW_KEY_F2] = Key::F2;
    key_map[GLFW_KEY_F3] = Key::F3;
    key_map[GLFW_KEY_F4] = Key::F4;
    key_map[GLFW_KEY_F5] = Key::F5;
    key_map[GLFW_KEY_F6] = Key::F6;
    key_map[GLFW_KEY_F7] = Key::F7;
    key_map[GLFW_KEY_F8] = Key::F8;
    key_map[GLFW_KEY_F9] = Key::F9;
    key_map[GLFW_KEY_F10] = Key::F10;
    key_map[GLFW_KEY_F11] = Key::F11;
    key_map[GLFW_KEY_F12] = Key::F12;
    key_map[GLFW_KEY_F13] = Key::F13;
    key_map[GLFW_KEY_F14] = Key::F14;
    key_map[GLFW_KEY_F15] = Key::F15;
    key_map[GLFW_KEY_F16] = Key::F16;
    key_map[GLFW_KEY_F17] = Key::F17;
    key_map[GLFW_KEY_F18] = Key::F18;
    key_map[GLFW_KEY_F19] = Key::F19;
    key_map[GLFW_KEY_F20] = Key::F20;
    key_map[GLFW_KEY_F21] = Key::F21;
    key_map[GLFW_KEY_F22] = Key::F22;
    key_map[GLFW_KEY_F23] = Key::F23;
    key_map[GLFW_KEY_F24] = Key::F24;
    key_map[GLFW_KEY_F25] = Key::F25;
    key_map[GLFW_KEY_KP_0] = Key::KP_0;
    key_map[GLFW_KEY_KP_1] = Key::KP_1;
    key_map[GLFW_KEY_KP_2] = Key::KP_2;
    key_map[GLFW_KEY_KP_3] = Key::KP_3;
    key_map[GLFW_KEY_KP_4] = Key::KP_4;
    key_map[GLFW_KEY_KP_5] = Key::KP_5;
    key_map[GLFW_KEY_KP_6] = Key::KP_6;
    key_map[GLFW_KEY_KP_7] = Key::KP_7;
    key_map[GLFW_KEY_KP_8] = Key::KP_8;
    key_map[GLFW_KEY_KP_9] = Key::KP_9;
    key_map[GLFW_KEY_KP_DECIMAL] = Key::KP_DECIMAL;
    key_map[GLFW_KEY_KP_DIVIDE] = Key::KP_DIVIDE;
    key_map[GLFW_KEY_KP_MULTIPLY] = Key::KP_MULTIPLY;
    key_map[GLFW_KEY_KP_SUBTRACT] = Key::KP_SUBTRACT;
    key_map[GLFW_KEY_KP_ADD] = Key::KP_ADD;
    key_map[GLFW_KEY_KP_ENTER] = Key::KP_ENTER;
    key_map[GLFW_KEY_KP_EQUAL] = Key::KP_EQUAL;
    key_map[GLFW_KEY_LEFT_SHIFT] = Key::LEFT_SHIFT;
    key_map[GLFW_KEY_LEFT_CONTROL] = Key::LEFT_CONTROL;
    key_map[GLFW_KEY_LEFT_ALT] = Key::LEFT_ALT;
    key_map[GLFW_KEY_LEFT_SUPER] = Key::LEFT_SUPER;
    key_map[GLFW_KEY_RIGHT_SHIFT] = Key::RIGHT_SHIFT;
    key_map[GLFW_KEY_RIGHT_CONTROL] = Key::RIGHT_CONTROL;
    key_map[GLFW_KEY_RIGHT_ALT] = Key::RIGHT_ALT;
    key_map[GLFW_KEY_RIGHT_SUPER] = Key::RIGHT_SUPER;
    key_map[GLFW_KEY_MENU] = Key::MENU;
}

Key Application::glfw_key_code_to_key(int code) {
    return code < 0 ? Key::UNDEFINED : key_map[code];
}

void Application::update(float delta) {
    glm::vec2 mouse_position(0, 0);
    input.get_mouse_position(&mouse_position.x, &mouse_position.y);
    if (input.is_key_pressed(Key::MOUSE_BUTTON_RIGHT)) {
        if (!is_mouse_pressed) {
            last_mouse_position = mouse_position;
            is_mouse_pressed = true;
        } else {
            auto mouse_delta = last_mouse_position - mouse_position;
            camera.add_yaw(-camera_sensetivity * mouse_delta.x);
            camera.add_pitch(camera_sensetivity * mouse_delta.y);
            last_mouse_position = mouse_position;
        }
    } else {
        is_mouse_pressed = false;
    }

    auto w_pressed = input.is_key_pressed(Key::W);
    auto a_pressed = input.is_key_pressed(Key::A);
    auto s_pressed = input.is_key_pressed(Key::S);
    auto d_pressed = input.is_key_pressed(Key::D);
    auto q_pressed = input.is_key_pressed(Key::Q);
    auto e_pressed = input.is_key_pressed(Key::E);

    auto camera_position = camera.get_position();
    auto face_move = (w_pressed ^ s_pressed) * (w_pressed ? 1 : -1);
    auto side_move = (a_pressed ^ d_pressed) * (a_pressed ? -1 : 1);
    auto vertical_move = (q_pressed ^ e_pressed) * (q_pressed ? -1 : 1);
    if (face_move != 0 || side_move != 0 || vertical_move != 0) {
        camera_position += camera.get_forward() * (10.0f * delta * (float)face_move);
        camera_position += camera.get_right() * (10.0f * delta * (float)side_move);
        camera_position += world_up * (10.0f * delta * (float)vertical_move);
        camera.set_position(camera_position);
    }
}

bool Application::load_scene(std::filesystem::path file_path) {
    std::string err;
    std::string warn;
    if (file_path.extension() == ".gltf") {
        loader.LoadASCIIFromFile(&model, &err, &warn, file_path.string());
    } else {
        loader.LoadBinaryFromFile(&model, &err, &warn, file_path.string());
    }
    if (!warn.empty()) {
        std::cout << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << err << std::endl;
        return false;
    }
    return true;
}

void Application::create_scene_resources(Morpho::Vulkan::CommandBuffer& cmd) {
    std::vector<VkBufferUsageFlags> buffer_usages(model.buffers.size(), 0);
    buffers.resize(model.buffers.size());
    for (auto& mesh : model.meshes) {
        for (auto& primitive : mesh.primitives) {
            if (primitive.indices >= 0) {
                auto buffer_view_index = model.accessors[primitive.indices].bufferView;
                auto buffer_index = model.bufferViews[buffer_view_index].buffer;
                buffer_usages[buffer_index] |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            }
            for (auto& key_value : primitive.attributes) {
                auto buffer_view_index = model.accessors[key_value.second].bufferView;
                auto buffer_index = model.bufferViews[buffer_view_index].buffer;
                buffer_usages[buffer_index] |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            }
        }
    }
    for (uint32_t i = 0; i < model.buffers.size(); i++) {
        auto buffer_size = (VkDeviceSize)model.buffers[i].data.size();
        auto staging_buffer = context->acquire_staging_buffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY
        );
        staging_buffer.update(model.buffers[i].data.data(), buffer_size);
        buffers[i] = context->acquire_buffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | buffer_usages[i],
            VMA_MEMORY_USAGE_GPU_ONLY
        );
        cmd.copy_buffer(staging_buffer, buffers[i], buffer_size);
    }
    texture_images.resize(model.textures.size());
    texture_image_views.resize(model.textures.size());
    for (uint32_t i = 0; i < model.textures.size(); i++) {
        auto& texture = model.textures[i];
        auto gltf_image = model.images[texture.source];
        // TODO: get format from bits + component + pixel_type
        assert(gltf_image.component == 4);
        auto image_size = (VkDeviceSize)(gltf_image.width * gltf_image.height * gltf_image.component * (gltf_image.bits / 8));
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
        auto staging_image_buffer = context->acquire_staging_buffer(
            image_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY
        );
        staging_image_buffer.update(gltf_image.image.data(), image_size);
        auto image = context->acquire_image(
            { (uint32_t)gltf_image.width, (uint32_t)gltf_image.height, (uint32_t)1 },
            format,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );
        cmd.image_barrier(
            image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT
        );
        cmd.copy_buffer_to_image(staging_image_buffer, image, { (uint32_t)gltf_image.width, (uint32_t)gltf_image.height, (uint32_t)1 });
        cmd.image_barrier(
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );
        texture_images[i] = image;
        auto image_view = context->create_image_view(
            format,
            image,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        texture_image_views[i] = image_view;
    }
    samplers.resize(model.samplers.size());
    for (uint32_t i = 0; i < model.samplers.size(); i++) {
        auto gltf_sampler = model.samplers[i];
        samplers[i] = context->acquire_sampler(
            gltf_to_sampler_address_mode(gltf_sampler.wrapT),
            gltf_to_sampler_address_mode(gltf_sampler.wrapR),
            gltf_to_sampler_address_mode(gltf_sampler.wrapS),
            gltf_to_filter(gltf_sampler.minFilter),
            gltf_to_filter(gltf_sampler.magFilter)
        );
    }
}

void Application::draw_model(const tinygltf::Model& model, Morpho::Vulkan::CommandBuffer& cmd) {
    current_material_index = -1;
    for (const auto& scene : model.scenes) {
        draw_scene(model, scene, cmd);
    }
}

void Application::draw_scene(const tinygltf::Model& model, const tinygltf::Scene& scene, Morpho::Vulkan::CommandBuffer& cmd) {
    for (const auto node_index : scene.nodes) {
        draw_node(model, model.nodes[node_index], cmd, glm::mat4(1.0f));
    }
}

void Application::draw_node(
    const tinygltf::Model& model,
    const tinygltf::Node& node,
    Morpho::Vulkan::CommandBuffer& cmd,
    glm::mat4 parent_to_world
) {
    glm::mat4 local_to_world = glm::mat4(1.0f);
    if (node.matrix.size() == 16) {
        auto& m = node.matrix;
        local_to_world = glm::mat4(
            m[0], m[1], m[2], m[3],
            m[4], m[5], m[6], m[7],
            m[8], m[9], m[10], m[11],
            m[12], m[13], m[14], m[15]
        );
    } else {
        if (node.scale.size() == 3) {
            auto& s = node.scale;
            local_to_world = glm::scale(
                local_to_world,
                glm::vec3(s[0], s[1], s[2])
            );
        }
        if (node.rotation.size() == 4) {
            auto& r = node.rotation;
            auto q = glm::quat((float)r[0], (float)r[1], (float)r[2], (float)r[3]);
            local_to_world = glm::mat4(q) * local_to_world;
        }
        if (node.translation.size() == 3) {
            auto& t = node.translation;
            local_to_world = glm::translate(
                local_to_world,
                glm::vec3(t[0], t[1], t[2])
            );
        }
    }
    local_to_world = parent_to_world * local_to_world;
    if (node.mesh >= 0) {
        auto uniform_buffer = context->acquire_staging_buffer(
            sizeof(UniformBufferObject),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY
        );
        UniformBufferObject mvp;
        mvp.model = local_to_world;
        mvp.view = camera.get_view();
        mvp.proj = camera.get_projection();
        uniform_buffer.update(&mvp, sizeof(UniformBufferObject));
        cmd.set_uniform_buffer(0, 0, uniform_buffer, 0, sizeof(UniformBufferObject));
        draw_mesh(model, model.meshes[node.mesh], cmd);
    }
    for (uint32_t i = 0; i < node.children.size(); i++) {
        draw_node(model, model.nodes[node.children[i]], cmd, local_to_world);
    }
}

void Application::draw_mesh(
    const tinygltf::Model& model,
    const tinygltf::Mesh& mesh,
    Morpho::Vulkan::CommandBuffer& cmd
) {
    for (auto& primitive : mesh.primitives) {
        draw_primitive(model, primitive, cmd);
    }
}

void Application::draw_primitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    Morpho::Vulkan::CommandBuffer& cmd
) {
    if (primitive.material < 0) {
        std::cout << "Primitive with no material" << std::endl;
        return;
    }
    uint32_t current_binding = 0;
    if (primitive.material != current_material_index) {
        auto& material = model.materials[primitive.material];
        cmd.clear_vertex_attribute_descriptions();
        cmd.clear_vertex_binding_descriptions();
        for (auto& key_value : primitive.attributes) {
            auto& attribute_name = key_value.first;
            auto accessor_index = key_value.second;
            auto& accessor = model.accessors[accessor_index];
            auto& buffer_view = model.bufferViews[accessor.bufferView];
            auto location = attribute_name_to_location.find(attribute_name);
            if (location == attribute_name_to_location.end()) {
                continue;
            }
            cmd.add_vertex_attribute_description(
                current_binding,
                (*location).second,
                gltf_to_format(accessor.type, accessor.componentType),
                0
            );
            cmd.add_vertex_binding_description(
                current_binding++,
                accessor.ByteStride(buffer_view),
                VK_VERTEX_INPUT_RATE_VERTEX
            );
        }
        auto texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
        auto base_color_image_view = texture_index < 0 ? white_image_view : texture_image_views[texture_index];
        auto sampler_index = texture_index < 0 ? -1 : model.textures[texture_index].sampler;
        auto sampler = sampler_index < 0 ? default_sampler : samplers[sampler_index];
        cmd.set_combined_image_sampler(0, 1, base_color_image_view, sampler);
        auto base_color_factor = glm::vec4(
            material.pbrMetallicRoughness.baseColorFactor[0],
            material.pbrMetallicRoughness.baseColorFactor[1],
            material.pbrMetallicRoughness.baseColorFactor[2],
            material.pbrMetallicRoughness.baseColorFactor[3]
        );
        auto base_color_factor_buffer = context->acquire_staging_buffer(
            sizeof(base_color_factor),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
        base_color_factor_buffer.update(&base_color_factor, sizeof(base_color_factor));
        cmd.set_uniform_buffer(0, 2, base_color_factor_buffer, 0, sizeof(base_color_factor));
        current_material_index = primitive.material;
    }
    current_binding = 0;
    for (auto& key_value : primitive.attributes) {
        if (attribute_name_to_location.find(key_value.first) == attribute_name_to_location.end()) {
            continue;
        }
        auto& attribute_name = key_value.first;
        auto accessor_index = key_value.second;
        auto& accessor = model.accessors[accessor_index];
        auto& buffer_view = model.bufferViews[accessor.bufferView];
        cmd.bind_vertex_buffer(
            buffers[buffer_view.buffer],
            current_binding++,
            accessor.byteOffset + buffer_view.byteOffset
        );
    }
    if (primitive.indices >= 0) {
        auto& accessor = model.accessors[primitive.indices];
        auto& buffer_view = model.bufferViews[accessor.bufferView];
        auto index_type = gltf_to_index_type(accessor.type, accessor.componentType);
        auto index_type_size = (uint32_t)tinygltf::GetComponentSizeInBytes(accessor.componentType);
        auto index_count = (uint32_t)accessor.count;
        cmd.bind_index_buffer(
            buffers[buffer_view.buffer],
            index_type,
            accessor.byteOffset + buffer_view.byteOffset
        );
        cmd.draw_indexed(
            index_count,
            1,
            0,
            0,
            0
        );
    }
}

struct hash_pair {
    template <class T1, class T2>
    size_t operator()(const std::pair<T1, T2>& p) const
    {
        auto first_hash = std::hash<T1>{}(p.first);
        auto second_hash = std::hash<T2>{}(p.second);
        return first_hash ^ second_hash;
    }
};

VkFormat Application::gltf_to_format(int type, int component_type) {
    static const std::unordered_map<std::pair<int, int>, VkFormat, hash_pair> map = {
        {{TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT}, VK_FORMAT_R32G32B32_SFLOAT},
        {{TINYGLTF_TYPE_VEC2, TINYGLTF_COMPONENT_TYPE_FLOAT}, VK_FORMAT_R32G32_SFLOAT},
    };
    return map.at(std::make_pair(type, component_type));
}

VkIndexType Application::gltf_to_index_type(int type, int component_type) {
    assert(type == TINYGLTF_TYPE_SCALAR);
    static const std::unordered_map<int, VkIndexType> map = {
        {TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT, VK_INDEX_TYPE_UINT16},
        {TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, VK_INDEX_TYPE_UINT32},
    };
    return map.at(component_type);
}

VkFilter Application::gltf_to_filter(int filter) {
    static const std::unordered_map<int, VkFilter> map = {
        {TINYGLTF_TEXTURE_FILTER_NEAREST, VK_FILTER_NEAREST},
        {TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST, VK_FILTER_NEAREST},
        {TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR, VK_FILTER_NEAREST},
        {TINYGLTF_TEXTURE_FILTER_LINEAR, VK_FILTER_LINEAR},
        {TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST, VK_FILTER_LINEAR},
        {TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR, VK_FILTER_LINEAR},
    };
    return map.at(filter);
}

VkSamplerAddressMode Application::gltf_to_sampler_address_mode(int address_mode) {
    static const std::unordered_map<int, VkSamplerAddressMode> map = {
        {TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
        {TINYGLTF_TEXTURE_WRAP_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT},
        {TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT},
    };
    return map.at(address_mode);
}
