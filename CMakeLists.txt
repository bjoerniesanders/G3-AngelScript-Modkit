cmake_minimum_required(VERSION 3.16)

# G3ASM: AngelScript-Modding fuer Gothic 3 (g3asm.dll + g3asm_test).
# Eigenstaendig baubar (32-bit, Gothic 3 ist x86):
#   cmake -S . -B build32 -A Win32 && cmake --build build32 --config Release
# Oder als Unterprojekt per add_subdirectory(g3-angelscript-modkit); dann gelten die Variablen des Hauptprojekts.
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(G3ASM_SUBPROJECT ON)
else()
    set(G3ASM_SUBPROJECT OFF)
    project(g3asm LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 17)
endif()

if(NOT WIN32)
    message(STATUS "g3-angelscript-modkit: nur Windows")
    return()
endif()

# MinHook: Standard ist ein vorhandener Ordner (G3ASM_MINHOOK_DIR) mit src/ und include/, sonst Download.
set(G3ASM_MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/minhook" CACHE PATH "MinHook-Quellbaum (src/, include/)")
if(NOT EXISTS ${G3ASM_MINHOOK_DIR}/include/MinHook.h)
    include(FetchContent)
    FetchContent_Declare(g3asm_minhook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git GIT_TAG v1.3.3)
    FetchContent_GetProperties(g3asm_minhook)
    if(NOT g3asm_minhook_POPULATED)
        FetchContent_Populate(g3asm_minhook)
    endif()
    set(G3ASM_MINHOOK_DIR ${g3asm_minhook_SOURCE_DIR})
endif()

# AngelScript 2.38.0 (zlib): wie MinHook ein vorhandener Ordner (G3ASM_ANGELSCRIPT_DIR, mit angelscript/ und add_on/), sonst Download mit Pruefsumme
set(G3ASM_ANGELSCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/angelscript/sdk" CACHE PATH "AngelScript-SDK (angelscript/, add_on/)")
if(NOT EXISTS ${G3ASM_ANGELSCRIPT_DIR}/angelscript/include/angelscript.h)
    include(FetchContent)
    FetchContent_Declare(g3asm_angelscript
        URL https://www.angelcode.com/angelscript/sdk/files/angelscript_2.38.0.zip
        URL_HASH SHA256=b33b5dbcda10317ef67d628353d83246984ce6fcac102d4dc2aed121eba52e6f)
    FetchContent_GetProperties(g3asm_angelscript)
    if(NOT g3asm_angelscript_POPULATED)
        FetchContent_Populate(g3asm_angelscript)
    endif()
    set(G3ASM_ANGELSCRIPT_DIR ${g3asm_angelscript_SOURCE_DIR})
endif()
set(G3ASM_AS_DIR ${G3ASM_ANGELSCRIPT_DIR})
file(GLOB G3ASM_AS_SRCS ${G3ASM_AS_DIR}/angelscript/source/*.cpp)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    enable_language(ASM_MASM)
    list(APPEND G3ASM_AS_SRCS ${G3ASM_AS_DIR}/angelscript/source/as_callfunc_x64_msvc_asm.asm)
endif()
list(APPEND G3ASM_AS_SRCS ${G3ASM_AS_DIR}/add_on/scriptbuilder/scriptbuilder.cpp ${G3ASM_AS_DIR}/add_on/scriptarray/scriptarray.cpp
                        ${G3ASM_AS_DIR}/add_on/scriptmath/scriptmath.cpp)
add_library(g3asm_angelscript STATIC ${G3ASM_AS_SRCS})
target_include_directories(g3asm_angelscript PUBLIC ${G3ASM_AS_DIR}/angelscript/include ${G3ASM_AS_DIR}/add_on)
# CLEAR_FPU_STACK=emms: AngelScript setzt vor jedem C++-Aufruf sonst per fninit das x87-Steuerwort zurueck (037F statt Genome 007F = 24 Bit).
# Folge war: verwaschene Gebaeude-Texturen nach dem Laden (gefunden 2026-11-06). emms leert nur den FPU-Stapel und laesst das Steuerwort stehen.
target_compile_definitions(g3asm_angelscript PRIVATE _CRT_SECURE_NO_WARNINGS CLEAR_FPU_STACK=emms)
set_target_properties(g3asm_angelscript PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded)

enable_language(C)
file(GLOB G3ASM_MINHOOK_SRCS ${G3ASM_MINHOOK_DIR}/src/*.c ${G3ASM_MINHOOK_DIR}/src/hde/*.c)
set(G3ASM_SRCS src/patch_main.cpp src/script_host.cpp src/sb_api.cpp src/engine_bindings.cpp src/game_hooks.cpp
              src/quest_bindings.cpp src/dialog_bindings.cpp src/file_patches.cpp src/pack_mod.cpp src/script_calls.cpp src/draw_api.cpp src/more_events.cpp src/combat_patches.cpp src/schema_dump.cpp src/ai_trace.cpp src/mod_settings.cpp)
list(TRANSFORM G3ASM_SRCS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
add_library(g3asm SHARED ${G3ASM_SRCS} ${G3ASM_MINHOOK_SRCS})
target_include_directories(g3asm PRIVATE src ${G3ASM_MINHOOK_DIR}/include)
target_link_libraries(g3asm PRIVATE g3asm_angelscript)
# Map-Datei neben der DLL (g3asm.map): ordnet Adressen aus og3_hooks.log ("g3asm.dll+0x...") Funktionen zu
target_link_options(g3asm PRIVATE /MAP)

add_executable(g3asm_test tests/test_runner.cpp)
# Kommandozeilenwerkzeug zum Packen/Pruefen von Mod-Archiven (Gegenstueck zu G3PakDir), teilt sich src/pack_mod.cpp mit der DLL
add_executable(g3asm_packmod tools/packmod_main.cpp src/pack_mod.cpp)

foreach(t g3asm g3asm_test g3asm_packmod)
    set_target_properties(${t} PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/patch)
endforeach()

# Schlanker Loader: Ersatz-dbghelp.dll, die g3asm.dll laedt (eigenstaendig, ohne den Proxy des Hauptrepos). Ausgabe: build32/loader/dbghelp.dll
add_library(g3asm_loader SHARED src/loader/loader.cpp src/loader/loader.def)
set_target_properties(g3asm_loader PROPERTIES OUTPUT_NAME dbghelp MSVC_RUNTIME_LIBRARY MultiThreaded
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/loader RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/loader
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/loader)
# Ersatz-SharedBase.dll fuer Tests ohne Spieldateien (CI): <build>/sbstub/SharedBase.dll
add_library(g3asm_sb_stub SHARED tests/sb_stub/sb_stub.cpp)
target_link_libraries(g3asm_sb_stub PRIVATE dbghelp)
set_target_properties(g3asm_sb_stub PROPERTIES OUTPUT_NAME SharedBase MSVC_RUNTIME_LIBRARY MultiThreaded
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/sbstub RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/sbstub
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sbstub)
add_executable(g3asm_loader_test tests/loader_test.cpp)
set_target_properties(g3asm_loader_test PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/patch)
