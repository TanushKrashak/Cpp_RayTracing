-- premake5.lua
workspace "RT_Cpp"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "RT_Cpp"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
include "Walnut/WalnutExternal.lua"

include "RT_Cpp"