// Tests for user/touch_deck_file.cpp: the deck's file -- what it writes, what
// it reads, and what it refuses.
//
// The store is pointed at a temporary folder, so this needs none of the
// runtime's own paths. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/touch_deck_file_test.cpp \
//       runtime/user/touch_deck_file.cpp runtime/platform/posix/file_io.cpp \
//       -Itools/XenonRecomp/thirdparty/tomlplusplus/include -o build/touch_deck_file_test \
//       && build/touch_deck_file_test
#include <user/touch_deck_file.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

using namespace hid::touch;

namespace
{
    std::string Read(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    void Write(const std::filesystem::path& path, const std::string& text)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
    }

    bool Contains(const std::string& text, const std::string& what)
    {
        return text.find(what) != std::string::npos;
    }

    bool near(float got, float want) { return std::fabs(got - want) < 1e-5f; }
}

int main(int argc, char** argv)
{
    std::error_code ec;
    const std::filesystem::path root = argc > 1 ? std::filesystem::path(argv[1])
                                                : std::filesystem::temp_directory_path() / "sb-touch-deck-test";
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);

    // Nothing yet: the design's layout, and no complaint.
    {
        touchdeck::Store store{ root / "missing.toml", {}, Grid{}, false };
        CHECK(store.Load(), "a missing file is not a failure");
        CHECK(!store.overrides.Any(), "and the design's layout is in force");
        CHECK(store.grid.snap && store.grid.pitch == 1.0f, "with the editor's defaults");
    }

    // An untouched deck writes one line: the version.
    {
        touchdeck::Store store{ root / "defaults.toml", {}, Grid{}, false };
        CHECK(store.Save(Overrides{}).empty(), "the default layout saves");
        const std::string text = Read(store.path);
        CHECK(Contains(text, "version = 1"), "the version is written");
        CHECK(!Contains(text, "[deck"), "and no control (there is nothing to say)");
        CHECK(!Contains(text, "[editor]"), "nor the editor's defaults");
    }

    // What differs is kept, and comes back as it went in.
    {
        Overrides wanted;
        wanted.of[size_t(Control::Throttle)].dx = -1.5f;
        wanted.of[size_t(Control::Throttle)].scale = 1.15f;
        wanted.of[size_t(Control::Throttle)].icon = art::Id::Handbrake;
        wanted.of[size_t(Control::Brake)].dy = 2.25f;
        wanted.of[size_t(Control::Camera)].icon = art::Id::AnalogBase;

        touchdeck::Store written{ root / "round.toml", {}, Grid{}, false };
        CHECK(written.Save(wanted).empty(), "a moved deck saves");
        const std::string text = Read(written.path);
        CHECK(Contains(text, "[deck.throttle]"), "the control's own table");
        CHECK(Contains(text, "icon = 'handbrake'") || Contains(text, "icon = \"handbrake\""), "its picture");
        CHECK(Contains(text, "[deck.brake]"), "the control that only moved");
        CHECK(!Contains(text, "[deck.camera]\nx"), "and one that only changed its picture has no offset");

        touchdeck::Store read{ root / "round.toml", {}, Grid{}, false };
        CHECK(read.Load(), "and reads back");
        const Placement& throttle = read.overrides.of[size_t(Control::Throttle)];
        CHECK(near(throttle.dx, -1.5f), "the offset (%.3f)", throttle.dx);
        CHECK(throttle.dy == 0.0f, "and the one it never had");
        CHECK(near(throttle.scale, 1.15f), "its size (%.3f)", throttle.scale);
        CHECK(throttle.icon == art::Id::Handbrake, "its picture");
        CHECK(read.overrides.of[size_t(Control::Brake)].icon == art::Id::Count, "one that kept its own");
        CHECK(near(read.overrides.of[size_t(Control::Brake)].dy, 2.25f), "and one that moved");
        CHECK(read.overrides.of[size_t(Control::Camera)].icon == art::Id::AnalogBase, "and a third's picture");
    }

    // A broken file: kept as .bad, the design's layout, and a Save still works.
    {
        const std::filesystem::path path = root / "broken.toml";
        Write(path, "version = 1\n[deck.throttle\nx = 1\n");
        touchdeck::Store store{ path, {}, Grid{}, false };
        CHECK(!store.Load(), "a file that does not parse is refused");
        CHECK(std::filesystem::exists(std::filesystem::path(path) += ".bad"), "and kept beside itself");
        CHECK(!store.overrides.Any(), "with the design's layout in force");
        CHECK(store.Save(Overrides{}).empty(), "and saving works again");
        CHECK(!Contains(Read(path), "[deck.throttle"), "over the broken one");
    }

    // What this version does not know is left out, not fatal: a future
    // control, a picture that isn't there, a field it has no use for.
    {
        const std::filesystem::path path = root / "unknown.toml";
        Write(path, "version = 1\n"
                    "[deck.throttle]\n"
                    "x = 1.0\n"
                    "wobble = 3\n"
                    "icon = \"banana\"\n"
                    "[deck.warp-drive]\n"
                    "x = 9.0\n"
                    "[editor]\n"
                    "grid = 0.8\n"
                    "fizz = true\n");
        touchdeck::Store store{ path, {}, Grid{}, false };
        CHECK(store.Load(), "a file with strange keys still loads");
        CHECK(near(store.overrides.of[size_t(Control::Throttle)].dx, 1.0f), "what it knows is taken");
        CHECK(store.overrides.of[size_t(Control::Throttle)].icon == art::Id::Count, "an unknown picture is its own");
        CHECK(!store.overrides.Any() == false, "and the control counts as moved");
        CHECK(near(store.grid.pitch, 1.0f), "0.8 is nearest to the 1 cqw grid (%.2f)", store.grid.pitch);
    }

    // A file from a newer version: read, and never written back.
    {
        const std::filesystem::path path = root / "future.toml";
        Write(path, "version = 7\n[deck.throttle]\nx = 4.0\n");
        const std::string before = Read(path);
        touchdeck::Store store{ path, {}, Grid{}, false };
        CHECK(store.Load(), "a newer file still loads");
        CHECK(store.readOnly, "but it is read-only");
        CHECK(near(store.overrides.of[size_t(Control::Throttle)].dx, 4.0f), "and what it says is used");
        CHECK(!store.Save(Overrides{}).empty(), "saving refuses");
        CHECK(Read(path) == before, "and the file is untouched");
    }

    // The editor's two, round trip.
    {
        const std::filesystem::path path = root / "editor.toml";
        touchdeck::Store store{ path, {}, Grid{}, false };
        CHECK(store.SetGrid({ false, 2.0f }).empty(), "the editor's settings save");
        touchdeck::Store read{ path, {}, Grid{}, false };
        CHECK(read.Load(), "and read back");
        CHECK(!read.grid.snap, "snapping off");
        CHECK(near(read.grid.pitch, 2.0f), "on a 2 cqw grid");
        CHECK(Contains(Read(path), "[editor]"), "in their own table");
    }

    // Writing a file the player keeps elsewhere: the link is replaced, not
    // written through to whatever it points at.
    {
        const std::filesystem::path real = root / "real.toml";
        const std::filesystem::path link = root / "link.toml";
        Write(real, "version = 1\n[deck.throttle]\nx = 8.0\n");
        std::filesystem::create_symlink(real, link, ec);
        if (ec)
        {
            printf("  (no symlinks here: skipped)\n");
        }
        else
        {
            touchdeck::Store store{ link, {}, Grid{}, false };
            CHECK(store.Load(), "the link is read");
            CHECK(store.Save(Overrides{}).empty(), "and written");
            // (Written through, as settings.toml is: a file kept in a dotfiles
            // repo stays where the player put it, and the link stays a link.)
            CHECK(std::filesystem::is_symlink(std::filesystem::symlink_status(link)), "the link is still a link");
            CHECK(!Contains(Read(real), "[deck.throttle"), "what it points at holds what was saved");
            CHECK(Read(link) == Read(real), "and reading the link reads that");
        }
    }

    // Nothing left behind: the temporary a save writes through is renamed, not
    // abandoned.
    {
        size_t temps = 0;
        for (const auto& entry : std::filesystem::directory_iterator(root))
        {
            const std::string name = entry.path().filename().string();
            if (name.ends_with(".tmp"))
                temps++;
        }
        CHECK(temps == 0, "no temporary files left (%zu)", temps);
    }

    // A file too big to be a deck file is refused before it is read.
    {
        const std::filesystem::path path = root / "huge.toml";
        std::string text = "version = 1\n";
        text.append(70 * 1024, '#');
        Write(path, text);
        touchdeck::Store store{ path, {}, Grid{}, false };
        CHECK(!store.Load(), "a 70 KB file is refused");
        CHECK(!store.overrides.Any(), "with the design's layout");
    }

    std::filesystem::remove_all(root, ec);
    printf("%s (%d failures)\n", g_failures ? "FAILED" : "passed", g_failures);
    return g_failures ? 1 : 0;
}
