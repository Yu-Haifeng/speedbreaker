// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The touch deck as the design handoff draws it: hid/touch.h has the fingers
// and what the game is told, hid/touch_layout.h where every control sits on
// the picture, and this file paints them.
//
// The material is the handoff's -- translucent charcoal glass, a 1px border, a
// hairline highlight along the top, and the orange only where a control is
// held or a pedal waits for a thumb. Two layers of its glass cannot be had
// here: ImGui cannot read the frame it draws over, so no 14px backdrop blur,
// and it draws no blurred shadow. Both are stood in for: the blur by a
// vertical gradient (clearer at the top, denser at the bottom, the light it
// would catch), the shadow by three tight rings under the glass that read as
// one soft edge at this size -- which keeps a control legible over a sunlit
// road, where the thin glass alone would wash out. A held control carries a
// little of the orange outside its border (two more strokes), so the press
// reads as light rather than a colour swap, and the press itself eases in
// over a few frames instead of popping.
//
// Nothing here is a hit test: the rectangles are the ones the fingers were
// tested against (hid/touch::DeckNow), drawn on the picture's own rectangle.
#include <stdafx.h>
#include "ui.h"
#include <ui/touch/touch_icons.h>

#include <hid/touch.h>
#include <video/picture_rect.h>

#include <imgui.h>

namespace ui
{
    namespace
    {
        using namespace hid::touch;

        // design/tokens.json, at index.html's final "minimal direction" layer.
        constexpr ImU32 kChalk = IM_COL32(241, 237, 227, 255);
        constexpr ImU32 kOrangeBright = IM_COL32(255, 123, 46, 255);
        constexpr ImU32 kNitro = IM_COL32(169, 220, 231, 255);
        // What a key's name is written in.
        constexpr ImU32 kKeyA = IM_COL32(168, 216, 136, 255);
        constexpr ImU32 kKeyB = IM_COL32(255, 142, 131, 255);
        constexpr ImU32 kKeyX = IM_COL32(143, 196, 255, 255);
        constexpr ImU32 kKeyY = IM_COL32(243, 215, 125, 255);
        constexpr ImU32 kKeyPlain = IM_COL32(241, 237, 227, 255);

        // The opacities the design sets on top of those. The glass is thinner
        // than the design's so the road shows through it: the deck is a
        // control surface, not a panel. The gradient's two ends average close
        // to the flat glass the circles still take (a circle has no corners
        // to split a gradient into).
        constexpr float kGlassAlpha = 0.42f;
        constexpr float kGlassTopAlpha = 0.30f;
        constexpr float kGlassBottomAlpha = 0.52f;
        constexpr float kBorderAlpha = 0.19f;
        constexpr float kHeldBorderAlpha = 0.78f;
        constexpr float kIconAlpha = 0.90f;
        constexpr float kQuietAlpha = 0.32f;  // the steering track's end arrows

        ImU32 Alpha(ImU32 colour, float a)
        {
            const float value = std::clamp(float((colour >> IM_COL32_A_SHIFT) & 0xFF) * a, 0.0f, 255.0f);
            return (colour & ~IM_COL32_A_MASK) | (ImU32(value) << IM_COL32_A_SHIFT);
        }

        // Two colours as their channels see them, for the states in between
        // (a border that is on its way to held, an icon on its way to orange).
        ImU32 Mix(ImU32 from, ImU32 to, float t)
        {
            t = std::clamp(t, 0.0f, 1.0f);
            const auto channel = [t](ImU32 a, ImU32 b, ImU32 shift) {
                const float fa = float((a >> shift) & 0xFF), fb = float((b >> shift) & 0xFF);
                return ImU32(std::lround(fa + (fb - fa) * t)) << shift;
            };
            return channel(from, to, IM_COL32_R_SHIFT) | channel(from, to, IM_COL32_G_SHIFT) |
                channel(from, to, IM_COL32_B_SHIFT) | channel(from, to, IM_COL32_A_SHIFT);
        }

        // The press of a control, eased: the design's states are instant, but
        // a deck that pops between them reads as flicker, so each control
        // keeps a 0..1 amount that follows whether a finger holds it -- onto
        // the control quickly (a press must land within a frame or two), off
        // it a little slower (a release eases back). Two more slots: the
        // D-pad's plate (it lights as one when any key does) and the shifter's.
        float& PressSlot(size_t slot)
        {
            static float s_press[size_t(Control::Count) + 2] = {};
            return s_press[slot];
        }

        float PressOf(size_t slot, bool held)
        {
            float& value = PressSlot(slot);
            const float dt = std::clamp(ImGui::GetIO().DeltaTime, 1.0f / 240.0f, 0.1f);
            value += (float(held) - value) * std::min(1.0f, dt * (held ? 16.0f : 10.0f));
            if (value < 0.004f)
                value = 0.0f;
            return value;
        }

        float PressOf(Control c, bool held)
        {
            return PressOf(size_t(c), held);
        }
        constexpr size_t kDpadPlateSlot = size_t(Control::Count), kShifterPlateSlot = size_t(Control::Count) + 1;

        // The design's blurred shadow, stood in for by three tight rings, the
        // furthest the faintest, all a hair down (the light is above). Rings
        // a point or two apart read as one soft edge at this size.
        void ShadowRect(ImDrawList* dl, ImVec2 a, ImVec2 b, float radius)
        {
            constexpr float kGrow[] = { 4.0f, 2.5f, 1.0f };
            constexpr ImU32 kAlpha[] = { 10, 16, 26 };
            for (int i = 0; i < 3; i++)
            {
                const float g = kGrow[i];
                dl->AddRectFilled(ImVec2(a.x - g, a.y - g + 1.0f), ImVec2(b.x + g, b.y + g + 1.0f),
                    IM_COL32(0, 0, 0, kAlpha[i]), radius + g);
            }
        }

        void ShadowCircle(ImDrawList* dl, ImVec2 centre, float radius)
        {
            constexpr float kGrow[] = { 4.0f, 2.5f, 1.0f };
            constexpr ImU32 kAlpha[] = { 10, 16, 26 };
            for (int i = 0; i < 3; i++)
                dl->AddCircleFilled(centre, radius + kGrow[i], IM_COL32(0, 0, 0, kAlpha[i]), 0);
        }

        // The glass body as the design's gradient: clearer at the top (the
        // light it catches), denser at the bottom (where it sits on the road).
        // ImGui fills a rounded rectangle with one colour, so the gradient is
        // three fills: a rounded cap at each end and a four-colour quad
        // between them, whose square sides are exact there -- past a radius
        // from either end a rounded rectangle is at its full width.
        void GlassBody(ImDrawList* dl, ImVec2 a, ImVec2 b, float radius)
        {
            const ImU32 top = IM_COL32(20, 18, 15, ImU32(255.0f * kGlassTopAlpha));
            const ImU32 bottom = IM_COL32(20, 18, 15, ImU32(255.0f * kGlassBottomAlpha));
            radius = std::min(radius, (b.y - a.y) * 0.5f);
            const float mid1 = a.y + radius, mid2 = b.y - radius;
            if (mid1 >= mid2)
            {
                dl->AddRectFilled(a, b, Mix(top, bottom, 0.5f), radius);
                return;
            }
            dl->AddRectFilled(a, ImVec2(b.x, mid1 + 0.5f), top, radius, ImDrawFlags_RoundCornersTop);
            dl->AddRectFilledMultiColor(ImVec2(a.x, mid1), ImVec2(b.x, mid2), top, top, bottom, bottom);
            dl->AddRectFilled(ImVec2(a.x, mid2 - 0.5f), b, bottom, radius, ImDrawFlags_RoundCornersBottom);
        }

        // The held bloom: two strokes outside the border, the orange reaching
        // a little way into the road. Not a blur -- wide strokes -- but at
        // this size it reads as light rather than a colour swap.
        void GlowRect(ImDrawList* dl, ImVec2 a, ImVec2 b, float radius, float press)
        {
            if (press <= 0.01f)
                return;
            dl->AddRect(ImVec2(a.x - 2.0f, a.y - 2.0f), ImVec2(b.x + 2.0f, b.y + 2.0f), Alpha(kOrangeBright, 0.16f * press),
                radius + 2.0f, 0, 2.5f);
            dl->AddRect(ImVec2(a.x - 4.5f, a.y - 4.5f), ImVec2(b.x + 4.5f, b.y + 4.5f), Alpha(kOrangeBright, 0.07f * press),
                radius + 4.5f, 0, 3.5f);
        }

        void GlowCircle(ImDrawList* dl, ImVec2 centre, float radius, float press)
        {
            if (press <= 0.01f)
                return;
            dl->AddCircle(centre, radius + 2.0f, Alpha(kOrangeBright, 0.16f * press), 0, 2.5f);
            dl->AddCircle(centre, radius + 4.5f, Alpha(kOrangeBright, 0.07f * press), 0, 3.5f);
        }

        // A control's glass. `press` is the eased 0..1 of it being held,
        // `radius` the corner in points.
        void Glass(ImDrawList* dl, ImVec2 a, ImVec2 b, float radius, float press, ImU32 borderIdle = IM_COL32(241, 237, 227, 255))
        {
            ShadowRect(dl, a, b, radius);
            GlassBody(dl, a, b, radius);
            if (press > 0.0f)
                dl->AddRectFilled(a, b, IM_COL32(255, 100, 26, ImU32(26.0f * press)), radius);
            // The design's inset highlight along the top edge, and a second,
            // fainter one a hair lower: a bevel, not one bright line.
            const float inset = std::min(radius, (b.x - a.x) * 0.35f);
            dl->AddLine(ImVec2(a.x + inset, a.y + 0.5f), ImVec2(b.x - inset, a.y + 0.5f), IM_COL32(255, 255, 255, 20), 1.0f);
            dl->AddLine(ImVec2(a.x + inset + 2.0f, a.y + 1.8f), ImVec2(b.x - inset - 2.0f, a.y + 1.8f), IM_COL32(255, 255, 255, 10), 1.0f);
            // The glass's thickness: a hairline of dark inside the bottom edge.
            dl->AddLine(ImVec2(a.x + inset, b.y - 0.5f), ImVec2(b.x - inset, b.y - 0.5f), IM_COL32(0, 0, 0, 46), 1.0f);
            GlowRect(dl, a, b, radius, press);
            const ImU32 border = Mix(Alpha(borderIdle, kBorderAlpha), Alpha(kOrangeBright, kHeldBorderAlpha), press);
            dl->AddRect(a, b, border, radius, 0, 1.0f);
        }

        void GlassCircle(ImDrawList* dl, ImVec2 centre, float radius, float press, ImU32 borderIdle = IM_COL32(241, 237, 227, 255))
        {
            ShadowCircle(dl, centre, radius);
            dl->AddCircleFilled(centre, radius, IM_COL32(20, 18, 15, ImU32(255.0f * kGlassAlpha)), 0);
            if (press > 0.0f)
                dl->AddCircleFilled(centre, radius, IM_COL32(255, 100, 26, ImU32(26.0f * press)), 0);
            GlowCircle(dl, centre, radius, press);
            const ImU32 border = Mix(Alpha(borderIdle, kBorderAlpha), Alpha(kOrangeBright, kHeldBorderAlpha), press);
            dl->AddCircle(centre, radius, border, 0, 1.0f);
        }

        // The canvas's fractions on the screen: the picture's rectangle, which
        // is what the deck was laid out on.
        struct View
        {
            ImVec2 at, size;   // the picture, in the UI's points
            float cqw = 1.0f;  // 1% of its width

            ImVec2 P(float u, float v) const { return ImVec2(at.x + u * size.x, at.y + v * size.y); }
            ImVec2 Min(const Rect& r) const { return P(r.x, r.y); }
            ImVec2 Max(const Rect& r) const { return P(r.Right(), r.Bottom()); }
            ImVec2 Centre(const Rect& r) const { return P(r.CentreX(), r.CentreY()); }
            float W(const Rect& r) const { return r.w * size.x; }
            float Len(float cqw) const { return cqw * this->cqw; }
        };

        // A control's glass. `scale` is the press's shrink (0.94-0.97 of the
        // design), `radius` the corner in points.
        void Glass(ImDrawList* dl, ImVec2 a, ImVec2 b, float radius, bool held, ImU32 borderIdle = IM_COL32(241, 237, 227, 255))
        {
            dl->AddRect(ImVec2(a.x - 1.0f, a.y - 1.0f), ImVec2(b.x + 1.0f, b.y + 1.0f), IM_COL32(0, 0, 0, 40), radius + 1.0f, 0, 2.0f);
            dl->AddRectFilled(a, b, IM_COL32(20, 18, 15, ImU32(255.0f * kGlassAlpha)), radius);
            if (held)
                dl->AddRectFilled(a, b, IM_COL32(255, 100, 26, 26), radius);
            // The design's inset highlight along the top edge.
            const float inset = std::min(radius, (b.x - a.x) * 0.35f);
            dl->AddLine(ImVec2(a.x + inset, a.y + 0.5f), ImVec2(b.x - inset, a.y + 0.5f), IM_COL32(255, 255, 255, 20), 1.0f);
            const ImU32 border = held ? Alpha(kOrangeBright, kHeldBorderAlpha) : Alpha(borderIdle, kBorderAlpha);
            dl->AddRect(a, b, border, radius, 0, 1.0f);
        }

        void GlassCircle(ImDrawList* dl, ImVec2 centre, float radius, bool held, ImU32 borderIdle = IM_COL32(241, 237, 227, 255))
        {
            dl->AddCircleFilled(centre, radius + 1.0f, IM_COL32(0, 0, 0, 40), 0);
            dl->AddCircleFilled(centre, radius, IM_COL32(20, 18, 15, ImU32(255.0f * kGlassAlpha)), 0);
            if (held)
                dl->AddCircleFilled(centre, radius, IM_COL32(255, 100, 26, 26), 0);
            const ImU32 border = held ? Alpha(kOrangeBright, kHeldBorderAlpha) : Alpha(borderIdle, kBorderAlpha);
            dl->AddCircle(centre, radius, border, 0, 1.0f);
        }

        // An icon, centred, `size` points across (the design's 24 unit box).
        void DrawIcon(ImDrawList* dl, touchicons::Id id, ImVec2 centre, float size, ImU32 colour)
        {
            using namespace touchicons;
            const Icon& icon = IconOf(id);
            const float k = size / kBox;
            const float half = kBox * 0.5f;
            // The largest shape the handoff has is 72 points; 96 is room to
            // spare, and a short one is drawn short rather than overrun.
            ImVec2 pts[96];
            auto place = [&](const float* xy, int count) {
                count = std::min(count, int(std::size(pts)));
                for (int p = 0; p < count; p++)
                    pts[p] = ImVec2(centre.x + (xy[p * 2] - half) * k, centre.y + (xy[p * 2 + 1] - half) * k);
                return count;
            };
            for (int i = 0; i < icon.fills; i++)
            {
                const Fill& f = kFills[icon.fill + i];
                dl->AddConcavePolyFilled(pts, place(f.xy, f.points), Alpha(colour, f.alpha));
            }
            for (int i = 0; i < icon.strokes; i++)
            {
                const Stroke& s = kStrokes[icon.stroke + i];
                const int count = place(s.xy, s.points);
                const float width = std::max(1.0f, s.width * k);
                const ImU32 col = Alpha(colour, s.alpha);
                dl->AddPolyline(pts, count, col, width, s.closed ? ImDrawFlags_Closed : 0);
                if (!s.closed)
                {
                    // The design's round caps: ImGui's polylines are cut flat.
                    dl->AddCircleFilled(pts[0], width * 0.5f, col, 8);
                    dl->AddCircleFilled(pts[count - 1], width * 0.5f, col, 8);
                }
            }
            for (int i = 0; i < icon.rings; i++)
            {
                const Ring& r = kRings[icon.ring + i];
                dl->AddCircle(ImVec2(centre.x + (r.cx - half) * k, centre.y + (r.cy - half) * k), r.radius * k,
                    Alpha(colour, r.alpha), 0, r.width * k);
            }
        }

        // A key's name, plain: no capsule and no border, just the Xbox
        // control's colour, so the icon above it is what the eye lands on. A
        // 1px shadow keeps it legible over a bright road without a panel
        // behind it.
        void KeyText(ImDrawList* dl, const char* text, ImVec2 centre, float textSize, ImU32 colour)
        {
            ImFont* font = ImGui::GetFont();
            const ImVec2 measured = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, text);
            const ImVec2 at(centre.x - measured.x * 0.5f, centre.y - measured.y * 0.5f);
            // Two shadows: the close one is the edge, the loose one its blur.
            dl->AddText(font, textSize, ImVec2(at.x + 2.0f, at.y + 2.0f), IM_COL32(0, 0, 0, 60), text);
            dl->AddText(font, textSize, ImVec2(at.x + 1.0f, at.y + 1.0f), IM_COL32(0, 0, 0, 130), text);
            dl->AddText(font, textSize, at, colour, text);
        }

        // A control's name above its icon, the two centred together on
        // `centre`: the key is inside the button, reading as its caption.
        void IconWithKey(ImDrawList* dl, touchicons::Id icon, const char* key, ImU32 keyColour, ImVec2 centre,
            float iconSize, ImU32 iconColour, float keySize, float gap)
        {
            ImFont* font = ImGui::GetFont();
            const float keyHeight = font->CalcTextSizeA(keySize, FLT_MAX, 0.0f, key).y;
            const float top = centre.y - (keyHeight + gap + iconSize) * 0.5f;
            KeyText(dl, key, ImVec2(centre.x, top + keyHeight * 0.5f), keySize, keyColour);
            DrawIcon(dl, icon, ImVec2(centre.x, top + keyHeight + gap + iconSize * 0.5f), iconSize, iconColour);
        }

        // The design's press: the control shrinks towards its centre.
        void Shrink(ImVec2& a, ImVec2& b, float scale)
        {
            const ImVec2 centre((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
            a = ImVec2(centre.x + (a.x - centre.x) * scale, centre.y + (a.y - centre.y) * scale);
            b = ImVec2(centre.x + (b.x - centre.x) * scale, centre.y + (b.y - centre.y) * scale);
        }

        // The steering panel: the track, the fill to the knob, the knob, and a
        // degree readout, which is the one number the design puts on a control.
        void DrawSteering(ImDrawList* dl, const View& v, const Deck& deck, const Sample& sample)
        {
            const bool held = sample.Pressed(Control::Steer);
            const float press = PressOf(Control::Steer, held);
            const float steer = sample.steerSet ? sample.steer : 0.0f;

            ImVec2 a = v.Min(deck.steer), b = v.Max(deck.steer);
            Glass(dl, a, b, v.Len(1.8f), press);

            // The track: a pill, a hairline border, a centre line marking
            // straight ahead.
            const ImVec2 ta = v.Min(deck.steerTrack), tb = v.Max(deck.steerTrack);
            const float pill = (tb.y - ta.y) * 0.5f;
            dl->AddRectFilled(ta, tb, IM_COL32(250, 247, 240, 14), pill);
            dl->AddRect(ta, tb, IM_COL32(241, 237, 227, 33), pill, 0, 1.0f);
            dl->AddLine(ImVec2((ta.x + tb.x) * 0.5f, ta.y + (tb.y - ta.y) * 0.22f),
                ImVec2((ta.x + tb.x) * 0.5f, tb.y - (tb.y - ta.y) * 0.22f), IM_COL32(255, 123, 46, 122), 1.0f);

            // The end arrows, quiet.
            const float arrow = v.Len(1.3f);
            DrawIcon(dl, touchicons::Id::ChevronLeft, ImVec2(ta.x + v.Len(1.2f) + arrow * 0.5f, (ta.y + tb.y) * 0.5f), arrow,
                Alpha(kChalk, kQuietAlpha));
            DrawIcon(dl, touchicons::Id::ChevronRight, ImVec2(tb.x - v.Len(1.2f) - arrow * 0.5f, (ta.y + tb.y) * 0.5f), arrow,
                Alpha(kChalk, kQuietAlpha));

            // Where the wheel is: the bar from the centre line to the knob.
            const Rect fill = SteerFill(deck, steer);
            if (fill.w > 0.0f)
            {
                ImVec2 fa = v.Min(fill), fb = v.Max(fill);
                dl->AddRectFilled(fa, fb, Alpha(kOrangeBright, 0.82f), (fb.y - fa.y) * 0.5f);
            }

            const Rect knob = KnobRect(deck, steer);
            const ImVec2 kc = v.Centre(knob);
            const float kr = v.W(knob) * 0.5f;
            if (press > 0.01f)
                dl->AddCircle(kc, kr + 2.0f, IM_COL32(255, 100, 26, ImU32(55.0f * press)), 0, 3.0f);  // the design's 3px drag ring
            ShadowCircle(dl, kc, kr);
            dl->AddCircleFilled(kc, kr, IM_COL32(22, 19, 15, 255), 0);
            // The knob's rim: brighter as the drag takes hold, so the thumb
            // can see what it is moving.
            dl->AddCircle(kc, kr, IM_COL32(241, 237, 227, ImU32(87.0f + 60.0f * press)), 0, 1.0f);
            // The key sits inside the knob, above its icon; holding the
            // panel lights it as the other buttons do.
            IconWithKey(dl, touchicons::Id::Steer, "LS", kKeyPlain, kc, v.Len(1.75f),
                Mix(kChalk, kOrangeBright, press), v.Len(0.9f), v.Len(0.08f));

            // The readout, in the panel's head row.
            char degrees[16];
            const int angle = int(std::lround(steer * 90.0f));
            snprintf(degrees, sizeof degrees, angle > 0 ? "+%d\xC2\xB0" : "%d\xC2\xB0", angle);
            ImFont* font = ImGui::GetFont();
            const float textSize = v.Len(1.18f);
            dl->AddText(font, textSize, ImVec2(ta.x, a.y + v.Len(0.9f)), Alpha(kChalk, 0.55f), degrees);
        }

        // The right thumb's cluster: the two pedals on the bottom edge, the
        // three orbs above them.
        void DrawActions(ImDrawList* dl, const View& v, const Deck& deck, const Sample& sample)
        {
            struct Pedal
            {
                Control control;
                touchicons::Id icon;
                const char* key;
                ImU32 keyColour;
                ImU32 border;
            };
            const Pedal pedals[] = {
                { Control::Throttle, touchicons::Id::Throttle, "RT", kKeyPlain, kOrangeBright },
                { Control::Brake, touchicons::Id::Brake, "LT", kKeyPlain, kChalk },
            };
            for (const Pedal& p : pedals)
            {
                const float press = PressOf(p.control, sample.Pressed(p.control));
                ImVec2 a = v.Min(deck.Of(p.control)), b = v.Max(deck.Of(p.control));
                Shrink(a, b, 1.0f + (0.97f - 1.0f) * press);
                Glass(dl, a, b, v.Len(1.25f), press, p.border);
                const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
                // The throttle's icon is orange at rest (it is the pedal that
                // pulls); the others take the accent only while held.
                const ImU32 iconAtRest = p.control == Control::Throttle ? kOrangeBright : Alpha(kChalk, kIconAlpha);
                IconWithKey(dl, p.icon, p.key, p.keyColour, c, v.Len(2.85f), Mix(iconAtRest, kOrangeBright, press), v.Len(1.05f),
                    v.Len(0.2f));
            }

            struct Orb
            {
                Control control;
                touchicons::Id icon;
                const char* key;
                ImU32 keyColour;
                float iconSize;
            };
            const Orb orbs[] = {
                { Control::Nitro, touchicons::Id::Nitro, "B", kKeyB, 3.25f },
                { Control::Handbrake, touchicons::Id::Handbrake, "A", kKeyA, 2.55f },
                { Control::Speedbreaker, touchicons::Id::Bullet, "X", kKeyX, 2.55f },
            };
            for (const Orb& o : orbs)
            {
                const float press = PressOf(o.control, sample.Pressed(o.control));
                ImVec2 a = v.Min(deck.Of(o.control)), b = v.Max(deck.Of(o.control));
                Shrink(a, b, 1.0f + (0.95f - 1.0f) * press);
                const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
                const float radius = (b.x - a.x) * 0.5f;
                if (o.control == Control::Nitro)
                {
                    // The charge ring: the design's identity for the boost,
                    // drawn full -- the game's nitrous is its own state, not
                    // one the runtime can read, and a made-up gauge would
                    // lie. A faint ring outside it is the light a lit ring
                    // would throw; both brighten as the thumb holds.
                    dl->AddCircle(c, radius + v.Len(0.85f), Alpha(kNitro, 0.10f + 0.12f * press), 0, v.Len(0.34f));
                    dl->AddCircle(c, radius + v.Len(0.55f), Alpha(kNitro, 0.50f + 0.28f * press), 0, v.Len(0.22f));
                }
                GlassCircle(dl, c, radius, press);
                IconWithKey(dl, o.icon, o.key, o.keyColour, c, v.Len(o.iconSize), Mix(Alpha(kChalk, kIconAlpha), kOrangeBright, press),
                    v.Len(1.0f), v.Len(0.15f));
            }
        }

        // A corner of a path: the outer ones are rounded with the quarter
        // circle from the point `radius` before `v` to the one after it; a
        // concave one (two arms of the cross meeting) stays the right angle
        // at `v` itself. Returns the path's new end.
        int RoundCorner(ImVec2* pts, int end, ImVec2 prev, ImVec2 v, ImVec2 next, float radius)
        {
            auto unit = [](ImVec2 d) {
                const float n = std::sqrt(d.x * d.x + d.y * d.y);
                return n > 0.0f ? ImVec2(d.x / n, d.y / n) : ImVec2(0.0f, 0.0f);
            };
            const ImVec2 d1 = unit(ImVec2(prev.x - v.x, prev.y - v.y));
            const ImVec2 d2 = unit(ImVec2(next.x - v.x, next.y - v.y));
            const ImVec2 centre(v.x + (d1.x + d2.x) * radius, v.y + (d1.y + d2.y) * radius);
            const float from = std::atan2(-d2.y, -d2.x);
            const float to = std::atan2(-d1.y, -d1.x);
            constexpr float kTwoPi = 6.28318530718f;
            const float sweep = std::remainder(to - from, kTwoPi);
            // Sweeping the other way is the cross's inner corner (where two
            // arms meet): a right angle, not a fillet, so the junction reads
            // as a cross rather than a bowl.
            if (sweep < 0.0f)
            {
                pts[end++] = v;
                return end;
            }
            constexpr int kSteps = 6;
            for (int i = 0; i <= kSteps; i++)
            {
                const float angle = from + sweep * float(i) / float(kSteps);
                pts[end++] = ImVec2(centre.x + std::cos(angle) * radius, centre.y + std::sin(angle) * radius);
            }
            return end;
        }

        // The plus the D-pad's four arms make, its outer corners rounded and
        // the four junctions between the arms right angles: its top arm's
        // left corner first, clockwise. This is both its glass and its only
        // border (no cross between the keys, no hub in the middle).
        int DpadPlus(ImVec2 origin, float cell, float radius, ImVec2* pts)
        {
            const ImVec2 v[] = {
                { origin.x + cell, origin.y }, { origin.x + 2.0f * cell, origin.y },
                { origin.x + 2.0f * cell, origin.y + cell }, { origin.x + 3.0f * cell, origin.y + cell },
                { origin.x + 3.0f * cell, origin.y + 2.0f * cell }, { origin.x + 2.0f * cell, origin.y + 2.0f * cell },
                { origin.x + 2.0f * cell, origin.y + 3.0f * cell }, { origin.x + cell, origin.y + 3.0f * cell },
                { origin.x + cell, origin.y + 2.0f * cell }, { origin.x, origin.y + 2.0f * cell },
                { origin.x, origin.y + cell }, { origin.x + cell, origin.y + cell },
            };
            int count = 0;
            for (int i = 0; i < 12; i++)
                count = RoundCorner(pts, count, v[(i + 11) % 12], v[i], v[(i + 1) % 12], radius);
            return count;
        }

        // The D-pad: four keys on a 3 x 3 grid, on one cross-shaped plate with
        // a single outer border -- no hub, no NAV text, no cross between the
        // keys. Each key's icon of what it opens sits at its centre, and its
        // direction (the D-pad it stands for) at its outer edge: up above,
        // down below, left and right outwards.
        void DrawDpad(ImDrawList* dl, const View& v, const Deck& deck, const Sample& sample)
        {
            const struct
            {
                Control control;
                touchicons::Id icon;   // what it opens
                touchicons::Id arrow;  // which way the D-pad it stands for points
                ImVec2 outward;        // that way, away from the plate's centre
            } keys[] = {
                { Control::DpadUp, touchicons::Id::Shop, touchicons::Id::ShiftUp, ImVec2(0.0f, -1.0f) },
                { Control::DpadLeft, touchicons::Id::Map, touchicons::Id::ChevronLeft, ImVec2(-1.0f, 0.0f) },
                { Control::DpadRight, touchicons::Id::Sms, touchicons::Id::ChevronRight, ImVec2(1.0f, 0.0f) },
                { Control::DpadDown, touchicons::Id::Blacklist, touchicons::Id::ShiftDown, ImVec2(0.0f, 1.0f) },
            };

            // The plate: one rounded cross, the only border on it.
            const ImVec2 origin = v.P(deck.dpadLeft.x, deck.dpadUp.y);
            const float cell = v.W(deck.dpadUp);
            const float radius = v.Len(1.0f);
            ImVec2 path[96];  // 12 corners, 7 points each
            const int pathCount = DpadPlus(origin, cell, radius, path);
            const bool anyHeld = sample.Pressed(Control::DpadUp) || sample.Pressed(Control::DpadDown) ||
                sample.Pressed(Control::DpadLeft) || sample.Pressed(Control::DpadRight);
            const float platePress = PressOf(kDpadPlateSlot, anyHeld);
            // The shadow: two strokes under the fill, the wide one the faint
            // spread, the narrow one the edge.
            dl->AddPolyline(path, pathCount, IM_COL32(0, 0, 0, 12), ImDrawFlags_Closed, 4.0f);
            dl->AddPolyline(path, pathCount, IM_COL32(0, 0, 0, 22), ImDrawFlags_Closed, 2.0f);
            dl->AddConcavePolyFilled(path, pathCount, IM_COL32(20, 18, 15, ImU32(255.0f * kGlassAlpha)));
            dl->AddLine(ImVec2(origin.x + cell + radius, origin.y + 0.5f), ImVec2(origin.x + 2.0f * cell - radius, origin.y + 0.5f),
                IM_COL32(255, 255, 255, 20), 1.0f);
            dl->AddPolyline(path, pathCount,
                Mix(Alpha(kChalk, kBorderAlpha), Alpha(kOrangeBright, kHeldBorderAlpha), platePress), ImDrawFlags_Closed, 1.0f);

            for (const auto& k : keys)
            {
                const float press = PressOf(k.control, sample.Pressed(k.control));
                const ImVec2 ka = v.Min(deck.Of(k.control)), kb = v.Max(deck.Of(k.control));
                if (press > 0.0f)
                {
                    // The key's own square lights, since the plate does not
                    // move as the other controls do.
                    ImVec2 ha = ka, hb = kb;
                    Shrink(ha, hb, 1.0f + (0.90f - 1.0f) * press);
                    dl->AddRectFilled(ha, hb, IM_COL32(255, 100, 26, ImU32(26.0f * press)), v.Len(0.8f));
                }

                const ImVec2 c((ka.x + kb.x) * 0.5f, (ka.y + kb.y) * 0.5f);
                // The action icon stays at the key's centre; the direction
                // (its key value) goes to the key's outer edge, all four the
                // same way: away from the plate's centre.
                const float arrow = v.Len(0.85f), icon = v.Len(1.75f), margin = v.Len(0.35f);
                const ImVec2 arrowAt(c.x + k.outward.x * ((kb.x - ka.x) * 0.5f - margin - arrow * 0.5f),
                    c.y + k.outward.y * ((kb.y - ka.y) * 0.5f - margin - arrow * 0.5f));
                DrawIcon(dl, k.icon, c, icon, Mix(Alpha(kChalk, 0.78f), kOrangeBright, press));
                DrawIcon(dl, k.arrow, arrowAt, arrow, Alpha(kChalk, 0.42f));
            }
        }

        // Start and Back, centred on the top edge.
        void DrawSystem(ImDrawList* dl, const View& v, const Deck& deck, const Sample& sample)
        {
            const struct
            {
                Control control;
                touchicons::Id icon;
                const char* key;
            } buttons[] = {
                { Control::Start, touchicons::Id::Pause, "START" },
                { Control::Back, touchicons::Id::Reset, "BACK" },
            };
            for (const auto& k : buttons)
            {
                const float press = PressOf(k.control, sample.Pressed(k.control));
                ImVec2 a = v.Min(deck.Of(k.control)), b = v.Max(deck.Of(k.control));
                Shrink(a, b, 1.0f + (0.95f - 1.0f) * press);
                Glass(dl, a, b, v.Len(1.15f), press);
                IconWithKey(dl, k.icon, k.key, kKeyPlain, ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), v.Len(1.6f),
                    Mix(Alpha(kChalk, 0.78f), kOrangeBright, press), v.Len(1.0f), v.Len(0.12f));
            }
        }

        // Rear view, quick rear view and camera.
        void DrawViews(ImDrawList* dl, const View& v, const Deck& deck, const Sample& sample)
        {
            const struct
            {
                Control control;
                touchicons::Id icon;
                const char* key;
                ImU32 keyColour;
            } buttons[] = {
                { Control::RearView, touchicons::Id::RearView, "Y", kKeyY },
                { Control::QuickRearView, touchicons::Id::QuickRearView, "LB", kKeyPlain },
                { Control::Camera, touchicons::Id::Camera, "RB", kKeyPlain },
            };
            for (const auto& k : buttons)
            {
                const float press = PressOf(k.control, sample.Pressed(k.control));
                ImVec2 a = v.Min(deck.Of(k.control)), b = v.Max(deck.Of(k.control));
                Shrink(a, b, 1.0f + (0.94f - 1.0f) * press);
                const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
                GlassCircle(dl, c, (b.x - a.x) * 0.5f, press);
                IconWithKey(dl, k.icon, k.key, k.keyColour, c, v.Len(2.08f), Mix(Alpha(kChalk, 0.78f), kOrangeBright, press),
                    v.Len(0.9f), v.Len(0.12f));
            }
        }

        // The shifter: RS's two directions, one above the other, each half
        // the whole of its button; the name sits on the line between them.
        void DrawShifter(ImDrawList* dl, const View& v, const Deck& deck, const Sample& sample)
        {
            const ImVec2 a = v.Min(deck.shifter), b = v.Max(deck.shifter);
            Glass(dl, a, b, v.Len(1.35f), 0.0f);

            const struct
            {
                Control control;
                touchicons::Id icon;
            } halves[] = {
                { Control::ShiftUp, touchicons::Id::ShiftUp },
                { Control::ShiftDown, touchicons::Id::ShiftDown },
            };
            const bool anyHeld = sample.Pressed(Control::ShiftUp) || sample.Pressed(Control::ShiftDown);
            const float platePress = PressOf(kShifterPlateSlot, anyHeld);
            if (platePress > 0.01f)
            {
                // As the other controls' held border, eased: the plate is the
                // two halves' shared rim.
                const ImU32 border = Mix(Alpha(kChalk, kBorderAlpha), Alpha(kOrangeBright, kHeldBorderAlpha), platePress);
                dl->AddRect(a, b, border, v.Len(1.35f), 0, 1.0f);
            }
            for (const auto& half : halves)
            {
                const float press = PressOf(half.control, sample.Pressed(half.control));
                const ImVec2 ha = v.Min(deck.Of(half.control)), hb = v.Max(deck.Of(half.control));
                if (press > 0.0f)
                {
                    // The half lights as a held control's glass does, inset
                    // from the plate's border (which the two halves share).
                    ImVec2 la = ha, lb = hb;
                    Shrink(la, lb, 1.0f + (0.94f - 1.0f) * press);
                    dl->AddRectFilled(la, lb, IM_COL32(255, 100, 26, ImU32(26.0f * press)), v.Len(1.0f));
                }
                DrawIcon(dl, half.icon, ImVec2((ha.x + hb.x) * 0.5f, (ha.y + hb.y) * 0.5f), v.Len(1.9f),
                    Mix(Alpha(kChalk, 0.78f), kOrangeBright, press));
            }

            // The divider, with the control's name on its centre line.
            const float mid = (a.y + b.y) * 0.5f;
            dl->AddLine(ImVec2(a.x, mid), ImVec2(b.x, mid), IM_COL32(241, 237, 227, 26), 1.0f);
            KeyText(dl, "RS", ImVec2((a.x + b.x) * 0.5f, mid), v.Len(0.95f), Alpha(kChalk, 0.72f));
        }
    }

    void DrawTouchOverlay()
    {
        if (!hid::touch::Active())
            return;
        const video::PictureRect picture = video::GetPictureRect();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (!picture.Valid() || display.x <= 0.0f || display.y <= 0.0f)
            return;

        View v;
        v.at = ImVec2(picture.x * display.x, picture.y * display.y);
        v.size = ImVec2(picture.w * display.x, picture.h * display.y);
        v.cqw = v.size.x * 0.01f;

        const Deck& deck = hid::touch::DeckNow();
        const Sample& sample = hid::touch::Snapshot();
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        // ImGui's anti-aliasing fringe is one *layout point*. On a phone or
        // iPad the framebuffer is three pixels to the point, so every vector
        // edge (the icons' strokes and fills, the D-pad's arrows, the glass)
        // came out with a three-pixel soft border -- the icons looked blurry
        // beside the text, which ImGui rasterizes at the framebuffer's own
        // density. The fringe is in this list's units: make it one pixel of
        // the framebuffer, whatever the screen's scale, and put it back for
        // the rest of the frame.
        const float fringe = dl->_FringeScale;
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        dl->_FringeScale = 1.0f / std::max(1.0f, std::max(scale.x, scale.y));

        // Drawn in the design's stacking order: the utility keys and the
        // views under the thumb's clusters.
        DrawDpad(dl, v, deck, sample);
        DrawSystem(dl, v, deck, sample);
        DrawViews(dl, v, deck, sample);
        DrawShifter(dl, v, deck, sample);
        DrawSteering(dl, v, deck, sample);
        DrawActions(dl, v, deck, sample);

        dl->_FringeScale = fringe;
    }
}
