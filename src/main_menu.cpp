#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>

#include "sprite_asset.hpp"
#include "menu_timings.hpp"
#include "title_scene.hpp"
#include "fighter_select.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <array>
#include <stdexcept>
#include <sstream>
#include <random>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Texture {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous = nullptr;
    int width = 0, height = 0;
    mutable std::unique_ptr<Gdiplus::Bitmap> image;

    ~Texture() {
        image.reset();
        if (previous) SelectObject(dc, previous);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
    }

    void load(const skullgirls::SpriteBytes& dds) {
        const auto word = [&](std::size_t pos, unsigned length) {
            unsigned value = 0;
            for (unsigned i = 0; i < length; ++i) value |= unsigned(dds[pos + i]) << (8 * i);
            return value;
        };
        if (dds.size() < 128 || word(0, 4) != 0x20534444 || (word(84, 4) != 0x35545844 && word(84, 4) != 0x31545844))
            throw std::runtime_error("Expected a DXT1 or DXT5 menu texture");
        const bool bc3 = word(84, 4) == 0x35545844;
        const unsigned block_size = bc3 ? 16 : 8;
        const auto w = word(16, 4), h = word(12, 4);
        if (!w || !h || w > 4096 || h > 4096 ||
            ((w + 3) / 4) * ((h + 3) / 4) > (dds.size() - 128) / block_size)
            throw std::runtime_error("Truncated menu texture");
        width = w;
        height = h;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        void* memory = nullptr;
        dc = CreateCompatibleDC(nullptr);
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &memory, nullptr, 0);
        if (!dc || !bitmap) throw std::runtime_error("Cannot allocate menu texture");
        previous = SelectObject(dc, bitmap);
        auto* pixels = static_cast<unsigned*>(memory);
        std::size_t pos = 128;
        for (unsigned y = 0; y < h; y += 4) {
            for (unsigned x = 0; x < w; x += 4, pos += block_size) {
                unsigned alpha[8] = {dds[pos], dds[pos + 1]};
                const auto levels = alpha[0] > alpha[1] ? 7u : 5u;
                for (unsigned i = 1; i < levels; ++i)
                    alpha[i + 1] = ((levels - i) * alpha[0] + i * alpha[1]) / levels;
                if (levels == 5) alpha[6] = 0, alpha[7] = 255;
                const auto alpha_codes = bc3 ? word(pos + 2, 3) | (std::uint64_t(word(pos + 5, 3)) << 24) : 0;
                const auto colors = pos + (bc3 ? 8 : 0);
                const auto color_codes = word(colors + 4, 4);
                const bool four_colors = bc3 || word(colors, 2) > word(colors + 2, 2);
                unsigned color[4][3]{};
                for (unsigned i = 0; i < 2; ++i) {
                    const auto rgb = word(colors + i * 2, 2);
                    const auto b = rgb & 31, g = (rgb >> 5) & 63, r = rgb >> 11;
                    color[i][0] = (b << 3) | (b >> 2);
                    color[i][1] = (g << 2) | (g >> 4);
                    color[i][2] = (r << 3) | (r >> 2);
                }
                for (unsigned c = 0; c < 3; ++c) {
                    color[2][c] = four_colors ? (2 * color[0][c] + color[1][c]) / 3 : (color[0][c] + color[1][c]) / 2;
                    color[3][c] = (color[0][c] + 2 * color[1][c]) / 3;
                }
                for (unsigned i = 0; i < 16; ++i) {
                    const auto px = x + i % 4, py = y + i / 4;
                    if (px >= w || py >= h) continue;
                    const auto index = (color_codes >> (2 * i)) & 3;
                    const auto a = bc3 ? alpha[(alpha_codes >> (3 * i)) & 7] : (!four_colors && index == 3 ? 0u : 255u);
                    const auto& rgb = color[index];
                    pixels[py * w + px] = (a << 24) | ((rgb[2] * a / 255) << 16)
                        | ((rgb[1] * a / 255) << 8) | (rgb[0] * a / 255);
                }
            }
        }
    }

    Gdiplus::Bitmap* pixels() const {
        if (!image) {
            DIBSECTION section{};
            if (!GetObjectW(bitmap, sizeof(section), &section)) throw std::runtime_error("Cannot read texture pixels");
            image = std::make_unique<Gdiplus::Bitmap>(
                width, height, width * 4, PixelFormat32bppPARGB,
                static_cast<BYTE*>(section.dsBm.bmBits));
        }
        return image.get();
    }

    void draw(Gdiplus::Graphics& target, float x, float y, float w, float h, BYTE opacity = 255) const {
        if (opacity == 255) {
            target.DrawImage(pixels(), Gdiplus::RectF(x, y, w, h), 0, 0,
                             width, height, Gdiplus::UnitPixel);
            return;
        }
        Gdiplus::ColorMatrix matrix{};
        for (int i = 0; i < 5; ++i) matrix.m[i][i] = 1.0f;
        matrix.m[3][3] = opacity / 255.0f;
        Gdiplus::ImageAttributes attributes;
        attributes.SetColorMatrix(&matrix);
        attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        target.DrawImage(pixels(), Gdiplus::RectF(x, y, w, h), 0, 0, width, height,
                         Gdiplus::UnitPixel, &attributes);
    }
};

constexpr std::array headers{"download", "achievements", "leaderboards", "extras",
                             "singleplayer", "versus", "training", "help", "exitgame"};

constexpr std::array descriptions{
    L"Download additional content.", L"View your achievements.", L"View the leaderboards.",
    L"View the art gallery, replays, credits, and more.",
    L"Play single player modes, including Arcade Mode and Story Mode.",
    L"Play against another player locally or online.",
    L"Practice your skills or learn with tutorials and trials.",
    L"Configure controls and settings, or view the move list.", L"Exit Skullgirls."
};
constexpr std::array configs{"", "", "", "extras", "singlePlayer", "versusSTEAM",
                             "training", "helpSTEAM", ""};

struct MenuOption {
    std::wstring label;
    std::vector<std::wstring> choices;
    std::size_t choice = 0;
};

std::wstring option_label(const std::string& key) {
    if (key == "StartTrainingMode") return L"Start Training Mode";
    if (key == "LifeBarLevel") return L"Health Level";
    if (key == "LifeBarSetting") return L"Health Setting";
    if (key == "HitstunBar") return L"Hitstun Bar";
    if (key == "CPUQuickMatch") return L"CPU Quick Match";
    if (key == "TournamentMode") return L"Tournament Mode";
    if (key == "Local_Versus") return L"Local Versus";
    if (key == "QuickMatch_STEAM") return L"Ranked Match";
    if (key == "LobbyPlay") return L"Lobby Play";
    if (key == "ControllerRemap") return L"Controller Remap";
    if (key == "VideoSettings") return L"Video Settings";
    if (key == "Movelist") return L"Move List";
    std::wstring label;
    for (std::size_t i = 0; i < key.size(); ++i) {
        const char c = key[i];
        if (i && ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) &&
            key[i - 1] >= 'a' && key[i - 1] <= 'z') label += L' ';
        label += static_cast<unsigned char>(c);
    }
    return label;
}

void draw_text(Gdiplus::Graphics& dc, const wchar_t* text, Gdiplus::RectF bounds,
               float size, Gdiplus::StringAlignment alignment) {
    Gdiplus::Font font(L"Arial", size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::StringFormat format;
    format.SetAlignment(alignment);
    Gdiplus::SolidBrush shadow(Gdiplus::Color(235, 0, 0, 0));
    Gdiplus::SolidBrush cream(Gdiplus::Color(255, 245, 224, 180));
    bounds.Offset(2, 2);
    dc.DrawString(text, -1, &font, bounds, &format, &shadow);
    bounds.Offset(-2, -2);
    dc.DrawString(text, -1, &font, bounds, &format, &cream);
}

constexpr std::array roster{
    "", "marie", "",
    "squigly", "bigband", "eliza",
    "msfortune", "peacock", "painwheel",
    "filia", "random", "cerebella",
    "valentine", "parasoul", "double",
    "fukua", "beowulf", "robofortune",
    "umbrella", "annie", "blackdahlia"
};

struct Menu {
    Texture training_frame, player_header, select_header, select_film, select_overlay, select_side,
            select_pedestal, select_cursor, select_cursor_p2, team_frame;
    std::array<Texture, roster.size()> portraits;
    std::array<Texture, roster.size()> fighter_portraits, fighter_names;
    std::array<skullgirls::FighterSelectLayout, 2> fighter_layout;
    Texture fighter_frame, portrait_frame;
    std::array<Texture, 3> team_normal, team_glow;
    std::vector<MenuOption> training_options;
    std::size_t training_cursor = 0;
    enum class TrainingView { none, options, team_size, fighters, complete };
    TrainingView training = TrainingView::none;
    std::array<int, 2> team_size{1, 2};
    std::array<std::vector<int>, 2> fighters;
    int player = 0;
    int fighter_cursor = 9;

    void start_character_select() {
        for (auto& team : fighters) team.clear();
        player = 0;
        training = TrainingView::team_size;
    }

    void training_input(WPARAM key, bool accept, bool cancel) {
        const int horizontal = (key == VK_RIGHT) - (key == VK_LEFT);
        const int vertical = (key == VK_DOWN) - (key == VK_UP);
        if (training == TrainingView::options) {
            if (cancel) {
                training = TrainingView::none;
                return;
            }
            if (vertical) {
                const auto count = training_options.size();
                training_cursor = (training_cursor + count + vertical) % count;
            } else if (accept && training_cursor == 0) {
                start_character_select();
            } else if (horizontal) {
                auto& option = training_options[training_cursor];
                if (horizontal < 0 && option.choice > 0) --option.choice;
                if (horizontal > 0 && option.choice + 1 < option.choices.size()) ++option.choice;
            }
        } else if (training == TrainingView::team_size) {
            if (cancel) {
                training = TrainingView::options;
            } else if (accept) {
                fighter_cursor = 9;
                training = TrainingView::fighters;
            } else {
                team_size[player] = std::clamp(team_size[player] + horizontal, 1, 3);
            }
        } else if (training == TrainingView::fighters) {
            if (cancel) {
                auto& team = fighters[player];
                if (team.empty()) training = TrainingView::team_size;
                else team.pop_back();
            } else if (accept) {
                auto& team = fighters[player];
                if (fighter_cursor == 10) {
                    std::uniform_int_distribution<int> random_fighter(0, int(roster.size()) - 1);
                    static std::mt19937 generator(std::random_device{}());
                    do { fighter_cursor = random_fighter(generator); }
                    while (!*roster[fighter_cursor] || fighter_cursor == 10);
                }
                team.push_back(fighter_cursor);
                if (int(team.size()) == team_size[player]) {
                    if (player == 0) {
                        player = 1;
                        training = TrainingView::team_size;
                    } else {
                        training = TrainingView::complete;
                    }
                }
            } else if (horizontal || vertical) {
                int row = fighter_cursor / 3;
                int column = fighter_cursor % 3;
                do {
                    row = (row + vertical + 7) % 7;
                    column = (column + horizontal + 3) % 3;
                } while (!*roster[row * 3 + column]);
                fighter_cursor = row * 3 + column;
            }
        } else if (cancel) {
            fighters[1].pop_back();
            training = TrainingView::fighters;
        }
    }
    std::unique_ptr<Gdiplus::Bitmap> background;
    Texture city, frame, spot, film, vertical_film, fade, checker, wings, glow, submenu_row, submenu_selected;
    std::array<Texture, headers.size()> labels;
    using Clock = std::chrono::steady_clock;
    static constexpr int option_count = static_cast<int>(headers.size());
    skullgirls::MenuTimings timings{};
    skullgirls::TitleScene title;
    std::array<Texture, 2> title_logo;
    Texture title_font;
    std::vector<std::unique_ptr<Texture>> title_shield;
    bool title_visible = true;
    bool entering_menu = false;
    Clock::time_point menu_started{};

    void leave_title() {
        title_visible = false;
        entering_menu = true;
        menu_started = Clock::now();
    }
    int selected = 4;
    int scroll_direction = 1;
    float scroll_from = 4;
    float scroll_target = 4;
    Clock::time_point scroll_started{};

    float scroll_progress(Clock::time_point now) const {
        const float transition_seconds = scroll_direction < 0 ? timings.up : timings.down;
        return std::clamp(std::chrono::duration<float>(now - scroll_started).count()
                          / transition_seconds, 0.0f, 1.0f);
    }

    float scroll_position(Clock::time_point now) const {
        const float remaining = 1.0f - scroll_progress(now);
        return scroll_target + (scroll_from - scroll_target) * remaining * remaining;
    }

    void scroll(int direction) {
        const auto now = Clock::now();
        scroll_from = scroll_position(now) - scroll_target + selected;
        scroll_target = selected + direction;
        selected = (selected + direction + option_count) % option_count;
        scroll_direction = direction;
        scroll_started = now;
    }
    std::chrono::steady_clock::time_point animation_start = std::chrono::steady_clock::now();
    std::array<std::vector<MenuOption>, headers.size()> options;
    std::array<std::size_t, headers.size()> default_cursor{};
    std::size_t cursor = 0;
    bool submenu = false;
    float submenu_from = 0;
    Clock::time_point submenu_started{};

    float submenu_position(Clock::time_point now) const {
        const float t = std::clamp(std::chrono::duration<float>(now - submenu_started).count()
                                   / (submenu ? timings.open : timings.close), 0.0f, 1.0f);
        const float remaining = 1 - t;
        const float target = submenu ? 1.0f : 0.0f;
        return target + (submenu_from - target) * remaining * remaining;
    }

    void set_submenu(bool open) {
        const auto now = Clock::now();
        submenu_from = submenu_position(now);
        submenu_started = now;
        submenu = open;
    }
    bool exit_prompt = false;
    bool unavailable = false;

    void load(const std::filesystem::path& directory) {
        const auto archive = directory / "ui-win.gfs";
        const auto scene = skullgirls::read_sprite_asset(archive, "ui/Win/Main.gbs");
        timings = skullgirls::read_menu_timings(scene);
        title = skullgirls::read_title_scene(scene);
        for (std::size_t i = 0; i < title_logo.size(); ++i)
            title_logo[i].load(skullgirls::read_sprite_asset(archive, title.logo[i].texture));
        for (const auto& frame : title.shield) {
            auto texture = std::make_unique<Texture>();
            texture->load(skullgirls::read_sprite_asset(archive, frame.sprite.texture));
            title_shield.push_back(std::move(texture));
        }
        title_font.load(skullgirls::read_sprite_asset(archive, title.font_atlas));
        const auto load_texture = [&](Texture& texture, const std::string& name) {
            texture.load(skullgirls::read_sprite_asset(archive, "ui/Win/Textures/" + name + ".dds"));
        };
        load_texture(city, "charselect/background_stage_placeholder01.dxt1");
        load_texture(frame, "charselect/background_main_art_deco_frame02");
        load_texture(spot, "charselect/background_spotlight_lighting02");
        background = std::make_unique<Gdiplus::Bitmap>(1280, 720, PixelFormat32bppPARGB);
        if (background->GetLastStatus() != Gdiplus::Ok)
            throw std::runtime_error("Cannot allocate menu background");
        {
            Gdiplus::Graphics dc(background.get());
            dc.Clear(Gdiplus::Color(255, 0, 0, 0));
            dc.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
            dc.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            city.draw(dc, 0, 0, 1280, 720);
            frame.draw(dc, 0, 0, 1280, 720);
            spot.draw(dc, 0, 0, 1280, 720, 180);
        }
        load_texture(film, "MainMenu/filmstrip_piece01");
        load_texture(fade, "MainMenu/bg_fading01");
        load_texture(checker, "MainMenu/mainmenu_checker01");
        load_texture(wings, "MainMenu/mainmenu_wings01");
        load_texture(glow, "MainMenu/overglow01");
        load_texture(submenu_row, "MainMenu/SubOptionChoice");
        load_texture(submenu_selected, "MainMenu/SubOptionHeader");
        load_texture(training_frame, "Options/MainOptions_B");
        load_texture(player_header, "charselect/SG_Team_Select_Size_Main_Header01");
        load_texture(select_header, "charselect/charselect_winged_frame01");
        load_texture(select_film, "charselect/charselect_filmstrip_center01");
        load_texture(select_overlay, "charselect/charselect_filmstrip_center02");
        load_texture(select_side, "charselect/charselect_filmstrip_sub01");
        load_texture(select_pedestal, "charselect/charselect_pedestal_base01");
        load_texture(select_cursor, "charselect/charselect_selector_red01");
        load_texture(select_cursor_p2, "charselect/charselect_selector_blue01");
        load_texture(team_frame, "charselect/SG_Assist_Frame01");
        load_texture(fighter_frame, "charselect/charselect_data_frame01");
        load_texture(portrait_frame, "charselect/charselect_portrait_frame01");
        const auto charselect = skullgirls::read_sprite_asset(archive, "ui/Win/Charselect.gbs");
        for (int side = 0; side < 2; ++side)
            fighter_layout[side] = skullgirls::read_fighter_select_layout(charselect, side == 1);
        for (int i = 0; i < 3; ++i) {
            const auto base = "charselect/SG_Team_Select_Size_Number" + std::to_string(i + 1);
            load_texture(team_normal[i], base + "_Normal01");
            load_texture(team_glow[i], base + "_Glow01");
        }
        for (std::size_t i = 0; i < roster.size(); ++i) {
            if (*roster[i])
                load_texture(portraits[i], "charselect/charselect_select_icons_" + std::string(roster[i]) + "01");
            if (*roster[i] && std::string_view(roster[i]) != "random") {
                load_texture(fighter_portraits[i], "charselect/charselect_portraits_" + std::string(roster[i]) + "01");
                load_texture(fighter_names[i], "charselect/charselect_portrait_name_" + std::string(roster[i]) + "01");
            }
        }
        const auto training_bytes = skullgirls::read_sprite_asset(directory / "ui.gfs",
            "ui/config/Options/trainingopt.ini");
        std::istringstream training_config(std::string(training_bytes.begin(), training_bytes.end()));
        for (std::string line; std::getline(training_config, line);) {
            line.erase(std::min(line.size(), line.find('#')));
            const auto end = line.find_last_not_of(" \t\r");
            if (end == std::string::npos) continue;
            line.erase(end + 1);
            if (line == "Option:") {
                training_options.emplace_back();
            } else if (!training_options.empty()) {
                auto& option = training_options.back();
                if (line.rfind("Text: ", 0) == 0) option.label = option_label(line.substr(6));
                if (line.rfind("DefaultChoice: ", 0) == 0) {
                    option.choice = option.choices.size();
                    option.choices.push_back(option_label(line.substr(15)));
                }
                if (line.rfind("Choice: ", 0) == 0) option.choices.push_back(option_label(line.substr(8)));
            }
        }
        if (training_options.empty()) throw std::runtime_error("No Training Options in game config");
        for (std::size_t i = 0; i < configs.size(); ++i) {
            if (!*configs[i]) continue;
            const auto bytes = skullgirls::read_sprite_asset(directory / "ui.gfs",
                "ui/config/MainMenu/" + std::string(configs[i]) + ".ini");
            std::istringstream input(std::string(bytes.begin(), bytes.end()));
            auto& entries = options[i];
            std::string line;
            while (std::getline(input, line)) {
                const auto end = line.find_last_not_of(" \t\r");
                if (end == std::string::npos) continue;
                line.erase(end + 1);
                if (line == "Option:") {
                    entries.emplace_back();
                } else if (!entries.empty()) {
                    auto& option = entries.back();
                    if (line == "DefaultCursorPos") default_cursor[i] = entries.size() - 1;
                    if (line.rfind("Text: ", 0) == 0) option.label = option_label(line.substr(6));
                    if (line.rfind("DefaultChoice: ", 0) == 0) option.choices.push_back(option_label(line.substr(15)));
                    if (line.rfind("Choice: ", 0) == 0) option.choices.push_back(option_label(line.substr(8)));
                }
            }
            entries.push_back({L"Back", {}, 0});
        }
        // Rotate the film texture clockwise for the right-hand film track.
        vertical_film.width = film.height;
        vertical_film.height = film.width;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = vertical_film.width;
        info.bmiHeader.biHeight = -vertical_film.height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        void* memory = nullptr;
        vertical_film.dc = CreateCompatibleDC(nullptr);
        vertical_film.bitmap = CreateDIBSection(vertical_film.dc, &info, DIB_RGB_COLORS, &memory, nullptr, 0);
        if (!vertical_film.dc || !vertical_film.bitmap) throw std::runtime_error("Cannot rotate film texture");
        vertical_film.previous = SelectObject(vertical_film.dc, vertical_film.bitmap);
        DIBSECTION section{};
        if (!GetObjectW(film.bitmap, sizeof(section), &section)) throw std::runtime_error("Cannot read film texture");
        const auto* source = static_cast<const unsigned*>(section.dsBm.bmBits);
        auto* target = static_cast<unsigned*>(memory);
        for (int y = 0; y < film.height; ++y)
            for (int x = 0; x < film.width; ++x)
                target[x * film.height + film.height - 1 - y] = source[y * film.width + x];
        for (std::size_t i = 0; i < labels.size(); ++i)
            load_texture(labels[i], "MainMenu/header_" + std::string(headers[i]) + "01");
    }

    void paint_training_options(Gdiplus::Graphics& dc) const {
        Gdiplus::SolidBrush dim(Gdiplus::Color(185, 0, 0, 0));
        dc.FillRectangle(&dim, 0, 0, 1280, 720);
        // MediumOptions frame, header and P1Slot transforms from Options.gbs.
        training_frame.draw(dc, 352, 100, 576, 512);
        player_header.draw(dc, 550, 87, 182, 30);
        draw_text(dc, L"★ PLAYER 1 ★", {550, 90, 182, 30}, 21, Gdiplus::StringAlignmentCenter);
        draw_text(dc, L"TRAINING OPTIONS", {352, 126, 576, 40}, 30, Gdiplus::StringAlignmentCenter);
        constexpr std::size_t visible = 11;
        const auto first = training_cursor >= visible ? training_cursor - visible + 1 : 0;
        for (std::size_t row = 0; row < visible && first + row < training_options.size(); ++row) {
            const auto i = first + row;
            const auto& option = training_options[i];
            const auto y = 178.0f + float(row) * 39;
            if (i == training_cursor) {
                Gdiplus::Pen focus(Gdiplus::Color(255, 133, 30, 26), 2);
                dc.DrawRectangle(&focus, 392.0f, y, 504.0f, 37.0f);
            }
            draw_text(dc, option.label.c_str(), {397, y + 3, 275, 34}, 24, Gdiplus::StringAlignmentNear);
            if (!option.choices.empty()) {
                draw_text(dc, option.choices[option.choice].c_str(), {680, y + 3, 184, 34},
                          24, Gdiplus::StringAlignmentCenter);
                if (option.choice > 0) draw_text(dc, L"‹", {650, y, 26, 34}, 26, Gdiplus::StringAlignmentCenter);
                if (option.choice + 1 < option.choices.size())
                    draw_text(dc, L"›", {862, y, 26, 34}, 26, Gdiplus::StringAlignmentCenter);
            }
        }
        if (first + visible < training_options.size())
            draw_text(dc, L"⌄", {610, 576, 60, 35}, 26, Gdiplus::StringAlignmentCenter);
    }

    void paint_character_select(Gdiplus::Graphics& dc) const {
        // Filmstrip and header transforms from CharSelectFighters. Roster order and
        // additional row placement follow the character-select frame in the recording.
        select_side.draw(dc, 398.9f, 0, 492, 720);
        select_film.draw(dc, 500.5f, 0, 289, 720);
        for (std::size_t i = 0; i < roster.size(); ++i) {
            if (!*roster[i]) continue;
            portraits[i].draw(dc, 535.5f + (i % 3) * 71, 103.0f + (i / 3) * 71, 68, 70);
        }
        select_overlay.draw(dc, 500.5f, 0, 289, 720);
        select_header.draw(dc, 407.6f, 35.2f, 465, 70);
        for (int side = 0; side < 2; ++side) {
            const auto left = side ? 839.0f : 71.0f;
            select_pedestal.draw(dc, left, 178, 156, 542);
            select_pedestal.draw(dc, left + 204, 178, 156, 542);
            select_pedestal.draw(dc, left + 102, 196, 156, 524);
            const bool choosing = training == TrainingView::fighters && side == player;
            const bool locked = !fighters[side].empty() && (side < player || training == TrainingView::complete);
            if (choosing || locked) {
                const auto& layout = fighter_layout[side];
                const auto draw = [&](const Texture& texture, const skullgirls::TitleRect& rect, bool mirror) {
                    const auto state = dc.Save();
                    if (mirror) {
                        dc.TranslateTransform(rect.x * 2 + rect.width, 0);
                        dc.ScaleTransform(-1, 1);
                    }
                    texture.draw(dc, rect.x, rect.y, rect.width, rect.height);
                    dc.Restore(state);
                };
                const int current = choosing ? fighter_cursor : fighters[side].back();
                if (fighter_portraits[current].bitmap) {
                    draw(fighter_portraits[current], layout.portrait, side == 1);
                    draw(portrait_frame, layout.name, side == 1);
                    draw(fighter_names[current], layout.name, false);
                }
                draw(fighter_frame, layout.frame, side == 1);
                const auto& rect = layout.frame;
                draw_text(dc, side ? L"PLAYER 2" : L"PLAYER 1",
                          {side ? rect.x + 115 : rect.x + 470, rect.y + 22, 125, 34},
                          21, Gdiplus::StringAlignmentCenter);
                continue;
            }
            team_frame.draw(dc, left + 10, 389, 370, 210);
            player_header.draw(dc, left + 104, 374, 182, 30);
            draw_text(dc, side ? L"★ PLAYER 2 ★" : L"★ PLAYER 1 ★",
                      {left + 104, 378, 182, 30}, 20, Gdiplus::StringAlignmentCenter);
            draw_text(dc, L"SELECT SIZE", {left + 10, 415, 370, 40}, 27, Gdiplus::StringAlignmentCenter);
            for (int i = 0; i < 3; ++i) {
                const auto& number = team_size[side] == i + 1 ? team_glow[i] : team_normal[i];
                number.draw(dc, left + 54 + i * 89, 448, 82, 82);
            }
            std::wstring picks;
            for (int pick : fighters[side]) {
                if (!picks.empty()) picks += L"  /  ";
                picks += option_label(roster[pick]);
            }
            if (!picks.empty()) draw_text(dc, picks.c_str(), {left + 10, 551, 370, 36}, 23, Gdiplus::StringAlignmentCenter);
        }
        if (training == TrainingView::fighters) {
            (player ? select_cursor_p2 : select_cursor).draw(dc,
                521.5f + (fighter_cursor % 3) * 71, 90.0f + (fighter_cursor / 3) * 71, 96, 96);
        }
        if (training == TrainingView::complete)
            draw_text(dc, L"Teams selected", {410, 650, 460, 40}, 26, Gdiplus::StringAlignmentCenter);
    }

    void paint(HDC buffer) const {
        Gdiplus::Graphics dc(buffer);
        dc.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
        dc.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        dc.DrawImage(background.get(), 0, 0, 1280, 720);
        if (training == TrainingView::team_size || training == TrainingView::fighters ||
            training == TrainingView::complete) {
            paint_character_select(dc);
            return;
        }
        if (title_visible) {
            const auto seconds = std::chrono::duration<double>(Clock::now() - animation_start).count();
            for (std::size_t i = 0; i < title_shield.size(); ++i) {
                const auto& frame = title.shield[i];
                const auto opacity = frame.opacity.sample(seconds, true);
                if (opacity) {
                    const auto& rect = frame.sprite.bounds;
                    title_shield[i]->draw(dc, rect.x, rect.y, rect.width, rect.height, static_cast<BYTE>(opacity));
                }
            }
            for (std::size_t i = 0; i < title_logo.size(); ++i) {
                const auto& rect = title.logo[i].bounds;
                title_logo[i].draw(dc, rect.x, rect.y, rect.width, rect.height);
            }
            int width = 0;
            for (unsigned char c : title.prompt) width += title.glyphs.at(c).advance;
            const auto& box = title.prompt_bounds;
            float x = box.x + (box.width - width) / 2;
            const float baseline = box.y + title.font_top;
            const float sx = float(title_font.width) / title.atlas_width;
            const float sy = float(title_font.height) / title.atlas_height;
            Gdiplus::ColorMatrix matrix{};
            for (int i = 0; i < 5; ++i) matrix.m[i][i] = 1.0f;
            matrix.m[3][3] = title.prompt_opacity.sample(seconds, true) / 255.0f;
            Gdiplus::ImageAttributes attributes;
            attributes.SetColorMatrix(&matrix);
            for (unsigned char c : title.prompt) {
                const auto& glyph = title.glyphs.at(c);
                if (glyph.width && glyph.height) {
                    dc.DrawImage(title_font.pixels(),
                        Gdiplus::RectF(x + glyph.bearing, baseline - glyph.top,
                                       float(glyph.width), float(glyph.height)),
                        glyph.x * sx, glyph.y * sy, glyph.width * sx, glyph.height * sy,
                        Gdiplus::UnitPixel, &attributes);
                }
                x += glyph.advance;
            }
            return;
        }
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - animation_start).count();
        const auto travel = static_cast<float>(std::fmod(elapsed * (2.0 / 0.033), 384.0));
        const auto now = Clock::now();
        const auto panel = submenu_position(now);
        const auto rise = 168 * panel;
        const auto menu_seconds = std::chrono::duration<double>(now - menu_started).count();
        const auto intro_progress = std::clamp(float(menu_seconds * 60 / (title.menu_opacity.frames - 1)),
                                               0.0f, 1.0f);
        const auto phase = [&](float displacement, float idle) {
            const auto offset = entering_menu ? displacement * (1 - intro_progress) : 0.0f;
            const auto wrapped = std::fmod(idle + offset, 384.0f);
            return wrapped < 0 ? wrapped + 384 : wrapped;
        };
        for (float x = phase(title.intro_horizontal, travel) - 384; x < 1280; x += 384)
            film.draw(dc, x, 250 - rise, 384, 100);
        for (float y = phase(title.intro_vertical, -travel) - 384; y < 720; y += 384)
            vertical_film.draw(dc, 1043, y, 100, 384);
        fade.draw(dc, 0, 0, 1280, 720, 160);
        const auto intro_alpha = entering_menu ? title.menu_opacity.sample(menu_seconds, false) : 255;
        if (intro_alpha == 0) return;
        if (panel < 1) glow.draw(dc, 1045, 250, 100, 100, static_cast<BYTE>(intro_alpha * (1 - panel)));
        checker.draw(dc, 162, 231 - rise, 548, 139, static_cast<BYTE>(intro_alpha));
        wings.draw(dc, 70, 230 - rise, 144, 81, static_cast<BYTE>(intro_alpha));
        const auto state = dc.Save();
        dc.TranslateTransform(870, 0);
        dc.ScaleTransform(-1, 1);
        wings.draw(dc, 70, 230 - rise, 144, 81, static_cast<BYTE>(intro_alpha));
        dc.Restore(state);
        const float progress = std::clamp(std::chrono::duration<float>(now - scroll_started).count()
                                          / timings.heading, 0.0f, 1.0f);
        if (submenu || exit_prompt || progress >= 1.0f) {
            labels[selected].draw(dc, 145, 228 - rise, 587, 144, static_cast<BYTE>(intro_alpha));
        } else {
            const int previous = (selected - scroll_direction + option_count) % option_count;
            labels[previous].draw(dc, 145 - 600 * progress, 228 - rise, 587, 144,
                                  static_cast<BYTE>(255 * (1 - progress)));
            labels[selected].draw(dc, 145 + 600 * (1 - progress), 228 - rise, 587, 144,
                                  static_cast<BYTE>(255 * progress));
        }
        if (panel < 1 && !exit_prompt) {
            const float position = scroll_position(now);
            for (int i = 0; i < option_count; ++i) {
                const float delta = std::remainder(i - position, float(option_count));
                const float distance = std::abs(delta);
                const float opacity = std::min(std::min(distance, 1.0f),
                                                std::clamp(9.0f - 2 * distance, 0.0f, 1.0f));
                if (opacity <= 0) continue;
                const float y = 299 + delta * 39 + std::clamp(delta, -1.0f, 1.0f) * 39;
                labels[i].draw(dc, 713, y - 27, 335, 82, static_cast<BYTE>(intro_alpha * opacity * (1 - panel)));
            }
        }
        if (panel > 0) {
            const auto& entries = options[selected];
            for (std::size_t i = 0; i < entries.size(); ++i) {
                const int y = 242 + static_cast<int>(i) * 64;
                const float x = -470 * (1 - panel) + (i == cursor ? 40 : 0);
                (i == cursor ? submenu_selected : submenu_row).draw(dc, x, float(y), 427, 60);
                auto label = entries[i].label;
                if (!entries[i].choices.empty()) label += L"   ‹ " + entries[i].choices[entries[i].choice] + L" ›";
                draw_text(dc, label.c_str(), {x + 40, float(y + 12), 350, 40}, 28, Gdiplus::StringAlignmentCenter);
            }
        }
        const auto* description = exit_prompt ? L"Exit Skullgirls?  Z / A to confirm.  X / S to cancel."
            : unavailable ? L"This action is unavailable in the reconstruction."
            : descriptions[selected];
        if (intro_alpha < 255) return;
        if (!submenu || unavailable)
            draw_text(dc, description, {165, submenu ? 595.0f : 411.0f, 530, submenu ? 46.0f : 100.0f},
                      28, Gdiplus::StringAlignmentCenter);
        Gdiplus::SolidBrush footer(Gdiplus::Color(155, 8, 5, 4));
        dc.FillRectangle(&footer, 0, 641, 1280, 44);
        if (!submenu) draw_text(dc, L"Choose a Menu Option", {55, 649, 420, 38}, 24, Gdiplus::StringAlignmentNear);
        draw_text(dc, submenu ? L"Z A  Accept  |  X S  Cancel" : L"← → Enter  News  |  ↑ ↓  Menu Select  |  Z A  Accept  |  X S  Cancel",
                  {500, 651, 747, 36}, 20, Gdiplus::StringAlignmentFar);
        if (training == TrainingView::options) paint_training_options(dc);
    }
};

Menu* menu_instance = nullptr;
HDC canvas_dc = nullptr;
HBITMAP canvas_bitmap = nullptr;
HGDIOBJ canvas_previous = nullptr;

bool create_canvas(HWND window) {
    const auto dc = GetDC(window);
    if (!dc) return false;
    canvas_dc = CreateCompatibleDC(dc);
    canvas_bitmap = CreateCompatibleBitmap(dc, 1280, 720);
    ReleaseDC(window, dc);
    if (!canvas_dc || !canvas_bitmap) return false;
    canvas_previous = SelectObject(canvas_dc, canvas_bitmap);
    return canvas_previous != nullptr;
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto& menu = *menu_instance;
    switch (message) {
    case WM_LBUTTONDOWN:
        if (!menu.title_visible) break;
        menu.leave_title();
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_KEYDOWN: {
        if (menu.title_visible) {
            menu.leave_title();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (menu.entering_menu) {
            const auto elapsed = std::chrono::duration<double>(Menu::Clock::now() - menu.menu_started).count();
            if (elapsed * 60 < menu.title.menu_opacity.frames - 1) return 0;
            menu.entering_menu = false;
        }
        const bool accept = wparam == 'Z' || wparam == 'A';
        const bool cancel = wparam == VK_ESCAPE || wparam == 'X' || wparam == 'S';
        if (menu.training != Menu::TrainingView::none) {
            menu.training_input(wparam, accept, cancel);
        } else if (menu.exit_prompt) {
            if (accept) DestroyWindow(window);
            if (cancel) menu.exit_prompt = false;
        } else if (cancel) {
            if (menu.unavailable) menu.unavailable = false;
            else if (menu.submenu) menu.set_submenu(false);
            else menu.exit_prompt = true;
        } else if (menu.submenu) {
            auto& entries = menu.options[menu.selected];
            if (wparam == VK_UP || wparam == VK_DOWN) {
                menu.cursor = (menu.cursor + (wparam == VK_UP ? entries.size() - 1 : 1)) % entries.size();
                menu.unavailable = false;
            } else {
                auto& option = entries[menu.cursor];
                if (!option.choices.empty() && (wparam == VK_LEFT || wparam == VK_RIGHT || accept)) {
                    const auto count = option.choices.size();
                    option.choice = (option.choice + (wparam == VK_LEFT ? count - 1 : 1)) % count;
                } else if (accept) {
                    if (menu.cursor + 1 == entries.size()) menu.set_submenu(false);
                    else if (menu.selected == 6 && menu.cursor == 0)
                        menu.training = Menu::TrainingView::options;
                    else menu.unavailable = true;
                }
            }
        } else if (wparam == VK_UP || wparam == VK_DOWN) {
            menu.scroll(wparam == VK_UP ? -1 : 1);

            menu.unavailable = false;
        } else if (accept) {
            if (menu.selected == 8) menu.exit_prompt = true;
            else if (!menu.options[menu.selected].empty()) {
                menu.set_submenu(true);
                menu.cursor = menu.default_cursor[menu.selected];
            } else menu.unavailable = true;
        }
        const wchar_t* caption = L"Skullgirls Main Menu Reconstruction";
        switch (menu.training) {
        case Menu::TrainingView::options: caption = L"Skullgirls - Training Options"; break;
        case Menu::TrainingView::team_size: caption = L"Skullgirls - Select Team Size"; break;
        case Menu::TrainingView::fighters: caption = L"Skullgirls - Select Fighter"; break;
        case Menu::TrainingView::complete: caption = L"Skullgirls - Teams Selected"; break;
        default: break;
        }
        SetWindowTextW(window, caption);
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_CREATE:
        return create_canvas(window) ? 0 : -1;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const auto dc = BeginPaint(window, &paint);
        RECT rect{};
        GetClientRect(window, &rect);
        const auto width = std::min(rect.right, rect.bottom * 16 / 9);
        const auto height = width * 9 / 16;
        if (width > 0 && height > 0 && canvas_dc) {
            RECT canvas{0, 0, 1280, 720};
            const auto black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            FillRect(canvas_dc, &canvas, black);
            menu.paint(canvas_dc);

            const LONG left = (rect.right - width) / 2;
            const LONG top = (rect.bottom - height) / 2;
            const RECT bars[] = {
                {0, 0, rect.right, top},
                {0, top + height, rect.right, rect.bottom},
                {0, top, left, top + height},
                {left + width, top, rect.right, top + height}
            };
            for (const auto& bar : bars) FillRect(dc, &bar, black);
            if (width == 1280 && height == 720) {
                BitBlt(dc, left, top, width, height, canvas_dc, 0, 0, SRCCOPY);
            } else {
                SetStretchBltMode(dc, HALFTONE);
                SetBrushOrgEx(dc, 0, 0, nullptr);
                StretchBlt(dc, left, top, width, height, canvas_dc, 0, 0, 1280, 720, SRCCOPY);
            }
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_NCDESTROY:
        if (canvas_previous) SelectObject(canvas_dc, canvas_previous);
        if (canvas_bitmap) DeleteObject(canvas_bitmap);
        if (canvas_dc) DeleteDC(canvas_dc);
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    struct GraphicsRuntime {
        ULONG_PTR token = 0;
        ~GraphicsRuntime() { if (token) Gdiplus::GdiplusShutdown(token); }
    } graphics;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&graphics.token, &input, nullptr) != Gdiplus::Ok) return 1;
    Menu menu;
    menu_instance = &menu;
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const std::filesystem::path path = argc == 2 ? argv[1] :
        L"D:\\Steam\\steamapps\\common\\Skullgirls\\data02";
    LocalFree(argv);
    try {
        menu.load(path);
    } catch (const std::exception& error) {
        MessageBoxA(nullptr, error.what(), "Skullgirls main menu", MB_OK | MB_ICONERROR);
        return 1;
    }
    WNDCLASSW window_class{};
    window_class.hInstance = instance;
    window_class.lpfnWndProc = window_proc;
    window_class.lpszClassName = L"SkullgirlsReconstructedMenu";
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    if (!RegisterClassW(&window_class)) return 1;
    RECT size{0, 0, 1280, 720};
    AdjustWindowRect(&size, WS_OVERLAPPEDWINDOW, FALSE);
    const auto window = CreateWindowExW(0, window_class.lpszClassName, L"Skullgirls Main Menu Reconstruction",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, size.right - size.left,
        size.bottom - size.top, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    constexpr DWORD high_resolution_timer = 0x00000002;
    auto timer = CreateWaitableTimerExW(nullptr, nullptr, high_resolution_timer, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (!timer) {
        DestroyWindow(window);
        return 1;
    }

    using Clock = std::chrono::steady_clock;
    constexpr auto frame_period = std::chrono::nanoseconds(16666667);
    menu.animation_start = Clock::now();
    auto deadline = menu.animation_start + frame_period;
    ShowWindow(window, show);

    for (;;) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                CloseHandle(timer);
                return static_cast<int>(message.wParam);
            }
            DispatchMessageW(&message);
        }

        const auto now = Clock::now();
        if (now >= deadline) {
            deadline += frame_period * (1 + (now - deadline) / frame_period);
            if (!IsIconic(window)) RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            continue;
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - now);
        LARGE_INTEGER due{};
        due.QuadPart = -std::max<LONGLONG>(1, remaining.count() / 100);
        if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) ||
            MsgWaitForMultipleObjectsEx(1, &timer, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE) == WAIT_FAILED) {
            CloseHandle(timer);
            DestroyWindow(window);
            return 1;
        }
    }
}
