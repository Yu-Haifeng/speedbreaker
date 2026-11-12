// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The touch controls (the design handoff's NFSMW Touch Control Deck): the
// screen's fingers as the pad the game already reads, and what the overlay
// draws. Nothing else in the game changes -- a touch is not a new input
// source to the game, it is controller 0's buttons, triggers and left stick.
//
//   - The deck lays itself out on the game's picture (video/picture_rect.h),
//     in the design's cqw (hid/touch_layout.h), so its controls keep their
//     place on the image whatever the screen's shape. The steering panel is
//     the exception: while held, its centre is where the thumb took it (as
//     close as the canvas's margins let it), and it goes back to its corner
//     when the thumb lifts.
//   - Where a finger presses is decided on the way down and loosened on the
//     way up, but for the thumb that rolls off the throttle onto the nitrous
//     or the Speedbreaker above it, which takes that orb too: the rules are
//     hid/touch_input.h's.
//   - On: Settings > Input > Touch Controls. Auto (the default) shows the
//     deck where there is a touchscreen and no controller to play with
//     instead, so an iPhone or iPad alone gets it, a Steam Deck (its own
//     touchscreen, its own controller) and a paired pad don't.
//   - A menu of ours (ui::CapturingInput) hides it and lets nothing stay
//     held: the game sees an idle pad under the menu, as before.
//   - Fingers still reach ui::ProcessEvent as they always did: a three-finger
//     tap opens the settings from anywhere, the deck's controls included --
//     on a phone without a controller that is the only way in.
//
// The deck is main-thread only: SDL's fingers arrive in the presenter's event
// poll, and it is drawn in the same frame.
#pragma once
#include <cstdint>

#include <SDL3/SDL_events.h>

#include <hid/touch_input.h>
#include <hid/touch_layout.h>

namespace hid::touch
{
    // Once, after SDL's video (presenter's video::Initialize).
    void Initialize();
    // Every SDL event, from hid::HandleEvent (the presenter's poll). Takes the
    // fingers that land on the deck; everything else passes through.
    void HandleEvent(const SDL_Event& event);
    // Once per presented frame, before the pad is filled: is the deck on, and
    // where is it? Also lets go of every finger when it turns off.
    void Update();
    // Once per presented frame, after the pad was filled: ages the presses.
    void EndFrame();
    // The deck is on: a touchscreen has it, a menu of ours doesn't.
    bool Active();
    // What the game is told (and what the overlay draws). Valid once Update()
    // has run; without Active() it is empty.
    const Sample& Snapshot();
    // The canvas and the deck last laid out, for the overlay to draw: the same
    // rectangles the fingers were tested against.
    const Canvas& CanvasNow();
    const Deck& DeckNow();
    // Every finger let go without lifting (the app went to the background).
    void Reset();
}
