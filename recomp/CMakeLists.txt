# Compiles XenonRecomp's output (../ppc, generated from the user's own XEX and
# never committed) into a static library. Regenerate ppc/ with:
#   tools/XenonRecomp/build/XenonRecomp/XenonRecomp config/nfsmw.toml tools/XenonRecomp/XenonUtils/ppc_context.h
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
# Built from the top-level CMakeLists.txt.
file(GLOB PPC_SOURCES ${ROOT}/ppc/*.cpp)
if(NOT PPC_SOURCES)
    message(FATAL_ERROR "No generated sources in ppc/. Run XenonRecomp first.")
endif()

add_library(NfsmwRecompLib STATIC ${PPC_SOURCES})
target_compile_features(NfsmwRecompLib PUBLIC cxx_std_20)
target_include_directories(NfsmwRecompLib PUBLIC
    ${ROOT}/ppc
    ${ROOT}/tools/XenonRecomp/XenonUtils
    ${ROOT}/tools/XenonRecomp/thirdparty/simde)
target_compile_options(NfsmwRecompLib PRIVATE
    -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable
    -fno-strict-aliasing)
if(SB_MSVC_FRONTEND)
    # clang-cl has no -include: /FI, in the same order. The third, on
    # Windows only, aligns the guest's jmp_bufs for the CRT's setjmp
    # (runtime/platform/win32/guest_setjmp.h).
    target_compile_options(NfsmwRecompLib PRIVATE
        "/FI${ROOT}/runtime/cpu/indirect.h"
        "/FI${ROOT}/runtime/cpu/guest_time.h"
        "/FI${ROOT}/runtime/platform/win32/guest_setjmp.h")
else()
    target_compile_options(NfsmwRecompLib PRIVATE
        # Checked indirect calls (runtime/cpu/indirect.cpp). SHELL: keeps each
        # -include with its file (CMake drops a repeated plain -include).
        "SHELL:-include \"${ROOT}/runtime/cpu/indirect.h\""
        # mftb reads the game's clock, which stands still while it's suspended.
        "SHELL:-include \"${ROOT}/runtime/cpu/guest_time.h\"")
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    if(SB_MSVC_FRONTEND)
        target_compile_options(NfsmwRecompLib PRIVATE /clang:-march=x86-64-v3)
    else()
        target_compile_options(NfsmwRecompLib PRIVATE -march=x86-64-v3)
    endif()
endif()
if(IOS)
    target_compile_options(NfsmwRecompLib PRIVATE -mcpu=apple-a15)
endif()

# The European (PAL, Spanish) release's code (../ppc-pal-es: XenonRecomp with
# config/pal-es/nfsmw.toml on that disc's default.xex, then
# scripts/region_ppc.py ppc-pal-es pal_es), linked beside the USA's under
# namespace pal_es; the runtime picks one at startup (runtime/region/region.h).
# SB_PAL_ES: AUTO (when ppc-pal-es/ is there), ON (required) or OFF.
set(SB_PAL_ES AUTO CACHE STRING "Link the PAL (Spanish) release's recompiled code: AUTO, ON or OFF")
file(GLOB PPC_PAL_ES_SOURCES ${ROOT}/ppc-pal-es/*.cpp)
if(SB_PAL_ES STREQUAL "ON" AND NOT PPC_PAL_ES_SOURCES)
    message(FATAL_ERROR "SB_PAL_ES=ON, but ppc-pal-es/ has no generated sources")
endif()
if(PPC_PAL_ES_SOURCES AND NOT SB_PAL_ES STREQUAL "OFF")
    file(READ ${ROOT}/ppc-pal-es/ppc_recomp_shared.h _sb_pal_head LIMIT 64)
    if(NOT _sb_pal_head MATCHES "^// Namespaced by scripts/region_ppc.py: pal_es")
        message(FATAL_ERROR "ppc-pal-es/ isn't namespaced: run python3 scripts/region_ppc.py ppc-pal-es pal_es")
    endif()
    add_library(NfsmwRecompPalEsLib STATIC ${PPC_PAL_ES_SOURCES})
    target_compile_features(NfsmwRecompPalEsLib PUBLIC cxx_std_20)
    target_include_directories(NfsmwRecompPalEsLib PRIVATE
        ${ROOT}/ppc-pal-es
        ${ROOT}/tools/XenonRecomp/XenonUtils
        ${ROOT}/tools/XenonRecomp/thirdparty/simde)
    get_target_property(_sb_ppc_options NfsmwRecompLib COMPILE_OPTIONS)
    target_compile_options(NfsmwRecompPalEsLib PRIVATE ${_sb_ppc_options})
    set(SB_HAVE_PAL_ES ON CACHE INTERNAL "ppc-pal-es is linked")
    message(STATUS "PAL (Spanish) release: linked (ppc-pal-es/)")
else()
    set(SB_HAVE_PAL_ES OFF CACHE INTERNAL "ppc-pal-es is linked")
    message(STATUS "PAL (Spanish) release: not linked (no ppc-pal-es/, or SB_PAL_ES=OFF)")
endif()

# The European (PAL, German) release's code, the same way (../ppc-pal-de:
# XenonRecomp with config/pal-de/nfsmw.toml on that disc's default.xex, then
# scripts/region_ppc.py ppc-pal-de pal_de). Its address table and names
# (runtime/region/pal_de_addresses.inc, pal_de_names.h; docs/REGION_PAL_DE.md)
# must be there too.
# SB_PAL_DE: AUTO (when ppc-pal-de/ is there), ON (required) or OFF.
set(SB_PAL_DE AUTO CACHE STRING "Link the PAL (German) release's recompiled code: AUTO, ON or OFF")
file(GLOB PPC_PAL_DE_SOURCES ${ROOT}/ppc-pal-de/*.cpp)
if(SB_PAL_DE STREQUAL "ON" AND NOT PPC_PAL_DE_SOURCES)
    message(FATAL_ERROR "SB_PAL_DE=ON, but ppc-pal-de/ has no generated sources")
endif()
if(PPC_PAL_DE_SOURCES AND NOT SB_PAL_DE STREQUAL "OFF")
    file(READ ${ROOT}/ppc-pal-de/ppc_recomp_shared.h _sb_pal_de_head LIMIT 64)
    if(NOT _sb_pal_de_head MATCHES "^// Namespaced by scripts/region_ppc.py: pal_de")
        message(FATAL_ERROR "ppc-pal-de/ isn't namespaced: run python3 scripts/region_ppc.py ppc-pal-de pal_de")
    endif()
    foreach(_sb_generated pal_de_addresses.inc pal_de_names.h)
        if(NOT EXISTS ${ROOT}/runtime/region/${_sb_generated})
            message(FATAL_ERROR "ppc-pal-de/ is there, but runtime/region/${_sb_generated} isn't: run "
                "python3 -I survey/region_map.py table --region pal-de <usa dir> <de dir> <funcmap.tsv> (docs/REGION_PAL_DE.md)")
        endif()
    endforeach()
    add_library(NfsmwRecompPalDeLib STATIC ${PPC_PAL_DE_SOURCES})
    target_compile_features(NfsmwRecompPalDeLib PUBLIC cxx_std_20)
    target_include_directories(NfsmwRecompPalDeLib PRIVATE
        ${ROOT}/ppc-pal-de
        ${ROOT}/tools/XenonRecomp/XenonUtils
        ${ROOT}/tools/XenonRecomp/thirdparty/simde)
    get_target_property(_sb_ppc_options NfsmwRecompLib COMPILE_OPTIONS)
    target_compile_options(NfsmwRecompPalDeLib PRIVATE ${_sb_ppc_options})
    set(SB_HAVE_PAL_DE ON CACHE INTERNAL "ppc-pal-de is linked")
    message(STATUS "PAL (German) release: linked (ppc-pal-de/)")
else()
    set(SB_HAVE_PAL_DE OFF CACHE INTERNAL "ppc-pal-de is linked")
    message(STATUS "PAL (German) release: not linked (no ppc-pal-de/, or SB_PAL_DE=OFF)")
endif()

# The Asian (NTSC) release's code, the same way (../ppc-ntsc-asia:
# XenonRecomp with config/ntsc-asia/nfsmw.toml on that disc's default.xex,
# then scripts/region_ppc.py ppc-ntsc-asia ntsc_asia). Its address table and
# names (runtime/region/ntsc_asia_addresses.inc, ntsc_asia_names.h;
# docs/REGION_NTSC_ASIA.md) must be there too.
# SB_NTSC_ASIA: AUTO (when ppc-ntsc-asia/ is there), ON (required) or OFF.
set(SB_NTSC_ASIA AUTO CACHE STRING "Link the Asian (NTSC) release's recompiled code: AUTO, ON or OFF")
file(GLOB PPC_NTSC_ASIA_SOURCES ${ROOT}/ppc-ntsc-asia/*.cpp)
if(SB_NTSC_ASIA STREQUAL "ON" AND NOT PPC_NTSC_ASIA_SOURCES)
    message(FATAL_ERROR "SB_NTSC_ASIA=ON, but ppc-ntsc-asia/ has no generated sources")
endif()
if(PPC_NTSC_ASIA_SOURCES AND NOT SB_NTSC_ASIA STREQUAL "OFF")
    file(READ ${ROOT}/ppc-ntsc-asia/ppc_recomp_shared.h _sb_ntsc_asia_head LIMIT 64)
    if(NOT _sb_ntsc_asia_head MATCHES "^// Namespaced by scripts/region_ppc.py: ntsc_asia")
        message(FATAL_ERROR "ppc-ntsc-asia/ isn't namespaced: run python3 scripts/region_ppc.py ppc-ntsc-asia ntsc_asia")
    endif()
    foreach(_sb_generated ntsc_asia_addresses.inc ntsc_asia_names.h)
        if(NOT EXISTS ${ROOT}/runtime/region/${_sb_generated})
            message(FATAL_ERROR "ppc-ntsc-asia/ is there, but runtime/region/${_sb_generated} isn't: run "
                "python3 -I survey/region_map.py table --region ntsc-asia <usa dir> <asia dir> <funcmap.tsv> (docs/REGION_NTSC_ASIA.md)")
        endif()
    endforeach()
    add_library(NfsmwRecompNtscAsiaLib STATIC ${PPC_NTSC_ASIA_SOURCES})
    target_compile_features(NfsmwRecompNtscAsiaLib PUBLIC cxx_std_20)
    target_include_directories(NfsmwRecompNtscAsiaLib PRIVATE
        ${ROOT}/ppc-ntsc-asia
        ${ROOT}/tools/XenonRecomp/XenonUtils
        ${ROOT}/tools/XenonRecomp/thirdparty/simde)
    get_target_property(_sb_ppc_options NfsmwRecompLib COMPILE_OPTIONS)
    target_compile_options(NfsmwRecompNtscAsiaLib PRIVATE ${_sb_ppc_options})
    set(SB_HAVE_NTSC_ASIA ON CACHE INTERNAL "ppc-ntsc-asia is linked")
    message(STATUS "Asian (NTSC) release: linked (ppc-ntsc-asia/)")
else()
    set(SB_HAVE_NTSC_ASIA OFF CACHE INTERNAL "ppc-ntsc-asia is linked")
    message(STATUS "Asian (NTSC) release: not linked (no ppc-ntsc-asia/, or SB_NTSC_ASIA=OFF)")
endif()
