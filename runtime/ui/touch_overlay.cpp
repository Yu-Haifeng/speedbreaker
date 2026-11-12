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
// and it draws no blurred shadow. The glass is therefore flat and a little
// denser than the design's gradient (which is nearly flat at this size) and
// carries a hairline of black under its border instead of the shadow, which
// keeps it legible over a sunlit road.
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
        // What a key capsule names its control in.
        constexpr ImU32 kKeyA = IM_COL32(168, 216, 136, 255);
        constexpr ImU32 kKeyB = IM_COL32(255, 142, 131, 255);
        constexpr ImU32 kKeyX = IM_COL32(143, 196, 255, 255);
        constexpr ImU32 kKeyY = IM_COL32(243, 215, 125, 255);
        constexpr ImU32 kKeyPlain = IM_COL32(241, 237, 227, 255);

        // The opacities the design sets on top of those.
        constexpr float kGlassAlpha = 0.60f;   // (the design's .70-.78 gradient, flat, thinned)
        constexpr float kBorderAlpha = 0.19f;
        constexpr float kHeldBorderAlpha = 0.78f;
        constexpr float kIconAlpha = 0.90f;
        constexpr float kKeyBorderAlpha = 0.62f;
        constexpr float kNitroRingAlpha = 0.72f;
        constexpr float kQuietAlpha = 0.32f;  // the track's end arrows, the shifter's header

        ImU32 Alpha(ImU32 colour, float a)
        {
            const float value = std::clamp(float((colour >> IM_COL32_A_SHIFT) & 0xFF) * a, 0.0f, 255.0f);
            return (colour & ~IM_COL32_A_MASK) | (ImU32(value) << IM_COL32_A_SHIFT);
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

        // A key capsule: the design's outline badge naming the Xbox control a
        // button stands for. Its right edge ends at `right`, its top at `top`;
        // `heightCqw` is its size, which follows the control it sits on (a
        // 5.45 cqw view button cannot carry a 7.45 cqw orb's badge).
        void KeyTag(ImDrawList* dl, const View& v, float right, float top, const char* text, ImU32 colour, float heightCqw)
        {
            ImFont* font = ImGui::GetFont();
            const float height = v.Len(heightCqw);
            const float textSize = height * 0.56f;
            const float pad = height * 0.24f;
            const ImVec2 measured = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, text);
            const float width = std::max(height * 1.24f, measured.x + pad * 2.0f);
            const ImVec2 a(right - width, top), b(right, top + height);
            dl->AddRectFilled(a, b, IM_COL32(10, 9, 8, 102), height * 0.5f);
            dl->AddRect(a, b, Alpha(colour, kKeyBorderAlpha), height * 0.5f, 0, 1.0f);
            dl->AddText(font, textSize, ImVec2(a.x + (width - measured.x) * 0.5f, a.y + (height - textSize) * 0.5f), colour, text);
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
            const float steer = sample.steerSet ? sample.steer : 0.0f;

            ImVec2 a = v.Min(deck.steer), b = v.Max(deck.steer);
            Glass(dl, a, b, v.Len(1.8f), false);

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
            if (held)
                dl->AddCircle(kc, kr + 2.0f, IM_COL32(255, 100, 26, 55), 0, 3.0f);  // the design's 3px drag ring
            dl->AddCircleFilled(kc, kr + 1.0f, IM_COL32(0, 0, 0, 90), 0);
            dl->AddCircleFilled(kc, kr, IM_COL32(22, 19, 15, 255), 0);
            dl->AddCircle(kc, kr, IM_COL32(241, 237, 227, 87), 0, 1.0f);
            DrawIcon(dl, touchicons::Id::Steer, kc, v.Len(2.5f), kChalk);

            // The readout and the control's name, in the panel's head row.
            char degrees[16];
            const int angle = int(std::lround(steer * 90.0f));
            snprintf(degrees, sizeof degrees, angle > 0 ? "+%d\xC2\xB0" : "%d\xC2\xB0", angle);
            ImFont* font = ImGui::GetFont();
            const float textSize = v.Len(1.18f);
            dl->AddText(font, textSize, ImVec2(ta.x, a.y + v.Len(0.9f)), Alpha(kChalk, 0.55f), degrees);
            KeyTag(dl, v, b.x - v.Len(1.35f), a.y + v.Len(0.85f), "LS", kKeyPlain, 1.8f);
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
                const bool held = sample.Pressed(p.control);
                ImVec2 a = v.Min(deck.Of(p.control)), b = v.Max(deck.Of(p.control));
                Shrink(a, b, held ? 0.97f : 1.0f);
                Glass(dl, a, b, v.Len(1.25f), held, p.border);
                const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
                // The throttle's icon is orange at rest (it is the pedal that
                // pulls); the others take the accent only while held.
                DrawIcon(dl, p.icon, c, v.Len(2.85f), held || p.control == Control::Throttle ? kOrangeBright : Alpha(kChalk, kIconAlpha));
                KeyTag(dl, v, b.x - v.Len(0.55f), a.y + v.Len(0.55f), p.key, p.keyColour, 1.95f);
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
                const bool held = sample.Pressed(o.control);
                ImVec2 a = v.Min(deck.Of(o.control)), b = v.Max(deck.Of(o.control));
                if (held)
                    Shrink(a, b, 0.95f);
                const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
                const float radius = (b.x - a.x) * 0.5f;
                if (o.control == Control::Nitro)
                {
                    // The charge ring: the design's identity for the boost,
                    // drawn full -- the game's nitrous is its own state, not
                    // one the runtime can read, and a made-up gauge would lie.
                    dl->AddCircle(c, radius + v.Len(0.55f), Alpha(kNitro, kNitroRingAlpha), 0, v.Len(0.22f));
                }
                GlassCircle(dl, c, radius, held);
                DrawIcon(dl, o.icon, c, v.Len(o.iconSize), held ? kOrangeBright : Alpha(kChalk, kIconAlpha));
                KeyTag(dl, v, b.x + v.Len(0.1f), a.y - v.Len(0.1f), o.key, o.keyColour, 1.9f);
            }
        }

        // The D-pad: four keys on a 3 x 3 grid, and the hub between them with
        // the cluster's name. (The design rounds each key outwards, 1.2 cqw on
        // its far corners and 0.4 on the others; ImGui rounds all four alike,
        // which at this size is the same thing.)
        void DrawDpad(ImDrawList* dl, const View& v, const Deck& deck, const Sample& sample)
        {
            const struct
            {
                Control control;
                touchicons::Id icon;   // what it opens
                touchicons::Id arrow;  // which way the D-pad it stands for points
            } keys[] = {
                { Control::DpadUp, touchicons::Id::Shop, touchicons::Id::ShiftUp },
                { Control::DpadLeft, touchicons::Id::Map, touchicons::Id::ChevronLeft },
                { Control::DpadRight, touchicons::Id::Sms, touchicons::Id::ChevronRight },
                { Control::DpadDown, touchicons::Id::Blacklist, touchicons::Id::ShiftDown },
            };
            for (const auto& k : keys)
            {
                const bool held = sample.Pressed(k.control);
                ImVec2 a = v.Min(deck.Of(k.control)), b = v.Max(deck.Of(k.control));
                Shrink(a, b, held ? 0.94f : 1.0f);
                Glass(dl, a, b, v.Len(1.0f), held);
                DrawIcon(dl, k.icon, ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), v.Len(1.95f),
                    held ? kOrangeBright : Alpha(kChalk, 0.78f));
                DrawIcon(dl, k.arrow, ImVec2(b.x - v.Len(0.78f), b.y - v.Len(0.66f)), v.Len(1.05f), Alpha(kChalk, 0.42f));
            }

            const ImVec2 a = v.P(deck.dpadLeft.Right(), deck.dpadUp.Bottom());
            const ImVec2 b = v.P(deck.dpadRight.x, deck.dpadDown.y);
            dl->AddRectFilled(a, b, IM_COL32(10, 9, 8, 90), v.Len(0.5f));
            dl->AddRect(a, b, IM_COL32(241, 237, 227, 41), v.Len(0.5f), 0, 1.0f);
            ImFont* font = ImGui::GetFont();
            const float textSize = v.Len(0.78f);
            const ImVec2 measured = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, "NAV");
            dl->AddText(font, textSize, ImVec2((a.x + b.x - measured.x) * 0.5f, (a.y + b.y - measured.y) * 0.5f),
                IM_COL32(241, 237, 227, 107), "NAV");
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
                const bool held = sample.Pressed(k.control);
                ImVec2 a = v.Min(deck.Of(k.control)), b = v.Max(deck.Of(k.control));
                Shrink(a, b, held ? 0.95f : 1.0f);
                Glass(dl, a, b, v.Len(1.15f), held);
                DrawIcon(dl, k.icon, ImVec2(a.x + v.Len(1.7f), (a.y + b.y) * 0.5f), v.Len(1.6f),
                    held ? kOrangeBright : Alpha(kChalk, 0.78f));
                KeyTag(dl, v, b.x - v.Len(0.6f), a.y + v.Len(0.5f), k.key, kKeyPlain, 1.7f);
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
                const bool held = sample.Pressed(k.control);
                ImVec2 a = v.Min(deck.Of(k.control)), b = v.Max(deck.Of(k.control));
                Shrink(a, b, held ? 0.94f : 1.0f);
                const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
                GlassCircle(dl, c, (b.x - a.x) * 0.5f, held);
                DrawIcon(dl, k.icon, c, v.Len(2.08f), held ? kOrangeBright : Alpha(kChalk, 0.78f));
                KeyTag(dl, v, b.x + v.Len(0.1f), a.y - v.Len(0.1f), k.key, k.keyColour, 1.6f);
            }
        }

        // The shifter: RS's two directions, one above the other.
        void DrawShifter(ImDrawList* dl, const View& v, const Deck& deck, const Sample& sample)
        {
            ImVec2 a = v.Min(deck.shifter), b = v.Max(deck.shifter);
            Glass(dl, a, b, v.Len(1.35f), false);

            // The header names the control the two halves stand for.
            ImFont* font = ImGui::GetFont();
            const float textSize = v.Len(0.8f);
            const ImVec2 measured = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, "RS");
            dl->AddText(font, textSize, ImVec2((a.x + b.x - measured.x) * 0.5f, a.y + v.Len(0.55f)), Alpha(kChalk, 0.55f), "RS");
            const float headerY = a.y + v.Len(1.8f);
            dl->AddLine(ImVec2(a.x, headerY), ImVec2(b.x, headerY), IM_COL32(241, 237, 227, 26), 1.0f);

            const struct
            {
                Control control;
                touchicons::Id icon;
            } halves[] = {
                { Control::ShiftUp, touchicons::Id::ShiftUp },
                { Control::ShiftDown, touchicons::Id::ShiftDown },
            };
            for (size_t i = 0; i < std::size(halves); i++)
            {
                const bool held = sample.Pressed(halves[i].control);
                ImVec2 ha = v.Min(deck.Of(halves[i].control)), hb = v.Max(deck.Of(halves[i].control));
                if (held)
                    dl->AddRectFilled(ha, hb, IM_COL32(255, 255, 255, 12));
                if (i == 1)
                    dl->AddLine(ImVec2(ha.x, ha.y), ImVec2(hb.x, ha.y), IM_COL32(241, 237, 227, 26), 1.0f);
                DrawIcon(dl, halves[i].icon, ImVec2((ha.x + hb.x) * 0.5f, (ha.y + hb.y) * 0.5f), v.Len(1.5f),
                    held ? kOrangeBright : Alpha(kChalk, 0.78f));
            }
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

        // Drawn in the design's stacking order: the utility keys and the
        // views under the thumb's clusters.
        DrawDpad(dl, v, deck, sample);
        DrawSystem(dl, v, deck, sample);
        DrawViews(dl, v, deck, sample);
        DrawShifter(dl, v, deck, sample);
        DrawSteering(dl, v, deck, sample);
        DrawActions(dl, v, deck, sample);
    }
}
