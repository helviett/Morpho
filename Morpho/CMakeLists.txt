# CMakeList.txt : CMake project for Morpho, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)


set (CMAKE_CXX_STANDARD 17)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Vulkan REQUIRED)

if (NOT Vulkan_FOUND)
    message ("Vulkan is not found")
endif()

set(SOURCE_FILES
    stb_image_impl.cpp
    morpho.cpp
    application.hpp application.cpp
    vulkan/command_buffer.hpp vulkan/command_buffer.cpp
    vulkan/context.hpp vulkan/context.cpp
    vulkan/render_pass_info.hpp vulkan/render_pass_info.cpp
    vulkan/render_pass.hpp vulkan/render_pass.cpp
    vulkan/framebuffer.hpp vulkan/framebuffer.cpp
    vulkan/shader.hpp vulkan/shader.cpp
    vulkan/pipeline_state.hpp vulkan/pipeline_state.cpp
    vulkan/pipeline.hpp vulkan/pipeline.cpp
    vulkan/vma.hpp vulkan/vma.cpp
    vulkan/buffer.hpp vulkan/buffer.cpp
    vulkan/resource_binding.hpp vulkan/resource_binding.cpp
    vulkan/resource_set.hpp vulkan/resource_set.cpp
    vulkan/pipeline_layout.hpp vulkan/pipeline_layout.cpp
    vulkan/descriptor_set.hpp vulkan/descriptor_set.cpp
    vulkan/descriptor_set_layout.hpp vulkan/descriptor_set_layout.cpp
    vulkan/limits.hpp vulkan/limits.cpp
    vulkan/image.hpp vulkan/image.cpp
    vulkan/image_view.hpp vulkan/image_view.cpp
    vulkan/sampler.hpp vulkan/sampler.cpp
)

# Add source to this project's executable.
add_executable (Morpho ${SOURCE_FILES})


target_link_libraries(Morpho ${CONAN_LIBS} Vulkan::Vulkan)
set_property(TARGET Morpho PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
