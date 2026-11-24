// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// What the player changed about the deck: where each control sits relative to
// where the layout would put it, how big it is, and which picture it wears --
// plus the editor's own two preferences (whether dragging snaps, and how
// coarse the grid is).
//
// A placement is an **offset**, not a rectangle. The deck is laid out afresh
// every frame from the canvas, which carries the picture's shape, the screen's
// safe area and what a cqw is worth in points (hid/touch_layout.h), so a
// rectangle saved on a phone would be wrong on a tablet, and one saved before
// a rotation wrong after it. An offset in cqw keeps its meaning; the default,
// {0, 0, 1, Count}, is exactly where the layout would have put the control, so
// an untouched deck is the designed one to the pixel.
//
// Header only and free of SDL and ImGui: user/touch_deck_file.cpp reads and
// writes it, ui/touch_deck_editor.cpp edits it, hid/touch_layout.h applies it,
// and tests/touch_deck_test.cpp gets all three with -I runtime.
#pragma once
#include <cstddef>
#include <cstdint>

#include <hid/touch_art.h>
#include <hid/touch_control.h>

namespace hid::touch
{
    // The smallest and largest a control can be made: smaller is no longer a
    // thumb's, larger would swallow its neighbours.
    constexpr float kMinPlacement = 0.5f, kMaxPlacement = 2.0f;

    struct Placement
    {
        // How far from the layout's own slot, in cqw -- times the deck's size
        // factor, so a control moved on a phone moves with it when the buttons
        // grow (hid/touch_layout.h's ApplyOverrides).
        float dx = 0.0f, dy = 0.0f;
        // How much bigger or smaller than its size class's: the slot is scaled
        // about its centre before it is moved.
        float scale = 1.0f;
        // The picture it wears, or art::Id::Count for the one its spec names.
        art::Id icon = art::Id::Count;

        bool Default() const { return dx == 0.0f && dy == 0.0f && scale == 1.0f && icon == art::Id::Count; }
    };

    struct Overrides
    {
        Placement of[kControls] = {};

        bool Any() const
        {
            for (const Placement& placement : of)
            {
                if (!placement.Default())
                    return true;
            }
            return false;
        }

        void Reset() { *this = Overrides{}; }
    };

    // What touch-deck.toml calls each control, in Control's order (the tests
    // hold this to the enum). A control's *key* name (hid/touch_spec.h's "RT",
    // "A") is what a player sees on the button; this is what the file says.
    inline constexpr const char* kControlKey[kControls] = {
        "steer",
        "throttle", "brake", "handbrake", "nitro", "speedbreaker",
        "rear-view", "quick-rear-view", "camera",
        "shift-up", "shift-down",
        "dpad-up", "dpad-down", "dpad-left", "dpad-right",
        "back", "start",
    };

    // The editor's snapping: whether dragging follows the lines around it, and
    // how far apart the grid's lines are (in cqw: 0.5, 1 or 2).
    struct Grid
    {
        bool snap = true;
        float pitch = 1.0f;
    };
}
