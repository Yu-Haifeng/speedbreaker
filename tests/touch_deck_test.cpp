// Tests for hid/touch_layout.h (and hid/touch_input.h): where the touch controls sit on the game's
// picture, which finger presses what, and what the game is told. Header only; from the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/touch_deck_test.cpp -o build/touch_deck_test && build/touch_deck_test
#include <hid/touch_layout.h>
#include <hid/touch_input.h>

#include <cmath>
#include <cstdio>
#include <iterator>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

using namespace hid::touch;

// The design's reference canvas in pixels (design/layout.json: 900 x 506.25,
// 1 cqw = 9 px). The tests say where a control goes in pixels there.
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

    // Bottom left: the steering panel and its track (the handoff's 35 x 11.1
    // at left 2.65, bottom 2.65, narrowed to 26 across; the track 4.9 high,
    // 1.35 in from the panel's sides, 3.55 down from its top).
    RectNear(d.steer, 23.85f, 382.5f, 234.0f, 99.9f, "steering panel");
    RectNear(d.steerTrack, 36.0f, 414.45f, 209.7f, 44.1f, "steering track");
    // The knob is 5.9 across, on the track's centre line: 0.5 cqw taller than
    // the track each way, so it overflows it.
    RectNear(d.steerKnob, 114.3f, 409.95f, 53.1f, 53.1f, "steering knob");
    Near(d.steerKnob.CentreX(), d.steerTrack.CentreX(), "knob centred on the track");

    // Bottom right: the action cluster (27 x 18.4 at right 2.65, bottom 2.65).
    RectNear(d.throttle, 736.2f, 417.6f, 139.95f, 64.8f, "throttle");
    RectNear(d.brake, 633.15f, 417.6f, 93.6f, 64.8f, "brake");
    RectNear(d.nitro, 792.0f, 319.5f, 84.15f, 84.15f, "nitro");
    RectNear(d.handbrake, 633.15f, 336.6f, 67.05f, 67.05f, "handbrake");
    RectNear(d.speedbreaker, 717.3f, 336.6f, 67.05f, 67.05f, "speedbreaker");
    Near(d.throttle.Right() * kW, 876.15f, "throttle's right edge on the margin");
    Near(d.brake.Right() * kW, 726.75f, "brake's right edge");
    Near((d.throttle.y - d.nitro.Bottom()) * kH, 1.55f * 9.0f, "nitro's bottom, 1.55 cqw above the pedals");

    // Top left: the D-pad's 3 x 3 grid of 4.45 cqw cells, arrows on its edges.
    RectNear(d.dpadUp, 63.9f, 23.85f, 40.05f, 40.05f, "d-pad up");
    RectNear(d.dpadLeft, 23.85f, 63.9f, 40.05f, 40.05f, "d-pad left");
    RectNear(d.dpadRight, 103.95f, 63.9f, 40.05f, 40.05f, "d-pad right");
    RectNear(d.dpadDown, 63.9f, 103.95f, 40.05f, 40.05f, "d-pad down");
    Near(d.dpadUp.Right(), d.dpadRight.x, "the up cell meets the right one's column");

    // Top middle: Start then Back, centred, 0.65 apart.
    RectNear(d.start, 372.375f, 24.3f, 74.7f, 36.45f, "start");
    RectNear(d.back, 452.925f, 24.3f, 74.7f, 36.45f, "back");
    Near((d.start.CentreX() + d.back.CentreX()) * kW, kW, "the two centred on the canvas");

    // Top right: three 5.45 across views, 0.55 apart; the shifter under them.
    RectNear(d.rearView, 719.1f, 23.85f, 49.05f, 49.05f, "rear view");
    RectNear(d.quickRearView, 773.1f, 23.85f, 49.05f, 49.05f, "quick rear view");
    RectNear(d.camera, 827.1f, 23.85f, 49.05f, 49.05f, "camera");
    Near(d.camera.Right() * kW, 876.15f, "camera's right edge on the margin");
    RectNear(d.shifter, 827.1f, 85.95f, 49.05f, 78.3f, "shifter");
    Near((d.shiftUp.y - d.shifter.y) * kH, 1.8f * 9.0f, "the shifter's header is 1.8 cqw");
    Near(d.shiftDown.Bottom(), d.shifter.Bottom(), "the down half ends the shifter");

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

static void Centring()
{
    // The steering panel's centre goes where a thumb took it; every other
    // control keeps the design's place.
    const Canvas c = CanvasFor(kW / kH);
    const Point place{ 0.30f, 0.70f };
    const Deck moved = Layout(c, place);
    Near(moved.steer.CentreX(), place.u, "the panel's centre is the thumb's, across");
    Near(moved.steer.CentreY(), place.v, "and down");
    Near(moved.steerTrack.CentreX(), place.u, "the track is centred on it");
    RectNear(moved.throttle, 736.2f, 417.6f, 139.95f, 64.8f, "the pedals stay put");
    RectNear(moved.dpadUp, 63.9f, 23.85f, 40.05f, 40.05f, "the D-pad stays put");

    // Nearer an edge than the panel is wide, it stops inside the margin.
    const Deck corner = Layout(c, { 0.0f, 0.0f });
    Near(corner.steer.x, c.left * c.uPerCqw, "the panel stops at the left margin");
    Near(corner.steer.y, c.top * c.vPerCqw, "and at the top's");
    const Deck far = Layout(c, { 1.0f, 1.0f });
    Near(1.0f - far.steer.Right(), c.right * c.uPerCqw, "and at the right's");
    Near(1.0f - far.steer.Bottom(), c.bottom * c.vPerCqw, "and the bottom's");
}

static void OtherShapes()
{
    // The design's cqw sizes hold on any picture shape: wider than 16:9 (an
    // iPhone filled by Hor+) the canvas is shorter in cqw, narrower (a 4:3
    // iPad) taller, and a control keeps its size in cqw either way.
    for (float aspect : { 16.0f / 9.0f, 19.5f / 9.0f, 4.0f / 3.0f, 16.0f / 10.0f })
    {
        const Canvas c = CanvasFor(aspect);
        const Deck d = Layout(c);
        const float w = 900.0f, h = w / aspect;  // pixels: a round control must be round here
        Near(d.steer.w, 0.26f, "steering panel 26 cqw across");
        Near(d.steer.h, 11.1f * c.vPerCqw, "steering panel 11.1 cqw down");
        Near(d.throttle.w + d.brake.w, 25.95f * c.uPerCqw, "the pedals' 25.95 cqw");
        Near(1.0f - d.steer.Bottom(), 2.65f * c.vPerCqw, "the panel 2.65 cqw above the bottom");
        Near(1.0f - d.camera.Right(), 2.65f * c.uPerCqw, "the views 2.65 cqw in from the right");
        // A round control stays round on the screen: the same cqw both ways.
        Near(d.nitro.w * w / (d.nitro.h * h), 1.0f, "nitro is round on the screen");
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
    // the bottom, which of a 600-point picture is 9.8 and 3.5 cqw, so both
    // must win.
    const Canvas c = CanvasFor(19.5f / 9.0f);
    Canvas same = InsetsFor(c, 600.0f, 9.0f, 0.0f, 9.0f, 12.0f);
    Near(same.left, 2.65f, "a small safe area leaves the design's margin");
    Near(same.bottom, 2.65f, "and the bottom's");

    Canvas notched = InsetsFor(c, 600.0f, 59.0f, 0.0f, 59.0f, 21.0f);
    Near(notched.left, 59.0f * 100.0f / 600.0f, "a notch pushes the left margin in");
    Near(notched.right, notched.left, "and the right's, symmetrically");
    Near(notched.top, 2.65f, "the top keeps the design's");
    CHECK(notched.bottom > 2.65f, "the home indicator pushes the bottom in");

    // The deck follows: the steering panel starts at the new margin and stays
    // clear of the bottom's.
    const Deck d = Layout(notched);
    Near(d.steer.x, notched.left * notched.uPerCqw, "the panel at the new left margin");
    Near(1.0f - d.steer.Bottom(), notched.bottom * notched.vPerCqw, "and clear of the bottom's");
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
    CHECK(HitTest(d, u, v) == Control::Throttle, "the throttle's middle presses the throttle");
    Inside(d.steerTrack, 0.5f, 0.5f, u, v);
    CHECK(HitTest(d, u, v) == Control::Steer, "the track's middle steers");
    Inside(d.nitro, 0.5f, 0.5f, u, v);
    CHECK(HitTest(d, u, v) == Control::Nitro, "the nitro orb's middle");
    Inside(d.shiftDown, 0.5f, 0.5f, u, v);
    CHECK(HitTest(d, u, v) == Control::ShiftDown, "the shifter's lower half");
    // The shifter's header is not a control: only its two halves shift.
    Inside(d.shifter, 0.5f, 0.04f, u, v);
    CHECK(HitTest(d, u, v) == Control::Count, "the shifter's header does nothing");
    // Between the pedals, in the 1.05 cqw gap the brake's width leaves.
    u = (d.brake.Right() + d.throttle.x) * 0.5f;
    v = d.throttle.CentreY();
    CHECK(HitTest(d, u, v) == Control::Count, "the gap between the pedals presses nothing");
    // The middle of the canvas: the game's own HUD, no control.
    CHECK(HitTest(d, 0.5f, 0.3f) == Control::Count, "the middle of the screen");
    // The steering panel is its whole area, not just the track: a thumb on the
    // panel's lower corner still steers.
    Inside(d.steer, 0.02f, 0.95f, u, v);
    CHECK(HitTest(d, u, v) == Control::Steer, "the panel's corner steers");
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
    const Rect& t = d.steerTrack;
    // A drag is read against where it started. With the panel centred there,
    // the track's centre line is straight ahead.
    const float origin = t.CentreX();
    Near(SteerFromDrag(d, origin, origin), 0.0f, "the track's middle is straight");
    // The knob's own travel is the reading's range: a thumb at a travel end is
    // full lock, and past it stays there.
    const float span = 0.44f * t.w;
    Near(SteerFromDrag(d, origin - span, origin), -1.0f, "the knob's leftmost place is full left");
    Near(SteerFromDrag(d, origin + span, origin), 1.0f, "its rightmost, full right");
    Near(SteerFromDrag(d, origin - span * 2.0f, origin), -1.0f, "past it still full left");
    Near(SteerFromDrag(d, origin + span * 2.0f, origin), 1.0f, "past it still full right");
    Near(SteerFromDrag(d, origin + span * 0.5f, origin), 0.5f, "half way out is half right");
    Near(SteerFromDrag(d, origin - span * 0.25f, origin), -0.25f, "and a quarter in is a quarter left");

    // Wherever the thumb lands, the knob lands under it (in pixels: a
    // thousandth of the canvas is a pixel at this size).
    for (float k : { -1.0f, -0.6f, -0.25f, 0.0f, 0.25f, 0.6f, 1.0f })
    {
        const float at = origin + k * span;
        Near((KnobRect(d, SteerFromDrag(d, at, origin)).CentreX() - at) * kW, 0.0f, "the knob under the thumb");
    }

    // The knob follows, 44% of the track's width each way at full lock, and
    // the fill runs from the centre line to it.
    Near(KnobRect(d, 0.0f).CentreX(), t.CentreX(), "the knob at rest is centred");
    Near(KnobRect(d, 1.0f).CentreX(), t.CentreX() + 0.44f * t.w, "full right: 44% along the track");
    Near(KnobRect(d, -1.0f).CentreX(), t.CentreX() - 0.44f * t.w, "full left: 44% back");
    Near(KnobRect(d, 2.0f).CentreX(), KnobRect(d, 1.0f).CentreX(), "past full lock the knob stays put");
    Rect f = SteerFill(d, 0.0f);
    Near(f.w, 0.0f, "no fill at rest");
    f = SteerFill(d, 0.5f);
    Near(f.x, t.CentreX(), "a right lock's fill starts at the centre");
    Near(f.w, 0.22f * t.w, "half right fills half of 44%");
    f = SteerFill(d, -0.5f);
    Near(f.Right(), t.CentreX(), "a left lock's fill ends at the centre");
    Near(f.w, 0.22f * t.w, "half left fills as much");
    Near(f.h, 0.36f * d.vPerCqw, "the fill is 0.36 cqw");
}

// The steering panel comes to the thumb that takes it, and the wheel is read
// from there.
static void PanelFollowsThumb()
{
    const Canvas c = CanvasFor(kW / kH);
    const Deck home = Layout(c);
    const float u = 0.30f, v = 0.70f;
    State s;
    Control hit = Control::Count;

    CHECK(Down(s, home, 1, u, v, hit) && hit == Control::Steer, "a thumb on the panel steers");
    // The panel comes to the thumb: its centre is where it landed, the track
    // runs through it, and the wheel starts straight ahead under it.
    Deck placed = DeckFor(c, s);
    Near(placed.steer.CentreX(), u, "the panel follows the thumb across");
    Near(placed.steer.CentreY(), v, "and down");
    Near(placed.steerTrack.CentreX(), u, "the track's centre is the thumb's");
    Sample f = SampleOf(s);
    CHECK(f.steerSet && std::fabs(f.steer) < 1e-6f, "and starts straight ahead");
    Near((KnobRect(placed, f.steer).CentreX() - u) * kW, 0.0f, "the knob under the thumb");

    // The knob follows the thumb across the track's travel, still under it.
    const float span = 0.44f * placed.steerTrack.w;
    for (float k : { -1.0f, -0.4f, 0.4f, 1.0f })
    {
        Move(s, placed, 1, u + k * span, v);
        f = SampleOf(s);
        Near(f.steer, k, "steering follows the thumb");
        Near((KnobRect(placed, f.steer).CentreX() - (u + k * span)) * kW, 0.0f, "the knob under it");
    }

    // Letting go centres the wheel and sends the panel back to the handoff's
    // corner, where the next thumb expects it.
    Up(s, 1);
    f = SampleOf(s);
    CHECK(!f.steerSet && f.steer == 0.0f, "letting go centres the wheel");
    const Deck released = DeckFor(c, s);
    Near(released.steer.CentreX(), home.steer.CentreX(), "the panel back in the corner, across");
    Near(released.steer.CentreY(), home.steer.CentreY(), "and down");

    // The next thumb takes it again, wherever it lands: nearer an edge than
    // the panel can reach the panel stops at the margin, and the drag still
    // starts straight ahead rather than snapping the wheel over.
    CHECK(Down(s, released, 2, 0.01f, 0.5f, hit) && hit == Control::Steer, "a thumb at the canvas's edge steers");
    placed = DeckFor(c, s);
    Near(placed.steer.x, c.left * c.uPerCqw, "the panel as far left as it goes");
    CHECK(std::fabs(SampleOf(s).steer) < 1e-6f, "straight ahead at the landing, not snapped left");
    Move(s, placed, 2, 0.01f + span, 0.5f);
    Near(SampleOf(s).steer, 1.0f, "and a thumb's travel right is full right");
    Up(s, 2);
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
    CHECK(Down(s, deck, 1, u, v, hit) && hit == Control::Throttle, "a finger on the throttle presses it");
    CHECK(Frame(s).rightTrigger == 1.0f, "and the game gets the trigger");
    CHECK(Frame(s).rightTrigger == 1.0f, "still, while the finger is down");

    // A second finger adds a second control.
    Inside(deck.nitro, 0.5f, 0.5f, u, v);
    CHECK(Down(s, deck, 2, u, v, hit) && hit == Control::Nitro, "a second finger on the nitro orb");
    Sample f = Frame(s);
    CHECK(f.buttons == hid::touch::button::B && f.rightTrigger == 1.0f, "throttle and nitro together");

    // Lifting one keeps the other.
    Up(s, 1);
    f = Frame(s);
    CHECK(f.buttons == hid::touch::button::B, "the nitro orb alone after the throttle's finger lifts");
    Up(s, 2);
    int frames = 0;
    while (Frame(s).buttons != 0 && frames < 10)
        frames++;
    CHECK(frames >= 1 && frames <= hid::touch::kPressUpdates, "a press outlives the finger by %d frames", frames);

    // A finger that lands on nothing never presses anything, wherever it goes.
    Reset(s);
    Inside(deck.steerTrack, 0.5f, 0.12f, u, v);  // the panel's head: not a control
    CHECK(!Down(s, deck, 3, 0.5f, 0.3f, hit) && hit == Control::Count, "the middle of the picture presses nothing");
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Move(s, deck, 3, u, v);
    CHECK(Frame(s).rightTrigger == 0.0f, "and sliding onto the throttle doesn't press it");
    // Nor does a lifted finger's slide press one.
    Up(s, 3);
    Move(s, deck, 3, u, v);
    CHECK(Frame(s).rightTrigger == 0.0f, "nor does a finger that has lifted");

    // A press survives its finger sliding off the control (pointer capture).
    Reset(s);
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Down(s, deck, 4, u, v, hit);
    Move(s, deck, 4, 0.5f, 0.3f);
    CHECK(Frame(s).rightTrigger == 1.0f, "a thumb that slid off its pedal still pulls");
    Up(s, 4);

    // The throttle's thumb rolling up onto the nitrous or the Speedbreaker:
    // both are held, and the orb lets go as the thumb comes back down.
    Reset(s);
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Down(s, deck, 20, u, v, hit);
    CHECK(hit == Control::Throttle, "a thumb on the throttle");
    Inside(deck.nitro, 0.45f, 0.55f, u, v);
    Move(s, deck, 20, u, v);
    f = Frame(s);
    CHECK(f.buttons == hid::touch::button::B && f.rightTrigger == 1.0f, "rolled onto the nitrous: both held");
    // Back down to the pedal: the nitrous lets go, the throttle doesn't.
    Inside(deck.throttle, 0.5f, 0.5f, u, v);
    Move(s, deck, 20, u, v);
    for (int i = 0; i < 1 + hid::touch::kPressUpdates; i++)
        Frame(s);
    f = Frame(s);
    CHECK(f.buttons == 0 && f.rightTrigger == 1.0f, "back on the pedal: the nitrous let go, the throttle held");
    // And onto the Speedbreaker.
    Inside(deck.speedbreaker, 0.5f, 0.5f, u, v);
    Move(s, deck, 20, u, v);
    f = Frame(s);
    CHECK(f.buttons == hid::touch::button::X && f.rightTrigger == 1.0f, "rolled onto the Speedbreaker");
    // Only those two roll: the brake or the handbrake don't join in.
    Inside(deck.brake, 0.5f, 0.5f, u, v);
    Move(s, deck, 20, u, v);
    for (int i = 0; i < 1 + hid::touch::kPressUpdates; i++)
        Frame(s);
    f = Frame(s);
    CHECK(f.buttons == 0 && f.rightTrigger == 1.0f, "the brake pedal joins nothing");
    Inside(deck.handbrake, 0.5f, 0.5f, u, v);
    Move(s, deck, 20, u, v);
    CHECK(Frame(s).buttons == 0, "nor the handbrake");
    // Lifting ends both.
    Inside(deck.nitro, 0.5f, 0.5f, u, v);
    Move(s, deck, 20, u, v);
    CHECK(Frame(s).buttons == hid::touch::button::B, "rolled onto the nitrous again");
    Up(s, 20);
    for (int i = 0; i < 1 + hid::touch::kPressUpdates; i++)
        Frame(s);
    f = Frame(s);
    CHECK(f.buttons == 0 && f.rightTrigger == 0.0f, "the lift lets go of both");

    // A finger that landed on an orb doesn't roll onto the next one.
    Reset(s);
    Inside(deck.nitro, 0.5f, 0.5f, u, v);
    Down(s, deck, 21, u, v, hit);
    Inside(deck.speedbreaker, 0.5f, 0.5f, u, v);
    Move(s, deck, 21, u, v);
    f = Frame(s);
    CHECK(f.buttons == hid::touch::button::B, "only the throttle's thumb rolls: the nitrous still alone, 0x%04x", f.buttons);

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

    // Steering: the first finger on the panel has it, the knob follows, and
    // it centres on release.
    Reset(s);
    const Rect& t = deck.steerTrack;
    CHECK(Down(s, deck, 7, t.CentreX(), t.CentreY(), hit) && hit == Control::Steer, "a finger on the track steers");
    f = Frame(s);
    CHECK(f.steerSet && std::fabs(f.steer) < 1e-6f, "the track's middle is straight");
    Move(s, deck, 7, t.CentreX() + 0.44f * t.w, t.CentreY());
    CHECK(std::fabs(Frame(s).steer - 1.0f) < 1e-6f, "moving right turns right");
    // Another finger on the panel presses nothing, and doesn't take the wheel.
    CHECK(!Down(s, deck, 8, deck.steer.x + 0.01f, deck.steer.y + 0.01f, hit), "a second finger on the panel");
    CHECK(std::fabs(Frame(s).steer - 1.0f) < 1e-6f, "leaves the first one steering");
    Up(s, 7);
    f = Frame(s);
    CHECK(!f.steerSet && f.steer == 0.0f, "and letting go centres it");
    // The other finger's lift does nothing.
    Up(s, 8);
    CHECK(Frame(s).steer == 0.0f, "the stray lift neither steers nor releases");

    // The shifter is a tap: one press, however briefly.
    Reset(s);
    Inside(deck.shiftUp, 0.5f, 0.5f, u, v);
    CHECK(Down(s, deck, 9, u, v, hit) && hit == Control::ShiftUp, "a finger on the up half shifts up");
    Up(s, 9);
    int upFrames = 0;
    for (int i = 0; i < 4; i++)
        if (Frame(s).shiftUp)
            upFrames++;
    CHECK(upFrames == hid::touch::kPressUpdates, "a tap shifts for %d frames, not %d", hid::touch::kPressUpdates, upFrames);
    Inside(deck.shiftDown, 0.5f, 0.5f, u, v);
    Down(s, deck, 10, u, v, hit);
    f = Frame(s);
    CHECK(f.shiftDown && !f.shiftUp, "the down half shifts down alone");

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

int main()
{
    Geometry();
    Centring();
    OtherShapes();
    SafeArea();
    HitTesting();
    Steering();
    PanelFollowsThumb();
    StateMachine();

    printf("%s (%d failures)\n", g_failures ? "FAILED" : "passed", g_failures);
    return g_failures ? 1 : 0;
}
