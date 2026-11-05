cmake_minimum_required(VERSION 3.16)

# OG3-Patch: AngelScript-Modding fuer Gothic 3 (og3_patch.dll + og3_patch_test).
# Eigenstaendig baubar (32-bit, Gothic 3 ist x86):
#   cmake -S . -B build32 -A Win32 && cmake --build build32 --config Release
# Oder als Unterprojekt per add_subdirectory(og3-patch); dann gelten die Variablen des Hauptprojekts.
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(OG3P_SUBPROJECT ON)
else()
    set(OG3P_SUBPROJECT OFF)
    project(og3_patch LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 17)
endif()

if(NOT WIN32)
    message(STATUS "og3-patch: nur Windows")
    return()
endif()

# MinHook: Standard ist ein vorhandener Ordner (OG3P_MINHOOK_DIR) mit src/ und include/, sonst Download.
set(OG3P_MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/minhook" CACHE PATH "MinHook-Quellbaum (src/, include/)")
if(NOT EXISTS ${OG3P_MINHOOK_DIR}/include/MinHook.h)
    include(FetchContent)
    FetchContent_Declare(og3p_minhook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git GIT_TAG v1.3.3)
    FetchContent_GetProperties(og3p_minhook)
    if(NOT og3p_minhook_POPULATED)
        FetchContent_Populate(og3p_minhook)
    endif()
    set(OG3P_MINHOOK_DIR ${og3p_minhook_SOURCE_DIR})
endif()

# AngelScript 2.38.0 (zlib): wie MinHook ein vorhandener Ordner (OG3P_ANGELSCRIPT_DIR, mit angelscript/ und add_on/), sonst Download mit Pruefsumme
set(OG3P_ANGELSCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/angelscript/sdk" CACHE PATH "AngelScript-SDK (angelscript/, add_on/)")
if(NOT EXISTS ${OG3P_ANGELSCRIPT_DIR}/angelscript/include/angelscript.h)
    include(FetchContent)
    FetchContent_Declare(og3p_angelscript
        URL https://www.angelcode.com/angelscript/sdk/files/angelscript_2.38.0.zip
        URL_HASH SHA256=b33b5dbcda10317ef67d628353d83246984ce6fcac102d4dc2aed121eba52e6f)
    FetchContent_GetProperties(og3p_angelscript)
    if(NOT og3p_angelscript_POPULATED)
        FetchContent_Populate(og3p_angelscript)
    endif()
    set(OG3P_ANGELSCRIPT_DIR ${og3p_angelscript_SOURCE_DIR})
endif()
set(OG3_AS_DIR ${OG3P_ANGELSCRIPT_DIR})
file(GLOB OG3_AS_SRCS ${OG3_AS_DIR}/angelscript/source/*.cpp)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    enable_language(ASM_MASM)
    list(APPEND OG3_AS_SRCS ${OG3_AS_DIR}/angelscript/source/as_callfunc_x64_msvc_asm.asm)
endif()
list(APPEND OG3_AS_SRCS ${OG3_AS_DIR}/add_on/scriptbuilder/scriptbuilder.cpp ${OG3_AS_DIR}/add_on/scriptarray/scriptarray.cpp
                        ${OG3_AS_DIR}/add_on/scriptmath/scriptmath.cpp)
add_library(og3_angelscript STATIC ${OG3_AS_SRCS})
target_include_directories(og3_angelscript PUBLIC ${OG3_AS_DIR}/angelscript/include ${OG3_AS_DIR}/add_on)
target_compile_definitions(og3_angelscript PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(og3_angelscript PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded)

enable_language(C)
file(GLOB OG3P_MINHOOK_SRCS ${OG3P_MINHOOK_DIR}/src/*.c ${OG3P_MINHOOK_DIR}/src/hde/*.c)
set(OG3P_SRCS src/patch_main.cpp src/script_host.cpp src/sb_api.cpp src/engine_bindings.cpp src/game_hooks.cpp
              src/quest_bindings.cpp src/dialog_bindings.cpp src/file_patches.cpp src/pack_mod.cpp)
list(TRANSFORM OG3P_SRCS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
add_library(og3_patch SHARED ${OG3P_SRCS} ${OG3P_MINHOOK_SRCS})
target_include_directories(og3_patch PRIVATE src ${OG3P_MINHOOK_DIR}/include)
target_link_libraries(og3_patch PRIVATE og3_angelscript)
# Map-Datei neben der DLL (og3_patch.map): ordnet Adressen aus og3_hooks.log ("og3_patch.dll+0x...") Funktionen zu
target_link_options(og3_patch PRIVATE /MAP)

add_executable(og3_patch_test tests/test_runner.cpp)
# Kommandozeilenwerkzeug zum Packen/Pruefen von Mod-Archiven (Gegenstueck zu G3PakDir), teilt sich src/pack_mod.cpp mit der DLL
add_executable(og3_packmod tools/packmod_main.cpp src/pack_mod.cpp)

foreach(t og3_patch og3_patch_test og3_packmod)
    set_target_properties(${t} PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/patch)
endforeach()
