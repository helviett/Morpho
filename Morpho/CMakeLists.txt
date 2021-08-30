# CMakeList.txt : CMake project for Morpho, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)


include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# Add source to this project's executable.
add_executable (Morpho "Morpho.cpp" "Morpho.h")


target_link_libraries(Morpho ${CONAN_LIBS})

# TODO: Add tests and install targets if needed.
