// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// What a touch control is, before anything decides where it goes: the rectangle
// shapes the deck is built from and the seventeen controls the game's pad has.
//
// Header only and free of SDL and ImGui (a rectangle is four floats, a control
// is an enum), so tests/touch_deck_test.cpp can include the whole deck with
// nothing but -I runtime, and hid/ never grows a graphics dependency.
//
// Split out of touch_layout.h (which now holds where the controls sit) so that
// hid/touch_spec.h -- what each control *is*: its picture, its key, its size
// class -- can name controls without depending on the layout.
#pragma once
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

    // A point of the canvas.
    struct Point
    {
        float u = 0.0f, v = 0.0f;
    };

    // The 17 controls the deck keeps mapped (design/keymap.json): every Xbox
    // 360 control the game has, none merged or renamed. Steer is the analog
    // stick, base and stick both. ShiftUp and ShiftDown are the two halves of
    // RS, which is one control with two directions.
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
}
