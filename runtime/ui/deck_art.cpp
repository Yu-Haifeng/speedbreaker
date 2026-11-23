// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See deck_art.h.
#include <stdafx.h>
#include "deck_art.h"
#include "ui.h"

#include <thirdparty/stb_image/stb_image.h>

#include <climits>

#include <imgui_impl_vulkan.h>

// The atlas, as a PNG compiled in (generated/deck_atlas.inc, written by
// runtime/CMakeLists.txt from ui/touch/deck_atlas.png).
namespace
{
    const unsigned char kAtlasPng[] = {
#include "deck_atlas.inc"
    };
}

namespace ui::deckart
{
    namespace
    {
        // The texture as ImGui 1.92 wants it handed over: the backend makes the
        // image, uploads the pixels and stores the descriptor in it (see
        // imgui_impl_vulkan.cpp's ImGui_ImplVulkan_UpdateTexture). It is ours,
        // not the font atlas's, so it is ours to destroy, too.
        ImTextureData s_texture;
        std::mutex* s_queueMutex = nullptr;
    }

    bool Initialize(const VulkanTarget& target)
    {
        if (s_texture.Status == ImTextureStatus_OK)
            return true;  // (already up: a second Initialize would leak the first)

        int width = 0, height = 0, channels = 0;
        unsigned char* pixels = stbi_load_from_memory(kAtlasPng, int(sizeof kAtlasPng), &width, &height, &channels, 4);
        if (!pixels)
        {
            fprintf(stderr, "[touch] the deck's art did not decode: %s\n", stbi_failure_reason());
            return false;
        }
        if (width != hid::touch::art::kAtlasWidth || height != hid::touch::art::kAtlasHeight)
        {
            // (The header is packed from this very PNG, so a mismatch means one
            // of the two is stale: re-run scripts/gen_deck_atlas.py.)
            fprintf(stderr, "[touch] the deck's art is %dx%d, but its header says %dx%d\n", width, height,
                hid::touch::art::kAtlasWidth, hid::touch::art::kAtlasHeight);
            stbi_image_free(pixels);
            return false;
        }

        s_queueMutex = target.queueMutex;
        s_texture.Create(ImTextureFormat_RGBA32, width, height);
        memcpy(s_texture.Pixels, pixels, size_t(width) * size_t(height) * 4);
        stbi_image_free(pixels);

        {
            // (The backend submits the upload on this queue and waits for it.)
            std::lock_guard lock(*s_queueMutex);
            ImGui_ImplVulkan_UpdateTexture(&s_texture);
        }
        if (s_texture.Status != ImTextureStatus_OK)
        {
            fprintf(stderr, "[touch] the deck's art: the upload failed\n");
            return false;
        }
        return true;
    }

    void Shutdown()
    {
        if (s_texture.Status != ImTextureStatus_OK)
            return;
        // The backend only frees a texture it considers unused, and it counts
        // that only for the ones ImGui owns (the font atlas's). This one is
        // past its last frame by the time we are here, so say so.
        s_texture.DestroyPixels();
        s_texture.UnusedFrames = INT_MAX;
        s_texture.SetStatus(ImTextureStatus_WantDestroy);
        if (s_queueMutex)
        {
            std::lock_guard lock(*s_queueMutex);
            ImGui_ImplVulkan_UpdateTexture(&s_texture);
        }
    }

    bool Ready()
    {
        return s_texture.Status == ImTextureStatus_OK;
    }

    ImTextureRef Atlas()
    {
        return Ready() ? s_texture.GetTexRef() : ImTextureRef();
    }

    const hid::touch::art::Info& Info(Art art)
    {
        return hid::touch::art::kArt[size_t(art)];
    }
}
