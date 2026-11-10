// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See manifest.h.
#include "manifest.h"

#include <cassert>
#include <iterator>

// The German and Asian discs' lists are always checked (static_asserts
// below), but each is a game manifest only in a build that links its code
// (SB_PAL_DE, SB_NTSC_ASIA; runtime/CMakeLists.txt): without it, that disc is
// refused up front (ReleaseWithoutCode) rather than installed and then not
// run.
#ifndef SB_PAL_DE
#define SB_PAL_DE 0
#endif
#ifndef SB_NTSC_ASIA
#define SB_NTSC_ASIA 0
#endif

namespace install
{
    namespace
    {
        // One namespace per region: its files, then its MANIFEST_INFO.
#define MANIFEST_INFO(version, titleId, count, bytes)
#define MANIFEST_FILE(path, size, sha256) ManifestFile{ path, size, sha256 },
        namespace usa
        {
            constexpr ManifestFile s_files[] = {
#include "manifest.inc"
            };
        }
        namespace pal_es
        {
            constexpr ManifestFile s_files[] = {
#include "manifest_pal_es.inc"
            };
        }
        namespace pal_de
        {
            constexpr ManifestFile s_files[] = {
#include "manifest_pal_de.inc"
            };
        }
        namespace ntsc_asia
        {
            constexpr ManifestFile s_files[] = {
#include "manifest_ntsc_asia.inc"
            };
        }
#undef MANIFEST_INFO
#undef MANIFEST_FILE

#define MANIFEST_INFO(version, titleId, count, bytes) \
        constexpr std::string_view s_version = version; \
        constexpr uint32_t s_titleId = titleId; \
        constexpr size_t s_count = count; \
        constexpr uint64_t s_bytes = bytes;
#define MANIFEST_FILE(path, size, sha256)
        namespace usa
        {
#include "manifest.inc"
        }
        namespace pal_es
        {
#include "manifest_pal_es.inc"
        }
        namespace pal_de
        {
#include "manifest_pal_de.inc"
        }
        namespace ntsc_asia
        {
#include "manifest_ntsc_asia.inc"
        }
#undef MANIFEST_INFO
#undef MANIFEST_FILE

        template <size_t N>
        constexpr uint64_t SumSizes(const ManifestFile (&files)[N])
        {
            uint64_t sum = 0;
            for (const ManifestFile& f : files)
                sum += f.size;
            return sum;
        }

        template <size_t N>
        constexpr const ManifestFile* XexOf(const ManifestFile (&files)[N])
        {
            for (const ManifestFile& f : files)
                if (f.path == "default.xex")
                    return &f;
            return nullptr;
        }

        // A hand-edited or truncated .inc fails the build, not an install.
        static_assert(std::size(usa::s_files) == usa::s_count, "manifest.inc: file count does not match MANIFEST_INFO");
        static_assert(SumSizes(usa::s_files) == usa::s_bytes, "manifest.inc: sizes do not add up to MANIFEST_INFO's total");
        static_assert(XexOf(usa::s_files), "manifest.inc: no default.xex entry");
        static_assert(std::size(pal_es::s_files) == pal_es::s_count, "manifest_pal_es.inc: file count does not match MANIFEST_INFO");
        static_assert(SumSizes(pal_es::s_files) == pal_es::s_bytes, "manifest_pal_es.inc: sizes do not add up to MANIFEST_INFO's total");
        static_assert(XexOf(pal_es::s_files), "manifest_pal_es.inc: no default.xex entry");
        // The regions are told apart by default.xex's hash and by the version
        // an install's marker records. Sizes need not differ: the Asian
        // executable is the same size as the Spanish one, so a place that
        // identifies an install by size alone has to hash that size.
        static_assert(XexOf(usa::s_files)->sha256 != XexOf(pal_es::s_files)->sha256, "two regions with one default.xex");
        static_assert(usa::s_version != pal_es::s_version, "two regions with one manifest version");
        static_assert(std::size(pal_de::s_files) == pal_de::s_count, "manifest_pal_de.inc: file count does not match MANIFEST_INFO");
        static_assert(SumSizes(pal_de::s_files) == pal_de::s_bytes, "manifest_pal_de.inc: sizes do not add up to MANIFEST_INFO's total");
        static_assert(XexOf(pal_de::s_files), "manifest_pal_de.inc: no default.xex entry");
        static_assert(XexOf(pal_de::s_files)->sha256 != XexOf(usa::s_files)->sha256
            && XexOf(pal_de::s_files)->sha256 != XexOf(pal_es::s_files)->sha256, "two regions with one default.xex");
        static_assert(pal_de::s_version != usa::s_version && pal_de::s_version != pal_es::s_version,
            "two regions with one manifest version");
        static_assert(std::size(ntsc_asia::s_files) == ntsc_asia::s_count,
            "manifest_ntsc_asia.inc: file count does not match MANIFEST_INFO");
        static_assert(SumSizes(ntsc_asia::s_files) == ntsc_asia::s_bytes,
            "manifest_ntsc_asia.inc: sizes do not add up to MANIFEST_INFO's total");
        static_assert(XexOf(ntsc_asia::s_files), "manifest_ntsc_asia.inc: no default.xex entry");
        static_assert(XexOf(ntsc_asia::s_files)->sha256 != XexOf(usa::s_files)->sha256
            && XexOf(ntsc_asia::s_files)->sha256 != XexOf(pal_es::s_files)->sha256
            && XexOf(ntsc_asia::s_files)->sha256 != XexOf(pal_de::s_files)->sha256, "two regions with one default.xex");
        static_assert(ntsc_asia::s_version != usa::s_version && ntsc_asia::s_version != pal_es::s_version
            && ntsc_asia::s_version != pal_de::s_version, "two regions with one manifest version");

        constexpr Manifest s_usa{ usa::s_version, usa::s_titleId, usa::s_files, usa::s_bytes, "usa", "USA" };
        constexpr Manifest s_palEs{ pal_es::s_version, pal_es::s_titleId, pal_es::s_files, pal_es::s_bytes, "pal-es",
            "Europe (PAL, Spanish)" };
#if SB_PAL_DE
        constexpr Manifest s_palDe{ pal_de::s_version, pal_de::s_titleId, pal_de::s_files, pal_de::s_bytes, "pal-de",
            "Europe (PAL, German)" };
#endif
#if SB_NTSC_ASIA
        constexpr Manifest s_ntscAsia{ ntsc_asia::s_version, ntsc_asia::s_titleId, ntsc_asia::s_files,
            ntsc_asia::s_bytes, "ntsc-asia", "Asia (NTSC)" };
#endif
        constexpr const Manifest* s_manifests[] = {
            &s_usa, &s_palEs,
#if SB_PAL_DE
            &s_palDe,
#endif
#if SB_NTSC_ASIA
            &s_ntscAsia,
#endif
        };

        char Lower(char c)
        {
            return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
        }
    }

    bool PathEqualsIgnoreCase(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); i++)
            if (Lower(a[i]) != Lower(b[i]))
                return false;
        return true;
    }

    const ManifestFile& Manifest::Xex() const
    {
        const ManifestFile* xex = Find("default.xex");
        assert(xex && "a manifest must list default.xex");
        return *xex;
    }

    const ManifestFile* Manifest::Find(std::string_view path) const
    {
        for (const ManifestFile& f : files)
            if (PathEqualsIgnoreCase(f.path, path))
                return &f;
        return nullptr;
    }

    const Manifest& GameManifest()
    {
        return s_usa;
    }

    std::span<const Manifest* const> GameManifests()
    {
        return s_manifests;
    }

    bool IsGameManifest(const Manifest& manifest)
    {
        for (const Manifest* m : s_manifests)
            if (m == &manifest)
                return true;
        return false;
    }

    const Manifest* ManifestForXexHash(std::string_view sha256)
    {
        for (const Manifest* m : s_manifests)
            if (m->Xex().sha256 == sha256)
                return m;
        return nullptr;
    }

    const Manifest* ManifestForXexSize(uint64_t size)
    {
        const Manifest* found = nullptr;
        for (const Manifest* m : s_manifests)
            if (m->Xex().size == size)
            {
                if (found)
                    return nullptr;  // two regions' executables of one size: hash it
                found = m;
            }
        return found;
    }

    const Manifest* ManifestForVersion(std::string_view version)
    {
        for (const Manifest* m : s_manifests)
            if (m->version == version)
                return m;
        return nullptr;
    }

    ReleaseWithoutCodeInfo ReleaseWithoutCode(std::string_view sha256)
    {
#if !SB_PAL_DE
        if (sha256 == XexOf(pal_de::s_files)->sha256)
            return { "Europe (PAL, German)", "German (PAL)", "ppc-pal-de" };
#endif
#if !SB_NTSC_ASIA
        if (sha256 == XexOf(ntsc_asia::s_files)->sha256)
            return { "Asia (NTSC)", "Asian (NTSC)", "ppc-ntsc-asia" };
#endif
        (void)sha256;
        return {};
    }
}
