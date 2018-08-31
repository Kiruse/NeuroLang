# NeuroLang CMake Build Profile
# Copyright (c) Kiruse. See license in LICENSE.txt, or online at http://neuromancy.kirusifix.com/license.

cmake_minimum_required(VERSION 3.10.0)
project(NeuroLang VERSION 0.1.0)

# Options
option(BUILD_SHARED_LIBS "Build as shared libraries" ON)
set(CMAKE_CXX_STANDARD 17)

include_directories("Include")

# ------------------------------------------------------------------------------
# Target: NeuroRT
add_library(NeuroRT "")
target_include_directories(NeuroRT PRIVATE "Include/Neuro/Runtime")

file(GLOB_RECURSE INCLUDES_RT_CXX "Include/Runtime/*.hpp")
file(GLOB_RECURSE INCLUDES_RT_C "Include/Runtime/*.h")
file(GLOB_RECURSE SOURCES_RT "Source/Runtime/*.cpp")
target_sources(NeuroRT PUBLIC ${INCLUDES_RT_CXX})
target_sources(NeuroRT PUBLIC ${INCLUDES_RT_C})
target_sources(NeuroRT PRIVATE ${SOURCES_RT})


# ------------------------------------------------------------------------------
# Target: NeuroLang
add_library(NeuroLang "")
target_include_directories(NeuroLang PRIVATE "Include/Neuro/Lang")
target_include_directories(NeuroLang PUBLIC "Include/Neuro/Runtime")

file(GLOB_RECURSE INCLUDES_LANG "Include/Lang/*.hpp")
file(GLOB_RECURSE SOURCES_LANG "Source/Lang/*.cpp")
target_sources(NeuroLang PUBLIC ${INCLUDES_LANG})
target_sources(NeuroLang PRIVATE ${SOURCES_LANG})

target_link_libraries(NeuroLang NeuroRT)


# ------------------------------------------------------------------------------
# Target: NeuroTest
add_executable(NeuroTest "")
target_include_directories(NeuroTest PRIVATE "Test")
target_include_directories(NeuroTest PRIVATE "Include/Neuro/Runtime")

file(GLOB_RECURSE INCLUDES_TEST "Test/*.hpp")
file(GLOB_RECURSE SOURCES_TEST "Test/*.cpp")
target_sources(NeuroTest PRIVATE ${INCLUDES_TEST})
target_sources(NeuroTest PRIVATE ${SOURCES_TEST})

target_link_libraries(NeuroTest NeuroRT)


# ------------------------------------------------------------------------------
# Compilation Definitions
target_compile_definitions(NeuroLang PRIVATE BUILD_NEURO_API)
target_compile_definitions(NeuroRT PRIVATE BUILD_NEURO_API)

if(BUILD_SHARED_LIBS)
target_compile_definitions(NeuroLang PRIVATE BUILD_NEURO_SHARED)
target_compile_definitions(NeuroRT PRIVATE BUILD_NEURO_SHARED)
endif(BUILD_SHARED_LIBS)


# ------------------------------------------------------------------------------
# CPack
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
