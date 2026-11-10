# The Asian (NTSC) disc: how its tables were made

SpeedBreaker runs the Asian disc (*Need for Speed - Most Wanted (HK)*, `default.xex`
SHA-256 `0cd7b9fa3957de085e298c91faa726116ad0079abaefead82c2f3337ee432e64`)
with its own recompiled code (`ppc-ntsc-asia/`, namespace `ntsc_asia`) beside the
USA's, and the runtime's hooks moved to its addresses. Building it is README
step 2. This page records where its committed files come from and how to check
or regenerate them.

## The committed files

| File | Written by |
|---|---|
| `config/ntsc-asia/nfsmw.toml`, `config/ntsc-asia/switch_tables.toml` | `survey/region_map.py toml --region ntsc-asia` |
| `runtime/region/ntsc_asia_addresses.inc`, `runtime/region/ntsc_asia_names.h` | `survey/region_map.py table --region ntsc-asia` |
| `runtime/install/manifest_ntsc_asia.inc` | `scripts/gen_install_manifest.py` |

They hold addresses, names, sizes and hashes only. A build that links
`ppc-ntsc-asia/` (`SB_NTSC_ASIA`) installs and runs the disc; one without it
refuses the disc up front and says it needs a build with its code. Without the
images, `tests/region_addresses_check.py . --region ntsc-asia` checks that the
table covers every guest address the runtime names and that
`config/ntsc-asia/` is `config/` translated.

## What the survey found

The Asian `default.xex` is 3,940,352 bytes, the same size as the Spanish one,
but the layouts are not interchangeable. It has 22,679 functions like the USA
build: 7,365 exact, 15,276 reloc-equal, 37 changed, one missing, and six that
only it has. The unchanged layout is in 25 pieces moved by +0x0..+0x590.

`.data` starts at the USA address but its internal layout moved at two points:
addresses the runtime names map by +0x190 or +0x5A0/+0x5A4, each found from
code references and checked to hold the same bytes. `RegionMap.data` therefore
uses a direct reference, references on both sides with one delta, or a
neighbouring delta with unique surrounding context when `.data` itself is not
byte-identical. There is no jump table only the Asian build has.

The executable boots in language 9 (`CHINESE` in its 13-entry language table)
and asks for `_ntsc` movies. The disc has 33 English NTSC movies, Chinese text
and subtitles, and only the English speech bank. SpeedBreaker offers Chinese
as this disc's default; `DiscLanguage("ntsc-asia")` returns it, and the boot
hook substitutes English when that is chosen instead. The movie suffix stays
NTSC either way.

Its `default.xex` size equals the Spanish one, so `ManifestForXexSize` returns
none for that size. `install/locate.cpp` hashes the actual executable instead
of guessing or trusting the install marker's recorded `xex_sha256`.

## Redoing them

Needs the USA and Asian discs' `default.xex` and the toolchain from README
step 2. From the repository root:

```sh
mkdir -p build game/survey/mw_usa game/survey/mw_ntsc_asia
clang++ -std=c++20 -O2 -include cstdlib -Itools/XenonRecomp/XenonUtils -Itools/XenonRecomp/thirdparty/simde \
  survey/xexsurvey.cpp tools/XenonRecomp/build/XenonUtils/libXenonUtils.a \
  tools/XenonRecomp/build/thirdparty/disasm/libdisasm.a -o build/xexsurvey
build/xexsurvey game/files/default.xex game/survey/mw_usa > game/survey/mw_usa/xexsurvey.txt
build/xexsurvey game/files-ntsc-asia/default.xex game/survey/mw_ntsc_asia > game/survey/mw_ntsc_asia/xexsurvey.txt

python3 -I survey/func_map.py game/survey/mw_usa game/survey/mw_ntsc_asia > game/survey/mw_ntsc_asia/funcmap.tsv

S=game/survey
python3 -I tests/region_addresses_check.py . $S/mw_usa $S/mw_ntsc_asia $S/mw_ntsc_asia/funcmap.tsv --region ntsc-asia
python3 -I survey/region_map.py toml  $S/mw_usa $S/mw_ntsc_asia $S/mw_ntsc_asia/funcmap.tsv --region ntsc-asia
python3 -I survey/region_map.py table $S/mw_usa $S/mw_ntsc_asia $S/mw_ntsc_asia/funcmap.tsv --region ntsc-asia
python3 scripts/gen_install_manifest.py game/files-ntsc-asia --config config/ntsc-asia/nfsmw.toml \
  -o runtime/install/manifest_ntsc_asia.inc
```

`cmake -DSB_REGION_SURVEY=game/survey` with `mw_ntsc_asia/` beside `mw_usa/`
makes ctest's `region_addresses_check_ntsc_asia` do the byte check too.
`region_parity_check` keeps its hooks, leaf functions and jump tables in step
with `config/` and the USA runtime sources.

## Testing

`region_addresses_check --region ntsc-asia` (with both images),
`region_parity_check`, and all three `region_manifest_test` builds pass. The
real HK ISO verifies all 45 files and installs as `Asia (NTSC)`; a headless
launch selects the Asian code, uses Chinese by default, and opens its English
NTSC movies and speech. English is selected in Settings > Game > Language.
