workspace "Sandbox"
    cppdialect "C++20"
    architecture "x64"
    configurations { "Debug", "Release" }
    location "build"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

    filter "toolset:clang"
        buildoptions { "-Wno-missing-braces" }


project "Morpho"
    kind "StaticLib"
    language "C++"
    targetdir "build/bin/%{cfg.buildcfg}"
    files { "Morpho/**.hpp", "Morpho/**.cpp" }
    location "build"
    externalincludedirs {
        os.getenv("VULKAN_SDK") .. "/Include",
        "ThirdParty/glfw/include",
        "ThirdParty/VulkanMemoryAllocator/include",
        "ThirdParty/stb",
    }
    includedirs {
        "Morpho",
    }

project "GLFW"
    kind "StaticLib"
    targetdir "build/bin/%{cfg.buildcfg}"
    location "build"
    includedirs "ThirdParty/glfw/include"
    srcdir = "ThirdParty/glfw/src/"
    files { 
        "ThirdParty/glfw/src/win32_**.c",
        srcdir .. "internal.h", srcdir .. "platform.h", srcdir .. "mappings.h",
        srcdir .. "context.c", srcdir .. "init.c", srcdir .. "input.c",
        srcdir .. "monitor.c", srcdir .. "platform.c", srcdir .. "vulkan.c",
        srcdir .. "window.c", srcdir .. "egl_context.c", srcdir .. "osmesa_context.c",
        srcdir .. "null_platform.h", srcdir .. "null_joystick.h", srcdir .. "null_init.c",
        srcdir .. "null_monitor.c", srcdir .. "null_window.c", srcdir .. "null_joystick.c",
        srcdir .. "wgl_context.c"
    }
    defines { "_GLFW_WIN32", "_UNICODE", "_CRT_SECURE_NO_WARNINGS", }

project "Sandbox"
    kind "WindowedApp"
    language "C++"
    targetdir "build/bin/%{cfg.buildcfg}"
    files { "Sandbox/**.hpp", "Sandbox/**.cpp" }
    location "build"
    entrypoint "mainCRTStartup"
    externalincludedirs {
        os.getenv("VULKAN_SDK") .. "/Include",
        "ThirdParty/glfw/include",
        "ThirdParty/glm",
        -- boolshit, Sanbox should only be aware of ptr to buffer only
        -- struct VmaAllocat* allocation; or somethin like that
        "ThirdParty/VulkanMemoryAllocator/include",
        "ThirdParty/tinygltf",
        "ThirdParty/stb",
    }
    includedirs {
        "Sandbox",
        "Morpho",
    }
    libdirs {
        os.getenv("VULKAN_SDK") .. "/Lib",
    }
    dependson { "Morpho", "GLFW" }
    defines {
        "GLM_FORCE_DEPTH_ZERO_TO_ONE",
        "GLM_FORCE_XYZW_ONLY",
        "GLM_ENABLE_EXPERIMENTAL",
    }
    links {
        "Morpho",
        "vulkan-1",
        "GLFW"
    }
    debugdir "build/bin/%{cfg.buildcfg}"
    postbuildcommands {
        "{COPYDIR} %[Sandbox/assets] %[build/bin/%{cfg.buildcfg}/assets]"
    }
