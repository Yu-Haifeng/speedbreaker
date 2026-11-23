// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Where the touch controls sit on the game's picture, in the handoff's units:
// a cqw is 1% of the picture's width, so the deck keeps its place on the
// picture whatever the screen's shape, and its size follows the picture's.
//
// Header only and SDL-free, for tests/touch_deck_test.cpp: hid/touch_input.h
// decides what a finger does there, ui/touch_overlay.cpp draws it (with the
// NFS.MW.IOS deck's own art, ui/deck_art.h), and hid/touch.cpp hands both the
// canvas.
//
// The zones are design/layout.json's -- steering on the left thumb, the pedals
// and face buttons on the right one, Start and Back with the D-pad in the top
// left corner and the views with the shifter in the top right -- but the
// controls are that art's round buttons now rather than the handoff's plates
// and sliders. Three things follow the art: the steering is a
// round stick (an 18 cqw base with a 7.4 cqw stick in it), the two pedals stand
// up like their pictures, and the four face buttons sit on a fan around the
// right thumb's base -- A, X, B, Y from the cluster's bottom left to its top
// right, one step apart -- so that a thumb reaches each one by turning,
// without crossing another.
//
// Every size is a Style's, one place to change and one place for the automatic
// sizing this is built to grow into (ScaleFor): a size class gives a control
// one dimension, and the other comes from its picture where the picture knows
// better. The pedals are the case that matters: gas and brake are not the same
// shape (0.81 and 0.98 wide for their height), so a pedal's height is its
// width over its art's aspect, and neither is stretched into the other's box.
//
// Two numbers follow the picture rather than a fixed 16:9 canvas:
//   - Down, a cqw is a different fraction of the canvas than across it: a
//     picture wider than 16:9 (an iPhone filled by Hor+, game/ultrawide) is
//     shorter in cqw, and one narrower is taller. Canvas::vPerCqw carries it.
//   - The 2.65 cqw margin gives way to the screen's safe area where that
//     reaches further in (an iPhone's notch and rounded corners in
//     landscape): InsetsFor().
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <hid/touch_art.h>
#include <hid/touch_control.h>
#include <hid/touch_spec.h>

namespace hid::touch
{
    namespace layout
    {
        constexpr float kDegrees = 0.017453292519943295f;
    }

    // The canvas a deck is laid out on: the picture's shape, and how far its
    // edges are kept clear.
    struct Canvas
    {
        // 1 cqw across (always 0.01) and down (the picture's shape: 0.01 at a
        // 1:1 picture, 0.017778 at 16:9).
        float uPerCqw = 0.01f;
        float vPerCqw = 0.0177778f;
        // cqw clear of the canvas's edges: the design's 2.65, or more where the
        // screen's safe area reaches in.
        float left = 2.65f, top = 2.65f, right = 2.65f, bottom = 2.65f;
        // What a cqw is worth on this screen, in the UI's points (1/163 inch
        // on iOS): the picture's width over a hundred. 0 where nobody said
        // (a test, a frame with no picture), and the deck keeps the design's
        // own sizes.
        float pointsPerCqw = 0.0f;

        // The picture's shape, width / height.
        float Aspect() const { return uPerCqw > 0.0f ? vPerCqw / uPerCqw : 0.0f; }
        // The canvas's height in cqw: 56.25 at 16:9, more when the picture is
        // narrower than that.
        float HeightCqw() const { return vPerCqw > 0.0f ? 1.0f / vPerCqw : 0.0f; }
    };

    // What the screen asks for: a thumb is the same size on every device, so
    // the deck follows the picture's size in points rather than its width in
    // cqw. The design's face button is 5.45 cqw (`referenceCqw`) and 70 points
    // on its 1280x720 reference; a phone's picture is two thirds of that
    // across, so its deck grows, and an iPad's a quarter more, so its shrinks.
    // 1 where the canvas has not been told what a cqw is worth.
    inline float ScaleFor(const Canvas& canvas, float referenceCqw)
    {
        constexpr float kThumbPoints = 70.0f;  // (a 5.45 cqw button at 1280x720, 1 cqw = 12.8)
        if (canvas.pointsPerCqw <= 0.0f || referenceCqw <= 0.0f)
            return 1.0f;
        return kThumbPoints / (referenceCqw * canvas.pointsPerCqw);
    }

    // The smallest the deck goes: below this a button is no longer a thumb's,
    // and the player asked for smaller anyway.
    constexpr float kMinScale = 0.75f;

    // The largest it goes on this canvas. Two stacks have to stay apart -- the
    // shifter's column down from the top margin, the face buttons' fan up from
    // the bottom one -- and together they are 38.9 cqw of deck at 1:1 (6.9 to
    // the shifter, two gears and their gap, then a fan's radius and a face
    // button's reach). Past that they would meet in the middle of the picture.
    inline float MaxScale(const Canvas& canvas)
    {
        constexpr float kStacks = 38.9f, kBetween = 0.5f;
        const float room = canvas.HeightCqw() - canvas.top - canvas.bottom - kBetween;
        return std::clamp(room / kStacks, kMinScale, 1.25f);
    }

    // Every size the deck is made of, in cqw. One struct, so that "make the
    // buttons bigger" is one number (scale) rather than seventeen.
    struct Style
    {
        // The player's setting (Settings > Input > Touch Size): 1 is what the
        // screen asks for, which is ScaleFor's.
        float scale = 1.0f;
        // cqw clear of the picture's edges, where the safe area does not reach
        // further in.
        float margin = 2.65f;
        // The smallest a control's touch area should be, as a diameter: the
        // design's 7.45. Best effort -- a control packed against its neighbour
        // keeps the gap (see Layout's pads).
        float minTouch = 7.45f;
        // Kept between two controls' inflated areas so they can never meet.
        float padEpsilon = 0.02f;
        // How much a held control shrinks, as the handoff's buttons do.
        float heldScale = 0.95f;

        // The analog stick: an 18 cqw base with a 7.4 cqw stick in it (the
        // art's own proportions, a size down from the handoff's 22), the stick
        // travelling until its edge meets the base's.
        float stickBase = 18.0f, stickKnob = 7.4f;
        // Where a thumb can take the stick: the picture's left, below the
        // D-pad.
        float steerGrabWidth = 42.0f;  // from the canvas's left edge

        // The action cluster: two pedals standing on the canvas's bottom
        // margin (the throttle inside, the brake at the margin), 0.8 apart --
        // a thumb's width at this size. A pedal is this wide and as tall as
        // its own picture is (gas.png is taller for its width than brake.png).
        float pedalWidth = 6.5f, pedalGap = 0.8f;
        // The four face buttons are the size of the view row's (topRowDiameter,
        // the LB and RB above them): the cluster's four are pressed by a thumb
        // that is already there, and do not need the room of a pedal.
        float face = 5.45f;
        // They sit on a quarter circle around a point on the bottom margin,
        // half a button in from the cluster's right edge, so the fan turns
        // from the bottom edge up to vertical with equal steps.
        float fanRadius = 18.0f;

        // The D-pad: a 3 x 3 grid of 4.45 cqw cells, the arrows on its edges.
        float dpadCell = 4.45f;

        // The top row: Start and Back on the left, quick rear view and camera
        // on the right, 5.45 across and 0.55 apart.
        float topRowDiameter = 5.45f, topRowGap = 0.55f;
        // Start and Back, the D-pad's own corner, with the D-pad pushed down
        // under them.
        float systemBelow = 0.55f;  // from the D-pad's top edge

        // The shifter: one round button over the other, under the camera.
        float shiftDiameter = 5.45f, shiftGap = 0.4f;
        float shifterBelowViews = 6.9f;  // 9.55 - 2.65, from the row's top

        // A key's name, as a capsule's height in cqw, per size class (see
        // ui/touch_overlay.cpp's KeyTag).
        float badgeSystem = 1.7f, badgeView = 1.6f, badgeFace = 1.5f, badgePedal = 1.95f, badgeStick = 1.8f;

        // The one dimension a size class gives, before the picture fills in the
        // other (and before scale).
        float Of(SizeClass c) const
        {
            switch (c)
            {
            case SizeClass::StickBase: return stickBase;
            case SizeClass::Stick: return stickKnob;
            case SizeClass::Pedal: return pedalWidth;
            case SizeClass::Face: return face;
            case SizeClass::Dpad: return dpadCell;
            case SizeClass::View: return topRowDiameter;
            case SizeClass::Shift: return shiftDiameter;
            case SizeClass::System: return topRowDiameter;
            }
            return 0.0f;
        }

        // The deck's size on this canvas: what the screen asks for (as far as
        // the layout can take it), times the player's setting -- the setting
        // cannot make the deck overlap itself, but it can always make it
        // smaller. Every size, gap and touch pad goes through it, so the deck
        // stays one shape.
        float Factor(const Canvas& canvas) const
        {
            const float cap = MaxScale(canvas);
            const float screen = std::min(ScaleFor(canvas, face), cap);
            return std::clamp(screen * scale, kMinScale, cap);
        }

        float Size(SizeClass c, const Canvas& canvas) const { return Of(c) * Factor(canvas); }

        // A key's name's capsule, in cqw: bigger on a bigger button, and only
        // where the picture does not say what the control is (a D-pad cell's
        // arrow, a gear's sign).
        float BadgeOf(SizeClass c) const
        {
            switch (c)
            {
            case SizeClass::System: return badgeSystem;
            case SizeClass::View: return badgeView;
            case SizeClass::Face: return badgeFace;
            case SizeClass::Pedal: return badgePedal;
            case SizeClass::StickBase:
            case SizeClass::Stick: return badgeStick;
            case SizeClass::Dpad:
            case SizeClass::Shift: break;
            }
            return 0.0f;
        }

        float Badge(SizeClass c, const Canvas& canvas) const { return BadgeOf(c) * Factor(canvas); }
    };

    // The canvas of a picture of this shape, with the design's margins.
    inline Canvas CanvasFor(float aspect, const Style& style = Style{})
    {
        Canvas c;
        c.vPerCqw = aspect * 0.01f;
        c.left = c.top = c.right = c.bottom = style.margin;
        return c;
    }

    // The design's margin, or the safe area where it reaches further in.
    // `safe*` and `pictureWidth` in pixels: a pixel is 100 / pictureWidth of a
    // cqw.
    inline Canvas InsetsFor(Canvas canvas, float pictureWidth, float safeLeft, float safeTop, float safeRight, float safeBottom)
    {
        if (pictureWidth > 0.0f)
        {
            const float perCqw = 100.0f / pictureWidth;
            canvas.left = std::max(canvas.left, safeLeft * perCqw);
            canvas.top = std::max(canvas.top, safeTop * perCqw);
            canvas.right = std::max(canvas.right, safeRight * perCqw);
            canvas.bottom = std::max(canvas.bottom, safeBottom * perCqw);
        }
        return canvas;
    }

    // Every control's rectangle. Nothing here is a hit test: `padCqw` is how
    // far a control's touch area reaches past its own rectangle, which
    // HitTest turns into "on the picture, or within a thumb's reach of it".
    struct Deck
    {
        float uPerCqw = 0.01f, vPerCqw = 0.0177778f;
        // The stick's base and the stick at centre, how far the stick travels
        // (a canvas fraction), and the area a thumb takes it from.
        Rect steer, steerStick, steerGrab;
        float steerTravel = 0.0f;
        // The action cluster: the pedals, and the four face buttons (rear view
        // is the cluster's Y).
        Rect throttle, brake, handbrake, nitro, speedbreaker;
        Rect dpadUp, dpadDown, dpadLeft, dpadRight;
        Rect rearView, quickRearView, camera;  // (rearView is the face button Y)
        Rect shiftUp, shiftDown;
        Rect start, back;
        // How far each control's touch area reaches past its rectangle, in cqw.
        float padCqw[kControls] = {};

        Rect Of(Control c) const;
    };

    // The seventeen fields above, by control: one list, so Of() and Layout
    // cannot drift apart.
    inline constexpr Rect Deck::*kRectOf[kControls] = {
        &Deck::steer,
        &Deck::throttle, &Deck::brake, &Deck::handbrake, &Deck::nitro, &Deck::speedbreaker,
        &Deck::rearView, &Deck::quickRearView, &Deck::camera,
        &Deck::shiftUp, &Deck::shiftDown,
        &Deck::dpadUp, &Deck::dpadDown, &Deck::dpadLeft, &Deck::dpadRight,
        &Deck::back, &Deck::start,
    };

    inline Rect Deck::Of(Control c) const
    {
        return size_t(c) < kControls ? this->*kRectOf[size_t(c)] : Rect{};
    }

    // Where the stick's base sits when nothing has taken it: the margin in from
    // the left and the bottom.
    inline Point SteerHomeCentre(const Canvas& canvas, const Style& style = Style{})
    {
        const float heightCqw = canvas.HeightCqw();
        const float half = style.Size(SizeClass::StickBase, canvas) * 0.5f;
        return { (canvas.left + half) * canvas.uPerCqw, (heightCqw - canvas.bottom - half) * canvas.vPerCqw };
    }

    // (The two-argument form is below.)
    inline Deck Layout(const Canvas& canvas, Point centre, const Style& style = Style{});

    // The deck on this canvas, with the stick in the handoff's corner.
    inline Deck Layout(const Canvas& canvas, const Style& style = Style{})
    {
        return Layout(canvas, SteerHomeCentre(canvas, style), style);
    }

    // The deck with the stick's base centred on `centre`, kept inside the
    // canvas's margins. Every size is the style's; only the stick moves.
    inline Deck Layout(const Canvas& canvas, Point centre, const Style& style)
    {
        Deck d;
        d.uPerCqw = canvas.uPerCqw;
        d.vPerCqw = canvas.vPerCqw;
        const float u = canvas.uPerCqw, v = canvas.vPerCqw;
        const float heightCqw = canvas.HeightCqw();
        auto rect = [u, v](float xCqw, float yCqw, float wCqw, float hCqw) {
            return Rect{ xCqw * u, yCqw * v, wCqw * u, hCqw * v };
        };
        auto square = [&rect](float centreX, float centreY, float side) {
            return rect(centreX - side * 0.5f, centreY - side * 0.5f, side, side);
        };
        // (A canvas too small for a control's margins keeps it in the middle
        // rather than in a range that has turned inside out.)
        auto keep = [](float value, float lo, float hi) { return hi < lo ? (lo + hi) * 0.5f : std::clamp(value, lo, hi); };
        const float factor = style.Factor(canvas);  // (Size() applies it too)
        auto size = [&style, &canvas](SizeClass c) { return style.Size(c, canvas); };

        // A pedal: this wide, and as tall as its own picture is for that width,
        // standing on `bottomCqw` -- the two are not the same shape, and neither
        // is stretched into the other's.
        auto pedal = [&rect, &size](float rightCqw, float bottomCqw, art::Id id) {
            const float w = size(SizeClass::Pedal);
            const float aspect = art::kArt[size_t(id)].aspect;
            const float h = aspect > 0.0f ? w / aspect : w;
            return rect(rightCqw - w, bottomCqw - h, w, h);
        };

        // Bottom left: the stick. Its base's centre was clamped, so a thumb
        // that landed nearer an edge than the base can reach gets it as close
        // as it goes.
        const float base = size(SizeClass::StickBase), half = base * 0.5f;
        const float cx = keep(centre.u / u, canvas.left + half, 100.0f - canvas.right - half);
        const float cy = keep(centre.v / v, canvas.top + half, heightCqw - canvas.bottom - half);
        d.steer = rect(cx - half, cy - half, base, base);
        d.steerStick = square(cx, cy, size(SizeClass::Stick));
        d.steerTravel = (base - size(SizeClass::Stick)) * 0.5f * u;
        // Below the D-pad: a thumb that lands above it is on a control of its
        // own, and one that lands on the D-pad's own edge is the D-pad's.
        const float systemGap = style.systemBelow * factor;
        const float grabTop = canvas.top + size(SizeClass::System) + systemGap + 3.0f * size(SizeClass::Dpad);
        d.steerGrab = rect(0.0f, grabTop, style.steerGrabWidth * factor, heightCqw - grabTop);

        // Bottom right: the action cluster, on the canvas's right and bottom
        // margins. The pedals stand on the bottom edge, the brake at the margin
        // and the throttle its width and the gap further in; the four face
        // buttons ride the fan above and to their left.
        const float clusterRight = 100.0f - canvas.right;
        const float clusterBottom = heightCqw - canvas.bottom;
        d.brake = pedal(clusterRight, clusterBottom, art::Id::Brake);
        d.throttle = pedal(d.brake.x / u - style.pedalGap * factor, clusterBottom, art::Id::Gas);
        // A face button at `degrees` above the bottom edge, on the fan.
        const float face = size(SizeClass::Face);
        const float fanRadius = style.fanRadius * factor;
        const float fanX = clusterRight - face * 0.5f;  // (its head on the margin)
        auto faceAt = [&rect, fanX, clusterBottom, fanRadius, face](float degrees) {
            return rect(fanX - fanRadius * std::cos(degrees * layout::kDegrees) - face * 0.5f,
                clusterBottom - fanRadius * std::sin(degrees * layout::kDegrees) - face * 0.5f, face, face);
        };
        // A first, at the angle where its disc rests on the bottom margin; then
        // X, B and Y, the last of them at the top of the quarter.
        const float first = std::asin(face * 0.5f / fanRadius) / layout::kDegrees;
        const float step = (90.0f - first) / 3.0f;
        d.handbrake = faceAt(first);
        d.speedbreaker = faceAt(first + step);
        d.nitro = faceAt(first + 2.0f * step);
        d.rearView = faceAt(first + 3.0f * step);

        // Top left: Start and Back, then the D-pad under them. The two corners
        // of the top edge are alike -- a margin, a row of 5.45 buttons -- so
        // the left thumb finds the system keys where the right one finds the
        // views. Start and Back as a pair, centred on the stick: the left
        // column -- the two keys, the cross and the stick -- is one line, and
        // the wide circle is what the column is built around.
        const float row = size(SizeClass::System);
        const float rowGap = style.topRowGap * factor;
        const float columnCentre = canvas.left + half;
        const float systemTotal = 2.0f * row + rowGap;
        const float systemInset = (base - systemTotal) * 0.5f;  // in from the margin
        d.start = rect(canvas.left + systemInset, canvas.top, row, row);
        d.back = rect(canvas.left + systemInset + row + rowGap, canvas.top, row, row);
        // The cross under them, inside the stick's width.
        const float cell = size(SizeClass::Dpad);
        const float dpadTop = canvas.top + row + systemGap;
        const float dpadX = columnCentre - 3.0f * cell * 0.5f;
        d.dpadUp = rect(dpadX + cell, dpadTop, cell, cell);
        d.dpadLeft = rect(dpadX, dpadTop + cell, cell, cell);
        d.dpadRight = rect(dpadX + 2.0f * cell, dpadTop + cell, cell, cell);
        d.dpadDown = rect(dpadX + cell, dpadTop + 2.0f * cell, cell, cell);

        // Top right: quick rear view then camera, 0.55 apart, mirrored on the
        // left pair -- the same distance in from the right edge as Start is
        // from the left, so the top band is symmetric; the shifter centred on
        // the two of them.
        const float topRowTotal = 2.0f * row + rowGap;
        const float topRowX = 100.0f - canvas.right - systemInset - topRowTotal;
        d.quickRearView = rect(topRowX, canvas.top, row, row);
        d.camera = rect(topRowX + row + rowGap, canvas.top, row, row);
        // Centred on the two views above it, half under one and half under the
        // other.
        const float shift = size(SizeClass::Shift);
        const float shifterX = 100.0f - canvas.right - systemInset - topRowTotal * 0.5f - shift * 0.5f;
        const float shifterY = canvas.top + style.shifterBelowViews * factor;
        d.shiftUp = rect(shifterX, shifterY, shift, shift);
        d.shiftDown = rect(shifterX, shifterY + shift + style.shiftGap * factor, shift, shift);

        // How far each control's touch area reaches past its rectangle: as much
        // as it takes to be `minTouch` across, and never more than half the
        // smallest gap to a neighbour -- two inflated areas that met would make
        // HitTest's first match decide between them.
        for (size_t i = 0; i < kControls; i++)
        {
            const Rect a = d.Of(Control(i));
            if (a.w <= 0.0f || a.h <= 0.0f)
                continue;
            float gap = 100.0f;  // the canvas is 100 cqw across
            for (size_t j = 0; j < kControls; j++)
            {
                if (j == i)
                    continue;
                const Rect b = d.Of(Control(j));
                const float across = std::max({ 0.0f, b.x / u - a.Right() / u, a.x / u - b.Right() / u });
                const float down = std::max({ 0.0f, b.y / v - a.Bottom() / v, a.y / v - b.Bottom() / v });
                gap = std::min(gap, std::hypot(across, down));
            }
            const art::Info& info = art::kArt[size_t(SpecOf(Control(i)).art)];
            const float widthCqw = a.w / u;
            const float want = std::max(0.0f, style.minTouch * factor * 0.5f - info.inradius * widthCqw);
            // (And no further than the packed distance field goes: it saturates
            // at half a picture's width.)
            d.padCqw[i] = std::clamp(std::min(want, gap * 0.5f - style.padEpsilon), 0.0f, 0.45f * widthCqw);
        }

        return d;
    }

    // The control at this point of the canvas, if any. A control's area is its
    // picture's own outline -- the geodesic distance field packed with the art,
    // so a round button's corners are not a button -- grown by the pad Layout
    // gave it. The stick's is the whole area a thumb takes it from, not just
    // where it happens to be drawn, and comes last: the other controls keep
    // their own shapes. Presses are decided here and nowhere else -- a finger
    // that slides onto a control never presses it, as the design's pointer
    // capture has it, so a finger owns whatever it landed on until it lifts.
    inline Control HitTest(const Deck& deck, float u, float v)
    {
        for (size_t i = 0; i < kControls; i++)
        {
            const Control c = Control(i);
            if (c == Control::Steer)
                continue;
            const Rect r = deck.Of(c);
            if (r.w <= 0.0f || r.h <= 0.0f)
                continue;
            const float pad = deck.padCqw[i];  // cqw
            const float padU = pad * deck.uPerCqw, padV = pad * deck.vPerCqw;
            if (u < r.x - padU || u > r.Right() + padU || v < r.y - padV || v > r.Bottom() + padV)
                continue;
            const art::Info& info = art::kArt[size_t(SpecOf(c).art)];
            // No field (the metadata was not packed): the rectangle it is, so
            // the deck stays usable.
            if (info.sdfW < 2 || info.sdfH < 2)
            {
                if (r.Contains(u, v))
                    return c;
                continue;
            }
            // The pad in the picture's own units (fractions of its width).
            if (art::Inside(info, (u - r.x) / r.w, (v - r.y) / r.h, pad / (r.w / deck.uPerCqw)))
                return c;
        }
        return deck.steerGrab.Contains(u, v) ? Control::Steer : Control::Count;
    }

    // Where a key's capsule goes: on the picture's own outline, up and to its
    // right, so half of it is on the control and half on the road. Found by
    // walking the diagonal out from the middle until the distance field turns
    // positive, so one rule covers a disc's 45-degree edge and the chamfer a
    // pedal's picture has.
    inline Point BadgeAnchor(const Rect& r, const art::Info& info)
    {
        const Point middle{ r.CentreX(), r.CentreY() };
        if (info.sdfW < 2 || info.sdfH < 2 || info.aspect <= 0.0f)
            return middle;
        // A step of `t` across is `t * aspect` down, which is the field's own
        // unit (the picture's width).
        const auto distance = [&info](float t) { return art::SdfAt(info, 0.5f + t, 0.5f - t * info.aspect); };
        float lo = 0.0f, hi = 0.0f;
        constexpr float kStep = 0.02f;
        while (hi < 0.7f && distance(hi) <= 0.0f)
        {
            lo = hi;
            hi += kStep;
        }
        if (distance(hi) <= 0.0f)
            return middle;  // (no edge: the picture covers its whole rectangle)
        for (int i = 0; i < 12; i++)
        {
            const float mid = (lo + hi) * 0.5f;
            if (distance(mid) <= 0.0f)
                lo = mid;
            else
                hi = mid;
        }
        return { r.x + (0.5f + hi) * r.w, r.y + (0.5f - hi * info.aspect) * r.h };
    }

    // Steering, -1 (left lock) to +1, from where the thumb is (`u`) against
    // where it landed (`originU`): straight ahead at the landing, and full
    // lock once the stick has travelled as far as the base lets it. Relative,
    // not absolute, so a base that cannot reach the thumb still starts the
    // drag straight ahead. The game applies its own steering curve on top.
    inline float SteerFromDrag(const Deck& deck, float u, float originU)
    {
        if (deck.steerTravel <= 0.0f)
            return 0.0f;
        return std::clamp((u - originU) / deck.steerTravel, -1.0f, 1.0f);
    }

    // The stick at `steer`, its centre as far along the base as it travels.
    inline Rect StickRect(const Deck& deck, float steer)
    {
        Rect k = deck.steerStick;
        k.x = deck.steer.CentreX() + std::clamp(steer, -1.0f, 1.0f) * deck.steerTravel - k.w * 0.5f;
        return k;
    }
}
