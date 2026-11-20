// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The device's own haptics: what the touch controls (hid/touch.h) play when
// there is no controller to rumble. iOS is the only platform with them, on
// Core Haptics (platform/ios_haptics.mm); everywhere else these are empty, so
// callers need no #if and tests link no platform code.
//
// What to play is decided in hid/haptic_feedback.h (SDL-free and unit
// tested): the game's two rumble motors as one pattern, a button's tap, the
// steering's detents. This header only carries it to the device.
//
// Main thread only (the frame loop's): Core Haptics wants it, and the engine
// is started and stopped from there.
#pragma once

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace platform::haptics
{
    // One haptic: how strong, and how sharp (0 dull and low, 1 crisp and
    // high). The touch deck's game rumble and its taps both come as these.
    struct Pattern
    {
        float intensity = 0.0f;  // 0..1
        float sharpness = 0.0f;  // 0..1
    };

#if defined(__APPLE__) && TARGET_OS_IOS
    // Does this device have a Taptic Engine? False on the simulator and on
    // iPads without one; everything below is then a no-op.
    bool Available();
    // Once the deck is up: create and start the engine (idempotent). The
    // engine stops itself only through Stop() and Shutdown().
    void Start();
    // The deck went away, or the app is being suspended: silence now, and
    // stop the engine. Start() makes it ready again.
    void Stop();
    // At exit: Stop() and let go of the Core Haptics objects.
    void Shutdown();
    // The game's rumble, once per presented frame: a continuous pattern,
    // updated in place. A silent pattern lets it fade out (the layer stops
    // the player shortly after) rather than cutting it off abruptly.
    void Rumble(const Pattern& pattern);
    // One short tap: a button pressed, or a steering detent.
    void Tap(const Pattern& pattern);
#else
    inline bool Available() { return false; }
    inline void Start() {}
    inline void Stop() {}
    inline void Shutdown() {}
    inline void Rumble(const Pattern&) {}
    inline void Tap(const Pattern&) {}
#endif
}
