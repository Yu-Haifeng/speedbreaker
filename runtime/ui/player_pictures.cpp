// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See player_pictures.h.
#include "player_pictures.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <system_error>

#include <thirdparty/stb_image/stb_image.h>

#include <hid/touch_art_analysis.h>

namespace ui::pictures
{
    namespace
    {
        using hid::touch::art::Analysis;
        using hid::touch::art::Info;

        // The deck's own sheet's geometry (hid/touch_art.h): the player's is
        // the same, so one cell means one picture in both and the drawing code
        // needs to know nothing about which sheet it is reading.
        constexpr int kArt = 256, kGutter = 8, kCell = kArt + kGutter * 2, kCols = 5;

        // A picture twice the deck's own is a big file already; anything larger
        // is not one of these.
        constexpr uint64_t kMaxFileBytes = 8 * 1024 * 1024;
        constexpr int kMaxSide = 4096;

        bool ReadAll(const std::filesystem::path& path, std::vector<unsigned char>& bytes, std::string& why)
        {
            std::error_code ec;
            const uint64_t size = std::filesystem::file_size(path, ec);
            if (ec)
            {
                why = std::format("can't read {}", path.filename().string());
                return false;
            }
            if (size > kMaxFileBytes || size == 0)
            {
                why = std::format("{} is {} bytes", path.filename().string(), size);
                return false;
            }
            std::ifstream file(path, std::ios::binary);
            bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            if (bytes.empty())
            {
                why = std::format("can't read {}", path.filename().string());
                return false;
            }
            return true;
        }

        // The picture at `want` at most, its shape kept (as the packer scales
        // the deck's own): each destination pixel is the alpha-weighted average
        // of the source pixels it covers, so an edge does not pick up a dark
        // fringe.
        bool Scale(const uint8_t* rgba, int w, int h, int want, std::vector<uint8_t>& out, int& ow, int& oh)
        {
            if (w <= 0 || h <= 0)
                return false;
            const float scale = float(want) / float(std::max(w, h));
            ow = std::clamp(int(std::lround(float(w) * scale)), 1, want);
            oh = std::clamp(int(std::lround(float(h) * scale)), 1, want);
            out.assign(size_t(ow) * size_t(oh) * 4, 0);
            for (int y = 0; y < oh; y++)
            {
                const int sy0 = int(int64_t(y) * h / oh);
                const int sy1 = std::max(sy0 + 1, int(int64_t(y + 1) * h / oh));
                for (int x = 0; x < ow; x++)
                {
                    const int sx0 = int(int64_t(x) * w / ow);
                    const int sx1 = std::max(sx0 + 1, int(int64_t(x + 1) * w / ow));
                    uint64_t r = 0, g = 0, b = 0, a = 0, n = 0;
                    for (int sy = sy0; sy < sy1; sy++)
                    {
                        for (int sx = sx0; sx < sx1; sx++)
                        {
                            const uint8_t* p = rgba + (size_t(sy) * size_t(w) + size_t(sx)) * 4;
                            r += uint64_t(p[0]) * p[3];
                            g += uint64_t(p[1]) * p[3];
                            b += uint64_t(p[2]) * p[3];
                            a += p[3];
                            n++;
                        }
                    }
                    uint8_t* q = out.data() + (size_t(y) * size_t(ow) + size_t(x)) * 4;
                    if (a > 0)
                    {
                        q[0] = uint8_t(r / a);
                        q[1] = uint8_t(g / a);
                        q[2] = uint8_t(b / a);
                    }
                    q[3] = uint8_t(a / std::max<uint64_t>(n, 1));
                }
            }
            return true;
        }

        void Blit(std::vector<uint8_t>& sheet, int sheetW, int atX, int atY, const std::vector<uint8_t>& src, int w, int h)
        {
            for (int y = 0; y < h; y++)
                std::copy_n(src.begin() + ptrdiff_t(size_t(y) * size_t(w) * 4), size_t(w) * 4,
                    sheet.begin() + ptrdiff_t((size_t(atY + y) * size_t(sheetW) + size_t(atX)) * 4));
        }
    }

    const char* NameOf(hid::touch::art::Id id)
    {
        const size_t index = size_t(id);
        return index < hid::touch::art::kCount ? hid::touch::art::kArtKey[index] : "";
    }

    bool Build(const std::filesystem::path& folder, Sheet& out, std::string& why)
    {
        using namespace hid::touch;
        out = Sheet{};
        why.clear();

        std::error_code ec;
        if (!std::filesystem::is_directory(folder, ec))
        {
            // Make it, so the player has somewhere to put a picture (the Files
            // app shows it on iOS, where there is no other way to find it).
            std::filesystem::create_directories(folder, ec);
            return true;
        }

        // What is there, measured: the pictures first, the sheet after, so an
        // empty folder costs nothing.
        struct Found
        {
            art::Id id = art::Id::Count;
            std::vector<uint8_t> pixels;  // scaled to fit a cell
            int w = 0, h = 0;
            Analysis analysis;
        };
        std::vector<Found> found;
        std::string refused;

        for (size_t i = 0; i < art::kCount; i++)
        {
            const art::Id id = art::Id(i);
            const std::filesystem::path path = folder / std::format("{}.png", art::kArtKey[i]);
            if (!std::filesystem::is_regular_file(path, ec))
                continue;

            std::vector<unsigned char> bytes;
            std::string reason;
            if (!ReadAll(path, bytes, reason))
            {
                fprintf(stderr, "[deck] %s\n", reason.c_str());
                refused = reason;
                continue;
            }
            int w = 0, h = 0, channels = 0;
            unsigned char* pixels = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &channels, 4);
            if (!pixels)
            {
                fprintf(stderr, "[deck] %s did not decode: %s\n", path.filename().string().c_str(),
                    stbi_failure_reason());
                refused = std::format("{} did not decode", path.filename().string());
                continue;
            }
            if (w > kMaxSide || h > kMaxSide)
            {
                fprintf(stderr, "[deck] %s is %dx%d; too big\n", path.filename().string().c_str(), w, h);
                refused = std::format("{} is {}x{}", path.filename().string(), w, h);
                stbi_image_free(pixels);
                continue;
            }

            Found entry;
            entry.id = id;
            if (!Scale(pixels, w, h, kArt, entry.pixels, entry.w, entry.h))
            {
                stbi_image_free(pixels);
                continue;
            }
            stbi_image_free(pixels);

            const char* analysisWhy = nullptr;
            if (!art::Analyse(entry.pixels.data(), entry.w, entry.h, 0, entry.analysis, &analysisWhy))
            {
                fprintf(stderr, "[deck] %s: %s\n", path.filename().string().c_str(),
                    analysisWhy ? analysisWhy : "nothing drawn");
                refused = std::format("{}: {}", path.filename().string(), analysisWhy ? analysisWhy : "nothing drawn");
                continue;
            }
            found.push_back(std::move(entry));
        }

        if (found.empty())
        {
            if (!refused.empty())
                why = refused;
            return refused.empty();  // (nothing there is not a failure)
        }

        // The sheet: one cell per picture slot, laid out as the deck's is, so
        // the cells that are not the player's are simply empty.
        const int rows = (int(art::kCount) + kCols - 1) / kCols;
        out.width = kCols * kCell;
        out.height = rows * kCell;
        out.rgba.assign(size_t(out.width) * size_t(out.height) * 4, 0);

        for (Found& entry : found)
        {
            const size_t index = size_t(entry.id);
            const int cellX = int(index % size_t(kCols)) * kCell + kGutter;
            const int cellY = int(index / size_t(kCols)) * kCell + kGutter;
            const int atX = cellX + (kArt - entry.w) / 2;
            const int atY = cellY + (kArt - entry.h) / 2;
            Blit(out.rgba, out.width, atX, atY, entry.pixels, entry.w, entry.h);

            Sheet::Entry packed;
            packed.id = entry.id;
            packed.info.u0 = float(atX + entry.analysis.x0) / float(out.width);
            packed.info.v0 = float(atY + entry.analysis.y0) / float(out.height);
            packed.info.u1 = float(atX + entry.analysis.x1 + 1) / float(out.width);
            packed.info.v1 = float(atY + entry.analysis.y1 + 1) / float(out.height);
            packed.info.aspect = entry.analysis.aspect;
            packed.info.inradius = entry.analysis.inradius;
            packed.info.sdfW = entry.analysis.sdfW;
            packed.info.sdfH = entry.analysis.sdfH;
            packed.info.sheet = 1;
            packed.info.field = nullptr;  // (art::Install points it at its own copy)
            packed.field = std::move(entry.analysis.field);
            out.entries.push_back(std::move(packed));
        }
        return true;
    }
}
