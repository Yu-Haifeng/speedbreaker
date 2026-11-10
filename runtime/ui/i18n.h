// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The language of SpeedBreaker's own overlay (the settings menu, the
// performance overlay and the dialogs): English, or Simplified Chinese.
// Settings > Advanced > Overlay Language chooses System (this device's
// language), English or 简体中文; System is the default.
//
// English source strings are the keys: i18n::Tr() returns the Chinese text
// for a known string, or the English one when there is no translation or
// English is selected. i18n::Format() does the same and fills {} arguments
// (std::format syntax). A translation is looked up only in the selected
// language; an unknown string stays in English, so the overlay is never
// blank while a translation is missing.
#pragma once
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace ui::i18n
{
    enum class Language : uint8_t
    {
        English,
        ChineseSimplified,
    };

    // settings::Id::OverlayLanguage's values (the [advanced] table stores
    // the tokens "system", "english" and "chinese").
    enum class Choice : int32_t
    {
        System = 0,
        English = 1,
        ChineseSimplified = 2,
    };

    // The language a locale tag names ("zh-CN", "zh_CN.UTF-8", "en-US"):
    // Simplified Chinese for a zh tag without a traditional script or
    // region, English for everything else (this build has no traditional
    // Chinese translation; those players can choose 简体中文 themselves).
    Language FromLocaleTag(std::string_view tag);

    // What SDL reported at startup (ui.cpp): the preferred locale's language
    // and country. Empty: the environment's LANG/LC_ALL, then English.
    void SetSystemLocale(std::string_view tag);
    std::string_view SystemLocaleTag();
    Language SystemLanguage();

    // Settings > Advanced > Overlay Language. System follows
    // SystemLanguage(); a change takes effect on the next frame.
    void Use(int32_t choice);
    // Tests and the platform's own switch: select a language directly.
    void Select(Language language);
    Language Current();

    // "en" or "zh-Hans", for the log.
    const char* Tag();

    // The selected language's text for an English source string. Both are
    // views: the caller's string (when there is no translation) or a literal
    // in the table.
    std::string_view Tr(std::string_view english);
    // The same, null-terminated, for ImGui calls that take a const char*.
    // Unlike Tr's view, the pointer stays valid for the process: one stable
    // copy per source string and language is kept, so a dynamic key, or a
    // language switch while a dialog holds a label, can't dangle.
    const char* C(const char* english);

    // A translated format string with its {} arguments filled in.
    template <typename... Args>
    std::string Format(std::string_view english, Args&&... args)
    {
        const std::string pattern(Tr(english));
        return std::vformat(pattern, std::make_format_args(args...));
    }

    // Whether a Chinese translation exists (tests; the UI falls back to
    // English when it doesn't).
    bool HasTranslation(std::string_view english);
}
