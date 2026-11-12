// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Where the game's picture sits on the screen: the rectangle it fills, with
// the letterbox or pillarbox bars left out when the screen's shape differs
// from the frame's (video/picture_fit.h). The touch controls lay themselves
// out on it (hid/touch_layout.h): the design's canvas is the picture, so a
// control keeps its place on the game's image whatever the screen's shape,
// and its size follows the picture's width (a "cqw" is 1% of it).
//
// The presenter writes it once per presented frame, from the same
// dx,dy,dw,dh it draws the picture with. Main thread only. The value holds
// between frames, and is invalid (0, 0, 0, 0) until the first picture: there
// is nothing to lay a control out on before that.
#pragma once

namespace video
{
    struct PictureRect
    {
        // Fractions of the swapchain, from its top left.
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        // The picture's own shape, width / height in pixels: 16:9 for a whole
        // frame, the screen's when it fills (Hor+ or Vert+), and not w / h
        // above unless the swapchain is square.
        float aspect = 0.0f;

        bool Valid() const { return w > 0.0f && h > 0.0f && aspect > 0.0f; }
    };

    // Main thread, every presented frame (an invalid rect for a frame with no
    // picture).
    void SetPictureRect(const PictureRect& rect);
    PictureRect GetPictureRect();
}
