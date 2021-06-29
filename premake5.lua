workspace "lime"
	architecture "x86_64"
	startproject "limec"

	configurations
	{
		"Debug",
		"Release",
	}
	
	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}"

project "limec"
	location "limec"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"
	
	pchheader "limecpch.h"
	pchsource "limec/src/limecpch.cpp"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
	}

	includedirs
	{
		"D:/dev/llvm-project/llvm/include",
		"D:/dev/llvm-project/build/include",
	}

	libdirs
	{
		"D:/dev/llvm-project/build/Debug/lib"
	}

	links
	{
		-- Just link all that shit
		-- Also I don't know if this does what I think it does, but it just works so idek
		"D:/dev/llvm-project/build/Debug/lib/**.lib"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"