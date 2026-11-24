// Tests for the touch deck's headers -- hid/touch_control.h, hid/touch_spec.h,
// hid/touch_layout.h, hid/touch_input.h (header only) and the generated
// hid/touch_art.h: where the controls sit on the game's picture, which fingers
// press what, and what the game is told. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/touch_deck_test.cpp -o build/touch_deck_test \
//       && build/touch_deck_test runtime/ui/touch_overlay.cpp
#include <hid/touch_layout.h>
#include <hid/touch_input.h>

#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

using namespace hid::touch;

// The reference canvas in pixels (900 x 506.25, 1 cqw = 9 px). The tests say
// where a control goes in pixels there.
static constexpr float kW = 900.0f, kH = 506.25f;

static void Near(float got, float want, const char* what)
{
    CHECK(std::fabs(got - want) < 0.01f, "%s: %.4f, want %.4f", what, got, want);
}

static void RectNear(const Rect& r, float x, float y, float w, float h, const char* what)
{
    char label[96];
    snprintf(label, sizeof label, "%s x", what);
    Near(r.x * kW, x, label);
    snprintf(label, sizeof label, "%s y", what);
    Near(r.y * kH, y, label);
    snprintf(label, sizeof label, "%s w", what);
    Near(r.w * kW, w, label);
    snprintf(label, sizeof label, "%s h", what);
    Near(r.h * kH, h, label);
}

static void Geometry()
{
    const Deck d = Layout(CanvasFor(kW / kH));

    // Bottom left: the analog stick, an 18 cqw base 2.65 in from the left and
    // the bottom, with a 7.4 cqw stick at its centre.
    RectNear(d.steer, 23.85f, 320.4f, 162.0f, 162.0f, "stick base");
    RectNear(d.steerStick, 71.55f, 368.1f, 66.6f, 66.6f, "stick");
    Near(d.steerStick.CentreX(), d.steer.CentreX(), "the stick centred in its base");
    Near(d.steerStick.CentreY(), d.steer.CentreY(), "and level with it");
    Near(d.steerTravel * kW, 47.7f, "the stick travels the base's half less the stick's");

    // Bottom right: the action cluster, in the iOS deck's arrangement. Two
    // standing pedals on the bottom margin (the brake at it, the throttle a
    // pedal and 0.8 cqw in from it), and the four face buttons on a fan to
    // their left: A, X, B and Y, a step apart all the way up, each the size of
    // the views' LB and RB. A pedal is 6.5 cqw wide and as tall as its own
    // picture is for that width -- gas.png is 0.81, brake.png 0.98 -- so the
    // two are neither the same height nor stretched into one another.
    RectNear(d.brake, 817.65f, 422.479f, 58.5f, 59.921f, "brake");
    RectNear(d.throttle, 751.95f, 410.052f, 58.5f, 72.348f, "gas");
    Near(d.brake.Right() * kW, 876.15f, "the brake's right edge on the margin");
    Near(d.brake.Bottom() * kH, 482.4f, "and the pedals' bottom on it");
    Near((d.brake.x - d.throttle.Right()) * kW, 0.8f * 9.0f, "0.8 cqw between the pedals");
    RectNear(d.handbrake, 666.97f, 433.35f, 49.05f, 49.05f, "A");
    RectNear(d.speedbreaker, 695.72f, 363.10f, 49.05f, 49.05f, "X");
    RectNear(d.nitro, 753.31f, 313.66f, 49.05f, 49.05f, "B");
    RectNear(d.rearView, 827.10f, 295.88f, 49.05f, 49.05f, "Y");
    Near(d.handbrake.w / d.camera.w, 1.0f, "a face button the size of RB");
    Near(d.rearView.Right() * kW, d.brake.Right() * kW, "Y's and the brake's edges level");

    // The fan: a quarter circle about a point on the bottom margin, the same
    // step along it between one button and the next, A's disc resting on that
    // margin and Y at the top of the quarter, over the brake.
    const Rect fan[] = { d.handbrake, d.speedbreaker, d.nitro, d.rearView };
    const float pivotX = d.rearView.CentreX(), pivotY = d.brake.Bottom();
    Near((pivotY - d.handbrake.Bottom()) * kH, 0.0f, "A rests on the pedals' bottom");
    Near((pivotY - d.rearView.CentreY()) * kH, 18.0f * 9.0f, "Y at the top of the quarter");
    CHECK(pivotX > d.brake.x && pivotX < d.brake.Right(), "Y over the brake (%.3f, brake %.3f..%.3f)",
        pivotX, d.brake.x, d.brake.Right());
    float radius = 0.0f, step = 0.0f;
    for (size_t i = 0; i < std::size(fan); i++)
    {
        const float r = std::hypot((pivotX - fan[i].CentreX()) * kW, (pivotY - fan[i].CentreY()) * kH);
        if (i == 0)
            radius = r;
        else
            Near(r, radius, "every face button a radius from the corner");
        if (i > 0)
        {
            const float chord = std::hypot((fan[i].CentreX() - fan[i - 1].CentreX()) * kW,
                (fan[i].CentreY() - fan[i - 1].CentreY()) * kH);
            if (i == 1)
                step = chord;
            else
                Near(chord, step, "and the same distance from the one before");
        }
    }
    Near(radius, 18.0f * 9.0f, "the fan's radius");
    Near(step, 2.0f * radius * std::sin(13.5488f * 3.14159265f / 180.0f), "equal steps along it");
    CHECK((d.throttle.x - d.speedbreaker.Right()) * kW < 9.0f, "X's edge within a cqw of the throttle's");

    // Top left: Start and Back on the margin, then the D-pad's 3 x 3 grid of
    // 4.45 cqw cells under them, arrows on its edges.
    RectNear(d.start, 53.325f, 23.85f, 49.05f, 49.05f, "start");
    RectNear(d.back, 107.325f, 23.85f, 49.05f, 49.05f, "back");
    Near((d.start.CentreX() + d.back.CentreX()) * 0.5f, d.steer.CentreX(), "the pair centred on the stick");
    Near((d.back.x - d.start.Right()) * kW, 0.55f * 9.0f, "0.55 cqw between start and back");
    RectNear(d.dpadUp, 84.825f, 77.85f, 40.05f, 40.05f, "d-pad up");
    RectNear(d.dpadLeft, 44.775f, 117.9f, 40.05f, 40.05f, "d-pad left");
    RectNear(d.dpadRight, 124.875f, 117.9f, 40.05f, 40.05f, "d-pad right");
    RectNear(d.dpadDown, 84.825f, 157.95f, 40.05f, 40.05f, "d-pad down");
    Near(d.dpadUp.Right(), d.dpadRight.x, "the up cell meets the right one's column");
    Near((d.dpadUp.y - d.back.Bottom()) * kH, 0.55f * 9.0f, "the D-pad 0.55 cqw under the two");
    // The whole column -- the two keys as a pair, the cross, the stick -- on
    // one line.
    Near((d.dpadLeft.CentreX() + d.dpadRight.CentreX()) * 0.5f, d.steer.CentreX(), "the D-pad centred on the stick");


    // Top right: quick rear view and camera, 5.45 across and 0.55 apart, the
    // last on the margin. (Rear view is the cluster's Y now.)
    RectNear(d.quickRearView, 743.625f, 23.85f, 49.05f, 49.05f, "quick rear view");
    RectNear(d.camera, 797.625f, 23.85f, 49.05f, 49.05f, "camera");
    Near((1.0f - d.camera.Right()) * kW, d.start.x * kW, "mirrored: the same distance in from its edge");
    RectNear(d.shiftUp, 770.625f, 85.95f, 49.05f, 49.05f, "gear up");
    RectNear(d.shiftDown, 770.625f, 138.6f, 49.05f, 49.05f, "gear down");
    Near((d.shiftUp.CentreX() - d.quickRearView.CentreX()) * kW, (d.camera.CentreX() - d.shiftUp.CentreX()) * kW,
        "the shifter centred between the two views");
    Near((d.shiftDown.y - d.shiftUp.Bottom()) * kH, 0.4f * 9.0f, "the shifter's gap");

    // Every control has a size, and none overlaps another (a press is decided
    // by HitTest's first match, so an overlap would hide one).
    for (size_t i = 0; i < kControls; i++)
    {
        const Rect a = d.Of(Control(i));
        CHECK(a.w > 0.0f && a.h > 0.0f, "control %zu has a size", i);
        for (size_t j = i + 1; j < kControls; j++)
        {
            const Rect b = d.Of(Control(j));
            const bool apart = a.Right() <= b.x || b.Right() <= a.x || a.Bottom() <= b.y || b.Bottom() <= a.y;
            CHECK(apart, "controls %zu and %zu don't overlap", i, j);
        }
    }
}

static void OtherShapes()
{
    // The sizes hold on any picture shape: wider than 16:9 (an iPhone filled by
    // Hor+) the canvas is shorter in cqw, narrower (a 4:3 iPad) taller, and a
    // control keeps its size in cqw either way.
    for (float aspect : { 16.0f / 9.0f, 19.5f / 9.0f, 4.0f / 3.0f, 16.0f / 10.0f })
    {
        const Canvas c = CanvasFor(aspect);
        const Deck d = Layout(c);
        const float w = 900.0f, h = w / aspect;  // pixels: a round control must be round here
        Near(d.steer.w, 0.18f, "the stick's base 18 cqw across");
        Near(d.steer.h, 18.0f * c.vPerCqw, "and 18 cqw down");
        Near(d.steerStick.w, 0.074f, "the stick 7.4 cqw across");
        Near(1.0f - d.steer.Bottom(), 2.65f * c.vPerCqw, "the base 2.65 cqw above the bottom");
        Near(1.0f - d.camera.Right(), d.start.x, "the views as far in from the right as Start is from the left");
        Near(d.steer.w * w / (d.steer.h * h), 1.0f, "the base is round on the screen");
        Near(d.nitro.w * w / (d.nitro.h * h), 1.0f, "so is the nitrous button");
        Near(d.dpadUp.w / d.camera.w, 4.45f / 5.45f, "a d-pad cell against a view button");
        for (size_t i = 0; i < kControls; i++)
        {
            const Rect r = d.Of(Control(i));
            CHECK(r.x >= 0.0f && r.y >= 0.0f && r.Right() <= 1.0f && r.Bottom() <= 1.0f, "control %zu is on the canvas", i);
        }
    }
}

static void SafeArea()
{
    // The design's 2.65 cqw margin, or the safe area where it reaches further
    // in: an iPhone in landscape keeps ~59 points clear at each side and 21 at
    // the bottom, which of a 600-point picture is 9.8 and 3.5 cqw.
    const Canvas c = CanvasFor(19.5f / 9.0f);
    Canvas same = InsetsFor(c, 600.0f, 9.0f, 0.0f, 9.0f, 12.0f);
    Near(same.left, 2.65f, "a small safe area leaves the design's margin");
    Near(same.bottom, 2.65f, "and the bottom's");

    Canvas notched = InsetsFor(c, 600.0f, 59.0f, 0.0f, 59.0f, 21.0f);
    Near(notched.left, 59.0f * 100.0f / 600.0f, "a notch pushes the left margin in");
    Near(notched.right, notched.left, "and the right's, symmetrically");
    Near(notched.top, 2.65f, "the top keeps the design's");
    CHECK(notched.bottom > 2.65f, "the home indicator pushes the bottom in");

    // The deck follows: the base starts at the new margin and stays clear of
    // the bottom's.
    const Deck d = Layout(notched);
    Near(d.steer.x, notched.left * notched.uPerCqw, "the base at the new left margin");
    Near(1.0f - d.steer.Bottom(), notched.bottom * notched.vPerCqw, "and clear of the bottom's");
}

static void Pedals()
{
    // Neither pedal is stretched into the other's box: each is 6.5 cqw across
    // and as tall as its own picture is for that width, standing on the bottom
    // margin -- gas.png is 0.81 wide for its height, brake.png 0.98.
    const Deck d = Layout(CanvasFor(kW / kH));
    const float gas = art::kArt[size_t(SpecOf(Control::Throttle).art)].aspect;
    const float brake = art::kArt[size_t(SpecOf(Control::Brake).art)].aspect;
    Near(d.throttle.h * kH / (d.throttle.w * kW), 1.0f / gas, "the gas pedal keeps its picture's shape");
    Near(d.brake.h * kH / (d.brake.w * kW), 1.0f / brake, "so does the brake");
    Near(d.throttle.Bottom(), d.brake.Bottom(), "both stand on the same margin");
    CHECK(std::fabs(d.throttle.h - d.brake.h) > 1e-4f, "and are no longer the same height");
    for (float aspect : { 16.0f / 9.0f, 4.0f / 3.0f, 19.5f / 9.0f })
    {
        const Deck a = Layout(CanvasFor(aspect));
        Near(a.throttle.w, 0.065f, "the gas is 6.5 cqw across, whatever the picture's shape");
        Near(a.brake.w, 0.065f, "and so is the brake");
        Near(a.throttle.h * a.uPerCqw / (a.throttle.w * a.vPerCqw), 1.0f / gas, "and keeps its shape");
        Near(a.brake.h * a.uPerCqw / (a.brake.w * a.vPerCqw), 1.0f / brake, "both of them");
    }
}

static void Shape()
{
    // A control's touch area is its picture's outline, not its rectangle: the
    // silhouette says the middle is on the button and the corners are not --
    // for the round ones, and for the two pedals whose pictures are chamfered
    // at the top alike.
    const Deck d = Layout(CanvasFor(kW / kH));
    for (size_t i = 0; i < kControls; i++)
    {
        const Control c = Control(i);
        if (c == Control::Steer)
            continue;  // (its area is the grab panel, not the base's disc)
        const art::Info& info = art::kArt[size_t(SpecOf(c).art)];
        CHECK(art::Inside(info, 0.5f, 0.5f, 0.0f), "control %zu: its middle is on the picture", i);
        CHECK(!art::Inside(info, 0.02f, 0.02f, 0.0f), "control %zu: its top left corner is not", i);
        CHECK(!art::Inside(info, 0.98f, 0.98f, 0.0f), "control %zu: nor its bottom right", i);
    }
    // The gears are rings with a cross cut out of the middle; the hole is the
    // button, because a thumb on it means the gear.
    CHECK(HitTest(d, d.shiftUp.CentreX(), d.shiftUp.CentreY()) == Control::ShiftUp, "the gear's cut-out presses the gear");
    CHECK(HitTest(d, d.shiftDown.CentreX(), d.shiftDown.CentreY()) == Control::ShiftDown, "and the other's");
    // The D-pad's cells touch their neighbours, so they are not grown at all: a
    // corner there presses nothing.
    const float px = 1.0f / kW, py = 1.0f / kH;
    CHECK(HitTest(d, d.dpadUp.x + px, d.dpadUp.y + py) == Control::Count, "a D-pad cell's corner presses nothing");
}

static void Pads()
{
    // A control's touch area grows by at most half the gap to its nearest
    // neighbour, so two of them can never meet -- and the D-pad's cells, which
    // touch, do not grow at all.
    const Deck d = Layout(CanvasFor(kW / kH));
    for (size_t i = 0; i < kControls; i++)
    {
        CHECK(d.padCqw[i] >= 0.0f, "control %zu: a pad is not negative", i);
        for (size_t j = i + 1; j < kControls; j++)
        {
            const Rect a = d.Of(Control(i)), b = d.Of(Control(j));
            const float across = std::max({ 0.0f, b.x - a.Right(), a.x - b.Right() }) * 100.0f;
            const float down = std::max({ 0.0f, b.y - a.Bottom(), a.y - b.Bottom() }) * 100.0f * kH / kW;
            const float gap = std::hypot(across, down);
            CHECK(d.padCqw[i] + d.padCqw[j] <= gap + 0.001f, "controls %zu and %zu: pads %.3f + %.3f <= gap %.3f", i, j,
                d.padCqw[i], d.padCqw[j], gap);
        }
    }
    CHECK(d.padCqw[size_t(Control::DpadUp)] == 0.0f, "a D-pad cell does not grow (its neighbours touch it)");
    CHECK(d.padCqw[size_t(Control::DpadDown)] == 0.0f, "nor that one");
    // The gap between the pedals is still nothing to press.
    CHECK(HitTest(d, (d.brake.Right() + d.throttle.x) * 0.5f, d.throttle.CentreY()) == Control::Count,
        "the middle of the gap between the pedals");
}

static void Specs()
{
    // The table is Control's order, every control has a picture, and each
    // picture's metadata is what the packer said it would be.
    CHECK(std::size(kSpecs) == kControls, "one spec a control: %zu", std::size(kSpecs));
    for (size_t i = 0; i < kControls; i++)
    {
        const ControlSpec& spec = kSpecs[i];
        CHECK(spec.control == Control(i), "spec %zu is control %zu", i, size_t(spec.control));
        CHECK(size_t(spec.art) < art::kCount, "spec %zu has a picture", i);
        CHECK((spec.key != nullptr) == (spec.keyFace != KeyFace::None), "spec %zu: a name has a colour", i);
        // (A picture's draw scale belongs to the picture, not the control.)
        CHECK(kDrawScale[size_t(spec.art)] > 0.5f && kDrawScale[size_t(spec.art)] <= 1.0f,
            "spec %zu: a sane draw scale", i);
        const art::Info& info = art::kArt[size_t(spec.art)];
        CHECK(info.u0 < info.u1 && info.v0 < info.v1, "picture %zu: its cell is not empty", i);
        CHECK(info.aspect > 0.2f && info.aspect < 5.0f, "picture %zu: an aspect", i);
        CHECK(info.sdfW >= 2 && info.sdfH >= 2, "picture %zu: a distance field", i);
        CHECK(info.sheet == 0, "picture %zu: the deck's own atlas", i);
        CHECK(info.field >= art::kSdf && size_t(info.field - art::kSdf) + size_t(info.sdfW) * size_t(info.sdfH) <=
                std::size(art::kSdf), "picture %zu: its field is inside kSdf", i);
    }
    // A key's capsule sits on the picture's own edge, up and to the right of
    // its middle -- where the field says the edge is.
    const Deck d = Layout(CanvasFor(kW / kH));
    for (const ControlSpec& spec : kSpecs)
    {
        if (!spec.key)
            continue;
        const Rect r = spec.control == Control::Steer ? d.steer : d.Of(spec.control);
        const art::Info& info = art::kArt[size_t(spec.art)];
        const Point p = BadgeAnchor(r, info);
        const float distance = art::SdfAt(info, (p.u - r.x) / r.w, (p.v - r.y) / r.h);
        CHECK(std::fabs(distance) < 0.03f, "control %zu: the badge is on the edge (%.3f)", size_t(spec.control), distance);
        CHECK(p.u > r.CentreX() && p.v < r.CentreY(), "control %zu: up and to the right of its middle", size_t(spec.control));
        CHECK(p.u <= r.Right() && p.v >= r.y, "control %zu: and still over the picture", size_t(spec.control));
    }
}

static void Styles()
{
    // The default Style is the deck as it was, and a scaled one is still a
    // deck: round things round, everything on the canvas, nothing on top of
    // anything else. (The automatic sizing that will use it does not exist yet.)
    const Canvas c = CanvasFor(kW / kH);
    const Deck a = Layout(c);
    const Deck b = Layout(c, Style{});
    for (size_t i = 0; i < kControls; i++)
    {
        const Rect x = a.Of(Control(i)), y = b.Of(Control(i));
        CHECK(x.x == y.x && x.y == y.y && x.w == y.w && x.h == y.h, "control %zu: the same with the default style", i);
    }
    for (float scale : { 1.25f, 0.8f })
    {
        Style style;
        style.scale = scale;
        const Deck d = Layout(CanvasFor(kW / kH, style), style);
        for (size_t i = 0; i < kControls; i++)
        {
            const Rect r = d.Of(Control(i));
            CHECK(r.x >= -1e-4f && r.y >= -1e-4f && r.Right() <= 1.0f + 1e-4f && r.Bottom() <= 1.0f + 1e-4f,
                "scale %.2f: control %zu is on the canvas", scale, i);
            for (size_t j = i + 1; j < kControls; j++)
            {
                const Rect o = d.Of(Control(j));
                const bool apart = r.Right() <= o.x + 1e-6f || o.Right() <= r.x + 1e-6f || r.Bottom() <= o.y + 1e-6f ||
                    o.Bottom() <= r.y + 1e-6f;
                CHECK(apart, "scale %.2f: controls %zu and %zu don't overlap", scale, i, j);
            }
        }
        Near(d.steer.w * kW / (d.steer.h * kH), 1.0f, "the stick's base is still round");
        Near(d.throttle.h * kH / (d.throttle.w * kW), 1.0f / art::kArt[size_t(art::Id::Gas)].aspect,
            "and the pedal keeps its picture's shape");
    }
}

static void Screens()
{
    // The deck is a thumb's width on every screen rather than a fixed fraction
    // of the picture: a cqw is 12.8 points on the design's 1280x720, 8.5 on an
    // iPhone's 852x393 (so its deck grows) and 13.7 on an iPad's 1366x1024 (so
    // its shrinks) -- but only as far as the picture's height allows, since the
    // shifter's column and the face buttons' fan must not meet.
    const struct
    {
        float w, h;
        const char* what;
    } screens[] = {
        { 1280.0f, 720.0f, "the design's 1280x720" },
        { 1920.0f, 1080.0f, "a 1080p window" },
        { 852.0f, 393.0f, "an iPhone's 852x393" },
        { 667.0f, 375.0f, "a small phone's 667x375" },
        { 1366.0f, 1024.0f, "an iPad's 1366x1024" },
        { 2560.0f, 1440.0f, "a 5K desktop's 2560x1440 points" },
    };
    for (const auto& screen : screens)
    {
        Canvas c = CanvasFor(screen.w / screen.h);
        c.pointsPerCqw = screen.w * 0.01f;
        const Style style;
        const float factor = style.Factor(c);
        CHECK(factor >= kMinScale && factor <= MaxScale(c) + 1e-4f, "%s: a factor in bounds (%.3f)", screen.what, factor);

        const Deck d = Layout(c, style);
        for (size_t i = 0; i < kControls; i++)
        {
            const Rect r = d.Of(Control(i));
            CHECK(r.x >= -1e-4f && r.y >= -1e-4f && r.Right() <= 1.0f + 1e-4f && r.Bottom() <= 1.0f + 1e-4f,
                "%s: control %zu is on the canvas", screen.what, i);
            for (size_t j = i + 1; j < kControls; j++)
            {
                const Rect o = d.Of(Control(j));
                const bool apart = r.Right() <= o.x + 1e-6f || o.Right() <= r.x + 1e-6f || r.Bottom() <= o.y + 1e-6f ||
                    o.Bottom() <= r.y + 1e-6f;
                CHECK(apart, "%s: controls %zu and %zu don't overlap (factor %.3f)", screen.what, i, j, factor);
            }
        }
    }

    // The design's own screen is the deck as drawn: no scale at all.
    Canvas reference = CanvasFor(1280.0f / 720.0f);
    reference.pointsPerCqw = 12.8f;
    Near(Style{}.Factor(reference), 1.0f, "the design's 1280x720 is the design's sizes");

    // A tablet's button is the 70 points the design asks for; a phone's picture
    // is too short for that (the fan would meet the shifter), so it stops at
    // what the height allows -- which is still bigger than the design's.
    Canvas ipad = CanvasFor(1366.0f / 1024.0f);
    ipad.pointsPerCqw = 13.66f;
    Near(5.45f * Style{}.Factor(ipad) * ipad.pointsPerCqw, 70.0f, "a 70 point button on an iPad");
    // A key's capsule follows the button it sits on, so it never crowds a small
    // one or floats on a big one.
    Near(Style{}.Badge(SizeClass::Face, ipad), Style{}.badgeFace * Style{}.Factor(ipad),
        "the badge is scaled with its control");

    Canvas phone = CanvasFor(852.0f / 393.0f);
    phone.pointsPerCqw = 8.52f;
    const float phoneFactor = Style{}.Factor(phone);
    Near(phoneFactor, MaxScale(phone), "a phone stops at the height's limit");
    CHECK(5.45f * phoneFactor * phone.pointsPerCqw > 5.45f * 8.52f, "and its buttons are bigger than unstretched");

    // The player's setting is on top of the screen's, and cannot push past the
    // same limit.
    Style big;
    big.scale = 1.25f;
    Near(big.Factor(phone), MaxScale(phone), "a bigger setting stops at the height's limit too");
    Style small;
    small.scale = 0.75f;
    CHECK(small.Factor(phone) < phoneFactor, "a smaller setting still makes the deck smaller on a phone (%.3f)",
        small.Factor(phone));
    Near(small.Factor(reference), 0.75f, "and on the design's screen it is just the setting");
}

// The player's own arrangement: an offset in cqw from where the layout would
// have put each control, scaled with the deck, and clamped to the canvas.
static void Placements()
{
    const Canvas canvas = CanvasFor(kW / kH);
    const Deck plain = Layout(canvas);
    const Deck untouched = Layout(canvas, Style{}, Overrides{});
    bool identical = true;
    for (size_t i = 0; i < kControls; i++)
    {
        const Rect a = plain.Of(Control(i)), b = untouched.Of(Control(i));
        identical = identical && a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h &&
            plain.padCqw[i] == untouched.padCqw[i] && plain.steerGrab.x == untouched.steerGrab.x;
    }
    CHECK(identical, "no placements: the deck is the design's, to the bit");
    CHECK(!Overrides{}.Any(), "an empty set changes nothing");

    // 2 cqw right and 1 up, on the design's screen, is 18 and 9 pixels.
    Overrides overrides;
    overrides.of[size_t(Control::Nitro)].dx = 2.0f;
    overrides.of[size_t(Control::Nitro)].dy = -1.0f;
    CHECK(overrides.Any(), "a moved control is a change");
    const Deck moved = Layout(canvas, Style{}, overrides);
    RectNear(moved.nitro, plain.nitro.x * kW + 18.0f, plain.nitro.y * kH - 9.0f, plain.nitro.w * kW,
        plain.nitro.h * kH, "2 and -1 cqw away");
    CHECK(moved.padCqw[size_t(Control::Nitro)] <= plain.padCqw[size_t(Control::Nitro)],
        "and its touch area gives way to its new neighbours");

    // The offset is in the deck's own cqw, so it grows with the buttons: on a
    // small picture (where a cqw is worth less than the design's) 2 cqw of
    // offset is 2 cqw of a bigger deck, not of a bigger screen.
    Canvas small = CanvasFor(kW / kH);
    small.pointsPerCqw = 8.0f;
    const float factor = Style{}.Factor(small);
    CHECK(factor > 1.0f, "a small picture grows the deck (%.3f)", factor);
    const Deck grown = Layout(small, Style{}, overrides), grownPlain = Layout(small);
    Near((grown.nitro.x - grownPlain.nitro.x) / small.uPerCqw, 2.0f * factor, "the offset scales with the deck");
    Near((grownPlain.nitro.y - grown.nitro.y) / small.vPerCqw, 1.0f * factor, "both ways");

    // A control scaled about its centre keeps the centre and takes the size.
    Overrides bigger;
    bigger.of[size_t(Control::Camera)].scale = 1.5f;
    const Deck scaled = Layout(canvas, Style{}, bigger);
    Near(scaled.camera.w * kW, plain.camera.w * kW * 1.5f, "1.5 times as wide");
    Near(scaled.camera.CentreX(), plain.camera.CentreX(), "about its own centre");
    Near(scaled.camera.CentreY() / kH, plain.camera.CentreY() / kH, "and its own middle");

    // Nothing leaves the canvas, and nothing is bigger than it: the whole
    // rectangle is pulled back inside the margins, not its centre.
    Overrides far;
    far.of[size_t(Control::Start)].dx = 500.0f;
    far.of[size_t(Control::Start)].dy = -500.0f;
    far.of[size_t(Control::Camera)].scale = 40.0f;
    const Deck clamped = Layout(canvas, Style{}, far);
    CHECK(clamped.start.Right() <= (1.0f - canvas.right * canvas.uPerCqw) + 1e-5f, "a control pushed right stays in");
    CHECK(clamped.start.y >= canvas.top * canvas.vPerCqw - 1e-5f, "and one pushed up");
    CHECK(clamped.camera.w <= (1.0f - canvas.left * canvas.uPerCqw - canvas.right * canvas.uPerCqw) + 1e-5f,
        "a control made enormous is cut down to the canvas");

    // The stick: moved, it is still one piece -- the knob in the base, the
    // travel the base's own width, and the thumb's reach around it rather than
    // the screen's bottom left.
    Overrides stick;
    stick.of[size_t(Control::Steer)].dx = 40.0f;
    stick.of[size_t(Control::Steer)].dy = -10.0f;
    const Deck moved_steer = Layout(canvas, Style{}, stick);
    Near(moved_steer.steerStick.CentreX(), moved_steer.steer.CentreX(), "the knob stays in the base");
    Near(moved_steer.steerStick.CentreY(), moved_steer.steer.CentreY(), "and level with it");
    Near(moved_steer.steerTravel, (moved_steer.steer.w / canvas.uPerCqw - Style{}.stickKnob) * 0.5f * canvas.uPerCqw,
        "and travels the base's own reach");
    CHECK(moved_steer.steerGrab.Contains(moved_steer.steer.CentreX(), moved_steer.steer.CentreY()),
        "a placed stick is taken from where it is");
    CHECK(HitTest(moved_steer, moved_steer.steer.CentreX(), moved_steer.steer.CentreY()) == Control::Steer,
        "and presses from its middle");

    // Rectangles back to offsets: what the editor drags to is what the file keeps.
    Placement back;
    Placed(back, moved.nitro, plain.nitro, canvas, Style{});
    Near(back.dx, 2.0f, "a dragged rectangle gives its offset back");
    Near(back.dy, -1.0f, "both ways");
    Near(back.scale, 1.0f, "and its size");
    Placement rounded = bigger.of[size_t(Control::Camera)];
    ClampPlacement(plain.camera, canvas, Style{}, rounded);
    Near(rounded.scale, 1.5f, "clamping leaves a placement the canvas can hold alone");

    // Crossing, not touching: the D-pad's cells share their edges.
    CHECK(!Overlaps(plain, Control::DpadUp), "a D-pad cell does not cross its neighbours");
    Overrides collide;
    collide.of[size_t(Control::Nitro)].dx = plain.camera.CentreX() / canvas.uPerCqw - plain.nitro.CentreX() / canvas.uPerCqw;
    collide.of[size_t(Control::Nitro)].dy = plain.camera.CentreY() / canvas.vPerCqw - plain.nitro.CentreY() / canvas.vPerCqw;
    CHECK(Overlaps(Layout(canvas, Style{}, collide), Control::Nitro), "one control put on another does");

    // A picture of another shape than its slot keeps its shape in the slot, and
    // it is the picture that is pressed, not the empty room around it.
    art::Info wide = art::kArt[size_t(art::Id::Camera)];
    wide.aspect = 2.0f;
    wide.inradius = 0.5f;
    wide.sheet = 1;
    const uint8_t inside[4] = { 0, 0, 0, 0 };  // (all of it, and no pad)
    art::Install(art::Id::Camera, wide, std::vector<uint8_t>(inside, inside + 4));
    const art::Info& installed = art::InfoOf(art::Id::Camera);
    const Rect slot = plain.camera;
    const Rect fitted = Fitted(slot, installed, canvas.uPerCqw, canvas.vPerCqw);
    Near(fitted.w, slot.w, "a wide picture keeps the slot's width");
    Near(fitted.h, slot.h * 0.5f, "and takes half its height");
    CHECK(fitted.y > slot.y, "centred, so the room is above and below");
    Near(installed.aspect, Fitted(slot, installed, canvas.uPerCqw, canvas.vPerCqw).w * kW /
            (Fitted(slot, installed, canvas.uPerCqw, canvas.vPerCqw).h * kH), "its own shape");
    // (A style with no minimum touch: otherwise the pad a control reaches with
    // covers the picture's letterboxing, which is the point of the pad.)
    Style noPad;
    noPad.minTouch = 0.0f;
    const Deck tight = Layout(canvas, noPad);
    CHECK(HitTest(tight, slot.CentreX(), slot.CentreY()) == Control::Camera, "its middle presses it");
    CHECK(HitTest(tight, slot.CentreX(), slot.y + slot.h * 0.05f) != Control::Camera,
        "the slot's empty top is not the picture");
    CHECK(HitTest(tight, slot.CentreX(), slot.y + slot.h * 0.28f) == Control::Camera, "the picture's top is");
    CHECK(Fitted(slot, art::kArt[size_t(art::Id::Camera)], canvas.uPerCqw, canvas.vPerCqw).x == slot.x,
        "a bundled picture is its slot, to the bit");
    art::Restore(art::Id::Camera);
    CHECK(art::InfoOf(art::Id::Camera).field == art::kArt[size_t(art::Id::Camera)].field, "and its own is back");
}

// The editor's snapping: the nearest line, whether grid or neighbour, and
// nothing at all when it is off.
static void Snapping()
{
    const Canvas canvas = CanvasFor(kW / kH);  // (no points: a magnet of 1.2 cqw)
    const float u = canvas.uPerCqw, v = canvas.vPerCqw;
    const auto at = [u, v](float x, float y, float w, float h) { return Rect{ x * u, y * v, w * u, h * v }; };
    const auto cqw = [u, v](const Rect& r) {
        return Rect{ r.x / u, r.y / v, r.w / u, r.h / v };
    };

    const Grid grid;  // on, 1 cqw
    // 4.05 is 0.05 from the grid's 4 -- nearer than the rectangle's middle or
    // its far edge are to theirs (a 4.7 width keeps them off the lines).
    Snap across, down;
    const Rect snapped = Snapped(at(4.05f, 10.95f, 4.7f, 4.7f), canvas, grid, nullptr, 0, nullptr, 0, &across, &down);
    Near(cqw(snapped).x, 4.0f, "a left edge 0.05 from a grid line lands on it");
    Near(cqw(snapped).y, 11.0f, "and a top edge 0.05 below one");
    CHECK(!across.guide && across.moved, "the grid, and it moved");
    Near(cqw(snapped).w, 4.7f, "a snapped control keeps its size");

    // An edge's middle counts too: 3.6 + 2.5 = 6.1, which is 0.1 from 6 -- and
    // nearer than either edge is to its own line.
    const Rect middle = Snapped(at(3.6f, 20.0f, 5.0f, 5.0f), canvas, grid, nullptr, 0, nullptr, 0);
    Near(cqw(middle).x, 3.5f, "the middle lands on the line when it is the nearest");

    // A neighbour's edge beats the grid when it is nearer, and the grid wins
    // when it is nearer than that.
    // (4.3 with a 4.3 width: the left edge is 0.3 from the grid's 4, and the
    // middle and far edge are further from theirs.)
    const float neighbour[1] = { 4.55f };
    Snap guide;
    const Rect toNeighbour = Snapped(at(4.3f, 30.0f, 4.3f, 4.3f), canvas, grid, neighbour, 1, nullptr, 0, &guide);
    Near(cqw(toNeighbour).x, 4.55f, "0.25 from a neighbour is followed, not the grid's 0.3");
    CHECK(guide.guide, "and it is a guide");
    const float nearish[1] = { 4.7f };
    Snap noGuide;
    const Rect toGrid = Snapped(at(4.3f, 30.0f, 4.3f, 4.3f), canvas, grid, nearish, 1, nullptr, 0, &noGuide);
    Near(cqw(toGrid).x, 4.0f, "0.3 from the grid beats 0.4 from a neighbour");
    CHECK(!noGuide.guide, "so no guide");

    // A line further than the magnet is not a line: 2.3 cqw away.
    const float tooFar[1] = { 2.0f };
    Snap ignored;
    const Rect magnet = Snapped(at(4.3f, 40.0f, 4.3f, 4.3f), canvas, grid, tooFar, 1, nullptr, 0, &ignored);
    Near(cqw(magnet).x, 4.0f, "beyond the magnet only the grid is left");
    CHECK(!ignored.guide, "no guide");

    // The magnet is a fingertip's width in points: a cqw worth 12 points makes
    // it one, so a line 1.1 cqw away is out of reach.
    Canvas points = canvas;
    points.pointsPerCqw = 12.0f;
    const float near[1] = { 5.4f };
    Snap outOfReach;
    const Rect tipped = Snapped(at(4.3f, 50.0f, 4.3f, 4.3f), points, grid, near, 1, nullptr, 0, &outOfReach);
    Near(tipped.x / points.uPerCqw, 4.0f, "1.1 cqw is out of a 12 point reach");
    CHECK(!outOfReach.guide, "so the grid again");

    // Off is off: the rectangle is the one it was given.
    Grid off;
    off.snap = false;
    const Rect free = Snapped(at(4.3f, 60.0f, 4.3f, 4.3f), canvas, off, neighbour, 1, nullptr, 0);
    Near(cqw(free).x, 4.3f, "snapping off leaves the drag alone");
    Near(cqw(free).y, 60.0f, "both ways");

    // A pitch of nothing is no grid, and a neighbour is still a line.
    Grid none;
    none.pitch = 0.0f;
    Snap onlyGuide;
    const Rect guidesOnly = Snapped(at(4.3f, 70.0f, 4.3f, 4.3f), canvas, none, neighbour, 1, nullptr, 0, &onlyGuide);
    Near(cqw(guidesOnly).x, 4.55f, "no grid, but the neighbour is still followed");
    CHECK(onlyGuide.guide, "as a guide");

    // The deck's own lines, as the editor collects them: three a neighbour.
    float lines[3 * (kControls - 1)];
    const size_t count = NeighbourLines(Layout(canvas), Control::Nitro, true, lines, std::size(lines));
    CHECK(count == 3 * (kControls - 1), "three lines for each of the other controls (%zu)", count);
    bool hasLeft = false;
    for (size_t i = 0; i < count; i++)
        hasLeft = hasLeft || std::fabs(lines[i] - 2.65f) < 1e-4f;
    CHECK(hasLeft, "and a neighbour's edge among them");
}

// A point inside a control, as the canvas fraction (fx, fy) of it takes.
static void Inside(const Rect& r, float fx, float fy, float& u, float& v)
{
    u = r.x + r.w * fx;
    v = r.y + r.h * fy;
}

static void HitTesting()
{
    const Deck d = Layout(CanvasFor(kW / kH));
    float u = 0, v = 0;

    Inside(d.throttle, 0.5f, 0.5f, u, v);
    CHECK(HitTest(d, u, v) == Control::Throttle, "the gas pedal's middle presses the throttle");
    Inside(d.steer, 0.5f, 0.5f, u, v);
    CHECK(HitTest(d, u, v) == Control::Steer, "the middle of the base steers");
    Inside(d.nitro, 0.5f, 0.5f, u, v);
    CHECK(HitTest(d, u, v) == Control::Nitro, "the nitrous button's middle");
    Inside(d.shiftDown, 0.5f, 0.5f, u, v);
    CHECK(HitTest(d, u, v) == Control::ShiftDown, "the shifter's lower half");
    // Between the pedals, in the 9 cqw the cluster leaves between them.
    u = (d.brake.Right() + d.throttle.x) * 0.5f;
    v = d.throttle.CentreY();
    CHECK(HitTest(d, u, v) == Control::Count, "the gap between the pedals presses nothing");
    // The middle of the canvas: the game's own HUD, no control.
    CHECK(HitTest(d, 0.5f, 0.3f) == Control::Count, "the middle of the screen");
    // The stick is taken from the whole left of the picture, not just where its
    // base is drawn: a thumb below and right of it still steers.
    Inside(d.steerGrab, 0.5f, 0.5f, u, v);
    CHECK(HitTest(d, u, v) == Control::Steer, "the middle of the grab area steers");
    u = d.steerGrab.x + d.steerGrab.w - 0.01f;
    v = d.steerGrab.Bottom() - 0.01f;
    CHECK(HitTest(d, u, v) == Control::Steer, "so does its far corner");
    // Every control is reachable at its middle.
    for (size_t i = 0; i < kControls; i++)
    {
        Inside(d.Of(Control(i)), 0.5f, 0.5f, u, v);
        CHECK(HitTest(d, u, v) == Control(i), "control %zu at its middle", i);
    }
}

static void Steering()
{
    const Deck d = Layout(CanvasFor(kW / kH));
    const float centre = d.steer.CentreX();
    // Straight ahead where the thumb landed, and full lock at the stick's
    // travel each way.
    Near(SteerFromDrag(d, centre, centre), 0.0f, "the middle is straight");
    Near(SteerFromDrag(d, centre + d.steerTravel, centre), 1.0f, "a travel to the right is full right");
    Near(SteerFromDrag(d, centre - d.steerTravel, centre), -1.0f, "a travel to the left is full left");
    Near(SteerFromDrag(d, centre + 5.0f, centre), 1.0f, "past it stays full right");
    Near(SteerFromDrag(d, centre - 5.0f, centre), -1.0f, "and full left");
    Near(SteerFromDrag(d, centre + d.steerTravel * 0.5f, centre), 0.5f, "half a travel is half right");
    // Relative to the landing, not to the base: a thumb that landed right of
    // the centre starts straight ahead and turns from there.
    const float landed = centre + d.steerTravel * 0.5f;
    Near(SteerFromDrag(d, landed, landed), 0.0f, "straight ahead at a landing right of centre");
    Near(SteerFromDrag(d, landed + d.steerTravel, landed), 1.0f, "and a travel on from it is full right");

    // The stick follows the angle, its centre travelling the base's half less
    // its own, and the base itself does not move.
    Near(StickRect(d, 0.0f).CentreX(), centre, "the stick at rest is centred");
    Near(StickRect(d, 1.0f).CentreX(), centre + d.steerTravel, "full right: a travel along");
    Near(StickRect(d, -1.0f).CentreX(), centre - d.steerTravel, "full left: a travel back");
    Near(StickRect(d, 2.0f).CentreX(), StickRect(d, 1.0f).CentreX(), "past full lock the stick stays put");
}

// A frame, as input.cpp runs one: the events landed, then the pad is filled
// and the deck ages.
static Sample Frame(State& s)
{
    const Sample sample = SampleOf(s);
    Advance(s);
    return sample;
}

static void StateMachine()
{
    const Deck deck = Layout(CanvasFor(kW / kH));
    float u = 0, v = 0;
    Control hit = Control::Count;

    // One finger on the throttle pulls it.
    State s;
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    CHECK(Down(s, deck, 1, u, v, hit) && hit == Control::Throttle, "a finger on the gas presses it");
    CHECK(Frame(s).rightTrigger == 1.0f, "and the game gets the trigger");
    CHECK(Frame(s).rightTrigger == 1.0f, "still, while the finger is down");

    // A second finger adds a second control.
    Inside(deck.nitro, 0.5f, 0.5f, u, v);
    CHECK(Down(s, deck, 2, u, v, hit) && hit == Control::Nitro, "a second finger on the nitrous");
    Sample f = Frame(s);
    CHECK(f.buttons == hid::touch::button::B && f.rightTrigger == 1.0f, "throttle and nitrous together");

    // Lifting one keeps the other.
    Up(s, 1);
    f = Frame(s);
    CHECK(f.buttons == hid::touch::button::B, "the nitrous alone after the gas's finger lifts");
    Up(s, 2);
    int frames = 0;
    while (Frame(s).buttons != 0 && frames < 10)
        frames++;
    CHECK(frames >= 1 && frames <= hid::touch::kPressUpdates, "a press outlives the finger by %d frames", frames);

    // A finger that lands on nothing never presses anything, wherever it goes.
    Reset(s);
    CHECK(!Down(s, deck, 3, 0.5f, 0.3f, hit) && hit == Control::Count, "the middle of the picture presses nothing");
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Move(s, deck, 3, u, v);
    CHECK(Frame(s).rightTrigger == 0.0f, "and sliding onto the gas doesn't press it");
    Up(s, 3);
    Move(s, deck, 3, u, v);
    CHECK(Frame(s).rightTrigger == 0.0f, "nor does a finger that has lifted");

    // A press survives its finger sliding off the control (pointer capture).
    Reset(s);
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Down(s, deck, 4, u, v, hit);
    Move(s, deck, 4, 0.5f, 0.3f);
    CHECK(Frame(s).rightTrigger == 1.0f, "a thumb that slid off the gas still pulls");
    Up(s, 4);

    // A pedal's thumb rolling onto a face button: both are held, and the
    // button lets go as the thumb comes back down.
    Reset(s);
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Down(s, deck, 20, u, v, hit);
    CHECK(hit == Control::Throttle, "a thumb on the gas");
    Inside(deck.nitro, 0.45f, 0.55f, u, v);
    Move(s, deck, 20, u, v);
    f = Frame(s);
    CHECK(f.buttons == hid::touch::button::B && f.rightTrigger == 1.0f, "rolled onto B: both held");
    // Back down to the pedal: the button lets go, the throttle doesn't.
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Move(s, deck, 20, u, v);
    for (int i = 0; i < 1 + hid::touch::kPressUpdates; i++)
        Frame(s);
    f = Frame(s);
    CHECK(f.buttons == 0 && f.rightTrigger == 1.0f, "back on the gas: B let go, the throttle held");

    // Any of the four, from either pedal.
    const struct
    {
        Control face;
        uint16_t bit;
        const char* name;
    } roll[] = {
        { Control::Handbrake, hid::touch::button::A, "A" },
        { Control::RearView, hid::touch::button::Y, "Y" },
        { Control::Speedbreaker, hid::touch::button::X, "X" },
        { Control::Nitro, hid::touch::button::B, "B" },
    };
    for (const bool pedal : { true, false })
    {
        for (const auto& r : roll)
        {
            Reset(s);
            const Control from = pedal ? Control::Throttle : Control::Brake;
            Inside(deck.Of(from), 0.5f, 0.5f, u, v);
            CHECK(Down(s, deck, 30, u, v, hit) && hit == from, "a thumb on the %s", pedal ? "gas" : "brake");
            Inside(deck.Of(r.face), 0.5f, 0.5f, u, v);
            Move(s, deck, 30, u, v);
            f = Frame(s);
            const float trigger = pedal ? f.rightTrigger : f.leftTrigger;
            const float other = pedal ? f.leftTrigger : f.rightTrigger;
            CHECK(f.buttons == r.bit && trigger == 1.0f && other == 0.0f, "%s and %s at once: 0x%04x",
                pedal ? "gas" : "brake", r.name, f.buttons);
            // Lifting ends both.
            Up(s, 30);
            for (int i = 0; i < 1 + hid::touch::kPressUpdates; i++)
                Frame(s);
            f = Frame(s);
            CHECK(f.buttons == 0 && (pedal ? f.rightTrigger : f.leftTrigger) == 0.0f, "the lift lets go of both");
        }
    }

    // A pedal's thumb rolling onto the other pedal: both, the way a drift is
    // held -- and back off it again, leaving the first one down.
    Reset(s);
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Down(s, deck, 22, u, v, hit);
    Inside(deck.brake, 0.5f, 0.5f, u, v);
    Move(s, deck, 22, u, v);
    f = Frame(s);
    CHECK(f.rightTrigger == 1.0f && f.leftTrigger == 1.0f, "both pedals from the throttle's thumb");
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Move(s, deck, 22, u, v);
    for (int i = 0; i < 1 + hid::touch::kPressUpdates; i++)
        Frame(s);
    f = Frame(s);
    CHECK(f.rightTrigger == 1.0f && f.leftTrigger == 0.0f, "back on the throttle: the brake let go");
    // The other way round, from the brake.
    Reset(s);
    Inside(deck.brake, 0.5f, 0.5f, u, v);
    Down(s, deck, 23, u, v, hit);
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Move(s, deck, 23, u, v);
    f = Frame(s);
    CHECK(f.leftTrigger == 1.0f && f.rightTrigger == 1.0f && f.buttons == 0, "and from the brake's, nothing else");

    // A finger that landed on a button doesn't roll onto the next one.
    Reset(s);
    Inside(deck.nitro, 0.5f, 0.5f, u, v);
    Down(s, deck, 21, u, v, hit);
    Inside(deck.speedbreaker, 0.5f, 0.5f, u, v);
    Move(s, deck, 21, u, v);
    f = Frame(s);
    CHECK(f.buttons == hid::touch::button::B, "only a pedal's thumb rolls: B still alone, 0x%04x", f.buttons);

    // Two fingers on one control: the last to lift ends it.
    Reset(s);
    Inside(deck.handbrake, 0.5f, 0.5f, u, v);
    CHECK(Down(s, deck, 5, u, v, hit) && Down(s, deck, 6, u, v, hit), "two fingers on the handbrake");
    CHECK(s.Fingers(Control::Handbrake) == 2, "both counted: %u", unsigned(s.Fingers(Control::Handbrake)));
    Up(s, 5);
    CHECK(Frame(s).buttons == hid::touch::button::A, "one finger left holds it");
    Up(s, 6);
    CHECK(s.Fingers(Control::Handbrake) == 0, "no finger on it now");
    for (int i = 0; i < 1 + hid::touch::kPressUpdates; i++)
        Frame(s);
    CHECK(Frame(s).buttons == 0, "and after its hold runs out, nothing");

    // Steering: the first finger in the grab area has it, always relative to
    // where it landed, and it centres on release.
    Reset(s);
    const float centre = deck.steer.CentreX();
    Down(s, deck, 7, centre, deck.steer.CentreY(), hit);
    CHECK(hit == Control::Steer, "a finger on the base steers");
    f = Frame(s);
    CHECK(f.steerSet && std::fabs(f.steer) < 1e-6f, "straight ahead where it landed");
    Move(s, deck, 7, centre + deck.steerTravel, deck.steer.CentreY());
    CHECK(std::fabs(Frame(s).steer - 1.0f) < 1e-6f, "a travel right turns right");
    // Another finger in the grab area presses nothing, and doesn't take the
    // wheel.
    CHECK(!Down(s, deck, 8, deck.steerGrab.x + 0.01f, deck.steerGrab.Bottom() - 0.01f, hit), "a second finger there");
    CHECK(std::fabs(Frame(s).steer - 1.0f) < 1e-6f, "leaves the first one steering");
    Up(s, 7);
    f = Frame(s);
    CHECK(!f.steerSet && f.steer == 0.0f, "and letting go centres it");
    Up(s, 8);
    CHECK(Frame(s).steer == 0.0f, "the stray lift neither steers nor releases");

    // The shifter is a tap: one press, however briefly.
    Reset(s);
    Inside(deck.shiftUp, 0.5f, 0.5f, u, v);
    CHECK(Down(s, deck, 9, u, v, hit) && hit == Control::ShiftUp, "a finger on the up gear shifts up");
    Up(s, 9);
    int upFrames = 0;
    for (int i = 0; i < 4; i++)
        if (Frame(s).shiftUp)
            upFrames++;
    CHECK(upFrames == hid::touch::kPressUpdates, "a tap shifts for %d frames, not %d", hid::touch::kPressUpdates, upFrames);
    Inside(deck.shiftDown, 0.5f, 0.5f, u, v);
    Down(s, deck, 10, u, v, hit);
    f = Frame(s);
    CHECK(f.shiftDown && !f.shiftUp, "the down gear shifts down alone");

    // A menu opening (or the app leaving) drops everything at once.
    Reset(s);
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Down(s, deck, 11, u, v, hit);
    Reset(s);
    CHECK(Frame(s).rightTrigger == 0.0f && Frame(s).buttons == 0, "a reset holds nothing");

    // Every control can be pressed, one finger at a time.
    for (size_t i = 0; i < kControls; i++)
    {
        const Control c = Control(i);
        Reset(s);
        Inside(deck.Of(c), 0.5f, 0.5f, u, v);
        Down(s, deck, 100, u, v, hit);
        CHECK(hit == c, "control %zu is reachable", i);
        CHECK(s.Pressed(c), "control %zu presses", i);
    }

    // Twelve fingers at once, the deck's most: the five action buttons, the
    // three views and the four D-pad arrows.
    Reset(s);
    const Control many[] = { Control::Throttle, Control::Brake, Control::Handbrake, Control::Nitro, Control::Speedbreaker,
        Control::RearView, Control::QuickRearView, Control::Camera, Control::DpadUp, Control::DpadDown, Control::DpadLeft,
        Control::DpadRight };
    for (size_t i = 0; i < std::size(many); i++)
    {
        Inside(deck.Of(many[i]), 0.5f, 0.5f, u, v);
        CHECK(Down(s, deck, 200 + i, u, v, hit), "finger %zu lands", i);
    }
    f = Frame(s);
    constexpr uint16_t kAllButtons = hid::touch::button::DpadUp | hid::touch::button::DpadDown | hid::touch::button::DpadLeft |
        hid::touch::button::DpadRight | hid::touch::button::LeftShoulder | hid::touch::button::RightShoulder | hid::touch::button::A |
        hid::touch::button::B | hid::touch::button::X | hid::touch::button::Y;
    CHECK(f.buttons == kAllButtons, "twelve fingers: 0x%04x, want 0x%04x", f.buttons, kAllButtons);
    CHECK(f.rightTrigger == 1.0f && f.leftTrigger == 1.0f, "both pedals");

    // Start and Back, which the twelve above had no room for.
    Reset(s);
    for (size_t i = 0; i < 2; i++)
    {
        Inside(deck.Of(i == 0 ? Control::Start : Control::Back), 0.5f, 0.5f, u, v);
        CHECK(Down(s, deck, 300 + i, u, v, hit), "%s lands", i == 0 ? "start" : "back");
    }
    f = Frame(s);
    CHECK(f.buttons == (hid::touch::button::Start | hid::touch::button::Back), "start and back: 0x%04x", f.buttons);
}

// The overlay's own source, read (as tests/cursor_idle_test does for ui.cpp):
// the deck's pictures are drawn once each -- the plate's translucency is baked
// into the atlas, not added by a second pass -- and the accent is taken while
// drawing rather than in a static initialiser.
static void Overlay(const char* path)
{
    if (!path)
        return;
    FILE* file = fopen(path, "rb");
    CHECK(file != nullptr, "the overlay's source opens: %s", path);
    if (!file)
        return;
    std::string text;
    char buffer[4096];
    size_t read = 0;
    while ((read = fread(buffer, 1, sizeof buffer, file)) > 0)
        text.append(buffer, read);
    fclose(file);

    size_t draws = 0;
    for (size_t at = text.find("AddImage("); at != std::string::npos; at = text.find("AddImage(", at + 1))
        draws++;
    CHECK(draws == 1, "one picture, one draw: %zu AddImage calls", draws);
    CHECK(text.find("for (int pass") == std::string::npos, "no second pass over a picture");

    const size_t drawn = text.find("void DrawTouchOverlay(");
    CHECK(drawn != std::string::npos, "DrawTouchOverlay is there");
    const size_t accent = text.find("AccentColour()");
    CHECK(accent != std::string::npos && accent > drawn, "the accent is taken while drawing, not at start-up");
}

int main(int argc, char** argv)
{
    Geometry();
    OtherShapes();
    SafeArea();
    Pedals();
    Shape();
    Pads();
    Specs();
    Styles();
    Screens();
    Placements();
    Snapping();
    HitTesting();
    Steering();
    StateMachine();
    Overlay(argc > 1 ? argv[1] : nullptr);

    printf("%s (%d failures)\n", g_failures ? "FAILED" : "passed", g_failures);
    return g_failures ? 1 : 0;
}
