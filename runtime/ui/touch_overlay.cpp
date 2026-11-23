// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The touch deck as it is drawn: hid/touch.h has the fingers and what the game
// is told, hid/touch_layout.h where every control sits on the picture, and the
// controls themselves are the NFS.MW.IOS deck's pictures (ui/deck_art.h) --
// one atlas of dark plates with white glyphs.
//
// Drawing them as they are is the whole material: the plate's translucency is
// in the picture's own alpha (the packer baked the second pass the handoff's
// look wanted), so nothing is painted behind it, and a held control is the same
// picture tinted -- the glyph turns the accent's colour while the black plate
// stays black. The one thing the pictures don't carry is the key's name, so
// each control that stands for a named key keeps its small capsule (RT, A,
// LB...), which is also what tells a player which Xbox button they are
// pressing.
//
// Nothing here is a hit test and nothing here decides a size: hid/touch_spec.h
// says which picture a control is drawn with and how much of its rectangle it
// takes, hid/touch_layout.h says where the rectangle is, and the same three
// feed hid::touch::HitTest, so what is drawn is what the finger was tested
// against -- outline, and all.
#include <stdafx.h>
#include "ui.h"
#include "deck_art.h"

#include <hid/touch.h>
#include <hid/touch_spec.h>
#include <video/picture_rect.h>

#include <imgui.h>

namespace ui
{
    namespace
    {
        using namespace hid::touch;

        // The letters a key's name is written in (design/tokens.json's face
        // colours); a plain key, and a held control's glyph, take the accent
        // every screen of ours wears (ui::AccentColour), not an orange of the
        // deck's own.
        constexpr ImU32 kKeyA = IM_COL32(168, 216, 136, 255);
        constexpr ImU32 kKeyB = IM_COL32(255, 142, 131, 255);
        constexpr ImU32 kKeyX = IM_COL32(143, 196, 255, 255);
        constexpr ImU32 kKeyY = IM_COL32(243, 215, 125, 255);
        constexpr ImU32 kKeyPlain = IM_COL32(241, 237, 227, 255);

        ImU32 KeyColour(KeyFace face)
        {
            switch (face)
            {
            case KeyFace::Plain: return kKeyPlain;
            case KeyFace::A: return kKeyA;
            case KeyFace::B: return kKeyB;
            case KeyFace::X: return kKeyX;
            case KeyFace::Y: return kKeyY;
            case KeyFace::None: break;
            }
            return kKeyPlain;
        }

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
            float Len(float cqw) const { return cqw * this->cqw; }
        };

        // `r` about its centre, `scale` of its size (a held control's press).
        Rect Scaled(const Rect& r, float scale)
        {
            if (scale == 1.0f)
                return r;
            const float w = r.w * scale, h = r.h * scale;
            return { r.x + (r.w - w) * 0.5f, r.y + (r.h - h) * 0.5f, w, h };
        }

        // One control's picture (ui/deck_art.h) filling `r`, tinted. One draw:
        // the plate's translucency is baked into the art's own alpha, so there
        // is no second pass to raise it.
        void Picture(ImDrawList* dl, ImTextureRef atlas, bool art, const View& v, const Rect& r, ui::deckart::Art id,
            ImU32 tint, float scale)
        {
            const Rect box = Scaled(r, scale);
            const ImVec2 a = v.Min(box), b = v.Max(box);
            if (art)
            {
                const hid::touch::art::Info& info = deckart::Info(id);
                dl->AddImage(atlas, a, b, ImVec2(info.u0, info.v0), ImVec2(info.u1, info.v1), tint);
                return;
            }
            // No art (it failed to decode): the control is still there to
            // press, so draw the disc it would have been and nothing else.
            const ImVec2 centre((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
            const float radius = std::min(b.x - a.x, b.y - a.y) * 0.5f;
            dl->AddCircleFilled(centre, radius, IM_COL32(14, 13, 12, 150), 0);
            dl->AddCircle(centre, radius, Alpha(kKeyPlain, 0.22f), 0, std::max(1.0f, v.Len(0.14f)));
        }

        // A key capsule: the name of the Xbox control a button stands for, its
        // middle on `centre` so that it straddles the edge it marks.
        void KeyTag(ImDrawList* dl, const View& v, ImVec2 centre, const char* text, ImU32 colour, float heightCqw)
        {
            if (!text || !text[0] || heightCqw <= 0.0f)
                return;
            ImFont* font = ImGui::GetFont();
            const float height = v.Len(heightCqw);
            const float textSize = height * 0.56f;
            const float pad = height * 0.24f;
            const ImVec2 measured = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, text);
            const float width = std::max(height * 1.24f, measured.x + pad * 2.0f);
            const ImVec2 a(centre.x - width * 0.5f, centre.y - height * 0.5f), b(centre.x + width * 0.5f, centre.y + height * 0.5f);
            dl->AddRectFilled(a, b, IM_COL32(10, 9, 8, 128), height * 0.5f);
            dl->AddRect(a, b, Alpha(colour, 0.62f), height * 0.5f, 0, 1.0f);
            dl->AddText(font, textSize, ImVec2(a.x + (width - measured.x) * 0.5f, a.y + (height - textSize) * 0.5f), colour, text);
        }
    }

    void DrawTouchOverlay(ImDrawList* list)
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
        const Canvas& canvas = hid::touch::CanvasNow();
        const Sample& sample = hid::touch::Snapshot();
        const Style& style = hid::touch::StyleNow();
        ImDrawList* dl = list ? list : ImGui::GetBackgroundDrawList();
        // The deck sits on the picture, which is all of it: where the caller
        // handed over a panel's list (the performance overlay's), that panel's
        // clip must stand aside, or the deck would be cut down to its corner.
        dl->PushClipRect(ImVec2(0.0f, 0.0f), display, false);
        const ImTextureRef atlas = deckart::Atlas();
        const bool art = deckart::Ready();
        const ImU32 accent = ui::AccentColour();

        // The plates, one draw each: every control of the spec table, a held
        // one tinted and a little smaller. (The steering is the exception and
        // comes after, because its base and its stick are two pictures.)
        for (const ControlSpec& spec : kSpecs)
        {
            if (spec.control == Control::Steer)
                continue;
            const bool held = sample.Pressed(spec.control);
            Picture(dl, atlas, art, v, deck.Of(spec.control), spec.art, held ? accent : IM_COL32_WHITE,
                spec.drawScale * (held ? style.heldScale : 1.0f));
        }

        // The steering: the base where the thumb last took it, the stick at its
        // angle, both keeping the accent while it is held.
        Picture(dl, atlas, art, v, deck.steer, deckart::Art::AnalogBase, IM_COL32_WHITE, 1.0f);
        const bool steering = sample.steerSet;
        Picture(dl, atlas, art, v, StickRect(deck, steering ? sample.steer : 0.0f), deckart::Art::AnalogStick,
            steering ? accent : IM_COL32_WHITE, 1.0f);

        // The key capsules last, so that no neighbour's plate can cover one.
        for (const ControlSpec& spec : kSpecs)
        {
            if (!spec.key)
                continue;
            const bool held = sample.Pressed(spec.control);
            const Rect r = Scaled(spec.control == Control::Steer ? deck.steer : deck.Of(spec.control),
                spec.drawScale * (held ? style.heldScale : 1.0f));
            // (hid/touch_layout.h finds the spot: the picture's own edge.)
            const Point spot = BadgeAnchor(r, deckart::Info(spec.art));
            // (A key capsule's height comes from its control's size: a 4.45 cqw
            // D-pad cell could not carry a 6 cqw pedal's badge.)
            KeyTag(dl, v, v.P(spot.u, spot.v), spec.key, KeyColour(spec.keyFace), style.Badge(spec.size, canvas));
        }
        dl->PopClipRect();
    }
}
