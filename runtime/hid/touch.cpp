// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See touch.h.
#include <stdafx.h>
#include "touch.h"

#include "input.h"

#include <ui/ui.h>
#include <user/settings.h>
#include <video/picture_rect.h>

#include <cstdlib>

#include <SDL3/SDL.h>

namespace
{
    hid::touch::State s_state;
    hid::touch::Sample s_sample;
    hid::touch::Canvas s_canvas;
    hid::touch::Deck s_deck;
    bool s_active = false;
    int s_touchDevices = 0;

    // NFSMW_TOUCH_MOUSE: the mouse as one finger, so the deck can be used and
    // photographed on a machine without a touchscreen. The left button is the
    // finger; nothing else changes.
    bool s_mouseFinger = false;
    bool s_mouseDown = false;
    constexpr uint64_t kMouseId = 0xFFFFFFFFull;

    void CountTouchDevices()
    {
        int n = 0;
        if (SDL_TouchID* devices = SDL_GetTouchDevices(&n))
        {
            s_touchDevices = 0;
            // A trackpad's indirect touch (a laptop's, the Steam Deck's) is
            // not a finger on the screen the deck is meant for.
            for (int i = 0; i < n; i++)
                if (SDL_GetTouchDeviceType(devices[i]) == SDL_TOUCH_DEVICE_DIRECT)
                    s_touchDevices++;
            SDL_free(devices);
        }
    }

    // A finger's identity: its device's id and SDL's, so two screens can't
    // hand the same number two fingers.
    uint64_t IdOf(const SDL_TouchFingerEvent& f)
    {
        return (uint64_t(uint32_t(f.touchID)) << 32) | uint64_t(uint32_t(f.fingerID));
    }

    // Where a finger is, in the canvas's fractions: the picture's own, so a
    // press lands on the control drawn under it whatever surrounds the
    // picture (letterbox bars, a screen of another shape).
    bool ToCanvas(const video::PictureRect& picture, float x, float y, float& u, float& v)
    {
        if (!picture.Valid())
            return false;
        u = (x - picture.x) / picture.w;
        v = (y - picture.y) / picture.h;
        return true;
    }

    // The deck's canvas: the picture's shape, with the design's margins or the
    // screen's safe area where that reaches further in.
    hid::touch::Canvas CanvasFromWindow()
    {
        const video::PictureRect picture = video::GetPictureRect();
        hid::touch::Canvas canvas = hid::touch::CanvasFor(picture.aspect);
        SDL_Window* window = ui::GetWindow();
        if (!window)
            return canvas;
        int width = 0, height = 0;
        SDL_GetWindowSize(window, &width, &height);
        SDL_Rect safe{};
        if (width <= 0 || height <= 0 || !SDL_GetWindowSafeArea(window, &safe) || safe.w <= 0 || safe.h <= 0)
            return canvas;
        // The safe area is the window's; the margins are the picture's, so what
        // lies outside the picture doesn't count.
        const float pictureW = picture.w * float(width), pictureH = picture.h * float(height);
        const float pictureX = picture.x * float(width), pictureY = picture.y * float(height);
        return hid::touch::InsetsFor(canvas, pictureW, std::max(0.0f, float(safe.x) - pictureX),
            std::max(0.0f, float(safe.y) - pictureY), std::max(0.0f, pictureX + pictureW - float(safe.x + safe.w)),
            std::max(0.0f, pictureY + pictureH - float(safe.y + safe.h)));
    }

    // The mouse as one finger (NFSMW_TOUCH_MOUSE), for a machine without a
    // touchscreen.
    void MouseFinger(const SDL_Event& e)
    {
        if (!s_mouseFinger || !s_active)
            return;
        switch (e.type)
        {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (e.button.button != SDL_BUTTON_LEFT || e.button.which == SDL_TOUCH_MOUSEID)
                return;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.button != SDL_BUTTON_LEFT || !s_mouseDown)
                return;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (!s_mouseDown || e.motion.which == SDL_TOUCH_MOUSEID)
                return;
            break;
        default:
            return;
        }
        const float x = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.x : e.button.x;
        const float y = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.y : e.button.y;
        int width = 0, height = 0;
        if (SDL_Window* window = ui::GetWindow())
            SDL_GetWindowSize(window, &width, &height);
        if (width <= 0 || height <= 0)
            return;
        float u = 0, v = 0;
        if (!ToCanvas(video::GetPictureRect(), x / float(width), y / float(height), u, v))
            return;
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            hid::touch::Control control = hid::touch::Control::Count;
            s_mouseDown = hid::touch::Down(s_state, s_deck, kMouseId, u, v, control);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            s_mouseDown = false;
            hid::touch::Up(s_state, kMouseId);
        }
        else
            hid::touch::Move(s_state, s_deck, kMouseId, u, v);
    }
}

namespace hid::touch
{
    void Initialize()
    {
        s_mouseFinger = std::getenv("NFSMW_TOUCH_MOUSE") != nullptr;
        CountTouchDevices();
        if (s_touchDevices)
            fprintf(stderr, "[touch] %d touchscreen%s\n", s_touchDevices, s_touchDevices == 1 ? "" : "s");
        if (s_mouseFinger)
            fprintf(stderr, "[touch] NFSMW_TOUCH_MOUSE: the left mouse button is one finger\n");
    }

    bool Active()
    {
        // A menu of ours owns the input: the game sees an idle pad, and the
        // deck is not in the way of what the menu shows.
        if (ui::CapturingInput())
            return false;
        switch (settings::TouchControlsMode(settings::GetInt(settings::Id::TouchControls)))
        {
        case settings::TouchControlsMode::Show:
            return true;
        case settings::TouchControlsMode::Hide:
            return false;
        case settings::TouchControlsMode::Auto:
            break;
        }
        // Auto: a touchscreen, and no controller to play with instead (a Steam
        // Deck has both, and would rather have its own controls).
        return s_touchDevices > 0 && !hid::PlayerOneGamepad();
    }

    void Update()
    {
        const bool wasActive = s_active;
        s_active = Active();
        if (!s_active)
        {
            // Nothing stays held: a menu opened, the deck went off, or there is
            // nothing to lay it out on yet.
            if (wasActive)
                Reset();
            s_deck = {};
            return;
        }
        if (!video::GetPictureRect().Valid())
        {
            // No picture yet (the game hasn't presented): nothing to sit on.
            Reset();
            s_deck = {};
            return;
        }
        s_canvas = CanvasFromWindow();
        // The steering panel sits where its thumb last took it (or in the
        // handoff's corner until then), so it comes to the thumb.
        s_deck = DeckFor(s_canvas, s_state);
        s_sample = SampleOf(s_state);
    }

    void EndFrame()
    {
        if (s_active)
            Advance(s_state);
    }

    const Sample& Snapshot()
    {
        return s_sample;
    }

    const Canvas& CanvasNow()
    {
        return s_canvas;
    }

    const Deck& DeckNow()
    {
        return s_deck;
    }

    void Reset()
    {
        hid::touch::Reset(s_state);
        s_sample = Sample{};
        s_mouseDown = false;
    }

    void HandleEvent(const SDL_Event& e)
    {
        switch (e.type)
        {
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            // A finger held while the window lost the focus may never lift.
            Reset();
            return;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            break;
        default:
            MouseFinger(e);
            return;
        }
        if (!s_active)
            return;
        const SDL_TouchFingerEvent& f = e.tfinger;
        // SDL makes the first finger the mouse (SDL_MOUSE_TOUCHID), a pen is
        // not a finger, and an indirect touch is a trackpad's: ui.cpp's own
        // filter, so the deck and the three-finger tap see the same fingers.
        if (f.touchID == SDL_MOUSE_TOUCHID || f.touchID == SDL_PEN_TOUCHID ||
            SDL_GetTouchDeviceType(f.touchID) != SDL_TOUCH_DEVICE_DIRECT)
            return;
        float u = 0, v = 0;
        if (!ToCanvas(video::GetPictureRect(), f.x, f.y, u, v))
            return;
        const uint64_t id = IdOf(f);
        switch (e.type)
        {
        case SDL_EVENT_FINGER_DOWN:
        {
            // A touchscreen that appeared after the launch (SDL has no event
            // for one): counted now, so Auto finds the deck up from the next
            // frame.
            if (s_touchDevices == 0)
                CountTouchDevices();
            Control control = Control::Count;
            Down(s_state, s_deck, id, u, v, control);
            break;
        }
        case SDL_EVENT_FINGER_MOTION:
            Move(s_state, s_deck, id, u, v);
            break;
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            Up(s_state, id);
            break;
        default:
            break;
        }
    }
}
