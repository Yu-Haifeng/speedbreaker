// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// What each control is, before anything decides where it sits: the picture it
// is drawn with (hid/touch_art.h), the Xbox key it stands for, the size class
// the layout gives it, and the two things only drawing cares about.
//
// One table, indexed by Control, so that a control is added or changed in one
// place: the overlay walks it instead of spelling out seventeen calls, the hit
// test asks it which picture to trace, and the layout takes its sizes from
// here rather than from a name per control.
//
// Header only, and free of SDL and ImGui: `keyFace` names a colour, it does not
// carry one -- ui/touch_overlay.cpp maps it to the accent and the four
// face-button colours, which are the only place those exist.
#pragma once
#include <cstddef>
#include <cstdint>

#include <hid/touch_art.h>
#include <hid/touch_control.h>

namespace hid::touch
{
    // How big a control is. The layout gives each class one dimension (a width,
    // or a diameter) and takes the other from the picture where the picture
    // knows better -- a pedal's height is its art's, so nothing is stretched.
    enum class SizeClass : uint8_t
    {
        StickBase,
        Stick,
        Pedal,
        Face,
        Dpad,
        View,
        Shift,
        System,
    };

    // The colour a key's name is written in (design/tokens.json's face
    // colours): the four the Xbox pad names, or the plain chalk of the rest.
    enum class KeyFace : uint8_t
    {
        None,
        Plain,
        A,
        B,
        X,
        Y,
    };

    struct ControlSpec
    {
        Control control;
        art::Id art;        // the picture it is drawn with
        const char* key;    // the key's name, or nullptr where the picture says it
        KeyFace keyFace;
        SizeClass size;
        // The picture's share of its rectangle. 1 fills it; the shifter's gears
        // are rings whose teeth reach the edge of their art, so they are drawn
        // a little smaller than the discs they stand next to.
        float drawScale;
    };

    // In Control's order, so SpecOf is the enum itself.
    inline constexpr ControlSpec kSpecs[kControls] = {
        // Steer is the base and the stick both: this is the base's picture,
        // the stick (art::Id::AnalogStick) is drawn on top of it where the
        // drag has taken it.
        { Control::Steer, art::Id::AnalogBase, "LS", KeyFace::Plain, SizeClass::StickBase, 1.0f },
        { Control::Throttle, art::Id::Gas, "RT", KeyFace::Plain, SizeClass::Pedal, 1.0f },
        { Control::Brake, art::Id::Brake, "LT", KeyFace::Plain, SizeClass::Pedal, 1.0f },
        { Control::Handbrake, art::Id::Handbrake, "A", KeyFace::A, SizeClass::Face, 1.0f },
        { Control::Nitro, art::Id::Nos, "B", KeyFace::B, SizeClass::Face, 1.0f },
        { Control::Speedbreaker, art::Id::Speedbreaker, "X", KeyFace::X, SizeClass::Face, 1.0f },
        { Control::RearView, art::Id::LookBack, "Y", KeyFace::Y, SizeClass::Face, 1.0f },
        // (Its own picture: the deck has one mirror, and the quick glance is
        // drawn from the handoff's icon as its sibling -- scripts/gen_touch_icon.py.)
        { Control::QuickRearView, art::Id::QuickRearView, "LB", KeyFace::Plain, SizeClass::View, 1.0f },
        { Control::Camera, art::Id::Camera, "RB", KeyFace::Plain, SizeClass::View, 1.0f },
        { Control::ShiftUp, art::Id::GearUp, nullptr, KeyFace::None, SizeClass::Shift, 0.86f },
        { Control::ShiftDown, art::Id::GearDown, nullptr, KeyFace::None, SizeClass::Shift, 0.86f },
        // The D-pad's four are the things it opens rather than the arrows: the
        // cross says which way each one is.
        { Control::DpadUp, art::Id::Enter, nullptr, KeyFace::None, SizeClass::Dpad, 1.0f },
        { Control::DpadDown, art::Id::List, nullptr, KeyFace::None, SizeClass::Dpad, 1.0f },
        { Control::DpadLeft, art::Id::Map, nullptr, KeyFace::None, SizeClass::Dpad, 1.0f },
        { Control::DpadRight, art::Id::Message, nullptr, KeyFace::None, SizeClass::Dpad, 1.0f },
        { Control::Back, art::Id::Reset, "BACK", KeyFace::Plain, SizeClass::System, 1.0f },
        { Control::Start, art::Id::Pause, "START", KeyFace::Plain, SizeClass::System, 1.0f },
    };

    inline constexpr const ControlSpec& SpecOf(Control c)
    {
        return kSpecs[size_t(c)];
    }
}
