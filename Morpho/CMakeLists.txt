# CMakeList.txt : CMake project for Morpho, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.25)


set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)

if (NOT Vulkan_FOUND)
    message ("Vulkan is not found")
endif()

set(SOURCE_FILES
    vulkan/command_buffer.hpp vulkan/command_buffer.cpp
    vulkan/context.hpp vulkan/context.cpp
    vulkan/vma.hpp vulkan/vma.cpp
    vulkan/resource_binding.hpp vulkan/resource_binding.cpp
    vulkan/resource_set.hpp vulkan/resource_set.cpp
    vulkan/limits.hpp vulkan/limits.cpp
    common/resource_cache.hpp
    common/hash_utils.hpp
    vulkan/descriptor_pool.hpp vulkan/descriptor_pool.cpp
    vulkan/resources.hpp vulkan/resources.cpp 
)

# Add source to this project's executable.
add_library (Morpho STATIC ${SOURCE_FILES})

message (${vulkan-memory-allocator_SOURCE_DIR})

find_package(glfw3 REQUIRED)
message ($ENV{VULKAN_SDK}/Include/vma/)
target_link_libraries(Morpho glfw Vulkan::Vulkan)
target_include_directories(Morpho PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(Morpho PUBLIC ${vulkan-memory-allocator_SOURCE_DIR}/include)
