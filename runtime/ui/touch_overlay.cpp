// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The touch deck as it is drawn: hid/touch.h has the fingers and what the game
// is told, hid/touch_layout.h where every control sits on the picture, and the
// controls themselves are the NFS.MW.IOS deck's pictures (ui/deck_art.h) --
// one atlas of dark plates with white glyphs, or a second one the player
// supplied (ui/player_pictures.h).
//
// Drawing them as they are is the whole material: the plate's translucency is
// in the picture's own alpha (the packer baked the second pass the handoff's
// look wanted), so nothing is painted behind it, and a held control is the same
// picture tinted -- the glyph turns the accent's colour while the black plate
// stays black. A picture the player supplied is drawn as it is (a tint would
// flatten its colours) and shows the press as a ring instead. The one thing the
// pictures don't carry is the key's name, so each control that stands for a
// named key keeps its small capsule (RT, A, LB...), which is also what tells a
// player which Xbox button they are pressing.
//
// Nothing here is a hit test and nothing here decides a size: hid/touch_spec.h
// says which picture a control is drawn with, hid/touch_layout.h says where its
// rectangle is and how much of it the picture takes, and the same three feed
// hid::touch::HitTest, so what is drawn is what the finger was tested
// against -- outline, and all.
#include <stdafx.h>
#include "ui.h"
#include "deck_art.h"

#include <hid/touch.h>
#include <hid/touch_spec.h>
#include <user/touch_deck_file.h>
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
            // What a cqw is in canvas fractions, both ways (hid/touch_layout.h's
            // Canvas says the same from the window's side).
            float UPerCqw() const { return 0.01f; }
            float VPerCqw() const { return size.y > 0.0f ? cqw / size.y : 0.0f; }
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

        // One picture (ui/deck_art.h) filling `r`, tinted. One draw: the
        // plate's translucency is baked into the art's own alpha, so there is
        // no second pass to raise it.
        void Picture(ImDrawList* dl, ImTextureRef texture, bool ready, const View& v, const Rect& r,
            const art::Info& info, ImU32 tint, float scale)
        {
            // (The picture's own rectangle inside the slot: a player's 2:1
            // picture keeps its shape and leaves the slot's ends empty rather
            // than being stretched into them. The deck's own are their slots,
            // so nothing moves for them.)
            const Rect box = Fitted(Scaled(r, scale), info, v.UPerCqw(), v.VPerCqw());
            const ImVec2 a = v.Min(box), b = v.Max(box);
            if (ready)
            {
                dl->AddImage(texture, a, b, ImVec2(info.u0, info.v0), ImVec2(info.u1, info.v1), tint);
                return;
            }
            // No art (it failed to decode): the control is still there to
            // press, so draw the disc it would have been and nothing else.
            const ImVec2 centre((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
            const float radius = std::min(b.x - a.x, b.y - a.y) * 0.5f;
            dl->AddCircleFilled(centre, radius, IM_COL32(14, 13, 12, 150), 0);
            dl->AddCircle(centre, radius, Alpha(kKeyPlain, 0.22f), 0, std::max(1.0f, v.Len(0.14f)));
        }

        // A rectangle in dashes: where a control being dragged will land, while
        // the picture of it is up above the finger (DrawDeckLift).
        void DashedRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 colour, float thickness)
        {
            constexpr float kOn = 4.0f, kOff = 3.0f;
            const float step = kOn + kOff;
            for (float x = a.x; x < b.x; x += step)
            {
                const float x1 = std::min(x + kOn, b.x);
                dl->AddLine(ImVec2(x, a.y), ImVec2(x1, a.y), colour, thickness);
                dl->AddLine(ImVec2(x, b.y), ImVec2(x1, b.y), colour, thickness);
            }
            for (float y = a.y; y < b.y; y += step)
            {
                const float y1 = std::min(y + kOn, b.y);
                dl->AddLine(ImVec2(a.x, y), ImVec2(a.x, y1), colour, thickness);
                dl->AddLine(ImVec2(b.x, y), ImVec2(b.x, y1), colour, thickness);
            }
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

        // One control's plate: the picture it wears, at its own draw scale, and
        // -- for a picture of the player's -- a ring while it is held.
        void Plate(ImDrawList* dl, const View& v, const Rect& rect, const art::Info& info, float drawScale, bool held,
            float heldScale, ImU32 accent)
        {
            const bool player = info.sheet != 0;
            const ImTextureRef texture = player ? deckart::PlayerAtlas() : deckart::Atlas();
            const bool ready = player ? deckart::PlayerReady() : deckart::Ready();
            const float scale = drawScale * (held ? heldScale : 1.0f);
            // (A player's picture keeps its own colours: a multiply tint would
            // flatten them to one. The press shows as a ring instead.)
            Picture(dl, texture, ready, v, rect, info, player ? IM_COL32_WHITE : (held ? accent : IM_COL32_WHITE), scale);
            if (player && held)
            {
                const Rect box = Fitted(Scaled(rect, scale), info, v.UPerCqw(), v.VPerCqw());
                const ImVec2 a = v.Min(box), b = v.Max(box);
                dl->AddRect(a, b, accent, std::min(b.x - a.x, b.y - a.y) * 0.45f, 0, std::max(1.5f, v.Len(0.16f)));
            }
        }
    }

    void DrawDeck(ImDrawList* list, const hid::touch::Deck& deck, const hid::touch::Sample& sample,
        const hid::touch::Style& style, const hid::touch::Canvas& canvas, const hid::touch::Overrides& overrides,
        hid::touch::Control selected, ImU32 accent, hid::touch::Control lifted)
    {
        const video::PictureRect picture = video::GetPictureRect();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (!picture.Valid() || display.x <= 0.0f || display.y <= 0.0f)
            return;

        View v;
        v.at = ImVec2(picture.x * display.x, picture.y * display.y);
        v.size = ImVec2(picture.w * display.x, picture.h * display.y);
        v.cqw = v.size.x * 0.01f;

        ImDrawList* dl = list ? list : ImGui::GetBackgroundDrawList();
        // The deck sits on the picture, which is all of it: where the caller
        // handed over a panel's list (the performance overlay's, the editor's),
        // that panel's clip must stand aside, or the deck would be cut down to
        // its corner.
        dl->PushClipRect(ImVec2(0.0f, 0.0f), display, false);

        // The plates, one draw each: every control of the spec table, a held
        // one tinted and a little smaller. (The steering is the exception and
        // comes after, because its base and its stick are two pictures.)
        for (const ControlSpec& spec : kSpecs)
        {
            if (spec.control == Control::Steer || spec.control == lifted)
                continue;
            const art::Id id = art::ArtOf(spec.control);
            Plate(dl, v, deck.Of(spec.control), art::InfoOf(id), kDrawScale[size_t(id)], sample.Pressed(spec.control),
                style.heldScale, accent);
        }

        // The steering: the base where the thumb last took it, the stick at its
        // angle, both keeping the accent while it is held.
        if (lifted != Control::Steer)
        {
            const art::Id baseArt = art::ArtOf(Control::Steer);
            Plate(dl, v, deck.steer, art::InfoOf(baseArt), kDrawScale[size_t(baseArt)], false, style.heldScale, accent);
            const bool steering = sample.steerSet;
            Plate(dl, v, StickRect(deck, steering ? sample.steer : 0.0f), art::InfoOf(art::Id::AnalogStick),
                kDrawScale[size_t(art::Id::AnalogStick)], steering, style.heldScale, accent);
        }

        // The key capsules last, so that no neighbour's plate can cover one.
        for (const ControlSpec& spec : kSpecs)
        {
            if (!spec.key || spec.control == lifted)
                continue;
            const bool held = sample.Pressed(spec.control);
            const art::Id id = art::ArtOf(spec.control);
            const Rect r = Scaled(spec.control == Control::Steer ? deck.steer : deck.Of(spec.control),
                kDrawScale[size_t(id)] * (held ? style.heldScale : 1.0f));
            // (hid/touch_layout.h finds the spot: the picture's own edge, which
            // is the slot's unless the picture is another shape.)
            const art::Info& info = art::InfoOf(id);
            const Point spot = BadgeAnchor(Fitted(r, info, canvas.uPerCqw, canvas.vPerCqw), info);
            // (A key capsule's height comes from its control's size, and from
            // how big the player has made it: a shrunken button should not keep
            // a full-size badge.)
            const float scale = std::clamp(overrides.of[size_t(spec.control)].scale, kMinPlacement, kMaxPlacement);
            KeyTag(dl, v, v.P(spot.u, spot.v), spec.key, KeyColour(spec.keyFace), style.Badge(spec.size, canvas) * scale);
        }

        // The editor's selection: the control it is working on, and the corner
        // its size is dragged by.
        if (selected != Control::Count)
        {
            // (DrawnOf: the picture's own rectangle, which is what the editor's
            // handle is pressed at too -- not the slot, and not the slot
            // without the picture's draw scale.)
            const ImVec2 a = v.Min(DrawnOf(deck, selected)), b = v.Max(DrawnOf(deck, selected));
            const float round = std::min(b.x - a.x, b.y - a.y) * 0.2f;
            if (selected == lifted)
                DashedRect(dl, a, b, accent, 1.5f);  // (the picture is up above the finger)
            else
                dl->AddRect(a, b, accent, round, 0, 1.5f);
            // The corner a control is sized by: a fingertip's target on a touch
            // screen, a small dot under a mouse. A dark rim first, so it reads
            // over any picture.
            const bool touch = ImGui::GetIO().MouseSource == ImGuiMouseSource_TouchScreen;
            const float radius = touch ? 10.0f : 6.0f;
            const ImVec2 corner(b.x, b.y);
            dl->AddCircleFilled(corner, radius + 1.5f, IM_COL32(0, 0, 0, 110), 24);
            dl->AddCircleFilled(corner, radius, accent, 24);
            dl->AddCircleFilled(corner, std::max(2.0f, radius * 0.3f), IM_COL32(255, 255, 255, 230), 12);
        }
        dl->PopClipRect();
    }

    // The control the editor has taken hold of, drawn above the finger that has
    // it: the picture at its own size -- never enlarged, so what is up there is
    // exactly what will land -- a shade under it, and its key's capsule. The
    // place it will land is the dashed outline DrawDeck leaves behind.
    void DrawDeckLift(ImDrawList* list, const hid::touch::Deck& deck, const hid::touch::Style& style,
        const hid::touch::Canvas& canvas, const hid::touch::Rect& slot, hid::touch::Control control, float offsetX,
        float offsetY, ImU32 accent)
    {
        const video::PictureRect picture = video::GetPictureRect();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (!picture.Valid() || display.x <= 0.0f || display.y <= 0.0f || control == Control::Count)
            return;

        View v;
        v.at = ImVec2(picture.x * display.x + offsetX, picture.y * display.y + offsetY);
        v.size = ImVec2(picture.w * display.x, picture.h * display.y);
        v.cqw = v.size.x * 0.01f;

        ImDrawList* dl = list ? list : ImGui::GetBackgroundDrawList();
        dl->PushClipRect(ImVec2(0.0f, 0.0f), display, false);

        const art::Id id = art::ArtOf(control);
        const art::Info& info = art::InfoOf(id);
        const float drawScale = kDrawScale[size_t(id)];
        const Rect box = Fitted(Scaled(slot, drawScale), info, canvas.uPerCqw, canvas.vPerCqw);
        {
            // The shadow: three rounded rectangles growing out of the picture's
            // own, so it reads as lifted off the picture rather than pasted on.
            const ImVec2 a = v.Min(box), b = v.Max(box);
            const float round = std::min(b.x - a.x, b.y - a.y) * 0.5f;
            const ImVec2 under(0.0f, std::max(3.0f, v.Len(0.45f)));
            for (int i = 3; i >= 1; i--)
            {
                const float grow = float(i) * std::max(1.0f, v.Len(0.2f));
                dl->AddRectFilled(ImVec2(a.x - grow + under.x, a.y - grow + under.y),
                    ImVec2(b.x + grow + under.x, b.y + grow + under.y), IM_COL32(0, 0, 0, 22), round + grow);
            }
        }
        // (Plate is the one place a picture is drawn -- DrawDeck's, and this
        // one's -- so the atlas still has a single path to the draw list.)
        Plate(dl, v, slot, info, drawScale, false, style.heldScale, accent);
        if (control == Control::Steer)
        {
            Plate(dl, v, StickRect(deck, 0.0f), art::InfoOf(art::Id::AnalogStick),
                kDrawScale[size_t(art::Id::AnalogStick)], false, style.heldScale, accent);
        }

        const ControlSpec& spec = SpecOf(control);
        if (spec.key)
        {
            const Point spot = BadgeAnchor(box, info);
            KeyTag(dl, v, v.P(spot.u, spot.v), spec.key, KeyColour(spec.keyFace), style.Badge(spec.size, canvas));
        }
        dl->PopClipRect();
    }

    void DrawTouchOverlay(ImDrawList* list)
    {
        if (!hid::touch::Active())
            return;
        DrawDeck(list, hid::touch::DeckNow(), hid::touch::Snapshot(), hid::touch::StyleNow(),
            hid::touch::CanvasNow(), touchdeck::Current().overrides, hid::touch::Control::Count, ui::AccentColour());
    }
}
