// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// What scripts/gen_deck_atlas.py does to a picture, in C++: trim it to what is
// drawn, fill the holes a silhouette would otherwise have (the gear rings), and
// measure the signed distance field that hit testing traces.
//
// The packer runs it once, over the deck's own art, and writes the answer into
// hid/touch_art.h. This runs it again at load, over a picture the player
// supplied (ui/player_pictures.cpp), so that a button whose picture is a
// triangle is pressed where the triangle is -- the same rule for the deck's
// pictures and the player's.
//
// The two must agree: tests/touch_art_test.cpp runs this over the bundled
// atlas's cells and compares with the generated table. Anything that changes
// here has to change there too.
//
// Header only, and free of SDL and ImGui: the input is RGBA8 bytes, which is
// what stb_image hands over.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <hid/touch_art.h>

namespace hid::touch::art
{
    struct Analysis
    {
        // The picture's own bounds, in its image's pixels (x1/y1 inclusive).
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        float aspect = 0.0f;    // trimmed width / height
        float inradius = 0.0f;  // widest inscribed circle's radius, in widths
        uint8_t sdfW = 0, sdfH = 0;
        std::vector<uint8_t> field;
    };

    namespace analysis
    {
        // Round half to even, as Python's round() does: the field is compared
        // byte for byte against the packer's, and a sample exactly between two
        // levels would otherwise go the other way.
        inline int Round(float value)
        {
            return int(std::nearbyint(value));
        }

        // Squared Euclidean distance transform of one line (Felzenszwalb).
        inline void Edt1D(const std::vector<float>& f, std::vector<float>& d)
        {
            const int n = int(f.size());
            d.assign(size_t(n), 0.0f);
            if (n <= 0)
                return;
            std::vector<int> v(size_t(n), 0);
            std::vector<float> z(size_t(n) + 1, 0.0f);
            int k = 0;
            v[0] = 0;
            z[0] = -1e20f;
            z[1] = 1e20f;
            for (int q = 1; q < n; q++)
            {
                float s = ((f[size_t(q)] + float(q) * float(q)) - (f[size_t(v[size_t(k)])] + float(v[size_t(k)]) * float(v[size_t(k)]))) /
                    float(2 * q - 2 * v[size_t(k)]);
                while (s <= z[size_t(k)])
                {
                    k--;
                    s = ((f[size_t(q)] + float(q) * float(q)) - (f[size_t(v[size_t(k)])] + float(v[size_t(k)]) * float(v[size_t(k)]))) /
                        float(2 * q - 2 * v[size_t(k)]);
                }
                k++;
                v[size_t(k)] = q;
                z[size_t(k)] = s;
                z[size_t(k) + 1] = 1e20f;
            }
            k = 0;
            for (int q = 0; q < n; q++)
            {
                while (z[size_t(k) + 1] < float(q))
                    k++;
                const float diff = float(q) - float(v[size_t(k)]);
                d[size_t(q)] = diff * diff + f[size_t(v[size_t(k)])];
            }
        }

        // For every sample, the distance in samples to the nearest `want` one.
        inline std::vector<float> DistanceTo(const std::vector<uint8_t>& grid, int gw, int gh, uint8_t want)
        {
            constexpr float kFar = 1e12f;
            std::vector<float> rows(size_t(gw) * size_t(gh), 0.0f);
            for (int y = 0; y < gh; y++)
                for (int x = 0; x < gw; x++)
                    rows[size_t(y) * size_t(gw) + size_t(x)] = grid[size_t(y) * size_t(gw) + size_t(x)] == want ? 0.0f : kFar;

            std::vector<float> line(size_t(std::max(gw, gh))), done;
            for (int y = 0; y < gh; y++)
            {
                line.assign(rows.begin() + ptrdiff_t(size_t(y) * size_t(gw)), rows.begin() + ptrdiff_t(size_t(y + 1) * size_t(gw)));
                Edt1D(line, done);
                std::copy(done.begin(), done.end(), rows.begin() + ptrdiff_t(size_t(y) * size_t(gw)));
            }

            std::vector<float> column(size_t(gh), 0.0f), transformed;
            std::vector<float> out(size_t(gw) * size_t(gh), 0.0f);
            for (int x = 0; x < gw; x++)
            {
                for (int y = 0; y < gh; y++)
                    column[size_t(y)] = rows[size_t(y) * size_t(gw) + size_t(x)];
                Edt1D(column, transformed);
                for (int y = 0; y < gh; y++)
                {
                    const float squared = transformed[size_t(y)];
                    out[size_t(y) * size_t(gw) + size_t(x)] = squared > 0.0f ? std::sqrt(squared) : 0.0f;
                }
            }
            return out;
        }

        // Every background sample the outside cannot reach is inside the shape:
        // the gears are rings, and their cut-out middle is a thumb's.
        inline void FillHoles(std::vector<uint8_t>& mask, int bw, int bh)
        {
            std::vector<uint8_t> outside(size_t(bw) * size_t(bh), 0);
            std::vector<int> stack;
            const auto seed = [&](int x, int y) {
                const size_t i = size_t(y) * size_t(bw) + size_t(x);
                if (!mask[i] && !outside[i])
                {
                    outside[i] = 1;
                    stack.push_back(x + y * bw);
                }
            };
            for (int x = 0; x < bw; x++)
            {
                seed(x, 0);
                seed(x, bh - 1);
            }
            for (int y = 0; y < bh; y++)
            {
                seed(0, y);
                seed(bw - 1, y);
            }
            while (!stack.empty())
            {
                const int at = stack.back();
                stack.pop_back();
                const int x = at % bw, y = at / bw;
                const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
                for (int k = 0; k < 4; k++)
                {
                    const int nx = x + dx[k], ny = y + dy[k];
                    if (nx < 0 || ny < 0 || nx >= bw || ny >= bh)
                        continue;
                    const size_t i = size_t(ny) * size_t(bw) + size_t(nx);
                    if (mask[i] || outside[i])
                        continue;
                    outside[i] = 1;
                    stack.push_back(nx + ny * bw);
                }
            }
            for (size_t i = 0; i < mask.size(); i++)
                mask[i] = uint8_t(mask[i] || !outside[i]);
        }
    }

    // The picture's bounds, shape and field, from `w` x `h` RGBA8 pixels
    // (`pitchBytes` a row, 0 for tightly packed). False with a reason in `why`
    // when there is nothing to measure, or the picture is too big for a field.
    inline bool Analyse(const uint8_t* rgba, int w, int h, int pitchBytes, Analysis& out, const char** why)
    {
        const auto fail = [why](const char* reason) {
            if (why)
                *why = reason;
            return false;
        };
        if (!rgba || w <= 0 || h <= 0)
            return fail("no pixels");
        if (w > 4096 || h > 4096)
            return fail("bigger than 4096 pixels a side");
        if (pitchBytes <= 0)
            pitchBytes = w * 4;
        if (pitchBytes < w * 4)
            return fail("a row shorter than its pixels");

        const auto alpha = [rgba, pitchBytes](int x, int y) {
            return rgba[size_t(y) * size_t(pitchBytes) + size_t(x) * 4 + 3];
        };

        // The picture's own bounds, at a threshold relative to its brightest
        // sample: a plate about 30% opaque must count as shape.
        int peak = 0;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                peak = std::max(peak, int(alpha(x, y)));
        const int threshold = std::max(8, int(float(peak) * 0.10f));
        int x0 = w, y0 = h, x1 = -1, y1 = -1;
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                if (alpha(x, y) <= threshold)
                    continue;
                x0 = std::min(x0, x);
                x1 = std::max(x1, x);
                y0 = std::min(y0, y);
                y1 = std::max(y1, y);
            }
        }
        if (x1 < x0 || y1 < y0)
            return fail("nothing drawn");

        const int tw = x1 - x0 + 1, th = y1 - y0 + 1;
        std::vector<uint8_t> mask(size_t(tw) * size_t(th), 0);
        for (int y = 0; y < th; y++)
            for (int x = 0; x < tw; x++)
                mask[size_t(y) * size_t(tw) + size_t(x)] = uint8_t(alpha(x0 + x, y0 + y) > threshold);
        analysis::FillHoles(mask, tw, th);

        // Down to the field's grid: how much of each cell is drawn, half of it
        // is.
        const int sw = kSdfW;
        const float aspect = float(tw) / float(th);
        const int sh = std::max(2, analysis::Round((float(sw) - 1.0f) / aspect) + 1);
        if (sh > 255)
            return fail("too tall for the field's grid (a very narrow picture)");

        std::vector<uint8_t> small(size_t(sw) * size_t(sh), 0);
        for (int sy = 0; sy < sh; sy++)
        {
            const float ya = float(sy) * float(th) / float(sh), yb = float(sy + 1) * float(th) / float(sh);
            for (int sx = 0; sx < sw; sx++)
            {
                const float xa = float(sx) * float(tw) / float(sw), xb = float(sx + 1) * float(tw) / float(sw);
                float covered = 0.0f, area = 0.0f;
                for (int y = int(ya); y < std::min(th, int(std::ceil(yb))); y++)
                {
                    const float dy = std::min(yb, float(y + 1)) - std::max(ya, float(y));
                    if (dy <= 0.0f)
                        continue;
                    for (int x = int(xa); x < std::min(tw, int(std::ceil(xb))); x++)
                    {
                        const float dx = std::min(xb, float(x + 1)) - std::max(xa, float(x));
                        if (dx <= 0.0f)
                            continue;
                        if (mask[size_t(y) * size_t(tw) + size_t(x)])
                            covered += dx * dy;
                        area += dx * dy;
                    }
                }
                small[size_t(sy) * size_t(sw) + size_t(sx)] = uint8_t(area > 0.0f && covered / area >= 0.5f);
            }
        }

        const std::vector<float> inside = analysis::DistanceTo(small, sw, sh, 0);
        const std::vector<float> outside = analysis::DistanceTo(small, sw, sh, 1);
        const float span = float(sw);  // one sample is 1/span of the width
        float inradius = 0.0f;
        std::vector<uint8_t> field(size_t(sw) * size_t(sh), 0);
        for (int y = 0; y < sh; y++)
        {
            for (int x = 0; x < sw; x++)
            {
                const size_t i = size_t(y) * size_t(sw) + size_t(x);
                float distance = 0.0f;
                if (small[i])
                {
                    distance = -inside[i] / span;
                    inradius = std::min(0.5f, std::max(inradius, -distance));
                }
                else
                {
                    distance = outside[i] / span;
                }
                const float clamped = std::min(0.5f, std::max(-0.5f, distance));
                field[i] = uint8_t(std::min(255.0f, std::max(0.0f, float(analysis::Round((clamped + 0.5f) * 255.0f)))));
            }
        }

        out.x0 = x0;
        out.y0 = y0;
        out.x1 = x1;
        out.y1 = y1;
        out.aspect = aspect;
        out.inradius = inradius;
        out.sdfW = uint8_t(sw);
        out.sdfH = uint8_t(sh);
        out.field = std::move(field);
        return true;
    }
}
