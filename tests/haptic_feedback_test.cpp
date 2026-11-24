// Tests for hid/haptic_feedback.h: the Xbox 360 rumble motors as one phone
// haptic, the touch deck's button taps and steering detents, and the engine's
// start/stop gate. Header only; from the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/haptic_feedback_test.cpp -o build/haptic_feedback_test && build/haptic_feedback_test
#include <hid/haptic_feedback.h>

#include <cmath>
#include <cstdint>
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

using namespace hid;
using hid::haptic::Cues;
using hid::haptic::EngineGate;
using hid::haptic::TouchFeedback;
using hid::touch::Control;

static void Near(float got, float want, const char* what)
{
    CHECK(std::fabs(got - want) < 0.005f, "%s: %.4f, want %.4f", what, got, want);
}

static constexpr uint32_t Bit(Control c)
{
    return uint32_t(1) << size_t(c);
}

static touch::Sample Frame(uint32_t pressed, float steer = 0.0f, bool steerSet = false)
{
    touch::Sample s;
    s.pressed = pressed;
    s.steer = steer;
    s.steerSet = steerSet;
    return s;
}

static Cues SteerStep(TouchFeedback& f, float steer, bool held = true, float dt = 0.25f)
{
    return f.Step(Frame(0, steer, held), dt);
}

static void Motors()
{
    // The curve: off at zero and under the floor, full at full, rising.
    CHECK(haptic::MotorResponse(0) == 0.0f, "0 motor");
    CHECK(haptic::MotorResponse(655) == 0.0f, "under the floor (%.4f of full)", 655.0f / 65535.0f);
    Near(haptic::MotorResponse(65535), 1.0f, "full motor");
    float last = -1.0f;
    for (int i = 0; i <= 20; i++)
    {
        const float got = haptic::MotorResponse(uint16_t(i * 65535 / 20));
        CHECK(got >= last, "rising at %d/20 (%.4f after %.4f)", i, got, last);
        last = got;
    }
    // The gamma is what lifts the quiet ones, so a third of the travel is
    // well above a third of the feel.
    Near(haptic::MotorResponse(21845), std::pow(1.0f / 3.0f, haptic::kMotorGamma), "a third of the travel");
}

static void Rumble()
{
    // Nothing asked for: nothing played.
    const haptic::Pattern none = haptic::GameRumble(0, 0);
    CHECK(none.intensity == 0.0f && none.sharpness == 0.0f, "no motors is silence");

    // The left motor alone is as dull as the phone goes; the right alone as
    // sharp; equal ones are the middle.
    const haptic::Pattern left = haptic::GameRumble(65535, 0);
    Near(left.intensity, 1.0f, "left alone: strength");
    Near(left.sharpness, haptic::kSharpnessMin, "left alone: dull");

    const haptic::Pattern right = haptic::GameRumble(0, 65535);
    Near(right.intensity, 1.0f, "right alone: strength");
    Near(right.sharpness, haptic::kSharpnessMax, "right alone: sharp");

    const haptic::Pattern both = haptic::GameRumble(65535, 65535);
    Near(both.intensity, 1.0f, "both: strength");
    Near(both.sharpness, 0.5f, "both: the middle");

    // The stronger motor is the strength, whichever side it is.
    const haptic::Pattern strongRight = haptic::GameRumble(30000, 50000);
    Near(strongRight.intensity, haptic::MotorResponse(50000), "the right is stronger");
    const haptic::Pattern strongLeft = haptic::GameRumble(50000, 30000);
    Near(strongLeft.intensity, haptic::MotorResponse(50000), "the left is stronger");

    // Swapping the motors keeps the strength and mirrors the sharpness.
    const haptic::Pattern a = haptic::GameRumble(10000, 40000);
    const haptic::Pattern b = haptic::GameRumble(40000, 10000);
    Near(a.intensity, b.intensity, "swapped strengths");
    Near(a.sharpness + b.sharpness, 1.0f, "swapped sharpness mirrors");
    Near(a.sharpness, 1.0f - b.sharpness, "swapped sharpness mirrors");
}

static void ButtonTaps()
{
    TouchFeedback f;
    Cues c = f.Step(Frame(Bit(Control::Handbrake)), 0.0f);
    CHECK(c.TapCount() == 1 && (c.tapped & Bit(Control::Handbrake)), "a new press taps once");

    c = f.Step(Frame(Bit(Control::Handbrake)), 0.0f);
    CHECK(c.TapCount() == 0, "held: no repeat");

    f.Step(Frame(0), 0.0f);
    c = f.Step(Frame(Bit(Control::Handbrake)), 0.0f);
    CHECK(c.TapCount() == 1, "let go and pressed again: one more");

    // The steering panel is not a button: its feedback is the detents.
    f.Reset();
    c = f.Step(Frame(Bit(Control::Steer)), 0.0f);
    CHECK(c.TapCount() == 0, "steering presses no tap");

    // Two controls in one frame are two bits (the caller plays one tap).
    f.Reset();
    c = f.Step(Frame(Bit(Control::Handbrake) | Bit(Control::Nitro)), 0.0f);
    CHECK(c.TapCount() == 2, "two buttons at once: %u", unsigned(c.TapCount()));
    CHECK((c.tapped & Bit(Control::Handbrake)) && (c.tapped & Bit(Control::Nitro)), "both bits");

    // The deck coming up under a held button still taps: nothing was seen
    // before it.
    f.Reset();
    c = f.Step(Frame(Bit(Control::Speedbreaker)), 0.0f);
    CHECK(c.TapCount() == 1, "a press present at the first frame taps");
}

static void SteeringDetents()
{
    // The first sample is the baseline, wherever it is: the panel comes to
    // the thumb, and nothing ticks just for taking it.
    TouchFeedback f;
    Cues c = SteerStep(f, 1.0f);
    CHECK(!c.fullLock && !c.centre, "the first sample (at full lock) is quiet");

    // Across the centre, once, after a side has been taken.
    f.Reset();
    CHECK(!SteerStep(f, 0.0f).centre, "landing at centre is quiet");
    CHECK(!SteerStep(f, 0.3f).centre, "taking the right is quiet");
    CHECK(!SteerStep(f, 0.1f).centre, "still right: quiet");
    c = SteerStep(f, -0.3f);
    CHECK(c.centre, "crossing to the left ticks");
    CHECK(!SteerStep(f, -0.4f).centre, "staying left: quiet");

    // Jitter inside the deadband never takes a side, so never ticks.
    f.Reset();
    SteerStep(f, 0.0f);
    CHECK(!SteerStep(f, 0.08f).centre, "below the arm: quiet");
    CHECK(!SteerStep(f, -0.08f).centre, "below the arm the other way: quiet");
    CHECK(!SteerStep(f, 0.08f).centre, "and back: quiet");

    // Full lock: once on arriving, not while held, and again only after
    // leaving the lock zone.
    f.Reset();
    SteerStep(f, 0.0f);
    CHECK(!SteerStep(f, 0.5f).fullLock, "halfway: no lock tick");
    CHECK(SteerStep(f, 0.98f).fullLock, "full lock ticks");
    CHECK(!SteerStep(f, 1.0f).fullLock, "held at lock: no repeat");
    CHECK(!SteerStep(f, 0.90f).fullLock, "still in the zone: no repeat");
    SteerStep(f, 0.80f);  // out of the zone: arms again
    CHECK(SteerStep(f, 0.99f).fullLock, "back to lock: ticks again");

    // Letting go and taking it again is a new hold, not a crossing.
    f.Reset();
    SteerStep(f, 0.6f);
    c = SteerStep(f, 0.0f, false);
    CHECK(!c.centre && !c.fullLock, "letting go is quiet");
    c = SteerStep(f, 0.6f);
    CHECK(!c.centre, "re-grabbing on the same side is quiet");
    CHECK(SteerStep(f, -0.6f).centre, "crossing after the re-grab ticks");

    // A thumb waggling across the centre is rate-limited: one tick, then
    // nothing until the cooldown has run.
    f.Reset();
    SteerStep(f, 0.2f, true, 1.0f / 60.0f);
    CHECK(SteerStep(f, -0.2f, true, 1.0f / 60.0f).centre, "the first crossing ticks");
    CHECK(!SteerStep(f, 0.2f, true, 1.0f / 60.0f).centre, "the next frame is still cooling");
    CHECK(!SteerStep(f, -0.2f, true, 1.0f / 60.0f).centre, "and the one after");
    CHECK(SteerStep(f, 0.2f, true, 0.25f).centre, "after the cooldown it ticks again");

    // dt is clamped, not trusted: a first frame or a suspension can't skip
    // a cooldown or wrap it.
    f.Reset();
    SteerStep(f, 0.3f, true, -1.0f);
    CHECK(SteerStep(f, -0.3f, true, 100.0f).centre, "an absurd dt is clamped");
}

static void Gate()
{
    EngineGate g;
    CHECK(g.Step(true, 0.0), "wanted: on at once");
    CHECK(g.Step(false, 1.0), "not wanted: held");
    CHECK(g.Step(false, 2.9), "still inside the hold");
    CHECK(!g.Step(false, 3.0), "the hold is over: off");

    g.Reset();
    g.Step(true, 0.0);
    CHECK(g.Step(false, 1.0), "holding after a stop is asked for");
    CHECK(g.Step(true, 1.5), "wanted again inside the hold: on");
    CHECK(g.Step(false, 2.0), "stops again: the hold restarts");
    CHECK(g.Step(false, 3.9), "just inside");
    CHECK(!g.Step(false, 4.0), "off after two seconds");

    EngineGate immediate;
    CHECK(immediate.Step(true, 0.0), "on");
    CHECK(!immediate.Step(false, 0.0, 0.0), "hold 0: off at once");

    EngineGate reset;
    reset.Step(true, 0.0);
    reset.Reset();
    CHECK(!reset.Step(false, 0.0), "Reset forces off");
}

static void Constants()
{
    for (const haptic::Pattern p : { haptic::kButtonTap, haptic::kCentreDetent, haptic::kLockDetent,
             haptic::kRefusedTap, haptic::kGuideTick, haptic::kPickTap })
        CHECK(p.intensity >= 0.0f && p.intensity <= 1.0f && p.sharpness >= 0.0f && p.sharpness <= 1.0f, "cues stay in range");
    CHECK(haptic::kLockDetent.intensity > haptic::kCentreDetent.intensity, "the lock tick is firmer than the centre's");
    CHECK(haptic::kButtonTap.intensity < 1.0f, "a button tap is not the strongest rumble");
    // The editor's: a refusal is the firmest and dullest of its three, so "no"
    // is told apart from "a line" and from "another control" by feel alone.
    CHECK(haptic::kRefusedTap.intensity > haptic::kButtonTap.intensity, "a refusal is firmer than a press");
    CHECK(haptic::kRefusedTap.sharpness < haptic::kGuideTick.sharpness, "and duller than a line taken");
    CHECK(haptic::kGuideTick.sharpness > haptic::kPickTap.sharpness, "a line ticks sharper than a pick");
}

int main()
{
    Motors();
    Rumble();
    ButtonTaps();
    SteeringDetents();
    Gate();
    Constants();

    printf("%s (%d failures)\n", g_failures ? "FAILED" : "passed", g_failures);
    return g_failures ? 1 : 0;
}
