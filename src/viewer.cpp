#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>
#include "foit.hpp"
#include "sprite_asset.hpp"

namespace {

constexpr UINT open_image = 1;
constexpr UINT exit_viewer = 2;
constexpr UINT play_filia = 3;
constexpr UINT pause_playback = 4;
constexpr UINT previous_sprite = 5;
constexpr UINT next_sprite = 6;
constexpr UINT_PTR playback_timer = 1;

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

struct FiliaSprites {
    skullgirls::SpriteBytes main_art, alternate_art, palette;
    std::vector<skullgirls::SpriteFrame> frames;
    std::vector<std::pair<std::size_t, std::size_t>> layers;
    std::size_t current = 0;
    std::vector<bool> unavailable;
};

std::unique_ptr<FiliaSprites> filia;

void show_filia_sprite(HWND window, int direction)
{
    if (!filia) return;
    auto& sprites = *filia;
    const auto count = sprites.layers.size();
    for (std::size_t attempts = 0; attempts < count; ++attempts) {
        if (direction > 0) sprites.current = (sprites.current + 1) % count;
        if (direction < 0) sprites.current = (sprites.current + count - 1) % count;
        if (direction == 0) direction = 1;
        if (sprites.unavailable[sprites.current]) continue;
        const auto [frame_index, slot] = sprites.layers[sprites.current];
        const auto& frame = sprites.frames[frame_index];
        const auto& layer = *frame.layers[slot];
        const auto& art = frame.art_bank == 4 ? sprites.alternate_art : sprites.main_art;
        try {
            const auto raw = skullgirls::decode_sprite_layer(layer, art);
            const auto rgba = skullgirls::colorize_sprite_with_palette(raw, sprites.palette);
            Image next;
            next.width = layer.tile_columns * 16;
            next.height = layer.tile_rows * 16;
            next.pixels.resize(rgba.size() / 4);
            for (int y = 0; y < next.height; ++y) {
                for (int x = 0; x < next.width; ++x) {
                    const auto pixel = y * next.width + x;
                    const auto* p = rgba.data() + 4 * pixel;
                    const unsigned background = ((x / 8 + y / 8) % 2) ? 192 : 224;
                    const auto blend = [&](unsigned c) {
                        return (p[c] * p[3] + background * (255 - p[3]) + 127) / 255;
                    };
                    next.pixels[pixel] = (blend(0) << 16) | (blend(1) << 8) | blend(2);
                }
            }
            image = std::move(next);
            const auto title = L"Skullgirls Sprite Viewer - Filia | Frame " + std::to_wstring(frame_index)
                + L", layer " + std::to_wstring(slot) + L" | "
                + std::to_wstring(sprites.current + 1) + L"/" + std::to_wstring(count);
            SetWindowTextW(window, title.c_str());
            InvalidateRect(window, nullptr, TRUE);
            return;
        } catch (const std::runtime_error&) {
            sprites.unavailable[sprites.current] = true;
        }
    }
    KillTimer(window, playback_timer);
    MessageBoxW(window, L"No supported Filia sprite layers could be decoded.", L"Filia", MB_OK | MB_ICONERROR);
}

void open_filia(HWND window, const std::filesystem::path& directory)
{
    using skullgirls::read_sprite_asset;
    try {
        auto next = std::make_unique<FiliaSprites>();
        next->main_art = read_sprite_asset(directory / "characters-art.gfs", "characters/filia.art");
        next->alternate_art = read_sprite_asset(directory / "characters-art-pt.gfs", "characters/filia.art-pt");
        const auto foit = read_sprite_asset(directory / "characters-foits.gfs", "characters/filia.foit");
        next->palette = skullgirls::is_versioned_foit(foit)
            ? skullgirls::read_dds_palette(read_sprite_asset(directory / "sprites.gfs", "sprites/filia_1p.dds"))
            : skullgirls::read_legacy_palette(foit);
        next->frames = skullgirls::read_foit_frames(foit, next->main_art.size());
        for (std::size_t i = 0; i < next->frames.size(); ++i) {
            const auto& frame = next->frames[i];
            if (frame.art_bank != 4 && frame.art_bank != 5) continue;
            for (std::size_t slot = 0; slot < frame.layers.size(); ++slot) {
                const auto& layer = frame.layers[slot];
                if (layer && layer->used_tiles) next->layers.emplace_back(i, slot);
            }
        }
        if (next->layers.empty()) throw std::runtime_error("No Filia layers");
        next->unavailable.resize(next->layers.size());
        filia = std::move(next);
        show_filia_sprite(window, 0);
    } catch (const std::exception&) {
        MessageBoxW(window, L"Could not load Filia from the Skullgirls data directory.", L"Filia", MB_OK | MB_ICONERROR);
    }
}

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
            filia.reset();
            SetWindowTextW(window, L"Skullgirls Sprite Viewer - Preview");
            InvalidateRect(window, nullptr, TRUE);
        } else {
            MessageBoxW(window, L"Could not read the RGBA PAM image.", L"Open sprite", MB_OK | MB_ICONERROR);
        }
    }
    dialog->Release();
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case open_image: choose_image(window); return 0;
        case play_filia:
            if (filia) SetTimer(window, playback_timer, 125, nullptr);
            return 0;
        case pause_playback: KillTimer(window, playback_timer); return 0;
        case previous_sprite:
        case next_sprite:
            KillTimer(window, playback_timer);
            show_filia_sprite(window, LOWORD(wparam) == next_sprite ? 1 : -1);
            return 0;
        case exit_viewer: DestroyWindow(window); return 0;
        }
        break;
    case WM_TIMER:
        if (wparam == playback_timer) show_filia_sprite(window, 1);
        return 0;
    case WM_SIZE:
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT bounds{};
        GetClientRect(window, &bounds);
        if (image.pixels.empty()) {
            DrawTextW(dc, L"Open a sprite from the File menu", -1, &bounds,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else if (bounds.right > 0 && bounds.bottom > 0) {
            const auto scale = std::min({1.0, double(bounds.right) / image.width,
                                        double(bounds.bottom) / image.height});
            const auto width = std::max(1, static_cast<int>(image.width * scale));
            const auto height = std::max(1, static_cast<int>(image.height * scale));
            BITMAPINFO bitmap{};
            bitmap.bmiHeader.biSize = sizeof(bitmap.bmiHeader);
            bitmap.bmiHeader.biWidth = image.width;
            bitmap.bmiHeader.biHeight = -image.height;
            bitmap.bmiHeader.biPlanes = 1;
            bitmap.bmiHeader.biBitCount = 32;
            bitmap.bmiHeader.biCompression = BI_RGB;
            StretchDIBits(dc, (bounds.right - width) / 2, (bounds.bottom - height) / 2,
                          width, height, 0, 0, image.width, image.height,
                          image.pixels.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
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
        !AppendMenuW(playback, MF_STRING, play_filia, L"&Play") ||
        !AppendMenuW(playback, MF_STRING, pause_playback, L"P&ause") ||
        !AppendMenuW(playback, MF_STRING, previous_sprite, L"&Previous sprite") ||
        !AppendMenuW(playback, MF_STRING, next_sprite, L"&Next sprite") ||
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
        if (filia) SetTimer(window, playback_timer, 125, nullptr);
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
