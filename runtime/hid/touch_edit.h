// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// One attempt at changing the deck's layout, and the small pieces the editor
// needs around it: whether the change would put one control on another, who it
// would land on, a name a player understands, the step a nudge takes, and the
// history that makes a change undoable.
//
// The deck's own layout rules stay in hid/touch_layout.h; this is the editor's
// arithmetic on top, kept here (and free of SDL and ImGui) so
// tests/touch_edit_test.cpp can drive it with nothing but -I runtime.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

#include <hid/touch_deck.h>
#include <hid/touch_layout.h>
#include <hid/touch_spec.h>

namespace hid::touch::edit
{
    // How far a nudge moves a control when the grid is off, in cqw of the
    // picture: a quarter of a percent of its width.
    inline constexpr float kNudgeCqw = 0.25f;

    // What the editor would make of one changed placement, before it is taken.
    struct Attempt
    {
        bool ok = false;                // (no other control crossed)
        Control control = Control::Count;
        Control other = Control::Count;  // the control it would land on; Count: none
        Overrides overrides;             // the arrangement, with the candidate in it, inside the canvas
        Deck deck;                       // that arrangement laid out (for the red outline, and for Overlaps)
    };

    // `candidate` for `control`, on top of `base`: clamped into the canvas, laid
    // out, and tested against the other sixteen. One call, one layout of the
    // result, so the editor's every-frame drag costs what it did before.
    inline Attempt Evaluate(const Canvas& canvas, const Style& style, const Overrides& base, Control control,
        const Placement& candidate)
    {
        Attempt attempt;
        attempt.control = control;
        if (control >= Control::Count)
            return attempt;
        attempt.overrides = base;
        attempt.overrides.of[size_t(control)] = candidate;
        // (The design's own slot is what a placement is measured from: the
        // layout's formula, not where the control happens to be now.)
        const Rect home = Layout(canvas, style).Of(control);
        ClampPlacement(home, canvas, style, attempt.overrides.of[size_t(control)]);
        attempt.deck = Layout(canvas, style, attempt.overrides);
        attempt.other = OverlapWith(attempt.deck, control);
        attempt.ok = attempt.other == Control::Count;
        return attempt;
    }

    // Two arrangements the same, to within `tolerance` cqw: what "changed" means
    // (a hair of float drift is not a change the player made).
    inline bool Same(const Overrides& a, const Overrides& b, float tolerance = 0.0f)
    {
        for (size_t i = 0; i < kControls; i++)
        {
            const Placement& x = a.of[i];
            const Placement& y = b.of[i];
            if (x.icon != y.icon)
                return false;
            if (std::fabs(x.dx - y.dx) > tolerance || std::fabs(x.dy - y.dy) > tolerance ||
                std::fabs(x.scale - y.scale) > tolerance)
                return false;
        }
        return true;
    }

    // What a control is called in the editor: the key it stands for where it
    // has one (RT, A, LB, START), and a name of its own where it does not (a
    // gear, a D-pad cell). (touch-deck.toml's kebab names are for the file.)
    inline constexpr const char* kDisplayName[kControls] = {
        "Steering",
        "Throttle", "Brake", "Handbrake", "Nitrous", "Speedbreaker",
        "Rear view", "Quick rear view", "Camera",
        "Gear up", "Gear down",
        "D-pad up", "D-pad down", "D-pad left", "D-pad right",
        "Back", "Start",
    };

    inline const char* NameOf(Control control)
    {
        const size_t index = size_t(control);
        if (index >= kControls)
            return "";
        const ControlSpec& spec = SpecOf(control);
        return spec.key ? spec.key : kDisplayName[index];
    }

    // "RT would overlap Y": what the editor says when a change is refused.
    inline std::string RefusalText(const Attempt& attempt)
    {
        if (attempt.ok || attempt.other == Control::Count || attempt.control == Control::Count)
            return std::string();
        char text[96];
        std::snprintf(text, sizeof text, "%s would overlap %s", NameOf(attempt.control), NameOf(attempt.other));
        return text;
    }

    // One nudge, in cqw of the *placement* (which the layout multiplies by the
    // deck's factor): a grid step when the grid is on, a quarter of a cqw when
    // it is not -- so a step lands on the line that is drawn whatever size the
    // deck is.
    inline float NudgeStep(const Grid& grid, float factor)
    {
        const float step = grid.snap ? grid.pitch : kNudgeCqw;
        return factor > 0.0f ? step / factor : step;
    }

    // The editor's undo: whole arrangements, last in first out, oldest dropped
    // when it is full. (An Overrides is 17 placements -- a few hundred bytes --
    // so a snapshot costs less than the picture it moves.)
    template <class T, size_t N>
    struct History
    {
        static_assert(N > 0);

        void Push(const T& before)
        {
            items_[next_] = before;
            next_ = (next_ + 1) % N;
            count_ = std::min(count_ + 1, N);
        }

        bool Undo(T& now)
        {
            if (count_ == 0)
                return false;
            count_--;
            next_ = (next_ + N - 1) % N;
            now = items_[next_];
            return true;
        }

        bool CanUndo() const { return count_ != 0; }
        size_t Size() const { return count_; }
        void Clear()
        {
            count_ = 0;
            next_ = 0;
        }

    private:
        T items_[N]{};
        size_t next_ = 0, count_ = 0;
    };

    inline constexpr size_t kHistoryCap = 32;
    using OverridesHistory = History<Overrides, kHistoryCap>;
}
