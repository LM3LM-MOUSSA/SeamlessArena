cmake_minimum_required(VERSION 3.25)

project(SeamlessArena VERSION 0.1.0 LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "SeamlessArena currently supports Windows with the MSVC toolchain only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "SeamlessArena is an x64-only native DLL. Configure with -A x64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

option(SEAMLESSARENA_BUILD_NATIVE "Build the Mod Engine 3 native DLL" ON)
option(SEAMLESSARENA_BUILD_TESTS "Build the standalone match-rules test runner" ON)

set(SA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SEAMLESSARENA_OUTPUT_ROOT "${SA_ROOT}/bin/x64" CACHE PATH
    "Directory for x64 CMake build outputs (configuration subdirectories are added automatically)")
set(SA_BINARY_ROOT "${SEAMLESSARENA_OUTPUT_ROOT}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SA_BINARY_ROOT}/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${SA_BINARY_ROOT}/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${SA_BINARY_ROOT}/$<CONFIG>")

add_library(SeamlessArenaCore STATIC
    src/SeamlessArena.Core/src/MatchRules.cpp
)
add_library(SeamlessArena::Core ALIAS SeamlessArenaCore)
target_include_directories(SeamlessArenaCore
    PUBLIC "${SA_ROOT}/src/SeamlessArena.Core/include"
)
target_compile_definitions(SeamlessArenaCore PUBLIC _ITERATOR_DEBUG_LEVEL=0)
target_compile_options(SeamlessArenaCore PRIVATE /W4 /permissive- /sdl)

if(SEAMLESSARENA_BUILD_TESTS)
    add_executable(SeamlessArenaTests src/SeamlessArena.Tests/main.cpp)
    set_target_properties(SeamlessArenaTests PROPERTIES OUTPUT_NAME "SeamlessArena.Tests")
    target_link_libraries(SeamlessArenaTests PRIVATE SeamlessArena::Core)
    target_compile_definitions(SeamlessArenaTests PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    target_compile_options(SeamlessArenaTests PRIVATE /W4 /permissive- /sdl)
endif()

if(SEAMLESSARENA_BUILD_NATIVE)
    set(SA_MINHOOK_DIR "${SA_ROOT}/deps/minhook")
    set(SA_LIBER_DIR "${SA_ROOT}/docs/elden api/libER-main")

    if(NOT EXISTS "${SA_MINHOOK_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "MinHook source is missing at deps/minhook. Initialize the repository submodule before configuring.")
    endif()
    if(NOT EXISTS "${SA_LIBER_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "libER source is missing at docs/elden api/libER-main. Restore the checked-in dependency before configuring.")
    endif()

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled dependencies as static libraries" FORCE)
    add_subdirectory("${SA_MINHOOK_DIR}" "${CMAKE_BINARY_DIR}/deps/minhook")
    add_subdirectory("${SA_LIBER_DIR}" "${CMAKE_BINARY_DIR}/deps/libER")

    add_library(SeamlessArenaNative SHARED
        src/SeamlessArena.Native/dllmain.cpp
        src/SeamlessArena.Native/me3_entry.cpp
        src/SeamlessArena.Native/ArenaCoordinator.cpp
        src/SeamlessArena.Native/ArenaGameTask.cpp
        src/SeamlessArena.Native/ArenaNetProtocol.cpp
        src/SeamlessArena.Native/ConfigLoader.cpp
        src/SeamlessArena.Native/EngineBridge.cpp
        src/SeamlessArena.Native/HookManager.cpp
        src/SeamlessArena.Native/Logger.cpp
        src/SeamlessArena.Native/VersionGuard.cpp
    )
    set_target_properties(SeamlessArenaNative PROPERTIES OUTPUT_NAME "SeamlessArena")
    target_include_directories(SeamlessArenaNative PRIVATE
        "${SA_ROOT}/src/SeamlessArena.Native"
        "${SA_ROOT}/src/SeamlessArena.Core/include"
        "${SA_ROOT}/deps/minhook/include"
        "${SA_LIBER_DIR}/include"
        "${SA_LIBER_DIR}/symbols"
    )
    target_compile_definitions(SeamlessArenaNative PRIVATE
        WIN32_LEAN_AND_MEAN
        SEAMLESSARENA_EXPORTS
        _ITERATOR_DEBUG_LEVEL=0
    )
    target_compile_options(SeamlessArenaNative PRIVATE
        /W4 /permissive- /sdl /wd4100 /wd4458
    )
    target_link_libraries(SeamlessArenaNative PRIVATE
        SeamlessArena::Core
        minhook
        libER_static
    )
endif()
