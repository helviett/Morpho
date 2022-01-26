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
    morpho.cpp
    application.hpp application.cpp
    vulkan/command_buffer.hpp vulkan/command_buffer.cpp
    vulkan/context.hpp vulkan/context.cpp
    vulkan/render_pass_info.hpp vulkan/render_pass_info.cpp
    vulkan/render_pass.hpp vulkan/render_pass.cpp
    vulkan/framebuffer.hpp vulkan/framebuffer.cpp
    vulkan/shader.hpp vulkan/shader.cpp
    vulkan/pipeline_info.hpp vulkan/pipeline_info.cpp
    vulkan/pipeline.hpp vulkan/pipeline.cpp
    vulkan/vma.hpp vulkan/vma.cpp
    vulkan/buffer.hpp vulkan/buffer.cpp
)

# Add source to this project's executable.
add_executable (Morpho ${SOURCE_FILES})


target_link_libraries(Morpho ${CONAN_LIBS} Vulkan::Vulkan)
set_property(TARGET Morpho PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
