// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See touch_deck_file.h.
//
// The file's shape, as this file writes it:
//
//   version = 1
//
//   [deck.throttle]
//   x = -1.5          # cqw, from where the layout would have put it
//   y = 0.0
//   scale = 1.15
//   icon = "gas"
//
//   [editor]
//   snap = true
//   grid = 1.0
//
// Only what differs from the design is written, so an untouched deck's file is
// one line; a control's table holds only the fields that differ. An older
// SpeedBreaker reading a newer file would drop what it does not know, so a
// file from a newer version is read but never written back.
#include "touch_deck_file.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

#include <fcntl.h>
#ifdef _WIN32
#include <platform/os.h>
#else
#include <unistd.h>
#endif

#include <platform/file_io.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#if __has_include(<toml++/toml.hpp>)
#include <toml++/toml.hpp>
#else
#include "../../tools/XenonRecomp/thirdparty/tomlplusplus/include/toml++/toml.hpp"
#endif
#pragma clang diagnostic pop

namespace touchdeck
{
    namespace
    {
        using namespace hid::touch;

        // The file's own version: what this build writes, and the newest it
        // understands in full. A newer one is read as far as it goes.
        constexpr int64_t kVersion = 1;

        // A deck file is a few hundred bytes. Anything larger is not one, and
        // reading it would be a way to make the game allocate without end.
        constexpr uint64_t kMaxBytes = 64 * 1024;

        // Nothing sensible is 20 decks away from where the layout puts it; a
        // crafted file could ask for the other side of the world otherwise.
        constexpr float kMaxOffset = 400.0f;

        // The grid's three pitches (ui/touch_deck_editor.cpp cycles them).
        constexpr float kPitches[3] = { 0.5f, 1.0f, 2.0f };

        float NearestPitch(float value)
        {
            if (!std::isfinite(value))
                return 1.0f;
            float best = kPitches[1];
            for (const float pitch : kPitches)
            {
                if (std::fabs(value - pitch) < std::fabs(value - best))
                    best = pitch;
            }
            return best;
        }

        int ControlIndex(std::string_view name)
        {
            for (size_t i = 0; i < kControls; i++)
            {
                if (name == kControlKey[i])
                    return int(i);
            }
            return -1;
        }

        int IconIndex(std::string_view name)
        {
            for (size_t i = 0; i < art::kCount; i++)
            {
                if (name == art::kArtKey[i])
                    return int(i);
            }
            return -1;
        }

        void Clamp(Placement& placement)
        {
            if (!std::isfinite(placement.dx) || !std::isfinite(placement.dy) || !std::isfinite(placement.scale))
                placement = Placement{};
            placement.dx = std::clamp(placement.dx, -kMaxOffset, kMaxOffset);
            placement.dy = std::clamp(placement.dy, -kMaxOffset, kMaxOffset);
            placement.scale = std::clamp(placement.scale, kMinPlacement, kMaxPlacement);
        }

        // A path through its links, so a file the player keeps elsewhere (a
        // dotfile repo, say) is written where it lives rather than replaced by
        // a link to itself.
        std::filesystem::path Target(const std::filesystem::path& path)
        {
            std::filesystem::path p = path;
            for (int hops = 0; hops < 8; hops++)
            {
                std::error_code ec;
                if (!std::filesystem::is_symlink(std::filesystem::symlink_status(p, ec)))
                    break;
                std::filesystem::path link = std::filesystem::read_symlink(p, ec);
                if (ec || link.empty())
                    break;
                p = link.is_absolute() ? link : p.parent_path() / link;
            }
            return p;
        }

        // Keeps a file that could not be used, beside itself, so the player's
        // work is not the thing a Save eats.
        void KeepBad(const std::filesystem::path& path, const std::string& why)
        {
            std::filesystem::path bad = path;
            bad += ".bad";
            std::error_code ec;
            std::filesystem::copy_file(path, bad, std::filesystem::copy_options::overwrite_existing, ec);
            fprintf(stderr, "[deck] %s doesn't parse: %s; the design's layout (kept as %s)\n", path.string().c_str(),
                why.c_str(), ec ? "nothing: the copy failed" : bad.string().c_str());
        }

        // Writes `text` so that the file is always either the old one or the
        // whole new one: a temporary file beside it, flushed, then renamed
        // (platform::AtomicReplace), then the folder itself synced. The
        // temporary is this process's own, so two instances saving at once do
        // not write into one file. "" or why it failed.
        std::string WriteAtomically(const std::filesystem::path& path, std::string_view text)
        {
            const std::filesystem::path target = Target(path);
            std::error_code ec;
            const std::filesystem::path dir = target.parent_path();
            if (!dir.empty())
            {
                std::filesystem::create_directories(dir, ec);
                if (ec)
                    return std::format("can't create {}: {}", dir.string(), ec.message());
            }
            std::filesystem::path temp = target;
            temp += std::format(".{}.tmp", long(getpid()));
            int fd = platform::OpenFile(temp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
            if (fd < 0)
                return std::format("can't write {}: {}", target.string(), strerror(errno));
            int error = 0;
            for (size_t done = 0; !error && done < text.size();)
            {
                const ssize_t n = write(fd, text.data() + done, text.size() - done);
                if (n > 0)
                    done += size_t(n);
                else if (n < 0 && errno == EINTR)
                    continue;
                else
                    error = n == 0 ? EIO : errno;
            }
#ifdef __APPLE__
            // fsync on macOS doesn't flush the drive's cache; F_FULLFSYNC does
            // (not every file system supports it).
            if (!error && fcntl(fd, F_FULLFSYNC) != 0 && fsync(fd) != 0)
                error = errno;
#else
            if (!error && fsync(fd) != 0)
                error = errno;
#endif
            if (close(fd) != 0 && !error)
                error = errno;
            if (error)
            {
                platform::RemoveFile(temp);
                return std::format("can't write {}: {}", target.string(), strerror(error));
            }
            if ((error = platform::AtomicReplace(temp, target)) != 0)
            {
                platform::RemoveFile(temp);
                return std::format("can't replace {}: {}", target.string(), strerror(error));
            }
            platform::SyncDirectory(dir);
            return "";
        }
    }

    bool Store::Load()
    {
        overrides = Overrides{};
        grid = Grid{};
        readOnly = false;
        if (path.empty())
            return true;

        const std::filesystem::path target = Target(path);
        std::error_code ec;
        if (!std::filesystem::exists(target, ec))
            return true;  // (nothing moved yet: the design's layout)
        if (!std::filesystem::is_regular_file(target, ec))
        {
            fprintf(stderr, "[deck] %s isn't a file; the design's layout\n", path.string().c_str());
            return false;
        }
        const uint64_t size = std::filesystem::file_size(target, ec);
        if (ec || size > kMaxBytes)
        {
            fprintf(stderr, "[deck] %s is %llu bytes; the design's layout\n", path.string().c_str(),
                static_cast<unsigned long long>(ec ? 0 : size));
            return false;
        }
        std::ifstream file(target, std::ios::binary);
        if (!file)
        {
            fprintf(stderr, "[deck] can't read %s; the design's layout\n", path.string().c_str());
            return false;
        }
        const std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };

        toml::table root;
        std::string error;
        toml::source_position where{};
#if TOML_EXCEPTIONS
        try
        {
            root = toml::parse(text, path.string());
        }
        catch (const toml::parse_error& e)
        {
            error = e.description();
            where = e.source().begin;
        }
#else
        toml::parse_result result = toml::parse(text, path.string());
        if (result)
            root = std::move(result).table();
        else
        {
            error = result.error().description();
            where = result.error().source().begin;
        }
#endif
        if (!error.empty())
        {
            KeepBad(path, std::format("{} (line {}, column {})", error, where.line, where.column));
            return false;
        }

        // A version this build does not fully know: take what it does, and
        // leave the file alone.
        const int64_t version = root["version"].value_or<int64_t>(int64_t(kVersion));
        if (version > kVersion)
        {
            readOnly = true;
            fprintf(stderr, "[deck] %s is version %lld, newer than %lld: read-only here\n", path.string().c_str(),
                static_cast<long long>(version), static_cast<long long>(kVersion));
        }

        for (auto&& [key, node] : root)
        {
            const std::string_view name = key.str();
            if (name == "version")
                continue;
            if (name == "deck")
            {
                const toml::table* deck = node.as_table();
                if (!deck)
                {
                    fprintf(stderr, "[deck] %s: deck isn't a table; ignored\n", path.filename().string().c_str());
                    continue;
                }
                for (auto&& [controlKey, controlNode] : *deck)
                {
                    const int index = ControlIndex(controlKey.str());
                    if (index < 0)
                    {
                        fprintf(stderr, "[deck] %s: no control called %s; ignored\n", path.filename().string().c_str(),
                            std::string(controlKey.str()).c_str());
                        continue;
                    }
                    const toml::table* entry = controlNode.as_table();
                    if (!entry)
                    {
                        fprintf(stderr, "[deck] %s: %s isn't a table; ignored\n", path.filename().string().c_str(),
                            std::string(controlKey.str()).c_str());
                        continue;
                    }
                    Placement& placement = overrides.of[size_t(index)];
                    for (auto&& [field, value] : *entry)
                    {
                        const std::string_view fieldName = field.str();
                        if (fieldName == "icon")
                        {
                            const std::string icon = value.value_or<std::string>(std::string());
                            const int art = IconIndex(icon);
                            if (art < 0)
                            {
                                fprintf(stderr, "[deck] %s: no picture called %s; ignored\n",
                                    path.filename().string().c_str(), icon.c_str());
                                continue;
                            }
                            placement.icon = art::Id(art);
                            continue;
                        }
                        if (fieldName != "x" && fieldName != "y" && fieldName != "scale")
                        {
                            fprintf(stderr, "[deck] %s: %s.%s unknown; ignored\n", path.filename().string().c_str(),
                                std::string(controlKey.str()).c_str(), std::string(fieldName).c_str());
                            continue;
                        }
                        if (!value.is_number())
                        {
                            fprintf(stderr, "[deck] %s: %s.%s isn't a number; ignored\n", path.filename().string().c_str(),
                                std::string(controlKey.str()).c_str(), std::string(fieldName).c_str());
                            continue;
                        }
                        const float number = float(value.value_or(0.0));
                        if (fieldName == "x")
                            placement.dx = number;
                        else if (fieldName == "y")
                            placement.dy = number;
                        else
                            placement.scale = number;
                    }
                    Clamp(placement);
                }
                continue;
            }
            if (name == "editor")
            {
                const toml::table* editor = node.as_table();
                if (!editor)
                {
                    fprintf(stderr, "[deck] %s: editor isn't a table; ignored\n", path.filename().string().c_str());
                    continue;
                }
                for (auto&& [field, value] : *editor)
                {
                    const std::string_view fieldName = field.str();
                    if (fieldName == "snap")
                        grid.snap = value.value_or(true);
                    else if (fieldName == "grid")
                        grid.pitch = NearestPitch(float(value.value_or(1.0)));
                    else
                        fprintf(stderr, "[deck] %s: editor.%s unknown; ignored\n", path.filename().string().c_str(),
                            std::string(fieldName).c_str());
                }
                continue;
            }
            fprintf(stderr, "[deck] %s: %s unknown; ignored\n", path.filename().string().c_str(),
                std::string(name).c_str());
        }
        return true;
    }

    namespace
    {
        // The file, from whatever the store now holds.
        std::string WriteFile(const Store& store)
        {
            if (store.readOnly)
                return std::format("{} is from a newer SpeedBreaker; its layout is read-only here",
                    store.path.filename().string());
            toml::table root;
            root.insert("version", kVersion);
            toml::table deck;
            // A thousandth of a cqw is a hundredth of a pixel on the design's
            // screen: the file says a number a person can read, and the deck
            // does not move by the difference.
            const auto round = [](float value) { return double(std::round(value * 1000.0f) / 1000.0f); };
            for (size_t i = 0; i < kControls; i++)
            {
                const Placement& placement = store.overrides.of[i];
                if (placement.Default())
                    continue;
                toml::table entry;
                if (placement.dx != 0.0f)
                    entry.insert("x", round(placement.dx));
                if (placement.dy != 0.0f)
                    entry.insert("y", round(placement.dy));
                if (placement.scale != 1.0f)
                    entry.insert("scale", round(placement.scale));
                if (placement.icon != art::Id::Count)
                    entry.insert("icon", std::string(art::kArtKey[size_t(placement.icon)]));
                deck.insert(kControlKey[i], std::move(entry));
            }
            if (!deck.empty())
                root.insert("deck", std::move(deck));
            if (!store.grid.snap || store.grid.pitch != 1.0f)
            {
                toml::table editor;
                if (!store.grid.snap)
                    editor.insert("snap", false);
                if (store.grid.pitch != 1.0f)
                    editor.insert("grid", double(store.grid.pitch));
                root.insert("editor", std::move(editor));
            }
            std::ostringstream text;
            text << toml::toml_formatter{ root };
            return WriteAtomically(store.path, text.str());
        }
    }

    std::string Store::Save(const Overrides& wanted)
    {
        overrides = wanted;
        for (Placement& placement : overrides.of)
            Clamp(placement);
        return WriteFile(*this);
    }

    std::string Store::SetGrid(const Grid& wanted)
    {
        grid = wanted;
        grid.pitch = NearestPitch(grid.pitch);
        return WriteFile(*this);
    }

}
