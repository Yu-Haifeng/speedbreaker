// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// What the touch controls (hid/touch_input.h) play on the device's haptics,
// with no controller to rumble: the game's two motors, a button's press, and
// the steering's detents.
//
//   - The game's rumble (XamInputSetState's wLeftMotorSpeed and
//     wRightMotorSpeed) is an Xbox 360 pad's two motors: the left one heavy
//     and low, the right one light and fast. A phone has one Taptic Engine,
//     so they become one pattern: the stronger motor is its strength, and
//     the right motor's share of the two is its sharpness (a dull rumble for
//     the left alone, a crisp buzz for the right). A perceptual curve makes
//     the game's quieter rumbles felt, since a phone is not a pad in hand.
//   - A button pressed is one short tap: the deck's own confirmation of a
//     control that has no travel. The steering is the exception: it has the
//     two detents below instead (and its bit in Sample::pressed is ignored).
//   - The steering ticks softly as it crosses centre and more firmly as it
//     reaches full lock, once each way, with hysteresis and a cooldown so a
//     thumb near a line cannot machine-gun it.
//
// Header only and SDL-free (platform/haptics.h's iOS side is the only
// platform code, and it is empty elsewhere), for tests/haptic_feedback_test.
// hid/input.cpp drives it from the frame loop, where the game's rumble and
// Settings > Input > Vibration are in hand.
#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <hid/touch_input.h>
#include <platform/haptics.h>

namespace hid::haptic
{
    using Pattern = platform::haptics::Pattern;

    // The motors' curve: x^0.7 lifts the game's quieter rumbles (a pad's
    // motor is felt in the hand; a phone's is not), and below this fraction
    // they are the noise floor and not played at all.
    constexpr float kMotorGamma = 0.7f;
    constexpr float kMotorFloor = 0.02f;

    // A motor alone: the left is as dull as the engine goes, the right as
    // sharp (the Taptic Engine has no separate motors to keep them apart).
    constexpr float kSharpnessMin = 0.05f;
    constexpr float kSharpnessMax = 0.95f;

    // One motor as a fraction of the phone's, 0..1 (0 below kMotorFloor).
    inline float MotorResponse(uint16_t motor)
    {
        const float x = float(motor) / 65535.0f;
        return x < kMotorFloor ? 0.0f : std::pow(x, kMotorGamma);
    }

    // The game's rumble as the phone's: the stronger motor is the strength,
    // and the right motor's share of the two is the sharpness (0 all left,
    // 1 all right). {0, 0} when the game asked for nothing.
    inline Pattern GameRumble(uint16_t low, uint16_t high)
    {
        const float left = MotorResponse(low);
        const float right = MotorResponse(high);
        const float strength = std::max(left, right);
        if (strength <= 0.0f)
            return {};
        const float balance = right / (left + right);
        return { strength, std::clamp(balance, kSharpnessMin, kSharpnessMax) };
    }

    // The deck's transients. The button tap stays below the game's strongest
    // rumble, so it reads as feedback on top of it, not as more rumble; the
    // centre tick is the softest, and the lock tick firmer and a little
    // sharper, so the two are told apart by feel.
    inline constexpr Pattern kButtonTap { 0.40f, 0.55f };
    inline constexpr Pattern kCentreDetent { 0.28f, 0.22f };
    inline constexpr Pattern kLockDetent { 0.55f, 0.50f };

    // Every control but the steering, whose feedback is the detents below.
    inline constexpr uint32_t kTapMask = [] {
        uint32_t mask = 0;
        for (size_t i = 0; i < touch::kControls; i++)
            if (touch::Control(i) != touch::Control::Steer)
                mask |= uint32_t(1) << i;
        return mask;
    }();

    // What this frame's sample is worth playing.
    struct Cues
    {
        uint32_t tapped = 0;  // a bit per Control, those that just went down
        bool centre = false;  // the steering crossed centre
        bool fullLock = false;  // the steering reached full lock

        uint8_t TapCount() const { return uint8_t(std::popcount(tapped)); }
    };

    // The steering's detents. The first sample of a hold is the baseline
    // (the panel comes to the thumb at centre, so nothing ticks); from there
    // a direction is taken only past a small deadband, and a crossing needs
    // the other side of it, so jitter around centre is nothing.
    constexpr float kCentreArm = 0.10f;
    constexpr float kCentreCooldown = 0.12f;
    constexpr float kFullLock = 0.97f;
    constexpr float kFullRelease = 0.85f;  // hysteresis: leave the lock zone
    constexpr float kLockCooldown = 0.20f;

    // One frame's sample in, the cues to play out. `dtSeconds` is the
    // presented frame's time (clamped, so a long first frame or a
    // suspension cannot skip a cooldown); nothing assumes 60 Hz.
    struct TouchFeedback
    {
        Cues Step(const touch::Sample& sample, float dtSeconds)
        {
            Cues cues;
            const float dt = std::clamp(dtSeconds, 0.0f, 0.25f);
            centreCooldown_ = std::max(0.0f, centreCooldown_ - dt);
            lockCooldown_ = std::max(0.0f, lockCooldown_ - dt);

            cues.tapped = (sample.pressed & ~lastPressed_) & kTapMask;
            lastPressed_ = sample.pressed;

            // Not steering: the next press is a new baseline, not a tick.
            if (!sample.steerSet)
            {
                haveSteer_ = false;
                steerDirection_ = 0;
                lockArmed_ = true;
                return cues;
            }

            const float steer = std::clamp(sample.steer, -1.0f, 1.0f);
            if (!haveSteer_)
            {
                // The hold's first sample: take its side, but play nothing.
                haveSteer_ = true;
                if (std::fabs(steer) >= kCentreArm)
                    steerDirection_ = steer > 0.0f ? 1 : -1;
                lockArmed_ = std::fabs(steer) < kFullLock;
                return cues;
            }
            if (std::fabs(steer) >= kCentreArm)
            {
                const int direction = steer > 0.0f ? 1 : -1;
                if (steerDirection_ != 0 && direction != steerDirection_ && centreCooldown_ <= 0.0f)
                {
                    cues.centre = true;
                    centreCooldown_ = kCentreCooldown;
                }
                steerDirection_ = direction;
            }
            if (std::fabs(steer) >= kFullLock)
            {
                if (lockArmed_ && lockCooldown_ <= 0.0f)
                {
                    cues.fullLock = true;
                    lockArmed_ = false;
                    lockCooldown_ = kLockCooldown;
                }
            }
            else if (std::fabs(steer) <= kFullRelease)
            {
                lockArmed_ = true;
            }
            return cues;
        }

        void Reset() { *this = TouchFeedback{}; }

    private:
        uint32_t lastPressed_ = 0;
        bool haveSteer_ = false;
        int steerDirection_ = 0;  // -1 left, +1 right, 0 not yet past the deadband
        bool lockArmed_ = true;
        float centreCooldown_ = 0.0f, lockCooldown_ = 0.0f;
    };

    // When the engine is up: the deck active, Vibration on, no controller or
    // wheel in hand. It stops a held moment after that goes away, so a menu
    // opened and closed (or the setting toggled) doesn't restart it, and at
    // once for a suspension's Stop().
    struct EngineGate
    {
        bool Step(bool wanted, double nowSeconds, double holdSeconds = 2.0)
        {
            if (wanted)
            {
                on_ = true;
                stopping_ = false;
                return true;
            }
            if (!on_)
                return false;
            if (!stopping_)
            {
                stopping_ = true;
                stopAt_ = nowSeconds;
            }
            if (nowSeconds - stopAt_ >= holdSeconds)
            {
                on_ = false;
                stopping_ = false;
                return false;
            }
            return true;
        }

        void Reset()
        {
            on_ = false;
            stopping_ = false;
            stopAt_ = 0.0;
        }

    private:
        bool on_ = false, stopping_ = false;
        double stopAt_ = 0.0;
    };
}
