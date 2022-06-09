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
		"%{prj.name}/src",
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		runtime "Debug"
		defines "LIMEC_DEBUG"
		libdirs "D:/dev/llvm-project/build/Debug/lib"
		links   "D:/dev/llvm-project/build/Debug/lib/**.lib"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		defines "LIMEC_RELEASE"
		libdirs "D:/dev/llvm-project/build/Release/lib"
		links   "D:/dev/llvm-project/build/Release/lib/**.lib"
		optimize "on"

project "lime"
	location "lime"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"
	
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
		--"D:/dev/llvm-project/llvm/include",
		--"D:/dev/llvm-project/build/include",
	}

	libdirs
	{
		--"D:/dev/llvm-project/build/Debug/lib"
	}

	links
	{
		--"D:/dev/llvm-project/build/Debug/lib/**.lib"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		runtime "Debug"
		defines "LIME_DEBUG"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		defines "LIME_RELEASE"
		optimize "on"