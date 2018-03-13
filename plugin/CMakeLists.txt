cmake_minimum_required (VERSION 2.6)
project (plugin)

include_directories(${CMAKE_SOURCE_DIR})

add_library(plugin SHARED plugin.c)
