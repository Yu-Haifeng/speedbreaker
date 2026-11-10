// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See i18n.h.
#include "i18n.h"

#include <update/release_names.h>  // SB_APP_NAME, in the two GitHub sentences

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <unordered_map>

namespace ui::i18n
{
    namespace
    {
        struct Entry
        {
            std::string_view english;
            std::string_view chinese;
        };

        // Simplified Chinese. The English string is the key: the translation
        // is used only when it is here, so an untranslated string stays
        // readable in English.
        constexpr Entry kZhHans[] = {
            // Settings menu: pages, chrome and dialogs.
            {"Settings", "设置"},
            {"Display", "显示"},
            {"Graphics", "画面"},
            {"Audio", "音频"},
            {"Input", "输入"},
            {"Controls", "按键"},
            {"Wheel", "方向盘"},
            {"Game", "游戏"},
            {"Advanced", "高级"},
            {"View + Menu on a controller, or tap with three fingers", "手柄 View + Menu，或三指点按"},
            {"F1 or Cmd+, / Back + Start", "F1 或 Cmd+, / Back + Start"},
            {"F1 / Back + Start", "F1 / Back + Start"},
            {"Reset page", "重置本页"},
            {"Close", "关闭"},
            {"Quit Game", "退出游戏"},
            {"A: set key   Y: clear   X: default   LB RB: page   B: close", "A：设置按键   Y：清除   X：默认   LB RB：翻页   B：关闭"},
            {"D-pad: choose, change   LB RB: page   B: close", "方向键：选择、修改   LB RB：翻页   B：关闭"},
            {"LB RB: page   B: close", "LB RB：翻页   B：关闭"},
            {"B: close", "B：关闭"},
            {"Quit SpeedBreaker? Progress since your last save will be lost.", "退出 SpeedBreaker？上次保存后的进度将丢失。"},
            {"Quit", "退出"},
            {"Cancel", "取消"},
            {"Reset", "重置"},
            {"Reset every {} setting to its default?", "将“{}”页的每项设置恢复为默认值？"},
            {"Press a key", "请按键"},
            {"none", "无"},
            {"That key", "该按键"},
            {"{} can't be used here. Press another key; Esc or B cancels.", "{} 不能在这里使用。请按其他键；Esc 或 B 取消。"},
            {"Press the key for {}. Esc or B cancels.", "请设置“{}”的按键。Esc 或 B 取消。"},
            {"Enter sets the key, Backspace clears it, Delete restores the row.{}{}{}", "Enter 设置按键，Backspace 清除，Delete 恢复本行。{}{}{}"},
            {"  Also: ", "  同时用于："},
            {"Always on in Game Mode: Steam sets the resolution (Properties > Game Resolution).", "游戏模式下始终开启：由 Steam 设置分辨率（属性 > 游戏分辨率）。"},
            {"The window is {} for now. The game starts {}, as set here.", "窗口目前为{}。游戏启动时为{}，与此处设置一致。"},
            {"{} sets this for this run: {}. The choice here is saved for later runs.", "{} 仅覆盖本次运行：{}。此处选择会保存供以后使用。"},
            {"Takes effect the next time the game starts.", "下次启动游戏时生效。"},
            {"Changes take effect the next time the game starts.", "更改将在下次启动游戏时生效。"},
            {"Off while the game runs at 30 fps (Frame Rate).", "游戏以 30 fps（帧率）运行时关闭。"},
            {"This screen shows 60 pictures a second: nothing to add here.", "此屏幕每秒显示 60 帧，此处无需额外插帧。"},
            {"In use: {}.", "当前使用：{}。"},
            {"{} is connected.", "已连接 {}。"},
            {"{}: this system can't drive its motors, so this does nothing. Steering, the pedals and the wheel's buttons still work.", "{}：此系统无法驱动其马达，因此此设置无效。转向、踏板和方向盘按键仍可使用。"},
            {"Saving...", "正在保存..."},
            {"Checking...", "正在检查..."},
            {"Couldn't check (offline)", "无法检查（离线）"},
            {"Save Bug Report", "保存错误报告"},
            {"Check for Updates", "检查更新"},
            {"Update to {}", "更新到 {}"},
            {"SpeedBreaker {}", "SpeedBreaker {}"},
            {"Copyright (C) 2026 project(u) and SpeedBreaker contributors. Free software under the GPL-3.0-or-later, with no warranty; third-party licenses come with the game. Not affiliated with Electronic Arts or Valve.",
                "Copyright (C) 2026 project(u) 与 SpeedBreaker 贡献者。本软件为 GPL-3.0-or-later 自由软件，不提供任何担保；第三方许可证随游戏提供。与 Electronic Arts 或 Valve 无关联。"},
            {"Saves the recent logs, crash and hang reports, your settings, a system summary and a screenshot of the last game frame. Nothing is uploaded.", "保存最近的日志、崩溃与卡死报告、设置、系统摘要，以及最后一帧游戏截图。不会上传任何内容。"},
            {"Saves the recent logs, crash and hang reports, your settings and a system summary. Nothing is uploaded.", "保存最近的日志、崩溃与卡死报告、设置和系统摘要。不会上传任何内容。"},
            {"Saving the report...", "正在保存报告..."},
            {"Report saved to {}", "报告已保存到 {}"},
            {"Couldn't save the report: {}", "无法保存报告：{}"},
            {"Calibrate", "校准"},
            {"Start", "开始"},
            {"Stop", "停止"},
            {"Keep", "保留"},
            {"Pedals", "踏板"},
            {"Turn the wheel to the left end, then keep it there.", "请将方向盘向左转到头并保持。"},
            {"Turn the wheel to the right end, then keep it there.", "请将方向盘向右转到头并保持。"},
            {"Press the accelerator fully, then keep it down.", "请将油门踩到底并保持。"},
            {"Press the brake fully, then keep it down.", "请将刹车踩到底并保持。"},
            {"Release the pedals.", "请松开踏板。"},
            {"Turn the wheel to the left end and hold it.", "请将方向盘向左转到头并保持。"},
            {"Turn the wheel to the right end and hold it.", "请将方向盘向右转到头并保持。"},
            {"Press the accelerator and hold it.", "请踩住油门。"},
            {"Press the brake and hold it.", "请踩住刹车。"},

            // Settings values.
            {"On", "开"},
            {"Off", "关"},
            {"Auto", "自动"},
            {"System", "跟随系统"},
            {"English", "English"},
            {"简体中文", "简体中文"},
            {"Full", "全屏"},
            {"75%", "75%"},
            {"Bicubic", "双三次"},
            {"Bilinear", "双线性"},
            {"FSR 1", "FSR 1"},
            {"Fill Screen", "填充屏幕"},
            {"16:9", "16:9"},
            {"Extended Linear", "扩展线性"},
            {"HDR10 (PQ)", "HDR10 (PQ)"},
            {"Original", "原始"},
            {"Sharper", "更清晰"},
            {"All", "全部"},
            {"60 fps", "60 fps"},
            {"30 fps", "30 fps"},
            {"100 nits", "100 尼特"},
            {"200 nits", "200 尼特"},
            {"300 nits", "300 尼特"},
            {"400 nits", "400 尼特"},
            {"1x (720p)", "1x (720p)"},
            {"1.5x (1080p)", "1.5x (1080p)"},
            {"2x (1440p)", "2x (1440p)"},
            {"3x (2160p)", "3x (2160p)"},
            {"1x (1600x1600)", "1x (1600x1600)"},
            {"1.5x (2400x2400)", "1.5x (2400x2400)"},
            {"2x (3200x3200)", "2x (3200x3200)"},

            // Settings rows: labels.
            {"Overlay Language", "Overlay 语言"},
            {"Fullscreen", "全屏"},
            {"V-Sync", "垂直同步"},
            {"Frame Rate", "帧率"},
            {"Smooth Motion", "平滑运动"},
            {"Screen Resolution", "屏幕分辨率"},
            {"Anti-Aliasing", "抗锯齿"},
            {"Upscaling", "放大"},
            {"Aspect Ratio", "宽高比"},
            {"Brightness", "亮度"},
            {"Xbox 360 Display Gamma", "Xbox 360 显示伽马"},
            {"Cooler Colors", "冷色调"},
            {"HDR Output", "HDR 输出"},
            {"HDR Format", "HDR 格式"},
            {"HDR Strength", "HDR 强度"},
            {"HDR Paper White", "HDR 纸张白"},
            {"Sharpening", "锐化"},
            {"Anisotropic Filtering", "各向异性过滤"},
            {"Mipmaps", "纹理多级渐远"},
            {"Internal Resolution", "内部渲染分辨率"},
            {"Shadow Resolution", "阴影分辨率"},
            {"Sharper UI", "锐利界面"},
            {"Movie Upscaling", "影片放大"},
            {"Window Width", "窗口宽度"},
            {"Window Height", "窗口高度"},
            {"Shadow Resolution Version", "阴影分辨率版本"},
            {"Master Volume", "主音量"},
            {"Mute", "静音"},
            {"Mute in Background", "后台静音"},
            {"Stick Deadzone", "摇杆死区"},
            {"Trigger Deadzone", "扳机死区"},
            {"Vibration", "振动"},
            {"Steering Wheel", "方向盘"},
            {"Steering Range", "转向范围"},
            {"Invert Steering", "反转转向"},
            {"Steering Deadzone", "转向死区"},
            {"Steering Response", "转向响应"},
            {"Force Feedback", "力反馈"},
            {"Centering Force", "回正力"},
            {"Wheel Settings Version", "方向盘设置版本"},
            {"Asynchronous Shaders", "异步着色器"},
            {"Performance Overlay", "性能 Overlay"},
            {"Check for Updates at Startup", "启动时检查更新"},
            {"Language", "游戏语言"},
            {"Steering Axis", "转向轴"},
            {"Throttle Axis", "油门轴"},
            {"Brake Axis", "刹车轴"},
            {"Clutch Axis", "离合轴"},
            {"Throttle at Rest", "油门静止值"},
            {"Throttle Pressed", "油门按下值"},
            {"Brake at Rest", "刹车静止值"},
            {"Brake Pressed", "刹车按下值"},
            {"Clutch at Rest", "离合静止值"},
            {"Clutch Pressed", "离合按下值"},
            {"Steering Full Left", "转向最左"},
            {"Steering Full Right", "转向最右"},
            {"A Button", "A 键"},
            {"B Button", "B 键"},
            {"X Button", "X 键"},
            {"Y Button", "Y 键"},
            {"LB Button", "LB 键"},
            {"RB Button", "RB 键"},
            {"Back Button", "Back 键"},
            {"Start Button", "Start 键"},

            // Settings rows: descriptions.
            {"Language of SpeedBreaker's own menus, performance overlay and dialogs. System follows this device's language.", "SpeedBreaker 自带菜单、性能 Overlay 和对话框的语言。跟随系统会使用本机语言。"},
            {"Fill the screen with a borderless window. Off: a resizable window.", "使用无边框窗口填满屏幕。关闭：可调整大小的窗口。"},
            {"Show each frame on a display refresh. Off can tear, but frames appear sooner.", "每次显示器刷新时显示一帧。关闭可能出现撕裂，但画面更早出现。"},
            {"Auto: 60, or 30 while the device is hot or can't hold 60. A steady 30 feels smoother than a shaky 60 (the Steam Frame's headset). The game keeps its speed.", "自动：60；设备过热或无法稳定 60 时降为 30。稳定的 30 比波动的 60 更顺畅（Steam Frame 头显）。游戏速度保持不变。"},
            {"Up to 120 fps on a 120 Hz screen (ProMotion): the game still runs at 60 steps a second, and a picture between each two is drawn from the poses either side. A little more delay and power. Heavy scenes drop to an even 60 for a while; off at 30 fps.",
                "在 120 Hz 屏幕（ProMotion）上最高 120 fps：游戏仍以每秒 60 步运行，每两帧之间根据前后姿态绘制一帧。略有额外延迟和功耗。繁重场景会暂时降至稳定的 60；30 fps 时关闭。"},
            {"Up to 120 fps on a 120 Hz screen, or 90 on a 90 Hz one (not on a Steam Deck): the game still runs at 60 steps a second, and a picture between each two is drawn from the poses either side. A little more delay and power. Heavy scenes drop to an even 60 for a while; off at 30 fps and with variable refresh.",
                "在 120 Hz 屏幕上最高 120 fps，90 Hz 屏幕上为 90（Steam Deck 不提供）：游戏仍以每秒 60 步运行，每两帧之间根据前后姿态绘制一帧。略有额外延迟和功耗。繁重场景会暂时降至稳定的 60；30 fps 和可变刷新率时关闭。"},
            {"75%: the picture and these menus at 3/4 of the screen's resolution each way, scaled up by iOS: a little softer, a little less GPU work.", "75%：画面和这些菜单按屏幕分辨率各方向 3/4 渲染，再由 iOS 放大：稍柔和，GPU 负担略低。"},
            {"Smooth jagged edges (the 360 used 2x MSAA; this is an FXAA-style pass).", "平滑锯齿边缘（360 使用 2x MSAA；这里是 FXAA 风格处理）。"},
            {"How the game's picture is enlarged to the window. Bicubic is sharp, bilinear softer. FSR 1 (AMD FidelityFX Super Resolution 1) keeps curves, wires and diagonals smooth when the window is much larger than the Internal Resolution (1x on a 1440p or 4K screen), for a little more GPU time.", "游戏画面放大到窗口的方式。双三次较锐利，双线性较柔和。窗口远大于内部渲染分辨率时（1440p 或 4K 屏幕使用 1x），FSR 1（AMD FidelityFX Super Resolution 1）能让曲线、电线和斜线更平滑，代价是少量 GPU 时间。"},
            {"How the game's picture is enlarged to the screen. Bicubic is sharp, bilinear softer.", "游戏画面放大到屏幕的方式。双三次较锐利，双线性较柔和。"},
            {"Fill other screen shapes: a wider view on an ultrawide, a taller one on a Steam Deck or a 4:3 iPad, the HUD at 16:9. Or 16:9 with bars.", "适配其他屏幕比例：超宽屏显示更宽视野，Steam Deck 或 4:3 iPad 显示更高视野，HUD 保持 16:9；也可使用带黑边的 16:9。"},
            {"Lighter or darker shadows and mid-tones, a gentle step at a time, as a TV's brightness. Black and white stay put. 0 is the original picture.", "像电视亮度一样逐级调亮或调暗阴影与中间调。黑白场保持不变。0 为原始画面。"},
            {"The gamma ramp the game itself sets for a real Xbox 360's TV output: a little deeper shadows and brighter mid-tones, as on the console. Off: the flatter picture of SpeedBreaker 0.1.3 and earlier.", "游戏为真实 Xbox 360 电视输出设置的伽马曲线：阴影稍深、中间调更亮，与主机一致。关闭：SpeedBreaker 0.1.3 及更早版本的较平画面。"},
            {"A cooler picture with less of the game's yellow: blue raised, red and green lowered a little, over the whole game picture (menus and movies too). Off: the game's own colors, as on the Xbox 360.", "降低游戏画面中的黄色调，使整体更冷：提高蓝色，略微降低红绿，作用于整个游戏画面（包括菜单和影片）。关闭：与 Xbox 360 一致的游戏原色。"},
            {"Brighter highlights on an HDR display (extended dynamic range). The game's own picture, including its front end, follows the highlights; this settings overlay stays at standard brightness. Off: the standard-range picture.", "在 HDR 显示器上提供更亮的高光（扩展动态范围）。游戏画面（包括前端）会随高光变化；此设置 Overlay 保持标准亮度。关闭：标准动态范围画面。"},
            {"How HDR is sent. Auto: Extended Linear on a display that reports HDR capability, otherwise standard range. HDR10 (PQ) sends the PQ signal and metadata; the display's HDR mode still follows macOS.", "HDR 的发送方式。自动：显示器报告支持 HDR 时使用扩展线性，否则使用标准范围。HDR10 (PQ) 发送 PQ 信号和元数据；显示器 HDR 模式仍由 macOS 控制。"},
            {"How far white and highlights go above SDR white: 2x by default, 3x, or 4x. A frame that is mostly white is dimmed toward SDR automatically, so a boot flash or a full-screen logo does not become a 4x flash.", "白色和高光高出 SDR 白色的倍数：默认 2x、3x 或 4x。以白色为主的画面会自动向 SDR 压暗，避免启动闪光或全屏 Logo 变成 4x 闪光。"},
            {"How bright the game's ordinary white is in HDR10, in nits; highlights go HDR Strength times above it. Auto matches the system: 400 on SteamOS and the Steam Deck, 200 on Windows and Linux, 100 on a Mac. Raise it if HDR looks dimmer than the game in SDR.", "HDR10 中游戏普通白色的亮度（尼特）；高光再乘以 HDR 强度。自动跟随系统：SteamOS 和 Steam Deck 为 400，Windows/Linux 为 200，Mac 为 100。若 HDR 比 SDR 暗，可提高此值。"},
            {"Extra sharpness for bicubic and FSR 1 upscaling (for FSR 1, the share of its sharpening pass's full strength). 0 turns it off.", "为双三次和 FSR 1 放大增加锐度（对 FSR 1 而言是锐化处理全强度的比例）。0 关闭。"},
            {"Extra sharpness for bicubic upscaling. 0 turns it off.", "为双三次放大增加锐度。0 关闭。"},
            {"Keeps roads and walls sharp at a distance. Auto: 16x on a dedicated GPU or an M-series iPad, 4x on other integrated ones.", "让远处的道路和墙壁保持清晰。自动：独立显卡或 M 系列 iPad 为 16x，其他集成显卡为 4x。"},
            {"Use the game's smaller textures in the distance, so they don't shimmer.", "远处使用游戏较小的纹理，避免闪烁。"},
            {"Render the 3D scene above the 360's 720p. 1.5x is 1080p, a little over half the pixels of 2x. On a screen of another shape filled by Fill Screen, each step renders that shape: 1x on a Steam Deck is its own 1280x800. Auto: from the screen (1440p: 2x; a Steam Deck: its 1280x800).", "以高于 360 的 720p 渲染 3D 场景。1.5x 为 1080p，像素略多于 2x 的一半。使用填充屏幕的其他比例屏幕时，每档按该比例渲染：Steam Deck 的 1x 为 1280x800。自动：根据屏幕选择（1440p 为 2x；Steam Deck 为其 1280x800）。"},
            {"Sharper sun shadows: draw the game's 1600x1600 shadow map at 1.5x or 2x. 1x is the 360's map, spread over the whole picture so shadows reach its sides, which makes them a little softer than on the 360. Higher settings use more GPU time and memory. Auto: 2x when Internal Resolution renders at 2x or more, otherwise 1x; always 1x on Steam Deck and Steam Frame.", "更锐利的太阳阴影：以 1.5x 或 2x 绘制游戏的 1600x1600 阴影贴图。1x 为 360 的贴图，铺满整个画面使阴影到达两侧，因此比 360 略柔和。更高设置消耗更多 GPU 时间和显存。自动：内部渲染分辨率为 2x 及以上时使用 2x，否则 1x；Steam Deck 和 Steam Frame 始终为 1x。"},
            {"Crisper menu and HUD text and icons above 1x Internal Resolution: the game's small UI art is enlarged with a sharper filter instead of a blur. Layout and colors are unchanged. Off: as before.", "内部渲染分辨率高于 1x 时，让菜单和 HUD 文字图标更清晰：游戏的小尺寸 UI 素材使用更锐利的放大滤镜而非模糊。布局和颜色不变。关闭：保持原样。"},
            {"How the game's 720p movies are enlarged. Sharper: a sharper filter where the movie is drawn into a higher Internal Resolution, FSR 1's edge-aware upscale where it reaches the screen at 1x, for cleaner edges and text. Original: as before.", "游戏 720p 影片的放大方式。更清晰：影片绘制到更高内部渲染分辨率时使用更锐利的滤镜，1x 直达屏幕时使用 FSR 1 边缘感知放大，使边缘和文字更干净。原始：保持原样。"},
            {"How the game's 720p movies are enlarged. Sharper: a sharper filter where the movie is drawn into a higher Internal Resolution, a Lanczos filter where it reaches the screen at 1x. Original: as before.", "游戏 720p 影片的放大方式。更清晰：影片绘制到更高内部渲染分辨率时使用更锐利的滤镜，1x 直达屏幕时使用 Lanczos 滤镜。原始：保持原样。"},
            {"The window's width when the game starts.", "游戏启动时的窗口宽度。"},
            {"The window's height when the game starts.", "游戏启动时的窗口高度。"},
            {"1 once a Shadow Resolution of 1x from before v0.2.4 (then the default) became Auto; 0 otherwise.", "v0.2.4 之前（当时为默认）的 1x 阴影分辨率转为自动后为 1；否则为 0。"},
            {"The volume of all game audio.", "所有游戏音频的音量。"},
            {"Silence all game audio.", "将全部游戏音频静音。"},
            {"Silence the game while its window isn't focused.", "窗口失去焦点时将游戏静音。"},
            {"How far a stick moves before the game sees it. Raise it if the car steers by itself.", "摇杆移动多远后游戏才会响应。若车辆自行转向，可提高此值。"},
            {"How far a trigger is pressed before it counts.", "扳机按下多远后才生效。"},
            {"Controller rumble, and the knocks a steering wheel's force feedback passes on (Settings > Wheel).", "手柄振动，以及方向盘力反馈传递的冲击（设置 > 方向盘）。"},
            {"Drive with a connected USB steering wheel. Off: the game ignores it and a controller or the keyboard plays, but it still works these menus.", "使用已连接的 USB 方向盘驾驶。关闭：游戏忽略它，可用手柄或键盘游玩，但它仍可操作这些菜单。"},
            {"How far you turn the wheel, lock to lock, for the car's full lock. 360 suits this game, which was made for a pad: half a turn each way. More is slower to steer; All uses the whole turn. The degrees assume a 900-degree wheel, as the G29 is.", "方向盘从一端到另一端转动多少角度对应车辆满舵。360 适合这款为手柄设计的游戏：左右各半圈。更大角度转向更慢；全部使用整个转角。角度按 900 度方向盘（如 G29）计算。"},
            {"For a wheel whose axis runs the other way: turning left steers left again. Calibrate sets this for you.", "用于轴向相反的方向盘：向左转时车辆重新向左。校准会自动设置。"},
            {"How far the wheel turns before the game sees it. A wheel needs none; raise it only if the car pulls to one side.", "方向盘转动多远后游戏才会响应。方向盘通常无需死区；仅当车辆偏向一侧时提高。"},
            {"1.00 steers in step with the wheel, and suits most. Above 1.00 calms the centre for holding a straight, and sharpens the last of the turn. Below 1.00 makes the centre twitchy: at 0.50 the first tenth of the steering is within 2 degrees of straight.", "1.00 时转向与方向盘同步，适合大多数人。高于 1.00 会让中心更稳、便于保持直线，并让末段转向更锐利。低于 1.00 会让中心更敏感：0.50 时前十分之一转向仅在直线 2 度范围内。"},
            {"How strongly the knocks the game asks for reach the wheel. Needs a wheel whose force feedback this system drives; Vibration (Input) turns it off too.", "游戏请求的冲击传递到方向盘的强度。需要系统能驱动其力反馈的方向盘；关闭“振动（输入）”也会关闭它。"},
            {"How hard the wheel pulls back to straight ahead. A wheel with no spring of its own needs this; it needs force feedback too.", "方向盘回正到前方的力度。自身没有弹簧的方向盘需要此项；也需要力反馈。"},
            {"Which steering these Wheel settings were made with: 0 before the game's own deadzone was stepped over, 1 since.", "这些方向盘设置使用的转向处理：游戏自身死区被跳过前为 0，之后为 1。"},
            {"Skip an object for a frame or two while its shader compiles, instead of stuttering.", "着色器编译时暂时跳过对象一两帧，避免卡顿。"},
            {"Show the frame rate and frame time in a corner of the screen.", "在屏幕角落显示帧率和帧时间。"},
            {"Ask GitHub once at each launch whether a newer SpeedBreaker is out. Only the question goes out: nothing about you or this device. Off: only Check for Updates asks.", "每次启动时向 GitHub 查询是否有新版本。只发送查询，不发送任何关于你或设备的信息。关闭：仅在手动检查更新时查询。"},
            {"A second key for the same control, or none.", "同一操作的第二个按键，或“无”。"},
            {"Which of the wheel's axes this is, as the device numbers them. -1: from the device.", "方向盘轴，按设备编号。-1：由设备决定。"},
            {"What the pedal's axis reads there, as Calibrate measured it.", "踏板轴在该位置的读数，由校准测得。"},
            {"Which of the wheel's buttons this control is, as Calibrate recorded it.", "此操作对应的方向盘按键，由校准记录。"},
            {"The language of the game's menus, race text and video subtitles: English (the USA disc's), French, German, Italian, Spanish, Dutch, Swedish, Danish, Polish, Finnish, or Chinese (the Asian disc's). The movies and the speech stay in English on the USA and Asian discs, as they have them. The European (PAL) Spanish disc has Spanish movies and speech, and starts in Spanish; another language there keeps them in Spanish. The Asian disc starts in Chinese; choose English there to switch its menus and subtitles to English.", "游戏菜单、比赛文字和影片字幕的语言：英语（美版光盘）、法语、德语、意大利语、西班牙语、荷兰语、瑞典语、丹麦语、波兰语、芬兰语或中文（亚洲版光盘）。美版和亚洲版光盘的影片与语音保持英语。欧版（PAL）西班牙语光盘包含西班牙语影片和语音，并以西班牙语启动；在那里选择其他语言仍会保留西班牙语影片和语音。亚洲版以中文启动；选择 English 可切换其菜单和字幕。"},

            // Performance overlay.
            {"{:.0f} fps", "{:.0f} 帧/秒"},
            {"{:.1f} ms  (worst {:.1f})", "{:.1f} ms（最差 {:.1f}）"},
            {"CPU render {:.1f} ms   GPU {:.1f} ms", "CPU 渲染 {:.1f} ms   GPU {:.1f} ms"},
            {"{:.0f} draws per frame", "每帧 {:.0f} 次绘制"},

            // Common dialogs and notices.
            {"OK", "确定"},
            {"Try again", "重试"},
            {"Open download page", "打开下载页面"},
            {"What's new in {}", "{} 的新内容"},
            {"This release has no notes.", "此版本没有更新说明。"},
            {"The rest is on the download page.", "其余内容见下载页面。"},
            {"No browser? Point a phone's camera at the code.", "没有浏览器？用手机相机扫描二维码。"},
            {"Download page: {}", "下载页面：{}"},
            {"Asking GitHub for the newest release{}", "正在向 GitHub 查询最新版本{}"},
            {"It takes a few seconds at most. Only the question goes out: nothing about you or this device.", "最多只需几秒。只发送查询，不发送任何关于你或设备的信息。"},
            {"Asks GitHub whether a newer " SB_APP_NAME " is out, and sends nothing but the question. With Check for Updates at Startup off, only this asks.", "向 GitHub 查询是否有更新的 " SB_APP_NAME "，只发送查询。关闭“启动时检查更新”后，只有此项会查询。"},
            {"This version: v{}", "当前版本：v{}"},
            {"Updated to v{}", "已更新到 v{}"},
            {"{} (you have v{})", "{}（当前为 v{}）"},
            {"Releases: {}", "版本发布：{}"},

            // Settings > Controls: the controls a key can drive.
            {"Right Trigger", "右扳机"},
            {"Left Trigger", "左扳机"},
            {"Left Stick Left", "左摇杆向左"},
            {"Left Stick Right", "左摇杆向右"},
            {"Left Stick Up", "左摇杆向上"},
            {"Left Stick Down", "左摇杆向下"},
            {"A", "A"},
            {"B", "B"},
            {"X", "X"},
            {"Y", "Y"},
            {"LB", "LB"},
            {"RB", "RB"},
            {"D-pad Up", "方向键上"},
            {"D-pad Down", "方向键下"},
            {"D-pad Left", "方向键左"},
            {"D-pad Right", "方向键右"},
            {"Back", "Back"},
            {"Right Stick Up", "右摇杆向上"},
            {"Right Stick Down", "右摇杆向下"},
            {"Right Stick Left", "右摇杆向左"},
            {"Right Stick Right", "右摇杆向右"},
            {"Left Stick Click", "按下左摇杆"},
            {"Right Stick Click", "按下右摇杆"},
            {"Accelerate", "加速"},
            {"Steer Left", "左转"},
            {"Steer Right", "右转"},
            {"Steer Up", "上转"},
            {"Steer Down", "下转"},
            {"Look Left", "向左看"},
            {"Look Right", "向右看"},
            {"Look Up", "向上看"},
            {"Look Down", "向下看"},
            {"Handbrake", "手刹"},
            {"Nitro", "氮气"},
            {"Gear Up", "升挡"},
            {"Gear Down", "降挡"},
            {"Shift Up", "升挡"},
            {"Shift Down", "降挡"},
            {"Reset Car", "复位车辆"},
            {"Camera", "视角"},
            {"Horn", "喇叭"},
            {"Pause", "暂停"},
            {"Map", "地图"},

            // Remaining overlay chrome and statuses.
            {"D-pad: scroll, choose   A: select   B: back", "方向键：滚动、选择   A：确定   B：返回"},
            {"D-pad: choose   A: select   B: back", "方向键：选择   A：确定   B：返回"},
            {"A or B: back", "A 或 B：返回"},
            {"A or B: close", "A 或 B：关闭"},
            {"A or B: back (the check carries on)", "A 或 B：返回（检查继续）"},
            {"Checking for updates", "正在检查更新"},
            {"{} is out", "{} 已发布"},
            {"Check", "检查"},
            {"Up to date", "已是最新版本"},
            {"Couldn't check (limit)", "无法检查（请求受限）"},
            {"Couldn't check", "无法检查"},
            {"Save", "保存"},
            {"Saved", "已保存"},
            {"Failed", "失败"},
            {"This wheel", "此方向盘"},
            {"fullscreen", "全屏"},
            {"windowed", "窗口"},

            // The bug report's help, in both platform wordings.
            {"Saves the recent logs, crash and hang reports, your settings, a system summary and a screenshot in one .zip in the Files app (SpeedBreaker folder). Nothing is uploaded: send the file yourself.",
                "将最近的日志、崩溃与卡死报告、设置、系统摘要和一张截图保存为文件 App（SpeedBreaker 文件夹）中的一个 .zip。不会上传任何内容：请自行发送该文件。"},
            {"Saves the recent logs, crash and hang reports, your settings, a system summary and a screenshot in one .zip on the Desktop (or the game's data folder). Nothing is uploaded: send the file yourself.",
                "将最近的日志、崩溃与卡死报告、设置、系统摘要和一张截图保存为桌面（或游戏数据文件夹）中的一个 .zip。不会上传任何内容：请自行发送该文件。"},
            {"Asks GitHub whether a newer " SB_APP_NAME " is out, and sends nothing but the question. With Check for Updates at Startup on, the game also asks once at each launch.",
                "向 GitHub 查询是否有更新的 " SB_APP_NAME "，只发送查询。开启“启动时检查更新”后，游戏每次启动也会查询一次。"},

            // Wheel calibration.
            {"Full left", "向左到底"},
            {"Full right", "向右到底"},
            {"Throttle", "油门"},
            {"Brake", "刹车"},
            {"Clutch", "离合"},
            {"Turn the wheel fully left", "将方向盘向左转到底"},
            {"Turn the wheel fully right", "将方向盘向右转到底"},
            {"Press the throttle to the floor", "将油门踩到底"},
            {"Press the brake to the floor", "将刹车踩到底"},
            {"Press the clutch to the floor", "将离合踩到底"},
            {"Press the wheel button you want for it", "按下要为它使用的方向盘按键"},
            {"D-pad right skips, left goes back, down stops (keeping what's set). Esc stops too.",
                "方向键右跳过、左返回、下停止（保留已设置内容）。Esc 也会停止。"},
            {"D-pad right skips, left goes back; B, Esc or D-pad down stops (keeping what's set).",
                "方向键右跳过、左返回；B、Esc 或方向键下停止（保留已设置内容）。"},
            {"Teaches the game your wheel: both ends of its turn, each pedal, and a button for every control, one at a time. A keeps what each step saw; the D-pad skips a step or goes back. Only a wheel that comes out wrong needs this.",
                "让游戏学习你的方向盘：转向两端、每个踏板，以及每个操作对应的按键，逐步进行。A 保留每一步的检测结果；方向键可跳过或返回。只有方向盘识别不正确时才需要校准。"},
            {"{} of {}, {}. {}{}", "第 {} / {} 步：{}。{}{}"},
            {", then A to keep it.", "，然后按 A 保留。"},
            {"{}: button {}. A, or the same button again, keeps it; another button replaces it.",
                "{}：按键 {}。按 A 或再次按同一按键保留；按其他按键则替换。"},
            {"{}: axis {}, {:+d} to {:+d}, {:.0f}% of its travel. A keeps it.",
                "{}：轴 {}，{:+d} 到 {:+d}，行程的 {:.0f}%。按 A 保留。"},
            {".", "。"},

            // Fixed in-game notices and toasts.
            {"No rumble over USB", "USB 连接无法振动"},
            {"Rumble doesn't work with Xbox controllers connected by USB on a Mac. Pair it over Bluetooth for rumble.",
                "Mac 上通过 USB 连接的 Xbox 手柄无法振动。请改用蓝牙配对以启用振动。"},
            {"Screen refresh rate", "屏幕刷新率"},
            {"This screen is running at {} Hz, which doesn't divide 60 fps evenly, so motion can stutter. For smooth play, set Refresh Rate to 60 Hz in Quick Access > Performance. Turn on Use per-game profile there and Steam keeps it for this game.",
                "此屏幕以 {} Hz 运行，60 fps 无法与其整除，因此动态画面可能出现卡顿。为获得流畅体验，请在“快捷访问 > 性能”中将刷新率设为 60 Hz。同时开启“使用每款游戏的配置文件”，Steam 就会为此游戏保存该设置。"},
            {"This screen is running at {} Hz, which doesn't divide 60 fps evenly, so motion can stutter. For smooth play, set the screen to 60 or 120 Hz in Steam's display settings, or turn on variable refresh if it has it.",
                "此屏幕以 {} Hz 运行，60 fps 无法与其整除，因此动态画面可能出现卡顿。为获得流畅体验，请在 Steam 显示设置中将屏幕设为 60 或 120 Hz；如果支持可变刷新率，也可以将其开启。"},
            {"This screen is running at {} Hz, which doesn't divide 60 fps evenly, so motion can stutter. For smooth play, set it to 60 or 120 Hz in your display settings, or turn on variable refresh (FreeSync, G-SYNC, Adaptive-Sync) if the screen has it.",
                "此屏幕以 {} Hz 运行，60 fps 无法与其整除，因此动态画面可能出现卡顿。为获得流畅体验，请在显示器设置中将其设为 60 或 120 Hz；如果屏幕支持可变刷新率（FreeSync、G-SYNC、Adaptive-Sync），也可以将其开启。"},
            {"The game needs a controller: an Xbox or PlayStation one over Bluetooth, or a Backbone.",
                "游戏需要控制器：可通过蓝牙连接 Xbox 或 PlayStation 手柄，或使用 Backbone。"},
            {"Controller disconnected: the game needs one (Xbox or PlayStation over Bluetooth, or a Backbone).",
                "控制器已断开：游戏需要一个控制器（蓝牙 Xbox 或 PlayStation 手柄，或 Backbone）。"},
            {"Player 1's controller disconnected. Reconnect it, or play on with the keyboard.",
                "玩家 1 的控制器已断开。请重新连接，或继续使用键盘游玩。"},

            // Settings > Controls: key row descriptions.
            {"Gas in the game's default layout.", "游戏默认布局中的油门。"},
            {"Brake and reverse in the game's default layout.", "游戏默认布局中的刹车和倒车。"},
            {"Steer left.", "向左转向。"},
            {"Steer right.", "向右转向。"},
            {"The left stick pushed up (menus).", "左摇杆向上（菜单）。"},
            {"The left stick pulled down.", "左摇杆向下。"},
            {"The A button.", "A 键。"},
            {"The B button.", "B 键。"},
            {"The X button.", "X 键。"},
            {"The Y button.", "Y 键。"},
            {"The left shoulder button.", "左肩键。"},
            {"The right shoulder button.", "右肩键。"},
            {"The D-pad, up.", "方向键上。"},
            {"The D-pad, down.", "方向键下。"},
            {"The D-pad, left.", "方向键左。"},
            {"The D-pad, right.", "方向键右。"},
            {"Start: pause.", "Start：暂停。"},
            {"Back.", "Back。"},
            {"The right stick pushed up.", "右摇杆向上。"},
            {"The right stick pulled down.", "右摇杆向下。"},
            {"The right stick pushed left.", "右摇杆向左。"},
            {"The right stick pushed right.", "右摇杆向右。"},
            {"Pressing the left stick in.", "按下左摇杆。"},
            {"Pressing the right stick in.", "按下右摇杆。"},
            {"left", "左"},
            {"right", "右"},
            {"up", "上"},
            {"down", "下"},
            {"Steering: none", "转向：无"},
            {"Steering axis {}{}: {:+.0f}%", "转向轴 {}{}：{:+.0f}%"},
            {" inverted", "（反向）"},
            {"   {}: none", "   {}：无"},
            {"   {} {}: {:.0f}%", "   {} {}：{:.0f}%"},
        };

        const std::unordered_map<std::string_view, std::string_view>& ZhHans()
        {
            static const std::unordered_map<std::string_view, std::string_view> map = [] {
                std::unordered_map<std::string_view, std::string_view> out;
                out.reserve(std::size(kZhHans));
                for (const Entry& e : kZhHans)
                    out.emplace(e.english, e.chinese);
                return out;
            }();
            return map;
        }

        std::atomic<Language> g_language{ Language::English };
        std::string g_systemTag;

        Language ChoiceLanguage(Choice choice)
        {
            switch (choice)
            {
            case Choice::English: return Language::English;
            case Choice::ChineseSimplified: return Language::ChineseSimplified;
            default: return SystemLanguage();
            }
        }
    }

    Language FromLocaleTag(std::string_view tag)
    {
        // "zh_CN.UTF-8", "zh-Hant-TW", "en-US": the language before the
        // first separator is enough.
        size_t end = tag.find_first_of("_-.");
        std::string_view language = tag.substr(0, end);
        if (language.size() == 2 && (language[0] == 'z' || language[0] == 'Z') && (language[1] == 'h' || language[1] == 'H'))
            return Language::ChineseSimplified;
        return Language::English;
    }

    void SetSystemLocale(std::string_view tag)
    {
        g_systemTag.assign(tag);
    }

    std::string_view SystemLocaleTag()
    {
        if (!g_systemTag.empty())
            return g_systemTag;
        for (const char* name : { "LC_ALL", "LC_MESSAGES", "LANG" })
            if (const char* value = std::getenv(name); value && *value)
                return value;
        return {};
    }

    Language SystemLanguage()
    {
        return FromLocaleTag(SystemLocaleTag());
    }

    void Use(int32_t choice)
    {
        if (choice < int32_t(Choice::System) || choice > int32_t(Choice::ChineseSimplified))
            choice = int32_t(Choice::System);
        const Language selected = ChoiceLanguage(Choice(choice));
        if (selected != Current())
        {
            g_language.store(selected, std::memory_order_relaxed);
            fprintf(stderr, "[ui] overlay language: %s (%s)\n", Tag(),
                selected == Language::ChineseSimplified ? "简体中文" : "English");
        }
    }

    void Select(Language language)
    {
        g_language.store(language, std::memory_order_relaxed);
    }

    Language Current()
    {
        return g_language.load(std::memory_order_relaxed);
    }

    const char* Tag()
    {
        return Current() == Language::ChineseSimplified ? "zh-Hans" : "en";
    }

    std::string_view Tr(std::string_view english)
    {
        if (Current() == Language::English)
            return english;
        const auto& map = ZhHans();
        if (auto it = map.find(english); it != map.end())
            return it->second;
        return english;
    }

    const char* C(const char* english)
    {
        return Tr(english).data();
    }

    bool HasTranslation(std::string_view english)
    {
        const auto& map = ZhHans();
        return map.find(english) != map.end();
    }
}
