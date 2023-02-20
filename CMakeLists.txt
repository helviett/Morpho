# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.12)
project("Morpho")

# Include sub-projects.
add_subdirectory("Morpho")
add_subdirectory("Sandbox")
