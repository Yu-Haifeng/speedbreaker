// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Which finger presses what on the touch deck (hid/touch_layout.h), and what
// the game is told: the design's rules, and the pad they amount to.
//
//   - A press starts where a finger lands and nowhere else -- but for one
//     roll the thumb makes: off the throttle onto the nitrous or the
//     Speedbreaker orb above it, which takes that orb too while the pedal
//     stays down (a thumb held on a pedal cannot reach them any other way).
//     Every other control is untouched by a slide, and a finger that slides
//     off a control keeps it until it lifts (the design's pointer capture),
//     so a throttle held by a thumb that drifts out of its pedal still pulls.
//   - A control held by several fingers stays pressed until the last of them
//     lifts.
//   - Steering is one finger's: the first that lands on the panel has it, and
//     the panel's centre comes to where it landed (as close as the canvas's
//     margins let it), so the wheel starts straight ahead under the thumb.
//     From there the knob follows the thumb's place across the track (relative
//     to the landing and linear: the game has its own steering curve), and it
//     returns to centre when that finger lifts, the panel with it: back to the
//     handoff's corner. Another finger on the panel presses nothing.
//   - A press is reported for at least kPressUpdates updates: input.cpp hands
//     the game one pad per presented frame, and the guest's own 60 Hz tick can
//     fall between two of them, so a tap shorter than a frame would otherwise
//     be a press the game never sees.
//
// Header only and SDL-free (a finger is a plain number), for
// tests/touch_deck_test.cpp: hid/touch.cpp feeds it SDL's fingers, and
// Sample() goes into the pad in input.cpp; ui/touch_overlay.cpp draws it.
#pragma once
#include <cstddef>
#include <cstdint>

#include <hid/touch_layout.h>

namespace hid::touch
{
    // XInput's button bits, as the game reads them (hid/key_map.h's, literals
    // so this header needs no kernel header).
    namespace button
    {
        constexpr uint16_t DpadUp = 0x0001, DpadDown = 0x0002, DpadLeft = 0x0004, DpadRight = 0x0008;
        constexpr uint16_t Start = 0x0010, Back = 0x0020;
        constexpr uint16_t LeftShoulder = 0x0100, RightShoulder = 0x0200;
        constexpr uint16_t A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000;
    }

    // The fewest updates a press is reported for (see above).
    constexpr uint8_t kPressUpdates = 2;
    // Fingers tracked at once: a phone takes ten, and the rest of the screen
    // (the game's own HUD) isn't ours.
    constexpr size_t kMaxFingers = 12;

    // What the deck is doing: the fingers on it, and the presses still being
    // reported.
    struct State
    {
        struct Finger
        {
            uint64_t id = 0;  // 0: free
            Control control = Control::Count;  // what the finger landed on
            Control rolled = Control::Count;   // and what it rolled onto from the throttle
        };
        Finger finger[kMaxFingers];
        uint64_t steerId = 0;  // the finger steering (0: none)
        float steer = 0.0f;    // -1 (left lock) to +1
        // Where that finger landed: the drag is read against it, and while it
        // is down it is where the panel's centre goes (DeckFor). Up clears it,
        // so the panel returns to the handoff's corner.
        Point steerOrigin;
        bool steerAnchored = false;
        uint8_t hold[kControls] = {};  // updates each control is still reported pressed for

        uint8_t Fingers(Control c) const
        {
            uint8_t n = 0;
            for (const Finger& f : finger)
                if (f.id != 0 && (f.control == c || f.rolled == c))
                    n++;
            return n;
        }
        bool Pressed(Control c) const { return Fingers(c) || hold[size_t(c)]; }
    };

    // A finger landing on the deck at (u, v), a point of the canvas. True: it
    // presses `control`; false: it presses nothing (it landed on the game's
    // picture, or on the steering panel another finger already has), and so
    // never will.
    bool Down(State& state, const Deck& deck, uint64_t finger, float u, float v, Control& control);
    // A finger moving: only the steering one changes anything (a button's
    // press doesn't depend on where its finger is).
    void Move(State& state, const Deck& deck, uint64_t finger, float u, float v);
    // A finger lifting.
    void Up(State& state, uint64_t finger);
    // Once per presented frame, after the frame's SampleOf: ages the presses,
    // so one that started and ended within the frame is reported for
    // kPressUpdates of them.
    void Advance(State& state);
    // Every finger let go without lifting (a menu opened, the app went to the
    // background): nothing stays held.
    void Reset(State& state);

    // What the game is told, and what the deck shows.
    struct Sample
    {
        uint16_t buttons = 0;
        float leftTrigger = 0.0f, rightTrigger = 0.0f;  // 0 or 1: the deck's pedals are buttons
        float steer = 0.0f;
        bool steerSet = false;  // the deck is steering: the axis comes from here
        bool shiftUp = false, shiftDown = false;  // the right stick's up and down, as the keyboard's keys
        uint32_t pressed = 0;                     // a bit per Control, for drawing

        bool Pressed(Control c) const { return (pressed >> size_t(c)) & 1u; }
    };

    inline bool Down(State& state, const Deck& deck, uint64_t id, float u, float v, Control& control)
    {
        control = HitTest(deck, u, v);
        if (id == 0 || control == Control::Count)
            return false;
        State::Finger* slot = nullptr;
        for (State::Finger& f : state.finger)
        {
            if (f.id == id)
                return false;  // (SDL never repeats one, but a stale id must not press twice)
            if (f.id == 0 && !slot)
                slot = &f;
        }
        if (!slot)
            return false;
        if (control == Control::Steer)
        {
            // One finger steers; another on the panel presses nothing. The
            // panel comes to where the finger landed, and the drag is read
            // from there: straight ahead under the thumb, wherever it is.
            if (state.steerId != 0)
                return false;
            state.steerId = id;
            state.steerOrigin = { u, v };
            state.steerAnchored = true;
            state.steer = 0.0f;
        }
        *slot = { id, control, Control::Count };
        state.hold[size_t(control)] = kPressUpdates;
        return true;
    }

    // The two the game wants held with the throttle: nitrous and the
    // Speedbreaker. The cluster puts them a thumb's roll above the pedal.
    constexpr bool Rollable(Control c)
    {
        return c == Control::Nitro || c == Control::Speedbreaker;
    }

    inline void Move(State& state, const Deck& deck, uint64_t id, float u, float v)
    {
        if (id == 0)
            return;
        if (id == state.steerId)
        {
            state.steer = SteerFromDrag(deck, u, state.steerOrigin.u);
            return;
        }
        for (State::Finger& f : state.finger)
        {
            if (f.id != id || f.control != Control::Throttle)
                continue;
            // The throttle's thumb rolling onto the nitrous or the
            // Speedbreaker: both are held, and the orb lets go as the thumb
            // comes back down (or anywhere else: only those two roll).
            const Control hit = HitTest(deck, u, v);
            const Control rolled = Rollable(hit) ? hit : Control::Count;
            if (rolled == f.rolled)
                return;
            f.rolled = rolled;
            if (rolled != Control::Count)
                state.hold[size_t(rolled)] = kPressUpdates;
            return;
        }
        (void)v;
    }

    inline void Up(State& state, uint64_t id)
    {
        if (id == 0)
            return;
        for (State::Finger& f : state.finger)
        {
            if (f.id != id)
                continue;
            if (f.control == Control::Steer && state.steerId == id)
            {
                state.steerId = 0;
                state.steer = 0.0f;
                // The panel goes back to the handoff's corner, so it is where
                // the next thumb expects it.
                state.steerOrigin = {};
                state.steerAnchored = false;
            }
            f = State::Finger{};
            return;
        }
    }

    inline void Advance(State& state)
    {
        for (size_t i = 0; i < kControls; i++)
        {
            if (state.Fingers(Control(i)))
                state.hold[i] = kPressUpdates;
            else if (state.hold[i])
                state.hold[i]--;
        }
    }

    inline void Reset(State& state)
    {
        state = State{};
    }

    inline Sample SampleOf(const State& state)
    {
        Sample s;
        const auto pressed = [&state](Control c) { return state.Pressed(c); };
        if (pressed(Control::DpadUp)) s.buttons |= button::DpadUp;
        if (pressed(Control::DpadDown)) s.buttons |= button::DpadDown;
        if (pressed(Control::DpadLeft)) s.buttons |= button::DpadLeft;
        if (pressed(Control::DpadRight)) s.buttons |= button::DpadRight;
        if (pressed(Control::Start)) s.buttons |= button::Start;
        if (pressed(Control::Back)) s.buttons |= button::Back;
        if (pressed(Control::QuickRearView)) s.buttons |= button::LeftShoulder;
        if (pressed(Control::Camera)) s.buttons |= button::RightShoulder;
        if (pressed(Control::Handbrake)) s.buttons |= button::A;
        if (pressed(Control::Nitro)) s.buttons |= button::B;
        if (pressed(Control::Speedbreaker)) s.buttons |= button::X;
        if (pressed(Control::RearView)) s.buttons |= button::Y;
        if (pressed(Control::Throttle)) s.rightTrigger = 1.0f;
        if (pressed(Control::Brake)) s.leftTrigger = 1.0f;
        s.steerSet = state.steerId != 0;
        s.steer = state.steer;
        s.shiftUp = pressed(Control::ShiftUp);
        s.shiftDown = pressed(Control::ShiftDown);
        for (size_t i = 0; i < kControls; i++)
            if (state.Pressed(Control(i)))
                s.pressed |= uint32_t(1) << i;
        return s;
    }

    // The deck for this canvas and this state: the steering panel centred
    // where the thumb is holding it (as close as the canvas's margins let it),
    // or in the handoff's corner when no thumb has it.
    inline Deck DeckFor(const Canvas& canvas, const State& state)
    {
        return state.steerAnchored ? Layout(canvas, state.steerOrigin) : Layout(canvas);
    }
}
