// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// stb_image, compiled for PNG alone: the touch deck's button art is a PNG
// compiled into the game (ui/deck_art.cpp). Public domain (or MIT, at your
// option): LICENSES/stb-MIT.txt, THIRD_PARTY_NOTICES.md. See
// stb_image.h for the reference; only this translation unit sets the
// implementation macro.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO  // (the bytes are compiled in: no file paths, no fopen)
#include "stb_image.h"
