// Tests for the overlay's language (ui/i18n.h): System follows the locale,
// English and Simplified Chinese select directly, unknown strings fall back
// to English, and translations fill {} arguments.
#include <ui/i18n.h>
#include <video/refresh_notice.h>

#include <cstdio>
#include <string>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main()
{
    using namespace ui::i18n;

    CHECK(FromLocaleTag("zh_CN.UTF-8") == Language::ChineseSimplified, "zh_CN");
    CHECK(FromLocaleTag("zh-Hant-TW") == Language::ChineseSimplified, "zh-Hant");
    CHECK(FromLocaleTag("en-US") == Language::English, "en-US");
    CHECK(FromLocaleTag("fr_FR") == Language::English, "fr");
    CHECK(FromLocaleTag("") == Language::English, "empty");

    SetSystemLocale("zh-CN");
    CHECK(SystemLanguage() == Language::ChineseSimplified, "system zh");
    Use(int32_t(Choice::System));
    CHECK(Current() == Language::ChineseSimplified, "System follows the device");
    CHECK(std::string(Tag()) == "zh-Hans", "%s", Tag());

    SetSystemLocale("en-GB");
    Use(int32_t(Choice::System));
    CHECK(Current() == Language::English, "System follows an English device");

    Use(int32_t(Choice::ChineseSimplified));
    CHECK(Current() == Language::ChineseSimplified, "Chinese, directly");
    CHECK(Tr("Settings") == "设置", "Settings: %.*s", int(Tr("Settings").size()), Tr("Settings").data());
    CHECK(std::string(C("Close")) == "关闭", "null-terminated");
    CHECK(Format("Reset every {} setting to its default?", Tr("Display")) == "将“显示”页的每项设置恢复为默认值？",
        "%s", Format("Reset every {} setting to its default?", Tr("Display")).c_str());
    CHECK(Tr("a string no translation has") == "a string no translation has", "fallback");
    CHECK(HasTranslation("Settings") && !HasTranslation("a string no translation has"), "has translation");
    CHECK(Format("{:.0f} fps", 59.6) == "60 帧/秒", "FPS format key and translation: %s", Format("{:.0f} fps", 59.6).c_str());
    CHECK(Format("Update to {}", "v0.3.0") == "更新到 v0.3.0", "dynamic update footer");

    using Notice = video::RefreshNotice;
    for (Notice::Platform platform : { Notice::Platform::GameMode, Notice::Platform::Desktop })
    {
        Notice::Inputs in;
        in.platform = platform;
        in.modeHz = 90;
        CHECK(HasTranslation(Notice::Pattern(in)), "refresh notice pattern: %s", Notice::Pattern(in));
        const std::string text = Format(Notice::Pattern(in), Notice::Key(in.modeHz));
        CHECK(text.find("90 Hz") != std::string::npos && text.find("卡顿") != std::string::npos, "refresh notice: %s", text.c_str());
        in.externalScreen = true;
        CHECK(HasTranslation(Notice::Pattern(in)), "external-screen refresh notice pattern");
    }

    Use(int32_t(Choice::English));
    CHECK(Current() == Language::English, "English, directly");
    CHECK(Tr("Settings") == "Settings", "English source");
    CHECK(std::string(Tag()) == "en", "%s", Tag());

    // The settings value's tokens and values agree with i18n::Choice.
    CHECK(int32_t(Choice::System) == 0 && int32_t(Choice::English) == 1 && int32_t(Choice::ChineseSimplified) == 2,
        "settings values");
    Use(999);
    CHECK(Current() == SystemLanguage(), "an unknown setting value: System");

    if (g_failures)
    {
        printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("i18n_test: all checks passed\n");
    return 0;
}
