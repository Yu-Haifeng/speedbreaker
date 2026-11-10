// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The known-good discs: every file's disc-relative path, size and SHA-256,
// generated from a verified dump by scripts/gen_install_manifest.py, one per
// supported release (region): manifest.inc (USA), manifest_pal_es.inc
// (Europe, the PAL disc with Spanish speech and movies), manifest_pal_de.inc
// (Europe, the German PAL disc) and manifest_ntsc_asia.inc (Asia). The last
// two are compiled in only by a build that links their code (SB_PAL_DE,
// SB_NTSC_ASIA): a build without it refuses that disc up front
// (ReleaseWithoutCode). Each region's code is recompiled from that one
// default.xex, so its hash doubles as the supported-version check and says
// which region a disc or install is (the title ID is the same in all; the
// Asian executable has the Spanish one's size, so a size alone is not enough).
//
// A compiled-in manifest (GameManifests()) passed to the installer's functions
// stands for all of them: they take the region from default.xex's hash or
// unique size; the marker separately records an install's manifest version.
// A test's own manifest is taken as it is.
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace install
{
    struct ManifestFile
    {
        std::string_view path;      // '/'-separated, in the dump's case (Movies/, NFS/, default.xex)
        uint64_t size;
        std::string_view sha256;    // lower-case hex
    };

    struct Manifest
    {
        std::string_view version;   // changes whenever the entries do; recorded in the install marker
        uint32_t titleId;           // from default.xex's execution info (0x454107D9)
        std::span<const ManifestFile> files;
        uint64_t totalBytes;
        std::string_view region = {};      // "usa", "pal-es", "pal-de", "ntsc-asia": recorded in the install marker
        std::string_view regionName = {};  // for the player: "USA", "Europe (PAL, ...)", "Asia (NTSC)"

        // The default.xex entry (the one file every manifest must have).
        const ManifestFile& Xex() const;
        // Case-insensitive; nullptr if absent.
        const ManifestFile* Find(std::string_view path) const;
    };

    // The USA manifest (manifest.inc), the default of every API that checks
    // files; they all take a Manifest so tests can pass a small one instead.
    const Manifest& GameManifest();

    // Every compiled-in manifest, USA first. Their default.xex hashes and
    // versions all differ (static_asserts in manifest.cpp); two may have the
    // same size (Spain and Asia).
    std::span<const Manifest* const> GameManifests();
    bool IsGameManifest(const Manifest& manifest);  // one of GameManifests()

    // The compiled-in manifest with that default.xex SHA-256 (lower-case hex),
    // default.xex size, or version; nullptr if none.
    const Manifest* ManifestForXexHash(std::string_view sha256);
    const Manifest* ManifestForXexSize(uint64_t size);
    const Manifest* ManifestForVersion(std::string_view version);

    // A release SpeedBreaker supports whose code this build doesn't link
    // (the German disc without SB_PAL_DE, the Asian one without
    // SB_NTSC_ASIA), by its default.xex SHA-256: its region name ("Asia
    // (NTSC)"), the disc's name in the installer's refusal ("Asian (NTSC)":
    // "the Asian (NTSC) disc") and the folder its code is generated into
    // ("ppc-ntsc-asia"). Empty when the hash is none of those.
    struct ReleaseWithoutCodeInfo
    {
        std::string_view regionName;
        std::string_view discName;
        std::string_view codeFolder;
    };
    ReleaseWithoutCodeInfo ReleaseWithoutCode(std::string_view sha256);

    // What the installer's functions use for `requested`: itself, unless it
    // is a compiled-in manifest and `other` (also compiled in, or nullptr)
    // says the files are another region's.
    inline const Manifest& Pick(const Manifest& requested, const Manifest* other)
    {
        return other && IsGameManifest(requested) ? *other : requested;
    }

    // ASCII case-insensitive equality: the disc's names are ASCII.
    bool PathEqualsIgnoreCase(std::string_view a, std::string_view b);
}
