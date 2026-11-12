// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See picture_rect.h.
#include "picture_rect.h"

namespace video
{
    namespace
    {
        // Main thread only (the presenter writes it, the UI and the input read
        // it, all there): no lock, as presenter's own per-frame state.
        PictureRect s_pictureRect;
    }

    void SetPictureRect(const PictureRect& rect)
    {
        s_pictureRect = rect;
    }

    PictureRect GetPictureRect()
    {
        return s_pictureRect;
    }
}
