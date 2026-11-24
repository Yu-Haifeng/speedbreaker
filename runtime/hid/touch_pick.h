// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Which control a point of the deck is on, and which one after that: the
// editor's picking. Two things differ from the deck's own hit test
// (hid/touch_layout.h's HitTest), both because this is arranging rather than
// playing:
//
//   - The control's rectangle counts, not its picture's outline. In the editor
//     a button's box is what you take hold of, and a picture's empty corners
//     are still the button's.
//   - A tap that lands where two controls overlap picks the one drawn on top,
//     and a second tap in the same place picks the one under it, so a control
//     that a neighbour covers can still be reached.
//
// Header only and free of SDL and ImGui, for tests/touch_edit_test.cpp.
#pragma once
#include <cstddef>

#include <hid/touch_control.h>
#include <hid/touch_layout.h>

namespace hid::touch::pick
{
    // The order the deck is drawn in, first on top: the spec table's controls
    // in reverse (they are drawn in order), and the steering first of all
    // (touch_overlay.cpp draws its base and stick last, over the plates).
    inline constexpr Control Order(size_t rank)
    {
        return rank == 0 ? Control::Steer : Control(size_t(Control::Count) - rank);
    }

    inline bool Contains(const Rect& r, float u, float v)
    {
        return r.w > 0.0f && r.h > 0.0f && u >= r.x && u <= r.Right() && v >= r.y && v <= r.Bottom();
    }

    // The topmost control at this point of the canvas, Count for none.
    inline Control PickAt(const Deck& deck, float u, float v)
    {
        for (size_t rank = 0; rank < kControls; rank++)
        {
            const Control control = Order(rank);
            if (Contains(deck.Of(control), u, v))
                return control;
        }
        return Control::Count;
    }

    // The next control under the same point after `after`, wrapping round; at
    // the end, `after` itself (there is nothing else there). `after` not being
    // at this point at all starts over from the top.
    inline Control NextUnder(const Deck& deck, float u, float v, Control after)
    {
        if (after == Control::Count)
            return PickAt(deck, u, v);
        size_t rank = kControls;
        for (size_t r = 0; r < kControls; r++)
        {
            if (Order(r) == after)
            {
                rank = r;
                break;
            }
        }
        if (rank == kControls || !Contains(deck.Of(after), u, v))
            return PickAt(deck, u, v);
        for (size_t step = 1; step < kControls; step++)
        {
            const Control control = Order((rank + step) % kControls);
            if (Contains(deck.Of(control), u, v))
                return control;
        }
        return after;
    }
}
