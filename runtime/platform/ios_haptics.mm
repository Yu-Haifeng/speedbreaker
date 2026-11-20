// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See haptics.h.
//
// iOS's own haptics for the touch controls, on Core Haptics. The patterns
// and when to play them are hid/haptic_feedback.h's; this file is only the
// device: one engine, one looping continuous player for the game's rumble,
// and a few transient players for the deck's taps and detents.
//
//   - The engine is built with playsHapticsOnly (no audio events), and this
//     file never touches AVAudioSession: SDL owns the shared session for the
//     game's sound, and only changing its category or activation could hurt
//     it. A haptic-only engine leaves it alone.
//   - The rumble player is a one-second continuous event with its base
//     intensity at full, looped; the game's motor strengths arrive as
//     dynamic intensity (a multiplier, 0..1) and sharpness (an offset from
//     the base's 0.5, -1..1) parameters. It starts muted, takes the first
//     parameters, then unmutes: no full-strength burst on the first frame.
//   - The engine is started and stopped by hid/input.cpp's gate, not by
//     auto-shutdown: a prewarmed engine is what makes the first tap
//     immediate. On a menu or a suspension it stops; a later Start() starts
//     it again and rebuilds the players (a system reset invalidates them).
//
// Manual reference counting: the runtime is built without -fobjc-arc.
#include "haptics.h"

#import <CoreHaptics/CoreHaptics.h>
#import <Foundation/Foundation.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>

namespace
{
    // Below this the game's rumble is silence (hid/haptic_feedback.h's
    // kMotorFloor already leaves it at zero; this is the phone's own floor).
    constexpr float kSilence = 0.02f;
    // How long a silent rumble is left to fade before its player is stopped:
    // the game's rumble often drops out for a frame between knocks.
    constexpr double kLoopStopHold = 0.15;
    // The most distinct transient patterns kept (the deck's three, and one
    // spare); each is created once and started again for every tap.
    constexpr size_t kTapCache = 4;

    // The engine, its rumble player, and whether the engine is running.
    // s_engine and the players are main-thread only; the two flags are read
    // by the engine's handlers (which arrive on other threads) and are
    // therefore atomics, like the note in haptics.h.
    CHHapticEngine* s_engine = nil;
    id<CHHapticAdvancedPatternPlayer> s_rumble = nil;
    bool s_loopPlaying = false;
    bool s_available = false;
    bool s_checked = false;
    std::atomic<bool> s_running{ false };
    std::atomic<bool> s_reset{ false };

    // The transient players, by the pattern that made them.
    struct TapPlayer
    {
        platform::haptics::Pattern pattern;
        id<CHHapticPatternPlayer> player;
    };
    TapPlayer s_taps[kTapCache];
    size_t s_tapCount = 0;

    // The rumble player's last parameters (to send only real changes), when
    // it last went silent, and errors already reported (a device that refuses
    // haptics should not fill the log a frame at a time).
    float s_lastIntensity = -1.0f, s_lastSharpness = -1.0f;
    double s_silentAt = 0.0;
    // A device that refuses the engine is not asked again every frame: one
    // try a second, so a transient failure (an audio session busy at launch)
    // recovers without a failing allocation per frame.
    double s_retryAt = 0.0;
    bool s_loggedStartError = false, s_loggedPlayerError = false, s_loggedParameterError = false;

    double Now()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void LogError(const char* what, NSError* error, bool& logged)
    {
        if (logged)
            return;
        logged = true;
        fprintf(stderr, "[haptics] %s: %s\n", what, error ? error.localizedDescription.UTF8String : "unknown error");
    }

    void ReleaseTaps()
    {
        for (size_t i = 0; i < s_tapCount; i++)
            [s_taps[i].player release];
        s_tapCount = 0;
    }

    // A one-second continuous event, looped. Its intensity is full and its
    // sharpness 0.5; the dynamic parameters below scale and offset those.
    void BuildRumble()
    {
        if (s_rumble)
        {
            if (s_loopPlaying)
                [s_rumble stopAtTime:CHHapticTimeImmediate error:nil];
            [s_rumble release];
            s_rumble = nil;
        }
        s_loopPlaying = false;
        s_lastIntensity = s_lastSharpness = -1.0f;

        @autoreleasepool
        {
            NSError* error = nil;
            CHHapticEventParameter* intensity = [[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity
                                                                                               value:1.0f] autorelease];
            CHHapticEventParameter* sharpness = [[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness
                                                                                               value:0.5f] autorelease];
            CHHapticEvent* event = [[[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
                                                                  parameters:@[ intensity, sharpness ]
                                                                relativeTime:0.0
                                                                    duration:1.0] autorelease];
            CHHapticPattern* pattern = [[[CHHapticPattern alloc] initWithEvents:@[ event ] parameters:@[] error:&error] autorelease];
            if (!pattern)
            {
                LogError("rumble pattern", error, s_loggedPlayerError);
                return;
            }
            id<CHHapticAdvancedPatternPlayer> player = [s_engine createAdvancedPlayerWithPattern:pattern error:&error];
            if (!player)
            {
                LogError("rumble player", error, s_loggedPlayerError);
                return;
            }
            player.loopEnabled = YES;
            s_rumble = [player retain];
        }
    }

    // Parameters for the rumble player: intensity is a multiplier (base 1),
    // sharpness an offset from the event's 0.5, so 2*s - 1 reaches both ends.
    void SendRumbleParameters(float intensity, float sharpness)
    {
        if (!s_rumble)
            return;
        @autoreleasepool
        {
            NSError* error = nil;
            CHHapticDynamicParameter* i = [[[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl
                                                                                           value:intensity
                                                                                    relativeTime:0.0] autorelease];
            CHHapticDynamicParameter* s = [[[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticSharpnessControl
                                                                                           value:std::clamp(sharpness * 2.0f - 1.0f, -1.0f, 1.0f)
                                                                                    relativeTime:0.0] autorelease];
            if (![s_rumble sendParameters:@[ i, s ] atTime:CHHapticTimeImmediate error:&error])
                LogError("rumble parameters", error, s_loggedParameterError);
        }
    }

    // The engine, started, with this generation's players. False when the
    // device has none or the engine refuses to start; the caller is then a
    // no-op (one log line, not one a frame).
    bool EnsureEngine()
    {
        if (!platform::haptics::Available())
            return false;
        const double now = Now();
        if (!s_running && now < s_retryAt)
            return false;
        if (!s_engine)
        {
            NSError* error = nil;
            CHHapticEngine* engine = [[CHHapticEngine alloc] initAndReturnError:&error];
            if (!engine)
            {
                s_retryAt = now + 1.0;
                LogError("engine unavailable", error, s_loggedStartError);
                return false;
            }
            engine.playsHapticsOnly = YES;
            engine.autoShutdownEnabled = NO;
            // Both handlers arrive on a thread of Core Haptics' own: they
            // only mark the engine (and its players) as needing a rebuild,
            // which the next main-thread call does.
            engine.stoppedHandler = ^(CHHapticEngineStoppedReason reason) {
                s_running = false;
                s_reset = true;
                fprintf(stderr, "[haptics] engine stopped (reason %ld)\n", long(reason));
            };
            engine.resetHandler = ^{
                s_running = false;
                s_reset = true;
            };
            s_engine = engine;
        }
        if (!s_running)
        {
            NSError* error = nil;
            if (![s_engine startAndReturnError:&error])
            {
                s_retryAt = now + 1.0;
                LogError("engine failed to start", error, s_loggedStartError);
                return false;
            }
            s_retryAt = 0.0;
            s_running = true;
            s_reset = true;
        }
        if (s_reset.exchange(false))
        {
            ReleaseTaps();
            BuildRumble();
        }
        return true;
    }
}

namespace platform::haptics
{
    bool Available()
    {
        if (!s_checked)
        {
            s_checked = true;
            s_available = [[CHHapticEngine capabilitiesForHardware] supportsHaptics];
            if (!s_available)
                fprintf(stderr, "[haptics] this device has no haptics\n");
        }
        return s_available;
    }

    void Start()
    {
        EnsureEngine();
    }

    void Stop()
    {
        if (s_rumble && s_loopPlaying)
            [s_rumble stopAtTime:CHHapticTimeImmediate error:nil];
        s_loopPlaying = false;
        s_lastIntensity = s_lastSharpness = -1.0f;
        if (s_engine && s_running)
            [s_engine stopWithCompletionHandler:nil];
        s_running = false;
        // A later Start() must not play this generation's players, and may
        // try the engine again at once.
        s_reset = true;
        s_retryAt = 0.0;
        ReleaseTaps();
    }

    void Shutdown()
    {
        Stop();
        [s_rumble release];
        s_rumble = nil;
        [s_engine release];
        s_engine = nil;
        s_running = false;
        s_reset = false;
    }

    void Rumble(const Pattern& pattern)
    {
        if (!EnsureEngine() || !s_rumble)
            return;

        const float intensity = std::clamp(pattern.intensity, 0.0f, 1.0f);
        const float sharpness = std::clamp(pattern.sharpness, 0.0f, 1.0f);
        const double now = Now();

        if (intensity <= kSilence)
        {
            if (!s_loopPlaying)
                return;
            if (s_lastIntensity > 0.0f)
            {
                SendRumbleParameters(0.0f, sharpness);
                s_lastIntensity = 0.0f;
                s_lastSharpness = sharpness;
                s_silentAt = now;
            }
            if (now - s_silentAt >= kLoopStopHold)
            {
                [s_rumble stopAtTime:CHHapticTimeImmediate error:nil];
                s_loopPlaying = false;
                s_lastIntensity = s_lastSharpness = -1.0f;
            }
            return;
        }

        if (!s_loopPlaying)
        {
            s_rumble.isMuted = YES;
            NSError* error = nil;
            if (![s_rumble startAtTime:CHHapticTimeImmediate error:&error])
            {
                LogError("rumble start", error, s_loggedPlayerError);
                return;
            }
            SendRumbleParameters(intensity, sharpness);
            s_rumble.isMuted = NO;
            s_loopPlaying = true;
            s_lastIntensity = intensity;
            s_lastSharpness = sharpness;
            return;
        }

        // Only real changes: the game's motors move every frame, and a
        // parameter a frame more than this would be no more felt.
        if (std::fabs(intensity - s_lastIntensity) > 0.02f || std::fabs(sharpness - s_lastSharpness) > 0.03f)
        {
            SendRumbleParameters(intensity, sharpness);
            s_lastIntensity = intensity;
            s_lastSharpness = sharpness;
        }
    }

    void Tap(const Pattern& pattern)
    {
        if (!EnsureEngine())
            return;
        const float intensity = std::clamp(pattern.intensity, 0.0f, 1.0f);
        const float sharpness = std::clamp(pattern.sharpness, 0.0f, 1.0f);
        if (intensity <= 0.0f)
            return;

        id<CHHapticPatternPlayer> player = nil;
        for (size_t i = 0; i < s_tapCount; i++)
        {
            if (std::fabs(s_taps[i].pattern.intensity - intensity) < 1e-4f &&
                std::fabs(s_taps[i].pattern.sharpness - sharpness) < 1e-4f)
            {
                player = s_taps[i].player;
                break;
            }
        }
        if (!player)
        {
            if (s_tapCount >= kTapCache)
                return;
            @autoreleasepool
            {
                NSError* error = nil;
                CHHapticEventParameter* i = [[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity
                                                                                           value:intensity] autorelease];
                CHHapticEventParameter* s = [[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness
                                                                                           value:sharpness] autorelease];
                CHHapticEvent* event = [[[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticTransient
                                                                      parameters:@[ i, s ]
                                                                    relativeTime:0.0
                                                                        duration:0.0] autorelease];
                CHHapticPattern* pattern = [[[CHHapticPattern alloc] initWithEvents:@[ event ] parameters:@[] error:&error] autorelease];
                if (!pattern)
                {
                    LogError("tap pattern", error, s_loggedPlayerError);
                    return;
                }
                player = [s_engine createPlayerWithPattern:pattern error:&error];
                if (!player)
                {
                    LogError("tap player", error, s_loggedPlayerError);
                    return;
                }
                s_taps[s_tapCount].pattern = { intensity, sharpness };
                s_taps[s_tapCount].player = [player retain];
                s_tapCount++;
            }
        }

        NSError* error = nil;
        // One still playing refuses another start: a dropped tick when two
        // cues fall on the same instant, not something to report.
        (void)[player startAtTime:CHHapticTimeImmediate error:&error];
    }
}
