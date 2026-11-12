// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Where the touch controls sit on the game's picture (the design handoff's
// NFSMW Touch Control Deck), in the design's own units: a cqw is 1% of the
// picture's width, so the deck keeps its place on the picture whatever the
// screen's shape, and its size follows the picture's.
//
// Header only and SDL-free, for tests/touch_deck_test.cpp: hid/touch_input.h
// decides what a finger does there, ui/touch_overlay.cpp draws it, and
// hid/touch.cpp hands both the canvas.
//
// The numbers are design/layout.json's (the machine-readable handoff its
// AGENTS.md names the geometry source), with index.html's final "minimal
// direction" layer for what the JSON leaves to CSS. Two follow from the
// picture rather than the design's fixed 16:9 canvas:
//   - Down, a cqw is a different fraction of the canvas than across it: a
//     picture wider than 16:9 (an iPhone filled by Hor+, game/ultrawide) is
//     shorter in cqw, and one narrower is taller. Canvas::vPerCqw carries it,
//     so every vertical extent below stays the design's cqw.
//   - The 2.65 cqw margin gives way to the screen's safe area where that
//     reaches further in (an iPhone's notch and rounded corners in
//     landscape), so a control never sits under one: InsetsFor().
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace hid::touch
{
    // A rectangle in canvas fractions: (0, 0) is the picture's top left, (1, 1)
    // its bottom right.
    struct Rect
    {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

        float CentreX() const { return x + w * 0.5f; }
        float CentreY() const { return y + h * 0.5f; }
        float Right() const { return x + w; }
        float Bottom() const { return y + h; }
        bool Contains(float px, float py) const { return px >= x && px < Right() && py >= y && py < Bottom(); }
    };

    // The 16 controls the deck keeps mapped (design/keymap.json): every Xbox
    // 360 control the game has, none merged or renamed. Steer is the whole
    // steering panel (its slider's touch area), ShiftUp and ShiftDown the two
    // halves of the shifter, which is one control (RS) with two directions.
    enum class Control : uint8_t
    {
        Steer,
        Throttle, Brake, Handbrake, Nitro, Speedbreaker,
        RearView, QuickRearView, Camera,
        ShiftUp, ShiftDown,
        DpadUp, DpadDown, DpadLeft, DpadRight,
        Back, Start,
        Count
    };
    constexpr size_t kControls = size_t(Control::Count);

    // The design's numbers, in cqw (design/layout.json).
    namespace layout
    {
        // The steering panel and its track (26 x 11.1, narrower than the
        // handoff's 35; the track 4.9 high, 1.35 in from the panel's sides and
        // 3.55 down from its top; the knob 5.9 across, on the track's centre
        // line, travelling 44% of the track's width each way).
        constexpr float kSteerWidth = 26.0f, kSteerHeight = 11.1f;
        constexpr float kSteerPadX = 1.35f, kSteerTrackTop = 3.55f;
        constexpr float kTrackHeight = 4.9f, kKnobDiameter = 5.9f, kSteerTravel = 0.44f;
        constexpr float kSteerFillHeight = 0.36f;

        // Where a thumb can take the panel: the picture's left, below the
        // D-pad. While it is held, the panel's centre is where the touch
        // landed (clamped to the canvas), so it comes to the thumb rather than
        // the thumb to it; it returns to the corner when the thumb lifts.
        constexpr float kSteerGrabWidth = 42.0f;  // from the canvas's left edge
        constexpr float kSteerGrabTop = 16.0f;     // the D-pad's bottom edge

        // The action cluster and its five buttons (the cluster has no look of
        // its own).
        constexpr float kActionWidth = 27.0f, kActionHeight = 18.4f;
        constexpr float kThrottleWidth = 15.55f, kBrakeWidth = 10.4f, kPedalHeight = 7.2f;
        constexpr float kOrbTop = 8.75f;  // the orbs' bottom edge, above the pedals
        constexpr float kNitroDiameter = 9.35f, kOrbDiameter = 7.45f;
        constexpr float kSpeedbreakerLeft = 9.35f;

        // The D-pad: a 3 x 3 grid of 4.45 cqw cells, the arrows on its edges.
        constexpr float kDpadCell = 4.45f;

        // Start and Back, centred on the top edge: a button 4.05 high, 0.65
        // apart, as wide as an icon and a key tag with their padding
        // (0.9 + 1.6 + 0.65 + 4.25 + 0.9). The design's own is about this wide
        // with a label between the two; the deck has none, so "START" and
        // "BACK" get the room.
        constexpr float kSystemTop = 2.7f, kSystemHeight = 4.05f, kSystemGap = 0.65f;
        constexpr float kSystemWidth = 8.3f;

        // Rear view, quick rear view and camera: 5.45 across, 0.55 apart.
        constexpr float kViewDiameter = 5.45f, kViewGap = 0.55f;
        constexpr float kViewCount = 3.0f;

        // The shifter: a header strip and two halves, under the view buttons.
        constexpr float kShifterWidth = 5.45f;
        constexpr float kShifterHeader = 1.8f, kShiftHeight = 3.45f;
        constexpr float kShifterBelowViews = 6.9f;  // 9.55 - 2.65, from the view buttons' top
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

        // The picture's shape, width / height.
        float Aspect() const { return uPerCqw > 0.0f ? vPerCqw / uPerCqw : 0.0f; }
        // The canvas's height in cqw: 56.25 at 16:9, more when the picture is
        // narrower than that.
        float HeightCqw() const { return vPerCqw > 0.0f ? 1.0f / vPerCqw : 0.0f; }
    };

    // The canvas of a picture of this shape, with the design's margins.
    inline Canvas CanvasFor(float aspect)
    {
        Canvas c;
        c.vPerCqw = aspect * 0.01f;
        return c;
    }

    // The design's 2.65 cqw margin, or the safe area where it reaches further
    // in. `safe*` and `pictureWidth` in pixels: a pixel is 100 / pictureWidth
    // of a cqw.
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

    // A point of the canvas.
    struct Point
    {
        float u = 0.0f, v = 0.0f;
    };

    // Every control's rectangle, and the parts of the steering panel that only
    // drawing needs. `uPerCqw` and `vPerCqw` are the canvas it was made for,
    // for the helpers below.
    struct Deck
    {
        float uPerCqw = 0.01f, vPerCqw = 0.0177778f;
        // The steering panel and the area a thumb takes it from. While a thumb
        // holds it, the panel's centre is where that thumb landed
        // (hid/touch_input.h's State::steerOrigin), so it is drawn under the
        // thumb rather than in the handoff's corner, and goes back there when
        // the thumb lifts; steerTrack is what a drag is read against, and
        // steerKnob is the knob at centre.
        Rect steer, steerTrack, steerKnob, steerGrab;
        Rect throttle, brake, handbrake, nitro, speedbreaker;
        Rect dpadUp, dpadDown, dpadLeft, dpadRight;
        Rect rearView, quickRearView, camera;
        Rect shifter, shiftUp, shiftDown;
        Rect start, back;

        Rect Of(Control c) const;
    };

    inline Rect Deck::Of(Control c) const
    {
        switch (c)
        {
        case Control::Steer: return steer;
        case Control::Throttle: return throttle;
        case Control::Brake: return brake;
        case Control::Handbrake: return handbrake;
        case Control::Nitro: return nitro;
        case Control::Speedbreaker: return speedbreaker;
        case Control::RearView: return rearView;
        case Control::QuickRearView: return quickRearView;
        case Control::Camera: return camera;
        case Control::ShiftUp: return shiftUp;
        case Control::ShiftDown: return shiftDown;
        case Control::DpadUp: return dpadUp;
        case Control::DpadDown: return dpadDown;
        case Control::DpadLeft: return dpadLeft;
        case Control::DpadRight: return dpadRight;
        case Control::Back: return back;
        case Control::Start: return start;
        case Control::Count: break;
        }
        return {};
    }

    // Where the panel's centre sits when nothing has taken it: the handoff's
    // corner, 2.65 cqw in from the left and the bottom.
    inline Point SteerHomeCentre(const Canvas& canvas)
    {
        const float heightCqw = canvas.HeightCqw();
        return { (canvas.left + layout::kSteerWidth * 0.5f) * canvas.uPerCqw,
            (heightCqw - canvas.bottom - layout::kSteerHeight * 0.5f) * canvas.vPerCqw };
    }

    // The deck with the steering panel centred on `centre`, kept inside the
    // canvas's margins. Every size is the design's; only the panel moves.
    inline Deck Layout(const Canvas& canvas, Point centre)
    {
        using namespace layout;
        Deck d;
        d.uPerCqw = canvas.uPerCqw;
        d.vPerCqw = canvas.vPerCqw;
        const float u = canvas.uPerCqw, v = canvas.vPerCqw;
        const float heightCqw = canvas.HeightCqw();
        auto rect = [u, v](float xCqw, float yCqw, float wCqw, float hCqw) {
            return Rect{ xCqw * u, yCqw * v, wCqw * u, hCqw * v };
        };
        // (A canvas too small for the panel's margins keeps it in the middle
        // rather than in a range that has turned inside out.)
        auto keep = [](float value, float lo, float hi) { return hi < lo ? (lo + hi) * 0.5f : std::clamp(value, lo, hi); };

        // Bottom left: the steering panel, its track, and the knob at centre.
        // The panel's centre was clamped, so a thumb that landed nearer an
        // edge than the panel can reach gets it as close as it goes.
        const float halfW = kSteerWidth * 0.5f, halfH = kSteerHeight * 0.5f;
        const float cx = keep(centre.u / u, canvas.left + halfW, 100.0f - canvas.right - halfW);
        const float cy = keep(centre.v / v, canvas.top + halfH, heightCqw - canvas.bottom - halfH);
        const float steerX = cx - halfW, steerY = cy - halfH;
        d.steer = rect(steerX, steerY, kSteerWidth, kSteerHeight);
        const float trackX = steerX + kSteerPadX, trackY = steerY + kSteerTrackTop;
        d.steerTrack = rect(trackX, trackY, kSteerWidth - 2.0f * kSteerPadX, kTrackHeight);
        d.steerKnob = rect(trackX + (kSteerWidth - 2.0f * kSteerPadX - kKnobDiameter) * 0.5f,
            trackY + (kTrackHeight - kKnobDiameter) * 0.5f, kKnobDiameter, kKnobDiameter);
        d.steerGrab = rect(0.0f, kSteerGrabTop, kSteerGrabWidth, heightCqw - kSteerGrabTop);

        // Bottom right: the action cluster. Its right and bottom edges are the
        // canvas's: the pedals on the bottom, the orbs above them.
        const float clusterX = 100.0f - canvas.right - kActionWidth;
        const float clusterY = heightCqw - canvas.bottom - kActionHeight;
        const float pedalY = clusterY + kActionHeight - kPedalHeight;
        d.throttle = rect(clusterX + kActionWidth - kThrottleWidth, pedalY, kThrottleWidth, kPedalHeight);
        d.brake = rect(clusterX, pedalY, kBrakeWidth, kPedalHeight);
        const float orbY = clusterY + kActionHeight - kOrbTop - kOrbDiameter;
        d.nitro = rect(clusterX + kActionWidth - kNitroDiameter, clusterY + kActionHeight - kOrbTop - kNitroDiameter,
            kNitroDiameter, kNitroDiameter);
        d.handbrake = rect(clusterX, orbY, kOrbDiameter, kOrbDiameter);
        d.speedbreaker = rect(clusterX + kSpeedbreakerLeft, orbY, kOrbDiameter, kOrbDiameter);

        // Top left: the D-pad's arrows on the edges of its 3 x 3 grid.
        d.dpadUp = rect(canvas.left + kDpadCell, canvas.top, kDpadCell, kDpadCell);
        d.dpadLeft = rect(canvas.left, canvas.top + kDpadCell, kDpadCell, kDpadCell);
        d.dpadRight = rect(canvas.left + 2.0f * kDpadCell, canvas.top + kDpadCell, kDpadCell, kDpadCell);
        d.dpadDown = rect(canvas.left + kDpadCell, canvas.top + 2.0f * kDpadCell, kDpadCell, kDpadCell);

        // Top middle: Start, then Back (design/layout.json's order).
        const float systemTotal = 2.0f * kSystemWidth + kSystemGap;
        const float startX = 50.0f - systemTotal * 0.5f;
        d.start = rect(startX, kSystemTop, kSystemWidth, kSystemHeight);
        d.back = rect(startX + kSystemWidth + kSystemGap, kSystemTop, kSystemWidth, kSystemHeight);

        // Top right: rear view, quick rear view, camera; the shifter under them.
        const float viewTotal = kViewCount * kViewDiameter + (kViewCount - 1.0f) * kViewGap;
        const float viewX = 100.0f - canvas.right - viewTotal;
        d.rearView = rect(viewX, canvas.top, kViewDiameter, kViewDiameter);
        d.quickRearView = rect(viewX + kViewDiameter + kViewGap, canvas.top, kViewDiameter, kViewDiameter);
        d.camera = rect(viewX + 2.0f * (kViewDiameter + kViewGap), canvas.top, kViewDiameter, kViewDiameter);
        const float shifterX = 100.0f - canvas.right - kShifterWidth;
        const float shifterY = canvas.top + kShifterBelowViews;
        d.shifter = rect(shifterX, shifterY, kShifterWidth, kShifterHeader + 2.0f * kShiftHeight);
        d.shiftUp = rect(shifterX, shifterY + kShifterHeader, kShifterWidth, kShiftHeight);
        d.shiftDown = rect(shifterX, shifterY + kShifterHeader + kShiftHeight, kShifterWidth, kShiftHeight);

        return d;
    }

    // The deck on this canvas, with the steering panel in the handoff's corner
    // (before a thumb has taken it; see hid/touch_input.h's State).
    inline Deck Layout(const Canvas& canvas)
    {
        return Layout(canvas, SteerHomeCentre(canvas));
    }

    // The control at this point of the canvas, if any. The steering panel's is
    // the whole area a thumb takes it from, not just where it happens to be
    // drawn, and comes last: the other controls keep their own rectangles.
    // Presses are decided here and nowhere else: a finger that slides onto a
    // control never presses it, as the design's pointer capture has it, so a
    // finger owns whatever it landed on until it lifts.
    inline Control HitTest(const Deck& deck, float u, float v)
    {
        for (size_t i = 0; i < kControls; i++)
        {
            const Control c = Control(i);
            if (c != Control::Steer && deck.Of(c).Contains(u, v))
                return c;
        }
        return deck.steerGrab.Contains(u, v) ? Control::Steer : Control::Count;
    }

    // Steering, -1 (left lock) to +1, from where the thumb is (`u`) against
    // where it landed (`originU`): straight ahead at the landing, and read
    // against the knob's own travel each way, so the knob is under the thumb
    // while the panel could centre on it. Relative, not absolute: a panel that
    // cannot reach the thumb (it is 26 cqw wide, so its centre stops 13 cqw
    // short of an edge) still starts the drag straight ahead instead of
    // snapping the wheel over. The game applies its own steering curve on top.
    inline float SteerFromDrag(const Deck& deck, float u, float originU)
    {
        const float span = layout::kSteerTravel * deck.steerTrack.w;
        if (span <= 0.0f)
            return 0.0f;
        return std::clamp((u - originU) / span, -1.0f, 1.0f);
    }

    // The knob at `steer` (the deck has it at rest).
    inline Rect KnobRect(const Deck& deck, float steer)
    {
        const Rect& t = deck.steerTrack;
        Rect k = deck.steerKnob;
        k.x = t.CentreX() + std::clamp(steer, -1.0f, 1.0f) * layout::kSteerTravel * t.w - k.w * 0.5f;
        return k;
    }

    // The bar from the track's centre line to the knob, either way.
    inline Rect SteerFill(const Deck& deck, float steer)
    {
        const Rect& t = deck.steerTrack;
        const float s = std::clamp(steer, -1.0f, 1.0f);
        const float width = std::fabs(s) * layout::kSteerTravel * t.w;
        const float h = layout::kSteerFillHeight * deck.vPerCqw;
        return Rect{ s >= 0.0f ? t.CentreX() : t.CentreX() - width, t.CentreY() - h * 0.5f, width, h };
    }
}
