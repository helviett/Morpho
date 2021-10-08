# CMakeList.txt : CMake project for Morpho, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)


set (CMAKE_CXX_STANDARD 17)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Vulkan REQUIRED)

include_directories(${Vulkan_INCLUDE_DIRS})

file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" )
file(GLOB BASE_HEADERS "*.hpp" "*.h")

# Add source to this project's executable.
add_executable (Morpho ${BASE_SRC})


target_link_libraries(Morpho ${CONAN_LIBS} ${Vulkan_LIBRARY})

set(SHADERS "${CMAKE_SOURCE_DIR}/Morpho/shaders")
set(TEXTURES "${CMAKE_SOURCE_DIR}/Morpho/textures")
set(SHADERS_BIN "${CMAKE_CURRENT_BINARY_DIR}/bin/shaders")
set(TEXTURES_BIN "${CMAKE_CURRENT_BINARY_DIR}/bin/textures")


# TODO: Add tests and install targets if needed.
