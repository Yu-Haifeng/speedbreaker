// Tests for the editor's arithmetic: hid/touch_edit.h (one change to the deck,
// accepted or refused, and who it would land on; the undo history; the names a
// player sees; a nudge's step) and hid/touch_pick.h (which control a tap takes
// hold of, and the one under it).
//
// Header only; from the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/touch_edit_test.cpp -o build/touch_edit_test && build/touch_edit_test
#include <hid/touch_edit.h>
#include <hid/touch_pick.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

using namespace hid::touch;

// The reference canvas in pixels (900 x 506.25, 1 cqw = 9 px, factor 1).
static constexpr float kW = 900.0f, kH = 506.25f;

// The placement that puts a control's centre on another point, in cqw.
static Placement Towards(const Deck& deck, Control control, float xCqw, float yCqw, const Canvas& canvas)
{
    Placement placement;
    const Rect rect = deck.Of(control);
    placement.dx = (xCqw * canvas.uPerCqw - rect.CentreX()) / canvas.uPerCqw;
    placement.dy = (yCqw * canvas.vPerCqw - rect.CentreY()) / canvas.vPerCqw;
    return placement;
}

static void Transactions()
{
    const Canvas canvas = CanvasFor(kW / kH);
    const Style style;
    const Deck plain = Layout(canvas, style);

    // No change at all is not a crossing.
    {
        const edit::Attempt same = edit::Evaluate(canvas, style, Overrides{}, Control::Nitro, Placement{});
        CHECK(same.ok, "a control left where it is is fine");
        CHECK(same.other == Control::Count, "and lands on nobody");
        CHECK(edit::RefusalText(same).empty(), "so there is nothing to say");
    }

    // Two controls in the same place: refused, and named.
    {
        const Rect steer = plain.steer;
        const Placement onTop = Towards(plain, Control::Nitro, steer.CentreX() / canvas.uPerCqw,
            steer.CentreY() / canvas.vPerCqw, canvas);
        const edit::Attempt land = edit::Evaluate(canvas, style, Overrides{}, Control::Nitro, onTop);
        CHECK(!land.ok, "one control put on another is refused");
        CHECK(land.other == Control::Steer, "and the one it lands on is the steering (%zu)", size_t(land.other));
        const std::string why = edit::RefusalText(land);
        CHECK(why.find(edit::NameOf(Control::Nitro)) != std::string::npos, "the refusal names the control: %s", why.c_str());
        CHECK(why.find(edit::NameOf(Control::Steer)) != std::string::npos, "and the one it would land on: %s", why.c_str());
    }

    // Somewhere empty is taken, and the arrangement carries the change.
    {
        Placement away;
        away.dx = -30.0f;
        away.dy = 20.0f;
        const edit::Attempt land = edit::Evaluate(canvas, style, Overrides{}, Control::Camera, away);
        CHECK(land.ok, "a control moved into the empty middle is fine");
        CHECK(land.overrides.of[size_t(Control::Camera)].dx != 0.0f, "and the change is in the arrangement");
        CHECK(land.deck.Of(Control::Camera).CentreX() != plain.camera.CentreX(), "and laid out where it was put");
    }

    // Nothing leaves the canvas, however far it is pushed.
    {
        Placement far;
        far.dx = 500.0f;
        far.dy = -500.0f;
        const edit::Attempt land = edit::Evaluate(canvas, style, Overrides{}, Control::Start, far);
        const Rect r = land.deck.Of(Control::Start);
        CHECK(r.x >= canvas.left * canvas.uPerCqw - 1e-5f, "pushed right, the control is still in");
        CHECK(r.y >= canvas.top * canvas.vPerCqw - 1e-5f, "and pushed up, it is still in");
        CHECK(land.overrides.of[size_t(Control::Start)].dx < 500.0f, "its placement was clamped first");
    }

    // No control, no attempt.
    {
        const edit::Attempt none = edit::Evaluate(canvas, style, Overrides{}, Control::Count, Placement{});
        CHECK(!none.ok, "there is nothing to evaluate without a control");
        CHECK(none.other == Control::Count, "and nobody to land on");
    }

    // Same: what "changed" means for the Save button.
    {
        Overrides a, b;
        a.of[size_t(Control::Nitro)].dx = 2.0f;
        b.of[size_t(Control::Nitro)].dx = 2.0005f;
        CHECK(edit::Same(a, b, 1e-3f), "a thousandth of a cqw is not a change");
        CHECK(!edit::Same(a, b, 0.0f), "and is, to the bit");
        b.of[size_t(Control::Nitro)].icon = art::Id::Camera;
        CHECK(!edit::Same(a, b, 1.0f), "another picture is always a change");
        CHECK(edit::Same(Overrides{}, Overrides{}, 0.0f), "and two untouched decks are the same");
    }
}

static void Nudges()
{
    Grid grid;  // on, 1 cqw
    CHECK(std::fabs(edit::NudgeStep(grid, 1.0f) - 1.0f) < 1e-6f, "a grid step, at the design's size");
    CHECK(std::fabs(edit::NudgeStep(grid, 1.25f) - 0.8f) < 1e-6f,
        "divided by the deck's factor, so a step is still one line on screen");
    grid.snap = false;
    CHECK(std::fabs(edit::NudgeStep(grid, 1.0f) - edit::kNudgeCqw) < 1e-6f, "free, a quarter of a cqw");
    CHECK(std::fabs(edit::NudgeStep(grid, 2.0f) - edit::kNudgeCqw * 0.5f) < 1e-6f, "still divided by the factor");
    CHECK(std::fabs(edit::NudgeStep(grid, 0.0f) - edit::kNudgeCqw) < 1e-6f, "and a canvas that says nothing is 1:1");
    Grid two;
    two.pitch = 2.0f;
    CHECK(std::fabs(edit::NudgeStep(two, 1.0f) - 2.0f) < 1e-6f, "a coarse grid takes a coarse step");
}

static void Names()
{
    std::set<std::string> seen;
    for (size_t i = 0; i < kControls; i++)
    {
        const Control control = Control(i);
        const char* name = edit::NameOf(control);
        CHECK(name && name[0], "control %zu has a name", i);
        CHECK(seen.insert(name).second, "control %zu's name is its own (%s)", i, name);
        const ControlSpec& spec = SpecOf(control);
        if (spec.key)
            CHECK(std::strcmp(name, spec.key) == 0, "control %zu wears its key's name (%s)", i, name);
    }
    CHECK(std::strcmp(edit::NameOf(Control::Count), "") == 0, "and no control is nameless");
}

static void History()
{
    edit::OverridesHistory history;
    CHECK(!history.CanUndo(), "nothing to undo to begin with");

    Overrides now;
    CHECK(!history.Undo(now), "and undoing says so");
    CHECK(now.of[0].dx == 0.0f, "leaving what it was given");

    // Last in, first out: what a change is undone to is the state before it.
    Overrides first;
    first.of[size_t(Control::Throttle)].dx = 1.0f;
    history.Push(Overrides{});
    now = first;
    Overrides second = first;
    second.of[size_t(Control::Throttle)].dx = 2.0f;
    history.Push(first);
    now = second;
    CHECK(history.Undo(now), "one step back");
    CHECK(now.of[size_t(Control::Throttle)].dx == 1.0f, "to where it was before the last change");
    CHECK(history.Undo(now), "and another");
    CHECK(now.of[size_t(Control::Throttle)].dx == 0.0f, "to the deck as it opened");
    CHECK(!history.CanUndo(), "and that is all of it");

    // Full: the oldest is dropped, never the newest.
    history.Clear();
    for (int i = 0; i < int(edit::kHistoryCap) + 5; i++)
    {
        Overrides step;
        step.of[0].dx = float(i);
        history.Push(step);
    }
    CHECK(history.Size() == edit::kHistoryCap, "the history is bounded (%zu)", history.Size());
    Overrides top;
    CHECK(history.Undo(top), "the newest is still there");
    CHECK(top.of[0].dx == float(edit::kHistoryCap) + 4, "with the last change pushed (%.1f)", top.of[0].dx);
    size_t steps = 1;
    while (history.Undo(top))
        steps++;
    CHECK(steps == edit::kHistoryCap, "and the oldest dropped (%zu steps)", steps);
    CHECK(top.of[0].dx == 5.0f, "the oldest kept is the %zu-th (%.1f)", edit::kHistoryCap, top.of[0].dx);
}

static void Picking()
{
    const Canvas canvas = CanvasFor(kW / kH);
    const Style style;
    const Deck plain = Layout(canvas, style);

    // The order is the drawing's: the steering is drawn last, over the plates.
    std::set<size_t> seen;
    for (size_t rank = 0; rank < kControls; rank++)
        seen.insert(size_t(pick::Order(rank)));
    CHECK(seen.size() == kControls, "every control has a place in the pick order (%zu)", seen.size());
    CHECK(pick::Order(0) == Control::Steer, "the steering is on top");
    CHECK(pick::Order(kControls - 1) == Control::Throttle, "and the throttle at the bottom");

    // A button's own middle is that button.
    {
        const Rect face = plain.handbrake;
        const Control at = pick::PickAt(plain, face.CentreX(), face.CentreY());
        CHECK(at == Control::Handbrake, "the middle of a face button is that button (%zu)", size_t(at));
    }
    // Nothing there is nobody.
    {
        const Control at = pick::PickAt(plain, 0.5f, 0.5f);
        CHECK(at == Control::Count, "the empty middle is no control (%zu)", size_t(at));
    }

    // Two in one place: the one drawn on top takes the tap, the one under it
    // the next tap, and the pair cycles. (The editor refuses to *make* such a
    // layout -- Evaluate above -- but a file can hold one, and then a covered
    // control still has to be reachable.)
    {
        Overrides over;
        over.of[size_t(Control::Nitro)] = Towards(plain, Control::Nitro, plain.steer.CentreX() / canvas.uPerCqw,
            plain.steer.CentreY() / canvas.vPerCqw, canvas);
        const Deck deck = Layout(canvas, style, over);
        CHECK(Overlaps(deck, Control::Nitro), "the nitrous is on the steering here");
        const float u = deck.steer.CentreX(), v = deck.steer.CentreY();
        const Control top = pick::PickAt(deck, u, v);
        CHECK(top == Control::Steer, "the steering is drawn last, so it is picked first (%zu)", size_t(top));
        const Control under = pick::NextUnder(deck, u, v, top);
        CHECK(under == Control::Nitro, "and the next tap takes what it covers (%zu)", size_t(under));
        const Control round = pick::NextUnder(deck, u, v, under);
        CHECK(round == Control::Steer, "then round again (%zu)", size_t(round));
    }

    // Alone: tapping the same place again keeps the control.
    {
        const Rect face = plain.camera;
        const Control first = pick::PickAt(plain, face.CentreX(), face.CentreY());
        CHECK(first == Control::Camera, "the camera takes its own middle");
        CHECK(pick::NextUnder(plain, face.CentreX(), face.CentreY(), first) == first, "and there is nothing under it");
    }
}

int main()
{
    Transactions();
    Nudges();
    Names();
    History();
    Picking();

    printf("%s (%d failures)\n", g_failures ? "FAILED" : "passed", g_failures);
    return g_failures ? 1 : 0;
}
