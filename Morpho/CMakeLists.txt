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

# include_directories(Vulkan::Headers)

file(GLOB_RECURSE BASE_SRC "*.cpp" "*.hpp" "*.h" )
file(GLOB_RECURSE BASE_HEADERS "*.hpp" "*.h")

# Add source to this project's executable.
add_executable (Morpho ${BASE_SRC})


target_link_libraries(Morpho ${CONAN_LIBS} Vulkan::Vulkan)
set_property(TARGET Morpho PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
