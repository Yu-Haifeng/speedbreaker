// Tests for hid/touch_art_analysis.h: what a picture's shape is, measured the
// way scripts/gen_deck_atlas.py measures the deck's own -- so that a picture
// the player supplies is pressed where it is drawn, like the bundled ones.
//
// Two halves: pictures made here (a disc, a ring, an L, a strip, one pixel,
// nothing at all, and the shapes that must be refused), and the bundled atlas's
// eighteen cells, whose measurements must be the generated table's. The second
// half needs the atlas: from the repo root,
//   clang++ -std=c++20 -O2 -Iruntime tests/touch_art_test.cpp \
//       runtime/thirdparty/stb_image/stb_image.cpp -o build/touch_art_test \
//       && build/touch_art_test runtime/ui/touch/deck_atlas.png
#include <hid/touch_art_analysis.h>

#include <thirdparty/stb_image/stb_image.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

using namespace hid::touch;

namespace
{
    // An RGBA8 picture, drawn by `paint(x, y)` returning its alpha.
    struct Image
    {
        int w = 0, h = 0;
        std::vector<uint8_t> rgba;

        Image(int width, int height) : w(width), h(height), rgba(size_t(width) * size_t(height) * 4, 0) {}

        void Set(int x, int y, uint8_t alpha)
        {
            const size_t i = (size_t(y) * size_t(w) + size_t(x)) * 4;
            rgba[i + 0] = rgba[i + 1] = rgba[i + 2] = 255;
            rgba[i + 3] = alpha;
        }

        template <typename Fn>
        void Paint(Fn paint)
        {
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    Set(x, y, paint(x, y));
        }
    };

    const char* Reason(const hid::touch::art::Analysis& analysis, bool ok, const char* why)
    {
        (void)analysis;
        return ok ? "" : (why ? why : "(no reason)");
    }
}

static void Synthetic()
{
    using hid::touch::art::Analysis;
    Analysis analysis;
    const char* why = nullptr;

    // A disc: as wide as it is tall, its middle inside, its corners not.
    {
        Image disc(64, 64);
        disc.Paint([](int x, int y) {
            const float dx = float(x) + 0.5f - 32.0f, dy = float(y) + 0.5f - 32.0f;
            return uint8_t(dx * dx + dy * dy <= 30.0f * 30.0f ? 255 : 0);
        });
        const bool ok = art::Analyse(disc.rgba.data(), disc.w, disc.h, 0, analysis, &why);
        CHECK(ok, "a disc is measured: %s", Reason(analysis, ok, why));
        if (ok)
        {
            CHECK(std::fabs(analysis.aspect - 1.0f) < 0.02f, "a disc is square (%.3f)", analysis.aspect);
            CHECK(analysis.sdfW == art::kSdfW, "its field is the packer's width (%u)", analysis.sdfW);
            CHECK(analysis.sdfH >= 2, "and has a height (%u)", analysis.sdfH);
            CHECK(analysis.inradius > 0.4f && analysis.inradius <= 0.5f, "its radius is most of it (%.3f)",
                analysis.inradius);
            CHECK(analysis.field.size() == size_t(analysis.sdfW) * size_t(analysis.sdfH), "the field is its grid");
            CHECK(analysis.x0 > 0 && analysis.y0 > 0 && analysis.x1 < 63 && analysis.y1 < 63, "trimmed to the disc");
        }
    }

    // A ring: the middle is drawn round, not drawn -- and must be pressed all
    // the same (the gears are rings).
    {
        Image ring(64, 64);
        ring.Paint([](int x, int y) {
            const float dx = float(x) + 0.5f - 32.0f, dy = float(y) + 0.5f - 32.0f;
            const float d = dx * dx + dy * dy;
            return uint8_t(d <= 900.0f && d >= 400.0f ? 255 : 0);
        });
        Analysis holed;
        const bool ok = art::Analyse(ring.rgba.data(), ring.w, ring.h, 0, holed, &why);
        CHECK(ok, "a ring is measured: %s", Reason(holed, ok, why));
        if (ok)
        {
            art::Info info;
            info.sdfW = holed.sdfW;
            info.sdfH = holed.sdfH;
            info.aspect = holed.aspect;
            info.field = holed.field.data();
            CHECK(art::SdfAt(info, 0.5f, 0.5f) <= 0.0f, "its cut-out middle is inside the shape");
            CHECK(art::SdfAt(info, 0.02f, 0.02f) > 0.0f, "and a corner of its rectangle is not");
        }
    }

    // An L: as tall as it is wide, but not much of either.
    {
        Image ell(50, 50);
        ell.Paint([](int x, int y) { return uint8_t(x < 18 || y > 32 ? 255 : 0); });
        Analysis l;
        const bool ok = art::Analyse(ell.rgba.data(), ell.w, ell.h, 0, l, &why);
        CHECK(ok, "an L is measured: %s", Reason(l, ok, why));
        if (ok)
            CHECK(std::fabs(l.aspect - 1.0f) < 0.05f, "an L in a square is square (%.3f)", l.aspect);
    }

    // A wide strip: 4:1, so the field's grid is a quarter as tall, rounded.
    {
        Image strip(40, 10);
        strip.Paint([](int, int) { return uint8_t(255); });
        Analysis flat;
        const bool ok = art::Analyse(strip.rgba.data(), strip.w, strip.h, 0, flat, &why);
        CHECK(ok, "a strip is measured: %s", Reason(flat, ok, why));
        if (ok)
        {
            CHECK(flat.sdfW == art::kSdfW, "the field's width is fixed (%u)", flat.sdfW);
            CHECK(flat.sdfH == 11, "39 / 4 rounds to 10, and a row for the edge (%u)", flat.sdfH);
            CHECK(flat.inradius <= 0.5f && flat.inradius > 0.09f, "a strip's radius is its half height (%.3f)",
                flat.inradius);
        }
    }

    // One pixel: nothing to see, and nothing to crash on.
    {
        Image pixel(1, 1);
        pixel.Set(0, 0, 255);
        Analysis dot;
        const bool ok = art::Analyse(pixel.rgba.data(), pixel.w, pixel.h, 0, dot, &why);
        CHECK(ok, "one pixel is measured: %s", Reason(dot, ok, why));
        if (ok)
        {
            CHECK(dot.sdfW == art::kSdfW && dot.sdfH == art::kSdfW, "as a square field (%ux%u)", dot.sdfW, dot.sdfH);
            CHECK(dot.field.size() == size_t(dot.sdfW) * size_t(dot.sdfH), "with a sample a cell");
        }
    }

    // Nothing at all: refused, with a reason.
    {
        Image blank(32, 32);
        Analysis none;
        why = nullptr;
        CHECK(!art::Analyse(blank.rgba.data(), blank.w, blank.h, 0, none, &why), "a blank picture is refused");
        CHECK(why && why[0], "and says why (%s)", why ? why : "");
    }

    // Faint plates count as shape: the deck's own are about 30% opaque.
    {
        Image faint(32, 32);
        faint.Paint([](int, int) { return uint8_t(80); });
        Analysis dim;
        const bool ok = art::Analyse(faint.rgba.data(), faint.w, faint.h, 0, dim, &why);
        CHECK(ok, "a 31%% plate is a shape: %s", Reason(dim, ok, why));
    }

    // Guard rails: too wide to be a picture, too narrow to have a field.
    {
        Image wide(5000, 10);
        wide.Paint([](int, int) { return uint8_t(255); });
        Analysis huge;
        why = nullptr;
        CHECK(!art::Analyse(wide.rgba.data(), wide.w, wide.h, 0, huge, &why), "5000 pixels wide is refused");
        CHECK(why && why[0], "and says why (%s)", why ? why : "");

        Image tall(78, 512);
        tall.Paint([](int, int) { return uint8_t(255); });
        Analysis thin;
        why = nullptr;
        CHECK(!art::Analyse(tall.rgba.data(), tall.w, tall.h, 0, thin, &why), "78 by 512 is too narrow for 40 samples");
        CHECK(why && why[0], "and says why (%s)", why ? why : "");

        Analysis empty;
        CHECK(!art::Analyse(nullptr, 64, 64, 0, empty, &why), "no pixels, no picture");
    }

    // A row wider than its pixels: the same picture, so the same answer.
    {
        Image tight(64, 64);
        tight.Paint([](int x, int y) {
            const float dx = float(x) + 0.5f - 32.0f, dy = float(y) + 0.5f - 32.0f;
            return uint8_t(dx * dx + dy * dy <= 900.0f ? 255 : 0);
        });
        std::vector<uint8_t> padded(size_t(96) * size_t(64) * 4, 0);
        for (int y = 0; y < 64; y++)
            std::copy_n(tight.rgba.begin() + ptrdiff_t(size_t(y) * 64 * 4), 64 * 4,
                padded.begin() + ptrdiff_t((size_t(y) * 96 + 16) * 4));
        Analysis strided, plain;
        const bool ok = art::Analyse(padded.data() + 16 * 4, 64, 64, 96 * 4, strided, &why);
        CHECK(ok, "a strided picture is measured: %s", Reason(strided, ok, why));
        const bool ok2 = art::Analyse(tight.rgba.data(), 64, 64, 0, plain, &why);
        CHECK(ok2, "and the tight one too");
        if (ok && ok2)
        {
            CHECK(strided.sdfW == plain.sdfW && strided.sdfH == plain.sdfH, "the same grid");
            CHECK(strided.field == plain.field, "and the same field");
            CHECK(strided.x0 == plain.x0 && strided.y1 == plain.y1, "and the same bounds");
        }
    }
}

// The bundled atlas: every cell, measured here, must be what the packer wrote
// into hid/touch_art.h -- which is what lets a player's picture be treated the
// same way as the deck's own.
static void Bundled(const char* path)
{
    // (stb_image is compiled without stdio, as the runtime has it: the file is
    // read here and decoded from memory -- ui/player_pictures.cpp does the same.)
    std::ifstream file(path, std::ios::binary);
    const std::vector<unsigned char> bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    int w = 0, h = 0, channels = 0;
    unsigned char* png = bytes.empty() ? nullptr
        : stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &channels, 4);
    if (!png)
    {
        printf("  FAIL the atlas did not decode (%s): %s\n", path, stbi_failure_reason());
        g_failures++;
        return;
    }
    CHECK(w == art::kAtlasWidth && h == art::kAtlasHeight, "the atlas is %dx%d, the header says %dx%d", w, h,
        art::kAtlasWidth, art::kAtlasHeight);

    for (size_t i = 0; i < art::kCount && i < std::size(art::kArt); i++)
    {
        const art::Info& want = art::kArt[i];
        const int cellX = int(i % size_t(art::kCols)) * art::kCell + (art::kCell - 256) / 2;
        const int cellY = int(i / size_t(art::kCols)) * art::kCell + (art::kCell - 256) / 2;

        // The cell's picture, tight, as the packer measured it.
        std::vector<uint8_t> cell(size_t(256) * size_t(256) * 4, 0);
        for (int y = 0; y < 256; y++)
            std::copy_n(png + ((size_t(cellY + y) * size_t(w) + size_t(cellX)) * 4), 256 * 4,
                cell.begin() + ptrdiff_t(size_t(y) * 256 * 4));

        art::Analysis got;
        const char* why = nullptr;
        const bool ok = art::Analyse(cell.data(), 256, 256, 0, got, &why);
        CHECK(ok, "%s: measured: %s", art::kArtKey[i], ok ? "" : (why ? why : ""));
        if (!ok)
            continue;

        CHECK(got.sdfW == want.sdfW && got.sdfH == want.sdfH, "%s: the field is %ux%u, the header says %ux%u",
            art::kArtKey[i], got.sdfW, got.sdfH, want.sdfW, want.sdfH);
        CHECK(std::fabs(got.aspect - want.aspect) <= 2e-6f, "%s: aspect %.7f, the header %.7f", art::kArtKey[i],
            got.aspect, want.aspect);
        CHECK(std::fabs(got.inradius - want.inradius) <= 0.002f, "%s: inradius %.5f, the header %.5f",
            art::kArtKey[i], got.inradius, want.inradius);

        // The cell's bounds, back from the atlas coordinates the header keeps.
        const auto round = [](float v) { return int(std::lround(v)); };
        CHECK(round(want.u0 * float(w)) - cellX == got.x0, "%s: the left edge (x %d, header %d)", art::kArtKey[i],
            got.x0, round(want.u0 * float(w)) - cellX);
        CHECK(round(want.v0 * float(h)) - cellY == got.y0, "%s: the top edge", art::kArtKey[i]);
        CHECK(round(want.u1 * float(w)) - 1 - cellX == got.x1, "%s: the right edge", art::kArtKey[i]);
        CHECK(round(want.v1 * float(h)) - 1 - cellY == got.y1, "%s: the bottom edge", art::kArtKey[i]);

        // The field itself: the same bytes, to within a level (the packer does
        // its arithmetic in doubles, this in floats).
        if (got.field.size() == size_t(want.sdfW) * size_t(want.sdfH) && want.field)
        {
            int worst = 0, at = -1;
            for (size_t k = 0; k < got.field.size(); k++)
            {
                const int diff = std::abs(int(got.field[k]) - int(want.field[k]));
                if (diff > worst)
                {
                    worst = diff;
                    at = int(k);
                }
            }
            CHECK(worst <= 1, "%s: the field differs by %d at sample %d", art::kArtKey[i], worst, at);
        }
        else
        {
            CHECK(false, "%s: the field is %zu bytes, the header's %ux%u", art::kArtKey[i], got.field.size(),
                want.sdfW, want.sdfH);
        }
    }

    stbi_image_free(png);
}

int main(int argc, char** argv)
{
    printf("synthetic pictures\n");
    Synthetic();
    if (argc > 1)
    {
        printf("the bundled atlas\n");
        Bundled(argv[1]);
    }
    else
    {
        printf("  (no atlas given: the bundled cells were not compared)\n");
    }

    printf("%s (%d failures)\n", g_failures ? "FAILED" : "passed", g_failures);
    return g_failures ? 1 : 0;
}
