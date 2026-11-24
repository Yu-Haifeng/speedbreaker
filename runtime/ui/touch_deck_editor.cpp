// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The deck editor: pick a control, drag it where your thumb wants it, size it
// from its bottom-right corner, choose its picture, save. Opened from
// Settings > Input, on every platform -- iOS has no text editor, so this is the
// only way a player can move anything, and it is why touch-deck.toml
// (user/touch_deck_file.h) is written by this and read by the game rather than
// the other way round.
//
// It is not a ui::SetModal: that closes the settings menu, which tells the
// system the game is playable again (XamNotifySystemUi) and lets the game run
// behind the editor. It is a sub-state of the menu instead (ui.cpp decides
// which of the two draws), and the deck is pinned on screen while it runs --
// DrawDeck (ui/touch_overlay.cpp) does not ask whether the deck is in use.
//
// Every change goes through one test (hid/touch_edit.h's Evaluate): the
// candidate placement is clamped into the picture, laid out, and refused if it
// would put one control on another -- because hit testing takes the first of
// two overlapping controls in table order, so an overlap is a button that
// cannot be pressed. A drag shows the refused position in red while the finger
// is down and refuses on release; a slider, a reset, a picture or a nudge is
// refused at once. A refusal says which control it would land on, flashes it,
// and taps the haptics.
//
// A drag on a touch screen lifts the control clear of the finger: the picture
// is drawn above it at its own size (ui::DrawDeckLift), and a dashed outline
// stays where it will land. A mouse does not need that, so it does not happen.
//
// Snapping: a drag follows the nearest line -- another control's edge or
// middle, the picture's safety margin, or the grid -- whichever is closest, and
// the editor draws the line it took. Off, a drag is free. The grid's pitch and
// the switch itself are the player's, kept in the file with the layout.
#include <stdafx.h>
#include "ui.h"
#include "deck_art.h"

#include <hid/haptic_feedback.h>
#include <hid/touch.h>
#include <hid/touch_edit.h>
#include <hid/touch_pick.h>
#include <hid/touch_spec.h>
#include <platform/haptics.h>
#include <user/settings.h>
#include <user/touch_deck_file.h>
#include <video/picture_rect.h>

#include <imgui.h>

namespace ui
{
    namespace
    {
        using namespace hid::touch;
        namespace edit = hid::touch::edit;

        struct Drag
        {
            enum class Mode
            {
                None,
                Move,
                Resize,
            };
            Mode mode = Mode::None;
            Control control = Control::Count;
            Placement start;
            Overrides before;  // the arrangement the drag began on (its undo entry)
            Rect rect;         // the control's rectangle when the drag began
            ImVec2 origin;     // where the pointer went down, in points
            bool touch = false;  // (a finger: lift the picture clear of it)
            bool valid = true;
            edit::Attempt attempt;  // the last candidate, for the refusal's words
        };

        bool s_open = false;
        bool s_readOnly = false;
        Control s_selected = Control::Count;
        Overrides s_draft;
        Overrides s_initial;    // what the file had when the editor opened (dirty)
        Overrides s_lastValid;  // the last arrangement with nothing crossing
        edit::OverridesHistory s_history;
        Grid s_grid;
        Drag s_drag;
        Snap s_snapX, s_snapY;
        bool s_valid = true;  // the arrangement under the finger, right now

        // The line the drag last followed, so the haptics tick once a line and
        // not once a frame.
        bool s_guideX = false, s_guideY = false;
        float s_guideLineX = 0.0f, s_guideLineY = 0.0f;
        double s_guideAt = 0.0;

        // A tap in the same place picks the next control under the pointer.
        ImVec2 s_pickPoint(-1.0e6f, -1.0e6f);
        double s_pickAt = 0.0;

        // A nudge held down is one undo entry per burst, not one a frame.
        double s_nudgeAt = 0.0;
        float s_nudgeX = 0.0f, s_nudgeY = 0.0f;
        bool s_nudgeRefused = false;

        // What went wrong last: a refusal, a save error, an undo.
        std::string s_hint;
        double s_hintUntil = 0.0;
        bool s_hintSticky = false;
        Control s_flash = Control::Count;
        double s_flashUntil = 0.0;

        // The bar's own corner, as the player dragged it, and whether the
        // picture popup was open last frame (Esc inside it must close the popup,
        // not the editor: ImGui has already closed it by the time we ask).
        ImVec2 s_barOffset(0.0f, 0.0f);
        bool s_picturesOpen = false;
        // The arrangement when the size slider was taken hold of: the whole
        // slide is one undo entry.
        Overrides s_slideBefore;

        constexpr double kFlashSeconds = 0.6;
        constexpr double kHintSeconds = 3.0;
        constexpr double kPickSeconds = 1.2;
        constexpr float kPickSlopPoints = 8.0f;
        constexpr double kBurstSeconds = 0.45;

        ImU32 Alpha(ImU32 colour, float a)
        {
            const float value = std::clamp(float((colour >> IM_COL32_A_SHIFT) & 0xFF) * a, 0.0f, 255.0f);
            return (colour & ~IM_COL32_A_MASK) | (ImU32(value) << IM_COL32_A_SHIFT);
        }

        // The device's haptics, if the player wants them: a refusal is a dull
        // thump, a line taken a crisp tick, a control picked a light tap.
        // (platform::haptics is empty off iOS, so this is a no-op there.)
        void Buzz(const platform::haptics::Pattern& pattern)
        {
            if (!settings::GetBool(settings::Id::Vibration))
                return;
            platform::haptics::Tap(pattern);
        }

        void Hint(std::string text, double seconds = kHintSeconds, bool sticky = false)
        {
            s_hint = std::move(text);
            s_hintSticky = sticky;
            s_hintUntil = sticky ? 0.0 : ImGui::GetTime() + seconds;
        }

        // "RT would overlap Y", the control it would land on flashed red, and a
        // thump: a change that is not taken says why.
        void Refuse(const edit::Attempt& attempt)
        {
            const std::string text = edit::RefusalText(attempt);
            Hint(text.empty() ? std::string("That would not fit") : text);
            if (attempt.other != Control::Count)
            {
                s_flash = attempt.other;
                s_flashUntil = ImGui::GetTime() + kFlashSeconds;
            }
            Buzz(hid::haptic::kRefusedTap);
        }

        // A change taken: its undo entry is the arrangement before it, and the
        // player hears that something happened.
        bool Commit(const Overrides& wanted, const Overrides& before)
        {
            if (s_readOnly)
                return false;
            s_history.Push(before);
            s_draft = wanted;
            s_lastValid = wanted;
            s_valid = true;
            Buzz(hid::haptic::kButtonTap);
            return true;
        }

        // The same, for a change that has been tested: refused if it would put
        // one control on another.
        bool CommitAttempt(const edit::Attempt& attempt, const Overrides& before)
        {
            if (s_readOnly)
                return false;
            if (!attempt.ok)
            {
                Refuse(attempt);
                return false;
            }
            return Commit(attempt.overrides, before);
        }

        void Undo()
        {
            if (s_readOnly)
                return;
            Overrides before;
            if (!s_history.Undo(before))
                return;
            s_draft = s_lastValid = before;
            s_drag = Drag{};
            s_valid = true;
            Hint("Undone");
            Buzz(hid::haptic::kButtonTap);
        }

        bool Dirty()
        {
            return !edit::Same(s_draft, s_initial, 1.0e-3f);
        }

        // The pitch the grid cycles through, and its name.
        constexpr float kPitches[] = { 0.5f, 1.0f, 2.0f };

        const char* PitchName(float pitch)
        {
            return pitch == 0.5f ? "0.5 cqw" : pitch == 2.0f ? "2 cqw" : "1 cqw";
        }

        float NextPitch(float pitch)
        {
            for (size_t i = 0; i < std::size(kPitches); i++)
            {
                if (kPitches[i] == pitch)
                    return kPitches[(i + 1) % std::size(kPitches)];
            }
            return kPitches[1];
        }

        Control Pick(const Deck& deck, float u, float v)
        {
            return pick::PickAt(deck, u, v);
        }

        // The lines other controls offer one axis of a drag, in cqw.
        size_t LinesFor(const Deck& deck, Control self, bool across, float* into, size_t room)
        {
            return NeighbourLines(deck, self, across, into, room);
        }

        // One axis of snapping, its three anchors all the same edge (a resize
        // moves one edge; a move uses all three through Snapped).
        Snap SnapEdge(float edgeCqw, const float* lines, size_t count, const Canvas& canvas, const Grid& grid)
        {
            if (!grid.snap)
                return Snap{};
            return SnapTo({ edgeCqw, edgeCqw, edgeCqw, grid.pitch, lines, count, MagnetCqw(canvas, grid) });
        }
    }

    bool TouchDeckEditing()
    {
        return s_open;
    }

    void OpenTouchDeckEditor()
    {
        if (s_open)
            return;
        if (!video::GetPictureRect().Valid())
        {
            // Nothing has been presented yet: there is no picture to lay the
            // deck on, and nothing to see while editing.
            Toast("The game hasn't drawn a frame yet: try again in a moment.");
            return;
        }
        s_open = true;
        s_selected = Control::Count;
        s_draft = touchdeck::Current().overrides;
        s_initial = s_draft;
        s_lastValid = s_draft;
        s_history.Clear();
        s_grid = touchdeck::Current().grid;
        s_readOnly = touchdeck::Current().readOnly;
        s_drag = Drag{};
        s_valid = true;
        s_snapX = s_snapY = Snap{};
        s_hint.clear();
        s_hintSticky = false;
        s_flash = Control::Count;
        s_pickPoint = ImVec2(-1.0e6f, -1.0e6f);
        s_nudgeX = s_nudgeY = 0.0f;
        s_nudgeRefused = false;
        s_barOffset = ImVec2(0.0f, 0.0f);
        s_picturesOpen = false;
        if (s_readOnly)
            Hint("This touch-deck.toml is from a newer SpeedBreaker: it can be looked at, not changed.", 0.0, true);
    }

    void CloseTouchDeckEditor(bool save)
    {
        if (!s_open)
            return;
        if (save)
        {
            std::string error = touchdeck::Current().Save(s_draft);
            if (!error.empty())
            {
                // (Stay open: the player's work is still here.)
                fprintf(stderr, "[deck] %s\n", error.c_str());
                Hint(error, 0.0, true);
                return;
            }
            fprintf(stderr, "[deck] saved %s\n", touchdeck::Path().string().c_str());
            // The file's pictures may have changed with it (a control choosing
            // a picture the player supplied).
            deckart::ReloadPlayerPictures();
        }
        s_open = false;
        s_drag = Drag{};
        s_selected = Control::Count;
    }

    void DrawTouchDeckEditor()
    {
        if (!s_open)
            return;
        const Canvas canvas = CanvasNow();
        const Style style = StyleNow();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (display.x <= 0.0f || display.y <= 0.0f)
            return;

        const Deck plain = Layout(canvas, style);          // where the design puts them
        const Deck deck = Layout(canvas, style, s_draft);  // where the player has
        const ImU32 accent = AccentColour();
        const ImGuiStyle& gs = ImGui::GetStyle();
        const float unit = ImGui::GetFontSize();
        const ImGuiIO& io = ImGui::GetIO();
        const double now = ImGui::GetTime();
        const video::PictureRect picture = video::GetPictureRect();
        const auto toScreen = [&](float cu, float cv) {
            return ImVec2((picture.x + cu * picture.w) * display.x, (picture.y + cv * picture.h) * display.y);
        };

        // ---- The bar's place: the empty band between the top row's two groups,
        // which is the one strip of the picture no control stands on. It is
        // measured from the design's own layout and does not follow the drag, so
        // it does not wander while a control is being put under it.
        const float barHeight = ImGui::GetFrameHeight() + 6.0f;
        const float bandLeft = toScreen(std::max(plain.Of(Control::Back).Right(), plain.Of(Control::Start).Right()), 0.0f).x;
        const float bandRight = toScreen(std::min(plain.Of(Control::QuickRearView).x, plain.Of(Control::Camera).x), 0.0f).x;
        const float bandWidth = std::max(unit * 12.0f, bandRight - bandLeft - unit * 0.5f);

        // What the bar says about the control it has selected: its name, its
        // offset and its size. (Its name is the key it stands for where it has
        // one, so this stays short.)
        const Control shown = s_selected;
        char readout[72] = "";
        if (shown != Control::Count)
        {
            const Placement& p = s_draft.of[size_t(shown)];
            std::snprintf(readout, sizeof readout, "%s  %+.1f, %+.1f  %d%%", edit::NameOf(shown), double(p.dx),
                double(p.dy), int(std::lround(p.scale * 100.0f)));
        }
        else
        {
            std::snprintf(readout, sizeof readout, "Touch deck");
        }
        const float readoutWidth = ImGui::CalcTextSize(readout).x;
        const float sizeWidth = unit * 6.0f + ImGui::CalcTextSize("Size").x + gs.ItemInnerSpacing.x;
        const float pad = gs.FramePadding.x * 2.0f;
        const float gap = gs.ItemSpacing.x;
        const auto width = [&](const char* label) { return ImGui::CalcTextSize(label).x + pad; };

        // The bar never clips: what does not fit goes into "More", in this order
        // of importance -- Save and Cancel stay, then the name, then Undo, the
        // size, the switch and the pitch, the pictures, the resets.
        float room = bandWidth - readoutWidth - width("Save") - width("Cancel") - gap * 4.0f - width("More");
        const auto take = [&room, gap](float need) {
            if (room < need)
                return false;
            room -= need + gap;
            return true;
        };
        const bool showUndo = take(width("Undo"));
        const bool showSize = take(sizeWidth);
        const bool showSnap = take(width("Snap: off"));
        const bool showGrid = take(width("0.5 cqw"));
        const bool showPicture = take(width("Picture..."));
        const bool showReset = take(width("Reset"));
        const bool showResetAll = take(width("Reset all"));
        const bool overflow = !(showUndo && showSize && showSnap && showGrid && showPicture && showReset && showResetAll);
        if (!overflow)
            room += width("More") + gap;

        const float barWidth = std::clamp(bandWidth - room, unit * 12.0f, bandWidth);
        ImVec2 barPos(bandLeft + (bandWidth - barWidth) * 0.5f + s_barOffset.x, 4.0f + s_barOffset.y);
        barPos.x = std::clamp(barPos.x, 2.0f, std::max(2.0f, display.x - barWidth - 2.0f));
        barPos.y = std::clamp(barPos.y, 2.0f, std::max(2.0f, display.y - barHeight - 2.0f));
        const ImVec2 barMax(barPos.x + barWidth, barPos.y + barHeight);

        // ---- The surface: the whole screen, so a press anywhere on the picture
        // can pick a control. (The bar is a window of its own, made after this
        // one: it sits on top, so a press on it is not a press on the deck.)
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(display);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##deck-editor", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        ImGui::InvisibleButton("surface", display, ImGuiButtonFlags_MouseButtonLeft);
        const bool pressed = ImGui::IsItemActivated();
        const bool held = ImGui::IsItemActive();
        // (The button going up ends a drag even if the item was deactivated by
        // something else on the way -- the drag began here, so its end is
        // ours to take.)
        const bool released = ImGui::IsItemDeactivated() ||
            (s_drag.mode != Drag::Mode::None && ImGui::IsMouseReleased(ImGuiMouseButton_Left));
        ImDrawList* dl = ImGui::GetWindowDrawList();

        const ImVec2 mouse = ImGui::GetMousePos();
        const bool onBar = mouse.x >= barPos.x && mouse.x <= barMax.x && mouse.y >= barPos.y && mouse.y <= barMax.y;
        const float u = (mouse.x / display.x - picture.x) / picture.w;
        const float v = (mouse.y / display.y - picture.y) / picture.h;
        const bool touch = io.MouseSource == ImGuiMouseSource_TouchScreen;
        const bool popup = s_picturesOpen;  // (last frame's: see the comment on it)

        // ---- A press: the size's handle first (it is on top of everything), then
        // the control under it -- the next one down if the same place is tapped
        // again, so a covered control can still be reached.
        if (pressed && !onBar && !popup)
        {
            s_snapX = s_snapY = Snap{};
            Control chosen = Control::Count;
            bool handle = false;
            if (s_selected != Control::Count && !s_readOnly)
            {
                const Rect drawn = DrawnOf(deck, s_selected);
                const ImVec2 corner = toScreen(drawn.Right(), drawn.Bottom());
                const float reach = touch ? 24.0f : 12.0f;
                if (std::hypot(mouse.x - corner.x, mouse.y - corner.y) <= reach)
                {
                    chosen = s_selected;
                    handle = true;
                }
            }
            if (chosen == Control::Count)
            {
                const bool again = now - s_pickAt <= kPickSeconds &&
                    std::hypot(mouse.x - s_pickPoint.x, mouse.y - s_pickPoint.y) <= kPickSlopPoints;
                chosen = again ? pick::NextUnder(deck, u, v, s_selected) : Pick(deck, u, v);
                s_pickPoint = mouse;
                s_pickAt = now;
                if (chosen != s_selected && chosen != Control::Count)
                    Buzz(hid::haptic::kPickTap);
            }
            s_selected = chosen;
            s_drag = Drag{};
            s_valid = true;
            if (chosen != Control::Count)
            {
                s_drag.control = chosen;
                s_drag.before = s_draft;
                s_drag.start = s_draft.of[size_t(chosen)];
                s_drag.rect = Moved(plain.Of(chosen), s_drag.start, canvas, style);
                s_drag.origin = mouse;
                s_drag.touch = touch;
                if (!s_readOnly)
                    s_drag.mode = handle ? Drag::Mode::Resize : Drag::Mode::Move;
                s_lastValid = s_draft;
            }
        }

        // ---- The drag: move (all three anchors snap) or size (the corner's two
        // edges snap, the opposite corner pinned). A press that has not moved is
        // a pick, not a drag: the grid would otherwise pull the control onto its
        // nearest line by a tap alone.
        const float moved = std::hypot(mouse.x - s_drag.origin.x, mouse.y - s_drag.origin.y);
        const bool dragging = held && s_drag.mode != Drag::Mode::None && s_drag.control != Control::Count &&
            moved > unit * 0.2f;
        if (dragging)
        {
            const Control control = s_drag.control;
            const Rect base = plain.Of(control);
            constexpr size_t kLines = 3 * (kControls - 1);
            float linesX[kLines], linesY[kLines];
            const size_t countX = LinesFor(deck, control, true, linesX, std::size(linesX));
            const size_t countY = LinesFor(deck, control, false, linesY, std::size(linesY));
            Placement placement = s_drag.start;

            if (s_drag.mode == Drag::Mode::Move)
            {
                Rect want = s_drag.rect;
                want.x += (mouse.x - s_drag.origin.x) / (display.x * picture.w);
                want.y += (mouse.y - s_drag.origin.y) / (display.y * picture.h);
                want = ClampedToCanvas(want, canvas);
                want = Snapped(want, canvas, s_grid, linesX, countX, linesY, countY, &s_snapX, &s_snapY);
                want = ClampedToCanvas(want, canvas);
                Placed(placement, want, base, canvas, style);
            }
            else
            {
                // The pointer's own edge, snapped, with the top left held.
                const Snap right = SnapEdge(u / canvas.uPerCqw, linesX, countX, canvas, s_grid);
                const Snap bottom = SnapEdge(v / canvas.vPerCqw, linesY, countY, canvas, s_grid);
                s_snapX = right;
                s_snapY = bottom;
                const float leftCqw = s_drag.rect.x / canvas.uPerCqw;
                const float topCqw = s_drag.rect.y / canvas.vPerCqw;
                const float widthCqw = std::max(1.0f, u / canvas.uPerCqw + right.delta - leftCqw);
                const float heightCqw = std::max(1.0f, v / canvas.vPerCqw + bottom.delta - topCqw);
                const float scale = std::clamp(
                    std::max(widthCqw / (base.w / canvas.uPerCqw), heightCqw / (base.h / canvas.vPerCqw)),
                    kMinPlacement, kMaxPlacement);
                Rect box{ s_drag.rect.x, s_drag.rect.y, base.w * scale, base.h * scale };
                box = ClampedToCanvas(box, canvas);
                Placed(placement, box, base, canvas, style);
            }
            // One test for the whole editor: clamped into the picture, laid out,
            // and refused if it lands on a neighbour.
            s_drag.attempt = edit::Evaluate(canvas, style, s_drag.before, control, placement);
            s_draft = s_drag.attempt.overrides;  // (the refused position is shown, in red)
            s_drag.valid = s_drag.attempt.ok;
            s_valid = s_drag.valid;
            if (s_drag.valid)
                s_lastValid = s_draft;

            // A line taken is felt: once a line, not once a frame.
            const bool guideX = s_snapX.moved && s_snapX.guide, guideY = s_snapY.moved && s_snapY.guide;
            const float lineX = guideX ? s_snapX.line : 0.0f, lineY = guideY ? s_snapY.line : 0.0f;
            const bool different = guideX != s_guideX ||
                (guideX && std::fabs(lineX - s_guideLineX) > 1.0e-3f) || guideY != s_guideY ||
                (guideY && std::fabs(lineY - s_guideLineY) > 1.0e-3f);
            if (different && now - s_guideAt > 0.08)
            {
                s_guideAt = now;
                s_guideX = guideX;
                s_guideLineX = lineX;
                s_guideY = guideY;
                s_guideLineY = lineY;
                if (guideX || guideY)
                    Buzz(hid::haptic::kGuideTick);
            }
        }

        // ---- Release: a drop that would put one control on another is not
        // taken -- the last arrangement that stood (this drag's, if it is the
        // design's own position then that) stays, and the player is told why.
        if (released)
        {
            if (s_drag.mode != Drag::Mode::None && s_drag.control != Control::Count)
            {
                if (s_drag.valid)
                {
                    if (!edit::Same(s_draft, s_drag.before, 1.0e-4f))
                        Commit(s_draft, s_drag.before);
                }
                else
                {
                    s_draft = s_lastValid;
                    Refuse(s_drag.attempt);
                }
            }
            s_valid = true;
            s_drag = Drag{};
            // (The lines the last drag followed go with it: they are that
            // drag's, not the layout's.)
            s_snapX = s_snapY = Snap{};
        }

        // ---- What is drawn: the grid, the deck, the lines a drag followed, and
        // the red of a position that will not be taken.
        if (s_grid.snap && s_grid.pitch > 0.0f && s_drag.mode != Drag::Mode::None)
        {
            const float step = s_grid.pitch;
            // (On a small screen half-cqw lines are a millimetre apart: draw
            // every other one rather than a wash of grey.)
            const int every = std::max(1, int(std::ceil(unit * 0.45f / (step * picture.w * display.x * 0.01f))));
            const ImU32 faint = IM_COL32(255, 255, 255, 26);
            for (int i = 1; i * step * every < 100.0f; i++)
            {
                const float x = i * step * every * canvas.uPerCqw;
                dl->AddLine(toScreen(x, 0.0f), toScreen(x, 1.0f), faint);
            }
            for (int i = 1; i * step * every < canvas.HeightCqw(); i++)
            {
                const float y = i * step * every * canvas.vPerCqw;
                dl->AddLine(toScreen(0.0f, y), toScreen(1.0f, y), faint);
            }
        }

        // The picture of the control under the finger is up above it on a touch
        // screen, and a dashed outline marks where it will land.
        const Control lifted = dragging && s_drag.touch ? s_drag.control : Control::Count;
        DrawDeck(dl, deck, Sample{}, style, canvas, s_draft, s_selected, accent, lifted);
        if (lifted != Control::Count)
        {
            // Above the finger, or below it where the picture's top is too near
            // the screen's for that, and never past an edge.
            const Rect drawn = DrawnOf(deck, lifted);
            const ImVec2 top = toScreen(drawn.x, drawn.y);
            const ImVec2 bottom = toScreen(drawn.Right(), drawn.Bottom());
            const float lift = std::clamp(display.y * 0.12f, 36.0f, 64.0f);
            float dy = -lift;
            if (top.y + dy < 4.0f)
                dy = lift;
            dy = std::clamp(dy, 4.0f - top.y, display.y - 4.0f - bottom.y);
            const float dx = std::clamp(0.0f, 4.0f - top.x, display.x - 4.0f - bottom.x);
            DrawDeckLift(dl, deck, style, canvas, deck.Of(lifted), lifted, dx, dy, accent);
            // The size being set, where the eye already is.
            if (s_drag.mode == Drag::Mode::Resize)
            {
                char label[16];
                std::snprintf(label, sizeof label, "%d%%", int(std::lround(s_draft.of[size_t(lifted)].scale * 100.0f)));
                const ImVec2 measured = ImGui::CalcTextSize(label);
                const ImVec2 at(mouse.x + unit * 0.8f, mouse.y - unit * 0.8f - lift * 0.5f);
                dl->AddRectFilled(ImVec2(at.x - 4.0f, at.y - 2.0f), ImVec2(at.x + measured.x + 4.0f, at.y + measured.y + 2.0f),
                    IM_COL32(10, 9, 8, 200), 4.0f);
                dl->AddText(at, IM_COL32(255, 255, 255, 235), label);
            }
        }
        if (s_snapX.moved)
        {
            const float x = s_snapX.line * canvas.uPerCqw;
            dl->AddLine(toScreen(x, 0.0f), toScreen(x, 1.0f), Alpha(accent, s_snapX.guide ? 1.0f : 0.5f), 1.5f);
        }
        if (s_snapY.moved)
        {
            const float y = s_snapY.line * canvas.vPerCqw;
            dl->AddLine(toScreen(0.0f, y), toScreen(1.0f, y), Alpha(accent, s_snapY.guide ? 1.0f : 0.5f), 1.5f);
        }
        if (!s_valid && s_drag.control != Control::Count)
        {
            // What is about to be refused: the control's rectangle in red.
            const ImVec2 a = toScreen(deck.Of(s_drag.control).x, deck.Of(s_drag.control).y);
            const ImVec2 b = toScreen(deck.Of(s_drag.control).Right(), deck.Of(s_drag.control).Bottom());
            dl->AddRect(a, b, IM_COL32(230, 90, 80, 220), unit * 0.3f, 0, 2.0f);
        }
        if (s_flash != Control::Count && now < s_flashUntil)
        {
            // The control a refusal landed on, so the words have a place.
            const ImVec2 a = toScreen(deck.Of(s_flash).x, deck.Of(s_flash).y);
            const ImVec2 b = toScreen(deck.Of(s_flash).Right(), deck.Of(s_flash).Bottom());
            const float fade = float(std::clamp((s_flashUntil - now) / kFlashSeconds, 0.0, 1.0));
            dl->AddRect(a, b, Alpha(IM_COL32(230, 90, 80, 255), fade), unit * 0.3f, 0, 2.5f);
        }
        ImGui::End();
        ImGui::PopStyleVar(2);

        // ---- The bar: one row, at the top of the picture where no control
        // stands (the bottom is the pedals', and a bar there covers them at any
        // size). It can be dragged aside by its empty part, in case a control
        // was put under it.
        ImGui::SetNextWindowPos(barPos);
        ImGui::SetNextWindowSize(ImVec2(barWidth, barHeight));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(gs.WindowPadding.x, 3.0f));
        ImGui::Begin("##deck-palette", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavInputs);
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !ImGui::IsAnyItemActive() &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            s_barOffset.x += io.MouseDelta.x, s_barOffset.y += io.MouseDelta.y;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(readout);
        const bool edits = !s_readOnly;
        if (showUndo)
        {
            ImGui::SameLine();
            ImGui::BeginDisabled(!edits || !s_history.CanUndo());
            if (ImGui::Button("Undo"))
                Undo();
            ImGui::EndDisabled();
        }
        if (showSnap)
        {
            ImGui::SameLine();
            ImGui::BeginDisabled(!edits);
            if (ImGui::Button(s_grid.snap ? "Snap: on" : "Snap: off"))
            {
                s_grid.snap = !s_grid.snap;
                const std::string message = touchdeck::Current().SetGrid(s_grid);
                if (!message.empty())
                    Hint(message);
            }
            ImGui::EndDisabled();
        }
        if (showGrid)
        {
            ImGui::SameLine();
            ImGui::BeginDisabled(!edits);
            if (ImGui::Button(PitchName(s_grid.pitch)))
            {
                s_grid.pitch = NextPitch(s_grid.pitch);
                const std::string message = touchdeck::Current().SetGrid(s_grid);
                if (!message.empty())
                    Hint(message);
            }
            ImGui::EndDisabled();
        }
        if (showPicture)
        {
            ImGui::SameLine();
            ImGui::BeginDisabled(!edits || s_selected == Control::Count);
            if (ImGui::Button("Picture..."))
                ImGui::OpenPopup("pictures");
            ImGui::EndDisabled();
        }
        if (showSize)
        {
            ImGui::SameLine();
            const Control control = s_selected;
            float scale = control == Control::Count ? 1.0f : s_draft.of[size_t(control)].scale;
            ImGui::SetNextItemWidth(unit * 6.0f);
            ImGui::BeginDisabled(!edits || control == Control::Count);
            // (The slider goes where the deck can follow: a step that would land
            // on a neighbour is refused at once, and the handle stays at the
            // last size the deck took rather than at the refused one.)
            if (ImGui::SliderFloat("Size", &scale, kMinPlacement, kMaxPlacement, "%.2f"))
            {
                Placement candidate = s_draft.of[size_t(control)];
                candidate.scale = scale;
                const edit::Attempt attempt = edit::Evaluate(canvas, style, s_draft, control, candidate);
                if (attempt.ok)
                    s_draft = s_lastValid = attempt.overrides;
                else
                    Refuse(attempt);
            }
            if (ImGui::IsItemActivated())
                s_slideBefore = s_draft;
            if (ImGui::IsItemDeactivatedAfterEdit() && !edit::Same(s_draft, s_slideBefore, 1.0e-4f))
            {
                // One undo entry for the whole slide, not one a frame.
                s_history.Push(s_slideBefore);
                Buzz(hid::haptic::kButtonTap);
            }
            ImGui::EndDisabled();
        }
        if (showReset)
        {
            ImGui::SameLine();
            ImGui::BeginDisabled(!edits || s_selected == Control::Count);
            if (ImGui::Button("Reset"))
            {
                const edit::Attempt attempt =
                    edit::Evaluate(canvas, style, s_draft, s_selected, Placement{});
                CommitAttempt(attempt, s_draft);
            }
            ImGui::EndDisabled();
        }
        if (showResetAll)
        {
            ImGui::SameLine();
            ImGui::BeginDisabled(!edits);
            if (ImGui::Button("Reset all"))
            {
                Overrides design;
                // (The design's own layout cannot cross itself, so this is taken
                // as it is -- but it still goes through the history.)
                if (Commit(design, s_draft))
                {
                    s_selected = Control::Count;
                    Hint("The design's layout");
                }
            }
            ImGui::EndDisabled();
        }
        if (overflow)
        {
            ImGui::SameLine();
            if (ImGui::Button("More"))
                ImGui::OpenPopup("more");
            if (ImGui::BeginPopup("more"))
            {
                ImGui::TextDisabled("Not on the bar:");
                ImGui::Separator();
                if (!showUndo)
                {
                    ImGui::BeginDisabled(!edits || !s_history.CanUndo());
                    if (ImGui::Button("Undo"))
                        Undo();
                    ImGui::EndDisabled();
                }
                if (!showSnap)
                {
                    ImGui::BeginDisabled(!edits);
                    if (ImGui::Button(s_grid.snap ? "Snap: on" : "Snap: off"))
                    {
                        s_grid.snap = !s_grid.snap;
                        const std::string message = touchdeck::Current().SetGrid(s_grid);
                        if (!message.empty())
                            Hint(message);
                    }
                    ImGui::EndDisabled();
                }
                if (!showGrid)
                {
                    ImGui::BeginDisabled(!edits);
                    if (ImGui::Button(PitchName(s_grid.pitch)))
                    {
                        s_grid.pitch = NextPitch(s_grid.pitch);
                        const std::string message = touchdeck::Current().SetGrid(s_grid);
                        if (!message.empty())
                            Hint(message);
                    }
                    ImGui::EndDisabled();
                }
                if (!showPicture)
                {
                    ImGui::BeginDisabled(!edits || s_selected == Control::Count);
                    if (ImGui::Button("Picture..."))
                        ImGui::OpenPopup("pictures");
                    ImGui::EndDisabled();
                }
                if (!showSize)
                {
                    const Control control = s_selected;
                    float scale = control == Control::Count ? 1.0f : s_draft.of[size_t(control)].scale;
                    ImGui::BeginDisabled(!edits || control == Control::Count);
                    if (ImGui::SliderFloat("Size", &scale, kMinPlacement, kMaxPlacement, "%.2f"))
                    {
                        Placement candidate = s_draft.of[size_t(control)];
                        candidate.scale = scale;
                        const edit::Attempt attempt = edit::Evaluate(canvas, style, s_draft, control, candidate);
                        if (attempt.ok)
                            s_draft = s_lastValid = attempt.overrides;
                        else
                            Refuse(attempt);
                    }
                    if (ImGui::IsItemActivated())
                        s_slideBefore = s_draft;
                    if (ImGui::IsItemDeactivatedAfterEdit() && !edit::Same(s_draft, s_slideBefore, 1.0e-4f))
                    {
                        s_history.Push(s_slideBefore);
                        Buzz(hid::haptic::kButtonTap);
                    }
                    ImGui::EndDisabled();
                }
                if (!showReset)
                {
                    ImGui::BeginDisabled(!edits || s_selected == Control::Count);
                    if (ImGui::Button("Reset"))
                    {
                        const edit::Attempt attempt =
                            edit::Evaluate(canvas, style, s_draft, s_selected, Placement{});
                        CommitAttempt(attempt, s_draft);
                    }
                    ImGui::EndDisabled();
                }
                if (!showResetAll)
                {
                    ImGui::BeginDisabled(!edits);
                    if (ImGui::Button("Reset all"))
                    {
                        Overrides design;
                        if (Commit(design, s_draft))
                        {
                            s_selected = Control::Count;
                            Hint("The design's layout");
                        }
                    }
                    ImGui::EndDisabled();
                }
                ImGui::EndPopup();
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!edits || !Dirty());
        if (ImGui::Button("Save"))
            CloseTouchDeckEditor(true);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            CloseTouchDeckEditor(false);

        // The pictures: a popup, so that looking through them does not cover the
        // deck while it is being arranged.
        if (ImGui::BeginPopup("pictures"))
        {
            ImGui::TextDisabled(s_selected == Control::Count ? "Pick a control first." : "The control's picture:");
            const float side = ImGui::GetFrameHeight() * 1.6f;
            ImGui::BeginChild("pictures-grid", ImVec2(side * 6.5f, side * 3.6f), ImGuiChildFlags_Borders);
            for (size_t i = 0; i < art::kCount; i++)
            {
                const art::Info& info = art::InfoOf(art::Id(i));
                const bool player = info.sheet != 0;
                const bool ready = player ? deckart::PlayerReady() : deckart::Ready();
                if (!ready)
                    continue;
                ImGui::PushID(int(i));
                const art::Id wearing = s_selected == Control::Count ? art::Id::Count
                    : (s_draft.of[size_t(s_selected)].icon == art::Id::Count ? SpecOf(s_selected).art
                                                                             : s_draft.of[size_t(s_selected)].icon);
                const bool chosen = wearing == art::Id(i);
                if (chosen)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                if (ImGui::ImageButton(art::kArtKey[i], player ? deckart::PlayerAtlas() : deckart::Atlas(),
                        ImVec2(side, side), ImVec2(info.u0, info.v0), ImVec2(info.u1, info.v1)) &&
                    s_selected != Control::Count && !s_readOnly)
                {
                    // A picture is a change like any other, and is tested like
                    // one: a bigger picture is a bigger rectangle.
                    Placement candidate = s_draft.of[size_t(s_selected)];
                    candidate.icon = art::Id(i);
                    const edit::Attempt attempt = edit::Evaluate(canvas, style, s_draft, s_selected, candidate);
                    if (CommitAttempt(attempt, s_draft))
                        ImGui::CloseCurrentPopup();
                }
                if (chosen)
                    ImGui::PopStyleColor();
                ImGui::PopID();
                if ((i + 1) % 6 != 0)
                    ImGui::SameLine();
            }
            ImGui::EndChild();
            if (ImGui::Button("Reload pictures"))
                deckart::ReloadPlayerPictures();
            ImGui::SameLine();
            ImGui::TextDisabled("Your own: %s", touchdeck::PicturesPath().filename().string().c_str());
            ImGui::EndPopup();
        }
        ImGui::End();
        ImGui::PopStyleVar();

        // ---- The hint, under the bar: a refusal, an undo, a save that could not
        // be written. It has no input of its own, so it never stands between a
        // finger and the deck.
        if (!s_hint.empty() && (s_hintSticky || now < s_hintUntil))
        {
            const ImVec2 measured = ImGui::CalcTextSize(s_hint.c_str());
            const float fade = s_hintSticky ? 1.0f : float(std::clamp((s_hintUntil - now) / 0.6, 0.0, 1.0));
            const ImVec2 at(barPos.x, barMax.y + 2.0f);
            dl = ImGui::GetForegroundDrawList();
            dl->AddRectFilled(ImVec2(at.x - 4.0f, at.y - 2.0f),
                ImVec2(at.x + measured.x + 4.0f, at.y + measured.y + 2.0f), Alpha(IM_COL32(10, 9, 8, 205), fade), 4.0f);
            dl->AddText(at, Alpha(IM_COL32(255, 255, 255, 240), fade), s_hint.c_str());
        }
        else if (!s_hint.empty() && !s_hintSticky)
        {
            s_hint.clear();
        }

        // ---- Keys and the pad. (Not while the picture popup is open: Esc there
        // closes the popup, and Enter picks a picture.)
        if (popup)
        {
            s_picturesOpen = ImGui::IsPopupOpen("pictures");
            return;
        }
        const bool ctrl = io.KeyCtrl || io.KeySuper;
        // (U as well as the usual: the scripted runs have no modifier keys, and
        // a keyboard without a command key still has a U.)
        if ((ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) || ImGui::IsKeyPressed(ImGuiKey_U, false))
            Undo();
        if (ImGui::IsKeyPressed(ImGuiKey_S, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false))
        {
            if (!s_readOnly)
            {
                s_grid.snap = !s_grid.snap;
                const std::string message = touchdeck::Current().SetGrid(s_grid);
                if (!message.empty())
                    Hint(message);
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false))  // (Y: undo, on a pad)
            Undo();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))
        {
            CloseTouchDeckEditor(false);
            s_picturesOpen = false;
            return;
        }
        // Enter (or A) keeps it -- the same as the bar's Save, so a keyboard or
        // a pad can finish without reaching for a button.
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false))
        {
            if (Dirty() && !s_readOnly)
            {
                CloseTouchDeckEditor(true);
                s_picturesOpen = false;
                return;
            }
        }

        // A nudge: the arrows and the D-pad, whether or not the grid is on (a
        // pad has no pointer, and neither has a keyboard on a phone). One undo
        // entry per burst -- a key held down is one change, not sixty.
        if (s_selected != Control::Count && !s_readOnly && !io.WantTextInput)
        {
            float nx = 0.0f, ny = 0.0f;
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft, true))
                nx -= 1.0f;
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight, true))
                nx += 1.0f;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp, true))
                ny -= 1.0f;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown, true))
                ny += 1.0f;
            if (nx != 0.0f || ny != 0.0f)
            {
                // (The step is a cqw of the picture; a placement is in cqw of
                // the deck's own size, so it is divided by the factor -- else a
                // step would not be a step on a phone.)
                const float step = edit::NudgeStep(s_grid, style.Factor(canvas));
                Placement candidate = s_draft.of[size_t(s_selected)];
                candidate.dx += nx * step;
                candidate.dy += ny * step;
                const edit::Attempt attempt = edit::Evaluate(canvas, style, s_draft, s_selected, candidate);
                const bool burst = now - s_nudgeAt <= kBurstSeconds && nx == s_nudgeX && ny == s_nudgeY;
                s_nudgeAt = now;
                s_nudgeX = nx;
                s_nudgeY = ny;
                if (attempt.ok)
                {
                    if (!burst)
                        s_history.Push(s_draft);
                    s_draft = s_lastValid = attempt.overrides;
                    s_valid = true;
                    s_nudgeRefused = false;
                }
                else if (!s_nudgeRefused)
                {
                    // (Once per refusal, not once a frame of a held key.)
                    s_nudgeRefused = true;
                    Refuse(attempt);
                }
            }
            else
            {
                s_nudgeX = s_nudgeY = 0.0f;
                s_nudgeRefused = false;
            }
        }
        s_picturesOpen = ImGui::IsPopupOpen("pictures");
    }
}
