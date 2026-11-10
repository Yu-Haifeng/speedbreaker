// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Which release of the game runs: the USA disc's default.xex, the European
// (PAL, Spanish) one, the European (PAL, German) one or the Asian (NTSC)
// one, chosen at startup by the installed default.xex's SHA-256
// (install/manifest.h) before any guest code runs.
//
// Another release's default.xex is the USA build's source built for its
// market: the same functions but some changed, laid out in pieces shifted
// from the USA's. Each release's own recompiled code (ppc-pal-es/,
// ppc-pal-de/ or ppc-ntsc-asia/, generated from the player's own disc with
// config/pal-es/, config/pal-de/ or config/ntsc-asia/, put in namespace
// pal_es, pal_de or ntsc_asia by scripts/region_ppc.py) links beside the
// USA's (ppc/) when the build has it (SB_PAL_ES=1, SB_PAL_DE=1,
// SB_NTSC_ASIA=1). The runtime is written against the USA addresses:
//   - a hook is SB_HOOK(sub_X) { ... SB_ORIGINAL(sub_X)(ctx, base); ... }:
//     each regional code's calls to its own copy of the function reach the
//     same hook (a forwarder overrides the regional copy), and SB_ORIGINAL
//     calls the running region's recompiled body;
//   - a guest address the runtime names (a global, a return address, a
//     constant) goes through region::Addr(usa), the running region's
//     (pal_es_addresses.inc, pal_de_addresses.inc, ntsc_asia_addresses.inc:
//     survey/region_map.py maps every one the sources name and checks the
//     regional bytes there match; tests/region_addresses_check.py keeps each
//     table complete).
// With the USA release running, Addr is the address itself and SB_ORIGINAL the
// USA body: the USA game behaves exactly as before.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

struct PPCFuncMapping;

namespace region
{
    enum class Release : uint8_t
    {
        Usa,
        PalEs,
        PalDe,
        NtscAsia,
    };

    namespace detail
    {
        inline Release g_release = Release::Usa;
        // The running regional release's address for `usa` (its table).
        uint32_t RegionalAddr(uint32_t usa);
    }

    inline Release Current()
    {
        return detail::g_release;
    }

    inline bool IsPalEs()
    {
        return detail::g_release == Release::PalEs;
    }

    inline bool IsPalDe()
    {
        return detail::g_release == Release::PalDe;
    }

    inline bool IsNtscAsia()
    {
        return detail::g_release == Release::NtscAsia;
    }

    // A European (PAL) executable: it boots in its disc's own language and
    // asks for "_pal" movies itself (game/language.h).
    inline bool IsPal()
    {
        return detail::g_release == Release::PalEs || detail::g_release == Release::PalDe;
    }

    // Any executable but the USA's: its addresses come from its own table,
    // and its boot language can be replaced by the chosen one (the Asian
    // build boots in Chinese, the PAL ones in their disc's language).
    inline bool IsRegional()
    {
        return detail::g_release != Release::Usa;
    }

    // Whether this build has the release's recompiled code.
    bool Available(Release release);

    // "usa", "pal-es", "pal-de", "ntsc-asia" (install/manifest.h's region
    // ids); "USA", "Europe (PAL, Spanish)", "Europe (PAL, German)",
    // "Asia (NTSC)".
    std::string_view Id(Release release);
    std::string_view Name(Release release);
    // The release with that manifest region id, if any.
    bool FromId(std::string_view id, Release& release);

    // Before any guest code runs (main.cpp, after reading default.xex), and
    // only for an Available release: switches the indirect-call table
    // (kernel/memory.cpp) to the release's functions.
    void Select(Release release);

    // The running release's guest address for a USA one the runtime names.
    // Every address the sources name is in each regional table; one that isn't
    // (a computed address passed by mistake) is logged once and returned as is.
    inline uint32_t Addr(uint32_t usa)
    {
        return IsRegional() ? detail::RegionalAddr(usa) : usa;
    }

    // The running release's recompiled functions (guest -> host), and its
    // code's extent (the indirect-call table covers it).
    const PPCFuncMapping* FuncMappings();
    uint64_t CodeBase();
    uint64_t CodeSize();

    // The regional tables themselves (tests). The German one is empty in a
    // build without its code (no SB_PAL_DE).
    struct AddressPair
    {
        uint32_t usa, regional;
    };
    const AddressPair* PalEsTable(size_t& count);
    const AddressPair* PalDeTable(size_t& count);
    const AddressPair* NtscAsiaTable(size_t& count);
}

#define SB_CAT2(a, b) a##b
#define SB_CAT(a, b) SB_CAT2(a, b)

// Per regional release: SB_<R>_sub_<USA> -> sub_<REGIONAL> for every function
// the runtime names (pal_<r>_names.h, ntsc_asia_names.h, written by
// survey/region_map.py), the forwarder that makes that release's own copy of
// a hooked function (weak) call the hook, and its part of SB_ORIGINAL's
// choice.
#if SB_PAL_ES
#include "pal_es_names.h"
#define SB_PAL_ES_SUB(name) SB_CAT(SB_PAL_ES_, name)
#define SB_PAL_ES_IMP(name) SB_CAT(__imp__, SB_PAL_ES_SUB(name))
#define SB_PAL_ES_FORWARD(name) \
    namespace pal_es \
    { \
        PPC_FUNC(SB_PAL_ES_IMP(name)); \
        PPC_FUNC(SB_PAL_ES_SUB(name)) { ::name(ctx, base); } \
    }
#define SB_PAL_ES_ORIGINAL(name) ::region::IsPalEs() ? pal_es::SB_PAL_ES_IMP(name) :
#else
#define SB_PAL_ES_FORWARD(name)
#define SB_PAL_ES_ORIGINAL(name)
#endif

#if SB_PAL_DE
#include "pal_de_names.h"
#define SB_PAL_DE_SUB(name) SB_CAT(SB_PAL_DE_, name)
#define SB_PAL_DE_IMP(name) SB_CAT(__imp__, SB_PAL_DE_SUB(name))
#define SB_PAL_DE_FORWARD(name) \
    namespace pal_de \
    { \
        PPC_FUNC(SB_PAL_DE_IMP(name)); \
        PPC_FUNC(SB_PAL_DE_SUB(name)) { ::name(ctx, base); } \
    }
#define SB_PAL_DE_ORIGINAL(name) ::region::IsPalDe() ? pal_de::SB_PAL_DE_IMP(name) :
#else
#define SB_PAL_DE_FORWARD(name)
#define SB_PAL_DE_ORIGINAL(name)
#endif

#if SB_NTSC_ASIA
#include "ntsc_asia_names.h"
#define SB_NTSC_ASIA_SUB(name) SB_CAT(SB_NTSC_ASIA_, name)
#define SB_NTSC_ASIA_IMP(name) SB_CAT(__imp__, SB_NTSC_ASIA_SUB(name))
#define SB_NTSC_ASIA_FORWARD(name) \
    namespace ntsc_asia \
    { \
        PPC_FUNC(SB_NTSC_ASIA_IMP(name)); \
        PPC_FUNC(SB_NTSC_ASIA_SUB(name)) { ::name(ctx, base); } \
    }
#define SB_NTSC_ASIA_ORIGINAL(name) ::region::IsNtscAsia() ? ntsc_asia::SB_NTSC_ASIA_IMP(name) :
#else
#define SB_NTSC_ASIA_FORWARD(name)
#define SB_NTSC_ASIA_ORIGINAL(name)
#endif

#if SB_PAL_ES || SB_PAL_DE || SB_NTSC_ASIA
// A hook of the recompiled function `name` (sub_<USA address>), followed by
// its body: declares the original in every release, and makes each regional
// code's own copy of the function (weak) call this hook.
#define SB_HOOK(name) \
    extern "C" PPC_FUNC(__imp__##name); \
    PPC_FUNC(name); \
    SB_PAL_ES_FORWARD(name) \
    SB_PAL_DE_FORWARD(name) \
    SB_NTSC_ASIA_FORWARD(name) \
    PPC_FUNC(name)

// The running release's recompiled body of the hooked function `name`.
#define SB_ORIGINAL(name) (SB_PAL_ES_ORIGINAL(name) SB_PAL_DE_ORIGINAL(name) SB_NTSC_ASIA_ORIGINAL(name) __imp__##name)
#else
#define SB_HOOK(name) \
    extern "C" PPC_FUNC(__imp__##name); \
    PPC_FUNC(name)
#define SB_ORIGINAL(name) __imp__##name
#endif
