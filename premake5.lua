-- premake5.lua
workspace "openalias"
   architecture "x64"
   configurations { "Debug", "Release" }
   startproject "openalias"

project "openalias"
   kind "ConsoleApp"
   language "C"
   targetdir "bin/%{cfg.buildcfg}"

   files { "openalias.c" }

   filter "system:windows"
      defines {"_WIN32" }
      links { "dnsapi" }
      systemversion "latest"

   filter "system:linux"
      defines { "_POSIX_C_SOURCE=200112L" }
      links { "resolv" }
      buildoptions { "-Wall", "-Wextra" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
