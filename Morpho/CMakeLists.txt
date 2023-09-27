# CMakeList.txt : CMake project for Morpho, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.15)


set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

if (NOT Vulkan_FOUND)
    message ("Vulkan is not found")
endif()

set(SOURCE_FILES
    vulkan/command_buffer.hpp vulkan/command_buffer.cpp
    vulkan/context.hpp vulkan/context.cpp
    vulkan/vma.hpp vulkan/vma.cpp
    vulkan/limits.hpp vulkan/limits.cpp
    common/resource_cache.hpp
    common/hash_utils.hpp
    vulkan/descriptor_pool.hpp vulkan/descriptor_pool.cpp
    vulkan/resources.hpp vulkan/resources.cpp 
    common/utils.hpp
)

# Add source to this project's executable.
add_library (Morpho STATIC ${SOURCE_FILES})

find_package(vulkan-memory-allocator CONFIG REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(Morpho glfw vulkan-memory-allocator::vulkan-memory-allocator Vulkan::Vulkan)
target_include_directories(Morpho PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
