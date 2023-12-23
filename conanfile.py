from conan import ConanFile
from conan.tools.files import copy
import os


class MorphoRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("glfw/3.3.8")
        self.requires("glm/0.9.9.8")
        self.requires("stb/cci.20220909")
        self.requires("vulkan-memory-allocator/3.0.1")
        self.requires("tinygltf/2.8.13")
        self.requires("imgui/1.90")

    def generate(self):
        imgui = self.dependencies['imgui']
        copy(
            self,
            "imgui_impl_*",
            imgui.cpp_info.srcdirs[0],
            os.path.join(self.build_folder, "imgui/bindings")
        )
