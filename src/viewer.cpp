// Windows viewer for exported RGBA images, sprite layers, and FOIT timelines.

#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <list>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>
#include <optional>
#include <string_view>
#include "foit.hpp"
#include "sprite_asset.hpp"

namespace {

constexpr UINT open_image = 1;
constexpr UINT exit_viewer = 2;
constexpr UINT play_sprite = 3;
constexpr UINT pause_playback = 4;
constexpr UINT previous_sprite = 5;
constexpr UINT next_sprite = 6;
constexpr UINT previous_animation = 7;
constexpr UINT next_animation = 8;
constexpr UINT first_sprite_slot = 9;
constexpr UINT follow_successor = 12;
constexpr UINT_PTR playback_timer = 1;

using Clock = std::chrono::steady_clock;
using SpriteTicks = std::chrono::duration<std::int64_t, std::ratio<1, 60>>;

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

Image image;

bool load_image(const wchar_t* path)
{
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    std::string magic, label, tuple;
    Image next;
    int depth = 0, max_value = 0;
    file >> magic >> label;
    if (magic != "P7" || label != "WIDTH") return false;
    file >> next.width >> label;
    if (label != "HEIGHT") return false;
    file >> next.height >> label;
    if (label != "DEPTH") return false;
    file >> depth >> label;
    if (label != "MAXVAL") return false;
    file >> max_value >> label;
    if (label != "TUPLTYPE") return false;
    file >> tuple >> label;
    if (tuple != "RGB_ALPHA" || label != "ENDHDR" || file.get() != '\n' ||
        depth != 4 || max_value != 255 || next.width < 1 || next.width > 4080 ||
        next.height < 1 || next.height > 4080) return false;

    next.pixels.resize(static_cast<std::size_t>(next.width) * next.height);
    for (int y = 0; y < next.height; ++y) {
        for (int x = 0; x < next.width; ++x) {
            unsigned char rgba[4];
            if (!file.read(reinterpret_cast<char*>(rgba), sizeof(rgba))) return false;
            const unsigned background = ((x / 8 + y / 8) % 2) ? 192 : 224;
            const auto channel = [&](unsigned i) {
                return (rgba[i] * rgba[3] + background * (255 - rgba[3]) + 127) / 255;
            };
            next.pixels[y * next.width + x] = (channel(0) << 16) | (channel(1) << 8) | channel(2);
        }
    }
    image.pixels.swap(next.pixels);
    image.width = next.width;
    image.height = next.height;
    return true;
}

struct SpritePlacement {
    const skullgirls::SpriteLayer* layer = nullptr;
    const skullgirls::SpriteLayer* overlay = nullptr;
    const skullgirls::SpriteBytes* art = nullptr;
    std::int64_t x = 0, y = 0;
};

// Stores the selected clip and tracks its decoded frame cache and playback clock.
struct AnimationPreview {
    std::vector<skullgirls::FoitAnimation> clips;
    std::vector<std::uint16_t> successors;
    std::vector<std::string> names;
    std::optional<std::size_t> child;
    std::size_t clip = 0, slot = 1;
    std::size_t record = 0;
    std::vector<SpritePlacement> sprites;
    std::vector<std::uint64_t> end_ticks;
    std::list<std::pair<const skullgirls::SpriteLayer*, Image>> decoded;
    std::size_t decoded_pixels = 0;
    std::int64_t left = 0, top = 0, right = 0, bottom = 0;
    std::uint64_t tick = 0;
    Clock::time_point started;
    bool playing = false;
    bool follow = false;
};

struct CharacterSprites {
    CharacterSprites(const std::filesystem::path& directory, const std::string& character)
        : assets(directory, character), palette(assets.read_default_palette()),
          name(character.begin(), character.end()) {}

    skullgirls::CharacterAssets assets;
    skullgirls::SpritePaletteLookup palette;
    std::vector<skullgirls::SpriteFrame> frames;
    std::vector<std::pair<std::size_t, std::size_t>> layers;
    std::size_t current = 0;
    std::vector<bool> unavailable;
    std::optional<AnimationPreview> animation;
    std::wstring name;
};

std::unique_ptr<CharacterSprites> character_sprites;

skullgirls::SpriteBytes decode_rgba(const CharacterSprites& sprites, const skullgirls::SpriteLayer& layer,
                                  const skullgirls::SpriteBytes& art)
{
    const auto raw = skullgirls::decode_sprite_layer(layer, art);
    skullgirls::SpriteBytes rgba(raw.size() / 3 * 4);
    for (std::size_t i = 0, j = 0; i < raw.size(); i += 3, j += 4) {
        const auto argb = sprites.palette.argb(raw[i + 2], raw[i + 1]);
        rgba[j] = static_cast<std::uint8_t>(argb >> 16);
        rgba[j + 1] = static_cast<std::uint8_t>(argb >> 8);
        rgba[j + 2] = static_cast<std::uint8_t>(argb);
        rgba[j + 3] = static_cast<std::uint8_t>(argb >> 24);
    }
    return rgba;
}

Image decode_layer(const CharacterSprites& sprites, const skullgirls::SpriteLayer& layer,
                   const skullgirls::SpriteBytes& art, const skullgirls::SpriteLayer* overlay = nullptr)
{
    auto rgba = decode_rgba(sprites, layer, art);
    if (overlay) skullgirls::composite_sprite_rgba(rgba, decode_rgba(sprites, *overlay, art));
    Image next;
    next.width = layer.tile_columns * 16;
    next.height = layer.tile_rows * 16;
    next.pixels.resize(rgba.size() / 4);
    for (int y = 0; y < next.height; ++y) {
        for (int x = 0; x < next.width; ++x) {
            const auto pixel = y * next.width + x;
            const auto* texel = rgba.data() + pixel * 4;
            const unsigned alpha = texel[3];
            const unsigned background = ((x / 8 + y / 8) % 2) ? 192 : 224;
            const auto blend = [&](unsigned channel) {
                return (texel[channel] * alpha + background * (255 - alpha) + 127) / 255;
            };
            next.pixels[pixel] = (blend(0) << 16) | (blend(1) << 8) | blend(2);
        }
    }
    return next;
}

// Cycle the sprite browser through drawable layers, skipping any layer that fails to decode.
void show_sprite(HWND window, int direction)
{
    if (!character_sprites) return;
    auto& sprites = *character_sprites;
    const auto count = sprites.layers.size();
    for (std::size_t attempts = 0; attempts < count; ++attempts) {
        if (direction > 0) sprites.current = (sprites.current + 1) % count;
        if (direction < 0) sprites.current = (sprites.current + count - 1) % count;
        if (direction == 0) direction = 1;
        if (sprites.unavailable[sprites.current]) continue;
        const auto [frame_index, slot] = sprites.layers[sprites.current];
        const auto& frame = sprites.frames[frame_index];
        const auto& layer = *frame.layers[slot];
        try {
            image = decode_layer(sprites, layer, sprites.assets.art(frame.art_bank));
            const auto title = L"Skullgirls Sprite Viewer - Filia | Frame " + std::to_wstring(frame_index)
                + L", layer " + std::to_wstring(slot) + L" | "
                + std::to_wstring(sprites.current + 1) + L"/" + std::to_wstring(count);
            SetWindowTextW(window, title.c_str());
            InvalidateRect(window, nullptr, FALSE);
            return;
        } catch (const std::runtime_error&) {
            sprites.unavailable[sprites.current] = true;
        }
    }
    KillTimer(window, playback_timer);
    MessageBoxW(window, L"No supported Filia sprite layers could be decoded.", L"Filia", MB_OK | MB_ICONERROR);
}

std::unique_ptr<CharacterSprites> load_character(const std::filesystem::path& directory,
                                                const std::string& name, bool timelines,
                                                std::optional<std::size_t> child = std::nullopt)
{
    auto sprites = std::make_unique<CharacterSprites>(directory, name);
    if (timelines) {
        auto asset = sprites->assets.read_animations_for_preview(child.has_value());
        auto& table = child ? asset.children.at(*child) : asset;
        sprites->frames = std::move(asset.sprites);
        auto& animation = sprites->animation.emplace();
        animation.clips = std::move(table.animations);
        animation.successors = std::move(table.successors);
        animation.names = std::move(table.names);
        animation.child = child;
    } else {
        sprites->frames = sprites->assets.read_frames();
    }
    for (const auto& frame : sprites->frames)
        sprites->assets.find_art(frame.art_bank);
    return sprites;
}

void open_filia(HWND window, const std::filesystem::path& directory)
{
    try {
        auto next = load_character(directory, "filia", false);
        for (std::size_t i = 0; i < next->frames.size(); ++i) {
            const auto& frame = next->frames[i];
            if (!next->assets.find_art(frame.art_bank)) continue;
            for (std::size_t slot = 0; slot < frame.layers.size(); ++slot) {
                const auto& layer = frame.layers[slot];
                if (layer && layer->used_tiles) next->layers.emplace_back(i, slot);
            }
        }
        if (next->layers.empty()) throw std::runtime_error("No Filia layers");
        next->unavailable.resize(next->layers.size());
        character_sprites = std::move(next);
        show_sprite(window, 0);
    } catch (const std::exception&) {
        MessageBoxW(window, L"Could not load Filia from the Skullgirls data directory.", L"Filia", MB_OK | MB_ICONERROR);
    }
}

SpritePlacement locate_sprite(CharacterSprites& sprites, unsigned reference)
{
    if (!reference || reference > sprites.frames.size()) return {};
    const auto& frame = sprites.frames[reference - 1];
    const auto* art = sprites.assets.find_art(frame.art_bank);
    if (!art) return {};
    for (std::size_t i = 0; i < frame.layers.size(); ++i) {
        const auto& layer = frame.layers[i];
        if (!layer || !layer->used_tiles) continue;
        // Fallback layers carry their own offsets relative to the same frame origin.
        const auto x = frame.layer_offsets[i][0] - std::int64_t(frame.origin[0]);
        const auto y = frame.layer_offsets[i][1] - std::int64_t(frame.origin[1]);
        const auto& second = frame.layers[1];
        const auto* overlay = i == 0 && second && second->used_tiles &&
            second->tile_columns == layer->tile_columns && second->tile_rows == layer->tile_rows
            ? &*second : nullptr;
        return {&*layer, overlay, art, x, y};
    }
    return {};
}

void show_record(HWND window, AnimationPreview& animation, std::size_t record)
{
    if (record == animation.record) return;
    const auto& clip = animation.clips[animation.clip];
    const auto& sprite = animation.sprites[record];
    if (animation.record >= clip.size() || animation.sprites[animation.record].layer != sprite.layer) {
        auto cached = std::find_if(animation.decoded.begin(), animation.decoded.end(),
            [&](const auto& entry) { return entry.first == sprite.layer; });
        if (cached == animation.decoded.end()) {
            Image next;
            try {
                if (sprite.layer) next = decode_layer(*character_sprites, *sprite.layer, *sprite.art, sprite.overlay);
            } catch (const std::runtime_error&) {
                // Keep the record and its timing visible if the sprite cannot be decoded.
            }
            constexpr auto pixel_budget = 8u * 1024 * 1024;
            while (!animation.decoded.empty() && (animation.decoded.size() == 128 ||
                   animation.decoded_pixels + next.pixels.size() > pixel_budget)) {
                animation.decoded_pixels -= animation.decoded.back().second.pixels.size();
                animation.decoded.pop_back();
            }
            animation.decoded_pixels += next.pixels.size();
            animation.decoded.emplace_front(sprite.layer, std::move(next));
        } else {
            animation.decoded.splice(animation.decoded.begin(), animation.decoded, cached);
        }
    }
    animation.record = record;
    auto title = L"Skullgirls Sprite Viewer - " + character_sprites->name + L" | Animation ";
    if (animation.child) title += std::to_wstring(*animation.child) + L":";
    title += std::to_wstring(animation.clip);
    if (!animation.names.empty()) {
        const auto& name = animation.names[animation.clip];
        if (!name.empty()) title += L" " + std::wstring(name.begin(), name.end());
    }
    title += L" | Slot " + std::to_wstring(animation.slot + 1)
        + L" | Record " + std::to_wstring(record + 1) + L"/" + std::to_wstring(clip.size())
        + L" (" + std::to_wstring(clip[record].ticks) + L" ticks)"
        + L" | Default next " + std::to_wstring(animation.successors[animation.clip]);
    SetWindowTextW(window, title.c_str());
    InvalidateRect(window, nullptr, FALSE);
}

// Resolve sprite positions and cumulative frame times whenever the clip or sprite slot changes.
void prepare_clip(HWND window, AnimationPreview& animation, std::size_t record)
{
    const auto& clip = animation.clips[animation.clip];
    animation.sprites.resize(clip.size());
    animation.end_ticks.resize(clip.size());
    animation.left = animation.top = animation.right = animation.bottom = 0;
    SpritePlacement sprite;
    std::uint64_t elapsed = 0;
    for (std::size_t i = 0; i < clip.size(); ++i) {
        const auto reference = clip[i].sprites[animation.slot];
        if (reference) sprite = locate_sprite(*character_sprites, reference);
        animation.sprites[i] = sprite;
        animation.end_ticks[i] = elapsed += std::max(1u, unsigned(clip[i].ticks));
        if (sprite.layer) {
            animation.left = std::min(animation.left, sprite.x);
            animation.top = std::min(animation.top, sprite.y);
            animation.right = std::max(animation.right, sprite.x + sprite.layer->tile_columns * 16);
            animation.bottom = std::max(animation.bottom, sprite.y + sprite.layer->tile_rows * 16);
        }
    }
    animation.record = clip.size();
    show_record(window, animation, record);
}

std::uint64_t animation_elapsed(const AnimationPreview& animation, Clock::time_point now)
{
    const auto elapsed = animation.playing
        ? std::chrono::duration_cast<SpriteTicks>(now - animation.started).count() : 0;
    return animation.tick + static_cast<std::uint64_t>(elapsed);
}

// Use elapsed time to select the current record and follow the default successor when enabled.
void advance_animation(HWND window, AnimationPreview& animation)
{
    if (!animation.playing || !animation.end_ticks.back()) return;
    const auto now = Clock::now();
    auto tick = animation_elapsed(animation, now);
    const auto length = animation.end_ticks.back();
    if (animation.follow && tick >= length) {
        auto successor = animation.successors[animation.clip];
        for (std::size_t remaining = animation.clips.size(); remaining && animation.clips[successor].empty(); --remaining)
            successor = animation.successors[successor];
        if (successor != animation.clip && !animation.clips[successor].empty()) {
            animation.clip = successor;
            animation.tick = tick - length;
            animation.started = now;
            prepare_clip(window, animation, 0);
            tick = std::min(animation.tick, animation.end_ticks.back() - 1);
        }
    }
    const auto found = std::upper_bound(animation.end_ticks.begin(), animation.end_ticks.end(),
                                        tick % animation.end_ticks.back());
    show_record(window, animation, static_cast<std::size_t>(found - animation.end_ticks.begin()));
}

void set_playback(HWND window, bool playing)
{
    if (!character_sprites) return;
    if (auto& animation = character_sprites->animation) {
        if (playing == animation->playing) return;
        if (playing) {
            if (!animation->end_ticks.back() || !SetTimer(window, playback_timer, 10, nullptr)) return;
            animation->started = Clock::now();
            animation->playing = true;
            advance_animation(window, *animation);
            return;
        }
        advance_animation(window, *animation);
        const auto length = animation->end_ticks.back();
        animation->tick = length ? animation_elapsed(*animation, Clock::now()) % length : 0;
        animation->playing = false;
    } else if (playing) {
        SetTimer(window, playback_timer, 125, nullptr);
        return;
    }
    KillTimer(window, playback_timer);
}

void step_record(HWND window, int direction)
{
    set_playback(window, false);
    if (!character_sprites) return;
    if (auto& animation = character_sprites->animation) {
        const auto count = animation->sprites.size();
        const auto record = (animation->record + (direction > 0 ? 1 : count - 1)) % count;
        animation->tick = record ? animation->end_ticks[record - 1] : 0;
        show_record(window, *animation, record);
    } else {
        show_sprite(window, direction);
    }
}

void step_clip(HWND window, AnimationPreview& animation, int direction)
{
    const auto count = animation.clips.size();
    do {
        animation.clip = (animation.clip + (direction > 0 ? 1 : count - 1)) % count;
    } while (animation.clips[animation.clip].empty());
    animation.tick = 0;
    prepare_clip(window, animation, 0);
    animation.started = Clock::now();
}

void open_animation(HWND window, const std::filesystem::path& directory,
                    std::wstring_view character, std::wstring_view index)
{
    try {
        if (character.empty() || character.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz_0123456789") != character.npos)
            throw std::runtime_error("Use a lowercase character asset name");
        const auto number = [](std::wstring_view text) {
            if (text.empty() || text.find_first_not_of(L"0123456789") != text.npos || text.size() > 5)
                throw std::runtime_error("Use a numeric animation index or child:move");
            std::size_t value = 0;
            for (wchar_t digit : text) value = value * 10 + static_cast<unsigned>(digit - L'0');
            return value;
        };
        std::optional<std::size_t> child;
        const auto colon = index.find(L':');
        if (colon != index.npos) {
            child = number(index.substr(0, colon));
            index.remove_prefix(colon + 1);
        }
        const auto clip = number(index);
        std::string name;
        name.reserve(character.size());
        for (wchar_t letter : character) name.push_back(static_cast<char>(letter));
        auto next = load_character(directory, name, true, child);
        auto& animation = *next->animation;
        if (clip >= animation.clips.size()) throw std::runtime_error("Animation index is out of range");
        if (animation.clips[clip].empty()) throw std::runtime_error("This animation has no frame records");
        animation.clip = clip;
        character_sprites = std::move(next);
        prepare_clip(window, animation, 0);
        set_playback(window, true);
    } catch (const std::exception& error) {
        const std::string detail(error.what());
        const std::wstring message(detail.begin(), detail.end());
        MessageBoxW(window, message.c_str(), L"Open animation", MB_OK | MB_ICONERROR);
    }
}

// Switch to image mode only after the selected PAM file has loaded successfully.
void choose_image(HWND window)
{
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) {
        MessageBoxW(window, L"Could not open the file picker.", L"Open sprite", MB_OK | MB_ICONERROR);
        return;
    }
    const COMDLG_FILTERSPEC filter[] = {{L"RGBA PAM images", L"*.pam"}};
    dialog->SetFileTypes(ARRAYSIZE(filter), filter);
    if (SUCCEEDED(dialog->Show(window))) {
        IShellItem* item = nullptr;
        PWSTR path = nullptr;
        const bool loaded = SUCCEEDED(dialog->GetResult(&item)) &&
                            SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) &&
                            load_image(path);
        CoTaskMemFree(path);
        if (item) item->Release();
        if (loaded) {
            KillTimer(window, playback_timer);
            character_sprites.reset();
            SetWindowTextW(window, L"Skullgirls Sprite Viewer - Preview");
            InvalidateRect(window, nullptr, FALSE);
        } else {
            MessageBoxW(window, L"Could not read the RGBA PAM image.", L"Open sprite", MB_OK | MB_ICONERROR);
        }
    }
    dialog->Release();
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* animation = character_sprites && character_sprites->animation
        ? &*character_sprites->animation : nullptr;
    switch (message) {
    case WM_INITMENUPOPUP: {
        const auto submenu = reinterpret_cast<HMENU>(wparam);
        for (UINT command = previous_animation; command <= follow_successor; ++command)
            EnableMenuItem(submenu, command, MF_BYCOMMAND | (animation ? MF_ENABLED : MF_GRAYED));
        if (animation) {
            CheckMenuRadioItem(submenu, first_sprite_slot, first_sprite_slot + 2,
                               first_sprite_slot + static_cast<UINT>(animation->slot), MF_BYCOMMAND);
            CheckMenuItem(submenu, follow_successor,
                          MF_BYCOMMAND | (animation->follow ? MF_CHECKED : MF_UNCHECKED));
        }
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case open_image: choose_image(window); return 0;
        case play_sprite: set_playback(window, true); return 0;
        case pause_playback: set_playback(window, false); return 0;
        case previous_sprite:
        case next_sprite:
            step_record(window, LOWORD(wparam) == next_sprite ? 1 : -1);
            return 0;
        case previous_animation:
        case next_animation:
            if (animation) step_clip(window, *animation, LOWORD(wparam) == next_animation ? 1 : -1);
            return 0;
        case follow_successor:
            if (animation) {
                const auto now = Clock::now();
                animation->tick = animation_elapsed(*animation, now) % animation->end_ticks.back();
                animation->started = now;
                animation->follow = !animation->follow;
            }
            return 0;
        case first_sprite_slot:
        case first_sprite_slot + 1:
        case first_sprite_slot + 2:
            if (animation) {
                animation->slot = LOWORD(wparam) - first_sprite_slot;
                prepare_clip(window, *animation, animation->record);
            }
            return 0;
        case exit_viewer: DestroyWindow(window); return 0;
        }
        break;
    case WM_KEYDOWN:
        switch (wparam) {
        case VK_LEFT: step_record(window, -1); return 0;
        case VK_RIGHT: step_record(window, 1); return 0;
        case VK_PRIOR:
        case VK_NEXT:
            if (animation) step_clip(window, *animation, wparam == VK_NEXT ? 1 : -1);
            return 0;
        case VK_SPACE:
            if (animation && !(lparam & (1L << 30))) set_playback(window, !animation->playing);
            return 0;
        }
        break;
    case WM_TIMER:
        if (wparam == playback_timer) {
            if (animation) advance_animation(window, *animation);
            else show_sprite(window, 1);
        }
        return 0;
    case WM_SIZE:
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT bounds{};
        GetClientRect(window, &bounds);
        const auto background = GetSysColorBrush(COLOR_WINDOW);
        const auto& displayed = animation && !animation->decoded.empty() ? animation->decoded.front().second : image;
        if (displayed.pixels.empty()) {
            FillRect(dc, &bounds, background);
            DrawTextW(dc, animation ? L"No drawable sprite in this record" : L"Open a sprite from the File menu", -1, &bounds,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else if (bounds.right > 0 && bounds.bottom > 0) {
            const auto view_width = animation ? animation->right - animation->left : displayed.width;
            const auto view_height = animation ? animation->bottom - animation->top : displayed.height;
            const auto scale = std::min({1.0, double(bounds.right) / view_width,
                                        double(bounds.bottom) / view_height});
            const auto width = std::max(1, static_cast<int>(displayed.width * scale));
            const auto height = std::max(1, static_cast<int>(displayed.height * scale));
            int x = (bounds.right - static_cast<int>(view_width * scale)) / 2;
            int y = (bounds.bottom - static_cast<int>(view_height * scale)) / 2;
            if (animation) {
                const auto& sprite = animation->sprites[animation->record];
                x += static_cast<int>((sprite.x - animation->left) * scale);
                y += static_cast<int>((sprite.y - animation->top) * scale);
            }
            const RECT border[] = {{0, 0, bounds.right, y}, {0, y + height, bounds.right, bounds.bottom},
                                   {0, y, x, y + height}, {x + width, y, bounds.right, y + height}};
            for (const auto& rect : border) FillRect(dc, &rect, background);
            BITMAPINFO bitmap{};
            bitmap.bmiHeader.biSize = sizeof(bitmap.bmiHeader);
            bitmap.bmiHeader.biWidth = displayed.width;
            bitmap.bmiHeader.biHeight = -displayed.height;
            bitmap.bmiHeader.biPlanes = 1;
            bitmap.bmiHeader.biBitCount = 32;
            bitmap.bmiHeader.biCompression = BI_RGB;
            StretchDIBits(dc, x, y, width, height, 0, 0, displayed.width, displayed.height,
                          displayed.pixels.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show)
{
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    window_class.lpszClassName = L"SkullgirlsSpriteViewer";
    if (!RegisterClassW(&window_class)) return 1;

    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU playback = CreatePopupMenu();
    if (!menu || !file ||
        !AppendMenuW(file, MF_STRING, open_image, L"&Open sprite...") ||
        !AppendMenuW(file, MF_SEPARATOR, 0, nullptr) ||
        !AppendMenuW(file, MF_STRING, exit_viewer, L"E&xit") ||
        !AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File") ||
        !playback ||
        !AppendMenuW(playback, MF_STRING, play_sprite, L"&Play") ||
        !AppendMenuW(playback, MF_STRING, pause_playback, L"P&ause") ||
        !AppendMenuW(playback, MF_STRING, previous_sprite, L"&Previous frame") ||
        !AppendMenuW(playback, MF_STRING, next_sprite, L"&Next frame") ||
        !AppendMenuW(playback, MF_SEPARATOR, 0, nullptr) ||
        !AppendMenuW(playback, MF_STRING, previous_animation, L"Previous &animation") ||
        !AppendMenuW(playback, MF_STRING, next_animation, L"Next a&nimation") ||
        !AppendMenuW(playback, MF_STRING, follow_successor, L"Follow &default successor") ||
        !AppendMenuW(playback, MF_SEPARATOR, 0, nullptr) ||
        !AppendMenuW(playback, MF_STRING, first_sprite_slot, L"Sprite slot &1") ||
        !AppendMenuW(playback, MF_STRING, first_sprite_slot + 1, L"Sprite slot &2") ||
        !AppendMenuW(playback, MF_STRING, first_sprite_slot + 2, L"Sprite slot &3") ||
        !AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(playback), L"&Playback")) return 1;

    HWND window = CreateWindowExW(0, window_class.lpszClassName, L"Skullgirls Sprite Viewer",
                                 WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                                 nullptr, menu, instance, nullptr);
    if (!window) return 1;
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(com)) return 1;
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc == 3 && std::wstring(argv[1]) == L"--filia") {
        open_filia(window, argv[2]);
        set_playback(window, true);
    } else if (argv && argc > 1 && std::wstring_view(argv[1]) == L"--animation") {
        if (argc == 4 || argc == 5)
            open_animation(window, argv[2], argv[3], argc == 5 ? argv[4] : L"0");
        else
            MessageBoxW(window, L"Use --animation <data-directory> <character> [[child:]move].",
                        L"Open animation", MB_OK | MB_ICONERROR);
    } else if (argv && argc == 2) {
        if (load_image(argv[1])) {
            SetWindowTextW(window, L"Skullgirls Sprite Viewer - Preview");
        } else {
            MessageBoxW(window, L"Could not read the RGBA PAM image.", L"Open sprite", MB_OK | MB_ICONERROR);
        }
    }
    LocalFree(argv);
    ShowWindow(window, show);

    MSG message{};
    int status;
    while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    CoUninitialize();
    return status < 0 ? 1 : static_cast<int>(message.wParam);
}
