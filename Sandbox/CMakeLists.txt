# CMakeList.txt : CMake project for Morpho, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)


set (CMAKE_CXX_STANDARD 17)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(SOURCE_FILES
    src/application.cpp src/application.hpp
    src/input/input.cpp src/input/input.hpp
    src/input/key.cpp src/input/key.hpp
    src/camera.cpp src/camera.hpp
    src/main.cpp src/stb_image_impl.cpp
    src/tinygltf_impl.cpp src/stb_image_write_impl.cpp
)

# Add source to this project's executable.
add_executable (Sandbox ${SOURCE_FILES})


target_link_libraries(Sandbox ${CONAN_LIBS} Morpho)
set_property(TARGET Sandbox PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
