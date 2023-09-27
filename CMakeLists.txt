# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.15)
project("Morpho")
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
# Include sub-projects.
add_subdirectory("Morpho")
add_subdirectory("Sandbox")
