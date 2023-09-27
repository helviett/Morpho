# CMakeList.txt : CMake project for Morpho, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.15)


set (CMAKE_CXX_STANDARD 20)
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_XYZW_ONLY)

set(SOURCE_FILES
    src/application.cpp src/application.hpp
    src/input/input.cpp src/input/input.hpp
    src/input/key.cpp src/input/key.hpp
    src/camera.cpp src/camera.hpp
    src/main.cpp src/stb_image_impl.cpp
    src/tinygltf_impl.cpp src/stb_image_write_impl.cpp
    src/math.hpp src/math.cpp
)

find_package(stb CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 REQUIRED)
find_package(tinygltf CONFIG REQUIRED)

foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

# Add source to this project's executable.
add_executable (Sandbox ${SOURCE_FILES})

target_link_libraries(Sandbox glfw TinyGLTF::TinyGLTF stb::stb glm::glm Morpho)
set_property(TARGET Sandbox PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
