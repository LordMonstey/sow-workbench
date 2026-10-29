cmake_minimum_required(VERSION 3.24)
project(Monstey_SoWNativeWorkbench VERSION 0.3.0 LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "Windows MSVC is required")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

enable_testing()

include(cmake/Monstey_Helpers.cmake)
include(cmake/Monstey_Libraries.cmake)
include(cmake/Monstey_Applications.cmake)
include(cmake/Monstey_Tests.cmake)
