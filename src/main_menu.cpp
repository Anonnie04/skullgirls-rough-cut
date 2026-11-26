// Shared preview screens and Training Room simulation, with native platform backends.

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#else
#include "linux_platform.hpp"
#endif

#include "sprite_asset.hpp"
#include "audio.hpp"
#include "audio_playback.hpp"
#include "menu_timings.hpp"
#include "title_scene.hpp"
#include "fighter_select.hpp"
#include "select_idle.hpp"
#include "foit.hpp"
#include "foit_script.hpp"
#include "cerebella.hpp"
#include "peacock.hpp"
#include "parasoul.hpp"
#include "valentine.hpp"
#include "fukua.hpp"
#include "squigly.hpp"
#include "msfortune.hpp"
#include "robofortune.hpp"
#include "painwheel.hpp"
#include "foit_effects.hpp"
#include "filia_movement.hpp"
#include "filia_hurt.hpp"
#include "filia_defense.hpp"
#include "filia_reactions.hpp"
#include "filia_presentation.hpp"
#include "filia_team.hpp"
#include "gfs.hpp"
#include "runtime_data.hpp"
#include "netplay_session.hpp"
#include "hud_portrait.hpp"
#include "combat_scene.hpp"
#include "stage_select.hpp"
#include "stage_scene.hpp"
#include "sprite_gpu.hpp"
#include <tuple>
#include <iomanip>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <array>
#include <stdexcept>
#include <sstream>
#include <random>
#include <memory>
#include <future>
#include <map>
#include <string>
#include <vector>

namespace {

// Owns the decoded texture and the Windows handles used to draw it.
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

    enum class Encoding { rgba, stage_rgb, straight_rgba };

    struct DdsLayout {
        unsigned width, height, block_size;
    };

    static unsigned word(const skullgirls::SpriteBytes& bytes, std::size_t pos, unsigned length) {
        unsigned value = 0;
        for (unsigned i = 0; i < length; ++i) value |= unsigned(bytes[pos + i]) << (8 * i);
        return value;
    }

    static DdsLayout dds_layout(const skullgirls::SpriteBytes& dds) {
        if (dds.size() < 128 || word(dds, 0, 4) != 0x20534444 ||
            (word(dds, 84, 4) != 0x35545844 && word(dds, 84, 4) != 0x31545844))
            throw std::runtime_error("Expected a DXT1 or DXT5 texture");
        const unsigned block_size = word(dds, 84, 4) == 0x35545844 ? 16 : 8;
        const auto w = word(dds, 16, 4), h = word(dds, 12, 4);
        if (!w || !h || w > 4096 || h > 4096 ||
            ((w + 3) / 4) * ((h + 3) / 4) > (dds.size() - 128) / block_size)
            throw std::runtime_error("Truncated texture");
        return {w, h, block_size};
    }

    void load(const skullgirls::SpriteBytes& dds, Encoding encoding = Encoding::rgba,
              unsigned max_extent = 4096) {
        const auto [w, h, block_size] = dds_layout(dds);
        unsigned sample = 1;
        while ((std::max(w, h) + sample - 1) / sample > max_extent) sample *= 2;
        width = (w + sample - 1) / sample;
        height = (h + sample - 1) / sample;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        void* memory = nullptr;
        bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &memory, nullptr, 0);
        if (!bitmap) throw std::runtime_error("Cannot allocate menu texture");
        decode_dds(dds, {w, h, block_size}, encoding, sample, static_cast<unsigned*>(memory));
    }

    static void decode_dds(const skullgirls::SpriteBytes& dds, DdsLayout layout, Encoding encoding,
                           unsigned sample, unsigned* pixels) {
        const auto [w, h, block_size] = layout;
        const bool bc3 = block_size == 16;
        const auto stride = (w + sample - 1) / sample;
        const auto read_word = [&](std::size_t pos, unsigned length) { return word(dds, pos, length); };
        std::size_t pos = 128;
        for (unsigned y = 0; y < h; y += 4) {
            for (unsigned x = 0; x < w; x += 4, pos += block_size) {
                if (sample > 4 && (x % sample || y % sample)) continue;
                unsigned alpha[8] = {dds[pos], dds[pos + 1]};
                const auto levels = alpha[0] > alpha[1] ? 7u : 5u;
                for (unsigned i = 1; i < levels; ++i)
                    alpha[i + 1] = ((levels - i) * alpha[0] + i * alpha[1]) / levels;
                if (levels == 5) alpha[6] = 0, alpha[7] = 255;
                const auto alpha_codes = bc3 ? read_word(pos + 2, 3) | (std::uint64_t(read_word(pos + 5, 3)) << 24) : 0;
                const auto colors = pos + (bc3 ? 8 : 0);
                const auto color_codes = read_word(colors + 4, 4);
                const bool four_colors = bc3 || read_word(colors, 2) > read_word(colors + 2, 2);
                unsigned color[4][3]{};
                for (unsigned i = 0; i < 2; ++i) {
                    const auto rgb = read_word(colors + i * 2, 2);
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
                    if (px >= w || py >= h || px % sample || py % sample) continue;
                    auto& pixel = pixels[(py / sample) * stride + px / sample];
                    const auto index = (color_codes >> (2 * i)) & 3;
                    const auto a = bc3 ? alpha[(alpha_codes >> (3 * i)) & 7] : (!four_colors && index == 3 ? 0u : 255u);
                    const auto& rgb = color[index];
                    if (encoding == Encoding::stage_rgb && bc3) {
                        // Flat stage backgrounds use alpha, red and green as the three color channels.
                        pixel = 0xff000000u | (a << 16) | (rgb[2] << 8) | rgb[1];
                        continue;
                    }
                    const auto scale = encoding == Encoding::straight_rgba ? 255 : a;
                    pixel = (a << 24) | ((rgb[2] * scale / 255) << 16)
                        | ((rgb[1] * scale / 255) << 8) | (rgb[0] * scale / 255);
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

struct TiledAnimation {
    struct Frame {
        unsigned first, count, ticks;
        float origin_x, origin_y;
    };
    struct Tile { unsigned x, y, u, v; };
    struct Image {
        float x, y;
        std::vector<unsigned> pixels;
        std::unique_ptr<Gdiplus::Bitmap> bitmap;
#ifdef _WIN32
        mutable skullgirls::SpriteGpu::Texture gpu;
#endif
    };
    std::vector<Frame> frames;
    std::vector<Image> images;
    unsigned ticks = 0;

    void load(const std::filesystem::path& archive, const std::string& sprite, std::string_view sequence) {
        skullgirls::SpriteArchive assets(archive);
        load(assets, sprite, sequence);
    }

    void load(skullgirls::SpriteArchive& archive, const std::string& sprite, std::string_view sequence) {
        const auto path = "sprites/" + sprite;
        const auto bytes = archive.read(path + ".spr.msb");
        skullgirls::StageReader reader(bytes);
        if (reader.string() != "2.0" || reader.string() != sprite)
            throw std::runtime_error("Unsupported tiled sprite");
        reader.skip(4);
        if (reader.string() != "unigned char tile_x, tile_y, tile_u, tile_v;" || reader.word(8) != 4)
            throw std::runtime_error("Unsupported loading tile format");
        const auto tile_count = reader.word(8);
        const auto frame_count = reader.word(8);
        const auto sequence_count = reader.word(8);
        if (tile_count > bytes.size() / 4 || frame_count > bytes.size() / 20 ||
            sequence_count > bytes.size() / 24 || reader.word(8) != 16 || reader.word(8) != 16)
            throw std::runtime_error("Invalid loading sprite layout");
        Texture atlas;
        atlas.load(archive.read(path + ".dds"));
        std::vector<Tile> tiles;
        tiles.reserve(static_cast<std::size_t>(tile_count));
        for (std::uint64_t i = 0; i < tile_count; ++i) {
            const Tile tile{static_cast<unsigned>(reader.word(1)), static_cast<unsigned>(reader.word(1)),
                            static_cast<unsigned>(reader.word(1)), static_cast<unsigned>(reader.word(1))};
            if ((tile.u + 1) * 16 > unsigned(atlas.width) || (tile.v + 1) * 16 > unsigned(atlas.height))
                throw std::runtime_error("Loading tile outside atlas");
            tiles.push_back(tile);
        }
        std::vector<Frame> all_frames;
        for (std::uint64_t i = 0; i < frame_count; ++i) {
            const Frame frame{static_cast<unsigned>(reader.word(4)), static_cast<unsigned>(reader.word(4)),
                              static_cast<unsigned>(reader.word(4)), reader.real(), reader.real()};
            if (frame.first > tile_count || frame.count > tile_count - frame.first ||
                !frame.ticks || frame.ticks > 65535 || !std::isfinite(frame.origin_x) ||
                !std::isfinite(frame.origin_y))
                throw std::runtime_error("Invalid loading sprite frame");
            all_frames.push_back(frame);
        }
        if (!sequence_count) throw std::runtime_error("Missing tiled sprite sequence");
        for (std::uint64_t i = 0; i < sequence_count; ++i) {
            const auto name = reader.string();
            const auto range = reader.word(8), loop_end = reader.word(8);
            const auto first = range >> 32, count = range & 0xffffffffu;
            if (!count || first > frame_count || count > frame_count - first || loop_end != first + count - 1)
                throw std::runtime_error("Invalid tiled animation sequence");
            if (frames.empty() && (sequence.empty() || name == sequence))
                frames.assign(all_frames.begin() + first, all_frames.begin() + first + count);
        }
        if (frames.empty()) throw std::runtime_error("Missing tiled sprite sequence");
        DIBSECTION dib{};
        if (!GetObjectW(atlas.bitmap, sizeof(dib), &dib))
            throw std::runtime_error("Cannot read loading atlas pixels");
        const auto* atlas_pixels = static_cast<const unsigned*>(dib.dsBm.bmBits);
        images.reserve(frames.size());
        for (const auto& frame : frames) {
            ticks += frame.ticks;
            unsigned left = frame.count ? 4096 : 0, top = frame.count ? 4096 : 0;
            unsigned right = 16, bottom = 16;
            for (unsigned i = frame.first; i < frame.first + frame.count; ++i) {
                left = std::min(left, tiles[i].x * 16);
                top = std::min(top, tiles[i].y * 16);
                right = std::max(right, (tiles[i].x + 1) * 16);
                bottom = std::max(bottom, (tiles[i].y + 1) * 16);
            }
            const auto width = right - left, height = bottom - top;
            Image image{float(left) - frame.origin_x, float(top) - frame.origin_y};
            image.pixels.resize(width * height);
            for (unsigned i = frame.first; i < frame.first + frame.count; ++i) {
                const auto& tile = tiles[i];
                for (unsigned y = 0; y < 16; ++y)
                    std::copy_n(atlas_pixels + (tile.v * 16 + y) * atlas.width + tile.u * 16, 16,
                                image.pixels.data() + (tile.y * 16 + y - top) * width + tile.x * 16 - left);
            }
            image.bitmap = std::make_unique<Gdiplus::Bitmap>(width, height, width * 4,
                PixelFormat32bppPARGB, reinterpret_cast<BYTE*>(image.pixels.data()));
            if (image.bitmap->GetLastStatus() != Gdiplus::Ok)
                throw std::runtime_error("Cannot prepare loading animation bitmap");
            images.push_back(std::move(image));
        }
    }

    void draw(Gdiplus::Graphics& dc, std::uint64_t tick) const {
        dc.Clear(Gdiplus::Color::Black);
        if (ticks) draw_at(dc, tick % ticks, 640, 360);
    }

    void draw_at(Gdiplus::Graphics& dc, std::uint64_t tick, float x, float y, float scale = 1.0f
#ifdef _WIN32
                 ,skullgirls::SpriteGpu* gpu=nullptr
#endif
                 ) const {
        if (tick >= ticks) return;
        auto remaining = tick;
        std::size_t index = 0;
        while (remaining >= frames[index].ticks) remaining -= frames[index++].ticks;
        const auto& image = images[index];
#ifdef _WIN32
        if(gpu) {
            gpu->queue(dc,*image.bitmap,{x+image.x*scale,y+image.y*scale,
                image.bitmap->GetWidth()*scale,image.bitmap->GetHeight()*scale},{1,1,1,1},image.gpu);
            return;
        }
#endif
        dc.DrawImage(image.bitmap.get(), x + image.x * scale, y + image.y * scale,
                     image.bitmap->GetWidth() * scale, image.bitmap->GetHeight() * scale);
    }

    void draw_frame(Gdiplus::Graphics& dc, unsigned frame, float x, float y,
                    float scale_x, float scale_y, bool mirror, float rotation, float alpha
#ifdef _WIN32
                    ,skullgirls::SpriteGpu* gpu=nullptr
#endif
                    ) const {
        if (frame >= images.size() || scale_x == 0 || scale_y == 0 || alpha <= 0) return;
        const auto saved = dc.Save();
        dc.TranslateTransform(x, y);
        dc.RotateTransform(rotation);
        dc.ScaleTransform(mirror ? -scale_x : scale_x, scale_y);
        const auto& image = images[frame];
#ifdef _WIN32
        if(gpu) {
            try {
                gpu->queue(dc,*image.bitmap,{image.x,image.y,float(image.bitmap->GetWidth()),float(image.bitmap->GetHeight())},
                    {1,1,1,std::clamp(alpha,0.0f,1.0f)},image.gpu);
                dc.Restore(saved);return;
            } catch(...) {dc.Restore(saved);throw;}
        }
#endif
        if (alpha>=1) {
            dc.DrawImage(image.bitmap.get(),Gdiplus::RectF(image.x,image.y,
                float(image.bitmap->GetWidth()),float(image.bitmap->GetHeight())));
            dc.Restore(saved);
            return;
        }
        Gdiplus::ColorMatrix color{};
        color.m[0][0] = color.m[1][1] = color.m[2][2] = color.m[4][4] = 1;
        color.m[3][3] = std::clamp(alpha, 0.0f, 1.0f);
        Gdiplus::ImageAttributes attributes;
        attributes.SetColorMatrix(&color);
        const auto width = image.bitmap->GetWidth(), height = image.bitmap->GetHeight();
        dc.DrawImage(image.bitmap.get(), Gdiplus::RectF(image.x, image.y, float(width), float(height)),
            0, 0, float(width), float(height), Gdiplus::UnitPixel, &attributes);
        dc.Restore(saved);
    }
};

#ifdef _WIN32
using Microsoft::WRL::ComPtr;

struct StageGpu {
    struct Vertex { float x, y, z; std::uint32_t color; float u, v; };
    struct Mesh {
        ComPtr<ID3D11Buffer> vertices, indices;
        ComPtr<ID3D11ShaderResourceView> texture;
    };
    struct Constants {
        float center, origin_y, distance, near_clip;
        float horizontal, vertical, depth_scale, opaque;
        float uv_u[4], uv_v[4];
    };
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> target, staging;
    ComPtr<ID3D11RenderTargetView> target_view;
    ComPtr<ID3D11ShaderResourceView> target_texture;
    std::uint64_t readbacks=0;
    int target_width=0,target_height=0;
    ComPtr<ID3D11DepthStencilView> depth_view;
    ComPtr<ID3D11VertexShader> vertex_shader;
    ComPtr<ID3D11PixelShader> pixel_shader;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11RasterizerState> raster;
    std::array<ComPtr<ID3D11DepthStencilState>, 2> depth_modes;
    std::array<ComPtr<ID3D11BlendState>, 2> blend_modes;
    std::array<ComPtr<ID3D11SamplerState>, 8> filters;
    std::vector<Mesh> buffers;
    bool warp = false;

    static void check(HRESULT result, const char* operation) {
        if (FAILED(result))
            throw std::runtime_error(std::string("Direct3D stage ") + operation + " failed (" +
                                     std::to_string(static_cast<unsigned long>(result)) + ")");
    }

    static ComPtr<ID3DBlob> compile(const char* source, const char* entry, const char* version) {
        ComPtr<ID3DBlob> bytes, errors;
        const auto result = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry,
            version, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &bytes, &errors);
        if (FAILED(result) && errors) OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
        check(result, "shader compilation");
        return bytes;
    }

    StageGpu(int width, int height, const std::vector<skullgirls::StageMesh>& meshes,
             const std::map<std::string, skullgirls::SpriteBytes>& textures,
             ID3D11Device* shared_device=nullptr,ID3D11DeviceContext* shared_context=nullptr) {
        target_width=width;target_height=height;
        HRESULT result=S_OK;
        if(shared_device) {device=shared_device;context=shared_context;}
        else result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context);
        if (FAILED(result)) {
            warp = true;
            result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context);
        }
        check(result, "device creation");
        D3D11_TEXTURE2D_DESC surface{};
        surface.Width = width;
        surface.Height = height;
        surface.MipLevels = surface.ArraySize = surface.SampleDesc.Count = 1;
        surface.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        surface.BindFlags = D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        check(device->CreateTexture2D(&surface, nullptr, &target), "render target");
        check(device->CreateRenderTargetView(target.Get(), nullptr, &target_view), "render target view");
        check(device->CreateShaderResourceView(target.Get(),nullptr,&target_texture),"stage texture view");
        surface.BindFlags = 0;
        surface.Usage = D3D11_USAGE_STAGING;
        surface.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        check(device->CreateTexture2D(&surface, nullptr, &staging), "readback texture");
        surface.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        surface.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        surface.Usage = D3D11_USAGE_DEFAULT;
        surface.CPUAccessFlags = 0;
        ComPtr<ID3D11Texture2D> depth_buffer;
        check(device->CreateTexture2D(&surface, nullptr, &depth_buffer), "depth buffer");
        check(device->CreateDepthStencilView(depth_buffer.Get(), nullptr, &depth_view), "depth buffer view");

        constexpr auto shader = R"(
cbuffer Stage : register(b0) { float4 camera; float4 projection; float4 uv_u; float4 uv_v; };
Texture2D surface : register(t0);
SamplerState texture_filter : register(s0);
struct Input { float3 p : POSITION; float4 color : COLOR; float2 uv : TEXCOORD; };
struct Pixel { float4 p : SV_POSITION; float4 color : COLOR; float2 uv : TEXCOORD; };
Pixel project(Input i) {
    Pixel pixel;
    float depth = camera.z - i.p.z;
    pixel.p = float4((i.p.x - camera.x) * projection.x, (i.p.y - camera.y) * projection.y,
                     (depth - camera.w) * projection.z, depth);
    pixel.color = i.color;
    float3 uv = float3(i.uv, 1);
    pixel.uv = float2(dot(uv, uv_u.xyz), 1 - dot(uv, uv_v.xyz));
    return pixel;
}
float4 shade(Pixel i) : SV_TARGET {
    float4 color = surface.Sample(texture_filter, i.uv) * i.color;
    if (projection.w > 0) clip(color.a - 0.9999);
    else {
        clip(0.9999 - color.a);
        clip(color.a - 0.0001);
    }
    return color;
}
)";
        const auto vs = compile(shader, "project", "vs_4_0");
        const auto ps = compile(shader, "shade", "ps_4_0");
        check(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertex_shader),
              "vertex shader");
        check(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixel_shader),
              "pixel shader");
        constexpr D3D11_INPUT_ELEMENT_DESC inputs[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        check(device->CreateInputLayout(inputs, static_cast<UINT>(std::size(inputs)),
            vs->GetBufferPointer(), vs->GetBufferSize(), &layout), "vertex layout");
        D3D11_BUFFER_DESC constant_spec{};
        constant_spec.ByteWidth = sizeof(Constants);
        constant_spec.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        check(device->CreateBuffer(&constant_spec, nullptr, &constants), "constants");
        D3D11_RASTERIZER_DESC raster_spec{};
        raster_spec.FillMode = D3D11_FILL_SOLID;
        raster_spec.CullMode = D3D11_CULL_NONE;
        raster_spec.DepthClipEnable = true;
        check(device->CreateRasterizerState(&raster_spec, &raster), "rasterizer");
        for (std::size_t mode = 0; mode < depth_modes.size(); ++mode) {
            D3D11_DEPTH_STENCIL_DESC depth_spec{};
            depth_spec.DepthEnable = true;
            depth_spec.DepthWriteMask = mode ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
            depth_spec.DepthFunc = D3D11_COMPARISON_LESS;
            check(device->CreateDepthStencilState(&depth_spec, &depth_modes[mode]), "depth state");
            D3D11_BLEND_DESC blend_spec{};
            auto& blend = blend_spec.RenderTarget[0];
            blend.BlendEnable = mode == 0;
            blend.SrcBlend = D3D11_BLEND_SRC_ALPHA;
            blend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            blend.BlendOp = D3D11_BLEND_OP_ADD;
            blend.SrcBlendAlpha = D3D11_BLEND_ONE;
            blend.DestBlendAlpha = D3D11_BLEND_ZERO;
            blend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
                                          D3D11_COLOR_WRITE_ENABLE_BLUE;
            check(device->CreateBlendState(&blend_spec, &blend_modes[mode]), "blend state");
        }
        for (unsigned mode = 0; mode < filters.size(); ++mode) {
            D3D11_SAMPLER_DESC sampler{};
            sampler.Filter = (mode & 1) ? D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_POINT;
            sampler.AddressU = (mode & 2) ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.AddressV = (mode & 4) ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
            sampler.MaxLOD = D3D11_FLOAT32_MAX;
            check(device->CreateSamplerState(&sampler, &filters[mode]), "texture sampler");
        }

        const auto upload = [&](const void* data, UINT size, UINT binding) {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = size;
            desc.BindFlags = binding;
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            const D3D11_SUBRESOURCE_DATA initial{data, 0, 0};
            ComPtr<ID3D11Buffer> buffer;
            check(device->CreateBuffer(&desc, &initial, &buffer), "geometry upload");
            return buffer;
        };
        std::map<std::string, ComPtr<ID3D11ShaderResourceView>> uploaded;
        for (const auto& mesh : meshes) {
            auto& texture = uploaded[mesh.material];
            if (!texture) {
                const auto& source = textures.at(mesh.material);
                const auto layout = Texture::dds_layout(source);
                const auto [w, h, block_size] = layout;
                D3D11_TEXTURE2D_DESC desc{};
                desc.Width = w;
                desc.Height = h;
                desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
                desc.Format = block_size == 16 ? DXGI_FORMAT_BC3_UNORM : DXGI_FORMAT_BC1_UNORM;
                desc.Usage = D3D11_USAGE_IMMUTABLE;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA initial{source.data() + 128, ((w + 3) / 4) * block_size, 0};
                std::vector<unsigned> decoded;
                // Direct3D requires block-aligned dimensions for BC textures.
                if (w % 4 || h % 4) {
                    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                    decoded.resize(w * h);
                    Texture::decode_dds(source, layout, Texture::Encoding::straight_rgba, 1, decoded.data());
                    initial = {decoded.data(), w * sizeof(unsigned), 0};
                }
                ComPtr<ID3D11Texture2D> resource;
                check(device->CreateTexture2D(&desc, &initial, &resource), "texture upload");
                check(device->CreateShaderResourceView(resource.Get(), nullptr, &texture), "texture view");
            }
            std::vector<Vertex> vertices;
            vertices.reserve(mesh.vertices.size());
            for (const auto& v : mesh.vertices) {
                const auto rgba = (v.rgba >> 24) | ((v.rgba >> 8) & 0xff00u) |
                    ((v.rgba << 8) & 0xff0000u) | (v.rgba << 24);
                vertices.push_back({v.x, v.y, v.z, rgba, v.u, v.v});
            }
            buffers.push_back({
                upload(vertices.data(), static_cast<UINT>(vertices.size() * sizeof(Vertex)), D3D11_BIND_VERTEX_BUFFER),
                upload(mesh.indices.data(), static_cast<UINT>(mesh.indices.size() * sizeof(unsigned)), D3D11_BIND_INDEX_BUFFER),
                texture,
            });
        }
    }

    void draw(int width, int height, const std::vector<skullgirls::StageMesh>& meshes,
              const std::vector<skullgirls::StageUv>& uv, float center, float view_height,
              float bottom, float distance, float focal, float near_clip, float far_clip,
              std::vector<unsigned>& color,bool download=true) {
        constexpr float clear[] = {174.0f / 255, 185.0f / 255, 199.0f / 255, 1.0f};
        context->ClearRenderTargetView(target_view.Get(), clear);
        context->ClearDepthStencilView(depth_view.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        context->OMSetRenderTargets(1, target_view.GetAddressOf(), depth_view.Get());
        const D3D11_VIEWPORT viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
        context->RSSetViewports(1, &viewport);
        context->RSSetState(raster.Get());
        context->IASetInputLayout(layout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertex_shader.Get(), nullptr, 0);
        context->PSSetShader(pixel_shader.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, constants.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, constants.GetAddressOf());
        Constants transform{center, bottom + view_height * 0.5f, distance, near_clip,
            2 * focal / width, 2 * focal / height, far_clip / (far_clip - near_clip), 0, {}, {}};
        constexpr UINT stride = sizeof(Vertex), offset = 0;
        for (bool opaque : {true, false}) {
            context->OMSetDepthStencilState(depth_modes[opaque].Get(), 0);
            context->OMSetBlendState(blend_modes[opaque].Get(), nullptr, 0xffffffff);
            transform.opaque = opaque ? 1.0f : 0.0f;
            for (std::size_t i = 0; i < meshes.size(); ++i) {
                const auto& mesh = meshes[i];
                const auto& buffer = buffers[i];
                std::copy_n(uv[i].begin(), 3, transform.uv_u);
                std::copy_n(uv[i].begin() + 3, 3, transform.uv_v);
                context->UpdateSubresource(constants.Get(), 0, nullptr, &transform, 0, 0);
                const unsigned filter = unsigned(mesh.animates_uv()) | (unsigned(mesh.wrap_u) << 1) |
                    (unsigned(mesh.wrap_v) << 2);
                context->PSSetSamplers(0, 1, filters[filter].GetAddressOf());
                context->PSSetShaderResources(0, 1, buffer.texture.GetAddressOf());
                context->IASetVertexBuffers(0, 1, buffer.vertices.GetAddressOf(), &stride, &offset);
                context->IASetIndexBuffer(buffer.indices.Get(), DXGI_FORMAT_R32_UINT, 0);
                context->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
            }
        }
        if(download)readback(color);
    }

    void readback(std::vector<unsigned>& color) {
        const int width=target_width,height=target_height;
        context->CopyResource(staging.Get(), target.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        check(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "frame readback");
        for (int y = 0; y < height; ++y) {
            const auto* row = static_cast<const BYTE*>(mapped.pData) + y * mapped.RowPitch;
            std::memcpy(color.data() + y * width, row, width * sizeof(unsigned));
        }
        context->Unmap(staging.Get(), 0);
        ++readbacks;
    }
};

#else
#include "linux_stage_gpu.hpp"
#endif

// Owns the stage mesh and textures and draws them through the platform GPU backend or software.
struct PracticeScene {
    static constexpr int width = 1280, height = 720;
    static constexpr int software_width = 480, software_height = 270;
    struct ProjectedVertex { float x, y, q, u, v; unsigned rgba; };
    std::vector<skullgirls::StageMesh> meshes;
    std::vector<ProjectedVertex> points;
    std::vector<skullgirls::StageUv> drawn_uv, current_uv;
    std::array<float, 4> drawn_camera{};
    bool has_drawn = false;
    std::unique_ptr<StageGpu> gpu;
    std::map<std::string, skullgirls::SpriteBytes> texture_data;
    std::map<std::string, std::unique_ptr<Texture>> textures;
    std::vector<unsigned> color = std::vector<unsigned>(width * height);
    std::vector<float> depth = std::vector<float>(software_width * software_height);
    Gdiplus::Bitmap image{width, height, width * 4, PixelFormat32bppPARGB,
                          reinterpret_cast<BYTE*>(color.data())};
    Gdiplus::Bitmap software_image{software_width, software_height, software_width * 4,
                                   PixelFormat32bppPARGB, reinterpret_cast<BYTE*>(color.data())};
    float fov = 0, near_clip = 0, far_clip = 0;

    PracticeScene(const std::filesystem::path& directory, const std::string& level,
                  float stage_width, float stage_height, float floor,
                  float field_of_view, float near_plane, float far_plane)
        : PracticeScene(directory, skullgirls::read_stage_scene(directory / "stages.gfs", level,
                                                               stage_width, stage_height, floor),
                        field_of_view, near_plane, far_plane) {}

    PracticeScene(const std::filesystem::path& directory, std::vector<skullgirls::StageMesh> stage_meshes,
                  float field_of_view, float near_plane, float far_plane, const std::string& texture_subpath = {},
                  const std::map<std::string, std::string>& replacements = {})
        : meshes(std::move(stage_meshes)),
          fov(field_of_view), near_clip(near_plane), far_clip(far_plane) {
        skullgirls::SpriteArchive stage_textures(directory / "stages-textures.gfs");
        for (const auto& mesh : meshes) {
            if (texture_data.find(mesh.material) != texture_data.end()) continue;
            const auto replacement = replacements.find(mesh.material);
            const auto& material = replacement != replacements.end() ? replacement->second : mesh.material;
            const auto filename = material + ".dds";
            auto override_texture = texture_subpath.empty() ? std::nullopt :
                stage_textures.read_if_present("stages/textures/" + texture_subpath + "/" + filename);
            auto bytes = override_texture ? std::move(*override_texture) : stage_textures.read("stages/textures/" + filename);
            Texture::dds_layout(bytes);
            texture_data.emplace(mesh.material, std::move(bytes));
        }
        // Draw the classroom before its translucent fog and character layers.
        std::stable_partition(meshes.begin(), meshes.end(),
            [](const auto& mesh) { return mesh.material == "class_notes_3d"; });
        for (const auto& mesh : meshes)
            if (mesh.vertices.size() > points.capacity()) points.reserve(mesh.vertices.size());
        drawn_uv.resize(meshes.size());
        current_uv.resize(meshes.size());
    }

    bool cpu_copy_valid=false;
    void initialize_gpu(HWND
#ifdef _WIN32
                        ,skullgirls::SpriteGpu* shared=nullptr
#endif
                        ) {
        try {
            gpu = std::make_unique<StageGpu>(width,height,meshes,texture_data
#ifdef _WIN32
                ,shared?shared->native_device():nullptr,shared?shared->native_context():nullptr
#endif
                );
        } catch (const std::runtime_error& error) {
            OutputDebugStringA(error.what());
            OutputDebugStringA(". Using software stage rendering.\n");
            prepare_software_textures();
        }
    }

    void prepare_software_textures() {
        if (textures.size() == texture_data.size()) return;
        std::map<std::string, std::unique_ptr<Texture>> decoded;
        for (const auto& [name, bytes] : texture_data) {
            auto texture = std::make_unique<Texture>();
            texture->load(bytes, Texture::Encoding::rgba, 1024);
            decoded.emplace(name, std::move(texture));
        }
        textures = std::move(decoded);
    }

    void draw(Gdiplus::Graphics& target, float center, float view_width, float view_height, float bottom,
              std::uint64_t tick
#ifdef _WIN32
              ,skullgirls::SpriteGpu* frame_renderer=nullptr
#endif
              ) {
#ifdef _WIN32
        const bool direct=frame_renderer && frame_renderer->presenting();
        if(direct && gpu && gpu->device.Get()!=frame_renderer->native_device()) {
            initialize_gpu(nullptr,frame_renderer);has_drawn=false;
        }
#endif
        const std::array<float, 4> camera{center, view_width, view_height, bottom};
        const double seconds = tick / 60.0;
        for (std::size_t i = 0; i < meshes.size(); ++i)
            current_uv[i] = meshes[i].uv_at(seconds);
        if (!has_drawn || camera != drawn_camera || current_uv != drawn_uv) {
            if (gpu) {
                const float distance = view_width * 0.5f / std::tan(fov * 0.00872664626f);
                try {
                    gpu->draw(width, height, meshes, current_uv, center, view_height, bottom,
                              distance, width * distance / view_width, near_clip, far_clip, color
#ifdef _WIN32
                              ,!direct
#endif
                              );
#ifdef _WIN32
                    cpu_copy_valid=!direct;
#else
                    cpu_copy_valid=true;
#endif
                } catch (const std::runtime_error& error) {
                    gpu.reset();
                    OutputDebugStringA(error.what());
                    OutputDebugStringA(". Using software stage rendering.\n");
                }
            }
            if (!gpu) {rasterize(center,view_width,view_height,bottom);cpu_copy_valid=true;}
            drawn_camera = camera;
            drawn_uv.swap(current_uv);
            has_drawn = true;
        }
#ifdef _WIN32
        if(direct && gpu) {frame_renderer->composite_stage(gpu->target_texture.Get());return;}
        if(gpu && !cpu_copy_valid) {
            try {gpu->readback(color);cpu_copy_valid=true;}
            catch(const std::exception&) {gpu.reset();rasterize(center,view_width,view_height,bottom);cpu_copy_valid=true;}
        }
#endif
        const auto composition = target.GetCompositingMode();
        target.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        target.DrawImage(gpu ? &image : &software_image, 0, 0, width, height);
        target.SetCompositingMode(composition);
    }

    void rasterize(float center, float view_width, float view_height, float bottom) {
        prepare_software_textures();
        constexpr int width = software_width, height = software_height;
        std::fill_n(color.begin(), width * height, 0xffaeb9c7);
        std::fill_n(depth.begin(), width * height, 0.0f);
        const float distance = view_width * 0.5f / std::tan(fov * 0.00872664626f);
        const float focal = width * distance / view_width;
        const auto edge = [](const ProjectedVertex& a, const ProjectedVertex& b, float x, float y) {
            return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
        };
        for (std::size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
            const auto& mesh = meshes[mesh_index];
            const auto& texture = *textures.at(mesh.material);
            DIBSECTION dib{};
            GetObjectW(texture.bitmap, sizeof(dib), &dib);
            const auto* texels = static_cast<const unsigned*>(dib.dsBm.bmBits);
            points.clear();
            const auto& uv = current_uv[mesh_index];
            const bool animated = mesh.animates_uv();
            const auto coordinate = [](float value, int size, bool wrap) {
                value = wrap ? value - std::floor(value) : std::clamp(value, 0.0f, 1.0f);
                return value * size - 0.5f;
            };
            const auto index = [](int value, int size, bool wrap) {
                return wrap ? (value + size) % size : std::clamp(value, 0, size - 1);
            };
            for (const auto& v : mesh.vertices) {
                const float z = distance - v.z;
                const float q = z >= near_clip && z <= far_clip ? 1.0f / z : 0.0f;
                points.push_back({width * 0.5f + (v.x - center) * focal * q,
                    height * 0.5f - (v.y - bottom - view_height * 0.5f) * focal * q, q,
                    (uv[0] * v.u + uv[1] * v.v + uv[2]) * q,
                    (uv[3] * v.u + uv[4] * v.v + uv[5]) * q, v.rgba});
            }
            for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
                const auto& a = points[mesh.indices[i]];
                const auto& b = points[mesh.indices[i + 1]];
                const auto& c = points[mesh.indices[i + 2]];
                if (!a.q || !b.q || !c.q) continue;
                const float area = edge(a, b, c.x, c.y);
                if (std::abs(area) < 0.001f) continue;
                const float inverse = 1.0f / area;
                const int left = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
                const int right = std::min(width - 1, static_cast<int>(std::ceil(std::max({a.x, b.x, c.x}))));
                const int top = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
                const int bottom = std::min(height - 1, static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));
                for (int y = top; y <= bottom; ++y) {
                    for (int x = left; x <= right; ++x) {
                        const float wa = edge(b, c, x + 0.5f, y + 0.5f) * inverse;
                        const float wb = edge(c, a, x + 0.5f, y + 0.5f) * inverse;
                        const float wc = 1.0f - wa - wb;
                        if (wa < 0 || wb < 0 || wc < 0) continue;
                        const float q = a.q * wa + b.q * wb + c.q * wc;
                        const auto pixel = y * width + x;
                        if (q <= depth[pixel]) continue;
                        const float u = (a.u * wa + b.u * wb + c.u * wc) / q;
                        const float v = (a.v * wa + b.v * wb + c.v * wc) / q;
                        const float tx = coordinate(u, texture.width, mesh.wrap_u);
                        const float ty = coordinate(1 - v, texture.height, mesh.wrap_v);
                        const int texel_x = static_cast<int>(std::floor(tx + (animated ? 0.0f : 0.5f)));
                        const int texel_y = static_cast<int>(std::floor(ty + (animated ? 0.0f : 0.5f)));
                        const int x0 = index(texel_x, texture.width, mesh.wrap_u);
                        const int y0 = index(texel_y, texture.height, mesh.wrap_v);
                        unsigned texel = texels[y0 * texture.width + x0];
                        if (animated) {
                            const int x1 = index(texel_x + 1, texture.width, mesh.wrap_u);
                            const int y1 = index(texel_y + 1, texture.height, mesh.wrap_v);
                            const auto tr = texels[y0 * texture.width + x1];
                            const auto bl = texels[y1 * texture.width + x0];
                            const auto br = texels[y1 * texture.width + x1];
                            const float fx = tx - texel_x, fy = ty - texel_y;
                            unsigned filtered = 0;
                            for (unsigned shift = 0; shift < 32; shift += 8) {
                                const float top = ((texel >> shift) & 255) * (1 - fx) + ((tr >> shift) & 255) * fx;
                                const float bottom = ((bl >> shift) & 255) * (1 - fx) + ((br >> shift) & 255) * fx;
                                filtered |= static_cast<unsigned>(top * (1 - fy) + bottom * fy + 0.5f) << shift;
                            }
                            texel = filtered;
                        }
                        if (a.rgba == 0xffffffff && b.rgba == a.rgba && c.rgba == a.rgba &&
                            (texel >> 24) == 255) {
                            depth[pixel] = q;
                            color[pixel] = texel;
                            continue;
                        }
                        const auto tint = [&](unsigned shift) {
                            return (((a.rgba >> shift) & 255) * a.q * wa +
                                    ((b.rgba >> shift) & 255) * b.q * wb +
                                    ((c.rgba >> shift) & 255) * c.q * wc) / (255.0f * q);
                        };
                        const float opacity = std::clamp(tint(0), 0.0f, 1.0f);
                        const auto alpha = static_cast<unsigned>(std::lround((texel >> 24) * opacity));
                        if (!alpha) continue;
                        unsigned source = alpha << 24;
                        for (unsigned shift = 0; shift < 24; shift += 8) {
                            const auto channel = static_cast<unsigned>(std::lround(
                                ((texel >> shift) & 255) * opacity *
                                std::clamp(tint(24 - shift), 0.0f, 1.0f)));
                            source |= channel << shift;
                        }
                        if (alpha == 255) {
                            depth[pixel] = q;
                            color[pixel] = source;
                        } else if (alpha) {
                            unsigned result = 0xff000000;
                            for (unsigned shift = 0; shift < 24; shift += 8) {
                                const auto channel = ((source >> shift) & 255) +
                                    ((color[pixel] >> shift) & 255) * (255 - alpha) / 255;
                                result |= channel << shift;
                            }
                            color[pixel] = result;
                        }
                    }
                }
            }
        }
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
    std::string keyword;
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
               const Gdiplus::Font& font, Gdiplus::StringAlignment alignment) {
    Gdiplus::StringFormat format;
    format.SetAlignment(alignment);
    Gdiplus::SolidBrush shadow(Gdiplus::Color(235, 0, 0, 0));
    Gdiplus::SolidBrush cream(Gdiplus::Color(255, 245, 224, 180));
    bounds.Offset(2, 2);
    dc.DrawString(text, -1, &font, bounds, &format, &shadow);
    bounds.Offset(-2, -2);
    dc.DrawString(text, -1, &font, bounds, &format, &cream);
}

void draw_text(Gdiplus::Graphics& dc, const wchar_t* text, Gdiplus::RectF bounds,
               float size, Gdiplus::StringAlignment alignment) {
    const Gdiplus::Font font(L"Arial", size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    draw_text(dc, text, bounds, font, alignment);
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

// Filia FOIT moves 117/118 and 121–124. Their script-only records
// set variable 0x3a, causing the engine to advance again within the same tick.
constexpr skullgirls::NormalFrame throw_start[] = {
    {399, 2}, {400, 3}, {401, 3}
};
constexpr skullgirls::NormalFrame throw_whiff[] = {
    {402, 9}, {401, 4}, {403, 3}, {404, 3}, {405, 3}, {406, 3}, {407, 3}
};
constexpr skullgirls::NormalFrame throw_capture_common[] = {
    {408, 15}, {409, 3}, {410, 3}, {411, 3},
    {412, 4}, {418, 2}, {419, 1}, {420, 1},
    {412, 4}, {418, 2}, {419, 1}, {420, 1},
    {412, 4}, {418, 2}, {419, 1}, {420, 1},
    {412, 3}, {418, 1}, {419, 1}, {420, 1}, {413, 4}
};
constexpr skullgirls::NormalFrame throw_capture_back[] = {{414, 4}, {415, 3}, {416, 3}, {417, 3}};
constexpr skullgirls::NormalFrame throw_capture_forward[] = {{414, 8}, {415, 3}, {416, 3}, {417, 3}};
constexpr skullgirls::NormalFrame throw_recovery[] = {
    {421, 6}, {422, 3}, {423, 3}, {407, 3}
};
// Air throw moves 130 and 132–134. Blank records preserve the previous pose.
constexpr skullgirls::NormalFrame air_throw_start[] = {{439, 3}, {440, 4}, {441, 6}};
constexpr skullgirls::NormalFrame air_throw_whiff[] = {{442, 6}, {443, 4}, {444, 4}};
constexpr skullgirls::NormalFrame air_throw_capture[] = {
    {441, 18}, {445, 4}, {446, 4}, {447, 4}, {448, 4}, {449, 4},
    {450, 6}, {451, 3}, {452, 11}, {453, 5}, {454, 3}
};
constexpr skullgirls::NormalFrame air_throw_recovery[] = {
    {454, 1}, {455, 5}, {456, 5}, {442, 4}, {443, 3}, {444, 4}
};
// Throw-tech entry, recoil loops and recovery from moves 125–129 and 135–139.
constexpr skullgirls::NormalFrame ground_throw_tech[] = {
    {424, 5}, {425, 4}, {426, 3}, {427, 2}, {428, 2}, {429, 2}, {430, 2}, {431, 2},
    {432, 3}, {433, 2}, {434, 2}, {432, 3}, {433, 2}, {434, 2}, {432, 3}, {433, 2}, {434, 2},
    {435, 2}, {436, 2}, {437, 3}, {438, 2}
};
constexpr skullgirls::NormalFrame air_throw_tech[] = {
    {457, 8}, {458, 3}, {459, 3}, {460, 3}, {461, 2}, {462, 2},
    {460, 3}, {461, 2}, {462, 2}, {463, 3}, {464, 2}, {465, 3}
};

// Owns both the front-end selection state and the active Training session.
struct Menu {
    std::shared_ptr<skullgirls::AudioOutput> audio;
    Texture training_frame, player_header, select_header, select_film, select_overlay, select_side,
            select_pedestal, select_cursor, select_cursor_p2, team_frame;
    std::array<Texture, roster.size()> portraits;
    std::array<Texture, roster.size()> fighter_portraits, fighter_names;
    std::array<skullgirls::FighterSelectLayout, 2> fighter_layout;
    Texture fighter_frame, portrait_frame, fighter_bracket, coin_base, coin_empty, coin_pending;
    std::array<Texture, 2> player_bar_label;
    std::array<Texture, 3> team_coin;
    std::array<Texture, roster.size()> fighter_coin;
    skullgirls::PedestalLayout pedestal_layout;
    Texture color_frame, color_backer, color_wing, color_arrow, color_of, pedestal_divider, team_film;
    std::array<Texture, 6> color_buttons, color_buttons_lit;
    std::array<Texture, roster.size()> pedestal_names;

    void paint_palette_panel(Gdiplus::Graphics& dc, int side, int fighter, std::size_t palette) const {
        // Local rectangles from CharSelectPickColorP1/P2. The visible panel Y
        // offset comes from the supplied reference capture.
        const float x = side ? 719.0f : 0.0f;
        constexpr float y = 442.0f;
        color_backer.draw(dc, x + 102, y - 42.4f, 358, 185);
        color_frame.draw(dc, x + 102, y - 42.4f, 358, 185);
        player_header.draw(dc, x + 186.5f, y - 52.9f, 182, 30);
        draw_text(dc, side ? L"★ PLAYER 2 ★" : L"★ PLAYER 1 ★",
                  {x + 186.5f, y - 51, 182, 30}, 20, Gdiplus::StringAlignmentCenter);
        color_wing.draw(dc, x + 146.3f, y - 20.8f, 45, 38);
        const auto wing_state = dc.Save();
        dc.TranslateTransform(2 * (x + 371.4f) + 45, 0);
        dc.ScaleTransform(-1, 1);
        color_wing.draw(dc, x + 371.4f, y - 20.8f, 45, 38);
        dc.Restore(wing_state);
        draw_text(dc, L"SELECT COLOR", {x + 187, y - 1.6f, 190, 32},
                  23, Gdiplus::StringAlignmentCenter);

        const auto current = std::to_wstring(palettes[fighter][palette].number);
        const auto total = std::to_wstring(palettes[fighter].size());
        draw_text(dc, current.c_str(), {x + 137.7f, y + 28, 92.2f, 74.9f},
                  48, Gdiplus::StringAlignmentCenter);
        color_of.draw(dc, x + 156.7f, y + 82.1f, 24, 24);
        draw_text(dc, L"of", {x + 158.4f, y + 84, 20.5f, 23.8f},
                  14, Gdiplus::StringAlignmentCenter);
        draw_text(dc, total.c_str(), {x + 178, y + 76.6f, 46, 32},
                  23, Gdiplus::StringAlignmentCenter);

        const auto arrow_state = dc.Save();
        dc.TranslateTransform(2 * (x + 231.0f) + 26, 0);
        dc.ScaleTransform(-1, 1);
        color_arrow.draw(dc, x + 231, y + 56.6f, 26, 38);
        dc.Restore(arrow_state);
        color_arrow.draw(dc, x + 396.6f, y + 56.6f, 26, 38);
        for (std::size_t i = 0; i < color_buttons.size(); ++i) {
            // FUN_0044eb20 maps palette numbers back to the six attack indicators.
            constexpr std::array<unsigned, 9> masks{0, 1, 2, 4, 8, 16, 32, 7, 56};
            const auto number = palettes[fighter][palette].number;
            const bool active = number < masks.size() && (masks[number] & (1u << i));
            (active ? color_buttons_lit[i] : color_buttons[i]).draw(dc, x + 261.8f + (i % 3) * 40, y + 33.5f + (i / 3) * 42.9f, 45, 45);
        }
    }
    std::array<Texture, 3> team_normal, team_glow;
    std::vector<MenuOption> training_options;
    std::size_t training_cursor = 0;
    // The current screen determines where paint and keyboard messages go.
    enum class TrainingView { none, options, team_size, fighters, stage_select, stage_confirmed, practice };
    TrainingView training = TrainingView::none;
    std::array<int, 2> team_size{1, 2};
    std::array<std::vector<int>, 2> fighters;
    int player = 0;
    int fighter_cursor = 9;
    struct Stage {
        std::wstring name;
        std::string id, parent, level;
        std::unique_ptr<Texture> preview;
    };
    std::vector<Stage> stages;
    skullgirls::StageSelectState stage_selection;
    std::vector<std::vector<std::size_t>> stage_groups;
    std::vector<std::size_t> random_stages;
    skullgirls::StageSelectLayout stage_layout;
    const Texture* previous_stage_preview = nullptr;
    std::chrono::steady_clock::time_point stage_entered{}, stage_switched{};
    Texture stage_bar, stage_arrow, stage_variant_arrow, stage_header;

    const Stage& current_stage() const {
        return stages.at(stage_selection.selected(stage_groups));
    }

    std::wstring stage_selection_name() const {
        return stage_selection.random() && training != TrainingView::stage_confirmed ? L"Random" : current_stage().name;
    }

    const Texture* selected_stage_preview() const {
        if (stage_selection.random() && training == TrainingView::stage_confirmed && stage_selection.resolved)
            return stages[*stage_selection.resolved].preview.get();
        const auto index = stage_selection.preview(stage_groups);
        return index ? stages[*index].preview.get() : nullptr;
    }

    void enter_stage_select() {
        training = TrainingView::stage_select;
        stage_selection.reset();
        previous_stage_preview = nullptr;
        stage_entered = stage_switched = std::chrono::steady_clock::now();
    }

    void confirm_stage_selection() {
        static std::mt19937 generator(std::random_device{}());
        stage_selection.confirm(stage_groups, random_stages, generator);
    }

    std::filesystem::path game_data;
    std::unique_ptr<Texture> practice_background;
    std::unique_ptr<PracticeScene> practice_scene;
#ifdef _WIN32
    mutable std::unique_ptr<skullgirls::SpriteGpu> sprite_renderer;
    mutable bool sprite_renderer_active=false,sprite_renderer_failed=false;
    bool direct_present_failed=false;
    mutable std::map<unsigned,skullgirls::SpriteGpu::Texture> combat_gpu_tiles;
    mutable std::array<skullgirls::SpriteGpu::Texture,roster.size()> super_gpu_portraits;
#endif
    std::array<Texture, 2> hud_atlas;
    skullgirls::CombatScene combat_scene;
    skullgirls::CombatPresentation combat_presentation;
    std::map<unsigned,std::wstring> combo_sayings;
    std::array<std::unique_ptr<Texture>, roster.size()> super_portraits;
    mutable std::array<std::unique_ptr<Gdiplus::Bitmap>, roster.size()> super_portrait_tints;
    using HudTileKey = std::pair<std::array<int, 5>, std::array<float, 4>>;
    mutable std::map<HudTileKey, std::unique_ptr<Gdiplus::Bitmap>> hud_tiles;
    mutable std::map<unsigned, std::unique_ptr<Gdiplus::Bitmap>> combat_tiles;
    struct HudOverlaySide {
        std::array<std::array<int, 2>, 3> life_bars{};
        std::array<unsigned, 5> attack_stats{};
        std::size_t active_slot = 0;
        unsigned meter = 0, hitstun = 0;
        bool assist_ready = false;

        bool operator==(const HudOverlaySide& other) const {
            return std::tie(life_bars, attack_stats, active_slot, meter, hitstun, assist_ready) ==
                std::tie(other.life_bars, other.attack_stats, other.active_slot, other.meter, other.hitstun, other.assist_ready);
        }
    };
    using HudOverlayState = std::pair<std::size_t, std::array<HudOverlaySide, 2>>;
    mutable std::optional<HudOverlayState> hud_overlay_state;
    mutable std::unique_ptr<Gdiplus::Bitmap> hud_overlay;
    mutable std::unique_ptr<Gdiplus::Font> hud_stats_font;
    std::array<std::array<std::unique_ptr<Gdiplus::Bitmap>, 3>, 2> hud_portraits;
    enum class CombatEffect {
        light, medium, heavy, block, pushblock, grab, throw_tech, ground_impact, wall_impact, air_recovery, burst, count
    };
    struct CombatSpark {
        CombatEffect effect;
        Gdiplus::PointF position;
        unsigned age;
        float scale;
    };
    struct PracticeLife {
        unsigned life = 15500;
        float displayed_life = 15500, damage_trail = 15500;

        void reset_life(unsigned value) {
            life = value;
            displayed_life = damage_trail = static_cast<float>(value);
        }

        void advance_life() {
            const float target = static_cast<float>(life);
            const auto approach = [](float current, float target, float rate) {
                return std::abs(current - target) < 1.0f ? target : current + (target - current) * rate;
            };
            displayed_life = approach(displayed_life, target, 0.28f);
            damage_trail = std::max(displayed_life, approach(damage_trail, target, 0.09f));
        }
    };
    struct PracticeHudStats : PracticeLife {
        unsigned last_hit = 0, damage = 0, maximum = 0, hits = 0;
        unsigned notice_ticks = 0, blocked_hits = 0;
        const wchar_t* notice = nullptr;
        std::vector<CombatSpark> sparks;
        bool combo = false;
        std::array<std::uint64_t, skullgirls::filia_snapback + 1> combo_attacks{};
    };
    std::array<PracticeHudStats, 2> hud_stats{};
    struct HudInputs {
        std::array<unsigned, 12> entries{};
        std::size_t count = 0;
        unsigned last = 0;
    };
    std::array<HudInputs, 2> hud_inputs{};
    struct RingletSpike {
        enum class Spin { none, standard, otg };
        float x;
        bool facing_right;
        std::uint64_t attack_instance;
        std::size_t palette_slot;
        unsigned frame = skullgirls::ringlet_first_pose_frame, tick = 0;
        bool hit = false;
        Spin spin = Spin::none;

        bool advance() {
            using namespace skullgirls;
            if (++tick < ringlet_spike_frames[frame].ticks) return true;
            tick = 0;
            ++frame;
            if (frame == 2) frame = 5; // Entry script records advance within the same tick.
            if (frame == ringlet_spike_clear_frame)
                frame = spin == Spin::none ? ringlet_dissolve_frame : frame + 1;
            if (frame == ringlet_short_spin_end && spin != Spin::otg) frame = ringlet_dissolve_frame;
            return frame != std::size(ringlet_spike_frames);
        }

        unsigned sprite() const { return skullgirls::ringlet_spike_frames[frame].sprite; }
        bool locks_owner() const { return frame < skullgirls::ringlet_unlock_frame; }
        bool can_hit() const {
            return !hit && frame >= skullgirls::ringlet_spike_arm_frame &&
                frame < skullgirls::ringlet_spike_clear_frame;
        }
    };
    struct Tricobezoar {
        float x, y;
        bool facing_right;
        std::uint64_t attack_instance;
        std::size_t palette_slot;
        unsigned age = 0;

        // Child 3's Start_ForReal and Loop states, excluding immediate script records.
        unsigned sprite() const {
            using namespace skullgirls;
            if (age < tricobezoar_start_ticks) return age < tricobezoar_charge_ticks ? 1191 : 1192;
            return 1193 + (age - tricobezoar_start_ticks) / 2 % 8;
        }
        bool can_hit() const { return age >= skullgirls::tricobezoar_charge_ticks; }
        void advance() {
            using namespace skullgirls;
            ++age;
            if (age < tricobezoar_charge_ticks) return;
            const float forward = facing_right ? 1.0f : -1.0f;
            if (age == tricobezoar_charge_ticks) {
                x += 188.0f * forward;
                y += 87.5f;
            }
            const bool initial_launch = age < tricobezoar_start_ticks;
            x += (initial_launch ? -60.0f : 125.0f / 3.0f) * forward;
            y += initial_launch ? 45.0f : 25.0f;
        }
    };
    struct PracticeFighter {
        std::optional<skullgirls::CerebellaPlayback> cerebella;
        std::optional<skullgirls::PeacockPlayback> peacock;
        std::optional<skullgirls::ParasoulPlayback> parasoul;
        std::optional<skullgirls::SquiglyPlayback> squigly;
        std::optional<skullgirls::FukuaPlayback> fukua;
        std::optional<skullgirls::ValentinePlayback> valentine;
        std::optional<skullgirls::MsFortunePlayback> msfortune;
        std::optional<skullgirls::PainwheelPlayback> painwheel;
        std::optional<skullgirls::RoboFortunePlayback> robofortune;
        skullgirls::CerebellaPlayback* scripted() { return squigly ? &*squigly : fukua ? &*fukua : valentine ? &*valentine : robofortune ? &*robofortune : painwheel ? &*painwheel : msfortune ? &*msfortune : parasoul ? &*parasoul : peacock ? &*peacock : cerebella ? &*cerebella : nullptr; }
        const skullgirls::CerebellaPlayback* scripted() const { return squigly ? &*squigly : fukua ? &*fukua : valentine ? &*valentine : robofortune ? &*robofortune : painwheel ? &*painwheel : msfortune ? &*msfortune : parasoul ? &*parasoul : peacock ? &*peacock : cerebella ? &*cerebella : nullptr; }
        bool scripted_attacking() const { return squigly ? squigly->attacking() : fukua ? fukua->attacking() : valentine ? valentine->attacking() : robofortune ? robofortune->attacking() : painwheel ? painwheel->attacking() : msfortune ? msfortune->attacking() : parasoul ? parasoul->attacking() : peacock ? peacock->attacking() : cerebella && cerebella->attacking(); }
        unsigned scripted_grab_successor() const { return squigly ? squigly->grab_successor() : fukua ? fukua->grab_successor() : valentine ? valentine->grab_successor() : robofortune ? robofortune->grab_successor() : painwheel ? painwheel->grab_successor() : msfortune ? msfortune->grab_successor() : parasoul ? parasoul->grab_successor() : peacock ? peacock->grab_successor() : cerebella ? cerebella->grab_successor() : 0; }

        skullgirls::FoitVisualState visual;
        unsigned visual_strength = 0;
        float practice_x = 0;
        std::size_t palette_slot = 0;
        const void* last_head_hit = nullptr;
        std::uint64_t last_head_hit_instance = 0;
        bool practice_left = false, practice_right = false;
        bool dash_facing_right = true;
        // Filia FOIT standing-to-dash conditions at 0x8e30b and 0x8e39b.
        skullgirls::FiliaDashHistory dash_input_history = [] {
            skullgirls::FiliaDashHistory history{};
            history.fill(skullgirls::filia_neutral);
            return history;
        }();
        bool practice_up = false;
        int dash_direction = 0;
        enum class ForwardDash { start, extend, loop, stop };
        ForwardDash forward_dash = ForwardDash::start;
        unsigned dash_frame = 0, dash_tick = 0, movement_sprite = 0, walk_tick = 0;
        unsigned idle_tick = 0;
        int walk_direction = 0;
        enum class GroundTransition { none, stand_turn, crouch_turn, backwalk_stop };
        GroundTransition ground_transition = GroundTransition::none;
        unsigned ground_transition_tick = 0;
        int ground_facing = 0;
        bool practice_down = false;
        unsigned stance_tick = 0;
        bool crouching = false, rising = false;
        unsigned jump_start = 0, air_ticks = 0, landing = 0;
        float jump_height = 0, jump_velocity = 0, jump_horizontal = 0;
        int jump_direction = 0;
        bool jump_facing_right = true;
        bool air_dash_used = false, super_jump = false;
        unsigned air_dash_tick = 0, air_dash_lockout = 0;
        unsigned fall_tick = 0;
        bool air_dash_fall = false, air_hurt_fall = false;
        int air_dash_direction = 0, pending_air_dash = 0;
        int normal_input = -1, active_normal = -1;
        unsigned pressed_punches = 0, pressed_kicks = 0;
        unsigned held_attack_buttons = 0;
        struct NormalInput { unsigned buttons; bool down; };
        // Normal transitions in moves 97–116 search age zero through age five.
        std::array<NormalInput, 6> normal_inputs{};
        skullgirls::FoitInputHistory script_inputs{};
        skullgirls::FoitScriptState script_state{};
        unsigned script_held_buttons = 0;
        unsigned next_script_record = 0, script_move_age = 0;
        struct SpecialCommand {
            int move = -1;
            unsigned strength = 0;
            bool half_circle = false;
        };
        struct BufferedSpecial {
            SpecialCommand command;
            unsigned ticks;
            bool facing_right;
        };
        std::optional<BufferedSpecial> buffered_special;
        static constexpr unsigned special_recovery_buffer = 4;
        float hairball_speed = 0, airball_lift = 0;
        unsigned hairball_roll_ticks = 0, updo_flight_ticks = 0;
        bool airball_dash_cancelled = false, airball_used = false;
        unsigned airball_hits = 0, airball_blocks = 0;
        unsigned taunt_step = 0, taunt_ticks = 0;
        unsigned blockbuster_ticks = 0;
        skullgirls::FoitPlaybackState<skullgirls::FoitEffectPlayback> blockbuster_playback;
        bool fenrir_connected = false;
        bool fenrir_captured = false;
        unsigned ringlet_strength = 0, ringlet_entry_ticks = 0;
        std::vector<RingletSpike> ringlet_spikes;
        bool ringlet_locked() const {
            return std::any_of(ringlet_spikes.begin(), ringlet_spikes.end(),
                [](const RingletSpike& spike) { return spike.locks_owner(); });
        }
        std::optional<Tricobezoar> tricobezoar;
        unsigned tension = 5;
        int queued_followup = -1;
        unsigned normal_frame = 0, normal_tick = 0, normal_pose = 0;
        bool normal_facing_right = true, normal_contact = false, rapid_jab_whiff = false;
        bool normal_hit_landed = false;
        const skullgirls::NormalHit* normal_hit = nullptr;
        const skullgirls::NormalHit* last_contact = nullptr;
        unsigned hit_stop = 0, hurt_frame = 0, hurt_tick = 0, hurt_hold = 0;
        unsigned blockstun = 0;
        enum class GuardStance { standing, crouching, airborne };
        enum class GuardPhase { none, entering, idle, leaving, stun, pushblock };
        GuardStance guard_stance = GuardStance::standing;
        GuardPhase guard_phase = GuardPhase::none;
        unsigned guard_tick = 0, guard_pose = 0;
        bool guard_special_height = false;
        int hurt_group = -1;
        skullgirls::HurtPhase hurt_phase = skullgirls::hurt_entry;
        skullgirls::HurtHeight hurt_height = skullgirls::HurtHeight::mid;
        bool hurt_facing_right = true;
        skullgirls::HeavyReaction::Type heavy_reaction = skullgirls::HeavyReaction::none;
        float recoil_x = 0;
        unsigned recoil_ticks = 0;
        unsigned reaction_phase = 0;
        unsigned wall_bounce_tick = 0;
        bool recoil_landed = false, waking_up = false;
        int ground_tech_direction = 0;
        int striking_normal = -1;
        std::uint64_t attack_instance = 0;
        bool burst_available = false;
        unsigned burst_ticks = 0;
        unsigned burst_fall_tick = 0;
        bool burst_connected = false;
        bool throw_input = false;
        bool throw_facing_right = true;
        bool aerial_throw = false;
        bool forward_ground_throw = false;
        unsigned throw_tick = 0;
        enum class ThrowPhase { none, start, whiff, capture, recovery, grabbed, tech };
        ThrowPhase throw_phase = ThrowPhase::none;
        unsigned tag_entry_ticks = 0;
        unsigned snapback_reserve = 0;

        void take_hit(const skullgirls::NormalHit& hit, bool facing_right, int move,
                      skullgirls::HeavyReaction forced = {}) {
            // Hatred Guard and Buer Overdrive author their remaining armor hits in 0x7b.
            // A strike still deals damage and hit stop, but preserves the current move.
            if (painwheel && painwheel->attacking() && painwheel->value(0x7b) > 0 && hit.height_mask) {
                painwheel->state.variables[0x7b] = painwheel->value(0x7b) - 1;
                hit_stop = std::max(hit_stop, hit.stop);
                return;
            }
            visual.freeze = visual.super_mode = 0;
            blockbuster_playback.reset();
            if (auto* actor = scripted()) { actor->visual.freeze = 0; actor->reacting = false; }
            const auto reaction = forced.type != skullgirls::HeavyReaction::none ? forced :
                skullgirls::filia_heavy_reaction(move, jump_height > 0);
            throw_phase = ThrowPhase::none;
            tag_entry_ticks = 0;
            fenrir_captured = false;
            blockstun = 0;
            guard_phase = GuardPhase::none;
            burst_ticks = burst_fall_tick = 0;
            heavy_reaction = reaction.type;
            recoil_x = facing_right ? -reaction.horizontal : reaction.horizontal;
            recoil_ticks = 0;
            reaction_phase = 0;
            wall_bounce_tick = 0;
            recoil_landed = false;
            waking_up = false;
            ground_tech_direction = 0;
            if (heavy_reaction != skullgirls::HeavyReaction::none)
                jump_velocity = reaction.upward;
            hurt_height = hit.reaction_height();
            hurt_group = jump_height > 0 || jump_velocity > 0 ? skullgirls::air_mid :
                skullgirls::filia_ground_hurt(hurt_height, crouching);
            hurt_facing_right = facing_right;
            hurt_phase = skullgirls::hurt_entry;
            hurt_frame = hurt_tick = 0;
            hurt_hold = hit.stun;
            hit_stop = std::max(hit_stop, hit.stop);
            const unsigned impact_pose = heavy_reaction == skullgirls::HeavyReaction::ground_bounce ? 785 :
                heavy_reaction == skullgirls::HeavyReaction::knockback ? skullgirls::filia_knockback_start[0].sprite :
                heavy_reaction == skullgirls::HeavyReaction::sweep ? skullgirls::filia_front_fall_start[0].sprite :
                heavy_reaction == skullgirls::HeavyReaction::sliding ? 769 :
                heavy_reaction == skullgirls::HeavyReaction::ceiling_bounce ? 788 :
                heavy_reaction == skullgirls::HeavyReaction::crumple ? 794 :
                heavy_reaction == skullgirls::HeavyReaction::stagger ? 816 :
                heavy_reaction == skullgirls::HeavyReaction::freeze ? 826 :
                skullgirls::filia_hurt[hurt_group][hurt_phase][0].sprite;
            if (impact_pose) movement_sprite = impact_pose;
            active_normal = normal_input = -1;
            queued_followup = -1;
            normal_inputs = {};
            buffered_special.reset();
            normal_hit = nullptr;
            dash_direction = air_dash_direction = 0;
            jump_start = landing = walk_tick = 0;
            pending_air_dash = 0;
            air_hurt_fall = false;
            ground_transition = GroundTransition::none;
            walk_direction = 0;
        }

        bool advance_extra_reaction(float left, float right, float ceiling) {
            using namespace skullgirls;
            using Reaction = HeavyReaction;
            const auto next_phase = [&] {
                ++reaction_phase;
                recoil_ticks = 0;
            };
            const auto play = [&](DefenseClip clip) {
                movement_sprite = clip.pose_at(recoil_ticks++, movement_sprite);
                if (recoil_ticks < clip.duration()) return false;
                next_phase();
                return true;
            };
            const auto move = [&](float gravity) {
                practice_x = std::clamp(practice_x + recoil_x, left, right);
                jump_height = std::max(0.0f, jump_height + jump_velocity);
                jump_velocity = jump_height > 0 ? jump_velocity - gravity : 0.0f;
            };
            const auto finish = [&] {
                hurt_group = -1;
                heavy_reaction = Reaction::none;
                crouching = rising = false;
                stance_tick = 0;
                if (jump_height > 0) {
                    jump_direction = 0;
                    jump_horizontal = 0;
                    air_hurt_fall = true;
                    jump_facing_right = hurt_facing_right;
                    fall_tick = 0;
                }
            };
            switch (heavy_reaction) {
            case Reaction::sliding:
                if (reaction_phase < 2) {
                    play(reaction_phase ? DefenseClip(slide_flight) : DefenseClip(slide_start));
                    if (reaction_phase == 2) reaction_phase = 1;
                    move(1.0f / 3);
                    if (!jump_height) {
                        reaction_phase = 2;
                        recoil_ticks = 0;
                    }
                } else if (reaction_phase == 2) {
                    const auto clip = DefenseClip(slide_loop);
                    const unsigned tick = recoil_ticks++;
                    movement_sprite = clip.pose_at(tick % clip.duration(), movement_sprite);
                    move(0);
                    recoil_x *= 0.8f;
                    if (!hurt_hold && recoil_ticks >= clip.duration()) next_phase();
                } else if (reaction_phase == 3) {
                    if (play(slide_end)) {
                        heavy_reaction = Reaction::back_knockdown;
                        recoil_landed = waking_up = true;
                    }
                }
                return true;
            case Reaction::ceiling_bounce:
                if (reaction_phase < 2) {
                    play(reaction_phase ? DefenseClip(ceiling_rise) : DefenseClip(ceiling_start));
                    if (reaction_phase == 2) reaction_phase = 1;
                    move(1.0f / 3);
                    if (jump_height >= ceiling || jump_velocity <= 0) {
                        jump_height = std::min(jump_height, ceiling);
                        jump_velocity = -std::abs(jump_velocity);
                        recoil_x *= 0.5f;
                        reaction_phase = 2;
                        recoil_ticks = 0;
                    }
                } else if (reaction_phase == 2) {
                    play(ceiling_impact);
                    move(1.0f);
                } else if (reaction_phase == 3) {
                    constexpr DefenseClip fall{ceiling_fall};
                    movement_sprite = fall.pose_at(recoil_ticks++ % fall.duration(), movement_sprite);
                    move(1.0f);
                    if (!jump_height) {
                        next_phase();
                        recoil_x = hurt_facing_right ? -4.0f : 4.0f;
                        jump_velocity = 20.0f;
                    }
                } else {
                    move(1.0f);
                    if (play(ceiling_land)) heavy_reaction = Reaction::launch;
                }
                return true;
            case Reaction::crumple:
                move(1.0f);
                if (reaction_phase == 0) play(crumple_start);
                else if (reaction_phase == 1) play(crumple_crouch);
                else if (play(crumple_fall)) {
                    heavy_reaction = Reaction::sweep;
                    recoil_landed = waking_up = true;
                    recoil_ticks = 8; // Enter the face-down floor hold in move 267.
                }
                return true;
            case Reaction::stagger:
                if (reaction_phase == 0) {
                    constexpr DefenseClip loop{stagger_loop};
                    movement_sprite = loop.pose_at(recoil_ticks++ % loop.duration(), movement_sprite);
                    if (!hurt_hold && recoil_ticks >= loop.duration()) next_phase();
                } else if (play(stagger_end)) finish();
                move(1.0f);
                return true;
            case Reaction::freeze:
                if (reaction_phase == 0) {
                    movement_sprite = DefenseClip(freeze_loop).pose_at(recoil_ticks++, movement_sprite);
                    if (!hurt_hold) next_phase();
                } else if (play(freeze_end)) finish();
                return true;
            default:
                return false;
            }
        }

        void tick_hurt(float left, float right, bool tech_pressed, float ceiling) {
            using Reaction = skullgirls::HeavyReaction;
            if (heavy_reaction != Reaction::none && heavy_reaction != Reaction::air_launch) {
                if (hurt_hold) --hurt_hold;
                if (advance_extra_reaction(left, right, ceiling)) return;
                const auto pose = [](const auto& frames, unsigned tick) {
                    for (const auto& frame : frames) {
                        if (tick < frame.ticks) return frame.sprite;
                        tick -= frame.ticks;
                    }
                    return 0u;
                };
                const auto move_recoil = [&] {
                    practice_x = std::clamp(practice_x + recoil_x, left, right);
                    jump_height = std::max(0.0f, jump_height + jump_velocity);
                };
                if (heavy_reaction == Reaction::wall_bounce) {
                    using namespace skullgirls;
                    if (!wall_bounce_tick) {
                        const unsigned tick = recoil_ticks++;
                        movement_sprite = pose(filia_wall_fly, tick < 9 ? tick : 9 + (tick - 9) % 12);
                        const float destination = practice_x + recoil_x;
                        move_recoil();
                        if ((recoil_x < 0 && destination <= left) || (recoil_x > 0 && destination >= right)) {
                            wall_bounce_tick = 1;
                            jump_height = std::max(jump_height, 1.0f);
                            return;
                        }
                        if (jump_height > 0) {
                            jump_velocity -= 1.0f;
                            return;
                        }
                        recoil_landed = true;
                        recoil_ticks = 0;
                        jump_velocity = 0;
                    } else {
                        const unsigned tick = wall_bounce_tick++;
                        if (tick <= filia_wall_impact_ticks) {
                            if (tick > 4) movement_sprite = 773;
                            return;
                        }
                        if (tick == filia_wall_impact_ticks + 1) {
                            recoil_x *= -0.8f;
                            jump_velocity += 8.0f;
                        }
                        movement_sprite = pose(filia_wall_exit, tick - filia_wall_impact_ticks - 1);
                        move_recoil();
                        if (jump_height > 0 && movement_sprite) {
                            jump_velocity -= 1.0f;
                            return;
                        }
                        recoil_landed = jump_height == 0;
                        recoil_ticks = recoil_landed ? 0 : 9;
                        if (recoil_landed) jump_velocity = 0;
                    }
                    movement_sprite = recoil_landed ? 752 : 759;
                    heavy_reaction = Reaction::sweep;
                    return;
                }
                if (heavy_reaction == Reaction::ground_bounce) {
                    if (!recoil_landed) {
                        if (jump_height > 0) move_recoil();
                        if (jump_height > 0) {
                            jump_velocity -= 1.0f;
                            constexpr skullgirls::DefenseClip flight{skullgirls::floorbounce_flight};
                            movement_sprite = flight.pose_at(recoil_ticks++ % flight.duration(), movement_sprite);
                            return;
                        }
                        recoil_landed = true;
                        recoil_ticks = 0;
                        jump_velocity *= -0.5f;
                    }
                    movement_sprite = pose(skullgirls::filia_ground_bounce, recoil_ticks++);
                    move_recoil();
                    jump_velocity -= 1.0f;
                    if (recoil_ticks == skullgirls::filia_ground_bounce_ticks) {
                        heavy_reaction = Reaction::launch;
                        recoil_landed = false;
                        recoil_ticks = 0;
                    }
                    return;
                }
                if (!recoil_landed) {
                    if (heavy_reaction == Reaction::launch) {
                        const float relative_recoil = hurt_facing_right ? recoil_x : -recoil_x;
                        const float angle = std::atan2(jump_velocity, relative_recoil) * 57.29578f;
                        movement_sprite = skullgirls::filia_launch_sprite(angle);
                    }
                    move_recoil();
                    jump_velocity -= 1.0f;
                    if (!jump_height && jump_velocity < 0) {
                        recoil_landed = true;
                        jump_velocity = 0;
                        recoil_ticks = 0;
                        if (heavy_reaction == Reaction::launch)
                            heavy_reaction = Reaction::back_knockdown;
                    } else if (heavy_reaction == Reaction::launch) {
                        return;
                    }
                }
                const bool front = heavy_reaction == Reaction::sweep;
                const bool back = heavy_reaction == Reaction::back_knockdown;
                const unsigned rebound_tick = back ? skullgirls::filia_back_rebound_tick :
                    skullgirls::filia_floor_rebound_tick;
                if (!recoil_landed) {
                    const unsigned tick = recoil_ticks++;
                    movement_sprite = front ? pose(skullgirls::filia_front_fall_start, tick) :
                        pose(skullgirls::filia_knockback_start, tick);
                    if (!movement_sprite) {
                        movement_sprite = front ? pose(skullgirls::filia_front_fall_loop, (tick - 9) % 10) :
                            pose(skullgirls::filia_knockback_loop, (tick - 9) % 12);
                    }
                    return;
                }
                if (tech_pressed && !waking_up && recoil_ticks <= rebound_tick && !jump_height && !jump_velocity) {
                    ground_tech_direction = int(practice_right) - int(practice_left);
                    if (ground_tech_direction) {
                        buffered_special.reset();
                        waking_up = true;
                        recoil_ticks = 0;
                        recoil_x = 0;
                    }
                }
                if (ground_tech_direction) {
                    using namespace skullgirls;
                    const bool forward = (ground_tech_direction > 0) == hurt_facing_right;
                    const auto& motion = ground_tech_motion[back][forward];
                    constexpr DefenseClip recovery{ground_tech_recovery};
                    const unsigned tick = recoil_ticks++;
                    const unsigned roll_ticks = motion.roll.duration();
                    if (tick < roll_ticks) {
                        if (tick == motion.roll.record_tick(motion.launch_record)) {
                            recoil_x = ground_tech_direction * motion.horizontal;
                            jump_velocity = motion.upward;
                        }
                        if (tick == motion.roll.record_tick(motion.brake_record)) recoil_x *= 0.5f;
                        move_recoil();
                        jump_velocity = jump_height > 0 ? jump_velocity - motion.gravity : 0.0f;
                        movement_sprite = motion.roll.pose_at(tick, movement_sprite);
                    } else {
                        movement_sprite = recovery.pose_at(tick - roll_ticks, movement_sprite);
                    }
                    if (recoil_ticks == roll_ticks + recovery.duration()) {
                        ground_tech_direction = 0;
                        heavy_reaction = Reaction::none;
                        hurt_group = -1;
                        crouching = rising = false;
                        stance_tick = 0;
                    }
                    return;
                }
                if (!waking_up) {
                    const unsigned tick = recoil_ticks++;
                    if (tick < rebound_tick) {
                        const unsigned sprite = back ? pose(skullgirls::filia_back_impact, tick) :
                            pose(skullgirls::filia_floor_impact, tick);
                        if (sprite) movement_sprite = sprite;
                        return;
                    }
                    if (tick == rebound_tick) {
                        const float limit = skullgirls::filia_floor_rebound_horizontal_limit;
                        recoil_x = std::clamp(back ? -recoil_x : recoil_x, -limit, limit);
                        jump_velocity = skullgirls::filia_floor_rebound_upward;
                    }
                    if (jump_height > 0 || jump_velocity > 0) {
                        move_recoil();
                        jump_velocity = jump_height > 0 ?
                            jump_velocity - skullgirls::filia_floor_rebound_gravity : 0.0f;
                    }
                    movement_sprite = back ? pose(skullgirls::filia_back_rebound, tick - rebound_tick) :
                        pose(skullgirls::filia_floor_rebound, tick - rebound_tick);
                    if (movement_sprite) return;
                    movement_sprite = back ? 738 : skullgirls::filia_floor_down_sprite;
                    if (jump_height == 0 && jump_velocity == 0) {
                        waking_up = true;
                        recoil_ticks = 0;
                    }
                    return;
                }
                const unsigned tick = recoil_ticks++;
                movement_sprite = back ? pose(skullgirls::filia_back_wakeup, tick) :
                    pose(skullgirls::filia_front_wakeup, tick);
                if (!movement_sprite) {
                    heavy_reaction = Reaction::none;
                    hurt_group = -1;
                    crouching = rising = false;
                    stance_tick = 0;
                }
                return;
            }

            const auto group = static_cast<skullgirls::HurtGroup>(hurt_group);
            if (hurt_hold) --hurt_hold;
            if ((hurt_phase == skullgirls::hurt_loop && hurt_hold <= skullgirls::filia_recovery_start(group)) ||
                (hurt_phase == skullgirls::hurt_recovery && hurt_hold <= skullgirls::filia_recovery_end(group))) {
                hurt_phase = hurt_phase == skullgirls::hurt_loop ? skullgirls::hurt_recovery : skullgirls::hurt_end;
                hurt_frame = hurt_tick = 0;
            }
            const auto& sequence = skullgirls::filia_hurt[group][hurt_phase];
            const auto& frame = sequence[hurt_frame];
            if (frame.sprite) movement_sprite = frame.sprite;
            if (++hurt_tick >= frame.ticks) {
                hurt_tick = 0;
                if (++hurt_frame == std::size(sequence) || !sequence[hurt_frame].ticks) {
                    hurt_frame = 0;
                    if (hurt_phase == skullgirls::hurt_entry) hurt_phase = skullgirls::hurt_loop;
                }
            }
            if (jump_height > 0 || jump_velocity > 0) {
                if (heavy_reaction == Reaction::air_launch)
                    practice_x = std::clamp(practice_x + recoil_x, left, right);
                jump_height = std::max(0.0f, jump_height + jump_velocity);
                jump_velocity = jump_height > 0 ? jump_velocity - 1.0f : 0.0f;
                if (jump_height == 0 && group == skullgirls::air_mid) {
                    // Move 235 returns airborne hitstun to the standing reaction dispatcher.
                    if (hurt_hold) {
                        hurt_group = skullgirls::filia_ground_hurt(hurt_height, false);
                        hurt_phase = skullgirls::hurt_entry;
                        hurt_frame = hurt_tick = 0;
                        crouching = rising = false;
                    } else {
                        landing = 1;
                    }
                }
            }
            if (!hurt_hold) {
                hurt_group = -1;
                heavy_reaction = Reaction::none;
                if (group == skullgirls::air_mid && jump_height > 0) {
                    // Air recovery 201/202 reaches move 83 via 94.
                    air_hurt_fall = true;
                    air_dash_fall = false;
                    movement_sprite = 181;
                    fall_tick = 1;
                    jump_direction = 0;
                    jump_facing_right = hurt_facing_right;
                    jump_horizontal = 0;
                }
            }
        }


        void advance_throw() {
            const auto play = [&](skullgirls::DefenseClip clip) {
                if (throw_tick >= clip.duration()) {
                    throw_tick = 0;
                    return false;
                }
                movement_sprite = clip.pose_at(throw_tick++, movement_sprite);
                return true;
            };
            const skullgirls::DefenseClip capture_common{throw_capture_common};
            switch (throw_phase) {
            case ThrowPhase::start:
                if (!(aerial_throw ? play(air_throw_start) : play(throw_start))) {
                    throw_phase = ThrowPhase::whiff;
                    if (aerial_throw) play(air_throw_whiff);
                    else play(throw_whiff);
                }
                break;
            case ThrowPhase::whiff:
                if (!(aerial_throw ? play(air_throw_whiff) : play(throw_whiff))) throw_phase = ThrowPhase::none;
                break;
            case ThrowPhase::capture:
                if (!aerial_throw && throw_tick < capture_common.duration()) {
                    play(capture_common);
                    break;
                }
                if (!aerial_throw) {
                    const skullgirls::DefenseClip tail = forward_ground_throw ? throw_capture_forward : throw_capture_back;
                    const auto tick = throw_tick - capture_common.duration();
                    if (tick < tail.duration()) {
                        if (forward_ground_throw && tick == throw_capture_forward[0].ticks)
                            throw_facing_right = !throw_facing_right;
                        movement_sprite = tail.pose_at(tick, movement_sprite);
                        ++throw_tick;
                        break;
                    }
                    throw_tick = 0;
                }
                if (!aerial_throw || !play(air_throw_capture)) {
                    throw_phase = ThrowPhase::recovery;
                    if (aerial_throw) play(air_throw_recovery);
                    else play(throw_recovery);
                }
                break;
            case ThrowPhase::recovery:
                if (!(aerial_throw ? play(air_throw_recovery) : play(throw_recovery))) throw_phase = ThrowPhase::none;
                break;
            case ThrowPhase::tech:
                if (!(aerial_throw ? play(air_throw_tech) : play(ground_throw_tech))) throw_phase = ThrowPhase::none;
                break;
            default: break;
            }
            if (throw_phase == ThrowPhase::none && (!aerial_throw || !jump_height)) movement_sprite = 0;
        }

        void move_during_throw(float left, float right) {
            if (throw_phase == ThrowPhase::tech) {
                const unsigned recoil_start = aerial_throw ? 11 : 22;
                if (throw_tick > recoil_start) {
                    const float recoil = std::max(0.0f, (aerial_throw ? 18.0f : 15.0f) -
                        float(throw_tick - recoil_start - 1));
                    practice_x = std::clamp(practice_x + (throw_facing_right ? -recoil : recoil), left, right);
                }
            }
            if (!aerial_throw || throw_phase == ThrowPhase::grabbed || throw_phase == ThrowPhase::capture) return;
            if (throw_phase == ThrowPhase::tech && throw_tick == 12) jump_velocity = -10;
            jump_height = std::max(0.0f, jump_height + jump_velocity);
            jump_velocity = jump_height > 0 ? jump_velocity - 1.0f : 0.0f;
            if (throw_phase == ThrowPhase::none) {
                jump_facing_right = throw_facing_right;
                if (!jump_height) {
                    movement_sprite = 113;
                    landing = 1;
                }
            }
        }

        bool update_dash_input(int horizontal, bool airborne) {
            std::move_backward(dash_input_history.begin(), dash_input_history.end() - 1,
                               dash_input_history.end());
            const int vertical = int(practice_up) - int(practice_down);
            dash_input_history.front() = skullgirls::filia_input_direction(horizontal, vertical);
            return skullgirls::filia_double_tap(dash_input_history, horizontal, airborne);
        }

        int prepare_special(const SpecialCommand& command, bool airborne) {
            visual_strength = command.strength;
            if (command.move == skullgirls::ground_hairball || command.move == skullgirls::air_hairball) {
                constexpr float ground_speed[] = {18, 22, 23};
                constexpr float air_lift[] = {6, 0, -8};
                constexpr unsigned ground_roll_ticks[] = {11, 17, 29};
                hairball_speed = airborne ? 22.0f : ground_speed[command.strength];
                hairball_roll_ticks = airborne ? 17 : ground_roll_ticks[command.strength];
                airball_lift = air_lift[command.strength];
            } else if (command.move == skullgirls::ringlet_spike) {
                ringlet_strength = command.strength;
                ringlet_entry_ticks = command.half_circle ? skullgirls::ringlet_half_entry_ticks :
                    skullgirls::ringlet_quarter_entry_ticks;
            }
            return command.move;
        }

        void update_script_inputs(unsigned direction, unsigned edges) {
            const unsigned held = held_attack_buttons & 63u;
            std::move_backward(script_inputs.begin(), script_inputs.end() - 1, script_inputs.end());
            script_inputs.front() = skullgirls::foit_input_sample(direction, edges, held,
                script_held_buttons & ~held);
            script_held_buttons = held | edges;
            script_move_age = std::min(script_move_age + 1, unsigned(script_inputs.size()));
        }

        template <class Accept>
        int script_transition(const skullgirls::FoitMoveTable& moves, unsigned source, bool facing,
                              Accept accept, unsigned oldest_trigger = skullgirls::FoitInputHistory{}.size() - 1) const {
            const auto& transitions = moves.transitions.at(source);
            for (const auto& transition : transitions) {
                if (accept(transition.target) && skullgirls::match_foit_condition(
                        transition.condition, script_inputs, facing, script_state, oldest_trigger))
                    return transition.target;
            }
            return -1;
        }

        int airball_dash_transition(const skullgirls::FoitMoveTable& moves) {
            script_state.variables[0x01] = static_cast<float>(airball_hits);
            script_state.variables[0x02] = static_cast<float>(airball_blocks);
            script_state.variables[0x3a] = 0.0f;
            script_state.variables[0x7d] = air_dash_used ? 0.0f : 1.0f;
            script_state.variables[0x80] = static_cast<float>(air_dash_lockout);
            if (!script_state.bits[64].has_value()) script_state.bits[64] = false;
            return script_transition(moves, 157, normal_facing_right,
                [](unsigned move) { return move >= 158 && move <= 160; });
        }

        void update_visuals(const skullgirls::FoitMoveTable& table, bool finish = false) {
            if (finish) {
                if (visual.source >= 0) visual.enter_records(table, unsigned(visual.source), unsigned(-1),
                    script_state, script_inputs, normal_facing_right);
                return;
            }
            unsigned source = 0, record = normal_frame;
            if (active_normal <= skullgirls::rapid_crouch_lp) source = skullgirls::filia_foit_normals[active_normal];
            else if (active_normal >= skullgirls::ringlet_psych && active_normal != skullgirls::fenrir_fall) {
                constexpr unsigned sources[] = {147, 146, 170, 191, 171, 172, 173, 174, 175,
                    176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 196};
                source = sources[active_normal - skullgirls::ringlet_psych];
            } else {
                std::vector<unsigned> parts;
                if (active_normal <= skullgirls::updo_light + 2)
                    parts = {161u + unsigned(active_normal - skullgirls::updo_light), 164, 165, 166, 167, 168};
                else if (active_normal == skullgirls::ground_hairball) parts = {148 + visual_strength, 154, 156};
                else if (active_normal == skullgirls::air_hairball) parts = {151 + visual_strength, 155, 157};
                else {
                    parts = {185, 167, 168};
                    record -= skullgirls::fenrir_fall_start;
                }
                for (const auto part : parts) {
                    source = part;
                    if (record < table.animations[part].size()) break;
                    record -= unsigned(table.animations[part].size());
                }
            }
            visual.enter_records(table, source, record, script_state, script_inputs, normal_facing_right);
        }

        unsigned apply_normal_script(const skullgirls::FoitMoveTable& moves) {
            const auto& records = moves.animations[skullgirls::filia_foit_normals[active_normal]];
            const auto apply = [&](const auto& expressions) {
                for (const auto& expression : expressions) {
                    const auto dst = expression.destination.value;
                    if ((expression.destination.flags == 1 && (dst == 0x7f || dst == 0xae || dst == 0xb0)) ||
                        (expression.destination.flags == 0x20 && dst == 0))
                        skullgirls::apply_foit_assignment(expression, script_state);
                }
            };
            unsigned ticks = skullgirls::filia_normals[active_normal][normal_frame].ticks;
            for (auto record = std::min(next_script_record, normal_frame);
                 record <= normal_frame && record < records.size(); ++record) {
                apply(records[record].script);
                for (const auto& variant : records[record].variants) {
                    if (!skullgirls::match_foit_condition(variant.condition, script_inputs,
                                                        normal_facing_right, script_state)) continue;
                    apply(variant.script);
                    if (record == normal_frame && variant.duration) ticks = *variant.duration;
                }
            }
            next_script_record = normal_frame + 1;
            return ticks;
        }

        bool sample_taunt(unsigned punches, unsigned kicks, bool forward_edge, bool airborne) {
            if (airborne || (taunt_ticks && !--taunt_ticks)) taunt_step = taunt_ticks = 0;
            if (airborne) return false;
            if (taunt_step == 1 && (punches & 2u)) ++taunt_step;
            else if (taunt_step == 2 && forward_edge) ++taunt_step;
            else if (taunt_step >= 3 && (kicks & 2u)) {
                if (++taunt_step == 5) {
                    taunt_step = taunt_ticks = 0;
                    return true;
                }
            } else if (punches & 1u) {
                taunt_step = 1;
                taunt_ticks = 55;
            }
            return false;
        }

        int blockbuster_successor() const {
            using namespace skullgirls;
            switch (active_normal) {
            case gregor_start: return gregor_loop;
            case gregor_air_start: return gregor_air_loop;
            case gregor_loop: case gregor_air_loop:
                return blockbuster_ticks < 31 ? active_normal : active_normal + 1;
            case gregor_end: return gregor_skid;
            case gregor_skid: return gregor_stop;
            case gregor_air_end: return gregor_fall;
            case gregor_fall: return gregor_fall_loop;
            case gregor_fall_loop: return gregor_fall_loop;
            case fenrir_start: return fenrir_rise;
            case fenrir_rise:
                return jump_velocity > 0 ? fenrir_rise : fenrir_connected ? fenrir_peak_hit : fenrir_peak;
            case fenrir_peak: return fenrir_connected ? fenrir_peak_hit : fenrir_fall;
            case fenrir_peak_hit: return fenrir_bite_wait;
            case fenrir_bite_wait: return fenrir_bite;
            case fenrir_bite: return fenrir_dive;
            case fenrir_dive: return fenrir_dive;
            default: return -1;
            }
        }

        void enter_blockbuster_phase(int move) {
            if (move != active_normal) blockbuster_ticks = 0;
            active_normal = move;
            normal_frame = move == skullgirls::fenrir_fall ? skullgirls::fenrir_fall_start : 0;
            normal_tick = 0;
            last_contact = nullptr;
        }

        // Supers use the same live FOIT evaluator as the authored intros and
        // combat entities. Immediate records, contact links and per-tick scripts
        // must share one clock for Gregor into Fenrir to remain a true combo.
        void tick_blockbuster(const skullgirls::FoitMoveTable& table, float left, float right, float opponent_x) {
            using namespace skullgirls;
            if (!blockbuster_playback) {
                auto& playback = blockbuster_playback.emplace();
                playback.root = &table;
                playback.initialize(table);
                playback.state = CerebellaPlayback::defaults();
                for (auto& slot : playback.state.variables) if (!slot) slot = 0.0f;
                for (auto& bit : playback.state.bits) bit = false;
                playback.state.bits[20] = true;
                playback.state.variables[0x1b] = 1.0f;
                playback.state.variables[0xa4] = playback.state.variables[0xa5] = playback.state.variables[0xa6] = 1.0f;
                playback.state.variables[0x36] = filia_gravity;
                playback.defaults = playback.state;
                playback.authored_timeline = playback.floor_collision = true;
                playback.start(171u + unsigned(active_normal - gregor_start));
                playback.x = practice_x;
                playback.y = jump_height;
            }
            auto& playback = *blockbuster_playback;
            playback.inputs = script_inputs;
            playback.facing_right = normal_facing_right;
            playback.state.variables[0x17] = practice_x<opponent_x?1.0f:-1.0f;
            playback.state.variables[0x38] = normal_facing_right ? 1.0f : -1.0f;
            playback.state.variables[0x0d] = (practice_x>=right?2.0f:practice_x<=left?-2.0f:0.0f)*(normal_facing_right?1:-1);
            playback.state.variables[0x42] = float(tension) * 100.0f;
            playback.state.team_variables[0x42] = float(tension) * 100.0f;
            playback.state.variables[0x47] = visual.freeze;
            playback.state.variables[0x3e] = jump_height<=0 && jump_velocity<=0?2.0f:1.0f;
            playback.x = practice_x;
            playback.y = jump_height;
            playback.visual = std::move(visual);
            playback.advance_authored(playback.state);
            visual = std::move(playback.visual);
            practice_x = std::clamp(playback.x, left, right);
            jump_height = std::max(0.0f, playback.y);
            jump_velocity = playback.value(0x9f);
            movement_sprite = normal_pose = playback.pose;
            normal_frame = playback.record ? playback.record - 1 : 0;
            normal_tick = 0;
            script_state = playback.state;
            normal_facing_right = playback.facing_right;
            if (playback.move==167 || playback.move==168) {
                active_normal=striking_normal=fenrir_fall;
                normal_frame += playback.move==167?fenrir_fall_loop:fenrir_whiff_landing;
                return;
            }
            if (playback.move < 171 || playback.move > 190) {
                blockbuster_playback.reset();
                active_normal = -1;
                normal_hit = nullptr;
                return;
            }
            active_normal = striking_normal = int(playback.move - 171) + gregor_start;
        }

        float move_blockbuster_air() {
            using namespace skullgirls;
            float forward = 0;
            if ((active_normal == gregor_start && normal_frame >= 44) ||
                (active_normal == gregor_air_start && normal_frame >= 32) ||
                active_normal == gregor_loop || active_normal == gregor_air_loop) forward = 35;
            if (active_normal == gregor_end || active_normal == gregor_air_end) forward = 28;
            if (active_normal == gregor_skid) forward = std::max(0.0f, 28.0f - blockbuster_ticks * 3.0f);
            if (active_normal == fenrir_start && normal_frame >= 23 && normal_frame <= 24 && !normal_tick) jump_velocity = 42;
            if (active_normal == fenrir_start || active_normal == fenrir_rise) forward = jump_velocity > 0 ? 5 : 0;
            if (active_normal == fenrir_bite && normal_frame == 7 && !normal_tick) jump_velocity = -12;
            if (active_normal == fenrir_dive) {
                jump_velocity = -18;
                forward = 18;
            }
            const bool suspended = active_normal == gregor_air_start || active_normal == gregor_air_loop ||
                active_normal == gregor_air_end || active_normal == fenrir_peak_hit ||
                active_normal == fenrir_bite_wait || (active_normal == fenrir_bite && normal_frame < 7);
            if (!suspended && (jump_height > 0 || jump_velocity > 0)) {
                jump_height = std::max(0.0f, jump_height + jump_velocity);
                jump_velocity = jump_height ? jump_velocity - 2.0f : 0.0f;
            }
            if (!jump_height && (active_normal == gregor_fall || active_normal == gregor_fall_loop))
                enter_blockbuster_phase(gregor_land);
            if (!jump_height && active_normal == fenrir_dive) enter_blockbuster_phase(fenrir_land);
            if (!jump_height && jump_velocity <= 0 && active_normal == fenrir_fall &&
                normal_frame < fenrir_whiff_landing) {
                normal_frame = fenrir_whiff_landing;
                normal_tick = 0;
            }
            return normal_facing_right ? forward : -forward;
        }

        static constexpr skullgirls::DefenseClip burst_clip{skullgirls::burst_animation};
        static constexpr unsigned burst_first_hit_tick = burst_clip.record_tick(7);
        static constexpr unsigned burst_end_hit_tick = burst_clip.record_tick(16);

        bool burst_protected() const {
            return burst_ticks > burst_clip.duration() - 12;
        }

        bool burst_can_hit() const {
            if (squigly) return !burst_connected && squigly->move==507 && squigly->hit && !squigly->hit_resolved;
            if (fukua) return !burst_connected && fukua->move==410 && fukua->hit && !fukua->hit_resolved;
            if (valentine) return !burst_connected && valentine->move == 465 && valentine->hit && !valentine->hit_resolved;
            if (msfortune) return !burst_connected && msfortune->move == 428 && msfortune->hit && !msfortune->hit_resolved;
            if (painwheel) return !burst_connected && painwheel->move == 457 && painwheel->hit && !painwheel->hit_resolved;
            if (parasoul) return !burst_connected && parasoul->move == 439 && parasoul->hit && !parasoul->hit_resolved;
            if (peacock) return !burst_connected && peacock->move == 429 && peacock->hit && !peacock->hit_resolved;
            if (cerebella) return !burst_connected && cerebella->move == 448 && cerebella->hit &&
                !cerebella->hit_resolved;
            const unsigned elapsed = burst_clip.duration() - burst_ticks;
            return !burst_connected && elapsed > burst_first_hit_tick && elapsed <= burst_end_hit_tick;
        }

        bool advance_burst_animation(float left, float right) {
            using namespace skullgirls;
            if (!burst_ticks && !burst_fall_tick) return false;
            normal_input = -1;
            throw_input = false;
            pressed_punches = pressed_kicks = 0;
            if (hit_stop) {
                --hit_stop;
                return true;
            }
            if (burst_ticks) {
                const unsigned tick = burst_clip.duration() - burst_ticks--;
                movement_sprite = burst_clip.pose_at(tick, movement_sprite);
                // Move 406 record 7 writes backward speed 27 and lift 25.
                if (tick == burst_first_hit_tick) {
                    recoil_x = hurt_facing_right ? -27.0f : 27.0f;
                    jump_velocity = 25.0f;
                }
                if (tick >= burst_first_hit_tick && (jump_height > 0 || jump_velocity > 0)) {
                    practice_x = std::clamp(practice_x + recoil_x, left, right);
                    jump_height = std::max(0.0f, jump_height + jump_velocity);
                    jump_velocity = jump_height > 0 ? jump_velocity - 1.0f : 0.0f;
                }
                if (!burst_ticks && jump_height > 0) burst_fall_tick = 1;
                return true;
            }
            constexpr DefenseClip fall{burst_fall};
            constexpr DefenseClip loop{burst_fall_loop};
            const unsigned tick = burst_fall_tick++ - 1;
            movement_sprite = tick < fall.duration() ? fall.pose_at(tick, movement_sprite) :
                loop.pose_at((tick - fall.duration()) % loop.duration(), movement_sprite);
            jump_height = std::max(0.0f, jump_height + jump_velocity);
            jump_velocity -= 1.0f;
            if (!jump_height) {
                jump_velocity = 0;
                movement_sprite = 113;
                landing = 1;
                burst_fall_tick = 0;
            }
            return true;
        }

        skullgirls::DefenseClip pushblock_clip() const {
            using namespace skullgirls;
            switch (guard_stance) {
            case GuardStance::standing: return stand_pushblock;
            case GuardStance::crouching: return crouch_pushblock;
            case GuardStance::airborne: return air_pushblock;
            }
            return stand_pushblock;
        }

        bool guard_locked() const {
            if (blockstun) return true;
            if (guard_phase != GuardPhase::pushblock) return false;
            const auto clip = pushblock_clip();
            // The final FOIT record holds until the gameplay state exits.
            return guard_tick < clip.duration() - clip.frames[clip.size - 1].ticks + 1;
        }

        void begin_guard_stun(bool airborne, bool low, unsigned height, bool facing_right) {
            guard_stance = airborne ? GuardStance::airborne :
                low ? GuardStance::crouching : GuardStance::standing;
            guard_special_height = low ? height == 2 : height == 1;
            guard_phase = GuardPhase::stun;
            guard_tick = 0;
            hurt_facing_right = facing_right;
            movement_sprite = guard_pose = airborne ? 909 :
                low ? (guard_special_height ? 900 : 902) : (guard_special_height ? 889 : 892);
        }

        void advance_guard_stun() {
            using namespace skullgirls;
            const unsigned tick = guard_tick++;
            unsigned pose = guard_pose;
            if (guard_phase == GuardPhase::pushblock) {
                pose = pushblock_clip().pose_at(tick, pose);
            } else if (guard_stance == GuardStance::airborne) {
                constexpr DefenseClip start{air_stun_start};
                constexpr DefenseClip loop{air_stun_loop};
                pose = tick < start.duration() ? start.pose_at(tick, pose) :
                    loop.pose_at((tick - start.duration()) % loop.duration(), pose);
            } else if (tick >= 2) {
                const DefenseClip loop = guard_stance == GuardStance::crouching ?
                    (guard_special_height ? DefenseClip(crouch_low_stun) : DefenseClip(crouch_mid_stun)) :
                    (guard_special_height ? DefenseClip(stand_high_stun) : DefenseClip(stand_mid_stun));
                pose = loop.pose_at((tick - 2) % loop.duration(), pose);
            }
            movement_sprite = guard_pose = pose;
        }

        void advance_guard_pose(bool wants_guard, bool facing_right) {
            using namespace skullgirls;
            if (active_normal >= 0 || hurt_group >= 0 || hit_stop || throw_phase != ThrowPhase::none ||
                fenrir_captured || jump_start || dash_direction || air_dash_direction ||
                landing || tag_entry_ticks || burst_ticks || burst_fall_tick ||
                (movement_sprite >= 920 && movement_sprite <= 932)) {
                guard_phase = GuardPhase::none;
                return;
            }
            if (!wants_guard && guard_phase == GuardPhase::none) return;
            const auto stance = jump_height > 0 ? GuardStance::airborne :
                practice_down ? GuardStance::crouching : GuardStance::standing;
            const auto start = [&](GuardPhase phase) {
                if (guard_phase == GuardPhase::none) guard_pose = sprite();
                guard_phase = phase;
                guard_tick = 0;
            };
            if (wants_guard) {
                if (guard_phase == GuardPhase::none || guard_phase == GuardPhase::leaving ||
                    stance != guard_stance || facing_right != hurt_facing_right)
                    start(GuardPhase::entering);
                else if (guard_phase == GuardPhase::stun || guard_phase == GuardPhase::pushblock)
                    start(GuardPhase::idle);
                guard_stance = stance;
                hurt_facing_right = facing_right;
            } else if (guard_phase != GuardPhase::leaving) {
                start(GuardPhase::leaving);
            }
            struct GuardClips { DefenseClip enter, idle, leave; };
            constexpr GuardClips clips[] = {
                {stand_preblock, stand_guard, stand_postblock},
                {crouch_preblock, crouch_guard, crouch_postblock},
                {air_preblock, air_guard, air_postblock},
            };
            const auto& set = clips[static_cast<unsigned>(guard_stance)];
            const auto clip = guard_phase == GuardPhase::entering ? set.enter :
                guard_phase == GuardPhase::leaving ? set.leave : set.idle;
            movement_sprite = guard_pose = clip.pose_at(guard_tick++, guard_pose);
            if (guard_tick == clip.duration()) {
                if (guard_phase == GuardPhase::leaving) guard_phase = GuardPhase::none;
                else start(GuardPhase::idle);
            }
        }

        void tick(float opponent_x, float left, float right, bool faces_right_at_tie, float ceiling = 600.0f,
                  const skullgirls::FoitMoveTable* scripts = nullptr) {
            normal_hit = nullptr;
            striking_normal = -1;
            if (fenrir_captured || throw_phase != ThrowPhase::none) {
                tag_entry_ticks = 0;
                burst_ticks = burst_fall_tick = 0;
                guard_phase = GuardPhase::none;
            }
            if (tag_entry_ticks) {
                constexpr auto duration = skullgirls::team_animation_ticks(skullgirls::tag_in_taunt);
                movement_sprite = skullgirls::team_animation_sprite(skullgirls::tag_in_taunt,
                    duration - tag_entry_ticks--, 365);
                normal_input = -1;
                throw_input = false;
                pressed_punches = pressed_kicks = 0;
                return;
            }
            if (advance_burst_animation(left, right)) return;
            if (buffered_special && !--buffered_special->ticks) buffered_special.reset();
            const bool wants_throw = std::exchange(throw_input, false);
            const unsigned fresh_punches = std::exchange(pressed_punches, 0u);
            const unsigned fresh_kicks = std::exchange(pressed_kicks, 0u);
            unsigned punches = fresh_punches;
            unsigned kicks = fresh_kicks;
            if (normal_input >= 0 && normal_input < 3) punches |= 1u << normal_input;
            if (normal_input >= 3) kicks |= 1u << (normal_input - 3);
            const unsigned edges = punches | (kicks << 3);
            if (wants_throw && !scripts) punches = kicks = 0;
            const unsigned punch_buttons = (held_attack_buttons | punches) & 7u;
            const bool two_punches = (punch_buttons & (punch_buttons - 1)) != 0;
            const bool punch_dash = !wants_throw && fresh_punches && two_punches;
            std::move_backward(normal_inputs.begin(), normal_inputs.end() - 1, normal_inputs.end());
            normal_inputs.front() = {punches | (kicks << 3), practice_down};
            const int direction = int(practice_right) - int(practice_left);
            const bool airborne = jump_height > 0 || jump_velocity > 0;
            const bool repeated_direction = update_dash_input(direction, airborne);
            if (scripts) {
                update_script_inputs(dash_input_history.front(), edges);
                script_state.variables[0x01] = normal_hit_landed ? 1.0f : 0.0f;
                script_state.variables[0x03] = normal_contact ? 1.0f : 0.0f;
                script_state.variables[0x05] = airborne ? 0.0f : 1.0f;
                script_state.variables[0x06] = 0.0f;
                script_state.variables[0x3e] = airborne ? 1.0f : 2.0f;
                script_state.variables[0x36] = skullgirls::filia_gravity;
                script_state.variables[0x42] = float(tension) * 100.0f;
                script_state.variables[0x47] = visual.freeze;
                script_state.variables[0x85] = 1.0f; // Point fighter can spend meter.
                script_state.variables[0x9e] = jump_facing_right ? jump_horizontal : -jump_horizontal;
                script_state.variables[0x9f] = jump_velocity;
                script_state.bits[15] = ringlet_locked();
                script_state.bits[32] = airborne && airball_used;
            }
            if (fenrir_captured) {
                normal_input = -1;
                return;
            }
            const bool air_throw_cancel = aerial_throw && throw_phase == ThrowPhase::recovery &&
                airborne && !hit_stop && (repeated_direction || punch_dash);
            if (throw_phase != ThrowPhase::none && !air_throw_cancel) {
                normal_input = -1;
                if (hit_stop) --hit_stop;
                else {
                    advance_throw();
                    move_during_throw(left, right);
                }
                return;
            }
            if (air_throw_cancel) {
                throw_phase = ThrowPhase::none;
                normal_input = -1;
                punches = kicks = 0;
            }
            const bool facing_right = practice_x < opponent_x || (practice_x == opponent_x && faces_right_at_tie);
            const auto forward = facing_right ? 0u : 4u;
            if (!airborne) {
                airball_dash_cancelled = airball_used = false;
                script_state.bits[64] = false;
                air_dash_lockout = 0;
                pending_air_dash = 0;
            }
            const bool scripted_normal = scripts && active_normal >= 0 && active_normal < skullgirls::updo_light;
            const auto command_source = scripted_normal ? skullgirls::filia_foit_normals[active_normal] :
                airborne ? 77u : practice_down ? 21u : 20u;
            const bool command_facing = scripted_normal ? normal_facing_right : airborne ? jump_facing_right : facing_right;
            const bool taunt = scripts ? !airborne && (active_normal < 0 || scripted_normal) &&
                script_transition(*scripts, command_source, command_facing,
                    [](unsigned move) { return move == 170; }) == 170 :
                sample_taunt(punches, kicks, dash_input_history[0] == forward && dash_input_history[1] != forward, airborne);
            const auto special = [&](bool command_facing_right, unsigned punch_buttons,
                                     unsigned kick_buttons, bool allow_feint) -> SpecialCommand {
                if (scripts) {
                    const auto target = script_transition(*scripts, command_source, command_facing_right,
                        [](unsigned move) { return (move >= 140 && move <= 145) ||
                            (move >= 147 && move <= 153) || (move >= 161 && move <= 163); });
                    if (target >= 161) return {skullgirls::updo_light + target - 161, unsigned(target - 161)};
                    if (target >= 151) return {skullgirls::air_hairball, unsigned(target - 151)};
                    if (target >= 148) return {skullgirls::ground_hairball, unsigned(target - 148)};
                    if (target == 147) return {skullgirls::ringlet_psych};
                    if (target >= 140) return {skullgirls::ringlet_spike, unsigned(target - 140) / 2,
                                                (target & 1) == 0};
                    return {};
                }
                if (kick_buttons && skullgirls::filia_quarter_circle(dash_input_history, !command_facing_right, airborne)) {
                    const unsigned strength = (kick_buttons & 4u) ? 2 : (kick_buttons & 2u) ? 1 : 0;
                    return {airborne ? skullgirls::air_hairball : skullgirls::ground_hairball, strength};
                }
                if (!airborne && punch_buttons) {
                    const unsigned strength = (punch_buttons & 4u) ? 2 : (punch_buttons & 2u) ? 1 : 0;
                    const bool can_spawn = !ringlet_locked();
                    const bool half_circle = can_spawn &&
                        skullgirls::filia_ringlet_half_circle(dash_input_history, command_facing_right);
                    if (!half_circle && skullgirls::filia_updo_motion(dash_input_history, command_facing_right))
                        return {skullgirls::updo_light + static_cast<int>(strength)};
                    if (can_spawn && (half_circle ||
                        skullgirls::filia_quarter_circle(dash_input_history, command_facing_right, false)))
                        return {skullgirls::ringlet_spike, strength, half_circle};
                }
                if (allow_feint && !airborne && kick_buttons &&
                    skullgirls::filia_quarter_circle(dash_input_history, command_facing_right, false))
                    return SpecialCommand{skullgirls::ringlet_psych};
                return {};
            };
            if (!wants_throw && (fresh_punches || fresh_kicks) &&
                (hit_stop || hurt_group >= 0 || blockstun || active_normal >= skullgirls::updo_light)) {
                // Match against the direction history at the button press, then wait for recovery.
                const auto command = special(facing_right, fresh_punches, fresh_kicks, true);
                if (command.move >= 0) {
                    buffered_special = BufferedSpecial{command, special_recovery_buffer + 1, facing_right};
                    normal_input = -1;
                }
            }
            int throw_move = -1;
            if (scripts && hurt_group < 0 && !guard_locked() && !jump_start && !landing &&
                (active_normal < 0 || scripted_normal)) {
                const auto source = scripted_normal ? command_source : 95u;
                throw_move = script_transition(*scripts, source, command_facing, [](unsigned move) {
                        return (move >= 117 && move <= 120) || move == 130 || move == 131;
                    });
            }
            if (hurt_group < 0 && active_normal >= 0 && active_normal < skullgirls::updo_light && queued_followup < 0) {
                // FUN_004985c0 locks the first eligible transition during hit stop.
                if (!wants_throw)
                    queued_followup = prepare_special(special(normal_facing_right,
                        punches | (held_attack_buttons & 7u), kicks | ((held_attack_buttons >> 3) & 7u),
                        normal_contact), airborne);
                const auto pressed = [&](int button, bool down) {
                    return std::any_of(normal_inputs.begin(), normal_inputs.end(), [&](const auto& sample) {
                        return (sample.buttons & (1u << button)) && (!down || sample.down);
                    });
                };
                const auto rapid_jab = [&] {
                    if (airborne || (!rapid_jab_whiff && !normal_contact)) return -1;
                    if (pressed(0, true)) return skullgirls::rapid_crouch_lp;
                    return pressed(0, false) ? skullgirls::rapid_stand_lp : -1;
                };
                if (queued_followup < 0 && scripted_normal) {
                    const auto target = script_transition(*scripts, command_source, normal_facing_right,
                        [](unsigned move) { return move >= 97 && move <= 116; }, script_move_age - 1);
                    queued_followup = skullgirls::filia_normal_from_foit(target);
                }
                if (!scripts && queued_followup < 0 && active_normal == 0) queued_followup = rapid_jab();
                if (!scripts && queued_followup < 0 && normal_contact) {
                    // Each FOIT button checks its crouching edge before its standing edge.
                    for (int button : {0, 3, 1, 4, 2, 5}) {
                        const bool down = !airborne && pressed(button, true);
                        if (!down && !pressed(button, false)) continue;
                        queued_followup = skullgirls::filia_normal_followup(active_normal, button, airborne, down);
                        if (queued_followup >= 0) break;
                    }
                }
                if (!scripts && queued_followup < 0 && active_normal == 6) queued_followup = rapid_jab();
            }
            if (hit_stop || hurt_group >= 0) {
                if (hurt_group >= 0 || active_normal >= 0) normal_input = -1;
                if (hit_stop) --hit_stop;
                else tick_hurt(left, right, wants_throw || fresh_punches || fresh_kicks, ceiling);
                return;
            }
            if (guard_locked()) {
                if (blockstun) --blockstun;
                advance_guard_stun();
                normal_input = -1;
                return;
            }
            if (scripts && blockbuster_playback) {
                tick_blockbuster(*scripts, left, right, opponent_x);
                return;
            }
            if (throw_move >= 0 || (!scripts && wants_throw && active_normal < 0 &&
                !jump_start && !landing)) {
                buffered_special.reset();
                throw_phase = ThrowPhase::start;
                aerial_throw = airborne;
                throw_facing_right = scripted_normal ? normal_facing_right : airborne ? jump_facing_right : facing_right;
                forward_ground_throw = scripts ? throw_move == 118 || throw_move == 120 :
                    !airborne && direction != (facing_right ? -1 : 1);
                throw_tick = 0;
                normal_input = active_normal = queued_followup = -1;
                dash_direction = air_dash_direction = 0;
                pending_air_dash = 0;
                jump_horizontal = 0;
                ground_transition = GroundTransition::none;
                advance_throw();
                if (aerial_throw) move_during_throw(left, right);
                return;
            }
            int next_normal = queued_followup;
            if (taunt && !jump_start && active_normal < skullgirls::filia_taunt)
                next_normal = skullgirls::filia_taunt;
            const auto super_kicks = ((held_attack_buttons >> 3) | kicks) & 7u;
            if (tension && !jump_start &&
                (active_normal < 0 || scripted_normal || (active_normal < skullgirls::filia_taunt && normal_contact))) {
                const bool facing = command_facing;
                if (scripts && (active_normal < 0 || scripted_normal)) {
                    const auto target = script_transition(*scripts, command_source, facing, [&](unsigned move) {
                        return move == 171 || move == 176 || move == 182 || (move == 191 && !tricobezoar);
                    });
                    switch (target) {
                    case 171: next_normal = skullgirls::gregor_start; break;
                    case 176: next_normal = skullgirls::gregor_air_start; break;
                    case 182: next_normal = skullgirls::fenrir_start; break;
                    case 191: next_normal = skullgirls::tricobezoar; break;
                    }
                } else {
                    const bool quarter_back = skullgirls::filia_quarter_circle(dash_input_history, !facing, airborne);
                    if (!airborne && punches && two_punches) {
                        if (quarter_back && tension >= 3 && !tricobezoar) next_normal = skullgirls::tricobezoar;
                        else if (skullgirls::filia_updo_motion(dash_input_history, facing)) next_normal = skullgirls::fenrir_start;
                    }
                    if (kicks && (super_kicks & (super_kicks - 1)) && quarter_back)
                        next_normal = airborne ? skullgirls::gregor_air_start : skullgirls::gregor_start;
                }
                // A tag chord with a fresh button edge takes priority over its component attacks.
                const unsigned snap_chord = punch_buttons & super_kicks & (punches | kicks) & 6u;
                if (!airborne && snap_chord && (active_normal < 0 || normal_contact) &&
                    skullgirls::filia_quarter_circle(dash_input_history, facing, false)) {
                    next_normal = skullgirls::filia_snapback;
                    snapback_reserve = (snap_chord & 4u) ? 1u : 0u;
                }
                if (next_normal >= skullgirls::tricobezoar) {
                    tension -= next_normal == skullgirls::tricobezoar ? 3 : 1;
                    normal_facing_right = facing;
                    blockbuster_ticks = 0;
                    fenrir_connected = false;
                    air_dash_direction = 0;
                    jump_horizontal = jump_velocity = 0;
                }
            }
            const bool idle_button_dash = punch_dash && !jump_start &&
                (airborne ? !air_dash_used && !air_dash_direction : !dash_direction && !practice_up);
            // The normal landing animation (43) can branch to attack dispatcher 95.
            if (next_normal < 0 && (!wants_throw || scripts) && !jump_start && active_normal < 0) {
                if (buffered_special && !(fresh_punches || fresh_kicks)) {
                    const auto& command = buffered_special->command;
                    if (buffered_special->facing_right == facing_right &&
                        airborne == (command.move == skullgirls::air_hairball) &&
                        (command.move != skullgirls::ringlet_spike || !ringlet_locked()))
                        next_normal = prepare_special(command, airborne);
                    buffered_special.reset();
                }
                if (next_normal < 0 && (normal_input >= 0 || (scripts && edges)))
                    next_normal = prepare_special(special(facing_right, punches, kicks, true), airborne);
                if (next_normal < 0 && (normal_input >= 0 || (scripts && edges)) && !idle_button_dash) {
                    if (scripts) {
                        const auto target = script_transition(*scripts, 95, airborne ? jump_facing_right : facing_right,
                            [](unsigned move) { return move >= 97 && move <= 116; });
                        next_normal = skullgirls::filia_normal_from_foit(target);
                    } else {
                        next_normal = normal_input + (airborne ? 12 : practice_down ? 6 : 0);
                    }
                }
            }
            if (next_normal >= 0) {
                buffered_special.reset();
                landing = 0;
                if (active_normal < 0) normal_facing_right = airborne ? jump_facing_right : facing_right;
                if (next_normal == skullgirls::filia_taunt) normal_facing_right = facing_right;
                active_normal = next_normal;
                pending_air_dash = 0;
                if (next_normal == skullgirls::air_hairball) {
                    airball_used = true;
                    airball_hits = airball_blocks = 0;
                }
                ++attack_instance;
                updo_flight_ticks = 0;
                normal_frame = normal_tick = 0;
                normal_pose = 0;
                next_script_record = 0;
                script_move_age = 0;
                script_state.variables[0x7f] = 0.0f;
                script_state.variables[0xae] = 0.0f;
                script_state.variables[0xb0] = 0.0f;
                script_state.bits[0] = false;
                normal_contact = false;
                normal_hit_landed = false;
                rapid_jab_whiff = false;
                last_contact = nullptr;
                queued_followup = -1;
                dash_direction = 0;
                walk_tick = 0;
                walk_direction = 0;
                ground_transition = GroundTransition::none;
                if (!airborne) {
                    crouching = (next_normal >= 6 && next_normal < 12) || next_normal == skullgirls::rapid_crouch_lp;
                    rising = false;
                    stance_tick = crouching ? static_cast<unsigned>(skullgirls::crouch_enter.size()) : 0;
                }
            }
            if (next_normal < 0 && active_normal == skullgirls::filia_launcher && practice_up && !airborne &&
                (scripts ? script_transition(*scripts, 102, normal_facing_right, [](unsigned move) {
                    return move >= 46 && move <= 48;
                }) >= 0 : normal_contact)) {
                active_normal = -1;
                last_contact = nullptr;
                normal_contact = normal_hit_landed = false;
            }
            normal_input = -1;
            const int relative = facing_right ? direction : -direction;
            // FOIT 43 can branch to jump (44), walk (26/29), or crouch (22).
            if (landing && (practice_up || relative || practice_down)) landing = 0;
            if (!airborne && active_normal < 0 && !practice_down && relative && (crouching || rising)) {
                // Horizontal input can interrupt the neutral stand-up animation.
                crouching = rising = false;
                stance_tick = 0;
                walk_tick = 0;
            }
            const bool doing_updo = active_normal >= skullgirls::updo_light && active_normal < skullgirls::ground_hairball;
            const bool ground_double_tap = repeated_direction && !dash_direction && !practice_down && !crouching &&
                !rising && !jump_start && !landing && !practice_up;
            if (!airborne && active_normal < 0 && (idle_button_dash || ground_double_tap)) {
                dash_direction = relative < 0 ? -1 : 1;
                dash_facing_right = facing_right;
                forward_dash = ForwardDash::start;
                dash_frame = dash_tick = 0;
                crouching = rising = false;
                landing = 0;
            }
            const float walk_speed = relative > 0 ? skullgirls::walk_forward.front().velocity :
                skullgirls::walk_backward.front().velocity;
            float movement = relative ? (facing_right ? walk_speed : -walk_speed) : 0.0f;
            const unsigned previous_sprite = sprite();
            movement_sprite = 0;
            const bool was_dashing = dash_direction != 0;
            const bool forward_dash_jump = dash_direction > 0 && forward_dash != ForwardDash::stop && relative > 0;
            if (!airborne && !jump_start) {
                const int facing = facing_right ? 1 : -1;
                if (active_normal >= 0 || was_dashing || landing || practice_up ||
                    rising || (practice_down != crouching) || (relative && !crouching && ground_facing == facing)) {
                    ground_transition = GroundTransition::none;
                } else if (ground_facing && ground_facing != facing) {
                    ground_transition = crouching ? GroundTransition::crouch_turn : GroundTransition::stand_turn;
                    ground_transition_tick = 0;
                } else if (!relative && walk_direction < 0 && previous_sprite >= 71 && previous_sprite <= 92 &&
                           ground_transition == GroundTransition::none) {
                    ground_transition = GroundTransition::backwalk_stop;
                    ground_transition_tick = 0;
                }
                ground_facing = facing;
            }
            const int next_walk_direction = practice_down || crouching || rising ? 0 : relative;
            if (walk_direction != next_walk_direction || dash_direction || active_normal >= 0) walk_tick = 0;
            walk_direction = next_walk_direction;
            if (dash_direction) {
                if (dash_direction > 0) {
                    const auto change_dash = [&](ForwardDash phase) {
                        forward_dash = phase;
                        dash_frame = dash_tick = 0;
                    };
                    if (scripts) {
                        if (forward_dash == ForwardDash::start && dash_frame + 1 == skullgirls::dash_forward.size()) {
                            script_state.variables[0xb0] = 1.0f; // Immediate record 13 of move 36.
                            if (script_transition(*scripts, 36, dash_facing_right,
                                    [](unsigned move) { return move == 37; }) == 37)
                                change_dash(ForwardDash::extend);
                        } else if (forward_dash == ForwardDash::extend || forward_dash == ForwardDash::loop) {
                            const auto source = forward_dash == ForwardDash::extend ? 37u : 38u;
                            if (script_transition(*scripts, source, dash_facing_right,
                                    [](unsigned move) { return move == 39; }) == 39)
                                change_dash(ForwardDash::stop);
                        }
                    } else if ((forward_dash == ForwardDash::extend || forward_dash == ForwardDash::loop) &&
                               direction != (dash_facing_right ? 1 : -1)) {
                        change_dash(ForwardDash::stop);
                    }
                }
                const auto step_dash = [&](const auto& frames) {
                    const auto& frame = frames[dash_frame];
                    movement_sprite = frame.sprite;
                    movement = dash_facing_right ? frame.velocity : -frame.velocity;
                    if (++dash_tick < frame.ticks) return false;
                    dash_tick = 0;
                    if (++dash_frame < frames.size()) return false;
                    dash_frame = 0;
                    return true;
                };
                if (dash_direction < 0) {
                    if (step_dash(skullgirls::dash_backward)) dash_direction = 0;
                } else {
                    switch (forward_dash) {
                    case ForwardDash::start:
                        if (step_dash(skullgirls::dash_forward)) {
                            if (scripts) dash_direction = 0; // Move 36 defaults to Stand.
                            else forward_dash = ForwardDash::extend;
                        }
                        break;
                    case ForwardDash::extend:
                        if (step_dash(skullgirls::dash_forward_extend)) forward_dash = ForwardDash::loop;
                        break;
                    case ForwardDash::loop:
                        step_dash(skullgirls::dash_forward_loop);
                        break;
                    case ForwardDash::stop:
                        if (step_dash(skullgirls::dash_forward_stop)) dash_direction = 0;
                        break;
                    }
                }
            }
            if (!movement_sprite && walk_direction) {
                const auto sprite_at = [](const auto& frames, unsigned tick) {
                    for (const auto& frame : frames) {
                        if (tick < frame.ticks) return frame.sprite;
                        tick -= frame.ticks;
                    }
                    return 0u;
                };
                if (relative > 0) {
                    movement_sprite = sprite_at(skullgirls::walk_forward, walk_tick);
                    if (!movement_sprite) {
                        walk_tick = 0;
                        movement_sprite = skullgirls::walk_forward.front().sprite;
                    }
                } else {
                    unsigned intro_ticks = 0;
                    for (const auto& frame : skullgirls::walk_backward) intro_ticks += frame.ticks;
                    movement_sprite = sprite_at(skullgirls::walk_backward, walk_tick);
                    if (!movement_sprite) {
                        movement_sprite = sprite_at(skullgirls::walk_backward_loop, walk_tick - intro_ticks);
                        if (!movement_sprite) {
                            walk_tick = intro_ticks;
                            movement_sprite = skullgirls::walk_backward_loop.front().sprite;
                        }
                    }
                }
                ++walk_tick;
            }
            if (active_normal < 0 && practice_up && !jump_start && !jump_velocity && !jump_height && !landing &&
                (!was_dashing || forward_dash_jump)) {
                jump_start = 1;
                jump_direction = relative;
                jump_facing_right = facing_right;
                // Move 52 sets +7/-7. The dash jump in move 53 sets +13 forward.
                super_jump = skullgirls::filia_super_jump(dash_input_history);
                const float speed = super_jump ? 10.0f : forward_dash_jump ? 13.0f : 7.0f;
                jump_horizontal = speed * direction;
                dash_direction = 0;
                air_ticks = 0;
                fall_tick = 0;
                air_dash_fall = air_hurt_fall = false;
                air_dash_used = false;
                crouching = rising = false;
            }
            if (jump_start) {
                constexpr std::array<unsigned, 4> startup{119, 120, 121, 121};
                movement = 0;
                movement_sprite = startup[jump_start - 1] + (super_jump ? 3 : 0);
                if (++jump_start > startup.size()) {
                    jump_start = 0;
                    jump_velocity = super_jump ? 39.0f : 32.0f;
                    air_dash_lockout = skullgirls::filia_air_dash_lockout;
                }
            } else if (!doing_updo && active_normal < skullgirls::gregor_start && (jump_velocity || jump_height)) {
                const int air_relative = jump_facing_right ? direction : -direction;
                const bool recovering_airball = active_normal == skullgirls::air_hairball &&
                    normal_frame >= skullgirls::hairball_end_frame && !airball_dash_cancelled;
                const int airball_link = recovering_airball && scripts ? airball_dash_transition(*scripts) : -1;
                const bool airball_cancel = recovering_airball &&
                    (scripts ? airball_link >= 0 : !air_dash_used && ((repeated_direction && air_relative > 0) || punch_dash));
                const bool air_double_tap = repeated_direction && !air_dash_used && !air_dash_direction;
                if (active_normal < 0 && !pending_air_dash && (idle_button_dash || air_double_tap))
                    pending_air_dash = air_relative < 0 ? -1 : 1;
                if (air_throw_cancel || airball_cancel || (pending_air_dash && !air_dash_lockout)) {
                    if (airball_cancel) {
                        if (scripts) {
                            for (const auto& assignment : scripts->animations[airball_link].front().script)
                                skullgirls::apply_foit_assignment(assignment, script_state);
                        }
                        airball_dash_cancelled = true;
                        jump_facing_right = normal_facing_right;
                        active_normal = queued_followup = -1;
                        last_contact = nullptr;
                    }
                    air_dash_used = true;
                    air_dash_direction = airball_cancel ? 1 : air_throw_cancel ? (air_relative < 0 ? -1 : 1) :
                        pending_air_dash;
                    pending_air_dash = 0;
                    air_dash_tick = 0;
                    fall_tick = 0;
                    air_hurt_fall = false;
                    jump_velocity = 0;
                }
                if (air_dash_lockout) --air_dash_lockout;
                movement = jump_horizontal;
                const unsigned air_direction = jump_direction > 0 ? 1 : jump_direction < 0 ? 2 : 0;
                const bool high_jump = super_jump;
                const unsigned first_pose = (high_jump ? 136 : 130) + 2 * air_direction;
                movement_sprite = first_pose + (air_ticks >= 4);
                if (air_dash_direction) {
                    const auto step = [&](const auto& frames) {
                        auto tick = air_dash_tick++;
                        for (const auto& frame : frames) {
                            if (tick < frame.ticks) {
                                movement_sprite = frame.sprite;
                                return jump_facing_right ? frame.velocity : -frame.velocity;
                            }
                            tick -= frame.ticks;
                        }
                        jump_direction = std::exchange(air_dash_direction, 0);
                        air_dash_fall = true;
                        movement_sprite = jump_direction > 0 ? 171 : 176;
                        jump_horizontal = 0;
                        return 0.0f;
                    };
                    movement = air_dash_direction > 0 ? step(skullgirls::air_dash_forward) :
                        step(skullgirls::air_dash_backward);
                }
                if (!air_dash_direction) {
                    if (jump_velocity < 7.0f || air_dash_fall || air_hurt_fall) {
                        const unsigned fall_direction = jump_direction > 0 ? 1 : jump_direction < 0 ? 2 : 0;
                        const unsigned fall = 166 + 5 * fall_direction;
                        const auto& descent = skullgirls::jump_descent;
                        const bool skip_approach = air_dash_fall || air_hurt_fall;
                        if (!skip_approach && fall_tick < descent.size()) {
                            const unsigned approach = (high_jump ? 154 : 142) + 4 * fall_direction;
                            movement_sprite = approach + descent[fall_tick++];
                        } else {
                            const auto tick = skip_approach ? fall_tick : fall_tick - descent.size();
                            const auto& entry = skullgirls::fall_entry;
                            const auto& high_entry = skullgirls::high_jump_fall_entry;
                            const bool use_high_entry = (high_jump && !air_dash_fall) || air_hurt_fall;
                            if (use_high_entry && tick < high_entry.size()) {
                                movement_sprite = 181 + 3 * fall_direction + high_entry[tick];
                                ++fall_tick;
                            } else if (!use_high_entry && tick < entry.size()) {
                                movement_sprite = fall + entry[tick];
                                ++fall_tick;
                            } else {
                                movement_sprite = fall + 4;
                            }
                        }
                    }
                    if (!fall_tick && movement_sprite == first_pose) ++air_ticks;
                    if (active_normal == skullgirls::air_hairball && normal_frame >= skullgirls::hairball_loop_frame &&
                        normal_frame < skullgirls::hairball_end_frame)
                        jump_velocity = airball_lift;
                    jump_height += jump_velocity;
                    jump_velocity -= active_normal == skullgirls::air_hairball ? 1.0f : skullgirls::filia_gravity;
                }
                if (!air_dash_direction && jump_height <= 0 && jump_velocity < 0) {
                    jump_height = jump_velocity = 0;
                    pending_air_dash = 0;
                    // The airborne roll keeps its own ending (155 -> 157) on floor contact.
                    if (active_normal != skullgirls::air_hairball) {
                        movement_sprite = 113;
                        active_normal = -1;
                        queued_followup = -1;
                        landing = 1;
                    }
                }
            } else if (landing) {
                const auto& poses = skullgirls::jump_landing;
                movement_sprite = poses[landing - 1];
                movement = 0;
                if (++landing > poses.size()) landing = 0;
            } else if (active_normal < 0 && !was_dashing && (practice_down || crouching || rising)) {
                const auto& down_poses = skullgirls::crouch_enter;
                const auto& up_poses = skullgirls::crouch_exit;
                const auto& idle_duration = skullgirls::crouch_idle_duration;
                movement = 0;
                if (!practice_down && crouching) {
                    crouching = false;
                    rising = true;
                    stance_tick = 0;
                } else if (practice_down && !crouching) {
                    crouching = true;
                    rising = false;
                    stance_tick = 0;
                }
                if (rising) {
                    movement_sprite = up_poses[stance_tick++];
                    if (stance_tick == up_poses.size()) rising = false;
                } else if (stance_tick < down_poses.size()) {
                    movement_sprite = down_poses[stance_tick++];
                } else {
                    auto tick = stance_tick++ - down_poses.size();
                    unsigned pose = 0;
                    while (pose < idle_duration.size() && tick >= idle_duration[pose])
                        tick -= idle_duration[pose++];
                    if (pose == idle_duration.size()) {
                        pose = 0;
                        stance_tick = static_cast<unsigned>(down_poses.size()) + 1;
                    }
                    movement_sprite = 22 + pose;
                }
            }
            if (ground_transition != GroundTransition::none) {
                const auto step_transition = [&](const auto& poses) {
                    movement_sprite = poses[ground_transition_tick++];
                    movement = 0;
                    if (ground_transition_tick == poses.size()) ground_transition = GroundTransition::none;
                };
                switch (ground_transition) {
                case GroundTransition::stand_turn: step_transition(skullgirls::stand_turn); break;
                case GroundTransition::crouch_turn: step_transition(skullgirls::crouch_turn); break;
                case GroundTransition::backwalk_stop: step_transition(skullgirls::backwalk_stop); break;
                case GroundTransition::none: break;
                }
            }
            if (doing_updo) {
                const auto& updo = skullgirls::updo_motion[active_normal - skullgirls::updo_light];
                if (normal_frame == updo.lift_frame && normal_tick == 0) {
                    jump_velocity = updo.upward;
                    jump_horizontal = updo.forward;
                    updo_flight_ticks = updo.flight_ticks;
                }
                if (jump_height > 0 || jump_velocity > 0) {
                    jump_height = std::max(0.0f, jump_height + jump_velocity);
                    jump_velocity -= updo.gravity;
                    if (jump_height == 0) {
                        jump_velocity = jump_horizontal = 0;
                        normal_frame = updo.flight_frame + skullgirls::updo_air_loop_end;
                        normal_tick = 0;
                    }
                }
            }
            const bool moving_blockbuster = active_normal >= skullgirls::gregor_start &&
                active_normal <= skullgirls::fenrir_land;
            if (scripts && moving_blockbuster) {
                tick_blockbuster(*scripts, left, right, opponent_x);
                return;
            }
            const float blockbuster_movement = moving_blockbuster ? move_blockbuster_air() : 0.0f;
            if (active_normal >= 0) {
                striking_normal = active_normal;
                if (active_normal == skullgirls::ringlet_spike && ringlet_entry_ticks) {
                    --ringlet_entry_ticks;
                    normal_pose = movement_sprite = previous_sprite;
                    return;
                }
                const bool fenrir_whiff = active_normal == skullgirls::fenrir_fall;
                const auto& sequence = skullgirls::filia_normals[fenrir_whiff ? skullgirls::updo_light : active_normal];
                normal_frame = skullgirls::next_visible_filia_normal_record(active_normal, normal_frame);
                if (scripts) update_visuals(*scripts);
                if (fenrir_whiff) {
                    // These FOIT records advance within the tick without drawing a pose.
                    while (sequence[normal_frame].ticks && !sequence[normal_frame].sprite) ++normal_frame;
                }
                const auto& frame = sequence[normal_frame];
                const auto frame_ticks = scripts && active_normal < skullgirls::updo_light
                    ? apply_normal_script(*scripts) : frame.ticks;
                if (active_normal == skullgirls::ringlet_spike && normal_frame == skullgirls::ringlet_spawn_frame &&
                    normal_tick == 0) {
                    const float distance = skullgirls::ringlet_distance[ringlet_strength];
                    // FUN_0049a260 offsets the parent's integer world position by facing.
                    ringlet_spikes.push_back({static_cast<int>(practice_x) +
                        (normal_facing_right ? distance : -distance), normal_facing_right, attack_instance, palette_slot});
                }
                if (active_normal == skullgirls::tricobezoar && normal_frame == skullgirls::tricobezoar_spawn_frame &&
                    normal_tick == 0) {
                    tricobezoar = Tricobezoar{static_cast<int>(practice_x) + (normal_facing_right ? -17.0f : 17.0f),
                        static_cast<int>(jump_height) + 408.0f, normal_facing_right, attack_instance, palette_slot};
                }
                rapid_jab_whiff = skullgirls::filia_rapid_jab_whiff(active_normal, normal_frame + 1);
                if (frame.sprite) normal_pose = frame.sprite;
                if (normal_pose) movement_sprite = normal_pose;
                for (const auto& hit : skullgirls::filia_normal_hits[active_normal]) {
                    if (!hit.frame || hit.frame > normal_frame + 1) break;
                    normal_hit = &hit;
                }
                if (normal_hit && normal_hit->clear_frame && normal_frame + 1 >= normal_hit->clear_frame)
                    normal_hit = nullptr;
                if (active_normal < 12 || active_normal >= skullgirls::rapid_stand_lp) movement = 0;
                if (moving_blockbuster) {
                    movement = blockbuster_movement;
                    ++blockbuster_ticks;
                }
                if (doing_updo && jump_height > 0) {
                    const auto& updo = skullgirls::updo_motion[active_normal - skullgirls::updo_light];
                    movement = normal_facing_right ? jump_horizontal : -jump_horizontal;
                    jump_horizontal = std::max(0.0f, jump_horizontal - updo.friction);
                }
                const bool hairball_roll =
                    (active_normal == skullgirls::ground_hairball || active_normal == skullgirls::air_hairball) &&
                    normal_frame >= skullgirls::hairball_loop_frame && normal_frame < skullgirls::hairball_end_frame;
                if (hairball_roll) {
                    if (normal_frame == skullgirls::hairball_loop_frame && normal_tick == 0)
                        last_contact = nullptr;
                    movement = normal_facing_right ? hairball_speed : -hairball_speed;
                    if (--hairball_roll_ticks == 0) {
                        normal_frame = skullgirls::hairball_end_frame;
                        normal_tick = 0;
                        practice_x = std::clamp(practice_x + movement, left, right);
                        return;
                    }
                }
                if (doing_updo) {
                    const auto& updo = skullgirls::updo_motion[active_normal - skullgirls::updo_light];
                    if (normal_frame >= updo.flight_frame && normal_frame < updo.flight_frame + 4 &&
                        updo_flight_ticks && --updo_flight_ticks == 0) {
                        normal_frame = updo.flight_frame + 4;
                        normal_tick = 0;
                        practice_x = std::clamp(practice_x + movement, left, right);
                        return;
                    }
                }
                if (++normal_tick >= frame_ticks) {
                    normal_tick = 0;
                    normal_frame = skullgirls::next_visible_filia_normal_record(active_normal, normal_frame + 1);
                    if (hairball_roll && normal_frame == skullgirls::hairball_end_frame) {
                        normal_frame = skullgirls::hairball_loop_frame;
                    }
                    if (doing_updo && jump_height > 0) {
                        const auto flight = skullgirls::updo_motion[active_normal - skullgirls::updo_light].flight_frame;
                        if (normal_frame == flight + skullgirls::updo_air_loop_end)
                            normal_frame = flight + skullgirls::updo_air_loop_start;
                    }
                    if (fenrir_whiff && normal_frame == skullgirls::fenrir_whiff_landing)
                        normal_frame = skullgirls::fenrir_fall_loop;
                    if (normal_frame == std::size(sequence) || !sequence[normal_frame].ticks) {
                        if (scripts) update_visuals(*scripts, true);
                        if (active_normal == skullgirls::air_hairball && !jump_height) landing = 1;
                        if (moving_blockbuster) enter_blockbuster_phase(blockbuster_successor());
                        else active_normal = -1;
                        queued_followup = -1;
                    }
                }
            }
            practice_x = std::clamp(practice_x + movement, left, right);
        }

        bool mirror(bool facing_right) const {
            if (auto* actor = scripted()) return !actor->facing_right;
            if (movement_sprite >= 1000 && movement_sprite <= 1035) return !normal_facing_right;
            if (blockstun) return !hurt_facing_right;
            if (throw_phase != ThrowPhase::none) return !throw_facing_right;
            if (movement_sprite >= 702) return !hurt_facing_right;
            if (movement_sprite >= 198) return !normal_facing_right;
            if (movement_sprite >= 113) return !jump_facing_right;
            if (movement_sprite >= 93) return !dash_facing_right;
            return !facing_right;
        }

        unsigned sprite() const {
            if (auto* actor = scripted()) return actor->pose;
            if (movement_sprite) return movement_sprite;
            // FOIT idle move 20 loops on this fighter's animation clock.
            constexpr std::array<unsigned, 20> duration{
                4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4};
            auto tick = idle_tick;
            unsigned index = 0;
            while (tick >= duration[index]) tick -= duration[index++];
            return 2 + index;
        }
    };
    std::array<PracticeFighter, 2> practice_players;
    struct PracticeTeamTravel {
        enum class Motion { tag_in, assist_in, tag_out, assist_out, snap_out };
        static constexpr float speed = 38.0f;
        float x, height = 0;
        bool facing_right = true;
        Motion motion = Motion::tag_in;
        unsigned ticks = 0;
        float span = 0;
        unsigned pose = 0;
        unsigned landing_ticks = 0;
        unsigned exit_flight_ticks = 0;
        float takeoff_height = 0;
        float progress = 0;
        std::optional<float> exit_gate;

        bool advance_assist_exit(float destination, unsigned tick) {
            using namespace skullgirls;
            constexpr unsigned startup = team_animation_ticks(assist_exit_start);
            constexpr unsigned rise = team_animation_ticks(assist_exit_rise);
            constexpr unsigned peak = team_animation_ticks(assist_exit_peak);
            constexpr unsigned fall = team_animation_ticks(assist_exit_fall);
            constexpr unsigned minimum_flight = rise + peak + fall + 2; // One tick in each hold.
            if (tick < startup) {
                pose = team_animation_sprite(assist_exit_start, tick, 119);
                return false;
            }
            if (!exit_flight_ticks) {
                takeoff_height = height;
                exit_flight_ticks = std::max(minimum_flight,
                    static_cast<unsigned>(std::ceil(std::abs(destination - x) / speed)));
            }
            const unsigned flight_tick = tick - startup;
            if (!landing_ticks && (flight_tick < exit_flight_ticks || x != destination)) {
                const unsigned left = exit_flight_ticks - std::min(flight_tick, exit_flight_ticks - 1);
                const float distance = destination - x;
                x = left == 1 && std::abs(distance) <= speed ? destination :
                    x + std::clamp(distance / left, -speed, speed);
                const float progress = float(std::min(flight_tick + 1, exit_flight_ticks)) / exit_flight_ticks;
                height = 180.0f * 4 * progress * (1 - progress) + takeoff_height * (1 - progress);
                const unsigned peak_at = rise + 1 + (exit_flight_ticks - minimum_flight) / 2;
                if (flight_tick < rise)
                    pose = team_animation_sprite(assist_exit_rise, flight_tick, pose);
                else if (flight_tick < peak_at) pose = assist_exit_rise_hold;
                else if (flight_tick < peak_at + peak)
                    pose = team_animation_sprite(assist_exit_peak, flight_tick - peak_at, pose);
                else if (flight_tick < peak_at + peak + fall)
                    pose = team_animation_sprite(assist_exit_fall, flight_tick - peak_at - peak, pose);
                else pose = assist_exit_fall_hold;
                return false;
            }
            height = 0;
            pose = team_animation_sprite(assist_exit_land, landing_ticks++, assist_exit_fall_hold);
            return landing_ticks > team_animation_ticks(assist_exit_land);
        }

        bool advance(float destination) {
            using namespace skullgirls;
            const unsigned tick = ticks++;
            if (motion == Motion::assist_out) return advance_assist_exit(destination, tick);
            constexpr unsigned snap_start = team_animation_ticks(snap_out_start);
            if (motion == Motion::snap_out && tick < snap_start) {
                pose = team_animation_sprite(snap_out_start, tick, 769);
                return false;
            }
            const float distance = destination - x;
            if (!span) {
                span = std::max(std::abs(distance), speed);
                takeoff_height = height;
            }
            const bool arrived = std::abs(distance) <= speed;
            x = arrived ? destination : x + std::copysign(speed, distance);
            progress = std::max(progress, std::clamp(1.0f - std::abs(destination - x) / span, 0.0f, 1.0f));
            switch (motion) {
            case Motion::tag_in:
                height = takeoff_height * (1.0f - progress);
                pose = 365; // TagIn_Normal_Loop, move 447
                return arrived;
            case Motion::assist_in:
                if (!arrived) {
                    height = takeoff_height * (1.0f - progress);
                    pose = 175; // Assist_Called_Loop, move 449
                    return false;
                }
                height = 0;
                pose = team_animation_sprite(assist_landing, landing_ticks++, 175);
                return landing_ticks > team_animation_ticks(assist_landing);
            case Motion::tag_out:
                height = takeoff_height + 120.0f * progress;
                pose = progress < 0.8f ? 1012 : team_animation_sprite(tag_out_fall,
                    static_cast<unsigned>((progress - 0.8f) * 5 * team_animation_ticks(tag_out_fall)), 1012);
                return arrived;
            case Motion::snap_out:
                height = takeoff_height + 144.0f * progress;
                pose = team_animation_sprite(snap_out_fly,
                    (tick - snap_start) % team_animation_ticks(snap_out_fly), 769);
                return arrived;
            case Motion::assist_out: break;
            }
            return false;
        }

        bool advance_exit(float gate, bool left) {
            // A panning camera can reveal more of the path while an actor is leaving.
            // Extend the route outward, keeping the actor moving in the same direction.
            exit_gate = exit_gate ? (left ? std::min(*exit_gate, gate) : std::max(*exit_gate, gate)) : gate;
            return advance(*exit_gate);
        }
    };
    struct PracticeTeamActor {
        std::size_t slot;
        bool home_on_left;
        PracticeTeamTravel travel;
    };
    struct PracticeTag {
        enum class Kind { regular, guard, knockout, snapback };
        PracticeTeamActor incoming;
        Kind kind;
        unsigned startup_tick = 0;
        std::optional<PracticeTeamTravel> forced_exit;
        bool from_burst = false;
    };
    struct PracticeAssist {
        PracticeFighter fighter;
        PracticeFighter::SpecialCommand command;
        PracticeTeamActor actor;
        enum class Phase { entering, acting, leaving } phase = Phase::entering;
    };
    struct PracticeTeam {
        std::size_t active_slot = 0;
        std::array<PracticeLife, 3> health{};
        std::optional<PracticeAssist> assist;
        std::optional<PracticeTag> tag;
        std::optional<PracticeTeamActor> departing;
        unsigned assist_cooldown = 0;
    };
    std::array<PracticeTeam, 2> practice_teams;

    bool practice_point_snapped(std::size_t side) const {
        const auto& tag = practice_teams[side].tag;
        return tag && tag->kind == PracticeTag::Kind::snapback;
    }

    bool practice_slot_is_filia(std::size_t side, std::size_t slot) const {
        return slot < fighters[side].size() && std::string_view(roster[fighters[side][slot]]) == "filia";
    }
    bool practice_slot_is_cerebella(std::size_t side, std::size_t slot) const {
        return slot < fighters[side].size() && std::string_view(roster[fighters[side][slot]]) == "cerebella";
    }
    bool practice_slot_is_peacock(std::size_t side, std::size_t slot) const {
        return slot < fighters[side].size() && std::string_view(roster[fighters[side][slot]]) == "peacock";
    }
    bool practice_slot_is_squigly(std::size_t side, std::size_t slot) const {
        return slot < fighters[side].size() && std::string_view(roster[fighters[side][slot]]) == "squigly";
    }
    bool practice_slot_is_fukua(std::size_t side, std::size_t slot) const {
        return slot < fighters[side].size() && std::string_view(roster[fighters[side][slot]]) == "fukua";
    }
    bool practice_slot_is_valentine(std::size_t side, std::size_t slot) const {
        return slot < fighters[side].size() && std::string_view(roster[fighters[side][slot]]) == "valentine";
    }
    bool practice_slot_is_parasoul(std::size_t side, std::size_t slot) const {
        return slot < fighters[side].size() && std::string_view(roster[fighters[side][slot]]) == "parasoul";
    }
    bool practice_slot_is_painwheel(std::size_t side, std::size_t slot) const {
        return slot < fighters[side].size() && std::string_view(roster[fighters[side][slot]]) == "painwheel";
    }
    bool practice_slot_is_msfortune(std::size_t side, std::size_t slot) const {
        return slot < fighters[side].size() && std::string_view(roster[fighters[side][slot]]) == "msfortune";
    }
    bool practice_slot_is_robofortune(std::size_t side, std::size_t slot) const {
        return slot < fighters[side].size() && std::string_view(roster[fighters[side][slot]]) == "robofortune";
    }
    bool practice_slot_playable(std::size_t side, std::size_t slot) const {
        return practice_slot_is_squigly(side, slot) || practice_slot_is_fukua(side, slot) || practice_slot_is_valentine(side, slot) || practice_slot_is_robofortune(side, slot) || practice_slot_is_painwheel(side, slot) || practice_slot_is_filia(side, slot) || practice_slot_is_cerebella(side, slot) || practice_slot_is_peacock(side, slot) || practice_slot_is_parasoul(side, slot) || practice_slot_is_msfortune(side, slot);
    }
    std::array<float, 2> practice_starts{};
    float practice_width = 0, practice_height = 0, practice_floor = 0;
    float practice_ceiling() const { return std::max(240.0f, practice_height - practice_floor - 100.0f); }
    std::vector<skullgirls::SelectFrame> movement_frames;
    std::vector<skullgirls::SpriteFrame> combat_metadata;
    skullgirls::FoitMoveTable filia_move_table;
    std::map<unsigned, skullgirls::SelectFrame> jab_overlays;
    skullgirls::FoitAnimationAsset cerebella_assets;
    std::vector<skullgirls::SelectFrame> cerebella_frames;
    std::map<unsigned, skullgirls::SelectFrame> cerebella_overlays;

    skullgirls::FoitAnimationAsset peacock_assets;
    std::vector<skullgirls::SelectFrame> peacock_frames;
    std::map<unsigned, skullgirls::SelectFrame> peacock_overlays;
    skullgirls::FoitAnimationAsset parasoul_assets;
    std::vector<skullgirls::SelectFrame> parasoul_frames;
    std::map<unsigned, skullgirls::SelectFrame> parasoul_overlays;
    skullgirls::FoitAnimationAsset painwheel_assets;
    std::vector<skullgirls::SelectFrame> painwheel_frames;
    std::map<unsigned, skullgirls::SelectFrame> painwheel_overlays;
    skullgirls::FoitAnimationAsset squigly_assets;
    std::vector<skullgirls::SelectFrame> squigly_frames;
    std::map<unsigned, skullgirls::SelectFrame> squigly_overlays;
    skullgirls::FoitAnimationAsset fukua_assets;
    std::vector<skullgirls::SelectFrame> fukua_frames;
    std::map<unsigned, skullgirls::SelectFrame> fukua_overlays;
    skullgirls::FoitAnimationAsset valentine_assets;
    std::vector<skullgirls::SelectFrame> valentine_frames;
    std::map<unsigned, skullgirls::SelectFrame> valentine_overlays;
    skullgirls::FoitAnimationAsset robofortune_assets;
    std::vector<skullgirls::SelectFrame> robofortune_frames;
    std::map<unsigned, skullgirls::SelectFrame> robofortune_overlays;
    skullgirls::FoitAnimationAsset msfortune_assets;
    std::vector<skullgirls::SelectFrame> msfortune_frames;
    std::map<unsigned, skullgirls::SelectFrame> msfortune_overlays;
    std::vector<skullgirls::SelectFrame> msfortune_headless_frames;
    std::map<unsigned, skullgirls::SelectFrame> msfortune_headless_overlays;

    const std::vector<skullgirls::SpriteFrame>& practice_metadata(const PracticeFighter& fighter) const {
        return fighter.squigly ? squigly_assets.sprites : fighter.fukua ? fukua_assets.sprites : fighter.valentine ? valentine_assets.sprites : fighter.robofortune ? robofortune_assets.sprites : fighter.painwheel ? painwheel_assets.sprites : fighter.msfortune ? msfortune_assets.sprites : fighter.parasoul ? parasoul_assets.sprites : fighter.peacock ? peacock_assets.sprites : fighter.cerebella ? cerebella_assets.sprites : combat_metadata;
    }

    void load_cerebella() {
        load_scripted_character("cerebella", cerebella_assets, cerebella_frames, cerebella_overlays);
    }
    void load_peacock() {
        load_scripted_character("peacock", peacock_assets, peacock_frames, peacock_overlays);
    }
    void load_squigly() {
        load_scripted_character("squigly", squigly_assets, squigly_frames, squigly_overlays);
    }
    void load_fukua() {
        load_scripted_character("fukua", fukua_assets, fukua_frames, fukua_overlays);
    }
    void load_valentine() {
        load_scripted_character("valentine", valentine_assets, valentine_frames, valentine_overlays);
    }
    void load_parasoul() {
        load_scripted_character("parasoul", parasoul_assets, parasoul_frames, parasoul_overlays);
    }
    void load_robofortune() {
        load_scripted_character("robofortune", robofortune_assets, robofortune_frames, robofortune_overlays);
    }
    void load_painwheel() {
        load_scripted_character("painwheel", painwheel_assets, painwheel_frames, painwheel_overlays);
    }
    void load_msfortune() {
        load_scripted_character("msfortune", msfortune_assets, msfortune_frames, msfortune_overlays);
        if (!msfortune_headless_frames.empty()) return;
        skullgirls::CharacterAssets art(game_data, "msfortune");
        auto frames = msfortune_frames;
        auto overlays = msfortune_overlays;
        for (unsigned ref = 2; ref < frames.size(); ++ref) {
            const auto& sprite = msfortune_assets.sprites[ref - 1];
            const auto& layer = sprite.layers[4];
            if (!layer || !layer->used_tiles) continue;
            const auto* bank = art.find_art(sprite.art_bank);
            if (!bank) continue;

            auto& frame = frames[ref];
            frame.width = layer->tile_columns * 16;
            frame.height = layer->tile_rows * 16;
            frame.origin_x = static_cast<float>(std::int64_t(sprite.origin[0]) - sprite.layer_offsets[4][0]);
            frame.origin_y = static_cast<float>(std::int64_t(sprite.origin[1]) - sprite.layer_offsets[4][1]);
            frame.pixels = skullgirls::decode_sprite_layer(*layer, *bank);
            overlays.erase(ref);
            if (const auto& overlay = sprite.layers[5]; overlay && overlay->used_tiles &&
                overlay->tile_columns == layer->tile_columns && overlay->tile_rows == layer->tile_rows) {
                auto& effect = overlays[ref];
                effect.width = frame.width;
                effect.height = frame.height;
                effect.pixels = skullgirls::decode_sprite_layer(*overlay, *bank);
            }
        }
        // A later Training load can retry if any of the bank reads or decodes fail.
        msfortune_headless_frames = std::move(frames);
        msfortune_headless_overlays = std::move(overlays);
    }
    // Decode a fighter's FOIT and sprite channels once. Apply the selected palette when drawing.
    void load_scripted_character(const std::string& name, skullgirls::FoitAnimationAsset& destination,
                                 std::vector<skullgirls::SelectFrame>& frames,
                                 std::map<unsigned, skullgirls::SelectFrame>& layer_overlays) {
        if (!frames.empty()) return;
        skullgirls::CharacterAssets assets(game_data, name);
        auto data = assets.read_animations(true);
        std::vector<skullgirls::SelectFrame> decoded(data.sprites.size() + 1);
        std::map<unsigned, skullgirls::SelectFrame> overlays;
        for (unsigned ref = 2; ref < decoded.size(); ++ref) {
            const auto& source = data.sprites[ref - 1];
            const unsigned layer_index = name=="msfortune" && source.layers[2] && source.layers[2]->used_tiles ? 2 : 0;
            const auto& layer = source.layers[layer_index];
            if (!layer || !layer->used_tiles) continue;
            const auto* bank = assets.find_art(source.art_bank);
            if (!bank) continue;
            auto& frame = decoded[ref];
            frame.width = layer->tile_columns * 16;
            frame.height = layer->tile_rows * 16;
            frame.origin_x = float(source.origin[0]) - float(source.layer_offsets[layer_index][0]);
            frame.origin_y = float(source.origin[1]) - float(source.layer_offsets[layer_index][1]);
            frame.pixels = skullgirls::decode_sprite_layer(*layer, *bank);
            if (const auto& overlay = source.layers[layer_index+1]; overlay && overlay->used_tiles &&
                overlay->tile_columns == layer->tile_columns && overlay->tile_rows == layer->tile_rows) {
                auto& effect = overlays[ref];
                effect.width = frame.width;
                effect.height = frame.height;
                effect.pixels = skullgirls::decode_sprite_layer(*overlay, *bank);
            }
        }
        destination = std::move(data);
        frames = std::move(decoded);
        layer_overlays = std::move(overlays);
    }

    unsigned peacock_team_pose(unsigned reference) const {
        for (unsigned id = 0; id < filia_move_table.animations.size(); ++id) {
            const auto& source = filia_move_table.animations[id];
            const auto frame = std::find_if(source.begin(), source.end(), [&](const auto& item) { return item.sprites[1] == reference; });
            if (frame == source.end()) continue;
            const auto name = std::find(peacock_assets.names.begin(), peacock_assets.names.end(), filia_move_table.names[id]);
            if (name == peacock_assets.names.end()) continue;
            const auto& clip = peacock_assets.animations[name - peacock_assets.names.begin()];
            if (clip.empty()) continue;
            const auto position = static_cast<std::size_t>(frame - source.begin());
            const auto index = std::min(position * clip.size() / source.size(), clip.size() - 1);
            unsigned pose = 2;
            for (std::size_t i = 0; i <= index; ++i) if (clip[i].sprites[1] > 1) pose = clip[i].sprites[1];
            return pose;
        }
        return 2;
    }

    unsigned squigly_team_pose(unsigned reference) const {
        for (unsigned id = 0; id < filia_move_table.animations.size(); ++id) {
            const auto& source = filia_move_table.animations[id];
            const auto frame = std::find_if(source.begin(), source.end(), [&](const auto& item) { return item.sprites[1] == reference; });
            if (frame == source.end()) continue;
            const auto name = std::find(squigly_assets.names.begin(), squigly_assets.names.end(), filia_move_table.names[id]);
            if (name == squigly_assets.names.end()) continue;
            const auto& clip = squigly_assets.animations[name - squigly_assets.names.begin()];
            if (clip.empty()) continue;
            const auto position = static_cast<std::size_t>(frame - source.begin());
            const auto index = std::min(position * clip.size() / source.size(), clip.size() - 1);
            unsigned pose = 2;
            for (std::size_t i = 0; i <= index; ++i) if (clip[i].sprites[1] > 1) pose = clip[i].sprites[1];
            return pose;
        }
        return 2;
    }
    unsigned fukua_team_pose(unsigned reference) const {
        for (unsigned id = 0; id < filia_move_table.animations.size(); ++id) {
            const auto& source = filia_move_table.animations[id];
            const auto frame = std::find_if(source.begin(), source.end(), [&](const auto& item) { return item.sprites[1] == reference; });
            if (frame == source.end()) continue;
            const auto name = std::find(fukua_assets.names.begin(), fukua_assets.names.end(), filia_move_table.names[id]);
            if (name == fukua_assets.names.end()) continue;
            const auto& clip = fukua_assets.animations[name - fukua_assets.names.begin()];
            if (clip.empty()) continue;
            const auto position = static_cast<std::size_t>(frame - source.begin());
            const auto index = std::min(position * clip.size() / source.size(), clip.size() - 1);
            unsigned pose = 2;
            for (std::size_t i = 0; i <= index; ++i) if (clip[i].sprites[1] > 1) pose = clip[i].sprites[1];
            return pose;
        }
        return 2;
    }
    unsigned valentine_team_pose(unsigned reference) const {
        for (unsigned id = 0; id < filia_move_table.animations.size(); ++id) {
            const auto& source = filia_move_table.animations[id];
            const auto frame = std::find_if(source.begin(), source.end(), [&](const auto& item) { return item.sprites[1] == reference; });
            if (frame == source.end()) continue;
            const auto name = std::find(valentine_assets.names.begin(), valentine_assets.names.end(), filia_move_table.names[id]);
            if (name == valentine_assets.names.end()) continue;
            const auto& clip = valentine_assets.animations[name - valentine_assets.names.begin()];
            if (clip.empty()) continue;
            const auto position = static_cast<std::size_t>(frame - source.begin());
            const auto index = std::min(position * clip.size() / source.size(), clip.size() - 1);
            unsigned pose = 2;
            for (std::size_t i = 0; i <= index; ++i) if (clip[i].sprites[1] > 1) pose = clip[i].sprites[1];
            return pose;
        }
        return 2;
    }
    unsigned parasoul_team_pose(unsigned reference) const {
        for (unsigned id = 0; id < filia_move_table.animations.size(); ++id) {
            const auto& source = filia_move_table.animations[id];
            const auto frame = std::find_if(source.begin(), source.end(), [&](const auto& item) { return item.sprites[1] == reference; });
            if (frame == source.end()) continue;
            const auto name = std::find(parasoul_assets.names.begin(), parasoul_assets.names.end(), filia_move_table.names[id]);
            if (name == parasoul_assets.names.end()) continue;
            const auto& clip = parasoul_assets.animations[name - parasoul_assets.names.begin()];
            if (clip.empty()) continue;
            const auto position = static_cast<std::size_t>(frame - source.begin());
            const auto index = std::min(position * clip.size() / source.size(), clip.size() - 1);
            unsigned pose = 2;
            for (std::size_t i = 0; i <= index; ++i) if (clip[i].sprites[1] > 1) pose = clip[i].sprites[1];
            return pose;
        }
        return 2;
    }

    unsigned painwheel_team_pose(unsigned reference) const {
        for (unsigned id = 0; id < filia_move_table.animations.size(); ++id) {
            const auto& source = filia_move_table.animations[id];
            const auto frame = std::find_if(source.begin(), source.end(), [&](const auto& item) { return item.sprites[1] == reference; });
            if (frame == source.end()) continue;
            const auto name = std::find(painwheel_assets.names.begin(), painwheel_assets.names.end(), filia_move_table.names[id]);
            if (name == painwheel_assets.names.end()) continue;
            const auto& clip = painwheel_assets.animations[name - painwheel_assets.names.begin()];
            if (clip.empty()) continue;
            const auto position = static_cast<std::size_t>(frame - source.begin());
            const auto index = std::min(position * clip.size() / source.size(), clip.size() - 1);
            unsigned pose = 2;
            for (std::size_t i = 0; i <= index; ++i) if (clip[i].sprites[1] > 1) pose = clip[i].sprites[1];
            return pose;
        }
        return 2;
    }
    unsigned robofortune_team_pose(unsigned reference) const {
        for (unsigned id = 0; id < filia_move_table.animations.size(); ++id) {
            const auto& source = filia_move_table.animations[id];
            const auto frame = std::find_if(source.begin(), source.end(), [&](const auto& item) { return item.sprites[1] == reference; });
            if (frame == source.end()) continue;
            const auto name = std::find(robofortune_assets.names.begin(), robofortune_assets.names.end(), filia_move_table.names[id]);
            if (name == robofortune_assets.names.end()) continue;
            const auto& clip = robofortune_assets.animations[name - robofortune_assets.names.begin()];
            if (clip.empty()) continue;
            const auto position = static_cast<std::size_t>(frame - source.begin());
            const auto index = std::min(position * clip.size() / source.size(), clip.size() - 1);
            unsigned pose = 2;
            for (std::size_t i = 0; i <= index; ++i) if (clip[i].sprites[1] > 1) pose = clip[i].sprites[1];
            return pose;
        }
        return 2;
    }

    unsigned msfortune_team_pose(unsigned reference) const {
        for (unsigned id = 0; id < filia_move_table.animations.size(); ++id) {
            const auto& source = filia_move_table.animations[id];
            const auto frame = std::find_if(source.begin(), source.end(), [&](const auto& item) { return item.sprites[1] == reference; });
            if (frame == source.end()) continue;
            const auto name = std::find(msfortune_assets.names.begin(), msfortune_assets.names.end(), filia_move_table.names[id]);
            if (name == msfortune_assets.names.end()) continue;
            const auto& clip = msfortune_assets.animations[name - msfortune_assets.names.begin()];
            if (clip.empty()) continue;
            const auto position = static_cast<std::size_t>(frame - source.begin());
            const auto index = std::min(position * clip.size() / source.size(), clip.size() - 1);
            unsigned pose = 2;
            for (std::size_t i = 0; i <= index; ++i) if (clip[i].sprites[1] > 1) pose = clip[i].sprites[1];
            return pose;
        }
        return 2;
    }

    unsigned cerebella_team_pose(unsigned reference) const {
        // Team travel carries a shared pose marker. Resolve it against the local
        // animation for the selected character before drawing the actor.
        constexpr std::pair<unsigned, unsigned> equivalents[] = {
            {439, 481}, {448, 488}, {450, 490}, {451, 491}, {452, 492}, {453, 493},
            {454, 494}, {455, 495}, {456, 496}, {457, 497}, {323, 365}, {324, 366},
            {447, 487}, {449, 489}, {295, 337}, {441, 482}, {442, 482},
        };
        for (const auto& [original, local] : equivalents) {
            const auto& source = filia_move_table.animations[original];
            const auto frame = std::find_if(source.begin(), source.end(), [&](const auto& item) {
                return item.sprites[1] == reference;
            });
            if (frame == source.end()) continue;
            const auto& clip = cerebella_assets.animations[local];
            const auto position = static_cast<std::size_t>(frame - source.begin());
            const auto index = original == 441 || original == 442 ? clip.size() - 1 :
                std::min(position * clip.size() / source.size(), clip.size() - 1);
            unsigned result = clip.front().sprites[1];
            for (std::size_t i = 0; i <= index; ++i) if (clip[i].sprites[1] > 1) result = clip[i].sprites[1];
            if (result <= 1 && (local == 493 || local == 494)) {
                // These clips retain the final pose of JumpToOffscreen_Rise.
                for (const auto& frame : cerebella_assets.animations[492])
                    if (frame.sprites[1] > 1) result = frame.sprites[1];
            }
            return result > 1 ? result : cerebella_assets.animations[20][1].sprites[1];
        }
        return 2;
    }

    const skullgirls::FoitMoveTable* filia_scripts() const {
        return filia_move_table.transitions.empty() ? nullptr : &filia_move_table;
    }

    void load_movement_frames() {
        if (!movement_frames.empty()) return;
        skullgirls::CharacterAssets assets(game_data, "filia");
        auto animations = assets.read_animations(true);
        auto& frames = animations.sprites;
        decltype(movement_frames) decoded(frames.size() + 1);
        decltype(jab_overlays) overlays;
        for (unsigned ref = 2; ref < decoded.size(); ++ref) {
            const auto& source = frames.at(ref - 1);
            if (!source.layers[0] || !source.layers[0]->used_tiles) continue;
            const auto& layer = *source.layers[0];
            const auto* art = assets.find_art(source.art_bank);
            if (!art) continue;
            auto& frame = decoded[ref];
            frame.width = layer.tile_columns * 16;
            frame.height = layer.tile_rows * 16;
            frame.origin_x = float(source.origin[0]) - float(source.primary_layer_offset[0]);
            frame.origin_y = float(source.origin[1]) - float(source.primary_layer_offset[1]);
            const auto& bank = *art;
            frame.pixels = skullgirls::decode_sprite_layer(layer, bank);
            // Jab recovery and rapid-fire smear art share the primary layer's canvas.
            if (source.layers[1]) {
                const auto& overlay = *source.layers[1];
                if (overlay.tile_columns != layer.tile_columns || overlay.tile_rows != layer.tile_rows)
                    continue;
                auto& effect = overlays[ref];
                effect.width = frame.width;
                effect.height = frame.height;
                effect.pixels = skullgirls::decode_sprite_layer(overlay, bank);
            }
        }
        movement_frames = std::move(decoded);
        jab_overlays = std::move(overlays);
        combat_metadata = std::move(frames);
        filia_move_table = std::move(animations);
    }
    int practice_camera_center = 0;
    float practice_camera_zoom = 0;
    int practice_camera_bottom = 0;
    std::uint64_t practice_stage_tick = 0;

    void update_practice_camera(bool snap) {
        const auto horizontal_bounds = [&](int side, float x, bool mirror) {
            const auto& frames = idle_frames[fighters[side].front()];
            int left = static_cast<int>(x), right = left;
            // Use the available idle art bounds until the combat character bounds are decoded.
            for (const auto& frame : frames) {
                if (!frame.width) continue;
                const float start = mirror ? frame.origin_x - frame.width : -frame.origin_x;
                left = std::min(left, static_cast<int>(std::floor(x + start)));
                right = std::max(right, static_cast<int>(std::ceil(x + start + frame.width)));
            }
            return std::pair{left, right};
        };
        const auto p1 = horizontal_bounds(0, practice_players[0].practice_x, !practice_facing_right(0));
        const auto p2 = horizontal_bounds(1, practice_players[1].practice_x, !practice_facing_right(1));
        // FUN_0057f150 computes the grounded horizontal zoom from the outer bounds.
        const int left = std::min(p1.first, p2.first) - 95;
        const int right = std::max(p1.second, p2.second) + 95;
        const float horizontal_zoom = std::clamp(((right - left) * 0.5f - 640.0f) / 160.0f, 0.0f, 1.0f);
        // FUN_0057f150 allows more zoom for height than for horizontal separation.
        const float vertical_zoom = std::clamp((practice_floor + std::max(practice_players[0].jump_height, practice_players[1].jump_height) + 288.0f - 720.0f) /
                                               180.0f, 0.0f, 2.5f);
        const float boundary_zoom = std::clamp(std::min((practice_width - 1280.0f) / 320.0f,
                                                       (practice_height - 720.0f) / 180.0f), 0.0f, 2.5f);
        const float zoom = std::min(std::max(horizontal_zoom, vertical_zoom), boundary_zoom);
        practice_camera_zoom = snap ? zoom : practice_camera_zoom + (zoom - practice_camera_zoom) / 2.5f;
        const int view_height = static_cast<int>(720.0f + 180.0f * practice_camera_zoom);
        int target_bottom = 0;
        for (const auto& player : practice_players) {
            if (player.jump_height <= 0) continue;
            const auto& frames = player.squigly ? squigly_frames : player.fukua ? fukua_frames : player.valentine ? valentine_frames : player.robofortune ? robofortune_frames : player.painwheel ? painwheel_frames : player.msfortune ? msfortune_frames :
                player.parasoul ? parasoul_frames : player.peacock ? peacock_frames :
                player.cerebella ? cerebella_frames : movement_frames;
            const auto pose = player.sprite();
            const float top = pose < frames.size() ? frames[pose].origin_y : 0.0f;
            target_bottom = std::max({target_bottom,
                static_cast<int>(practice_floor + player.jump_height) - static_cast<int>(view_height * 0.4f),
                static_cast<int>(practice_floor + player.jump_height + top) - view_height});
        }
        const int vertical_delta = target_bottom - practice_camera_bottom;
        practice_camera_bottom += snap || std::abs(vertical_delta) <= 4 ? vertical_delta :
            static_cast<int>(vertical_delta / 2.5f);
        practice_camera_bottom = std::clamp(practice_camera_bottom, 0,
            std::max(0, static_cast<int>(practice_height) - view_height));

        // FUN_0057f020 keeps the fighters inside the central tracking region and eases the center.
        const int half_width = static_cast<int>(640.0f + 160.0f * practice_camera_zoom);
        const int tracking = static_cast<int>(half_width * 0.45f);
        const int center = snap ? (left + right) / 2 :
            (std::min(left, practice_camera_center - tracking) +
             std::max(right, practice_camera_center + tracking)) / 2;
        const int delta = center - practice_camera_center;
        practice_camera_center += snap || std::abs(delta) <= 4 ? delta : static_cast<int>(delta / 2.5f);
        practice_camera_center = std::clamp(practice_camera_center, half_width,
                                            std::max(half_width, static_cast<int>(practice_width) - half_width));
    }

    TiledAnimation loading_animation;
    std::array<TiledAnimation, static_cast<std::size_t>(CombatEffect::count)> combat_effects;
    skullgirls::FoitMoveTable shared_effects;
    std::map<std::uint32_t, TiledAnimation> effect_textures;
    std::vector<skullgirls::FoitEffectPlayback> practice_effects;
    float super_flash_amount = 0;
    std::uint64_t next_effect_id = 1;

    static std::uint32_t effect_name_hash(std::string_view name) {
        std::uint32_t hash = 0x1505;
        for (unsigned char ch : name) hash = hash * 33u ^ unsigned(std::toupper(ch));
        return hash;
    }

    // Match effect texture hashes in move scripts to named sequences in the sprite archive.
    void load_practice_effects() {
        if (shared_effects.animations.empty())
            shared_effects = skullgirls::CharacterAssets(game_data, "hitspark").read_animations(true);
        skullgirls::SpriteArchive archive(game_data / "sprites.gfs");
        std::map<std::uint32_t, std::pair<std::string, std::string>> names;
        for (const auto& entry : archive.entries()) {
            constexpr std::string_view suffix = ".spr.msb";
            const auto& path = entry.name;
            if (path.size() < suffix.size() || path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
            const auto begin = path.find_last_of('/') + 1;
            const auto name = path.substr(begin, path.size() - begin - suffix.size());
            const auto bytes = archive.read(path);
            skullgirls::StageReader reader(bytes);
            if (reader.string() != "2.0") continue;
            reader.string();
            reader.skip(4);
            reader.string();
            if (reader.word(8) != 4) continue;
            const auto tiles = reader.word(8), frames = reader.word(8), sequences = reader.word(8);
            reader.skip(16);
            reader.skip(static_cast<std::size_t>(tiles * 4 + frames * 20));
            for (std::uint64_t i = 0; i < sequences; ++i) {
                const auto sequence = reader.string();
                reader.skip(16);
                names.emplace(effect_name_hash(sequence), std::pair{name, sequence});
            }
        }
        const auto load = [&](std::uint32_t hash) {
            if (!hash || effect_textures.count(hash)) return;
            const auto found = names.find(hash);
            if (found == names.end()) return;
            TiledAnimation animation;
            animation.load(archive, found->second.first, found->second.second);
            effect_textures.emplace(hash, std::move(animation));
        };
        const auto scan = [&](const skullgirls::FoitMoveTable& table) {
            std::uint32_t hash;
            std::memcpy(&hash, &table.initial_variables[0x50], sizeof(hash));
            load(hash);
            for (const auto& animation : table.animations) for (const auto& frame : animation) {
                const auto script = [&](const auto& expressions) {
                    for (const auto& expression : expressions)
                        if (expression.destination.flags == 1 && expression.destination.value == 0x50 &&
                            expression.opcode == 1 && expression.left.flags == 0x24) load(expression.left.value);
                };
                script(frame.script);
                for (const auto& variant : frame.variants) script(variant.script);
            }
        };
        for (const auto* table : std::array<const skullgirls::FoitMoveTable*, 11>{&fukua_assets,&squigly_assets,&valentine_assets,&robofortune_assets, &shared_effects, &filia_move_table, &cerebella_assets, &peacock_assets, &parasoul_assets, &msfortune_assets, &painwheel_assets}) {
            scan(*table);
            for (const auto& child : table->children) scan(child);
        }
    }

    static skullgirls::FoitVisualState& fighter_visuals(PracticeFighter& fighter) {
        return fighter.scripted() ? fighter.scripted()->visual : fighter.visual;
    }

    skullgirls::FoitScriptState effect_parent_state(const PracticeFighter& fighter) const {
        for (std::size_t side = 0; side < practice_players.size(); ++side)
            if (&fighter == &practice_players[side] && practice_presentation.actors[side].intro) {
                auto state = practice_presentation.actors[side].intro->state;
                state.variables[0xa0] = practice_presentation.actors[side].intro->x;
                state.variables[0xa1] = practice_presentation.actors[side].intro->y;
                return state;
            }
        auto state = fighter.scripted() ? fighter.scripted()->state : fighter.script_state;
        state.variables[0x53] = state.variables[0x53].value_or(1.0f);
        state.variables[0x0a] = fighter.hurt_group >= 0 ? 1.0f : 0.0f;
        state.variables[0x35] = float(fighter.blockstun);
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            if (&fighter == &practice_players[side]) {
                state.variables[0x40] = float(hud_stats[side].life);
                state.variables[0x1b] = 1.0f;
                state.variables[0x19] = float(side+1);
            } else if (practice_teams[side].assist && &fighter == &practice_teams[side].assist->fighter) {
                state.variables[0x40] = float(practice_teams[side].health[practice_teams[side].assist->actor.slot].life);
            }
        }
        state.variables[0xa0] = fighter.practice_x;
        state.variables[0xa1] = fighter.jump_height;
        return state;
    }

    bool practice_super_frozen() const {
        return std::any_of(practice_players.begin(), practice_players.end(), [](const auto& fighter) {
            return (fighter.scripted() ? fighter.scripted()->visual.freeze : fighter.visual.freeze) != 0;
        });
    }

    const char* practice_audio_bank(const skullgirls::FoitMoveTable& root) const {
        return &root == &squigly_assets ? "squigly" : &root == &fukua_assets ? "fukua" : &root == &valentine_assets ? "valentine" : &root == &robofortune_assets ? "robofortune" : &root == &filia_move_table ? "filia" : &root == &cerebella_assets ? "cerebella" :
            &root == &peacock_assets ? "peacock" : &root == &parasoul_assets ? "parasoul" :
            &root == &msfortune_assets ? "msfortune" : &root == &painwheel_assets ? "painwheel" : "hitspark";
    }

    void dispatch_practice_sound(const skullgirls::FoitMoveTable& root, std::uint32_t id,
                                const skullgirls::FoitScriptState& context, std::size_t side,
                                std::size_t slot, float x, std::uint64_t entity_id) {
        if (!audio || rollback_replaying) return;
        struct Target {
            std::uint64_t identity = 0, entity = 0;
            std::size_t side = 0, slot = 0;
            std::string bank;
            float x = 0;
            skullgirls::FoitScriptState state;
            skullgirls::FoitScriptState* live = nullptr;
        };
        const auto fighter_target = [&](std::size_t team, std::size_t member) -> std::optional<Target> {
            if (member >= fighters[team].size()) return std::nullopt;
            auto* fighter = &practice_players[team];
            if (member != practice_teams[team].active_slot) {
                auto& assist = practice_teams[team].assist;
                if (!assist || assist->actor.slot != member) return std::nullopt;
                fighter = &assist->fighter;
            }
            auto* state = fighter->scripted() ? &fighter->scripted()->state : &fighter->script_state;
            if (member == practice_teams[team].active_slot && practice_presentation.actors[team].intro)
                state = &practice_presentation.actors[team].intro->state;
            return Target{1 + team * 3 + member, 0, team, member, roster[fighters[team][member]],
                          fighter->practice_x, *state, state};
        };
        const auto effect_target = [&](skullgirls::FoitEffectPlayback& effect) {
            return Target{16 + effect.entity_id, effect.entity_id, effect.side, effect.palette_slot,
                          practice_audio_bank(*effect.root), effect.x, effect.state, &effect.state};
        };
        Target initial{entity_id ? 16 + entity_id : 1 + side * 3 + slot, entity_id,
                       side, slot, practice_audio_bank(root), x, context, nullptr};
        if (entity_id) {
            const auto found = std::find_if(practice_effects.begin(), practice_effects.end(),
                [&](const auto& effect) { return effect.entity_id == entity_id; });
            if (found != practice_effects.end()) initial.live = &found->state;
        } else if (auto target = fighter_target(side, slot)) initial.live = target->live;
        skullgirls::dispatch_sound_events(initial, id, [&](Target& target, std::uint32_t event) {
            const auto opposite = 1 - target.side;
            const auto enemy_slot = practice_teams[opposite].active_slot;
            const std::string self = target.bank;
            const std::string enemy = enemy_slot < fighters[opposite].size() ? roster[fighters[opposite][enemy_slot]] : "";
            // The source resolver hashes the entity asset name. A target
            // operand stays literal when no
            // captured target is represented by this simulation.
            skullgirls::AudioEventNames names{self, enemy,
                roster[fighters[target.side][practice_teams[target.side].active_slot]], ""};
            if (target.live && target.live->variables[0x98]) target.state.variables[0x98] = target.live->variables[0x98];
            const auto result = audio->event(target.bank, event, target.state,
                std::clamp((target.x - practice_width * 0.5f) / 640.f, -1.f, 1.f),
                unsigned(target.side * 3 + target.slot), names);
            if (result.script_value && target.live) target.live->variables[0x98] = *result.script_value;
            return result;
        }, [&](const Target& source, unsigned destination) {
            std::vector<Target> targets;
            if (destination == 4) {
                for (auto& effect : practice_effects)
                    if (effect.alive && effect.root && effect.side == source.side && effect.palette_slot == source.slot &&
                        effect.entity_id != source.entity && (!source.entity || effect.spawner_id == source.entity))
                        targets.push_back(effect_target(effect));
            } else {
                const auto team = destination == 2 ? 1 - source.side : source.side;
                const auto member = destination == 2 || destination == 3 ? practice_teams[team].active_slot : source.slot;
                if (auto target = fighter_target(team, member)) targets.push_back(std::move(*target));
            }
            return targets;
        });
    }

    void spawn_practice_visuals(skullgirls::FoitVisualState& visual, const skullgirls::FoitMoveTable& root,
                               std::size_t side, std::size_t slot, float x, float y, bool facing,
                               std::uint64_t instance, std::uint64_t spawner_id = 0) {
        auto sounds = std::exchange(visual.pending_sounds, {});
        for (const auto& sound : sounds)
            dispatch_practice_sound(root, sound.first, sound.second, side, slot, x, spawner_id);
        auto pending = std::exchange(visual.pending, {});
        for (const auto& request : pending) {
            if (practice_effects.size() >= 512) break;
            // The projectile already has combat playback and rendering.
            if (request.spawn && &root == &filia_move_table && request.spawn->child == 3) continue;
            const auto& source = request.spawn ? root : shared_effects;
            if (source.animations.empty()) continue;
            if (request.spawn && request.spawn->child >= source.children.size()) continue;
            const bool fortune_head = &source == &msfortune_assets && request.spawn && request.spawn->child == 0;
            if (fortune_head && std::any_of(practice_effects.begin(), practice_effects.end(), [&](const auto& effect) {
                return effect.persistent_owner && effect.side == side && effect.palette_slot == slot && effect.alive;
            })) continue;
            skullgirls::FoitEffectPlayback effect;
            effect.root = &source;
            effect.entity_id = next_effect_id++;
            effect.spawner_id = spawner_id;
            effect.floor_collision = &source == &squigly_assets || &source == &fukua_assets || &source == &valentine_assets || &source == &robofortune_assets || &source == &painwheel_assets || &source == &peacock_assets || &source == &parasoul_assets || &source == &msfortune_assets;
            effect.preserve_world_velocity_on_turn=&source==&squigly_assets;
            effect.generic = &source == &shared_effects;
            effect.authored_timeline = practice_presentation.phase == PracticePresentation::Phase::intro;
            effect.combat_entity = &source == &squigly_assets || &source == &fukua_assets || &source == &valentine_assets || &source == &robofortune_assets || &source == &painwheel_assets || &source == &peacock_assets || &source == &parasoul_assets;
            effect.initialize(request.spawn ? source.children[request.spawn->child] : source);
            effect.persistent_owner = fortune_head;
            // Shadows and linked stance children follow authored owner-category
            // links across move phases instead of the summoning instance.
            effect.independent_owner=&source==&fukua_assets || &source==&squigly_assets ||
                (&source==&robofortune_assets && request.spawn && (request.spawn->child==2 || request.spawn->child==57));
            effect.initialize_before_links=&source==&fukua_assets || &source==&squigly_assets || &source==&valentine_assets ||
                (&source==&robofortune_assets && request.spawn && request.spawn->child==57);
            if (fortune_head) effect.state.bits[20] = request.parent.bits[20];
            effect.side = side;
            effect.palette_slot = slot;
            effect.owner_instance = instance;
            effect.random_state ^= static_cast<std::uint32_t>(instance * 0x85ebca6bu +
                practice_stage_tick * 0xc2b2ae35u + practice_effects.size() + side);
            effect.facing_right = facing;
            effect.state.variables[0x38] = facing ? 1.0f : -1.0f;
            effect.state.variables[0x17] = facing ? 1.0f : -1.0f;
            effect.state.variables[0x19] = float(side + 1);
            effect.offset_x = (request.spawn ? float(request.spawn->x) : request.x) * (facing ? 1.0f : -1.0f);
            effect.offset_y = request.spawn ? -float(request.spawn->y) : request.y;
            // Script spawns occur before the spawner's physics integration.
            // Use that captured position rather than moving trails and muzzle
            // flashes one full velocity step ahead of their source.
            const float source_x = effect.combat_entity ? request.parent.variables[0xa0].value_or(x) : x;
            const float source_y = effect.combat_entity ? request.parent.variables[0xa1].value_or(y) : y;
            effect.x = source_x + effect.offset_x;
            effect.y = source_y + effect.offset_y;
            effect.attachment_position = std::array<float, 2>{source_x, source_y};
            effect.state.variables[0xa0] = effect.x;
            effect.state.variables[0xa1] = effect.y;
            effect.opponent = effect_parent_state(practice_players[1 - side]);
            const auto bounds = practice_effect_bounds(effect);
            effect.state.variables[0x0f] = bounds[0];
            effect.state.variables[0x10] = bounds[1];
            effect.state.variables[0x11] = practice_height;
            if(&source==&robofortune_assets)effect.state.variables[0x12]=0.0f;
            if (effect.combat_entity && spawner_id) effect.spawner = request.parent;
            if (request.spawn) {
                effect.apply(request.spawn->script, request.owner.value_or(request.parent));
                if (effect.authored_timeline) {
                    if (auto& intro = practice_presentation.actors[side].intro)
                        for (const auto& write : effect.parent_writes) skullgirls::apply_foit_assignment(write, intro->state);
                    effect.parent_writes.clear();
                } else if (effect.combat_entity) {
                    auto* owner = &practice_players[side];
                    if (practice_teams[side].assist && practice_teams[side].assist->actor.slot == slot)
                        owner = &practice_teams[side].assist->fighter;
                    auto& state = owner->scripted() ? owner->scripted()->state : owner->script_state;
                    for (const auto& write : effect.parent_writes) skullgirls::apply_foit_assignment(write, state);
                    effect.parent_writes.clear();
                }
            }
            else {
                effect.state.variables[0xa8] = float(request.effect);
                effect.state.variables[0x98] = request.parameter;
                effect.state.variables[0xa4] = effect.state.variables[0xa5] = effect.state.variables[0xa6] = 1.0f;
                effect.state.variables[0x8f] = 8.0f;
                if (request.attached) effect.state.variables[0x39] = 1.0f;
            }
            if (effect.spawner_id) {
                auto source_entity = std::find_if(practice_effects.begin(), practice_effects.end(),
                    [&](const auto& value) { return value.entity_id == effect.spawner_id; });
                if (source_entity != practice_effects.end())
                    for (const auto& write : effect.spawner_writes) skullgirls::apply_foit_assignment(write, source_entity->state);
                effect.spawner_writes.clear();
            }
            practice_effects.push_back(std::move(effect));
        }
    }

    void collect_practice_visuals(PracticeFighter& fighter, std::size_t side, std::size_t slot) {
        const auto& root = fighter.squigly ? static_cast<const skullgirls::FoitMoveTable&>(squigly_assets) : fighter.fukua ? static_cast<const skullgirls::FoitMoveTable&>(fukua_assets) : fighter.valentine ? static_cast<const skullgirls::FoitMoveTable&>(valentine_assets) : fighter.robofortune ? static_cast<const skullgirls::FoitMoveTable&>(robofortune_assets) : fighter.painwheel ? static_cast<const skullgirls::FoitMoveTable&>(painwheel_assets) : fighter.msfortune ? static_cast<const skullgirls::FoitMoveTable&>(msfortune_assets) : fighter.parasoul ? static_cast<const skullgirls::FoitMoveTable&>(parasoul_assets) : fighter.peacock ? static_cast<const skullgirls::FoitMoveTable&>(peacock_assets) :
            fighter.cerebella ? static_cast<const skullgirls::FoitMoveTable&>(cerebella_assets) : filia_move_table;
        if (fighter.msfortune && !practice_presentation.actors[side].intro) {
            auto& actor = *fighter.msfortune;
            const bool queued = std::any_of(actor.visual.pending.begin(), actor.visual.pending.end(), [](const auto& request) {
                return request.spawn && request.spawn->child == 0;
            });
            const bool spawned = std::any_of(practice_effects.begin(), practice_effects.end(), [&](const auto& effect) {
                return effect.persistent_owner && effect.side == side && effect.palette_slot == slot && effect.alive;
            });
            // Tag-ins and assists can enter without playing CreateHead.
            if (!queued && !spawned) {
                for (const auto& variant : msfortune_assets.animations[20].front().variants)
                    for (const auto& spawn : variant.spawns)
                        actor.visual.pending.push_back({0, &spawn, 0, 0, 1, false, actor.state});
            }
        }
        auto& timeline = practice_presentation.actors[side].intro;
        auto& visual = timeline ? timeline->visual : fighter_visuals(fighter);
        spawn_practice_visuals(visual, root, side, slot, timeline ? timeline->x : fighter.practice_x, timeline ? timeline->y : fighter.jump_height,
            timeline ? timeline->facing_right : !fighter.mirror(practice_facing_right(side)), fighter.attack_instance);
    }

    void advance_practice_effects(bool frozen) {
        for (std::size_t i = 0; i < practice_effects.size() && i < 512; ++i) {
            auto& effect = practice_effects[i];
            auto& team = practice_teams[effect.side];
            auto* owner = &practice_players[effect.side];
            bool on_screen = team.active_slot == effect.palette_slot;
            if (team.assist && team.assist->actor.slot == effect.palette_slot &&
                team.assist->phase == PracticeAssist::Phase::acting) {
                owner = &team.assist->fighter;
                on_screen = true;
            }
            // A held item remains independent of the summoning animation.
            if (!frozen && effect.root == &peacock_assets && effect.table == &peacock_assets.children[32]) {
                if (!effect.combat_entity) effect.state.variables[0x81] = std::max(0.0f, effect.value(0x81) - 1.0f);
                const auto strength = static_cast<unsigned>(effect.value(0xa8));
                if (effect.move == 1 && strength >= 1 && strength <= 3 &&
                    !(owner->held_attack_buttons & (1u << (strength - 1)))) effect.start(2);
            }
            auto parent = effect_parent_state(*owner);
            if (effect.combat_entity && owner->scripted()) {
                effect.inputs = owner->scripted()->inputs;
                effect.input_facing = owner->scripted()->facing_right;
            }
            if (effect.spawner_id) {
                const auto source = std::find_if(practice_effects.begin(), practice_effects.end(),
                    [&](const auto& value) { return value.entity_id == effect.spawner_id; });
                if (source != practice_effects.end()) {
                    effect.spawner = source->state;
                    effect.spawner->variables[0xa0] = source->x;
                    effect.spawner->variables[0xa1] = source->y;
                }
            }
            if (effect.persistent_owner && !effect.authored_timeline) {
                if (frozen) continue;
                if (!on_screen) effect.alive = false;
                if (effect.alive && owner->msfortune) {
                    const auto& fortune = *owner->msfortune;
                    effect.inputs = fortune.inputs;
                    effect.input_facing = fortune.facing_right;
                    effect.state.variables[0x17] = effect.x < practice_players[1 - effect.side].practice_x ? 1.0f : -1.0f;
                    if (!effect.hit_stop) {
                        if (effect.move != 25) effect.state.variables[0x6a] = std::max(0.0f, effect.value(0x6a) - 1);
                        effect.state.variables[0x35] = std::max(0.0f, effect.value(0x35) - 1);
                        // Head Spike strikes Fortune's own floating head on the kick's active record.
                        if (effect.move == 4 && fortune.move == 167 && fortune.hit) effect.start(5);
                    }
                    const bool calling = fortune.calling_head() && owner->hurt_group < 0 && !owner->blockstun &&
                        !owner->fenrir_captured && owner->throw_phase != PracticeFighter::ThrowPhase::grabbed &&
                        !effect.captured_target && effect.y == 0;
                    parent.bits[21] = calling;
                    if (!effect.hit_stop && (effect.move == 29 || effect.move == 30)) {
                        if (!calling) {
                            effect.start(31);
                            effect.state.variables[0x9e] = 0.0f;
                        } else {
                            // The run animation aims for the authored pickup offset.
                            const float target = std::clamp(owner->practice_x + (fortune.facing_right ? 75.0f : -75.0f),
                                0.0f, practice_width);
                            if (std::abs(target - effect.x) <= 30.0f) effect.start(32);
                            else {
                                effect.facing_right = target > effect.x;
                                effect.state.variables[0x38] = effect.facing_right ? 1.0f : -1.0f;
                            }
                        }
                    }
                }
            }
            effect.opponent = effect_parent_state(practice_players[1 - effect.side]);
            const auto bounds = practice_effect_bounds(effect);
            effect.state.variables[0x0f] = bounds[0];
            effect.state.variables[0x10] = bounds[1];
            // FUN_0049a7f0 exposes signed screen exits relative to facing.
            // The playfield bounds provide the Training viewport's outer limit.
            if (effect.combat_entity) {
                const float horizontal = effect.x < -128 ? -2.0f : effect.x > practice_width + 128 ? 2.0f : 0.0f;
                effect.state.variables[0x0d] = horizontal * (effect.facing_right ? 1 : -1);
                effect.state.variables[0x0e] = effect.y > practice_height + 128 ? 2.0f : 0.0f;
            }
            if (!on_screen) {
                parent.variables[0xa0] = effect.x - effect.offset_x;
                parent.variables[0xa1] = effect.y - effect.offset_y;
            }
            effect.advance(parent, on_screen && (practice_presentation.phase == PracticePresentation::Phase::intro ||
                (owner->attack_instance == effect.owner_instance && owner->active_normal >= 0)), frozen);
            // Mine landing resets velocity before its authored position offset.
            // Apply the Training floor even when there is no downward velocity.
            if(effect.root==&robofortune_assets && effect.table==&robofortune_assets.children[2] &&
               effect.move>=10 && effect.move<=12 && effect.y<0) {
                effect.y=0;effect.state.variables[0xa1]=0.0f;
            }
            if (effect.persistent_owner && effect.alive && !effect.authored_timeline && effect.move > 1) {
                const auto bounded_x = std::clamp(effect.x, 0.0f, practice_width);
                effect.state.variables[0x0d] = effect.x == bounded_x ? 0.0f : 1.0f;
                if (effect.x != bounded_x) {
                    effect.x = bounded_x;
                    effect.state.variables[0x9e] = 0.0f;
                }
                effect.state.variables[0xa0] = effect.x;
                effect.state.variables[0xa1] = effect.y;
            }
            if (effect.captured_target && !frozen) {
                auto& victim = practice_players[1 - effect.side];
                if (effect.capture_damage) record_practice_damage(effect.side, std::exchange(effect.capture_damage, 0u));
                if (!effect.alive || effect.capture_state.variables[0xb0].value_or(0) != 0) {
                    constexpr skullgirls::NormalHit release{0,20,0,3};
                    victim.take_hit(release, effect.capture_state.variables[0x38].value_or(-1) > 0, -1,
                        {skullgirls::HeavyReaction::knockback, std::abs(effect.capture_state.variables[0x9e].value_or(0)),
                            effect.capture_state.variables[0x9f].value_or(0)});
                    effect.captured_target = false;
                } else {
                    victim.throw_phase = PracticeFighter::ThrowPhase::grabbed;
                    const auto& poses = victim.squigly ? squigly_assets.animations[437] : victim.fukua ? fukua_assets.animations[340] : victim.valentine ? valentine_assets.animations[395] : victim.robofortune ? robofortune_assets.animations[357] : victim.painwheel ? painwheel_assets.animations[387] : victim.msfortune ? msfortune_assets.animations[358] : victim.parasoul ? parasoul_assets.animations[369] : victim.peacock ? peacock_assets.animations[359] :
                        victim.cerebella ? cerebella_assets.animations[378] : filia_move_table.animations[336];
                    const auto pose = static_cast<unsigned>(std::max(1.0f, effect.value(0x6f)));
                    const auto sprite = poses[std::min<std::size_t>(pose-1,poses.size()-1)].sprites[1];
                    if (sprite>1) { victim.movement_sprite=sprite; if(victim.scripted())victim.scripted()->pose=sprite; }
                    victim.practice_x=std::clamp(effect.x+effect.value(0x70)*(effect.facing_right?1:-1),0.0f,practice_width);
                    victim.jump_height=std::max(0.0f,effect.y+effect.value(0x71));victim.jump_velocity=0;
                    if(effect.move==5 && (effect.x<95 || effect.x>practice_width-95)) effect.start(6);
                }
            }
            auto& intro = practice_presentation.actors[effect.side].intro;
            auto& owner_state = intro ? intro->state : owner->scripted() ? owner->scripted()->state : owner->script_state;
            if (!effect.persistent_owner || on_screen)
                for (const auto& expression : effect.parent_writes) skullgirls::apply_foit_assignment(expression, owner_state);
            effect.parent_writes.clear();
            if (effect.spawner_id) {
                auto source = std::find_if(practice_effects.begin(), practice_effects.end(),
                    [&](const auto& value) { return value.entity_id == effect.spawner_id; });
                if (source != practice_effects.end())
                    for (const auto& write : effect.spawner_writes) skullgirls::apply_foit_assignment(write, source->state);
                effect.spawner_writes.clear();
            }
            auto pending = std::exchange(effect.visual, {});
            const auto* root = effect.root;
            const auto side = effect.side, slot = effect.palette_slot;
            const auto x = effect.x, y = effect.y;
            const auto facing = effect.facing_right;
            const auto instance = effect.owner_instance;
            const auto entity_id = effect.entity_id;
            // Spawning may reallocate the vector, so release the reference first.
            spawn_practice_visuals(pending, *root, side, slot, x, y, facing, instance, entity_id);
        }
        practice_effects.erase(std::remove_if(practice_effects.begin(), practice_effects.end(), [&](const auto& effect) {
            return !effect.alive || effect.x < -1000 || effect.x > practice_width + 1000 ||
                effect.y < -1500 || effect.y > practice_height + 2500;
        }), practice_effects.end());
    }

    bool robo_beam_part(const skullgirls::FoitEffectPlayback& effect,std::initializer_list<unsigned> children) const {
        return effect.root==&robofortune_assets && std::any_of(children.begin(),children.end(),
            [&](unsigned child){return effect.table==&robofortune_assets.children[child];});
    }
    float robo_beam_reach(const skullgirls::FoitEffectPlayback& effect) const {
        return std::max(0.0f,effect.facing_right?practice_width-effect.x:effect.x);
    }
    std::optional<Gdiplus::PointF> projectile_overlap(const skullgirls::FoitEffectPlayback& effect,
                                                     const PracticeFighter& target,bool facing) const {
        if(robo_beam_part(effect,{34,37,39,53})) {
            auto boxes=robofortune_assets.sprites[effect.pose-1].box_groups[0];
            for(auto& box:boxes)box[3]=std::max(box[3],int(std::ceil(robo_beam_reach(effect))));
            return practice_box_contact(boxes,effect.x,effect.y,effect.facing_right,
                practice_metadata(target)[target.sprite()-1].box_groups[1],target.practice_x,target.jump_height,facing);
        }
        return practice_attack_overlap(effect.pose,effect.x,effect.y,effect.facing_right,target,facing,false,
            effect.root==&peacock_assets,effect.root==&parasoul_assets,effect.root==&msfortune_assets,
            effect.root==&painwheel_assets,effect.root==&robofortune_assets,effect.root==&valentine_assets,effect.root==&squigly_assets,effect.root==&fukua_assets);
    }

    void resolve_scripted_projectiles() {
        if (practice_super_frozen()) return;
        // Set tears react to the owner's flame hitboxes as well as Quake's toggle.
        for(auto& tear:practice_effects) {
            if(tear.root!=&parasoul_assets || tear.table!=&parasoul_assets.children[34] || !tear.alive ||
                (tear.move!=2 && tear.move!=9))continue;
            PracticeFighter target;target.practice_x=tear.x;target.jump_height=tear.y;
            target.parasoul.emplace().pose=tear.pose;
            const auto& owner=practice_players[tear.side];
            const skullgirls::FoitHit* source=nullptr;
            if(owner.parasoul && owner.parasoul->hit && !owner.hit_stop && owner.hurt_group<0 && !owner.blockstun &&
                practice_attack_overlap(owner.parasoul->pose,owner.practice_x,owner.jump_height,
                    owner.parasoul->facing_right,target,tear.facing_right,false,false,true))source=owner.parasoul->hit;
            if(!source)for(const auto& flame:practice_effects) {
                if(&flame==&tear || flame.root!=&parasoul_assets || flame.side!=tear.side || !flame.alive || !flame.hit || flame.hit_stop)continue;
                if(practice_attack_overlap(flame.pose,flame.x,flame.y,flame.facing_right,target,tear.facing_right,false,false,true)) {source=flame.hit;break;}
            }
            if(source)tear.start(tear.move==9 ? (source->strength<=1?10u:11u) :
                source->strength<=1?3u:source->strength==2?4u:5u);
        }
        // Fukua's light shadow uses a native zero-damage grab descriptor.
        // Enter its authored success/recovery records after a grounded capture.
        for(auto& effect:practice_effects) {
            if(effect.root!=&fukua_assets || effect.table!=&fukua_assets.children[10] ||
               !effect.alive || effect.move!=5 || !effect.hit || effect.hit_resolved || effect.captured_target)continue;
            auto& target=practice_players[1-effect.side];
            if(target.jump_height>0 || target.jump_velocity>0 || target.hurt_group>=0 || target.blockstun ||
               target.fenrir_captured || target.throw_phase==PracticeFighter::ThrowPhase::grabbed ||
               target.burst_protected() || practice_point_snapped(1-effect.side))continue;
            const auto impact=projectile_overlap(effect,target,!target.mirror(practice_facing_right(1-effect.side)));
            if(!impact)continue;
            effect.report_contact(false);effect.captured_target=true;effect.state.variables[0x09]=1.0f;
            effect.capture_state=effect_parent_state(target);effect.capture_state.variables[0xb0]=0.0f;
            effect.start(6);target.throw_phase=PracticeFighter::ThrowPhase::grabbed;
            target.active_normal=target.queued_followup=-1;target.normal_hit=nullptr;target.hit_stop=0;
            spawn_combat_spark(effect.side,CombatEffect::grab,*impact);
        }
        // Silver Chord's tail head authors a capture probe (0x72), not a
        // FoitHit. Its Stop/End records own damage, retraction and release.
        for(auto& effect:practice_effects) {
            if(effect.root!=&squigly_assets || effect.table!=&squigly_assets.children[51] ||
               !effect.alive || effect.move!=0 || effect.value(0x72)!=2 || effect.captured_target || effect.pose<=1)continue;
            auto& target=practice_players[1-effect.side];
            if(target.fenrir_captured || target.throw_phase==PracticeFighter::ThrowPhase::grabbed ||
               target.burst_protected() || practice_point_snapped(1-effect.side))continue;
            const auto impact=projectile_overlap(effect,target,!target.mirror(practice_facing_right(1-effect.side)));
            if(!impact)continue;
            const skullgirls::NormalHit probe{0,0,0,3};
            const bool blocked=practice_guard(effect.side,probe,effect.x>=target.practice_x);
            effect.report_contact(blocked);
            if(blocked) {spawn_combat_spark(effect.side,CombatEffect::block,*impact);continue;}
            effect.captured_target=true;effect.state.variables[0x09]=1.0f;
            effect.capture_state=effect_parent_state(target);effect.capture_state.variables[0xb0]=0.0f;
            target.throw_phase=PracticeFighter::ThrowPhase::grabbed;target.active_normal=target.queued_followup=-1;
            target.normal_hit=nullptr;target.hit_stop=0;spawn_combat_spark(effect.side,CombatEffect::grab,*impact);
        }
        for (auto& effect : practice_effects) {
            if ((effect.root != &fukua_assets && effect.root != &squigly_assets && effect.root != &valentine_assets && effect.root != &robofortune_assets && effect.root != &painwheel_assets && effect.root != &peacock_assets && effect.root != &parasoul_assets && effect.root != &msfortune_assets) || !effect.alive || !effect.hit ||
                effect.hit_resolved || effect.hit_stop) continue;
            bool intercepted=false;
            for(auto& shield:practice_effects) {
                if(shield.root!=&parasoul_assets || shield.table!=&parasoul_assets.children[44] ||
                    shield.side==effect.side || !shield.alive || shield.move>3)continue;
                PracticeFighter guard;guard.practice_x=shield.x;guard.jump_height=shield.y;
                guard.parasoul.emplace().pose=shield.pose;
                if(const auto impact=projectile_overlap(effect,guard,shield.facing_right)) {
                    effect.report_contact(true);
                    shield.state.variables[0x06]=1.0f;
                    spawn_combat_spark(effect.side,CombatEffect::block,*impact);
                    intercepted=true;break;
                }
            }
            if(intercepted)continue;
            auto& target = practice_players[1 - effect.side];
            if (!practice_slot_playable(1 - effect.side, practice_teams[1 - effect.side].active_slot) ||
                practice_point_snapped(1 - effect.side) || target.burst_protected() || target.ground_tech_direction ||
                target.fenrir_captured || target.throw_phase == PracticeFighter::ThrowPhase::grabbed) continue;
            const auto impact = projectile_overlap(effect,target,!target.mirror(practice_facing_right(1-effect.side)));
            if (!impact) continue;
            const auto& hit = *effect.hit;
            const bool head_bite=effect.root==&msfortune_assets && effect.persistent_owner && (effect.move==51 || effect.move==52);
            if ((effect.root == &parasoul_assets && effect.table == &parasoul_assets.children[45]) || head_bite) {
                if (effect.captured_target || target.jump_height>0 || target.hurt_group>=0 || target.blockstun) continue;
                effect.captured_target = true;
                for(auto& slot:effect.capture_state.variables)slot=0.0f;
                effect.capture_state.variables[0x38]=effect.facing_right?-1.0f:1.0f;
                effect.capture_state.variables[0x53]=1.0f;
                effect.report_contact(false);
                effect.start(head_bite?54u:4u);
                target.throw_phase=PracticeFighter::ThrowPhase::grabbed;
                target.active_normal=target.queued_followup=-1;target.normal_hit=nullptr;target.hit_stop=0;
                spawn_combat_spark(effect.side,CombatEffect::grab,*impact);
                continue;
            }
            const skullgirls::NormalHit contact{0, hit.stun, hit.stop, hit.height & 3u};
            const bool blocked = contact.height_mask && practice_guard(effect.side, contact, effect.x >= target.practice_x);
            effect.report_contact(blocked);
            effect.hit_stop = hit.stop;
            if (blocked) spawn_combat_spark(effect.side, CombatEffect::block, *impact);
            else {
                target.take_hit(contact, effect.x >= target.practice_x, -1,
                    practice_hit_reaction(effect.side, cerebella_hit_reaction(hit, target)));
                record_practice_damage(effect.side, hit.damage);
                spawn_combat_spark(effect.side, hit.strength <= 1 ? CombatEffect::light :
                    hit.strength == 2 ? CombatEffect::medium : CombatEffect::heavy, *impact);
            }
        }
    }

    void paint_practice_effects(Gdiplus::Graphics& dc, float camera, float floor, bool behind) const {
        for (const auto& effect : practice_effects) {
            if (!effect.alive || ((unsigned(effect.value(0x8f)) & 4u) != 0) != behind) continue;
            const float sx = effect.value(0xa4), sy = effect.value(0xa5);
            if (sx == 0 || sy == 0 || effect.value(0xa6) <= 0) continue;
            const auto texture = effect_textures.find(effect.texture());
            if (texture != effect_textures.end() && effect.pose == 1) {
#ifdef _WIN32
                if(sprite_renderer_active) {
                    try {
                        texture->second.draw_frame(dc,unsigned(std::max(0.0f,effect.value(0x51))),
                            effect.x-camera,floor-effect.y,sx,sy,!effect.facing_right,
                            effect.value(0xa3),effect.value(0xa6),sprite_renderer.get());
                        continue;
                    } catch(const std::exception&) {fail_sprite_renderer(dc);}
                }
#endif
                texture->second.draw_frame(dc, unsigned(std::max(0.0f, effect.value(0x51))),
                    effect.x - camera, floor - effect.y, sx, sy, !effect.facing_right,
                    effect.value(0xa3), effect.value(0xa6));
            } else if (!effect.generic && effect.pose > 1) {
                const auto saved = dc.Save();
                dc.TranslateTransform(effect.x - camera, floor - effect.y);
                dc.RotateTransform(effect.value(0xa3));
                dc.ScaleTransform(sx, sy);
                draw_practice_frame(dc, effect.side, effect.pose, 0, 0, !effect.facing_right, false,
                    effect.palette_slot, effect.value(0xa6),{},
                    robo_beam_part(effect,{33,36,49,50})?robo_beam_reach(effect)/std::abs(sx):0.0f);
                dc.Restore(saved);
            }
        }
    }

    void spawn_combat_spark(std::size_t side, CombatEffect effect, Gdiplus::PointF position, float scale = 1.0f) {
        hud_stats[side].sparks.push_back({effect, position, 0, scale});
    }

    static Gdiplus::PointF contact_center(const skullgirls::WorldBox& hit, const skullgirls::WorldBox& hurt) {
        return {float(std::max(hit[2], hurt[2]) + std::min(hit[3], hurt[3])) * 0.5f,
                float(std::max(hit[1], hurt[1]) + std::min(hit[0], hurt[0])) * 0.5f};
    }
    std::chrono::steady_clock::time_point loading_started{};
    std::future<void> practice_load;
    bool practice_paused = false;
    enum class PauseOption { resume, help, options, commands, leaderboards, achievements, exit_training };
    std::size_t pause_cursor = 0;
    std::size_t pause_training_cursor = 0;
    std::optional<PauseOption> pause_page;
    enum class CommandPage { menu, characters, moves };
    CommandPage command_page = CommandPage::menu;
    struct CommandRow { std::wstring name, input; };
    struct CommandList { std::string key; std::wstring title; std::vector<CommandRow> moves; };
    std::vector<CommandList> command_lists;
    std::size_t command_menu_cursor = 0, command_character_cursor = 0, command_move_cursor = 0, command_list = 0;
    static constexpr std::array pause_options{L"Resume Game", L"Help & Options...", L"Training Options...",
        L"Command Lists...", L"Leaderboards", L"Achievements...", L"Exit Game"};
    static constexpr std::array pause_training_options{L"Reset Position", L"Change Stage", L"Back"};

    std::size_t pause_training_count() const {
        return pause_training_options.size() + (training_options.empty() ? 0 : training_options.size() - 1);
    }

    std::size_t training_value(std::string_view keyword, std::size_t fallback = 0) const {
        // Network matches use synchronized human inputs, not Training cheats.
        if(network_mode) {
            if(keyword=="OPT_STR_DEATHALLOWED")return 1;
            if(keyword=="OPT_STR_LIFEBAR_SETTING")return 1;
            return fallback;
        }
        const auto option = std::find_if(training_options.begin(), training_options.end(),
            [&](const auto& setting) { return setting.keyword == keyword; });
        return option == training_options.end() ? fallback : option->choice;
    }

    unsigned training_life() const {
        return std::max(1u, static_cast<unsigned>(training_value("OPT_STR_LIFEBAR_LEVEL", 100)) * 155);
    }

    skullgirls::HeavyReaction practice_hit_reaction(std::size_t attacker,
                                                     skullgirls::HeavyReaction fallback = {}) const {
        using Reaction = skullgirls::HeavyReaction;
        if (attacker != 0) return fallback;
        switch (training_value("PRACTICE_HIT_REACTION")) {
        case 1: return {Reaction::sliding, 20, 8};
        case 2: return {Reaction::ceiling_bounce, 8, 38};
        case 3: return {Reaction::crumple, 0, 0};
        case 4: return {Reaction::stagger, 0, 0};
        case 5: return {Reaction::freeze, 0, 0};
        default: return fallback;
        }
    }

    unsigned practice_speed() const {
        constexpr unsigned speeds[] = {100, 75, 50, 25, 10};
        return speeds[training_value("OPT_STR_SLOWMOTION")];
    }

    static skullgirls::HeavyReaction cerebella_hit_reaction(const skullgirls::FoitHit& hit,
                                                            const PracticeFighter& defender) {
        using Reaction = skullgirls::HeavyReaction;
        const bool airborne = defender.jump_height > 0 || defender.jump_velocity > 0;
        auto state = skullgirls::CerebellaPlayback::defaults();
        state.variables[0x07] = defender.hurt_group >= 0 ? 1.0f : 0.0f;
        state.variables[0x35] = float(hit.stun);
        state.variables[0x3b] = airborne ? 2.0f : 3.0f;
        state.variables[0x9f] = defender.jump_velocity;
        const auto& script = hit.scripts[airborne ? 0 : 1];
        skullgirls::apply_foit_script(script, state, skullgirls::CerebellaPlayback::defaults());
        const auto value = [&](unsigned slot) { return state.variables[slot].value_or(0.0f); };
        auto type = value(0x3b) == 2 || airborne ? Reaction::air_launch : Reaction::none;
        for (const auto& expression : script) {
            // FrontFall's hashed move name forces the sweep reaction.
            if (!expression.comparison && expression.destination.flags == 1 && expression.destination.value == 0x5b &&
                expression.left.flags == 0x24 && expression.left.value == 0x3c080ff9) type = Reaction::sweep;
        }
        return {type, std::abs(value(0x9e)), value(0x9f)};
    }

    skullgirls::HeavyReaction blockbuster_hit_reaction(std::size_t side, const skullgirls::FoitHit& hit) {
        using R=skullgirls::HeavyReaction;
        auto& attacker=practice_players[side];
        const auto& defender=practice_players[1-side];
        auto state=effect_parent_state(defender);
        const auto source=effect_parent_state(attacker);
        const bool airborne=defender.jump_height>0 || defender.jump_velocity>0;
        state.variables[0x07]=defender.hurt_group>=0?1.0f:0.0f;
        state.variables[0x1b]=1.0f;
        state.variables[0x18]=defender.practice_x<attacker.practice_x?-1.0f:1.0f;
        state.variables[0x35]=float(hit.stun);
        state.variables[0x36]=defender.robofortune?skullgirls::CerebellaPlayback::gravity:skullgirls::filia_gravity;
        state.variables[0x38]=attacker.practice_x>=defender.practice_x?1.0f:-1.0f;
        state.variables[0x3b]=airborne?2.0f:3.0f;
        state.variables[0x9f]=defender.jump_velocity;
        state.variables[0x5d]=0.0f;
        for (auto expression:hit.scripts[airborne?0:1]) {
            for (auto* operand:{&expression.left,&expression.right}) {
                if (operand->flags!=0x81 || operand->value>=source.variables.size()) continue;
                const float value=source.variables[operand->value].value_or(0.0f);
                operand->flags=0;std::memcpy(&operand->value,&value,sizeof(value));
            }
            if (expression.destination.flags==0x81 || expression.destination.flags==0xa0) {
                expression.destination.flags=expression.destination.flags==0x81?1:0x20;
                for (auto* operand:{&expression.left,&expression.right}) {
                    if (operand->flags!=1 || operand->value>=state.variables.size()) continue;
                    const float number=state.variables[operand->value].value_or(0.0f);
                    operand->flags=0;std::memcpy(&operand->value,&number,sizeof(number));
                }
                skullgirls::apply_foit_assignment(expression,attacker.blockbuster_playback->state);
            } else skullgirls::apply_foit_assignment(expression,state);
        }
        const auto value=[&](unsigned slot){return state.variables[slot].value_or(0.0f);};
        const auto reaction=unsigned(std::max(0.0f,value(0x5d)));
        const auto type=reaction==8?R::wall_bounce:reaction==7?R::ground_bounce:reaction==3?R::launch:
            reaction==2?R::sweep:airborne||value(0x3b)==2?R::air_launch:R::none;
        return {type,value(0x9e),value(0x9f)};
    }

    ~Menu() { if (practice_load.valid()) practice_load.wait(); }

    // Load the fighters and stage in the background while the window draws the loading animation.
    std::string practice_music_intro, practice_music_loop;
    void start_practice() {
        if (audio) {
            audio->stop_combat(); audio->prepare_character("hitspark");
            for (const auto& team : fighters) for (auto id : team) audio->prepare_character(roster[id]);
        }
        confirm_stage_selection();
        loading_started = std::chrono::steady_clock::now();
        practice_scene.reset();
        practice_background.reset();
        practice_load = std::async(std::launch::async, [this] { load_practice(); });
    }

    void load_practice() {
        load_movement_frames();
        const auto selected = [&](std::string_view name) {
            return std::any_of(fighters.begin(), fighters.end(), [name](const auto& team) {
                return std::any_of(team.begin(), team.end(), [name](int id) { return name == roster[id]; });
            });
        };
        if (selected("cerebella")) load_cerebella();
        if (selected("peacock")) load_peacock();
        if (selected("painwheel")) load_painwheel();
        if (selected("squigly")) load_squigly();
        if (selected("fukua")) load_fukua();
        if (selected("valentine")) load_valentine();
        if (selected("robofortune")) load_robofortune();
        if (selected("parasoul")) load_parasoul();
        if (selected("msfortune")) load_msfortune();
        load_practice_effects();
        skullgirls::SpriteArchive hud_assets(game_data / "ui.gfs");
        for (const auto& team : fighters) for (const auto id : team) {
            if (super_portraits[id]) continue;
            auto portrait = std::make_unique<Texture>();
            portrait->load(hud_assets.read("ui/true_color_hud/super_" + std::string(std::string_view(roster[id])=="bossmarie"?"bloodymarie":roster[id]) + "01.dds"));
            super_portraits[id] = std::move(portrait);
        }
        for (const auto& team : fighters) for (const auto id : team) prepare_super_portrait(id);
        prepare_combat_art();
        if (fighters != staged_fighters || locked_colors != staged_colors) {
            prepare_practice_bitmaps();
            prepare_hud_portraits();
            staged_fighters = fighters;
            staged_colors = locked_colors;
        }
        skullgirls::SpriteArchive stages_archive(game_data / "stages.gfs");
        const auto& level = current_stage().level;
        const auto config = stages_archive.read("stages/" + level + ".lvl");
        practice_music_intro.clear(); practice_music_loop.clear();
        std::istringstream stream(std::string(config.begin(), config.end()));
        std::string background_asset, texture_subpath, world = level;
        std::map<std::string, std::string> replacements;
        unsigned replacement_index = 0;
        bool replace_ignore_first = false;
        float width = 0, height = 0, floor = 0;
        decltype(practice_starts) starts{};
        float fov = 66, near_plane = 200, far_plane = 24000;
        for (std::string line; std::getline(stream, line);) {
            const auto comment = line.find('#');
            if (comment != std::string::npos) line.resize(comment);
            std::istringstream row(line);
            std::string key;
            row >> key;
            if (key == "Music_Intro") row >> practice_music_intro;
            else if (key == "Music_Loop") row >> practice_music_loop;
            else if (key == "StageSize:") row >> width >> height;
            else if (key == "BottomClearance:") row >> floor;
            else if (key == "Start1:") row >> starts[0];
            else if (key == "Start2:") row >> starts[1];
            else if (key == "2D") row >> background_asset;
            else if (key == "OverrideWorldName") row >> world;
            else if (key == "TextureSubpath") row >> texture_subpath;
            else if (key == "ForceReplace") row >> replacement_index;
            else if (key == "ReplaceIgnoreFirst") replace_ignore_first = true;
            else if (key == "ReplaceNumIfChar") {
                std::string character;
                unsigned index;
                if (!(row >> character >> index)) continue;
                for (const auto& team : fighters) {
                    if (std::any_of(team.begin(), team.end(),
                            [&](int fighter) { return _stricmp(roster[fighter], character.c_str()) == 0; }))
                        replacement_index = index;
                }
            } else if (key == "Replace") {
                std::string material, replacement;
                if (!(row >> material)) continue;
                if (replace_ignore_first || replacement_index > 0) {
                    const unsigned index = replace_ignore_first ? replacement_index : replacement_index - 1;
                    for (unsigned i = 0; i <= index && row >> replacement; ++i) {
                        if (i == index) replacements[material] = replacement;
                    }
                }
            } else if (key == "CharReplace") {
                std::string material, replacement;
                unsigned team, slot;
                if (!(row >> material >> team >> slot >> replacement) ||
                    !team || team > fighters.size() || !slot || slot > fighters[team - 1].size()) continue;
                const auto marker = replacement.find("[NAME]");
                if (marker != std::string::npos) replacement.replace(marker, 6, roster[fighters[team - 1][slot - 1]]);
                replacements[material] = std::move(replacement);
            } else if (key == "CAMERA") {
                row >> fov;
                float near_value, far_value;
                if (row >> near_value) near_plane = near_value;
                if (row >> far_value) far_plane = far_value;
            }
        }
        if (!std::isfinite(width) || !std::isfinite(height) || width < 1600 || height < 900)
            throw std::runtime_error("Invalid training stage layout");
        std::unique_ptr<Texture> backdrop;
        std::unique_ptr<PracticeScene> scene;
        const auto scene_name = "stages/" + world + "/background.sgi.msb";
        const auto& entries = stages_archive.entries();
        const bool has_scene = std::any_of(entries.begin(), entries.end(),
            [&](const auto& entry) { return entry.name == scene_name; });
        // Use the authored flat backdrops where the bind pose leaves scenery detached.
        const bool use_flat_backdrop = level == "medici_office_3d" || level == "training_3d" ||
            level == "boardwalk_3d" || level == "rooftops_day_3d" || level == "rooftops_night_3d";
        if (has_scene && !use_flat_backdrop && fov > 0 && fov < 180 && near_plane > 0 && far_plane > near_plane) {
            try {
                auto meshes = skullgirls::read_stage_scene(stages_archive, world, width, height, floor);
                if (meshes.empty()) throw std::runtime_error("Training stage contains no drawable scenery");
                scene = std::make_unique<PracticeScene>(game_data, std::move(meshes), fov, near_plane, far_plane,
                                                        texture_subpath, replacements);
            } catch (const std::runtime_error& error) {
                if (background_asset.empty()) throw;
                OutputDebugStringA(error.what());
                OutputDebugStringA(". Using the flat stage background.\n");
            }
        }
        if (!scene) {
            if (background_asset.empty()) throw std::runtime_error("Training stage has no supported background");
            const std::filesystem::path path(background_asset);
            const auto replacement = replacements.find(path.stem().string());
            if (replacement != replacements.end())
                background_asset = (path.parent_path() / (replacement->second + path.extension().string())).generic_string();
            backdrop = std::make_unique<Texture>();
            backdrop->load(skullgirls::read_sprite_asset(game_data / "stages-textures2d.gfs", background_asset),
                           Texture::Encoding::stage_rgb);
        }
        practice_width = width;
        practice_height = height;
        practice_floor = floor;
        practice_starts = starts;
        practice_scene = std::move(scene);
        practice_background = std::move(backdrop);
    }

    // Rethrow loading errors and initialize rendering on the window thread before starting the intro.
    void finish_practice(HWND window) {
        practice_load.get();
        if (audio) {
            if (!practice_music_intro.empty() && !practice_music_loop.empty()) audio->music(practice_music_intro, practice_music_loop);
            else audio->stage_music(current_stage().id, current_stage().parent);
        }
        prepare_sprite_renderer();
        if (practice_scene) practice_scene->initialize_gpu(window
#ifdef _WIN32
            ,sprite_renderer.get()
#endif
            );
        practice_stage_tick = 0;
        practice_paused = false;
        practice_snapshot.reset();
        practice_keys_held = {};
        reset_practice();
        start_practice_intro();
        idle_start = std::chrono::steady_clock::now();
        training = TrainingView::practice;
    }

    // FUN_0051e6a0 copies the Windows keyboard state to the current snapshot
    // and preserves the previous snapshot. Combat commands read the snapshots.
    std::array<unsigned, 2> practice_keys_held{};
    std::array<unsigned, 2> practice_keys_previous{};
    std::array<unsigned, 2> practice_attack_edges{};
    struct PracticeInput {
        unsigned held = 0;
        unsigned attack_edges = 0;
    };
    struct PracticePushBox {
        float x;
        skullgirls::WorldBox edges;
    };
    std::array<std::optional<PracticePushBox>, 2> previous_push_boxes{};
    struct PracticePresentation {
        enum class Phase { none, intro, knockout, results };
        struct Actor {
            unsigned variant = 0, initial_pose = 0, pose = 0, effect_pose = 0;
            std::optional<skullgirls::FoitEffectPlayback> intro;
            bool facing_right = true, starts_facing_right = true, crouching = false;
        };
        Phase phase = Phase::none;
        unsigned tick = 0;
        std::array<Actor, 2> actors{};
        std::array<bool, 2> defeated{};
    };
    PracticePresentation practice_presentation;
    unsigned practice_intro_cycle = 0, practice_win_cycle = 0;
    struct PracticeSnapshot {
        std::array<PracticeFighter, 2> fighters;
        std::array<PracticeTeam, 2> teams;
        std::array<PracticeHudStats, 2> stats;
        std::array<HudInputs, 2> inputs;
        std::array<std::optional<PracticePushBox>, 2> push_boxes;
        int camera_center, camera_bottom;
        float camera_zoom;
        std::uint64_t stage_tick;
        unsigned recent_dummy_contact;
        std::uint32_t training_rng;
        std::minstd_rand cpu_rng;
        PracticePresentation presentation;
        unsigned intro_cycle, win_cycle;
        std::vector<skullgirls::FoitEffectPlayback> effects;
        float flash_amount;
        skullgirls::CombatPresentation combat;
    };
    std::optional<PracticeSnapshot> practice_snapshot;
    struct DummyRecording {
        std::vector<PracticeInput> input;
        bool facing_right = false;
    };
    std::array<DummyRecording, 3> dummy_recordings{};
    std::optional<std::size_t> dummy_recording, dummy_playing;
    std::size_t dummy_cursor = 0, current_playback_slot = 0;
    bool dummy_armed = false;
    bool dummy_waiting_for_reversal = false, dummy_was_stunned = false;
    unsigned recent_dummy_contact = 0;
    std::uint32_t training_rng_state = 1;
    std::minstd_rand training_random{1};

    // FUN_00469b20 draws an unsigned 15-bit sample from the game's LCG.
    float training_roll(float range) {
        training_rng_state = training_rng_state * 0x41c64e6du + 0x3039u;
        return ((training_rng_state >> 16) & 0x7fffu) * (range / 32768.0f);
    }

    bool dummy_input_override() const {
        return dummy_armed || dummy_recording.has_value() || dummy_playing.has_value();
    }

    // Keep simulation state in one snapshot, including random state, effects, and the intro clock.
    PracticeSnapshot capture_practice_state() const {
        return {practice_players,practice_teams,hud_stats,hud_inputs,previous_push_boxes,
            practice_camera_center,practice_camera_bottom,practice_camera_zoom,practice_stage_tick,
            recent_dummy_contact,training_rng_state,training_random,practice_presentation,
            practice_intro_cycle,practice_win_cycle,practice_effects,super_flash_amount,combat_presentation};
    }
    void load_practice_state(const PracticeSnapshot& saved) {
        practice_players = saved.fighters;
        practice_teams = saved.teams;
        hud_stats = saved.stats;
        hud_inputs = saved.inputs;
        previous_push_boxes = saved.push_boxes;
        practice_camera_center = saved.camera_center;
        practice_camera_bottom = saved.camera_bottom;
        practice_camera_zoom = saved.camera_zoom;
        practice_stage_tick = saved.stage_tick;
        recent_dummy_contact = saved.recent_dummy_contact;
        training_rng_state = saved.training_rng;
        training_random = saved.cpu_rng;
        practice_presentation = saved.presentation;
        practice_intro_cycle = saved.intro_cycle;
        practice_win_cycle = saved.win_cycle;
        practice_effects = saved.effects;
        super_flash_amount = saved.flash_amount;
        combat_presentation = saved.combat;
    }
    void save_or_restore_practice(bool restore) {
        if(!restore || !practice_snapshot) {
            if(!restore)practice_snapshot=capture_practice_state();
            return;
        }
        load_practice_state(*practice_snapshot);
        practice_keys_previous = practice_keys_held;
        practice_attack_edges = {};
        dummy_recording.reset();dummy_playing.reset();dummy_armed=false;
    }

    struct NetplayState {
        PracticeSnapshot simulation;
        std::array<unsigned,2> held,previous,edges;
        std::uint64_t next_entity;
        bool paused;
    };
    bool network_mode=false,rollback_replaying=false;
    // TODO(STEAM): agree on identities, teams, stage, rules, input mapping and seed
    // through the original lobby/session setup before calling this entry point.
    // Steam Input for the Valve controller named Work is deferred separately;
    // native XInput controllers do not intrinsically require Steam networking.
    void begin_netplay_match(std::uint32_t shared_seed) {
        network_mode=true;rollback_replaying=false;practice_paused=false;
        dummy_recording.reset();dummy_playing.reset();dummy_armed=false;
        practice_keys_held={};practice_keys_previous={};practice_attack_edges={};
        next_effect_id=1;practice_stage_tick=0;practice_intro_cycle=practice_win_cycle=0;
        training_rng_state=shared_seed;training_random.seed(shared_seed);reset_practice();
    }
    NetplayState capture_netplay_state() const {
        return {capture_practice_state(),practice_keys_held,practice_keys_previous,practice_attack_edges,next_effect_id,practice_paused};
    }
    void load_netplay_state(const NetplayState& saved) {
        load_practice_state(saved.simulation);practice_keys_held=saved.held;
        practice_keys_previous=saved.previous;practice_attack_edges=saved.edges;
        next_effect_id=saved.next_entity;practice_paused=saved.paused;
    }
    void advance_netplay_frame(const std::array<PracticeInput,2>& inputs,bool replay) {
        if(!network_mode)throw std::runtime_error("Netplay match has not been initialized");
        for(unsigned side=0;side<2;++side){practice_keys_held[side]=inputs[side].held;practice_attack_edges[side]=inputs[side].attack_edges;}
        struct Restore {bool& target;bool old;~Restore(){target=old;}} restore{rollback_replaying,rollback_replaying};
        rollback_replaying=replay;practice_tick();
    }
    using NetInputDecoder=std::function<std::array<PracticeInput,2>(const std::array<skullgirls::NetInput,2>&)>;
    skullgirls::NetplaySession::Callbacks netplay_callbacks(NetInputDecoder decode,
        std::function<std::uint32_t(const NetplayState&)> checksum) {
        if(!decode || !checksum)throw std::invalid_argument("Netplay requires explicit input decoding and logical-state checksum");
        // Original FUN_0053c000 registers these callbacks with the session.
        // TODO(INPUT): resolve original input fields before supplying the decoder;
        // never serialize pointers in NetplayState as a network payload.
        return {[this](){return std::make_shared<NetplayState>(capture_netplay_state());},
            [this](const auto& state){load_netplay_state(*std::static_pointer_cast<const NetplayState>(state));},
            [this,decode](const auto& inputs,bool replay){advance_netplay_frame(decode(inputs),replay);},
            [checksum](const auto& state){return checksum(*std::static_pointer_cast<const NetplayState>(state));},
            {}};
    }

    void toggle_dummy_recording() {
        dummy_armed = false;
        if (dummy_recording) {
            dummy_recording.reset();
            return;
        }
        dummy_playing.reset();
        const auto slot = training_value("OPT_STR_PLAYBACKSLOT");
        if (slot >= dummy_recordings.size()) return;
        dummy_recordings[slot] = {{}, practice_facing_right(1)};
        dummy_recording = slot;
    }

    std::optional<std::size_t> select_dummy_playback_slot() {
        const auto choice = training_value("OPT_STR_PLAYBACKSLOT");
        if (choice < dummy_recordings.size()) {
            if (dummy_recordings[choice].input.empty()) return {};
            return current_playback_slot = choice;
        }
        if (std::none_of(dummy_recordings.begin(), dummy_recordings.end(),
                [](const auto& recording) { return !recording.input.empty(); })) return {};
        // FUN_0057ca80 skips empty slots and reselects on each repeated playback.
        do {
            current_playback_slot = choice == 3 ? (current_playback_slot + 1) % dummy_recordings.size() :
                static_cast<unsigned>(training_roll(1000.0f)) % dummy_recordings.size();
        } while (dummy_recordings[current_playback_slot].input.empty());
        return current_playback_slot;
    }

    void toggle_dummy_playback() {
        dummy_armed = false;
        if (dummy_playing) {
            dummy_playing.reset();
            return;
        }
        dummy_recording.reset();
        dummy_playing = select_dummy_playback_slot();
        dummy_cursor = 0;
        dummy_waiting_for_reversal = training_value("OPT_STR_PLAYBACKMODE") == 2;
        dummy_was_stunned = false;
    }

    PracticeInput training_dummy_input(PracticeInput manual) {
        auto& dummy = practice_players[1];
        const bool facing = practice_facing_right(1);
        if (dummy_recording) {
            auto& input = dummy_recordings[*dummy_recording].input;
            input.push_back(manual);
            if (input.size() == 3600) dummy_recording.reset();
            return manual;
        }
        if (dummy_playing) {
            const bool stunned = dummy.hurt_group >= 0 || dummy.blockstun || dummy.hit_stop;
            const bool recovered = std::exchange(dummy_was_stunned, stunned) && !stunned;
            if (dummy_waiting_for_reversal && !recovered) return manual;
            const auto& recording = dummy_recordings[*dummy_playing];
            const bool flip = training_value("OPT_STR_PLAYBACKSIDECORRECTION") == 0 &&
                recording.facing_right != facing;
            const auto orient = [&](PracticeInput input) {
                if (flip) input.held = (input.held & ~3u) | ((input.held & 1u) << 1) | ((input.held & 2u) >> 1);
                return input;
            };
            if (std::exchange(dummy_waiting_for_reversal, false)) {
                // FUN_0057cb60 primes the command history through the first recorded button.
                const auto button = std::find_if(recording.input.begin(), recording.input.end(),
                    [](PracticeInput input) { return ((input.held | input.attack_edges) & 0x3f0u) != 0; });
                if (button != recording.input.end()) {
                    for (auto it = recording.input.begin(); it != button; ++it) {
                        const auto sample = orient(*it).held;
                        const int x = int(bool(sample & 2)) - int(bool(sample & 1));
                        const int y = int(bool(sample & 8)) - int(bool(sample & 4));
                        std::move_backward(dummy.dash_input_history.begin(), dummy.dash_input_history.end() - 1,
                            dummy.dash_input_history.end());
                        dummy.dash_input_history.front() = skullgirls::filia_input_direction(x, y);
                    }
                    dummy_cursor = static_cast<std::size_t>(button - recording.input.begin());
                    practice_keys_previous[1] &= ~0x3f0u;
                }
            }
            const auto input = orient(recording.input[dummy_cursor++]);
            if (dummy_cursor == recording.input.size()) {
                const auto mode = training_value("OPT_STR_PLAYBACKMODE");
                dummy_cursor = 0;
                if (mode == 0) dummy_playing.reset();
                if (mode == 1) dummy_playing = select_dummy_playback_slot();
                if (mode == 2) dummy_waiting_for_reversal = true;
            }
            return input;
        }
        if (manual.held || manual.attack_edges) return manual;
        const auto cpu = training_value("OPT_STR_TRAINING_CPU_DIFFICULTY");
        if (cpu) {
            const float distance = std::abs(practice_players[0].practice_x - dummy.practice_x);
            if (distance > (cpu == 1 ? 180 : 120)) return {facing ? 2u : 1u};
            if (cpu == 1 || dummy.active_normal >= 0) return {};
            const unsigned interval = 38 - static_cast<unsigned>(cpu) * 5;
            return {practice_stage_tick % interval == 0 ? 16u << (training_random() % 6) : 0u};
        }
        const auto action = training_value("OPT_STR_DUMMYACTION");
        if (action == 1) return {4};
        if (action == 2) return {8};
        if (action == 3 && !dummy.jump_height && !dummy.jump_velocity && !dummy.jump_start)
            return {dummy.landing || !(practice_keys_previous[1] & 4u) ? 4u : 8u};
        return {};
    }

    bool training_contact_mode(std::size_t mode, bool first) {
        switch (mode) {
        case 1: return !first;
        case 2: return true;
        case 3: return first;
        case 4: return training_roll(10.0f) > 5.0f;
        default: return false;
        }
    }

    bool practice_guard(std::size_t side, const skullgirls::NormalHit& hit, bool facing_attacker) {
        auto& defender = practice_players[1 - side];
        auto& stats = hud_stats[side];
        const auto& tag = practice_teams[1 - side].tag;
        if ((tag && tag->kind == PracticeTag::Kind::regular) || defender.tag_entry_ticks ||
            defender.burst_ticks || defender.burst_fall_tick ||
            defender.hurt_group >= 0 || defender.active_normal >= 0 || defender.jump_start ||
            defender.air_dash_direction ||
            defender.throw_phase != PracticeFighter::ThrowPhase::none) return false;
        const bool airborne = defender.jump_height > 0 || defender.jump_velocity > 0;
        const bool holding_back = facing_attacker ? defender.practice_left : defender.practice_right;
        const auto mode = training_value("OPT_STR_BLOCKING");
        const bool automatic = !airborne && side == 0 && !dummy_input_override() &&
            (mode == 3 ? !recent_dummy_contact || defender.blockstun != 0 :
                mode == 4 ? training_roll(10.0f) <= 5.0f || defender.blockstun != 0 :
                training_contact_mode(mode, recent_dummy_contact == 0));
        if (!automatic && !holding_back) return false;
        const auto block_type = training_value("OPT_STR_BLOCKTYPE");
        const bool low = !airborne && (automatic ? (block_type == 1 ||
            (block_type == 0 && (hit.height_mask == 2 ||
                (hit.height_mask == 3 && training_value("OPT_STR_DUMMYACTION") == 1)))) :
            defender.practice_down);
        if (!(hit.height_mask & (low ? 2u : 1u))) return false;
        if (!defender.blockstun) stats.blocked_hits = 0;
        ++stats.blocked_hits;
        if (side == 0) recent_dummy_contact = 25;
        defender.crouching = low;
        defender.rising = false;
        defender.dash_direction = 0;
        defender.walk_direction = 0;
        defender.walk_tick = 0;
        defender.blockstun = std::max(1u, hit.stun * 2 / 3);
        defender.hit_stop = std::max(defender.hit_stop, hit.stop);
        defender.begin_guard_stun(airborne, low, hit.height_mask, facing_attacker);
        stats.notice = L"Block";
        stats.notice_ticks = 45;
        return true;
    }

    // The original requests a pushblock in FUN_005a1170 before the fighter update.
    bool training_pushblock_requested(unsigned contact_count, unsigned hitstop, unsigned stun) {
        const auto threshold = training_value("OPT_STR_PUSHBLOCKAFTERNHITS");
        const auto frequency = training_value("OPT_STR_PUSHBLOCKHOWOFTEN");
        if (frequency == 2)
            return hitstop == 1 && stun > 1 && training_roll(100.0f) <= 15.0f;
        return threshold && (contact_count == threshold || (frequency == 1 && contact_count > threshold)) &&
            (hitstop || (frequency == 1 && stun));
    }

    void advance_training_pushblock(float left, float right) {
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& defender = practice_players[side];
            if (!defender.blockstun || defender.guard_phase == PracticeFighter::GuardPhase::pushblock) continue;
            const auto punches = (defender.held_attack_buttons | defender.pressed_punches) & 7u;
            const bool manual = defender.pressed_punches && (punches & (punches - 1));
            const std::size_t attacker_side = 1 - side;
            auto& stats = hud_stats[attacker_side];
            if (!manual && (side != 1 || dummy_input_override() ||
                !training_pushblock_requested(stats.blocked_hits, defender.hit_stop, defender.blockstun))) continue;
            if (manual) {
                defender.pressed_punches = 0;
                defender.normal_input = -1;
            }
            auto& attacker = practice_players[attacker_side];
            attacker.practice_x = std::clamp(attacker.practice_x +
                (defender.hurt_facing_right ? 85.0f : -85.0f), left, right);
            defender.guard_phase = PracticeFighter::GuardPhase::pushblock;
            defender.guard_tick = 0;
            if (defender.cerebella) {
                defender.cerebella->start(defender.jump_height > 0 ? 437u : defender.crouching ? 431u : 422u);
                defender.cerebella->reacting = true;
            }
            if (defender.squigly) {
                defender.squigly->start(defender.jump_height > 0 ? 495u : defender.crouching ? 489u : 480u);
                defender.squigly->reacting = true;
            }
            if (defender.fukua) {
                defender.fukua->start(defender.jump_height > 0 ? 398u : defender.crouching ? 392u : 383u);
                defender.fukua->reacting = true;
            }
            if (defender.valentine) {
                defender.valentine->start(defender.jump_height > 0 ? 453u : defender.crouching ? 447u : 438u);
                defender.valentine->reacting = true;
            }
            if (defender.parasoul) {
                defender.parasoul->start(defender.jump_height > 0 ? 427u : defender.crouching ? 421u : 412u);
                defender.parasoul->reacting = true;
            }
            if (defender.painwheel) {
                defender.painwheel->start(defender.jump_height > 0 ? 445u : defender.crouching ? 439u : 430u);
                defender.painwheel->reacting = true;
            }
            if (defender.robofortune) {
                defender.robofortune->start(defender.jump_height>0?415u:defender.crouching?409u:400u);
                defender.robofortune->reacting=true;
            }
            if (defender.msfortune) {
                defender.msfortune->start(defender.jump_height > 0 ? 416u : defender.crouching ? 410u : 401u);
                defender.msfortune->reacting = true;
            }
            if (defender.peacock) {
                defender.peacock->start(defender.jump_height > 0 ? 417u : defender.crouching ? 411u : 402u);
                defender.peacock->reacting = true;
            }
            const float height = defender.jump_height + (defender.crouching ? 95.0f : 160.0f);
            spawn_combat_spark(attacker_side, CombatEffect::pushblock, {defender.practice_x, height});
            stats.notice = L"Pushblock";
            stats.notice_ticks = 45;
        }
    }

    void advance_training_bursts() {
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& defender = practice_players[side];
            if (practice_point_snapped(side) || !defender.burst_available || defender.hurt_group < 0 || defender.fenrir_captured ||
                defender.throw_phase != PracticeFighter::ThrowPhase::none || defender.ground_tech_direction ||
                !(defender.pressed_punches || defender.pressed_kicks)) continue;
            defender.burst_available = false;
            defender.burst_ticks = PracticeFighter::burst_clip.duration();
            if (defender.cerebella) defender.cerebella->start(448);
            if (defender.peacock) defender.peacock->start(429);
            if (defender.painwheel) defender.painwheel->start(457);
            if (defender.squigly) defender.squigly->start(507);
            if (defender.fukua) defender.fukua->start(410);
            if (defender.valentine) defender.valentine->start(465);
            if (defender.parasoul) defender.parasoul->start(439);
            if (defender.msfortune) defender.msfortune->start(428);
            if (defender.robofortune) defender.robofortune->start(427);
            defender.burst_fall_tick = 0;
            defender.burst_connected = false;
            defender.hurt_group = -1;
            defender.heavy_reaction = skullgirls::HeavyReaction::none;
            defender.hurt_hold = defender.hit_stop = defender.blockstun = 0;
            defender.guard_phase = PracticeFighter::GuardPhase::none;
            defender.normal_input = -1;
            defender.pressed_punches = defender.pressed_kicks = 0;
            defender.throw_input = false;
            defender.buffered_special.reset();
            defender.normal_inputs = {};
            defender.air_hurt_fall = false;
            defender.jump_velocity = 0;
            defender.jump_facing_right = defender.hurt_facing_right;
            hud_stats[side].notice = L"Burst";
            hud_stats[side].notice_ticks = 45;
        }
    }

    static constexpr std::array practice_assist_options{
        "PRACTICE_ASSIST_1_1", "PRACTICE_ASSIST_1_2", "PRACTICE_ASSIST_1_3",
        "PRACTICE_ASSIST_2_1", "PRACTICE_ASSIST_2_2", "PRACTICE_ASSIST_2_3",
    };

    PracticeFighter::SpecialCommand practice_assist_move(std::size_t side, std::size_t slot) const {
        constexpr std::array<std::size_t, 3> defaults{2, 4, 6};
        const auto choice = training_value(practice_assist_options[side * 3 + slot], defaults[slot]);
        const auto strength = static_cast<unsigned>(choice % 3);
        if (practice_slot_is_peacock(side, slot)) {
            constexpr int moves[] = {165, 166, 167, 189, 193, 197, 170, 171, 172};
            return {moves[std::min<std::size_t>(choice, std::size(moves) - 1)]};
        }
        if (practice_slot_is_squigly(side, slot)) {
            constexpr int moves[]={195,188,203,235,246,252,261,195,246};
            return {moves[std::min<std::size_t>(choice,std::size(moves)-1)]};
        }
        if (practice_slot_is_fukua(side, slot)) {
            constexpr int moves[]={158,159,160,148,149,150,144,145,146};
            return {moves[std::min<std::size_t>(choice,std::size(moves)-1)]};
        }
        if (practice_slot_is_valentine(side, slot)) {
            constexpr int moves[] = {170,171,172,198,199,200,183,184,185};
            return {moves[std::min<std::size_t>(choice,std::size(moves)-1)]};
        }
        if (practice_slot_is_parasoul(side, slot)) {
            constexpr int moves[] = {210, 211, 212, 183, 184, 185, 215, 216, 217};
            return {moves[std::min<std::size_t>(choice, std::size(moves) - 1)]};
        }
        if (practice_slot_is_painwheel(side, slot)) {
            constexpr int moves[] = {182, 183, 184, 185, 187, 189, 173, 174, 175};
            return {moves[std::min<std::size_t>(choice, std::size(moves) - 1)]};
        }
        if (practice_slot_is_robofortune(side, slot)) {
            constexpr int moves[] = {162, 163, 164, 176, 177, 179, 169, 171, 173};
            return {moves[std::min<std::size_t>(choice, std::size(moves) - 1)]};
        }
        if (practice_slot_is_msfortune(side, slot)) {
            constexpr int moves[] = {171, 173, 175, 182, 183, 184, 194, 197, 164};
            return {moves[std::min<std::size_t>(choice, std::size(moves) - 1)]};
        }
        if (practice_slot_is_cerebella(side, slot)) {
            constexpr int moves[] = {182, 184, 185, 187, 189, 191, 173, 166, 178};
            return {moves[std::min<std::size_t>(choice, std::size(moves) - 1)]};
        }
        constexpr int moves[] = {skullgirls::updo_light, skullgirls::ground_hairball, skullgirls::ringlet_spike};
        return {moves[choice / 3] + (choice < 3 ? static_cast<int>(strength) : 0), strength};
    }

    std::optional<std::size_t> next_practice_reserve(std::size_t side) const {
        const auto& team = practice_teams[side];
        for (std::size_t slot = 0; slot < fighters[side].size(); ++slot)
            if (slot != team.active_slot && team.health[slot].life && practice_slot_playable(side, slot)) return slot;
        return {};
    }

    float practice_team_gate(bool home_on_left) const {
        const float width = 1280.0f + 320.0f * practice_camera_zoom;
        const float left = std::clamp(practice_camera_center - width * 0.5f, 0.0f,
                                      std::max(0.0f, practice_width - width));
        constexpr float offscreen_margin = 160.0f;
        return home_on_left ? left - offscreen_margin : left + width + offscreen_margin;
    }

    void begin_practice_tag(std::size_t side, std::size_t slot, PracticeTag::Kind kind) {
        using Motion = PracticeTeamTravel::Motion;
        const bool home_on_left = practice_facing_right(side);
        auto& team = practice_teams[side];
        const auto motion = kind == PracticeTag::Kind::regular ? Motion::tag_in : Motion::assist_in;
        const float height = motion == Motion::tag_in ? 140.0f : 180.0f;
        team.tag = PracticeTag{{slot, home_on_left, {practice_team_gate(home_on_left), height, home_on_left, motion}}, kind};
    }

    bool try_practice_snapback(std::size_t attacker_side, const skullgirls::NormalHit& hit) {
        const auto side = 1 - attacker_side;
        auto& team = practice_teams[side];
        auto slot = next_practice_reserve(side);
        if (!slot) return false;

        const auto& attacker = practice_players[attacker_side];
        std::size_t preferred = attacker.snapback_reserve;
        if (preferred >= team.active_slot) ++preferred;
        if (practice_slot_playable(side, preferred) && team.health[preferred].life) slot = preferred;
        // The incoming slot already owns its health, even if it was on screen as an assist.
        if (team.assist && team.assist->actor.slot == *slot) team.assist.reset();
        if (team.departing && team.departing->slot == *slot) team.departing.reset();

        auto& point = practice_players[side];
        const bool facing = practice_facing_right(side);
        point.take_hit(hit, facing, skullgirls::filia_snapback);
        point.burst_available = false;
        begin_practice_tag(side, *slot, PracticeTag::Kind::snapback);
        team.tag->forced_exit = PracticeTeamTravel{point.practice_x, point.jump_height, facing,
            PracticeTeamTravel::Motion::snap_out};
        team.tag->forced_exit->pose = 769;
        hud_stats[side].notice = L"Snapback";
        hud_stats[side].notice_ticks = 45;
        return true;
    }

    void tag_practice_fighter(std::size_t side, std::size_t slot, PracticeTag::Kind kind) {
        auto& team = practice_teams[side];
        auto& point = practice_players[side];
        const bool guard_tag = kind == PracticeTag::Kind::guard;
        team.health[team.active_slot] = hud_stats[side];
        team.active_slot = slot;
        static_cast<PracticeLife&>(hud_stats[side]) = team.health[slot];
        PracticeFighter incoming;
        incoming.practice_x = point.practice_x;
        incoming.palette_slot = slot;
        incoming.tension = point.tension - unsigned(guard_tag);
        incoming.attack_instance = point.attack_instance;
        incoming.ringlet_spikes = std::move(point.ringlet_spikes);
        incoming.tricobezoar = std::move(point.tricobezoar);
        incoming.normal_facing_right = practice_facing_right(side);
        if (practice_slot_is_cerebella(side, slot)) {
            auto& bella = incoming.cerebella.emplace();
            bella.facing_right = incoming.normal_facing_right;
            bella.start(guard_tag ? static_cast<unsigned>(practice_assist_move(side, slot).move) :
                kind == PracticeTag::Kind::regular ? 488u : 40u);
            bella.animate(cerebella_assets);
            if (guard_tag) ++incoming.attack_instance;
        } else if (practice_slot_is_peacock(side, slot)) {
            auto& actor = incoming.peacock.emplace();
            actor.facing_right = incoming.normal_facing_right;
            actor.start(guard_tag ? static_cast<unsigned>(practice_assist_move(side, slot).move) :
                kind == PracticeTag::Kind::regular ? 468u : 41u);
            actor.animate(peacock_assets);
            if (guard_tag) ++incoming.attack_instance;
        } else if (practice_slot_is_squigly(side, slot)) {
            auto& actor = incoming.squigly.emplace();
            actor.facing_right = incoming.normal_facing_right;
            actor.start(guard_tag ? static_cast<unsigned>(practice_assist_move(side, slot).move) :
                kind == PracticeTag::Kind::regular ? 545u : 46u);
            actor.animate(squigly_assets);
            if (guard_tag) ++incoming.attack_instance;
        } else if (practice_slot_is_fukua(side, slot)) {
            auto& actor = incoming.fukua.emplace();
            actor.facing_right = incoming.normal_facing_right;
            actor.start(guard_tag ? static_cast<unsigned>(practice_assist_move(side, slot).move) :
                kind == PracticeTag::Kind::regular ? 450u : 42u);
            actor.animate(fukua_assets);
            if (guard_tag) ++incoming.attack_instance;
        } else if (practice_slot_is_valentine(side, slot)) {
            auto& actor = incoming.valentine.emplace();
            actor.facing_right = incoming.normal_facing_right;
            actor.start(guard_tag ? static_cast<unsigned>(practice_assist_move(side, slot).move) :
                kind == PracticeTag::Kind::regular ? 503u : 41u);
            actor.animate(valentine_assets);
            if (guard_tag) ++incoming.attack_instance;
        } else if (practice_slot_is_parasoul(side, slot)) {
            auto& actor = incoming.parasoul.emplace();
            actor.facing_right = incoming.normal_facing_right;
            actor.start(guard_tag ? static_cast<unsigned>(practice_assist_move(side, slot).move) :
                kind == PracticeTag::Kind::regular ? 493u : 40u);
            actor.animate(parasoul_assets);
            if (guard_tag) ++incoming.attack_instance;
        } else if (practice_slot_is_painwheel(side, slot)) {
            auto& actor = incoming.painwheel.emplace();
            actor.facing_right = incoming.normal_facing_right;
            actor.start(guard_tag ? static_cast<unsigned>(practice_assist_move(side, slot).move) :
                kind == PracticeTag::Kind::regular ? 493u : 41u);
            actor.animate(painwheel_assets);
            if (guard_tag) ++incoming.attack_instance;
        } else if (practice_slot_is_robofortune(side, slot)) {
            auto& actor = incoming.robofortune.emplace();
            actor.facing_right = incoming.normal_facing_right;
            actor.start(guard_tag ? static_cast<unsigned>(practice_assist_move(side, slot).move) :
                kind == PracticeTag::Kind::regular ? 466u : 44u);
            actor.animate(robofortune_assets);
            if (guard_tag) ++incoming.attack_instance;
        } else if (practice_slot_is_msfortune(side, slot)) {
            auto& actor = incoming.msfortune.emplace();
            actor.facing_right = incoming.normal_facing_right;
            actor.start(guard_tag ? static_cast<unsigned>(practice_assist_move(side, slot).move) :
                kind == PracticeTag::Kind::regular ? 469u : 44u);
            actor.animate(msfortune_assets);
            if (guard_tag) ++incoming.attack_instance;
        } else if (guard_tag) {
            incoming.active_normal = incoming.prepare_special(practice_assist_move(side, slot), false);
            ++incoming.attack_instance;
        } else if (kind == PracticeTag::Kind::regular) {
            incoming.tag_entry_ticks = skullgirls::team_animation_ticks(skullgirls::tag_in_taunt);
        } else {
            incoming.landing = 1;
        }
        point = std::move(incoming);
    }

    // Resolve entries and exits and hand control over when the incoming fighter is ready.
    void advance_practice_tags() {
        using Motion = PracticeTeamTravel::Motion;
        using Kind = PracticeTag::Kind;
        for (std::size_t side = 0; side < practice_teams.size(); ++side) {
            auto& team = practice_teams[side];
            if (team.departing && team.departing->travel.advance_exit(practice_team_gate(team.departing->home_on_left),
                                                                     team.departing->home_on_left))
                team.departing.reset();
            if (!team.tag) continue;
            auto& point = practice_players[side];
            auto& tag = *team.tag;
            using Phase = PracticeFighter::ThrowPhase;
            if (tag.kind == Kind::snapback) {
                if (point.hit_stop) {
                    --point.hit_stop;
                    continue;
                }
                if (tag.forced_exit) {
                    if (tag.forced_exit->advance_exit(practice_team_gate(tag.incoming.home_on_left),
                                                     tag.incoming.home_on_left))
                        tag.forced_exit.reset();
                    continue;
                }
            } else if (tag.kind != Kind::knockout &&
                (point.hurt_group >= 0 || point.fenrir_captured || point.throw_phase != Phase::none)) {
                if (tag.incoming.travel.pose) {
                    team.departing = tag.incoming;
                    team.departing->travel = {tag.incoming.travel.x, tag.incoming.travel.height,
                        !tag.incoming.home_on_left, Motion::assist_out};
                }
                team.tag.reset();
                continue;
            }
            point.practice_left = point.practice_right = point.practice_up = point.practice_down = false;
            point.throw_input = false;
            point.pressed_punches = point.pressed_kicks = point.held_attack_buttons = 0;
            point.normal_input = -1;
            if (!tag.incoming.travel.ticks)
                tag.incoming.travel.x = practice_team_gate(tag.incoming.home_on_left);
            if (tag.kind == Kind::regular) {
                constexpr auto duration = skullgirls::team_animation_ticks(skullgirls::tag_out_taunt);
                point.movement_sprite = skullgirls::team_animation_sprite(skullgirls::tag_out_taunt,
                    tag.startup_tick, 1000);
                if (point.cerebella) {
                    if (!tag.startup_tick) point.cerebella->start(481);
                    point.cerebella->animate(cerebella_assets);
                }
                if (point.peacock) {
                    if (!tag.startup_tick) point.peacock->start(462);
                    point.peacock->animate(peacock_assets);
                }
                if (point.squigly) {
                    if (!tag.startup_tick) point.squigly->start(540);
                    point.squigly->animate(squigly_assets);
                }
            if (point.fukua) {
                    if (!tag.startup_tick) point.fukua->start(443);
                    point.fukua->animate(fukua_assets);
                }
            if (point.valentine) {
                    if (!tag.startup_tick) point.valentine->start(498);
                    point.valentine->animate(valentine_assets);
                }
            if (point.parasoul) {
                    if (!tag.startup_tick) point.parasoul->start(472);
                    point.parasoul->animate(parasoul_assets);
                }
                if (point.robofortune) {
                    if (!tag.startup_tick) point.robofortune->start(460);
                    point.robofortune->animate(robofortune_assets);
                }
                if (point.painwheel) {
                    if (!tag.startup_tick) point.painwheel->start(490);
                    point.painwheel->animate(painwheel_assets);
                }
                if (point.msfortune) {
                    if (!tag.startup_tick) point.msfortune->start(461);
                    point.msfortune->animate(msfortune_assets);
                }
                point.normal_facing_right = tag.incoming.home_on_left;
                if (tag.startup_tick < duration) ++tag.startup_tick;
                const auto approach_ticks = static_cast<unsigned>(
                    std::ceil(std::abs(point.practice_x - tag.incoming.travel.x) / PracticeTeamTravel::speed));
                if (tag.startup_tick + approach_ticks < duration) continue;
            }
            if (!tag.incoming.travel.advance(point.practice_x)) continue;
            if (tag.kind == Kind::regular &&
                tag.startup_tick < skullgirls::team_animation_ticks(skullgirls::tag_out_taunt)) continue;
            if (tag.kind != Kind::snapback) {
                team.departing = PracticeTeamActor{team.active_slot, tag.incoming.home_on_left,
                    {point.practice_x, point.jump_height, tag.incoming.home_on_left, Motion::tag_out}};
                team.departing->travel.pose = 1012;
                if (tag.kind != Kind::regular)
                    team.departing->travel = {point.practice_x, point.jump_height,
                        !tag.incoming.home_on_left, Motion::assist_out};
            } else {
                hud_stats[1 - side].combo = false;
            }
            tag_practice_fighter(side, tag.incoming.slot, tag.kind);
            hud_stats[side].notice = tag.kind == Kind::knockout ? L"Next Fighter" :
                tag.kind == Kind::guard ? L"Guard Tag" : tag.kind == Kind::snapback ? L"Snapback" : L"Tag";
            hud_stats[side].notice_ticks = 45;
            team.tag.reset();
        }
    }

    void sample_practice_team_actions() {
        for (std::size_t side = 0; side < practice_teams.size(); ++side) {
            auto& team = practice_teams[side];
            if (team.assist_cooldown) --team.assist_cooldown;
            auto& point = practice_players[side];
            const bool bursting = point.burst_ticks || point.burst_fall_tick;
            if (team.assist || team.tag || team.departing || point.tag_entry_ticks ||
                !practice_slot_playable(side, team.active_slot) || point.throw_input ||
                point.throw_phase != PracticeFighter::ThrowPhase::none || point.hurt_group >= 0 || point.jump_start ||
                (!bursting && (point.jump_height || point.jump_velocity)) ||
                !(point.pressed_punches || point.pressed_kicks)) continue;
            const unsigned edges = point.pressed_punches | (point.pressed_kicks << 3);
            const unsigned buttons = point.held_attack_buttons | edges;
            const auto chord = [&](unsigned punch, unsigned kick) {
                const unsigned mask = (1u << punch) | (8u << kick);
                return (buttons & mask) == mask && (edges & mask);
            };
            const bool tag_second = chord(1, 1), tag_third = chord(2, 2);
            const bool tag = tag_second || tag_third;
            const bool assist_second = chord(0, 1) || chord(1, 0) || chord(2, 0);
            const bool assist_third = chord(1, 2) || chord(2, 1) || chord(0, 2);
            if (!tag && !assist_second && !assist_third) continue;
            std::size_t slot = (tag ? tag_third : !assist_second) ? 1 : 0;
            if (slot >= team.active_slot) ++slot;
            if (!practice_slot_playable(side, slot) || !team.health[slot].life) continue;

            const bool facing_right = practice_facing_right(side);
            const int x = int(point.practice_right) - int(point.practice_left);
            const bool guard_tag = tag && (point.blockstun || bursting) && point.tension &&
                x == (facing_right ? 1 : -1);
            if ((!guard_tag && (bursting || point.guard_locked() || point.hit_stop)) ||
                (tag && point.active_normal >= 0) || (!tag && team.assist_cooldown)) continue;
            if (tag && !guard_tag) {
                auto history = point.dash_input_history;
                std::move_backward(history.begin(), history.end() - 1, history.end());
                history.front() = skullgirls::filia_input_direction(x, int(point.practice_up) - int(point.practice_down));
                if (point.tension && skullgirls::filia_quarter_circle(history, facing_right, false)) continue;
            }

            point.normal_input = -1;
            point.pressed_punches = point.pressed_kicks = 0;
            if (!tag) {
                auto& assist = team.assist.emplace();
                assist.actor = {slot, facing_right, {practice_team_gate(facing_right), 180, facing_right,
                    PracticeTeamTravel::Motion::assist_in}};
                assist.command = practice_assist_move(side, slot);
                auto& fighter = assist.fighter;
                if (practice_slot_is_cerebella(side, slot)) {
                    fighter.cerebella.emplace().facing_right = facing_right;
                }
                if (practice_slot_is_peacock(side, slot)) fighter.peacock.emplace().facing_right = facing_right;
                if (practice_slot_is_robofortune(side, slot)) fighter.robofortune.emplace().facing_right = facing_right;
                if (practice_slot_is_painwheel(side, slot)) fighter.painwheel.emplace().facing_right = facing_right;
                if (practice_slot_is_squigly(side, slot)) {
                    auto& actor=fighter.squigly.emplace();actor.facing_right=facing_right;
                    actor.state.variables[0x1b]=0.0f;
                }
            if (practice_slot_is_fukua(side, slot)) {
                    auto& actor=fighter.fukua.emplace();actor.facing_right=facing_right;
                    actor.state.variables[0x1b]=0.0f;
                }
            if (practice_slot_is_valentine(side, slot)) {
                    auto& actor=fighter.valentine.emplace();actor.facing_right=facing_right;
                    actor.state.variables[0x1b]=0.0f;
                }
                if (practice_slot_is_parasoul(side, slot)) fighter.parasoul.emplace().facing_right = facing_right;
                if (practice_slot_is_msfortune(side, slot)) fighter.msfortune.emplace().facing_right = facing_right;
                fighter.palette_slot = slot;
                fighter.attack_instance = (std::uint64_t{1} << 63) | practice_stage_tick;
                hud_stats[side].notice = L"Assist";
                hud_stats[side].notice_ticks = 45;
            } else {
                begin_practice_tag(side, slot, guard_tag ? PracticeTag::Kind::guard : PracticeTag::Kind::regular);
                if (bursting) {
                    // Burst_ForReal and Burst_Fall can branch to DHC_Out on the switch.
                    team.tag->from_burst = true;
                    point.burst_ticks = point.burst_fall_tick = 0;
                }
            }
        }
    }

    void practice_key(WPARAM key, bool pressed) {
        // Left, right, down, up, LP, MP, HP, LK, MK, HK.
        constexpr std::array<std::array<WPARAM, 10>, 2> keys{{
            {VK_LEFT, VK_RIGHT, VK_DOWN, VK_UP, 'A', 'S', 'D', 'Z', 'X', 'C'},
            {'J', 'L', 'K', 'I', 'V', 'B', 'N', 'F', 'G', 'H'},
        }};
        for (std::size_t side = 0; side < keys.size(); ++side) {
            const auto& bindings = keys[side];
            const auto action = std::find(bindings.begin(), bindings.end(), key);
            if (action == bindings.end()) continue;
            const unsigned mask = 1u << (action - bindings.begin());
            auto& held = practice_keys_held[side];
            if (pressed) {
                // Preserve attack edges even when key-up arrives before the next simulation tick.
                if (!practice_paused && mask >= 16u && !(held & mask)) practice_attack_edges[side] |= mask;
                held |= mask;
            } else {
                held &= ~mask;
            }
            return;
        }
    }

    void sample_practice_keys() {
        const std::array inputs{
            PracticeInput{practice_keys_held[0], std::exchange(practice_attack_edges[0], 0u)},
            network_mode ? PracticeInput{practice_keys_held[1],std::exchange(practice_attack_edges[1],0u)} :
                training_dummy_input({practice_keys_held[1], std::exchange(practice_attack_edges[1], 0u)}),
        };
        constexpr unsigned throw_buttons = (1u << 4) | (1u << 7);
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            const auto [held, attack_edges] = inputs[side];
            const unsigned pressed = (held & ~practice_keys_previous[side]) | attack_edges;
            practice_keys_previous[side] = held;
            auto& history = hud_inputs[side];
            const unsigned shown = held | attack_edges;
            if (shown && shown != history.last) {
                std::move_backward(history.entries.begin(), history.entries.end() - 1, history.entries.end());
                history.entries.front() = shown;
                history.count = std::min(history.count + 1, history.entries.size());
            }
            history.last = shown;
            auto& player = practice_players[side];
            player.practice_left = (held & 1u) != 0;
            player.practice_right = (held & 2u) != 0;
            player.practice_down = (held & 12u) == 4u;
            player.practice_up = (held & 12u) == 8u;
            player.held_attack_buttons = held >> 4;
            player.pressed_punches = (pressed >> 4) & 7u;
            player.pressed_kicks = (pressed >> 7) & 7u;
            if ((held & throw_buttons) == throw_buttons && (pressed & throw_buttons)) {
                player.throw_input = true;
                player.normal_input = -1;
                continue;
            }
            for (int button : {0, 3, 1, 4, 2, 5}) {
                if (!(pressed & (1u << (button + 4)))) continue;
                player.normal_input = button;
                break;
            }
        }
    }

    void reset_practice() {
        if (audio) audio->stop_combat();
        practice_effects.clear();
        super_flash_amount = 0;
        combat_presentation = {};
        for (std::size_t i = 0; i < practice_assist_options.size(); ++i) {
            auto option = std::find_if(training_options.begin(), training_options.end(), [&](const auto& item) {
                return item.keyword == practice_assist_options[i];
            });
            if (option == training_options.end()) continue;
            const bool bella = practice_slot_is_cerebella(i / 3, i % 3);
            option->label = L"P" + std::to_wstring(i / 3 + 1) + (practice_slot_is_fukua(i / 3, i % 3) ? L" Fukua " : practice_slot_is_squigly(i / 3, i % 3) ? L" Squigly " : practice_slot_is_valentine(i / 3, i % 3) ? L" Valentine " : practice_slot_is_robofortune(i / 3, i % 3) ? L" Robo-Fortune " : practice_slot_is_painwheel(i / 3, i % 3) ? L" Painwheel " : practice_slot_is_msfortune(i / 3, i % 3) ? L" Ms. Fortune " : practice_slot_is_parasoul(i / 3, i % 3) ? L" Parasoul " : practice_slot_is_peacock(i / 3, i % 3) ? L" Peacock " : bella ? L" Cerebella " : L" Filia ") +
                std::to_wstring(i % 3 + 1) + L" Assist";
            option->choices = practice_slot_is_robofortune(i / 3, i % 3) ? std::vector<std::wstring>{L"Theonite Beam (Light)",L"Theonite Beam (Medium)",L"Theonite Beam (Heavy)",L"Danger! (Light)",L"Danger! (Medium)",L"Danger! (Heavy)",L"Headrone Ram",L"Headrone Airstrike",L"Headrone Salvo"} : practice_slot_is_painwheel(i / 3, i % 3) ? std::vector<std::wstring>{L"Gae Bolga Stinger (Light)",L"Gae Bolga Stinger (Medium)",L"Gae Bolga Stinger (Heavy)",L"Buer Reaper (Light)",L"Buer Reaper (Medium)",L"Buer Reaper (Heavy)",L"Pinion Dash (Light)",L"Pinion Dash (Medium)",L"Pinion Dash (Heavy)"} : practice_slot_is_msfortune(i / 3, i % 3) ?
                std::vector<std::wstring>{L"Fiber Upper (Light)", L"Fiber Upper (Medium)", L"Fiber Upper (Heavy)",
                    L"Cat Scratch (Light)", L"Cat Scratch (Medium)", L"Cat Scratch (Heavy)", L"Cat Slide", L"Cat Claw", L"Head Roll"} : practice_slot_is_squigly(i / 3, i % 3) ? std::vector<std::wstring>{L"Arpeggio",L"Silver Chord",L"Tremolo",L"Liver Mortis",L"Draugen Punch",L"Center Stage",L"Drag n Bite",L"Arpeggio",L"Draugen Punch"} : practice_slot_is_fukua(i / 3, i % 3) ? std::vector<std::wstring>{L"Love Dart (Light)",L"Love Dart (Medium)",L"Love Dart (Heavy)",L"Drill (Light)",L"Drill (Medium)",L"Drill (Heavy)",L"Shadow (Light)",L"Shadow (Medium)",L"Shadow (Heavy)"} : practice_slot_is_valentine(i / 3, i % 3) ? std::vector<std::wstring>{L"Dead Cross (Light)",L"Dead Cross (Medium)",L"Dead Cross (Heavy)",L"Savage Bypass (Light)",L"Savage Bypass (Medium)",L"Savage Bypass (Heavy)",L"Vial Hazard A",L"Vial Hazard B",L"Vial Hazard C"} : practice_slot_is_parasoul(i / 3, i % 3) ?
                std::vector<std::wstring>{L"Napalm Shot (Light)", L"Napalm Shot (Medium)", L"Napalm Shot (Heavy)",
                    L"Napalm Quake", L"Napalm Pillar", L"Napalm Trigger", L"Egret Call", L"Egret Dive", L"Egret Charge"} : practice_slot_is_peacock(i / 3, i % 3) ?
                std::vector<std::wstring>{L"Screwball Cannon (Light)", L"Screwball Cannon (Medium)", L"Screwball Cannon (Heavy)",
                    L"George (Light)", L"George (Medium)", L"George (Heavy)",
                    L"Shadow (Light)", L"Shadow (Medium)", L"Shadow (Heavy)"} : bella ? std::vector<std::wstring>{L"Diamond Deflector", L"Devil Horns",
                L"Cerecopter", L"Lock 'n' Load (Light)", L"Lock 'n' Load (Medium)",
                L"Lock 'n' Load (Heavy)", L"Diamond Drop", L"Merry Go-Rilla", L"Excellebella"} :
                std::vector<std::wstring>{L"Updo (Light)", L"Updo (Medium)", L"Updo (Heavy)",
                L"Hairball (Light)", L"Hairball (Medium)", L"Hairball (Heavy)",
                L"Ringlet Spike (Light)", L"Ringlet Spike (Medium)", L"Ringlet Spike (Heavy)"};
        }
        practice_presentation = {};
        practice_keys_previous = practice_keys_held;
        practice_attack_edges = {};
        previous_push_boxes = {};
        hud_stats = {};
        for (auto& stats : hud_stats) stats.reset_life(training_life());
        hud_inputs = {};
        dummy_recording.reset();
        dummy_playing.reset();
        dummy_armed = false;
        recent_dummy_contact = 0;
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            practice_teams[side] = {};
            for (auto& health : practice_teams[side].health) health.reset_life(training_life());
            practice_players[side] = {};
            if (practice_slot_is_cerebella(side, 0)) {
                auto& bella = practice_players[side].cerebella.emplace();
                bella.facing_right = side == 0;
            }
            if (practice_slot_is_peacock(side, 0)) practice_players[side].peacock.emplace().facing_right = side == 0;
            if (practice_slot_is_robofortune(side, 0)) practice_players[side].robofortune.emplace().facing_right = side == 0;
            if (practice_slot_is_painwheel(side, 0)) practice_players[side].painwheel.emplace().facing_right = side == 0;
            if (practice_slot_is_squigly(side, 0)) practice_players[side].squigly.emplace().facing_right = side == 0;
            if (practice_slot_is_fukua(side, 0)) practice_players[side].fukua.emplace().facing_right = side == 0;
            if (practice_slot_is_valentine(side, 0)) practice_players[side].valentine.emplace().facing_right = side == 0;
            if (practice_slot_is_parasoul(side, 0)) practice_players[side].parasoul.emplace().facing_right = side == 0;
            if (practice_slot_is_msfortune(side, 0)) practice_players[side].msfortune.emplace().facing_right = side == 0;
            practice_players[side].practice_x = practice_starts[side];
            practice_players[side].tension = training_value("OPT_STR_METERLEVEL", 5);
        }
        update_practice_camera(true);
    }

    bool practice_input(WPARAM key, bool repeated, bool shift = false) {
        // TODO(STEAM): submit device inputs through GGPO; pause/reset must be
        // synchronized commands rather than local Training shortcuts.
        if(network_mode)return false;
        if (!repeated) practice_key(key, true);
        if (!practice_paused) {
            if (repeated) return false;
            if (key == VK_RETURN) {
                practice_paused = true;
                practice_attack_edges = {};
                pause_cursor = 0;
                pause_page.reset();
            } else if (key == 'R') {
                reset_practice();
                if (shift) start_practice_intro();
            } else if (key == VK_F6 || key == VK_F7) {
                save_or_restore_practice(key == VK_F7);
            } else if (key == VK_F8 || key == VK_F9) {
                if (key == VK_F8) toggle_dummy_recording();
                else toggle_dummy_playback();
            } else if (key == VK_BACK) {
                const auto action = training_value("OPT_STR_SELECTBUTTON");
                const bool use_dummy = action == 1 || (action == 2 && shift);
                if (!use_dummy) save_or_restore_practice(!shift && practice_snapshot.has_value());
                else {
                    const auto slot = training_value("OPT_STR_PLAYBACKSLOT");
                    const bool available = slot < dummy_recordings.size() ? !dummy_recordings[slot].input.empty() :
                        std::any_of(dummy_recordings.begin(), dummy_recordings.end(),
                            [](const auto& recording) { return !recording.input.empty(); });
                    if (shift && slot < dummy_recordings.size()) {
                        dummy_recordings[slot] = {};
                        dummy_recording.reset();
                        dummy_playing.reset();
                        dummy_armed = false;
                    } else if (dummy_recording || dummy_armed) {
                        toggle_dummy_recording();
                    } else if (available) {
                        toggle_dummy_playback();
                    } else if (slot < dummy_recordings.size()) {
                        dummy_armed = true;
                    }
                }
            } else {
                return false;
            }
            return true;
        }
        const bool accept = !repeated && (key == VK_RETURN || key == 'Z' || key == 'A');
        const bool escape = !repeated && key == VK_ESCAPE;
        const bool back = escape || (!repeated && (key == 'X' || key == 'S'));
        if (pause_page == PauseOption::commands) return command_input(key, accept, back);
        if (pause_page && (back || (accept && *pause_page != PauseOption::options))) {
            pause_page.reset();
            return true;
        }
        if (escape) {
            practice_paused = false;
            practice_keys_previous = practice_keys_held;
            return true;
        }
        const bool training_page = pause_page == PauseOption::options;
        if (pause_page && !training_page) return false;
        auto& selected = training_page ? pause_training_cursor : pause_cursor;
        if (key == VK_UP || key == VK_DOWN) {
            const auto count = training_page ? pause_training_count() : pause_options.size();
            selected = (selected + (key == VK_UP ? count - 1 : 1)) % count;
            return true;
        }
        if (training_page && selected >= pause_training_options.size() &&
            (key == VK_LEFT || key == VK_RIGHT)) {
            auto& setting = training_options[selected - pause_training_options.size() + 1];
            const auto count = setting.choices.size();
            if (!count) return false;
            setting.choice = (setting.choice + (key == VK_RIGHT ? 1 : count - 1)) % count;
            if (setting.keyword == "OPT_STR_METERLEVEL")
                for (auto& fighter : practice_players) fighter.tension = static_cast<unsigned>(setting.choice);
            if (setting.keyword == "OPT_STR_LIFEBAR_LEVEL" ||
                (setting.keyword == "OPT_STR_LIFEBAR_SETTING" && setting.choice == 2)) {
                for (auto& stats : hud_stats) stats.reset_life(training_life());
                for (auto& team : practice_teams)
                    for (auto& health : team.health) health.reset_life(training_life());
            }
            return true;
        }
        if (!accept) return false;
        const auto option = static_cast<PauseOption>(pause_cursor);
        if (!training_page && option != PauseOption::resume && option != PauseOption::exit_training) {
            pause_page = option;
            pause_training_cursor = 0;
            command_page = CommandPage::menu;
            command_menu_cursor = command_character_cursor = command_move_cursor = 0;
            return true;
        }
        if (training_page && selected == 2) {
            pause_page.reset();
            return true;
        }
        if (training_page && selected >= pause_training_options.size()) return false;
        practice_paused = false;
        practice_keys_previous = practice_keys_held;
        if (training_page && selected == 0) reset_practice();
        if ((training_page && selected == 1) || option == PauseOption::exit_training) {
            if (!training_page) {
                practice_palette_lookups.clear();
                practice_palette_cache.clear();
                practice_variant_bytes=0;
                staged_fighters = {};
                staged_colors = {};
            }
            practice_background.reset();
            practice_scene.reset();
            if (audio) { audio->stop_combat(); audio->music(training_page ? "select_intro" : "menu_intro", training_page ? "select_loop" : "menu_loop"); }
            if (training_page) enter_stage_select();
            else training = TrainingView::none;
        }
        pause_page.reset();
        return true;
    }

    bool practice_facing_right(std::size_t side) const {
        const float x = practice_players[side].practice_x;
        const float other = practice_players[1 - side].practice_x;
        return x < other || (x == other && side == 0);
    }

    void separate_practice_fighters(bool player_one_was_left, float left, float right) {
        using Phase = PracticeFighter::ThrowPhase;
        const auto push_box = [&](std::size_t side) -> std::optional<PracticePushBox> {
            const auto& fighter = practice_players[side];
            if (practice_point_snapped(side) || fighter.throw_phase == Phase::grabbed ||
                fighter.fenrir_captured || fighter.ground_tech_direction)
                return std::nullopt;
            const auto& boxes = practice_metadata(fighter)[fighter.sprite() - 1].box_groups[2];
            if (boxes.empty()) return std::nullopt;
            const bool facing_right = side == 0 ? player_one_was_left : !player_one_was_left;
            return PracticePushBox{fighter.practice_x, skullgirls::foit_world_box(boxes.front(),
                static_cast<int>(fighter.practice_x), static_cast<int>(fighter.jump_height),
                !fighter.mirror(facing_right))};
        };
        const std::array current{push_box(0), push_box(1)};
        const auto previous = std::exchange(previous_push_boxes, current);
        if (!current[0] || !current[1]) return;
        const auto& start = previous[0] && previous[1] ? previous : current;
        for (std::size_t side = 0; side < current.size(); ++side) {
            const auto other = 1 - side;
            const auto& from = start[side]->edges;
            const auto& to = current[side]->edges;
            if ((from[1] > start[other]->edges[0] && to[1] > current[other]->edges[0]) ||
                (from[2] > start[other]->edges[3] && to[2] > current[other]->edges[3])) return;
        }

        // FUN_0049d2b0 sweeps both rectangles from their previous positions.
        // Choose the side order when the boxes first share a vertical span.
        std::int64_t steps = 1;
        for (std::size_t side = 0; side < current.size(); ++side)
            for (std::size_t edge : {2u, 3u})
                steps = std::max(steps, std::abs(current[side]->edges[edge] - start[side]->edges[edge]));

        std::array<float, 2> correction{};
        std::optional<std::size_t> left_side;
        float allowed_overlap = 0;
        for (std::int64_t step = 1; step <= steps; ++step) {
            const float progress = static_cast<float>(step) / static_cast<float>(steps);
            const auto interpolate = [progress](float from, float to) {
                return from + (to - from) * progress;
            };
            std::array<std::array<float, 4>, 2> boxes{};
            std::array<float, 2> x{};
            for (std::size_t side = 0; side < current.size(); ++side) {
                x[side] = interpolate(start[side]->x, current[side]->x) + correction[side];
                for (std::size_t edge = 0; edge < boxes[side].size(); ++edge)
                    boxes[side][edge] = interpolate(static_cast<float>(start[side]->edges[edge]),
                        static_cast<float>(current[side]->edges[edge]));
                boxes[side][2] += correction[side];
                boxes[side][3] += correction[side];
            }
            if (boxes[0][0] < boxes[1][1] || boxes[1][0] < boxes[0][1]) continue;

            const auto first = left_side.value_or(x[0] <= x[1] ? 0u : 1u);
            const auto second = 1u - first;
            const float overlap = boxes[first][3] - boxes[second][2];
            if (!left_side) {
                left_side = first;
                const bool both_in_hit_stop = practice_players[0].hit_stop && practice_players[1].hit_stop;
                allowed_overlap = std::max(0.0f, overlap) * (both_in_hit_stop ? 1.0f : 0.5f);
            }
            const float excess = overlap - allowed_overlap;
            if (excess <= 0) continue;
            correction[first] -= excess * 0.5f;
            correction[second] += excess * 0.5f;
        }

        for (std::size_t side = 0; side < current.size(); ++side) {
            auto& x = practice_players[side].practice_x;
            const auto displacement = static_cast<int>(x + correction[side]) - static_cast<int>(x);
            x = std::clamp(x + displacement, left, right);
            previous_push_boxes[side] = push_box(side);
        }
    }

    std::optional<Gdiplus::PointF> practice_attack_overlap(unsigned sprite, float x, float y, bool facing,
                                                        const PracticeFighter& defender, bool defender_facing,
                                                        bool cerebella_attack = false, bool peacock_attack = false, bool parasoul_attack = false, bool msfortune_attack = false, bool painwheel_attack = false, bool robofortune_attack = false, bool valentine_attack = false, bool squigly_attack = false, bool fukua_attack = false) const {
        const auto& hurts = practice_metadata(defender)[defender.sprite() - 1].box_groups[1];
        const auto& source = squigly_attack ? squigly_assets.sprites : fukua_attack ? fukua_assets.sprites : valentine_attack ? valentine_assets.sprites : robofortune_attack ? robofortune_assets.sprites : painwheel_attack ? painwheel_assets.sprites : msfortune_attack ? msfortune_assets.sprites : parasoul_attack ? parasoul_assets.sprites : peacock_attack ? peacock_assets.sprites : cerebella_attack ? cerebella_assets.sprites : combat_metadata;
        return practice_box_contact(source[sprite - 1].box_groups[0], x, y, facing, hurts,
            defender.practice_x, defender.jump_height, defender_facing);
    }

    static std::optional<Gdiplus::PointF> practice_box_contact(const std::vector<skullgirls::FoitBox>& hits,
        float x, float y, bool facing, const std::vector<skullgirls::FoitBox>& hurts,
        float target_x, float target_y, bool target_facing) {
        for (const auto& hit : hits) {
            const auto attack = skullgirls::foit_world_box(hit, static_cast<int>(x), static_cast<int>(y), facing);
            for (const auto& hurt : hurts) {
                const auto target = skullgirls::foit_world_box(hurt, static_cast<int>(target_x), static_cast<int>(target_y), target_facing);
                if (skullgirls::foit_boxes_overlap(attack, target)) return contact_center(attack, target);
            }
        }
        return {};
    }

    void resolve_fortune_head_strike(std::size_t side) {
        auto& fighter = practice_players[side];
        const auto target_side = 1 - side;
        if (!practice_players[target_side].msfortune || fighter.hit_stop || fighter.hurt_group >= 0 ||
            fighter.blockstun || fighter.fenrir_captured || fighter.throw_phase != PracticeFighter::ThrowPhase::none) return;
        const auto* actor = fighter.scripted();
        unsigned damage = 0, stun = 0, stop = 0;
        const void* descriptor = nullptr;
        const bool facing = actor ? actor->facing_right : fighter.normal_facing_right;
        if (actor && fighter.scripted_attacking() && actor->hit && !fighter.scripted_grab_successor()) {
            descriptor = actor->hit;
            damage = actor->hit->damage;
            stun = actor->hit->stun;
            stop = actor->hit->stop;
        } else if (!actor && fighter.normal_hit && fighter.striking_normal >= 0) {
            descriptor = fighter.normal_hit;
            const auto contact = fighter.normal_hit - skullgirls::filia_normal_hits[fighter.striking_normal];
            damage = skullgirls::filia_hit_damage[fighter.striking_normal][contact];
            stun = fighter.normal_hit->stun;
            stop = fighter.normal_hit->stop;
        }
        if (!descriptor || !damage ||
            (fighter.last_head_hit == descriptor && fighter.last_head_hit_instance == fighter.attack_instance)) return;

        const auto sprite = fighter.sprite();
        const auto& metadata = practice_metadata(fighter);
        if (sprite <= 1 || sprite > metadata.size()) return;
        for (auto& head : practice_effects) {
            if (!head.persistent_owner || !head.alive || head.side != target_side ||
                head.palette_slot != practice_teams[target_side].active_slot || head.hit_stop ||
                head.move <= 3 || head.value(0xa4) == 0 || head.pose <= 1 || head.pose > msfortune_assets.sprites.size()) continue;
            const auto impact = practice_box_contact(metadata[sprite - 1].box_groups[0], fighter.practice_x, fighter.jump_height,
                facing, msfortune_assets.sprites[head.pose - 1].box_groups[1], head.x, head.y, head.facing_right);
            if (!impact) continue;
            fighter.last_head_hit = descriptor;
            fighter.last_head_hit_instance = fighter.attack_instance;
            head.hit = nullptr;
            head.start(61); // Hit_ByEnemy
            head.hit_stop = stop;
            head.state.variables[0x35] = float(std::max(7u, stun));
            head.state.variables[0x54] = float(damage);
            head.state.variables[0x38] = facing ? -1.0f : 1.0f;
            if (head.captured_target) head.capture_state.variables[0xb0] = 1.0f;

            // The head's script transfers 55 percent of strike damage without a knockout.
            const auto life = hud_stats[target_side].life;
            if (life > 100) record_practice_damage(side, std::min(life - 100, damage * 55 / 100));
            spawn_combat_spark(side, CombatEffect::medium, *impact);
            break;
        }
    }

    void resolve_practice_bursts() {
        std::array<bool, 2> contacts{};
        for (std::size_t side = 0; side < contacts.size(); ++side) {
            const auto& fighter = practice_players[side];
            const auto& target = practice_players[1 - side];
            if (practice_point_snapped(1 - side) || !fighter.burst_can_hit() ||
                target.burst_protected() || target.ground_tech_direction ||
                target.fenrir_captured || target.throw_phase == PracticeFighter::ThrowPhase::grabbed ||
                !practice_slot_playable(1 - side, practice_teams[1 - side].active_slot)) continue;
            contacts[side] = practice_attack_overlap(fighter.sprite(), fighter.practice_x,
                fighter.jump_height, fighter.hurt_facing_right, target,
                !target.mirror(practice_facing_right(1 - side)), fighter.cerebella.has_value(), fighter.peacock.has_value(), fighter.parasoul.has_value(), fighter.msfortune.has_value(), fighter.painwheel.has_value(), fighter.robofortune.has_value(),fighter.valentine.has_value(),fighter.squigly.has_value(),fighter.fukua.has_value()).has_value();
        }
        for (std::size_t side = 0; side < contacts.size(); ++side) {
            if (!contacts[side]) continue;
            auto& fighter = practice_players[side];
            auto& target = practice_players[1 - side];
            constexpr skullgirls::NormalHit burst_hit{7, 36, 9, 3, 16};
            fighter.burst_connected = true;
            if (fighter.scripted()) fighter.scripted()->hit_resolved = true;
            fighter.hit_stop = std::max(fighter.hit_stop, burst_hit.stop);
            target.take_hit(burst_hit, practice_facing_right(1 - side), -1,
                {skullgirls::HeavyReaction::knockback, 24, 18});
        }
    }

    static RingletSpike::Spin ringlet_hit_spin(const PracticeFighter& target) {
        if (target.jump_height || target.jump_velocity) return RingletSpike::Spin::none;
        if (target.hurt_group >= 0 && target.recoil_landed && !target.waking_up) return RingletSpike::Spin::otg;
        return RingletSpike::Spin::standard;
    }

    // Run the reserve fighter's entry, attack, and exit alongside the active fighter.
    void advance_practice_assists(float left, float right) {
        using Phase = PracticeAssist::Phase;
        for (std::size_t side = 0; side < practice_teams.size(); ++side) {
            auto& team = practice_teams[side];
            if (!team.assist) continue;
            auto& assist = *team.assist;
            auto& fighter = assist.fighter;
            auto& opponent = practice_players[1 - side];
            auto& actor = assist.actor;
            auto& life = team.health[actor.slot].life;
            if (assist.phase == Phase::leaving) {
                if (actor.travel.advance_exit(practice_team_gate(actor.home_on_left), actor.home_on_left)) {
                    team.assist.reset();
                    team.assist_cooldown = 45;
                }
                continue;
            }
            if (assist.phase == Phase::entering) {
                const float spacing = actor.home_on_left ? 70.0f : -70.0f;
                const float target = actor.travel.landing_ticks ? actor.travel.x :
                    std::clamp(practice_players[side].practice_x + spacing, left, right);
                if (!actor.travel.advance(target)) continue;
                fighter.practice_x = actor.travel.x;
                fighter.normal_facing_right = fighter.practice_x == opponent.practice_x ? side == 0 :
                    fighter.practice_x < opponent.practice_x;
                if (fighter.cerebella) {
                    fighter.cerebella->facing_right = fighter.normal_facing_right;
                    fighter.cerebella->start(static_cast<unsigned>(assist.command.move));
                    fighter.active_normal = assist.command.move;
                } else if (fighter.peacock) {
                    fighter.peacock->facing_right = fighter.normal_facing_right;
                    fighter.peacock->start(static_cast<unsigned>(assist.command.move));
                    fighter.active_normal = assist.command.move;
                } else if (fighter.painwheel) {
                    fighter.painwheel->facing_right = fighter.normal_facing_right;
                    if (fighter.painwheel->move == 20) fighter.painwheel->animate(painwheel_assets);
                    fighter.painwheel->start(static_cast<unsigned>(assist.command.move));
                    fighter.active_normal = assist.command.move;
                } else if (fighter.squigly) {
                    fighter.squigly->facing_right = fighter.normal_facing_right;
                    fighter.squigly->start(static_cast<unsigned>(assist.command.move));
                    fighter.active_normal = assist.command.move;
                } else if (fighter.fukua) {
                    fighter.fukua->facing_right = fighter.normal_facing_right;
                    fighter.fukua->start(static_cast<unsigned>(assist.command.move));
                    fighter.active_normal = assist.command.move;
                } else if (fighter.valentine) {
                    fighter.valentine->facing_right = fighter.normal_facing_right;
                    fighter.valentine->start(static_cast<unsigned>(assist.command.move));
                    fighter.active_normal = assist.command.move;
                }
            if (fighter.parasoul) {
                    fighter.parasoul->facing_right = fighter.normal_facing_right;
                    fighter.parasoul->start(static_cast<unsigned>(assist.command.move));
                    fighter.active_normal = assist.command.move;
                } else if (fighter.robofortune) {
                    fighter.robofortune->facing_right = fighter.normal_facing_right;
                    fighter.robofortune->start(static_cast<unsigned>(assist.command.move));
                    fighter.active_normal = assist.command.move;
                } else if (fighter.msfortune) {
                    fighter.msfortune->facing_right = fighter.normal_facing_right;
                    fighter.msfortune->start(static_cast<unsigned>(assist.command.move));
                    fighter.active_normal = assist.command.move;
                } else fighter.active_normal = fighter.prepare_special(assist.command, false);
                assist.phase = Phase::acting;
            }
            auto& spikes = fighter.ringlet_spikes;
            spikes.erase(std::remove_if(spikes.begin(), spikes.end(),
                [](RingletSpike& spike) { return !spike.advance(); }), spikes.end());
            if (fighter.cerebella) tick_cerebella(fighter, opponent.practice_x, left, right, side == 0);
            else if (fighter.peacock) tick_peacock(fighter, opponent.practice_x, left, right, side == 0);
            else if (fighter.robofortune) tick_robofortune(fighter, opponent.practice_x, left, right, side == 0);
            else if (fighter.painwheel) tick_painwheel(fighter, opponent.practice_x, left, right, side == 0);
            else if (fighter.squigly) tick_squigly(fighter, opponent.practice_x, left, right, side == 0);
            else if (fighter.fukua) tick_fukua(fighter, opponent.practice_x, left, right, side == 0);
            else if (fighter.valentine) tick_valentine(fighter, opponent.practice_x, left, right, side == 0);
            else if (fighter.parasoul) tick_parasoul(fighter, opponent.practice_x, left, right, side == 0);
            else if (fighter.msfortune) tick_msfortune(fighter, opponent.practice_x, left, right, side == 0);
            else fighter.tick(opponent.practice_x, left, right, side == 0, practice_ceiling(), filia_scripts());
            const auto outward_facing = !fighter.mirror(fighter.practice_x <= opponent.practice_x);
            if (fighter.hurt_group < 0 && opponent.normal_hit && opponent.striking_normal >= 0 &&
                opponent.movement_sprite >= 198) {
                if (const auto impact = practice_attack_overlap(opponent.movement_sprite, opponent.practice_x,
                    opponent.jump_height, opponent.normal_facing_right, fighter, outward_facing)) {
                    const auto move = opponent.striking_normal;
                    const auto hit_index = opponent.normal_hit - skullgirls::filia_normal_hits[move];
                    const auto damage = skullgirls::filia_hit_damage[move][hit_index];
                    if (training_value("OPT_STR_LIFEBAR_SETTING") != 2) {
                        const unsigned minimum = !training_value("OPT_STR_DEATHALLOWED");
                        life = std::max(minimum, life - std::min(life, damage));
                    }
                    fighter.take_hit(*opponent.normal_hit, !opponent.normal_facing_right, move);
                    spawn_combat_spark(1 - side, CombatEffect::medium, *impact);
                }
            }
            if (!practice_point_snapped(1 - side) && !opponent.burst_protected() &&
                !opponent.ground_tech_direction && !opponent.fenrir_captured &&
                opponent.throw_phase != PracticeFighter::ThrowPhase::grabbed &&
                practice_slot_playable(1 - side, practice_teams[1 - side].active_slot)) {
                const bool facing = !opponent.mirror(practice_facing_right(1 - side));
                const auto strike = [&](const skullgirls::NormalHit& hit, int move, unsigned damage,
                                        float origin_x, bool forward, Gdiplus::PointF impact) {
                    if (practice_guard(side, hit, origin_x >= opponent.practice_x)) {
                        spawn_combat_spark(side, CombatEffect::block, impact);
                        return false;
                    }
                    opponent.take_hit(hit, !forward, move, practice_hit_reaction(side,
                        move == skullgirls::ringlet_spike ? skullgirls::ringlet_spike_reaction :
                            skullgirls::HeavyReaction{}));
                    record_practice_damage(side, damage, move, fighter.attack_instance);
                    spawn_combat_spark(side, CombatEffect::medium, impact);
                    return true;
                };
                if (fighter.cerebella && fighter.cerebella->attacking() && fighter.cerebella->hit &&
                    !fighter.cerebella->hit_resolved) {
                    auto& bella = *fighter.cerebella;
                    if (const auto impact = practice_attack_overlap(bella.pose, fighter.practice_x, fighter.jump_height,
                        bella.facing_right, opponent, facing, true)) {
                        const auto& source = *bella.hit;
                        const skullgirls::NormalHit descriptor{0, source.stun, source.stop, source.height & 3u};
                        const bool connected = strike(descriptor, -1, source.damage, fighter.practice_x,
                                                      bella.facing_right, *impact);
                        bella.report_contact(!connected);
                    }
                }
                if (fighter.peacock && fighter.peacock->attacking() && fighter.peacock->hit && !fighter.peacock->hit_resolved) {
                    auto& actor = *fighter.peacock;
                    if (const auto impact = practice_attack_overlap(actor.pose, fighter.practice_x, fighter.jump_height,
                        actor.facing_right, opponent, facing, false, true)) {
                        const auto& source = *actor.hit;
                        const skullgirls::NormalHit descriptor{0, source.stun, source.stop, source.height & 3u};
                        fighter.hit_stop = std::max(fighter.hit_stop, unsigned(source.stop));
                        const bool connected = strike(descriptor, -1, source.damage, fighter.practice_x, actor.facing_right, *impact);
                        actor.report_contact(!connected);
                    }
                }
                if (fighter.painwheel && fighter.painwheel->attacking() && fighter.painwheel->hit && !fighter.painwheel->hit_resolved) {
                    auto& actor = *fighter.painwheel;
                    if (const auto impact = practice_attack_overlap(actor.pose, fighter.practice_x, fighter.jump_height,
                        actor.facing_right, opponent, facing, false, false, false, false, true)) {
                        const auto& source = *actor.hit;
                        const skullgirls::NormalHit descriptor{0, source.stun, source.stop, source.height & 3u};
                        fighter.hit_stop = std::max(fighter.hit_stop, unsigned(source.stop));
                        const bool connected = strike(descriptor, -1, source.damage, fighter.practice_x, actor.facing_right, *impact);
                        actor.report_contact(!connected);
                    }
                }
                if (fighter.squigly && fighter.squigly->attacking() && fighter.squigly->hit && !fighter.squigly->hit_resolved) {
                    auto& actor = *fighter.squigly;
                    if (const auto impact = practice_attack_overlap(actor.pose, fighter.practice_x, fighter.jump_height,
                        actor.facing_right, opponent, facing, false, false, false, false, false, false, false, true)) {
                        const auto& source = *actor.hit;
                        const skullgirls::NormalHit descriptor{0, source.stun, source.stop, source.height & 3u};
                        fighter.hit_stop = std::max(fighter.hit_stop, unsigned(source.stop));
                        const bool connected = strike(descriptor, -1, source.damage, fighter.practice_x, actor.facing_right, *impact);
                        actor.report_contact(!connected);
                    }
                }
                if (fighter.fukua && fighter.fukua->attacking() && fighter.fukua->hit && !fighter.fukua->hit_resolved) {
                    auto& actor = *fighter.fukua;
                    if (const auto impact = practice_attack_overlap(actor.pose, fighter.practice_x, fighter.jump_height,
                        actor.facing_right, opponent, facing, false, false, false, false, false, false, false, false, true)) {
                        const auto& source = *actor.hit;
                        const skullgirls::NormalHit descriptor{0, source.stun, source.stop, source.height & 3u};
                        fighter.hit_stop = std::max(fighter.hit_stop, unsigned(source.stop));
                        const bool connected = strike(descriptor, -1, source.damage, fighter.practice_x, actor.facing_right, *impact);
                        actor.report_contact(!connected);
                    }
                }
                if (fighter.valentine && fighter.valentine->attacking() && fighter.valentine->hit && !fighter.valentine->hit_resolved) {
                    auto& actor = *fighter.valentine;
                    if (const auto impact = practice_attack_overlap(actor.pose, fighter.practice_x, fighter.jump_height,
                        actor.facing_right, opponent, facing, false, false, false, false, false, false, true)) {
                        const auto& source = *actor.hit;
                        const skullgirls::NormalHit descriptor{0, source.stun, source.stop, source.height & 3u};
                        fighter.hit_stop = std::max(fighter.hit_stop, unsigned(source.stop));
                        const bool connected = strike(descriptor, -1, source.damage, fighter.practice_x, actor.facing_right, *impact);
                        actor.report_contact(!connected);
                    }
                }
                if (fighter.parasoul && fighter.parasoul->attacking() && fighter.parasoul->hit && !fighter.parasoul->hit_resolved) {
                    auto& actor = *fighter.parasoul;
                    if (const auto impact = practice_attack_overlap(actor.pose, fighter.practice_x, fighter.jump_height,
                        actor.facing_right, opponent, facing, false, false, true)) {
                        const auto& source = *actor.hit;
                        const skullgirls::NormalHit descriptor{0, source.stun, source.stop, source.height & 3u};
                        fighter.hit_stop = std::max(fighter.hit_stop, unsigned(source.stop));
                        const bool connected = strike(descriptor, -1, source.damage, fighter.practice_x, actor.facing_right, *impact);
                        actor.report_contact(!connected);
                    }
                }
                if (fighter.msfortune && fighter.msfortune->attacking() && fighter.msfortune->hit && !fighter.msfortune->hit_resolved) {
                    auto& actor = *fighter.msfortune;
                    if (const auto impact = practice_attack_overlap(actor.pose, fighter.practice_x, fighter.jump_height,
                        actor.facing_right, opponent, facing, false, false, false, true)) {
                        const auto& source = *actor.hit;
                        const skullgirls::NormalHit descriptor{0, source.stun, source.stop, source.height & 3u};
                        fighter.hit_stop = std::max(fighter.hit_stop, unsigned(source.stop));
                        const bool connected = strike(descriptor, -1, source.damage, fighter.practice_x, actor.facing_right, *impact);
                        actor.report_contact(!connected);
                    }
                }
                if (fighter.normal_hit && fighter.normal_hit != fighter.last_contact && fighter.striking_normal >= 0 &&
                    fighter.movement_sprite >= 198) {
                    if (const auto impact = practice_attack_overlap(fighter.movement_sprite, fighter.practice_x,
                        fighter.jump_height, fighter.normal_facing_right, opponent, facing)) {
                        const auto move = fighter.striking_normal;
                        const auto hit_index = fighter.normal_hit - skullgirls::filia_normal_hits[move];
                        fighter.last_contact = fighter.normal_hit;
                        fighter.hit_stop = std::max(fighter.hit_stop, fighter.normal_hit->stop);
                        strike(*fighter.normal_hit, move, skullgirls::filia_hit_damage[move][hit_index],
                               fighter.practice_x, fighter.normal_facing_right, *impact);
                    }
                }
                for (auto& spike : spikes) {
                    if (!spike.can_hit()) continue;
                    if (const auto impact = practice_attack_overlap(spike.sprite(), spike.x, 0,
                        spike.facing_right, opponent, facing)) {
                        spike.hit = true;
                        const auto spin = ringlet_hit_spin(opponent);
                        if (strike(skullgirls::ringlet_spike_hit, skullgirls::ringlet_spike, 600,
                                   spike.x, spike.facing_right, *impact)) spike.spin = spin;
                    }
                }
            }
            if (fighter.hurt_group < 0 && !fighter.jump_height &&
                (!life || (fighter.active_normal < 0 && spikes.empty()))) {
                actor.travel = {fighter.practice_x, fighter.jump_height, !actor.home_on_left,
                    PracticeTeamTravel::Motion::assist_out};
                assist.phase = Phase::leaving;
            }
        }
    }

    void start_practice_intro() {
        const auto choice = training_value("PRACTICE_FILIA_INTRO");
        practice_presentation = {};
        practice_presentation.phase = PracticePresentation::Phase::intro;
        const unsigned cycle = practice_intro_cycle++;
        bool any = false;
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& fighter = practice_players[side];
            const auto& table = fighter.squigly ? static_cast<const skullgirls::FoitMoveTable&>(squigly_assets) : fighter.fukua ? static_cast<const skullgirls::FoitMoveTable&>(fukua_assets) : fighter.valentine ? static_cast<const skullgirls::FoitMoveTable&>(valentine_assets) : fighter.robofortune ? static_cast<const skullgirls::FoitMoveTable&>(robofortune_assets) : fighter.painwheel ? static_cast<const skullgirls::FoitMoveTable&>(painwheel_assets) : fighter.msfortune ? msfortune_assets : fighter.parasoul ? parasoul_assets :
                fighter.peacock ? peacock_assets : fighter.cerebella ? cerebella_assets : filia_move_table;
            if (table.animations.empty() || (!fighter.scripted() && choice == 4)) continue;
            // Select a variant, then let its authored successors and done signal
            // control the timeline. No duration or move-number cutoffs.
            std::vector<unsigned> entries;
            for (unsigned m = 0; m < table.names.size(); ++m)
                if (table.names[m].size() >= 8 && table.names[m].compare(table.names[m].size() - 8, 8, "_ForReal") == 0 &&
                    table.names[m].compare(0, 6, "Intro_") == 0) entries.push_back(m);
            if (entries.empty()) for (unsigned m=0;m<table.names.size();++m)
                if (table.names[m].compare(0,6,"Intro_")==0 && table.names[m].find('_',6)==std::string::npos) entries.push_back(m);
            if (entries.empty()) continue;
            auto& actor = practice_presentation.actors[side];
            actor.facing_right = practice_facing_right(side);
            actor.variant = !fighter.scripted() && choice ? unsigned(choice - 1) : (cycle + side) % entries.size();
            auto& playback = actor.intro.emplace();
            playback.root = &table;
            playback.initialize(table);
            const auto& enabled = fighter.scripted() ? fighter.scripted()->state : skullgirls::CerebellaPlayback::defaults();
            for (unsigned i = 0; i < playback.state.variables.size(); ++i)
                playback.state.variables[i] = enabled.variables[i].value_or(0.0f);
            for (auto& bit : playback.state.bits) bit = false;
            playback.state.bits[20] = true;
            playback.state.variables[0x19] = float(side + 1);
            playback.state.variables[0x17] = playback.state.variables[0x38] = actor.facing_right ? 1.0f : -1.0f;
            playback.state.variables[0x1b] = playback.state.variables[0x05] = 1.0f;
            playback.state.variables[0x87] = 2.0f;
            playback.state.variables[0xa4] = playback.state.variables[0xa5] = playback.state.variables[0xa6] = 1.0f;
            playback.defaults = playback.state;
            playback.authored_timeline = true;
            playback.facing_right = actor.facing_right;
            playback.x = fighter.practice_x;
            playback.start(entries[actor.variant % entries.size()]);
            any = true;
        }
        if (!any) practice_presentation = {};
    }

    void finish_practice_intro() {
        // Round initialization also covers skipping in the middle of an intro:
        // no invisible proxy, temporary headless mode or child handshake leaks
        // into controllable gameplay.
        practice_effects.clear();
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& fighter = practice_players[side];
            const bool facing = practice_facing_right(side);
            if (fighter.cerebella) { fighter.cerebella.emplace().facing_right = facing; fighter.cerebella->animate(cerebella_assets); }
            if (fighter.peacock) { fighter.peacock.emplace().facing_right = facing; fighter.peacock->animate(peacock_assets); }
            if (fighter.squigly) { fighter.squigly.emplace().facing_right = facing; fighter.squigly->animate(squigly_assets); }
            if (fighter.fukua) { fighter.fukua.emplace().facing_right = facing; fighter.fukua->animate(fukua_assets); }
            if (fighter.valentine) { fighter.valentine.emplace().facing_right = facing; fighter.valentine->animate(valentine_assets); }
            if (fighter.parasoul) { fighter.parasoul.emplace().facing_right = facing; fighter.parasoul->animate(parasoul_assets); }
            if (fighter.painwheel) { fighter.painwheel.emplace().facing_right = facing; fighter.painwheel->animate(painwheel_assets); }
            if (fighter.robofortune) { fighter.robofortune.emplace().facing_right = facing; fighter.robofortune->animate(robofortune_assets); }
            if (fighter.msfortune) { fighter.msfortune.emplace().facing_right = facing; fighter.msfortune->animate(msfortune_assets); }
            fighter.jump_height = fighter.jump_velocity = 0;
            fighter.script_state = {};
            fighter.visual = {};
            fighter.idle_tick = 0;
        }
        practice_presentation = {};
        practice_attack_edges = {};
        practice_keys_previous = practice_keys_held;
    }

    bool begin_practice_knockout() {
        using Phase = PracticePresentation::Phase;
        using Reaction = skullgirls::HeavyReaction;
        auto& scene = practice_presentation;
        for (std::size_t side = 0; side < hud_stats.size(); ++side)
            scene.defeated[side] = !hud_stats[side].life && !next_practice_reserve(side);
        if (!scene.defeated[0] && !scene.defeated[1]) return false;

        scene.phase = Phase::knockout;
        scene.tick = 0;
        dummy_armed = false;
        dummy_recording.reset();
        dummy_playing.reset();
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& fighter = practice_players[side];
            fighter_visuals(fighter).freeze = 0;
            auto& team = practice_teams[side];
            auto& stats = hud_stats[side];
            stats.combo = false;
            stats.notice_ticks = 0;
            team.assist.reset();
            team.tag.reset();
            team.departing.reset();
            fighter.burst_available = false;
            fighter.throw_input = false;
            fighter.normal_input = fighter.queued_followup = -1;
            fighter.pressed_punches = fighter.pressed_kicks = fighter.held_attack_buttons = 0;
            fighter.practice_left = fighter.practice_right = fighter.practice_down = fighter.practice_up = false;
            fighter.buffered_special.reset();
            if (!scene.defeated[side]) continue;
            if (fighter.hurt_group < 0 || fighter.heavy_reaction == Reaction::none ||
                fighter.heavy_reaction == Reaction::air_launch || fighter.fenrir_captured ||
                fighter.throw_phase != PracticeFighter::ThrowPhase::none) {
                constexpr skullgirls::NormalHit knockout_hit{0, 0, 0, 3};
                fighter.take_hit(knockout_hit, practice_facing_right(side), -1, {Reaction::sweep, 8, 14});
            }
        }
        return true;
    }

    // Advance intros, knockout recovery, or result animations while regular combat is paused.
    void advance_practice_presentation() {
        using namespace skullgirls;
        using Phase = PracticePresentation::Phase;
        auto& scene = practice_presentation;
        const unsigned tick = scene.tick++;
        ++practice_stage_tick;
        if (scene.phase == Phase::intro) {
            bool finished = true;
            for (std::size_t side = 0; side < scene.actors.size(); ++side) {
                auto& actor = scene.actors[side];
                if (!actor.intro) continue;
                auto& playback = *actor.intro;
                playback.opponent = effect_parent_state(practice_players[1 - side]);
                playback.advance({}, true);
                actor.pose = playback.pose;
                actor.facing_right = playback.facing_right;
                if (playback.value(0x87) != 0) finished = false;
            }
            if (finished || practice_attack_edges[0] || practice_attack_edges[1]) finish_practice_intro();
            return;
        }
        if (scene.phase == Phase::knockout) {
            bool settled = true;
            for (std::size_t side = 0; side < practice_players.size(); ++side) {
                if (!practice_slot_is_filia(side, practice_teams[side].active_slot)) continue;
                auto& actor = scene.actors[side];
                auto& fighter = practice_players[side];
                if (scene.defeated[side] && !actor.pose && !fighter.hit_stop &&
                    !fighter.jump_height && (fighter.recoil_landed || fighter.hurt_group < 0)) {
                    actor.pose = fighter.heavy_reaction == HeavyReaction::back_knockdown ? 751 : 756;
                    actor.facing_right = fighter.hurt_facing_right;
                }
                if (scene.defeated[side] ? actor.pose != 0 :
                    !fighter.jump_height && !fighter.hit_stop && fighter.active_normal < 0 && fighter.hurt_group < 0 &&
                        fighter.throw_phase == PracticeFighter::ThrowPhase::none) continue;
                settled = false;
                fighter.tick(practice_players[1 - side].practice_x, 0, practice_width, side == 0,
                             practice_ceiling(), filia_scripts());
            }
            if ((!settled || tick < 44) && tick < 119) return;

            scene.phase = Phase::results;
            scene.tick = 0;
            const auto choice = training_value("PRACTICE_FILIA_WIN");
            const unsigned cycle = practice_win_cycle++;
            for (std::size_t side = 0; side < practice_players.size(); ++side) {
                auto& actor = scene.actors[side];
                auto& fighter = practice_players[side];
                fighter.ringlet_spikes.clear();
                fighter.tricobezoar.reset();
                fighter.jump_height = 0;
                if (fighter.cerebella) {
                    fighter.cerebella->facing_right = practice_facing_right(side);
                    fighter.cerebella->start(scene.defeated[side] ? 545 : 541);
                    fighter.cerebella->animate(cerebella_assets);
                    continue;
                }
                if (fighter.peacock) {
                    fighter.peacock->facing_right = practice_facing_right(side);
                    fighter.peacock->start(scene.defeated[side] ? 529 : 521);
                    fighter.peacock->animate(peacock_assets);
                    continue;
                }
                if (fighter.squigly) {
                    fighter.squigly->facing_right = practice_facing_right(side);
                    fighter.squigly->start(scene.defeated[side] ? 609 : 605);
                    fighter.squigly->animate(squigly_assets);
                    continue;
                }
            if (fighter.fukua) {
                    fighter.fukua->facing_right = practice_facing_right(side);
                    fighter.fukua->start(scene.defeated[side] ? 507 : 506);
                    fighter.fukua->animate(fukua_assets);
                    continue;
                }
            if (fighter.valentine) {
                    fighter.valentine->facing_right = practice_facing_right(side);
                    fighter.valentine->start(scene.defeated[side] ? 569 : 565);
                    fighter.valentine->animate(valentine_assets);
                    continue;
                }
            if (fighter.parasoul) {
                    fighter.parasoul->facing_right = practice_facing_right(side);
                    fighter.parasoul->start(scene.defeated[side] ? 551 : 547);
                    fighter.parasoul->animate(parasoul_assets);
                    continue;
                }
                if (fighter.painwheel) {
                    fighter.painwheel->facing_right = practice_facing_right(side);
                    fighter.painwheel->start(scene.defeated[side] ? 559 : 553);
                    fighter.painwheel->animate(painwheel_assets);
                    continue;
                }
                if (fighter.robofortune) {
                    fighter.robofortune->facing_right = practice_facing_right(side);
                    fighter.robofortune->start(scene.defeated[side]?523u:519u);
                    fighter.robofortune->animate(robofortune_assets);
                    continue;
                }
                if (fighter.msfortune) {
                    fighter.msfortune->facing_right = practice_facing_right(side);
                    fighter.msfortune->start(scene.defeated[side] ? 527 : 520);
                    fighter.msfortune->animate(msfortune_assets);
                    continue;
                }
                if (!practice_slot_is_filia(side, practice_teams[side].active_slot)) continue;
                actor.variant = choice ? static_cast<unsigned>(choice - 1) : cycle % 3;
                actor.facing_right = practice_facing_right(side);
                actor.starts_facing_right = !fighter.mirror(actor.facing_right);
                actor.crouching = !scene.defeated[side] && fighter.crouching;
                actor.initial_pose = scene.defeated[side] ? 1095 : fighter.sprite();
                actor.pose = actor.initial_pose;
            }
            return;
        }
        for (std::size_t side = 0; side < scene.actors.size(); ++side) {
            if (auto& peacock = practice_players[side].peacock) { peacock->animate(peacock_assets); continue; }
            if (auto& robo = practice_players[side].robofortune) { robo->animate(robofortune_assets); continue; }
            if (auto& painwheel = practice_players[side].painwheel) { painwheel->animate(painwheel_assets); continue; }
            if (auto& squigly = practice_players[side].squigly) { squigly->animate(squigly_assets); continue; }
            if (auto& fukua = practice_players[side].fukua) { fukua->animate(fukua_assets); continue; }
            if (auto& valentine = practice_players[side].valentine) { valentine->animate(valentine_assets); continue; }
            if (auto& parasoul = practice_players[side].parasoul) { parasoul->animate(parasoul_assets); continue; }
            if (auto& msfortune = practice_players[side].msfortune) { msfortune->animate(msfortune_assets); continue; }
            if (auto& bella = practice_players[side].cerebella) {
                bella->animate(cerebella_assets);
                continue;
            }
            auto& actor = scene.actors[side];
            if (!actor.pose) continue;
            unsigned elapsed = tick;
            const auto play_prefix = [&](DefenseClip clip) {
                if (elapsed < clip.duration()) {
                    actor.pose = clip.pose_at(elapsed, actor.initial_pose);
                    return true;
                }
                elapsed -= clip.duration();
                return false;
            };
            if (!scene.defeated[side]) {
                if (actor.starts_facing_right != actor.facing_right &&
                    play_prefix(actor.crouching ? DefenseClip(result_turn_crouched) : DefenseClip(result_turn)))
                    continue;
                if (actor.crouching && play_prefix(result_stand_up)) continue;
            }
            const auto& clips = scene.defeated[side] ? filia_loss : filia_wins[actor.variant];
            if (!play_prefix(clips.start))
                actor.pose = clips.loop.pose_at(elapsed % clips.loop.duration(), actor.pose);
        }
    }

    void tick_peacock(PracticeFighter& fighter, float opponent_x, float left, float right, bool tie_right) {
        auto& actor = *fighter.peacock;
        const auto& moves = peacock_assets;
        if (moves.animations.empty()) return;
        fighter.normal_hit = nullptr;
        fighter.striking_normal = -1;
        if (fighter.fenrir_captured || fighter.throw_phase == PracticeFighter::ThrowPhase::grabbed) return;
        const unsigned buttons = std::exchange(fighter.pressed_punches, 0u) |
            (std::exchange(fighter.pressed_kicks, 0u) << 3);
        fighter.throw_input = false;
        fighter.normal_input = -1;
        const int direction = int(fighter.practice_right) - int(fighter.practice_left);
        const int vertical = int(fighter.practice_up) - int(fighter.practice_down);
        const bool grounded = fighter.jump_height <= 0 && fighter.jump_velocity <= 0;
        const bool repeated = fighter.update_dash_input(direction, !grounded);
        const bool facing = fighter.practice_x < opponent_x || (fighter.practice_x == opponent_x && tie_right);
        if (grounded && actor.move <= 31) actor.facing_right = facing;
        actor.sample(skullgirls::filia_input_direction(direction, vertical), buttons,
            fighter.held_attack_buttons, grounded, fighter.tension);
        actor.state.variables[0x17] = facing ? 1.0f : -1.0f;
        actor.state.variables[0x38] = actor.facing_right ? 1.0f : -1.0f;
        actor.state.variables[0xa0] = fighter.practice_x;
        actor.state.variables[0xa1] = fighter.jump_height;
        actor.state.variables[0xad] = opponent_x;
        if (fighter.hit_stop) { --fighter.hit_stop; return; }
        if (fighter.burst_ticks || fighter.burst_fall_tick) {
            actor.animate(moves);
            actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
            if (fighter.burst_ticks) --fighter.burst_ticks;
            if (!fighter.burst_ticks && actor.move == 429) actor.start(fighter.jump_height > 0 ? 431 : 41);
            fighter.burst_fall_tick = actor.move >= 431 && actor.move <= 434 ? 1 : 0;
            if (actor.move < 429 || actor.move > 434) {
                fighter.burst_ticks = 0;
                fighter.burst_fall_tick = fighter.jump_height > 0 ? 1u : 0u;
                actor.start(fighter.burst_fall_tick ? 431u : 41u);
            }
            if (fighter.jump_height == 0 && fighter.jump_velocity <= 0 && fighter.burst_fall_tick) {
                fighter.burst_ticks = fighter.burst_fall_tick = 0;
                actor.start(41);
            }
            fighter.active_normal = -1;
            return;
        }
        if (fighter.blockstun || fighter.hurt_group >= 0) {
            const bool guard = fighter.blockstun != 0;
            const unsigned reaction = guard ? (grounded ? fighter.crouching ? 405 : 396 : 414) :
                !grounded ? 254 : fighter.crouching ? 248 :
                fighter.hurt_height == skullgirls::HurtHeight::high ? 232 :
                fighter.hurt_height == skullgirls::HurtHeight::low ? 240 : 236;
            if (!actor.reacting) { actor.start(reaction); actor.reacting = true; }
            actor.facing_right = fighter.hurt_facing_right;
            if (guard) --fighter.blockstun;
            else if (fighter.hurt_hold) --fighter.hurt_hold;
            if (!grounded) {
                fighter.jump_height = std::max(0.0f, fighter.jump_height + fighter.jump_velocity);
                fighter.jump_velocity = fighter.jump_height > 0 ? fighter.jump_velocity - actor.gravity : 0;
            }
            if (!grounded || !guard) {
                fighter.practice_x = std::clamp(fighter.practice_x + fighter.recoil_x, left, right);
                if (grounded) fighter.recoil_x = std::copysign(std::max(0.0f, std::abs(fighter.recoil_x) - 1.5f), fighter.recoil_x);
            }
            if (fighter.blockstun || fighter.hurt_hold) actor.animate(moves);
            else {
                fighter.guard_phase = PracticeFighter::GuardPhase::none;
                fighter.hurt_group = -1;
                actor.reacting = false;
                actor.start(grounded ? fighter.crouching ? 21 : 20 : 116);
                actor.state.variables[0x9e] = 0.0f;
                actor.state.variables[0x9f] = fighter.jump_velocity;
            }
            fighter.active_normal = -1;
            return;
        }
        fighter.crouching = grounded && fighter.practice_down;
        const int relative = actor.facing_right ? direction : -direction;
        const auto old = actor.move;
        const bool teching = fighter.throw_phase == PracticeFighter::ThrowPhase::tech;
        // Authored links handle commands and contact-dependent cancels.
        const auto oldest = actor.attacking() && actor.age ? actor.age - 1 : 63;
        auto next = actor.transition(moves, old, [&](unsigned target) {
            return teching ? target == 156 || target == 164 || target == 220 || target == 224 : target >= 20 && target <= 219;
        }, oldest);
        if (next == actor.normals)
            next = actor.transition(moves, actor.normals, [](unsigned target) { return target >= 120 && target <= 158; });
        const bool free_ground = grounded && old >= 20 && old <= 47;
        if (free_ground && next < actor.first_attack) {
            const auto punches = (buttons | fighter.held_attack_buttons) & 7u;
            if (repeated || ((buttons & 7u) && (punches & (punches - 1)) && !(buttons & 56u))) {
                if (old <= 38) next = relative < 0 ? 39 : 36;
            } else if (fighter.practice_up && (old <= 31 || (old == 36 && relative > 0))) {
                next = skullgirls::filia_super_jump(fighter.dash_input_history) ?
                    (relative > 0 ? 54 : relative < 0 ? 56 : 55) : (old == 36 ? 53 : 52);
            } else if (next == 32) next = relative < 0 ? 39 : 36;
        }
        if (next != old && next != actor.normals) {
            const unsigned cost = next == 208 ? 3 : next == 201 || next == 207 || (next >= 215 && next <= 219) ? 1 : 0;
            if (fighter.tension >= cost) {
                fighter.tension -= cost;
                if (next >= 215 && next <= 218) fighter.snapback_reserve = next >= 217 ? 1 : 0;
                actor.start(next);
                if (actor.attacking()) ++fighter.attack_instance;
            }
        }
        if (actor.move >= 20 && actor.move <= 25 && grounded)
            actor.state.variables[0x9e] = actor.state.variables[0x9f] = 0.0f;
        actor.animate(moves);
        const auto spent = std::exchange(actor.spent_meter, 0u);
        fighter.tension -= std::min(fighter.tension, spent);
        if (teching && !(actor.move >= 152 && actor.move <= 156) && !(actor.move >= 160 && actor.move <= 164))
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        const bool throwing = actor.move == 147 || actor.move == 157;
        if (throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::none)
            fighter.throw_phase = PracticeFighter::ThrowPhase::start;
        else if (!throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::start)
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        fighter.throw_facing_right = actor.facing_right;
        const float before = fighter.jump_height;
        fighter.practice_x = std::clamp(actor.value(0xa0), left, right);
        actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
        if (fighter.jump_height == 0 && (before > 0 || (actor.move >= 97 && actor.move <= 105))) actor.start(41);
        fighter.active_normal = actor.attacking() ? static_cast<int>(actor.move) : -1;
        fighter.normal_facing_right = actor.facing_right;
    }

    void tick_painwheel(PracticeFighter& fighter, float opponent_x, float left, float right, bool tie_right) {
        auto& actor = *fighter.painwheel;
        const auto& moves = painwheel_assets;
        if (moves.animations.empty()) return;
        if (actor.move==20) actor.animate(moves);
        actor.state.variables[0x1e]=0.0f;
        actor.state.variables[0x1b]=1.0f;
        fighter.normal_hit = nullptr;
        fighter.striking_normal = -1;
        if (fighter.fenrir_captured || fighter.throw_phase == PracticeFighter::ThrowPhase::grabbed) return;
        const unsigned buttons = std::exchange(fighter.pressed_punches, 0u) |
            (std::exchange(fighter.pressed_kicks, 0u) << 3);
        fighter.throw_input = false;
        fighter.normal_input = -1;
        const int direction = int(fighter.practice_right) - int(fighter.practice_left);
        const int vertical = int(fighter.practice_up) - int(fighter.practice_down);
        const bool grounded = fighter.jump_height <= 0 && fighter.jump_velocity <= 0;
        const bool repeated = fighter.update_dash_input(direction, !grounded);
        const bool facing = fighter.practice_x < opponent_x || (fighter.practice_x == opponent_x && tie_right);
        if (grounded && actor.move <= 32) actor.facing_right = facing;
        actor.sample(skullgirls::filia_input_direction(direction, vertical), buttons,
            fighter.held_attack_buttons, grounded, fighter.tension);
        actor.state.variables[0x17] = facing ? 1.0f : -1.0f;
        actor.state.variables[0x38] = actor.facing_right ? 1.0f : -1.0f;
        actor.state.variables[0xa0] = fighter.practice_x;
        actor.state.variables[0xa1] = fighter.jump_height;
        actor.state.variables[0x14] = (opponent_x - fighter.practice_x) * (actor.facing_right ? 1.0f : -1.0f);
        if (fighter.hit_stop) { --fighter.hit_stop; return; }
        if (fighter.burst_ticks || fighter.burst_fall_tick) {
            actor.animate(moves);
            actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
            if (fighter.burst_ticks) --fighter.burst_ticks;
            if (!fighter.burst_ticks && actor.move == 457) actor.start(fighter.jump_height > 0 ? 459 : 41);
            fighter.burst_fall_tick = actor.move >= 459 && actor.move <= 462 ? 1 : 0;
            if (actor.move < 457 || actor.move > 462) {
                fighter.burst_ticks = 0;
                fighter.burst_fall_tick = fighter.jump_height > 0 ? 1u : 0u;
                actor.start(fighter.burst_fall_tick ? 459u : 41u);
            }
            if (fighter.jump_height == 0 && fighter.jump_velocity <= 0 && fighter.burst_fall_tick) {
                fighter.burst_ticks = fighter.burst_fall_tick = 0;
                actor.start(41);
            }
            fighter.active_normal = -1;
            return;
        }
        if (fighter.blockstun || fighter.hurt_group >= 0) {
            const bool guard = fighter.blockstun != 0;
            const unsigned reaction = guard ? (grounded ? fighter.crouching ? 433 : 424 : 442) :
                !grounded ? 282 : fighter.crouching ? 276 :
                fighter.hurt_height == skullgirls::HurtHeight::high ? 260 :
                fighter.hurt_height == skullgirls::HurtHeight::low ? 268 : 264;
            if (!actor.reacting) { actor.start(reaction); actor.reacting = true; }
            actor.facing_right = fighter.hurt_facing_right;
            if (guard) --fighter.blockstun;
            else if (fighter.hurt_hold) --fighter.hurt_hold;
            if (!grounded) {
                fighter.jump_height = std::max(0.0f, fighter.jump_height + fighter.jump_velocity);
                fighter.jump_velocity = fighter.jump_height > 0 ? fighter.jump_velocity - actor.gravity : 0;
            }
            if (!grounded || !guard) {
                fighter.practice_x = std::clamp(fighter.practice_x + fighter.recoil_x, left, right);
                if (grounded) fighter.recoil_x = std::copysign(std::max(0.0f, std::abs(fighter.recoil_x) - 1.5f), fighter.recoil_x);
            }
            if (fighter.blockstun || fighter.hurt_hold) actor.animate(moves);
            else {
                fighter.guard_phase = PracticeFighter::GuardPhase::none;
                fighter.hurt_group = -1;
                actor.reacting = false;
                actor.start(grounded ? fighter.crouching ? 22 : 21 : 107);
                actor.state.variables[0x9e] = 0.0f;
                actor.state.variables[0x9f] = fighter.jump_velocity;
            }
            fighter.active_normal = -1;
            return;
        }
        fighter.crouching = grounded && fighter.practice_down;
        const int relative = actor.facing_right ? direction : -direction;
        const auto old = actor.move;
        const bool teching = fighter.throw_phase == PracticeFighter::ThrowPhase::tech;
        // Authored links handle commands and contact-dependent cancels.
        const auto oldest = actor.attacking() && actor.age ? actor.age - 1 : 63;
        auto next = actor.transition(moves, old, [&](unsigned target) {
            return teching ? target == 158 || target == 172 || target == 248 || target == 252 : target >= 21 && target <= 247;
        }, oldest);
        if (next == actor.normals || next == 111)
            next = actor.transition(moves, next, [](unsigned target) { return target >= 112 && target <= 160; });
        const bool free_ground = grounded && old >= 21 && old <= 48;
        if (free_ground && next < actor.first_attack) {
            const auto punches = (buttons | fighter.held_attack_buttons) & 7u;
            if (repeated || ((buttons & 7u) && (punches & (punches - 1)) && !(buttons & 56u))) {
                if (old <= 39) next = relative < 0 ? 39 : 37;
            } else if (fighter.practice_up && (old <= 32 || (old == 37 && relative > 0))) {
                next = skullgirls::filia_super_jump(fighter.dash_input_history) ?
                    (relative > 0 ? 55 : relative < 0 ? 57 : 56) : (old == 37 ? 53 : 52);
            } else if (next == 33) next = relative < 0 ? 39 : 37;
        }
        if (next != old && next != actor.normals) {
            const unsigned cost = next == 236 ? 3 : next == 231 || next == 230 ? 2 : next == 222 || next == 233 || next == 225 || (next >= 243 && next <= 247) ? 1 : 0;
            if (fighter.tension >= cost) {
                fighter.tension -= cost;
                if (next >= 243 && next <= 246) fighter.snapback_reserve = next >= 245 ? 1 : 0;
                actor.start(next);
                if (actor.attacking()) ++fighter.attack_instance;
            }
        }
        if (actor.move >= 21 && actor.move <= 26 && grounded)
            actor.state.variables[0x9e] = actor.state.variables[0x9f] = 0.0f;
        actor.animate(moves);

        if (teching && !(actor.move >= 154 && actor.move <= 158) && !(actor.move >= 168 && actor.move <= 172))
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        const bool throwing = actor.move == 146 || actor.move == 159;
        if (throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::none)
            fighter.throw_phase = PracticeFighter::ThrowPhase::start;
        else if (!throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::start)
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        fighter.throw_facing_right = actor.facing_right;
        const float before = fighter.jump_height;
        fighter.practice_x = std::clamp(fighter.practice_x + (actor.value(0xa0) - fighter.practice_x) *
            (actor.facing_right ? 1.0f : -1.0f), left, right);
        actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
        if (actor.move==163 && fighter.jump_height==0) actor.start(164);
        else if (actor.move==228 && fighter.jump_height==0) actor.start(229);
        else if (fighter.jump_height == 0 && (before > 0 || (actor.move >= 98 && actor.move <= 106))) actor.start(41);
        fighter.active_normal = actor.attacking() ? static_cast<int>(actor.move) : -1;
        fighter.normal_facing_right = actor.facing_right;
    }

    void tick_squigly(PracticeFighter& fighter, float opponent_x, float left, float right, bool tie_right) {
        auto& actor = *fighter.squigly;
        const auto& moves = squigly_assets;
        if (moves.animations.empty()) return;
        if(actor.move==20)actor.animate(moves);
        fighter.normal_hit = nullptr;
        fighter.striking_normal = -1;
        if (fighter.fenrir_captured || fighter.throw_phase == PracticeFighter::ThrowPhase::grabbed) return;
        const unsigned buttons = std::exchange(fighter.pressed_punches, 0u) |
            (std::exchange(fighter.pressed_kicks, 0u) << 3);
        fighter.throw_input = false;
        fighter.normal_input = -1;
        const int direction = int(fighter.practice_right) - int(fighter.practice_left);
        const int vertical = int(fighter.practice_up) - int(fighter.practice_down);
        const bool grounded = fighter.jump_height <= 0 && fighter.jump_velocity <= 0;
        const bool repeated = fighter.update_dash_input(direction, !grounded);
        const bool facing = fighter.practice_x < opponent_x || (fighter.practice_x == opponent_x && tie_right);
        if (grounded && actor.move <= 35) actor.facing_right = facing;
        actor.sample(skullgirls::filia_input_direction(direction, vertical), buttons,
            fighter.held_attack_buttons, grounded, fighter.tension);
        actor.state.variables[0x17] = facing ? 1.0f : -1.0f;
        actor.state.variables[0x38] = actor.facing_right ? 1.0f : -1.0f;
        actor.state.variables[0xa0] = fighter.practice_x;
        actor.state.variables[0xa1] = fighter.jump_height;
        // The stance and super scripts own 0xad as scratch storage,
        // so it must not be overwritten with an opponent position.
        actor.state.variables[0x0f]=left;actor.state.variables[0x10]=right;
        actor.state.variables[0x11]=practice_ceiling();
        actor.state.variables[0x14]=(opponent_x-fighter.practice_x)*(actor.facing_right?1.0f:-1.0f);
        if (fighter.hit_stop) { --fighter.hit_stop; return; }
        if (fighter.burst_ticks || fighter.burst_fall_tick) {
            actor.animate(moves);
            actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
            if (fighter.burst_ticks) --fighter.burst_ticks;
            if (!fighter.burst_ticks && actor.move == 507) actor.start(fighter.jump_height > 0 ? 509 : 46);
            fighter.burst_fall_tick = actor.move >= 509 && actor.move <= 512 ? 1 : 0;
            if (actor.move < 507 || actor.move > 512) {
                fighter.burst_ticks = 0;
                fighter.burst_fall_tick = fighter.jump_height > 0 ? 1u : 0u;
                actor.start(fighter.burst_fall_tick ? 509u : 46u);
            }
            if (fighter.jump_height == 0 && fighter.jump_velocity <= 0 && fighter.burst_fall_tick) {
                fighter.burst_ticks = fighter.burst_fall_tick = 0;
                actor.start(46);
            }
            fighter.active_normal = -1;
            return;
        }
        if (fighter.blockstun || fighter.hurt_group >= 0) {
            const bool guard = fighter.blockstun != 0;
            const unsigned reaction = guard ? (grounded ? fighter.crouching ? 483 : 474 : 492) :
                !grounded ? 332 : fighter.crouching ? 326 :
                fighter.hurt_height == skullgirls::HurtHeight::high ? 310 :
                fighter.hurt_height == skullgirls::HurtHeight::low ? 318 : 314;
            if (!actor.reacting) { actor.start(reaction); actor.reacting = true; }
            actor.facing_right = fighter.hurt_facing_right;
            if (guard) --fighter.blockstun;
            else if (fighter.hurt_hold) --fighter.hurt_hold;
            if (!grounded) {
                fighter.jump_height = std::max(0.0f, fighter.jump_height + fighter.jump_velocity);
                fighter.jump_velocity = fighter.jump_height > 0 ? fighter.jump_velocity - actor.gravity : 0;
            }
            if (!grounded || !guard) {
                fighter.practice_x = std::clamp(fighter.practice_x + fighter.recoil_x, left, right);
                if (grounded) fighter.recoil_x = std::copysign(std::max(0.0f, std::abs(fighter.recoil_x) - 1.5f), fighter.recoil_x);
            }
            if (fighter.blockstun || fighter.hurt_hold) actor.animate(moves);
            else {
                fighter.guard_phase = PracticeFighter::GuardPhase::none;
                fighter.hurt_group = -1;
                actor.reacting = false;
                actor.start(grounded ? fighter.crouching ? 25 : 21 : 111);
                actor.state.variables[0x9e] = 0.0f;
                actor.state.variables[0x9f] = fighter.jump_velocity;
            }
            fighter.active_normal = -1;
            return;
        }
        fighter.crouching = grounded && fighter.practice_down;
        const int relative = actor.facing_right ? direction : -direction;
        const auto old = actor.move;
        const bool teching = fighter.throw_phase == PracticeFighter::ThrowPhase::tech;
        // Authored links handle commands and contact-dependent cancels.
        const auto oldest = actor.attacking() && actor.age ? actor.age - 1 : 63;
        auto next = actor.transition(moves, old, [&](unsigned target) {
            // TODO(SQUIGLY): Daisy Pusher and the Raging Dragon capture backend
            // need the original opponent-context/child grab lifecycle.
            if(target==286 || target==289)return false;
            return teching ? target == 151 || target == 160 || target == 298 || target == 302 : target >= 21 && target <= 297;
        }, oldest);
        if (next == actor.normals)
            next = actor.transition(moves, actor.normals, [](unsigned target) { return target >= 115 && target <= 153; });
        const bool free_ground = grounded && old >= 21 && old <= 53;
        if (free_ground && next < actor.first_attack) {
            const auto punches = (buttons | fighter.held_attack_buttons) & 7u;
            if (repeated || ((buttons & 7u) && (punches & (punches - 1)) && !(buttons & 56u))) {
                if (old <= 43) next = relative < 0 ? 43 : 40;
            } else if (fighter.practice_up && (old <= 35 || (old == 40 && relative > 0))) {
                next = skullgirls::filia_super_jump(fighter.dash_input_history) ?
                    (relative > 0 ? 59 : relative < 0 ? 61 : 60) : (old == 40 ? 58 : 57);
            } else if (next == 36) next = relative < 0 ? 43 : 40;
        }
        if (next != old && next != actor.normals) {
            const unsigned cost=next==288?3:next>=282&&next<=286?1:next>=293&&next<=297?1:0;
            if(fighter.tension>=cost) {
                if(next>=293&&next<=296)fighter.snapback_reserve=next>=295?1:0;
                actor.start(next);
                if (actor.attacking()) ++fighter.attack_instance;
            }
        }
        if (actor.move >= 21 && actor.move <= 29 && grounded)
            actor.state.variables[0x9e] = actor.state.variables[0x9f] = 0.0f;
        actor.animate(moves);
        if(const auto meter=actor.state.team_variables[0x42])
            fighter.tension=std::min(fighter.tension,unsigned(std::max(0.0f,*meter)/100));

        if (teching && !(actor.move >= 147 && actor.move <= 151) && !(actor.move >= 156 && actor.move <= 160))
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        const bool throwing = actor.move == 144 || actor.move == 152;
        if (throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::none)
            fighter.throw_phase = PracticeFighter::ThrowPhase::start;
        else if (!throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::start)
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        fighter.throw_facing_right = actor.facing_right;
        const float before = fighter.jump_height;
        fighter.practice_x = std::clamp(actor.value(0xa0), left, right);
        actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
        if(actor.move==244 && fighter.jump_height==0)actor.start(245);
        else if(actor.move==272 && fighter.jump_height==0) {
            const auto landing=actor.transition(moves,272,[](unsigned id){return id>=273&&id<=276;});
            actor.start(landing==272?273u:landing);
        } else if(fighter.jump_height==0 && (before>0 || (actor.move>=102&&actor.move<=110)) &&
                  !(actor.move>=240&&actor.move<=245))actor.start(46);

        fighter.active_normal = actor.attacking() ? static_cast<int>(actor.move) : -1;
        fighter.normal_facing_right = actor.facing_right;
    }

    void tick_fukua(PracticeFighter& fighter, float opponent_x, float left, float right, bool tie_right) {
        auto& actor = *fighter.fukua;
        const auto& moves = fukua_assets;
        if (moves.animations.empty()) return;
        if(actor.move==20)actor.animate(moves);
        fighter.normal_hit = nullptr;
        fighter.striking_normal = -1;
        if (fighter.fenrir_captured || fighter.throw_phase == PracticeFighter::ThrowPhase::grabbed) return;
        const unsigned buttons = std::exchange(fighter.pressed_punches, 0u) |
            (std::exchange(fighter.pressed_kicks, 0u) << 3);
        fighter.throw_input = false;
        fighter.normal_input = -1;
        const int direction = int(fighter.practice_right) - int(fighter.practice_left);
        const int vertical = int(fighter.practice_up) - int(fighter.practice_down);
        const bool grounded = fighter.jump_height <= 0 && fighter.jump_velocity <= 0;
        const bool repeated = fighter.update_dash_input(direction, !grounded);
        const bool facing = fighter.practice_x < opponent_x || (fighter.practice_x == opponent_x && tie_right);
        if (grounded && actor.move <= 33) actor.facing_right = facing;
        actor.sample(skullgirls::filia_input_direction(direction, vertical), buttons,
            fighter.held_attack_buttons, grounded, fighter.tension);
        actor.state.variables[0x17] = facing ? 1.0f : -1.0f;
        actor.state.variables[0x38] = actor.facing_right ? 1.0f : -1.0f;
        actor.state.variables[0xa0] = fighter.practice_x;
        actor.state.variables[0xa1] = fighter.jump_height;
        // 0xad is authored scratch storage,
        // so it must not be overwritten with an opponent position.
        actor.state.variables[0x0f]=left;actor.state.variables[0x10]=right;
        actor.state.variables[0x11]=practice_ceiling();
        actor.state.variables[0x14]=(opponent_x-fighter.practice_x)*(actor.facing_right?1.0f:-1.0f);
        if (fighter.hit_stop) { --fighter.hit_stop; return; }
        if (fighter.burst_ticks || fighter.burst_fall_tick) {
            actor.animate(moves);
            actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
            if (fighter.burst_ticks) --fighter.burst_ticks;
            if (!fighter.burst_ticks && actor.move == 410) actor.start(fighter.jump_height > 0 ? 412 : 42);
            fighter.burst_fall_tick = actor.move >= 412 && actor.move <= 415 ? 1 : 0;
            if (actor.move < 410 || actor.move > 415) {
                fighter.burst_ticks = 0;
                fighter.burst_fall_tick = fighter.jump_height > 0 ? 1u : 0u;
                actor.start(fighter.burst_fall_tick ? 412u : 42u);
            }
            if (fighter.jump_height == 0 && fighter.jump_velocity <= 0 && fighter.burst_fall_tick) {
                fighter.burst_ticks = fighter.burst_fall_tick = 0;
                actor.start(42);
            }
            fighter.active_normal = -1;
            return;
        }
        if (fighter.blockstun || fighter.hurt_group >= 0) {
            const bool guard = fighter.blockstun != 0;
            const unsigned reaction = guard ? (grounded ? fighter.crouching ? 386 : 377 : 395) :
                !grounded ? 235 : fighter.crouching ? 229 :
                fighter.hurt_height == skullgirls::HurtHeight::high ? 213 :
                fighter.hurt_height == skullgirls::HurtHeight::low ? 221 : 217;
            if (!actor.reacting) { actor.start(reaction); actor.reacting = true; }
            actor.facing_right = fighter.hurt_facing_right;
            if (guard) --fighter.blockstun;
            else if (fighter.hurt_hold) --fighter.hurt_hold;
            if (!grounded) {
                fighter.jump_height = std::max(0.0f, fighter.jump_height + fighter.jump_velocity);
                fighter.jump_velocity = fighter.jump_height > 0 ? fighter.jump_velocity - actor.gravity : 0;
            }
            if (!grounded || !guard) {
                fighter.practice_x = std::clamp(fighter.practice_x + fighter.recoil_x, left, right);
                if (grounded) fighter.recoil_x = std::copysign(std::max(0.0f, std::abs(fighter.recoil_x) - 1.5f), fighter.recoil_x);
            }
            if (fighter.blockstun || fighter.hurt_hold) actor.animate(moves);
            else {
                fighter.guard_phase = PracticeFighter::GuardPhase::none;
                fighter.hurt_group = -1;
                actor.reacting = false;
                actor.start(grounded ? fighter.crouching ? 23 : 21 : 87);
                actor.state.variables[0x9e] = 0.0f;
                actor.state.variables[0x9f] = fighter.jump_velocity;
            }
            fighter.active_normal = -1;
            return;
        }
        fighter.crouching = grounded && fighter.practice_down;
        const int relative = actor.facing_right ? direction : -direction;
        const auto old = actor.move;
        const bool teching = fighter.throw_phase == PracticeFighter::ThrowPhase::tech;
        // Authored links handle commands and contact-dependent cancels.
        const auto oldest = actor.attacking() && actor.age ? actor.age - 1 : 63;
        auto next = actor.transition(moves, old, [&](unsigned target) {
            // TODO(FUKUA): the five-bar throw's opponent/landing/bomb host
            // callback is not recovered; its FOIT contains no damaging hit.
            if(target==168)return false;
            return teching ? target == 132 || target == 143 || target == 201 || target == 205 : target >= 21 && target <= 200;
        }, oldest);
        if (next == actor.normals)
            next = actor.transition(moves, actor.normals, [](unsigned target) { return target >= 91 && target <= 134; });
        const bool free_ground = grounded && old >= 21 && old <= 43;
        if (free_ground && next < actor.first_attack) {
            const auto punches = (buttons | fighter.held_attack_buttons) & 7u;
            if (repeated || ((buttons & 7u) && (punches & (punches - 1)) && !(buttons & 56u))) {
                if (old <= 41) next = relative < 0 ? 41 : 38;
            } else if (fighter.practice_up && (old <= 33 || (old == 38 && relative > 0))) {
                next = skullgirls::filia_super_jump(fighter.dash_input_history) ?
                    (relative > 0 ? 47 : relative < 0 ? 49 : 48) : (old == 38 ? 46 : 45);
            } else if (next == 34) next = relative < 0 ? 41 : 38;
        }
        if (next != old && next != actor.normals) {
            const unsigned cost=next==168?5:next==191?3:next==172||(next>=173&&next<=175)||next==177||next==178||next==181||next==187?1:next>=196&&next<=199?1:0;
            if(fighter.tension>=cost) {
                if(next>=196&&next<=199)fighter.snapback_reserve=next>=198?1:0;
                actor.start(next);
                if (actor.attacking()) ++fighter.attack_instance;
            }
        }
        if (actor.move >= 21 && actor.move <= 27 && grounded)
            actor.state.variables[0x9e] = actor.state.variables[0x9f] = 0.0f;
        actor.animate(moves);
        if(const auto meter=actor.state.team_variables[0x42])
            fighter.tension=std::min(fighter.tension,unsigned(std::max(0.0f,*meter)/100));

        if (teching && !(actor.move >= 128 && actor.move <= 132) && !(actor.move >= 139 && actor.move <= 143))
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        const bool throwing = (actor.move>=120&&actor.move<=123) || actor.move==133 || actor.move==134;
        if (throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::none)
            fighter.throw_phase = PracticeFighter::ThrowPhase::start;
        else if (!throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::start)
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        fighter.throw_facing_right = actor.facing_right;
        const float before = fighter.jump_height;
        fighter.practice_x = std::clamp(actor.value(0xa0), left, right);
        actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
        if((actor.move==184 || actor.move==185) && fighter.jump_height==0)actor.start(186);
        else if((actor.move==188 || actor.move==189) && fighter.jump_height==0)actor.start(190);
        else if(fighter.jump_height==0 && (before>0 || (actor.move>=78&&actor.move<=86)) &&
                  !(actor.move>=181&&actor.move<=190))actor.start(42);

        fighter.active_normal = actor.attacking() ? static_cast<int>(actor.move) : -1;
        fighter.normal_facing_right = actor.facing_right;
    }

    void tick_valentine(PracticeFighter& fighter, float opponent_x, float left, float right, bool tie_right) {
        auto& actor = *fighter.valentine;
        const auto& moves = valentine_assets;
        if (moves.animations.empty()) return;
        fighter.normal_hit = nullptr;
        fighter.striking_normal = -1;
        if (fighter.fenrir_captured || fighter.throw_phase == PracticeFighter::ThrowPhase::grabbed) return;
        const unsigned buttons = std::exchange(fighter.pressed_punches, 0u) |
            (std::exchange(fighter.pressed_kicks, 0u) << 3);
        fighter.throw_input = false;
        fighter.normal_input = -1;
        const int direction = int(fighter.practice_right) - int(fighter.practice_left);
        const int vertical = int(fighter.practice_up) - int(fighter.practice_down);
        const bool grounded = fighter.jump_height <= 0 && fighter.jump_velocity <= 0;
        const bool repeated = fighter.update_dash_input(direction, !grounded);
        const bool facing = fighter.practice_x < opponent_x || (fighter.practice_x == opponent_x && tie_right);
        if (grounded && actor.move <= 31) actor.facing_right = facing;
        actor.sample(skullgirls::filia_input_direction(direction, vertical), buttons,
            fighter.held_attack_buttons, grounded, fighter.tension);
        actor.state.variables[0x17] = facing ? 1.0f : -1.0f;
        actor.state.variables[0x38] = actor.facing_right ? 1.0f : -1.0f;
        actor.state.variables[0xa0] = fighter.practice_x;
        actor.state.variables[0xa1] = fighter.jump_height;
        // 0xad is authored scratch storage,
        // so it must not be overwritten with an opponent position.
        actor.state.variables[0x0f]=left;actor.state.variables[0x10]=right;
        actor.state.variables[0x11]=practice_ceiling();
        actor.state.variables[0x14]=(opponent_x-fighter.practice_x)*(actor.facing_right?1.0f:-1.0f);
        if (fighter.hit_stop) { --fighter.hit_stop; return; }
        if (fighter.burst_ticks || fighter.burst_fall_tick) {
            actor.animate(moves);
            actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
            if (fighter.burst_ticks) --fighter.burst_ticks;
            if (!fighter.burst_ticks && actor.move == 465) actor.start(fighter.jump_height > 0 ? 467 : 41);
            fighter.burst_fall_tick = actor.move >= 467 && actor.move <= 470 ? 1 : 0;
            if (actor.move < 465 || actor.move > 470) {
                fighter.burst_ticks = 0;
                fighter.burst_fall_tick = fighter.jump_height > 0 ? 1u : 0u;
                actor.start(fighter.burst_fall_tick ? 467u : 41u);
            }
            if (fighter.jump_height == 0 && fighter.jump_velocity <= 0 && fighter.burst_fall_tick) {
                fighter.burst_ticks = fighter.burst_fall_tick = 0;
                actor.start(41);
            }
            fighter.active_normal = -1;
            return;
        }
        if (fighter.blockstun || fighter.hurt_group >= 0) {
            const bool guard = fighter.blockstun != 0;
            const unsigned reaction = guard ? (grounded ? fighter.crouching ? 441 : 432 : 450) :
                !grounded ? 290 : fighter.crouching ? 284 :
                fighter.hurt_height == skullgirls::HurtHeight::high ? 268 :
                fighter.hurt_height == skullgirls::HurtHeight::low ? 276 : 272;
            if (!actor.reacting) { actor.start(reaction); actor.reacting = true; }
            actor.facing_right = fighter.hurt_facing_right;
            if (guard) --fighter.blockstun;
            else if (fighter.hurt_hold) --fighter.hurt_hold;
            if (!grounded) {
                fighter.jump_height = std::max(0.0f, fighter.jump_height + fighter.jump_velocity);
                fighter.jump_velocity = fighter.jump_height > 0 ? fighter.jump_velocity - actor.gravity : 0;
            }
            if (!grounded || !guard) {
                fighter.practice_x = std::clamp(fighter.practice_x + fighter.recoil_x, left, right);
                if (grounded) fighter.recoil_x = std::copysign(std::max(0.0f, std::abs(fighter.recoil_x) - 1.5f), fighter.recoil_x);
            }
            if (fighter.blockstun || fighter.hurt_hold) actor.animate(moves);
            else {
                fighter.guard_phase = PracticeFighter::GuardPhase::none;
                fighter.hurt_group = -1;
                actor.reacting = false;
                actor.start(grounded ? fighter.crouching ? 21 : 20 : 115);
                actor.state.variables[0x9e] = 0.0f;
                actor.state.variables[0x9f] = fighter.jump_velocity;
            }
            fighter.active_normal = -1;
            return;
        }
        fighter.crouching = grounded && fighter.practice_down;
        const int relative = actor.facing_right ? direction : -direction;
        const auto old = actor.move;
        const bool teching = fighter.throw_phase == PracticeFighter::ThrowPhase::tech;
        // Authored links handle commands and contact-dependent cancels.
        const auto oldest = actor.attacking() && actor.age ? actor.age - 1 : 63;
        auto next = actor.transition(moves, old, [&](unsigned target) {
            // TODO(VALENTINE): recover the native secondary-capture, counter-hit
            // and team revival host hooks before enabling these command entries.
            // Their authored animation data alone does not implement those mechanics.
            if(target==186 || target==187 || target==227 || target==237 || target==248 || target==249)return false;
            return teching ? target == 159 || target == 169 || target == 256 || target == 260 : target >= 20 && target <= 255;
        }, oldest);
        if (next == actor.normals)
            next = actor.transition(moves, actor.normals, [](unsigned target) { return target >= 119 && target <= 162; });
        const bool free_ground = grounded && old >= 20 && old <= 48;
        if (free_ground && next < actor.first_attack) {
            const auto punches = (buttons | fighter.held_attack_buttons) & 7u;
            if (repeated || ((buttons & 7u) && (punches & (punches - 1)) && !(buttons & 56u))) {
                if (old <= 39) next = relative < 0 ? 39 : 36;
            } else if (fighter.practice_up && (old <= 31 || (old == 36 && relative > 0))) {
                next = skullgirls::filia_super_jump(fighter.dash_input_history) ?
                    (relative > 0 ? 54 : relative < 0 ? 56 : 55) : (old == 36 ? 53 : 52);
            } else if (next == 32) next = relative < 0 ? 39 : 36;
        }
        if (next != old && next != actor.normals) {
            const unsigned cost = next==237 ? 3 : next==227 ? 2 : next==248 || next==249 ? 5 :
                next==214 || next==215 || next==219 || next==223 || (next>=251 && next<=255) ? 1 : 0;
            if (fighter.tension >= cost) {
                if(next>=251 && next<=254)fighter.snapback_reserve=next>=253?1:0;
                actor.start(next);
                if (actor.attacking()) ++fighter.attack_instance;
            }
        }
        if (actor.move >= 20 && actor.move <= 25 && grounded)
            actor.state.variables[0x9e] = actor.state.variables[0x9f] = 0.0f;
        actor.animate(moves);
        if(const auto meter=actor.state.team_variables[0x42])
            fighter.tension=std::min(fighter.tension,unsigned(std::max(0.0f,*meter)/100));

        if (teching && !(actor.move >= 155 && actor.move <= 159) && !(actor.move >= 165 && actor.move <= 169))
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        const bool throwing = actor.move == 152 || actor.move == 161;
        if (throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::none)
            fighter.throw_phase = PracticeFighter::ThrowPhase::start;
        else if (!throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::start)
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        fighter.throw_facing_right = actor.facing_right;
        const float before = fighter.jump_height;
        fighter.practice_x = std::clamp(actor.value(0xa0), left, right);
        actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
        if(actor.move==194 && fighter.jump_height==0)actor.start(195);
        else if(actor.move==244 && fighter.jump_height==0)actor.start(245);
        else if(actor.move>=216 && actor.move<=217 && fighter.jump_height==0)actor.start(218);
        else if (fighter.jump_height == 0 && (before > 0 || (actor.move >= 98 && actor.move <= 107))) actor.start(41);
        fighter.active_normal = actor.attacking() ? static_cast<int>(actor.move) : -1;
        fighter.normal_facing_right = actor.facing_right;
    }

    void tick_parasoul(PracticeFighter& fighter, float opponent_x, float left, float right, bool tie_right) {
        auto& actor = *fighter.parasoul;
        const auto& moves = parasoul_assets;
        if (moves.animations.empty()) return;
        fighter.normal_hit = nullptr;
        fighter.striking_normal = -1;
        if (fighter.fenrir_captured || fighter.throw_phase == PracticeFighter::ThrowPhase::grabbed) return;
        const unsigned buttons = std::exchange(fighter.pressed_punches, 0u) |
            (std::exchange(fighter.pressed_kicks, 0u) << 3);
        fighter.throw_input = false;
        fighter.normal_input = -1;
        const int direction = int(fighter.practice_right) - int(fighter.practice_left);
        const int vertical = int(fighter.practice_up) - int(fighter.practice_down);
        const bool grounded = fighter.jump_height <= 0 && fighter.jump_velocity <= 0;
        const bool repeated = fighter.update_dash_input(direction, !grounded);
        const bool facing = fighter.practice_x < opponent_x || (fighter.practice_x == opponent_x && tie_right);
        if (grounded && actor.move <= 31) actor.facing_right = facing;
        actor.sample(skullgirls::filia_input_direction(direction, vertical), buttons,
            fighter.held_attack_buttons, grounded, fighter.tension);
        actor.state.variables[0x17] = facing ? 1.0f : -1.0f;
        actor.state.variables[0x38] = actor.facing_right ? 1.0f : -1.0f;
        actor.state.variables[0xa0] = fighter.practice_x;
        actor.state.variables[0xa1] = fighter.jump_height;
        actor.state.variables[0xad] = opponent_x;
        if (fighter.hit_stop) { --fighter.hit_stop; return; }
        if (fighter.burst_ticks || fighter.burst_fall_tick) {
            actor.animate(moves);
            actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
            if (fighter.burst_ticks) --fighter.burst_ticks;
            if (!fighter.burst_ticks && actor.move == 439) actor.start(fighter.jump_height > 0 ? 441 : 40);
            fighter.burst_fall_tick = actor.move >= 441 && actor.move <= 444 ? 1 : 0;
            if (actor.move < 439 || actor.move > 444) {
                fighter.burst_ticks = 0;
                fighter.burst_fall_tick = fighter.jump_height > 0 ? 1u : 0u;
                actor.start(fighter.burst_fall_tick ? 441u : 40u);
            }
            if (fighter.jump_height == 0 && fighter.jump_velocity <= 0 && fighter.burst_fall_tick) {
                fighter.burst_ticks = fighter.burst_fall_tick = 0;
                actor.start(40);
            }
            fighter.active_normal = -1;
            return;
        }
        if (fighter.blockstun || fighter.hurt_group >= 0) {
            const bool guard = fighter.blockstun != 0;
            const unsigned reaction = guard ? (grounded ? fighter.crouching ? 415 : 406 : 424) :
                !grounded ? 264 : fighter.crouching ? 258 :
                fighter.hurt_height == skullgirls::HurtHeight::high ? 242 :
                fighter.hurt_height == skullgirls::HurtHeight::low ? 250 : 246;
            if (!actor.reacting) { actor.start(reaction); actor.reacting = true; }
            actor.facing_right = fighter.hurt_facing_right;
            if (guard) --fighter.blockstun;
            else if (fighter.hurt_hold) --fighter.hurt_hold;
            if (!grounded) {
                fighter.jump_height = std::max(0.0f, fighter.jump_height + fighter.jump_velocity);
                fighter.jump_velocity = fighter.jump_height > 0 ? fighter.jump_velocity - actor.gravity : 0;
            }
            if (!grounded || !guard) {
                fighter.practice_x = std::clamp(fighter.practice_x + fighter.recoil_x, left, right);
                if (grounded) fighter.recoil_x = std::copysign(std::max(0.0f, std::abs(fighter.recoil_x) - 1.5f), fighter.recoil_x);
            }
            if (fighter.blockstun || fighter.hurt_hold) actor.animate(moves);
            else {
                fighter.guard_phase = PracticeFighter::GuardPhase::none;
                fighter.hurt_group = -1;
                actor.reacting = false;
                actor.start(grounded ? fighter.crouching ? 21 : 20 : 108);
                actor.state.variables[0x9e] = 0.0f;
                actor.state.variables[0x9f] = fighter.jump_velocity;
            }
            fighter.active_normal = -1;
            return;
        }
        fighter.crouching = grounded && fighter.practice_down;
        const int relative = actor.facing_right ? direction : -direction;
        const auto old = actor.move;
        const bool teching = fighter.throw_phase == PracticeFighter::ThrowPhase::tech;
        // Authored links handle commands and contact-dependent cancels.
        const auto oldest = actor.attacking() && actor.age ? actor.age - 1 : 63;
        auto next = actor.transition(moves, old, [&](unsigned target) {
            return teching ? target == 173 || target == 182 || target == 230 || target == 234 : target >= 20 && target <= 229;
        }, oldest);
        if (next == actor.normals)
            next = actor.transition(moves, actor.normals, [](unsigned target) { return target >= 112 && target <= 175; });
        const bool free_ground = grounded && old >= 20 && old <= 47;
        if (free_ground && next < actor.first_attack) {
            const auto punches = (buttons | fighter.held_attack_buttons) & 7u;
            if (repeated || ((buttons & 7u) && (punches & (punches - 1)) && !(buttons & 56u))) {
                if (old <= 38) next = relative < 0 ? 38 : 36;
            } else if (fighter.practice_up && (old <= 31 || (old == 36 && relative > 0))) {
                next = skullgirls::filia_super_jump(fighter.dash_input_history) ?
                    (relative > 0 ? 53 : relative < 0 ? 55 : 54) : (old == 36 ? 52 : 51);
            } else if (next == 32) next = relative < 0 ? 38 : 36;
        }
        if (next != old && next != actor.normals) {
            const unsigned cost = next == 221 ? 3 : next == 220 || next == 224 || (next >= 225 && next <= 229) ? 1 : 0;
            if (fighter.tension >= cost) {
                fighter.tension -= cost;
                if (next >= 225 && next <= 228) fighter.snapback_reserve = next >= 227 ? 1 : 0;
                actor.start(next);
                if (actor.attacking()) ++fighter.attack_instance;
            }
        }
        if (actor.move >= 20 && actor.move <= 25 && grounded)
            actor.state.variables[0x9e] = actor.state.variables[0x9f] = 0.0f;
        actor.animate(moves);

        if (teching && !(actor.move >= 169 && actor.move <= 173) && !(actor.move >= 178 && actor.move <= 182))
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        const bool throwing = actor.move == 165 || actor.move == 174;
        if (throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::none)
            fighter.throw_phase = PracticeFighter::ThrowPhase::start;
        else if (!throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::start)
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        fighter.throw_facing_right = actor.facing_right;
        const float before = fighter.jump_height;
        fighter.practice_x = std::clamp(actor.value(0xa0), left, right);
        actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
        if (fighter.jump_height == 0 && (before > 0 || (actor.move >= 96 && actor.move <= 107))) actor.start(40);
        fighter.active_normal = actor.attacking() ? static_cast<int>(actor.move) : -1;
        fighter.normal_facing_right = actor.facing_right;
    }

    void tick_robofortune(PracticeFighter& fighter, float opponent_x, float left, float right, bool tie_right) {
        auto& actor = *fighter.robofortune;
        const auto& moves = robofortune_assets;
        if (moves.animations.empty()) return;
        if (actor.move == 20) actor.animate(moves);
        fighter.normal_hit = nullptr;
        fighter.striking_normal = -1;
        if (fighter.fenrir_captured || fighter.throw_phase == PracticeFighter::ThrowPhase::grabbed) return;
        const unsigned buttons = std::exchange(fighter.pressed_punches, 0u) |
            (std::exchange(fighter.pressed_kicks, 0u) << 3);
        fighter.throw_input = false;
        fighter.normal_input = -1;
        const int direction = int(fighter.practice_right) - int(fighter.practice_left);
        const int vertical = int(fighter.practice_up) - int(fighter.practice_down);
        const bool grounded = fighter.jump_height <= 0 && fighter.jump_velocity <= 0;
        const bool repeated = fighter.update_dash_input(direction, !grounded);
        const bool facing = fighter.practice_x < opponent_x || (fighter.practice_x == opponent_x && tie_right);
        if (grounded && actor.move <= 33) actor.facing_right = facing;
        actor.state.variables[0x1b]=1.0f;
        actor.state.variables[0x87]=0.0f;
        actor.sample(skullgirls::filia_input_direction(direction, vertical), buttons,
            fighter.held_attack_buttons, grounded, fighter.tension);
        actor.state.variables[0x17] = facing ? 1.0f : -1.0f;
        actor.state.variables[0x38] = actor.facing_right ? 1.0f : -1.0f;
        actor.state.variables[0xa0] = fighter.practice_x;
        actor.state.variables[0xa1] = fighter.jump_height;
        actor.state.variables[0xad] = opponent_x;
        if (fighter.hit_stop) { --fighter.hit_stop; return; }
        if (fighter.burst_ticks || fighter.burst_fall_tick) {
            actor.animate(moves);
            actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
            if (fighter.burst_ticks) --fighter.burst_ticks;
            if (!fighter.burst_ticks && actor.move == 427) actor.start(fighter.jump_height > 0 ? 429 : 44);
            fighter.burst_fall_tick = actor.move >= 429 && actor.move <= 432 ? 1 : 0;
            if (actor.move < 427 || actor.move > 432) {
                fighter.burst_ticks = 0;
                fighter.burst_fall_tick = fighter.jump_height > 0 ? 1u : 0u;
                actor.start(fighter.burst_fall_tick ? 429u : 44u);
            }
            if (fighter.jump_height == 0 && fighter.jump_velocity <= 0 && fighter.burst_fall_tick) {
                fighter.burst_ticks = fighter.burst_fall_tick = 0;
                actor.start(44);
            }
            fighter.active_normal = -1;
            return;
        }
        if (fighter.hurt_group >= 0) {
            if (!actor.reacting) {
                auto& reaction=actor.reaction.emplace();
                reaction.root=&moves;
                reaction.initialize(moves);
                reaction.state=actor.state;
                for (auto& slot:reaction.state.variables) if (!slot) slot=0.0f;
                for (auto& bit:reaction.state.bits) bit=false;
                reaction.state.bits[20]=true;
                reaction.state.variables[0x1b]=1.0f;
                reaction.state.variables[0xa4]=reaction.state.variables[0xa5]=reaction.state.variables[0xa6]=1.0f;
                reaction.defaults=skullgirls::CerebellaPlayback::defaults();
                reaction.authored_timeline=reaction.floor_collision=true;
                unsigned entry=!grounded?252u:fighter.crouching?246u:
                    fighter.hurt_height==skullgirls::HurtHeight::high?230u:
                    fighter.hurt_height==skullgirls::HurtHeight::low?238u:234u;
                using R=skullgirls::HeavyReaction;
                switch (fighter.heavy_reaction) {
                case R::wall_bounce:entry=305;break;
                case R::launch:case R::air_launch:entry=296;break;
                case R::ground_bounce:entry=324;break;
                case R::sweep:entry=293;break;
                case R::back_knockdown:case R::knockback:entry=311;break;
                case R::sliding:entry=316;break;
                case R::ceiling_bounce:entry=328;break;
                case R::crumple:entry=333;break;
                case R::stagger:entry=337;break;
                case R::freeze:entry=340;break;
                default:break;
                }
                reaction.facing_right=fighter.hurt_facing_right;
                reaction.start(entry);
                reaction.state.variables[0x9e]=fighter.recoil_x*(fighter.hurt_facing_right?1:-1);
                reaction.state.variables[0x9f]=fighter.jump_velocity;
                actor.reacting=true;
            }
            auto& reaction=*actor.reaction;
            reaction.inputs=actor.inputs;
            reaction.state.variables[0x38]=reaction.facing_right?1.0f:-1.0f;
            reaction.state.variables[0x35]=float(fighter.hurt_hold);
            reaction.state.variables[0x07]=1.0f;
            reaction.state.variables[0x0f]=left;
            reaction.state.variables[0x10]=right;
            reaction.state.variables[0x0d]=(fighter.practice_x<=left?-2.0f:fighter.practice_x>=right?2.0f:0.0f)*(reaction.facing_right?1:-1);
            reaction.state.variables[0x3e]=grounded?2.0f:1.0f;
            reaction.x=fighter.practice_x;reaction.y=fighter.jump_height;
            reaction.advance_authored(reaction.state);
            if (fighter.hurt_hold) --fighter.hurt_hold;
            fighter.practice_x=std::clamp(reaction.x,left,right);
            fighter.jump_height=std::max(0.0f,reaction.y);
            fighter.jump_velocity=reaction.value(0x9f);
            fighter.hurt_facing_right=reaction.facing_right;
            actor.pose=reaction.pose;
            actor.move=reaction.move;
            if ((reaction.move>=21 && reaction.move<108) ||
                (!fighter.hurt_hold && reaction.move>=381 && reaction.move<=417)) {
                fighter.hurt_group=-1;
                fighter.heavy_reaction=skullgirls::HeavyReaction::none;
                actor.reacting=false;
                actor.reaction.reset();
                actor.start(fighter.jump_height>0?96u:21u);
            }
            fighter.active_normal=-1;
            return;
        }
        if (fighter.blockstun) {
            if (!actor.reacting) actor.start(grounded ? fighter.crouching ? 403u : 394u : 412u);
            actor.reacting = true;
            actor.facing_right = fighter.hurt_facing_right;
            actor.state.variables[0x35] = float(--fighter.blockstun);
            actor.animate(moves);
            if (!fighter.blockstun) {
                actor.reacting = false;
                fighter.guard_phase = PracticeFighter::GuardPhase::none;
                actor.start(grounded ? fighter.practice_down ? 23u : 21u : 96u);
            }
            fighter.active_normal = -1;
            return;
        }
        fighter.crouching = grounded && fighter.practice_down;
        const int relative = actor.facing_right ? direction : -direction;
        const auto old = actor.move;
        actor.state.move_categories=moves.categories.at(old);
        const bool teching = fighter.throw_phase == PracticeFighter::ThrowPhase::tech;
        // Authored links handle commands and contact-dependent cancels.
        const auto oldest = actor.attacking() && actor.age ? actor.age - 1 : 63;
        auto next = actor.transition(moves, old, [&](unsigned target) {
            return teching ? target == 152 || target == 161 || target == 218 || target == 222 :
                target >= 21 && target <= 217;
        }, oldest);
        if (next == actor.normals)
            next = actor.transition(moves, actor.normals, [](unsigned target) { return target >= 108 && target <= 140; });
        const bool free_ground = grounded && old >= 21 && old <= 54;
        if (free_ground && next < actor.first_attack) {
            const auto punches = (buttons | fighter.held_attack_buttons) & 7u;
            if (repeated || ((buttons & 7u) && (punches & (punches - 1)) && !(buttons & 56u))) {
                if (old <= 41) next = relative < 0 ? 41 : 38;
            } else if (fighter.practice_up && (old <= 33 || (old == 38 && relative > 0))) {
                next = skullgirls::filia_super_jump(fighter.dash_input_history) ?
                    (relative > 0 ? 57 : relative < 0 ? 59 : 58) : (old == 38 ? 56 : 55);
            } else if (next == 34) next = relative < 0 ? 41 : 38;
        }
        if (next != old && next != actor.normals) {
            const unsigned cost = next == 195 ? 3 : next == 180 || next == 189 || (next >= 213 && next <= 216) ? 1 : 0;
            if (fighter.tension >= cost) {
                // Blockbusters spend the authored amount on their startup record.
                // SuperLaser selects its 1/3/5-bar version before that write.
                if (next >= 213 && next <= 216) fighter.snapback_reserve = next >= 215 ? 1 : 0;
                actor.start(next);
                if (actor.attacking()) ++fighter.attack_instance;
            }
        }
        if (actor.move >= 21 && actor.move <= 27 && grounded)
            actor.state.variables[0x9e] = actor.state.variables[0x9f] = 0.0f;
        actor.animate(moves);
        if(const auto meter=actor.state.team_variables[0x42])
            fighter.tension=std::min(fighter.tension,unsigned(std::max(0.0f,*meter)/100));

        if (teching && !(actor.move >= 148 && actor.move <= 152) && !(actor.move >= 157 && actor.move <= 161))
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        const bool throwing = actor.move == 145 || actor.move == 153;
        if (throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::none)
            fighter.throw_phase = PracticeFighter::ThrowPhase::start;
        else if (!throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::start)
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        fighter.throw_facing_right = actor.facing_right;
        const float before = fighter.jump_height;
        fighter.practice_x = std::clamp(actor.value(0xa0), left, right);
        actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
        if((actor.move==193 || actor.move==200) && fighter.jump_height==0 && fighter.jump_velocity<=0)
            actor.start(actor.move==193?194u:201u);
        else if (fighter.jump_height == 0 && (before > 0 || (actor.move >= 87 && actor.move <= 95)))
            actor.start(44u);
        fighter.active_normal = actor.attacking() ? static_cast<int>(actor.move) : -1;
        fighter.normal_facing_right = actor.facing_right;
    }

    void tick_msfortune(PracticeFighter& fighter, float opponent_x, float left, float right, bool tie_right) {
        auto& actor = *fighter.msfortune;
        const auto& moves = msfortune_assets;
        if (moves.animations.empty()) return;
        if (actor.move == 20) actor.animate(moves);
        fighter.normal_hit = nullptr;
        fighter.striking_normal = -1;
        if (fighter.fenrir_captured || fighter.throw_phase == PracticeFighter::ThrowPhase::grabbed) return;
        const unsigned buttons = std::exchange(fighter.pressed_punches, 0u) |
            (std::exchange(fighter.pressed_kicks, 0u) << 3);
        fighter.throw_input = false;
        fighter.normal_input = -1;
        const int direction = int(fighter.practice_right) - int(fighter.practice_left);
        const int vertical = int(fighter.practice_up) - int(fighter.practice_down);
        const bool grounded = fighter.jump_height <= 0 && fighter.jump_velocity <= 0;
        const bool repeated = fighter.update_dash_input(direction, !grounded);
        const bool facing = fighter.practice_x < opponent_x || (fighter.practice_x == opponent_x && tie_right);
        if (grounded && actor.move <= 33) actor.facing_right = facing;
        actor.state.variables[0x1b]=1.0f;
        actor.state.variables[0x87]=0.0f;
        actor.sample(skullgirls::filia_input_direction(direction, vertical), buttons,
            fighter.held_attack_buttons, grounded, fighter.tension);
        actor.state.variables[0x17] = facing ? 1.0f : -1.0f;
        actor.state.variables[0x38] = actor.facing_right ? 1.0f : -1.0f;
        actor.state.variables[0xa0] = fighter.practice_x;
        actor.state.variables[0xa1] = fighter.jump_height;
        actor.state.variables[0xad] = opponent_x;
        if (fighter.hit_stop) { --fighter.hit_stop; return; }
        if (fighter.burst_ticks || fighter.burst_fall_tick) {
            actor.animate(moves);
            actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
            if (fighter.burst_ticks) --fighter.burst_ticks;
            if (!fighter.burst_ticks && actor.move == 428) actor.start(fighter.jump_height > 0 ? 430 : 44);
            fighter.burst_fall_tick = actor.move >= 430 && actor.move <= 433 ? 1 : 0;
            if (actor.move < 428 || actor.move > 433) {
                fighter.burst_ticks = 0;
                fighter.burst_fall_tick = fighter.jump_height > 0 ? 1u : 0u;
                actor.start(fighter.burst_fall_tick ? 430u : 44u);
            }
            if (fighter.jump_height == 0 && fighter.jump_velocity <= 0 && fighter.burst_fall_tick) {
                fighter.burst_ticks = fighter.burst_fall_tick = 0;
                actor.start(44);
            }
            fighter.active_normal = -1;
            return;
        }
        if (fighter.blockstun || fighter.hurt_group >= 0) {
            const bool guard = fighter.blockstun != 0;
            const unsigned reaction = guard ? (grounded ? fighter.crouching ? 404 : 395 : 413) :
                !grounded ? 253 : fighter.crouching ? 247 :
                fighter.hurt_height == skullgirls::HurtHeight::high ? 231 :
                fighter.hurt_height == skullgirls::HurtHeight::low ? 239 : 235;
            if (!actor.reacting) { actor.start(reaction); actor.reacting = true; }
            actor.facing_right = fighter.hurt_facing_right;
            if (guard) --fighter.blockstun;
            else if (fighter.hurt_hold) --fighter.hurt_hold;
            if (!grounded) {
                fighter.jump_height = std::max(0.0f, fighter.jump_height + fighter.jump_velocity);
                fighter.jump_velocity = fighter.jump_height > 0 ? fighter.jump_velocity - actor.gravity : 0;
            }
            if (!grounded || !guard) {
                fighter.practice_x = std::clamp(fighter.practice_x + fighter.recoil_x, left, right);
                if (grounded) fighter.recoil_x = std::copysign(std::max(0.0f, std::abs(fighter.recoil_x) - 1.5f), fighter.recoil_x);
            }
            if (fighter.blockstun || fighter.hurt_hold) actor.animate(moves);
            else {
                fighter.guard_phase = PracticeFighter::GuardPhase::none;
                fighter.hurt_group = -1;
                actor.reacting = false;
                actor.start(grounded ? fighter.crouching ? 23 : 21 : 103);
                actor.state.variables[0x9e] = 0.0f;
                actor.state.variables[0x9f] = fighter.jump_velocity;
            }
            fighter.active_normal = -1;
            return;
        }
        fighter.crouching = grounded && fighter.practice_down;
        if (actor.move == 161 && actor.recall_button && !(actor.held & actor.recall_button)) actor.start(162);
        const int relative = actor.facing_right ? direction : -direction;
        const auto old = actor.move;
        const bool teching = fighter.throw_phase == PracticeFighter::ThrowPhase::tech;
        // Authored links handle commands and contact-dependent cancels.
        const auto oldest = actor.attacking() && actor.age ? actor.age - 1 : 63;
        auto next = actor.transition(moves, old, [&](unsigned target) {
            return teching ? target == 147 || target == 156 || target == 219 || target == 223 :
                target >= 21 && target <= 218 && (old != 161 || target != 162);
        }, oldest);
        if (next == actor.normals)
            next = actor.transition(moves, actor.normals, [](unsigned target) { return target >= 107 && target <= 149; });
        const bool free_ground = grounded && old >= 21 && old <= 54;
        if (free_ground && next < actor.first_attack) {
            const auto punches = (buttons | fighter.held_attack_buttons) & 7u;
            if (repeated || ((buttons & 7u) && (punches & (punches - 1)) && !(buttons & 56u))) {
                if (old <= 41) next = relative < 0 ? 41 : 38;
            } else if (fighter.practice_up && (old <= 33 || (old == 38 && relative > 0))) {
                next = skullgirls::filia_super_jump(fighter.dash_input_history) ?
                    (relative > 0 ? 57 : relative < 0 ? 59 : 58) : (old == 38 ? 56 : 55);
            } else if (next == 34) next = relative < 0 ? 41 : 38;
        }
        if (next != old && next != actor.normals) {
            const unsigned cost = next == 209 ? 3 : next == 205 || next == 201 || (next >= 214 && next <= 218) ? 1 : 0;
            if (fighter.tension >= cost) {
                fighter.tension -= cost;
                if (next >= 214 && next <= 217) fighter.snapback_reserve = next >= 216 ? 1 : 0;
                actor.start(next);
                if (next >= 157 && next <= 159 && actor.value(0x8b) != 0)
                    actor.recall_button = (buttons | fighter.held_attack_buttons) & 3u;
                if (actor.attacking()) ++fighter.attack_instance;
            }
        }
        if (actor.move >= 21 && actor.move <= 27 && grounded)
            actor.state.variables[0x9e] = actor.state.variables[0x9f] = 0.0f;
        actor.animate(moves);

        if (teching && !(actor.move >= 143 && actor.move <= 147) && !(actor.move >= 152 && actor.move <= 156))
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        const bool throwing = actor.move == 139 || actor.move == 148;
        if (throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::none)
            fighter.throw_phase = PracticeFighter::ThrowPhase::start;
        else if (!throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::start)
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        fighter.throw_facing_right = actor.facing_right;
        const float before = fighter.jump_height;
        fighter.practice_x = std::clamp(actor.value(0xa0), left, right);
        actor.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
        if (fighter.jump_height == 0 && (before > 0 || (actor.move >= 87 && actor.move <= 95)))
            actor.start(actor.move==202?203u:actor.move>=168 && actor.move<=170?198u:44u);
        fighter.active_normal = actor.attacking() ? static_cast<int>(actor.move) : -1;
        fighter.normal_facing_right = actor.facing_right;
    }

    void tick_cerebella(PracticeFighter& fighter, float opponent_x, float left, float right, bool tie_right) {
        using Bella = skullgirls::CerebellaPlayback;
        auto& bella = *fighter.cerebella;
        const auto& moves = cerebella_assets;
        if (moves.animations.empty()) return;
        fighter.normal_hit = nullptr;
        fighter.striking_normal = -1;
        if (fighter.fenrir_captured || fighter.throw_phase == PracticeFighter::ThrowPhase::grabbed) {
            bella.pose = fighter.movement_sprite ? fighter.movement_sprite : bella.pose;
            return;
        }
        const auto buttons = std::exchange(fighter.pressed_punches, 0u) |
            (std::exchange(fighter.pressed_kicks, 0u) << 3);
        fighter.throw_input = false;
        fighter.normal_input = -1;
        const int direction = int(fighter.practice_right) - int(fighter.practice_left);
        const int vertical = int(fighter.practice_up) - int(fighter.practice_down);
        const bool grounded = fighter.jump_height <= 0 && fighter.jump_velocity <= 0;
        const bool repeated = fighter.update_dash_input(direction, !grounded);
        const bool face_right = fighter.practice_x < opponent_x || (fighter.practice_x == opponent_x && tie_right);
        if (grounded && bella.move <= 31) bella.facing_right = face_right;
        bella.sample(skullgirls::filia_input_direction(direction, vertical), buttons,
                    fighter.held_attack_buttons, grounded, fighter.tension);
        bella.state.variables[0x17] = face_right ? 1.0f : -1.0f;
        bella.state.variables[0x38] = bella.facing_right ? 1.0f : -1.0f;
        bella.state.variables[0xa0] = fighter.practice_x;
        bella.state.variables[0xa1] = fighter.jump_height;
        if (fighter.hit_stop) {
            --fighter.hit_stop;
            return;
        }
        if (fighter.burst_ticks || fighter.burst_fall_tick) {
            bella.animate(moves);
            bella.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
            if (fighter.burst_ticks) --fighter.burst_ticks;
            if (!fighter.burst_ticks && bella.move == 448) {
                if (fighter.jump_height > 0) {
                    bella.start(450);
                    fighter.burst_fall_tick = 1;
                } else bella.start(40);
            }
            if (bella.move == 450 || bella.move == 451) fighter.burst_fall_tick = 1;
            if (bella.move < 448 || bella.move > 453) {
                fighter.burst_ticks = 0;
                fighter.burst_fall_tick = fighter.jump_height > 0 ? 1u : 0u;
                bella.start(fighter.burst_fall_tick ? 450u : 40u);
            }
            if (fighter.jump_height == 0 && fighter.jump_velocity <= 0 && (bella.move == 450 || bella.move == 451)) {
                fighter.burst_ticks = fighter.burst_fall_tick = 0;
                bella.start(40);
            }
            fighter.active_normal = -1;
            return;
        }
        if (fighter.blockstun || fighter.hurt_group >= 0) {
            const bool guard = fighter.blockstun != 0;
            const bool crouched = fighter.crouching;
            const unsigned reaction = guard ? (grounded ? crouched ? 425 : 415 : 433) :
                !grounded ? 273 : crouched ? 267 : fighter.hurt_height == skullgirls::HurtHeight::high ? 251 :
                    fighter.hurt_height == skullgirls::HurtHeight::low ? 259 : 255;
            if (!bella.reacting) {
                bella.start(reaction);
                bella.reacting = true;
            }
            bella.facing_right = fighter.hurt_facing_right;
            if (guard) --fighter.blockstun;
            else if (fighter.hurt_hold) --fighter.hurt_hold;
            if (!grounded) {
                fighter.jump_height = std::max(0.0f, fighter.jump_height + fighter.jump_velocity);
                fighter.jump_velocity = fighter.jump_height > 0 ? fighter.jump_velocity - Bella::gravity : 0;
                fighter.practice_x = std::clamp(fighter.practice_x + fighter.recoil_x, left, right);
            } else if (!guard) {
                fighter.practice_x = std::clamp(fighter.practice_x + fighter.recoil_x, left, right);
                fighter.recoil_x = std::copysign(std::max(0.0f, std::abs(fighter.recoil_x) - 1.5f), fighter.recoil_x);
            }
            if (fighter.blockstun || fighter.hurt_hold) bella.animate(moves);
            else {
                fighter.guard_phase = PracticeFighter::GuardPhase::none;
                fighter.hurt_group = -1;
                bella.reacting = false;
                bella.start(grounded ? crouched ? 21 : 20 : 94);
                bella.state.variables[0x9e] = 0.0f;
                bella.state.variables[0x9f] = fighter.jump_velocity;
            }
            fighter.active_normal = -1;
            return;
        }
        fighter.crouching = grounded && fighter.practice_down;
        const int relative = bella.facing_right ? direction : -direction;
        const auto old_move = bella.move;
        const bool teching = fighter.throw_phase == PracticeFighter::ThrowPhase::tech;
        const auto order_floor = bella.attacking() && bella.age ? bella.age - 1 : 63;
        auto next = bella.transition(moves, old_move, [&](unsigned target) {
            if (teching) return target == 146 || target == 155 || target == 239 || target == 243;
            return target >= 20 && target < 239;
        }, order_floor);
        const bool free_ground = grounded && old_move >= 20 && old_move <= 42;
        if (free_ground && fighter.tension && (buttons & 9u) == 9u && bella.showstopper_motion()) next = 218;
        if (next == Bella::normals)
            next = bella.transition(moves, Bella::normals, [](unsigned target) {
                return target >= Bella::first_attack && target <= 147;
            });
        if (free_ground && next < Bella::first_attack) {
            const auto punches = (buttons | fighter.held_attack_buttons) & 7u;
            if (repeated || ((buttons & 7) && (punches & (punches - 1)) && !(buttons & 56))) {
                if (old_move <= 35) next = relative < 0 ? 38 : 36;
            } else if (fighter.practice_up && (old_move <= 31 || (old_move == 36 && relative > 0))) {
                next = skullgirls::filia_super_jump(fighter.dash_input_history) ?
                    (relative > 0 ? 45u : relative < 0 ? 47u : 46u) :
                    (old_move == 36 ? 54u : relative > 0 ? 52u : relative < 0 ? 53u : 51u);
            } else if (next == 32) {
                next = relative < 0 ? 38 : 36;
            }
        }
        if (next >= 234 && next <= 237) fighter.snapback_reserve = next >= 236 ? 1u : 0u;
        if (next == 157 && grounded) next = old_move;
        if (next != old_move && next != Bella::normals) {
            const unsigned cost = next == 227 ? 3 : next == 218 || next == 228 || (next >= 234 && next <= 238) ? 1 : 0;
            if (fighter.tension >= cost) {
                fighter.tension -= cost;
                const bool is_command = cost || (next >= Bella::first_attack && (!bella.attacking() || next <= 137));
                bella.start(next);
                if (is_command) ++fighter.attack_instance;
            }
        }
        if (bella.move >= 20 && bella.move <= 25 && grounded) {
            bella.state.variables[0x9e] = 0.0f;
            bella.state.variables[0x9f] = 0.0f;
        }
        bella.animate(moves);
        if (teching && !(bella.move >= 142 && bella.move <= 146) && !(bella.move >= 151 && bella.move <= 155))
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        const bool throwing = (bella.move >= 134 && bella.move <= 138) || bella.move == 147 || bella.move == 148;
        if (throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::none)
            fighter.throw_phase = PracticeFighter::ThrowPhase::start;
        else if (!throwing && fighter.throw_phase == PracticeFighter::ThrowPhase::start)
            fighter.throw_phase = PracticeFighter::ThrowPhase::none;
        const auto before_y = fighter.jump_height;
        bella.integrate(fighter.practice_x, fighter.jump_height, fighter.jump_velocity, left, right);
        if (before_y > 0 && fighter.jump_height == 0 && bella.move < Bella::first_attack)
            bella.start(40);
        fighter.active_normal = bella.attacking() ? static_cast<int>(bella.move) : -1;
        fighter.normal_facing_right = bella.facing_right;
    }

    void sample_super_freeze_inputs(PracticeFighter& fighter) {
        const int horizontal = int(fighter.practice_right) - int(fighter.practice_left);
        const int vertical = int(fighter.practice_up) - int(fighter.practice_down);
        const auto direction = skullgirls::filia_input_direction(horizontal, vertical);
        const unsigned edges = std::exchange(fighter.pressed_punches, 0u) |
            (std::exchange(fighter.pressed_kicks, 0u) << 3);
        auto& visual = fighter_visuals(fighter);
        if (edges) {
            visual.buffered_buttons = edges;
            visual.buffer_ticks = 5;
        } else if (visual.buffer_ticks && !--visual.buffer_ticks) visual.buffered_buttons = 0;
        if (fighter.scripted())
            fighter.scripted()->sample(direction, edges, fighter.held_attack_buttons, fighter.jump_height <= 0, fighter.tension);
        else {
            fighter.update_script_inputs(direction, edges);
            fighter.update_dash_input(horizontal, fighter.jump_height > 0);
            std::move_backward(fighter.normal_inputs.begin(), fighter.normal_inputs.end() - 1, fighter.normal_inputs.end());
            fighter.normal_inputs.front() = {edges, fighter.practice_down};
        }
        fighter.normal_input = -1;
        fighter.throw_input = false;
    }

    // Run one simulation step, from input and team actions through contact and effects.
    void practice_tick() {
        if (practice_paused) return;
        const bool super_frozen = practice_super_frozen();
        ++combat_presentation.tick;
        for (std::size_t side=0;side<practice_players.size();++side) {
            const auto& fighter=practice_players[side];
            const auto& visual=fighter.scripted() ? fighter.scripted()->visual : fighter.visual;
            combat_presentation.cutins[side].advance(visual.freeze!=0);
            combat_presentation.combos[side].advance(hud_stats[side].hits,hud_stats[side].combo,super_frozen);
            auto& ghosts=combat_presentation.ghosts[side];
            if (!super_frozen && !fighter.hit_stop) for (auto& ghost:ghosts) ++ghost.age;
            ghosts.erase(std::remove_if(ghosts.begin(),ghosts.end(),[](const auto& ghost){return ghost.age>=18;}),ghosts.end());
            if (!super_frozen && visual.super_mode && !fighter.hit_stop && combat_presentation.tick%3==0 && fighter.sprite()>1) {
                if (ghosts.size()>=6) ghosts.erase(ghosts.begin());
                ghosts.push_back({fighter.sprite(),0,practice_teams[side].active_slot,fighter.practice_x,fighter.jump_height,
                    fighter.mirror(practice_facing_right(side))});
            }
        }
        // FUN_0058ad60 eases the super-freeze presentation over fourteen ticks
        // on entry and ten ticks on exit.
        super_flash_amount = std::clamp(super_flash_amount + (super_frozen ? 1.0f / 14 : -0.1f), 0.0f, 1.0f);
        for (auto& team : practice_teams)
            for (auto& health : team.health) health.advance_life();
        for (auto& stats : hud_stats) {
            stats.advance_life();
            auto& sparks = stats.sparks;
            if (!super_frozen) for (auto& spark : sparks) ++spark.age;
            sparks.erase(std::remove_if(sparks.begin(), sparks.end(), [&](const CombatSpark& spark) {
                return spark.age >= combat_effects[static_cast<std::size_t>(spark.effect)].ticks;
            }), sparks.end());
        }
        if (practice_presentation.phase != PracticePresentation::Phase::none ||
            (training_value("OPT_STR_DEATHALLOWED") && begin_practice_knockout())) {
            advance_practice_presentation();
            for (std::size_t side = 0; side < practice_players.size(); ++side)
                collect_practice_visuals(practice_players[side], side, practice_teams[side].active_slot);
            advance_practice_effects(false);
            return;
        }
        if (training_value("OPT_STR_DEATHALLOWED")) {
            for (std::size_t side = 0; side < practice_teams.size(); ++side) {
                if (hud_stats[side].life) continue;
                if (practice_point_snapped(side)) continue;
                if (const auto slot = next_practice_reserve(side)) {
                    auto& team = practice_teams[side];
                    if (team.assist && team.assist->phase != PracticeAssist::Phase::leaving) {
                        auto& assist = *team.assist;
                        const bool acting = assist.phase == PracticeAssist::Phase::acting;
                        assist.actor.travel = {acting ? assist.fighter.practice_x : assist.actor.travel.x,
                            acting ? assist.fighter.jump_height : assist.actor.travel.height,
                            !assist.actor.home_on_left, PracticeTeamTravel::Motion::assist_out};
                        assist.phase = PracticeAssist::Phase::leaving;
                    }
                    if (team.tag) {
                        team.tag->kind = PracticeTag::Kind::knockout;
                        team.tag->incoming.travel.motion = PracticeTeamTravel::Motion::assist_in;
                    } else if (!team.departing) {
                        begin_practice_tag(side, *slot, PracticeTag::Kind::knockout);
                    }
                    hud_stats[1 - side].combo = false;
                }
            }
        }
        sample_practice_keys();
        if (!super_frozen) for (auto& fighter : practice_players) {
            auto& visual = fighter_visuals(fighter);
            if (!visual.buffer_ticks) continue;
            const auto buttons = std::exchange(visual.buffered_buttons, 0u);
            visual.buffer_ticks = 0;
            fighter.pressed_punches |= buttons & 7u;
            fighter.pressed_kicks |= buttons >> 3;
            if ((buttons & 9u) == 9u) fighter.throw_input = true;
            for (int button : {0, 3, 1, 4, 2, 5}) {
                if (!(buttons & (1u << button))) continue;
                fighter.normal_input = button;
                break;
            }
        }
        for (std::size_t side = 0; side < hud_stats.size(); ++side) {
            auto& stats = hud_stats[side];
            if (stats.notice_ticks) --stats.notice_ticks;
            auto& target = practice_players[1 - side];
            if (stats.combo && target.hurt_group < 0 && target.throw_phase != PracticeFighter::ThrowPhase::grabbed) {
                stats.combo = false;
                target.burst_available = false;
                if (training_value("OPT_STR_LIFEBAR_SETTING") == 0)
                    hud_stats[1 - side].life = training_life();
            }
        }
        if (!super_frozen) ++practice_stage_tick;
        const float half_width = 640.0f + 160.0f * practice_camera_zoom;
        // FUN_0057f150 publishes fighter limits 95 units inside the camera edges.
        const float left = std::max(0.0f, practice_camera_center - half_width + 95.0f);
        const float right = std::min(practice_width, practice_camera_center + half_width - 95.0f);
        const auto& dummy = practice_players[1];
        if (dummy.hurt_group >= 0 || dummy.blockstun) recent_dummy_contact = 25;
        if (recent_dummy_contact) --recent_dummy_contact;
        if (!super_frozen) {
            advance_training_bursts();
            advance_training_pushblock(left, right);
            sample_practice_team_actions();
            advance_practice_tags();
        }
        const float x1 = practice_players[0].practice_x;
        const float x2 = practice_players[1].practice_x;
        const auto advance_fighter = [&](std::size_t side, float opponent_x) {
            auto& fighter = practice_players[side];
            if (practice_super_frozen() && fighter_visuals(fighter).freeze == 0) {
                sample_super_freeze_inputs(fighter);
                return;
            }
            if (fighter.blockbuster_playback)
                fighter.blockbuster_playback->opponent = effect_parent_state(practice_players[1 - side]);
            if (fighter.peacock) {
                tick_peacock(fighter, opponent_x, left, right, side == 0);
                return;
            }
            if (fighter.squigly) {
                tick_squigly(fighter, opponent_x, left, right, side == 0);
                return;
            }
            if (fighter.fukua) {
                tick_fukua(fighter, opponent_x, left, right, side == 0);
                return;
            }
            if (fighter.valentine) {
                tick_valentine(fighter, opponent_x, left, right, side == 0);
                return;
            }
            if (fighter.parasoul) {
                tick_parasoul(fighter, opponent_x, left, right, side == 0);
                return;
            }
            if (fighter.robofortune) {
                tick_robofortune(fighter, opponent_x, left, right, side == 0);
                return;
            }
            if (fighter.painwheel) {
                tick_painwheel(fighter, opponent_x, left, right, side == 0);
                return;
            }
            if (fighter.msfortune) {
                tick_msfortune(fighter, opponent_x, left, right, side == 0);
                return;
            }
            if (fighter.cerebella) {
                tick_cerebella(fighter, opponent_x, left, right, side == 0);
                return;
            }
            auto& spikes = fighter.ringlet_spikes;
            if (!practice_super_frozen() && (!fighter.hit_stop || fighter.active_normal != skullgirls::ringlet_spike)) {
                spikes.erase(std::remove_if(spikes.begin(), spikes.end(),
                    [](RingletSpike& spike) { return !spike.advance(); }), spikes.end());
            }
            if (fighter.tricobezoar && !practice_super_frozen()) {
                auto& projectile = *fighter.tricobezoar;
                projectile.advance();
                if (projectile.x < -300 || projectile.x > practice_width + 300 ||
                    projectile.y < -300 || projectile.y > practice_height + 300) fighter.tricobezoar.reset();
            }
            const bool was_idle = !fighter.movement_sprite;
            const bool frozen = fighter.hit_stop != 0;
            const auto previous_burst_ticks = fighter.burst_ticks;
            const bool guard_locked = fighter.guard_locked();
            const bool was_above_ground = fighter.jump_height > 0;
            const bool was_recovering = fighter.air_hurt_fall;
            const bool was_ground_teching = fighter.ground_tech_direction != 0;
            const bool was_at_wall = fighter.wall_bounce_tick != 0;
            const bool awaiting_bounce = fighter.heavy_reaction == skullgirls::HeavyReaction::ground_bounce &&
                !fighter.recoil_landed;
            const auto& tag = practice_teams[side].tag;
            if (tag && (tag->kind == PracticeTag::Kind::regular || tag->kind == PracticeTag::Kind::snapback ||
                        tag->from_burst)) {
                fighter.normal_hit = nullptr;
                fighter.striking_normal = -1;
            } else {
                fighter.tick(opponent_x, left, right, side == 0, practice_ceiling(), filia_scripts());
                if (!guard_locked && !frozen) {
                    const auto& opponent = practice_players[1 - side];
                    const bool facing_right = practice_facing_right(side);
                    const bool holding_back = facing_right ? fighter.practice_left : fighter.practice_right;
                    const bool nearby_attack = opponent.active_normal >= 0 &&
                        std::abs(opponent_x - fighter.practice_x) < 360.0f;
                    fighter.advance_guard_pose(holding_back && nearby_attack, facing_right);
                }
            }
            if (!was_recovering && fighter.air_hurt_fall)
                spawn_combat_spark(side, CombatEffect::air_recovery, {fighter.practice_x, fighter.jump_height + 90});
            constexpr auto burst_effect_tick = PracticeFighter::burst_clip.duration() -
                PracticeFighter::burst_clip.record_tick(6) - 1;
            if (previous_burst_ticks == burst_effect_tick + 1 && fighter.burst_ticks == burst_effect_tick)
                spawn_combat_spark(side, CombatEffect::burst, {fighter.practice_x, fighter.jump_height + 250});
            if (!was_ground_teching && fighter.ground_tech_direction) {
                hud_stats[side].notice = L"Ground Tech";
                hud_stats[side].notice_ticks = 45;
            }
            if (!was_at_wall && fighter.wall_bounce_tick)
                spawn_combat_spark(side, CombatEffect::wall_impact, {fighter.practice_x, fighter.jump_height + 90});
            if ((was_above_ground && !fighter.jump_height && fighter.recoil_landed) ||
                (awaiting_bounce && fighter.recoil_landed))
                spawn_combat_spark(side, CombatEffect::ground_impact, {fighter.practice_x, 0});
            if (!frozen)
                fighter.idle_tick = was_idle && !fighter.movement_sprite
                    ? (fighter.idle_tick + 1) % skullgirls::stand_idle_ticks : 0;
        };
        advance_fighter(0, x2);
        if (practice_slot_playable(1, practice_teams[1].active_slot)) {
            advance_fighter(1, x1);
            if (!practice_super_frozen()) separate_practice_fighters(x1 <= x2, left, right);
        }
        if (!practice_super_frozen()) resolve_practice_bursts();

        std::array<const skullgirls::NormalHit*, 2> contacts{};
        std::array<skullgirls::NormalHit, 2> cerebella_hits{};
        std::array<RingletSpike*, 2> spike_contacts{};
        std::array<bool, 2> skullball_contacts{};
        std::array<Gdiplus::PointF, 2> impacts{};
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& player = practice_players[side];
            const auto& opponent = practice_players[1 - side];
            if (practice_super_frozen()) continue;
            resolve_fortune_head_strike(side);
            if (practice_point_snapped(1 - side) || opponent.ground_tech_direction || opponent.burst_protected() ||
                opponent.fenrir_captured || opponent.throw_phase == PracticeFighter::ThrowPhase::grabbed ||
                !practice_slot_playable(1 - side, practice_teams[1 - side].active_slot)) continue;
            if (player.blockbuster_playback) {
                const auto& actor = *player.blockbuster_playback;
                if (!actor.hit || actor.hit_resolved || player.hit_stop || player.hurt_group >= 0 || player.blockstun) continue;
                if (const auto impact = practice_attack_overlap(actor.pose, player.practice_x, player.jump_height,
                    actor.facing_right, opponent, !opponent.mirror(practice_facing_right(1 - side)))) {
                    cerebella_hits[side] = {0, actor.hit->stun, actor.hit->stop, actor.hit->height & 3u};
                    contacts[side] = &cerebella_hits[side];
                    impacts[side] = *impact;
                }
                continue;
            }
            if (player.fukua || player.squigly || player.valentine || player.robofortune || player.painwheel || player.peacock || player.parasoul || player.msfortune) {
                const auto& actor = *player.scripted();
                if (!player.scripted_attacking() || !actor.hit || actor.hit_resolved || player.hit_stop ||
                    player.hurt_group >= 0 || player.blockstun) continue;
                if (player.scripted_grab_successor() && !(player.robofortune && (actor.move==189 || actor.move==190)) && (!player.painwheel || actor.move == 146 || actor.move == 159) && ((actor.move == (player.fukua ? (actor.move==134?134u:133u) : player.squigly ? 152u : player.valentine ? 161u : player.robofortune ? 153u : player.painwheel ? 159u : player.msfortune ? 148u : player.parasoul ? 174u : 157u)) != (opponent.jump_height > 0 || opponent.jump_velocity > 0) ||
                    opponent.hurt_group >= 0 || opponent.blockstun || opponent.throw_phase == PracticeFighter::ThrowPhase::tech)) continue;
                if (const auto impact = practice_attack_overlap(actor.pose, player.practice_x, player.jump_height,
                    actor.facing_right, opponent, !opponent.mirror(practice_facing_right(1 - side)), false, player.peacock.has_value(), player.parasoul.has_value(), player.msfortune.has_value(), player.painwheel.has_value(), player.robofortune.has_value(),player.valentine.has_value(),player.squigly.has_value(),player.fukua.has_value())) {
                    const auto& hit = *actor.hit;
                    cerebella_hits[side] = {0, hit.stun, hit.stop, hit.height & 3u};
                    contacts[side] = &cerebella_hits[side];
                    impacts[side] = *impact;
                }
                continue;
            }
            if (player.cerebella) {
                const auto& bella = *player.cerebella;
                if (!bella.attacking() || !bella.hit || bella.hit_resolved || player.hit_stop ||
                    player.hurt_group >= 0 || player.blockstun) continue;
                if (bella.grab_successor()) {
                    const bool airborne_target = opponent.jump_height > 0 || opponent.jump_velocity > 0;
                    const bool aerial_grab = bella.move == 147 || bella.move == 157 || bella.move == 178;
                    const bool ground_grab = (bella.move >= 134 && bella.move <= 137) ||
                        bella.move == 166 || bella.move == 173 || bella.move == 218;
                    if ((aerial_grab && !airborne_target) || (ground_grab && airborne_target) ||
                        ((bella.hit->height & 4u) && (opponent.hurt_group >= 0 || opponent.blockstun))) continue;
                }
                if (const auto impact = practice_attack_overlap(bella.pose, player.practice_x, player.jump_height,
                    bella.facing_right, opponent, !opponent.mirror(practice_facing_right(1 - side)), true)) {
                    const auto& hit = *bella.hit;
                    cerebella_hits[side] = {0, hit.stun, hit.stop, hit.height & 3u};
                    contacts[side] = &cerebella_hits[side];
                    impacts[side] = *impact;
                }
                continue;
            }
            if (!player.normal_hit && player.ringlet_spikes.empty() && !player.tricobezoar) continue;
            const bool opponent_facing_right = !opponent.mirror(practice_facing_right(1 - side));
            const auto overlaps = [&](unsigned sprite, float x, float y, bool facing) {
                const auto impact = practice_attack_overlap(sprite, x, y, facing, opponent, opponent_facing_right);
                if (impact) impacts[side] = *impact;
                return impact.has_value();
            };
            if (player.movement_sprite >= 198 && player.normal_hit && player.normal_hit != player.last_contact &&
                overlaps(player.movement_sprite, player.practice_x, player.jump_height, player.normal_facing_right)) {
                contacts[side] = player.normal_hit;
            } else if (player.tricobezoar && player.tricobezoar->can_hit() &&
                overlaps(player.tricobezoar->sprite(), player.tricobezoar->x, player.tricobezoar->y,
                    player.tricobezoar->facing_right)) {
                skullball_contacts[side] = true;
            } else {
                for (auto& spike : player.ringlet_spikes) {
                    const auto sprite = spike.sprite();
                    if (spike.can_hit() && overlaps(sprite, spike.x, 0, spike.facing_right)) {
                        spike_contacts[side] = &spike;
                        break;
                    }
                }
            }
        }
        for (std::size_t side = 0; side < contacts.size(); ++side) {
            auto& player = practice_players[side];
            auto& opponent = practice_players[1 - side];
            if (skullball_contacts[side]) {
                const auto projectile = *player.tricobezoar;
                player.tricobezoar.reset();
                if (practice_guard(side, skullgirls::tricobezoar_hit, projectile.x >= opponent.practice_x)) {
                    spawn_combat_spark(side, CombatEffect::block, impacts[side]);
                } else {
                    opponent.take_hit(skullgirls::tricobezoar_hit, !projectile.facing_right, skullgirls::tricobezoar,
                        practice_hit_reaction(side, {skullgirls::HeavyReaction::launch, 10, 45}));
                    record_practice_damage(side, projectile.sprite() == 1192 ? 4750 : 3000,
                                           skullgirls::tricobezoar, projectile.attack_instance);
                    spawn_combat_spark(side, CombatEffect::heavy, impacts[side]);
                }
                continue;
            }
            if (auto* spike = spike_contacts[side]) {
                spike->hit = true;
                if (player.active_normal < 0 || player.active_normal == skullgirls::ringlet_spike)
                    player.normal_contact = true;
                if (practice_guard(side, skullgirls::ringlet_spike_hit, spike->x >= opponent.practice_x)) {
                    spawn_combat_spark(side, CombatEffect::block, impacts[side]);
                    continue;
                }
                spike->spin = ringlet_hit_spin(opponent);
                auto hit = skullgirls::ringlet_spike_hit;
                if (!opponent.jump_height && !opponent.jump_velocity)
                    hit.stop = skullgirls::ringlet_grounded_impact_hold;
                opponent.take_hit(hit, !spike->facing_right, skullgirls::ringlet_spike,
                    practice_hit_reaction(side, skullgirls::ringlet_spike_reaction));
                spawn_combat_spark(side, CombatEffect::medium, impacts[side], 0.8f);
                record_practice_damage(side, 600, skullgirls::ringlet_spike, spike->attack_instance);
                if (training_value("OPT_STR_TRAININGSHOWCROSSUPS") &&
                    (spike->x < opponent.practice_x) != (player.practice_x < opponent.practice_x)) {
                    hud_stats[side].notice = L"Cross-up";
                    hud_stats[side].notice_ticks = 60;
                }
                continue;
            }
            const auto* hit = contacts[side];
            if (!hit) continue;
            if (player.blockbuster_playback) {
                auto& actor = *player.blockbuster_playback;
                const bool facing_attacker = player.practice_x >= opponent.practice_x;
                const bool guarded = hit->height_mask && practice_guard(side, *hit, facing_attacker);
                actor.report_contact(guarded);
                player.normal_contact = true;
                player.normal_hit_landed |= !guarded;
                player.hit_stop = std::max(player.hit_stop, hit->stop);
                if (guarded) spawn_combat_spark(side, CombatEffect::block, impacts[side]);
                else {
                    const auto reaction = blockbuster_hit_reaction(side,*actor.hit);
                    opponent.take_hit(*hit, facing_attacker, player.striking_normal, practice_hit_reaction(side, reaction));
                    record_practice_damage(side, actor.hit->damage);
                    spawn_combat_spark(side, CombatEffect::heavy, impacts[side]);
                }
                continue;
            }
            if (player.fukua || player.squigly || player.valentine || player.robofortune || player.painwheel || player.peacock || player.parasoul || player.msfortune) {
                auto& actor = *player.scripted();
                const bool facing_attacker = player.practice_x >= opponent.practice_x;
                const bool guarded = hit->height_mask && practice_guard(side, *hit, facing_attacker);
                actor.report_contact(guarded);
                player.hit_stop = std::max(player.hit_stop, hit->stop);
                if (guarded) spawn_combat_spark(side, CombatEffect::block, impacts[side]);
                else {
                    if (const auto success = player.scripted_grab_successor()) {
                        if ((!player.painwheel || actor.move == 146 || actor.move == 159) &&
                            (!player.valentine || actor.move==152 || actor.move==161) &&
                            (!player.fukua || (actor.move>=120&&actor.move<=123) || actor.move==133 || actor.move==134) && practice_throw_tech_requested(side)) {
                            begin_practice_throw_tech(side, actor.move == (player.fukua ? (actor.move==134?134u:133u) : player.squigly ? 152u : player.valentine ? 161u : player.robofortune ? 153u : player.painwheel ? 159u : player.msfortune ? 148u : player.parasoul ? 174u : 157u), impacts[side]);
                            continue;
                        }
                        actor.captured_target = true;
                        actor.release_target = false;
                        for (auto& slot : actor.target_state.variables) slot = 0.0f;
                        actor.target_state.variables[0x38] = facing_attacker ? 1.0f : -1.0f;
                        actor.target_state.variables[0x53] = 1.0f;
                        actor.start(success);
                        opponent.throw_phase = PracticeFighter::ThrowPhase::grabbed;
                        opponent.active_normal = opponent.queued_followup = -1;
                        opponent.normal_hit = nullptr;
                        opponent.hit_stop = 0;
                        spawn_combat_spark(side, CombatEffect::grab, impacts[side]);
                        continue;
                    }
                    if (actor.move != (player.fukua ? 200u : player.squigly ? 297u : player.valentine ? 255u : player.robofortune ? 217u : player.painwheel ? 247u : player.msfortune ? 218u : player.parasoul ? 229u : 219u) || !try_practice_snapback(side, *hit))
                        opponent.take_hit(*hit, facing_attacker, -1,
                            practice_hit_reaction(side, cerebella_hit_reaction(*actor.hit, opponent)));
                    record_practice_damage(side, actor.hit->damage);
                    const auto tier = actor.hit->strength;
                    spawn_combat_spark(side, tier <= 1 ? CombatEffect::light : tier == 2 ? CombatEffect::medium :
                        CombatEffect::heavy, impacts[side]);
                }
                continue;
            }
            if (player.cerebella) {
                auto& bella = *player.cerebella;
                const auto facing_attacker = player.practice_x >= opponent.practice_x;
                const bool guarded = hit->height_mask && practice_guard(side, *hit, facing_attacker);
                bella.report_contact(guarded);
                player.hit_stop = std::max(player.hit_stop, hit->stop);
                if (guarded) spawn_combat_spark(side, CombatEffect::block, impacts[side]);
                else {
                    if (const unsigned success = bella.grab_successor()) {
                        if ((bella.move <= 137 || bella.move == 147) && practice_throw_tech_requested(side)) {
                            begin_practice_throw_tech(side, bella.move == 147, impacts[side]);
                            continue;
                        }
                        bella.captured_target = true;
                        bella.release_target = false;
                        for (auto& slot : bella.target_state.variables) slot = 0.0f;
                        bella.target_state.variables[0x38] = opponent.mirror(practice_facing_right(1 - side)) ? -1.0f : 1.0f;
                        bella.target_state.variables[0x53] = 1.0f;
                        bella.start(success);
                        opponent.throw_phase = PracticeFighter::ThrowPhase::grabbed;
                        opponent.active_normal = opponent.queued_followup = -1;
                        opponent.normal_hit = nullptr;
                        opponent.hit_stop = 0;
                        spawn_combat_spark(side, CombatEffect::grab, impacts[side]);
                        continue;
                    }
                    if (bella.move != 238 || !try_practice_snapback(side, *hit))
                        opponent.take_hit(*hit, facing_attacker, -1,
                            practice_hit_reaction(side, cerebella_hit_reaction(*bella.hit, opponent)));
                    record_practice_damage(side, bella.hit->damage);
                    const auto tier = bella.hit->strength;
                    spawn_combat_spark(side, tier <= 1 ? CombatEffect::light : tier == 2 ? CombatEffect::medium :
                        CombatEffect::heavy, impacts[side]);
                }
                continue;
            }
            player.last_contact = hit;
            player.normal_contact = true;
            player.hit_stop = std::max(player.hit_stop, hit->stop);
            const bool facing_attacker = player.practice_x >= opponent.practice_x;
            if (practice_guard(side, *hit, facing_attacker)) {
                if (player.striking_normal == skullgirls::air_hairball) ++player.airball_blocks;
                spawn_combat_spark(side, CombatEffect::block, impacts[side]);
                continue;
            }
            player.normal_hit_landed = true;
            if (player.striking_normal == skullgirls::air_hairball) ++player.airball_hits;
            const auto counter = training_value("OPT_STR_FORCECOUNTERHIT");
            const bool force_counter = side == 0 && !dummy_input_override() &&
                training_contact_mode(counter == 1 ? 3 : counter == 3 ? 1 : counter, recent_dummy_contact == 0);
            if (force_counter) {
                hud_stats[side].notice = L"Counter Hit";
                hud_stats[side].notice_ticks = 60;
            } else if (training_value("OPT_STR_TRAININGSHOWCROSSUPS") &&
                       ((side == 0 ? x1 < x2 : x2 < x1) != (player.practice_x < opponent.practice_x))) {
                hud_stats[side].notice = L"Cross-up";
                hud_stats[side].notice_ticks = 60;
            }
            if (player.striking_normal != skullgirls::filia_snapback || !try_practice_snapback(side, *hit))
                opponent.take_hit(*hit, !player.normal_facing_right, player.striking_normal, practice_hit_reaction(side));
            if (player.striking_normal >= 0) {
                const auto move = player.striking_normal;
                if (move == skullgirls::fenrir_start || move == skullgirls::fenrir_rise ||
                    move == skullgirls::fenrir_peak) player.fenrir_connected = true;
                const auto contact = hit - skullgirls::filia_normal_hits[move];
                constexpr CombatEffect l = CombatEffect::light, m = CombatEffect::medium, h = CombatEffect::heavy;
                // Filia's normal hit descriptors store strength tiers 1, 2 and 3.
                constexpr CombatEffect strike_effects[skullgirls::filia_taunt + 1][2] = {
                    {l}, {m}, {h}, {l}, {m}, {h}, {l}, {m}, {h}, {l}, {m, m}, {h},
                    {l}, {m}, {h}, {l}, {m, m}, {h}, {l}, {l}, {m, m}, {m, m}, {h, h}, {m, h}, {m, h},
                };
                const auto effect = move >= skullgirls::gregor_start ? CombatEffect::heavy :
                    strike_effects[move][contact != 0];
                spawn_combat_spark(side, effect, impacts[side]);
                const auto damage = skullgirls::filia_hit_damage[move][contact];
                record_practice_damage(side, damage, move, player.attack_instance);
            }
        }
        if (!practice_super_frozen()) {
            resolve_fenrir(left, right);
            resolve_scripted_grabs(left, right);
            resolve_practice_throws(left, right);
            advance_practice_assists(left, right);
        }
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            collect_practice_visuals(practice_players[side], side, practice_teams[side].active_slot);
            if (auto& assist = practice_teams[side].assist; assist && assist->phase == PracticeAssist::Phase::acting)
                collect_practice_visuals(assist->fighter, side, assist->actor.slot);
        }
        advance_practice_effects(practice_super_frozen());
        resolve_scripted_projectiles();
        update_practice_camera(false);
    }

    void resolve_scripted_grabs(float left, float right) {
        using Phase = PracticeFighter::ThrowPhase;
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& attacker = practice_players[side];
            if (!attacker.scripted() || !attacker.scripted()->captured_target) continue;
            auto& bella = *attacker.scripted();
            auto& defender = practice_players[1 - side];
            if (bella.throw_damage) record_practice_damage(side, std::exchange(bella.throw_damage, 0u));
            const bool buer = attacker.painwheel && (bella.move == 186 || bella.move == 188 ||
                bella.move == 190 || bella.move == 193 || bella.move == 196 || bella.move == 199);
            // Buer's zero-damage catch leads to a separate damaging hitbox.
            // Keep the captured victim synchronized until that impact record.
            if (buer && bella.hit && bella.hit->damage && !bella.hit_resolved) {
                const auto& source = *bella.hit;
                defender.take_hit({0,source.stun,source.stop,source.height & 3u}, !bella.facing_right, -1,
                    practice_hit_reaction(side, cerebella_hit_reaction(source, defender)));
                record_practice_damage(side, source.damage);
                bella.report_contact(false);
                bella.captured_target = bella.release_target = false;
                continue;
            }
            if ((bella.release_target && !buer) || !attacker.scripted_attacking() || attacker.hurt_group >= 0) {
                const float forward = bella.target_state.variables[0x38].value_or(-1.0f);
                const float horizontal = bella.target_state.variables[0x9e].value_or(0.0f);
                const float upward = bella.target_state.variables[0x9f].value_or(0.0f);
                constexpr skullgirls::NormalHit release{0, 20, 0, 3};
                defender.take_hit(release, forward > 0, -1,
                    {skullgirls::HeavyReaction::knockback, std::abs(horizontal), upward});
                bella.captured_target = bella.release_target = false;
                continue;
            }
            defender.throw_phase = Phase::grabbed;
            const auto pose = static_cast<unsigned>(std::max(1.0f, bella.value(0x6f)));
            const auto& table = defender.squigly ? squigly_assets.animations[437] : defender.fukua ? fukua_assets.animations[340] : defender.valentine ? valentine_assets.animations[395] : defender.robofortune ? robofortune_assets.animations[357] : defender.painwheel ? painwheel_assets.animations[387] : defender.msfortune ? msfortune_assets.animations[358] : defender.parasoul ? parasoul_assets.animations[369] : defender.peacock ? peacock_assets.animations[359] :
                defender.cerebella ? cerebella_assets.animations[378] : filia_move_table.animations[336];
            const auto& frame = table[std::min<std::size_t>(pose - 1, table.size() - 1)];
            if (frame.sprites[1] > 1) {
                defender.movement_sprite = frame.sprites[1];
                if (defender.scripted()) defender.scripted()->pose = frame.sprites[1];
            }
            const float forward = bella.facing_right ? 1.0f : -1.0f;
            defender.practice_x = std::clamp(attacker.practice_x + bella.value(0x70) * forward, left, right);
            defender.jump_height = std::max(0.0f, attacker.jump_height + bella.value(0x71));
            defender.jump_velocity = 0;

        }
    }

    void resolve_fenrir(float left, float right) {
        using namespace skullgirls;
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& attacker = practice_players[side];
            auto& defender = practice_players[1 - side];
            if (attacker.blockbuster_playback) {
                auto& playback = *attacker.blockbuster_playback;
                if (playback.move == 188 && playback.value(0x6f) > 0 && !playback.captured_target) {
                    playback.captured_target = true;
                    playback.capture_state = effect_parent_state(defender);
                    playback.capture_state.variables[0x53] = 1.0f;
                    defender.fenrir_captured = true;
                    defender.active_normal = defender.queued_followup = -1;
                }
                if (playback.capture_damage) {
                    record_practice_damage(side, std::exchange(playback.capture_damage, 0u));
                    spawn_combat_spark(side, CombatEffect::heavy, {defender.practice_x, defender.jump_height});
                }
                if (defender.fenrir_captured) {
                    const float forward = playback.facing_right ? 1.0f : -1.0f;
                    if (playback.move == 190 && playback.value(0x6f) < 0) {
                        constexpr NormalHit release{0, 16, 0, 3};
                        defender.take_hit(release, !playback.facing_right, fenrir_land,
                            {HeavyReaction::ground_bounce, 18, -18});
                        playback.captured_target = false;
                    } else {
                        set_throw_victim_pose(defender, throw_victim_pose(4));
                        defender.practice_x = std::clamp(attacker.practice_x + playback.value(0x70) * forward, left, right);
                        defender.jump_height = std::max(0.0f, attacker.jump_height - playback.value(0x71));
                        defender.jump_velocity = 0;
                    }
                }
                continue;
            }
            const auto move = attacker.active_normal;
            const float forward = attacker.normal_facing_right ? 1.0f : -1.0f;
            const auto impact = Gdiplus::PointF{attacker.practice_x + 150 * forward, attacker.jump_height};
            if (move == fenrir_bite && attacker.fenrir_connected && attacker.normal_frame >= 4 && attacker.normal_frame <= 5 &&
                !attacker.normal_tick && !defender.fenrir_captured) {
                defender.fenrir_captured = true;
                defender.active_normal = defender.queued_followup = -1;
                defender.hurt_facing_right = !attacker.normal_facing_right;
                set_throw_victim_pose(defender, throw_victim_pose(4));
                record_practice_damage(side, 3500);
                spawn_combat_spark(side, CombatEffect::heavy, impact);
            }
            if (!defender.fenrir_captured) continue;
            if (move == fenrir_bite || move == fenrir_dive) {
                defender.practice_x = std::clamp(impact.X, left, right);
                defender.jump_height = impact.Y + 100;
                defender.jump_velocity = 0;
                continue;
            }
            constexpr NormalHit release{0, 16, 0, 3};
            if (move == fenrir_land) {
                record_practice_damage(side, 500);
                spawn_combat_spark(side, CombatEffect::ground_impact, {defender.practice_x, 0});
                defender.take_hit(release, !attacker.normal_facing_right, fenrir_land,
                    {HeavyReaction::ground_bounce, 18, -18});
            } else {
                defender.take_hit(release, !attacker.normal_facing_right, -1);
            }
        }
    }

    void record_practice_damage(std::size_t side, unsigned damage, int move = -1, std::uint64_t instance = 0) {
        if (!damage) return;
        if (side == 0) recent_dummy_contact = 25;
        auto& stats = hud_stats[side];
        if (!stats.combo) {
            stats.damage = stats.hits = 0;
            stats.combo_attacks = {};
        }
        stats.combo = true;
        if (move >= 0 && instance) {
            auto& last = stats.combo_attacks[move];
            if (last && last != instance) practice_players[1 - side].burst_available = true;
            last = instance;
        }
        stats.last_hit = damage;
        stats.damage += damage;
        stats.maximum = std::max(stats.maximum, stats.damage);
        ++stats.hits;
        auto& life = hud_stats[1 - side].life;
        if (training_value("OPT_STR_LIFEBAR_SETTING") != 2) {
            const unsigned minimum = !training_value("OPT_STR_DEATHALLOWED");
            life = std::max(minimum, life - std::min(life, damage));
        }
    }

    void prepare_hud_portraits() {
        skullgirls::SpriteArchive assets(game_data / "ui.gfs");
        skullgirls::SpriteArchive characters(game_data / "characters-foits.gfs");
        hud_overlay_state.reset();
        hud_portraits = {};
        for (std::size_t side = 0; side < hud_portraits.size(); ++side)
        for (std::size_t slot = 0; slot < fighters[side].size(); ++slot) {
            const auto fighter = fighters[side][slot];
            const auto name = "ui/palettized_hud_art/character_" + std::string(roster[fighter]);
            const auto load = [&](const char* suffix) {
                return skullgirls::read_hud_portrait_layer(assets.read(name + suffix));
            };
            const auto index = load("_colors.pcx");
            const auto shade = load("_shade.pcx");
            const auto lines = load("_lines.pcx");
            const auto& palette = palettes[fighter][locked_colors[side][slot]].colors;
            const auto palette_map=skullgirls::read_hud_palette_map(
                characters.read("characters/" + std::string(roster[fighter]) + ".foit"));
            const auto pixels = skullgirls::colorize_hud_portrait(index, shade, lines, palette, palette_map);
            skullgirls::SelectFrame portrait{};
            portrait.width = index.width;
            portrait.height = index.height;
            hud_portraits[side][slot] = make_fighter_bitmap(portrait,
                [&](std::size_t pixel) { return pixels[pixel]; });
        }
    }

    void set_throw_victim_pose(PracticeFighter& defender, unsigned filia_pose) {
        if (!defender.scripted()) {
            defender.movement_sprite = filia_pose;
            return;
        }
        const auto& original = filia_move_table.animations[336];
        const auto record = std::find_if(original.begin(), original.end(), [&](const auto& frame) {
            return frame.sprites[1] == filia_pose;
        });
        const auto& poses = defender.squigly ? squigly_assets.animations[437] : defender.fukua ? fukua_assets.animations[340] : defender.valentine ? valentine_assets.animations[395] : defender.robofortune ? robofortune_assets.animations[357] : defender.painwheel ? painwheel_assets.animations[387] : defender.msfortune ? msfortune_assets.animations[358] : defender.parasoul ? parasoul_assets.animations[369] : defender.peacock ? peacock_assets.animations[359] : cerebella_assets.animations[378];
        const auto index = record == original.end() ? 0u : static_cast<unsigned>(record - original.begin());
        const auto sprite = poses[std::min<std::size_t>(index, poses.size() - 1)].sprites[1];
        if (sprite > 1) defender.scripted()->pose = defender.movement_sprite = sprite;
    }

    bool practice_throw_tech_requested(std::size_t attacker_side) {
        const auto tech = training_value("OPT_STR_THROWTECH");
        const auto& defender = practice_players[1 - attacker_side];
        return defender.throw_input || defender.throw_phase == PracticeFighter::ThrowPhase::start ||
            (attacker_side == 0 && !dummy_input_override() &&
                (tech == 1 || (tech == 2 && recent_dummy_contact != 0) ||
                    (tech == 3 && training_roll(10.0f) <= 5.0f)));
    }

    void begin_practice_throw_tech(std::size_t attacker_side, bool air, Gdiplus::PointF impact) {
        auto& attacker = practice_players[attacker_side];
        const bool facing = attacker.scripted() ? attacker.scripted()->facing_right : attacker.throw_facing_right;
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& fighter = practice_players[side];
            fighter.throw_phase = PracticeFighter::ThrowPhase::tech;
            fighter.aerial_throw = air;
            fighter.throw_facing_right = side == attacker_side ? facing : !facing;
            fighter.throw_tick = 0;
            fighter.throw_input = false;
            fighter.hit_stop = 0;
            fighter.active_normal = fighter.queued_followup = -1;
            fighter.normal_hit = nullptr;
            fighter.blockstun = 0;
            fighter.dash_direction = fighter.air_dash_direction = 0;
            fighter.jump_horizontal = 0;
            if (fighter.cerebella) {
                fighter.cerebella->facing_right = fighter.throw_facing_right;
                fighter.cerebella->start((air ? 151u : 142u) + unsigned(side != attacker_side));
            } else if (fighter.peacock) {
                fighter.peacock->facing_right = fighter.throw_facing_right;
                fighter.peacock->start((air ? 160u : 152u) + unsigned(side != attacker_side));
            } else if (fighter.squigly) {
                fighter.squigly->facing_right = fighter.throw_facing_right;
                fighter.squigly->start((air ? 156u : 147u) + unsigned(side != attacker_side));
            } else if (fighter.fukua) {
                fighter.fukua->facing_right = fighter.throw_facing_right;
                fighter.fukua->start((air ? 139u : 128u) + unsigned(side != attacker_side));
            } else if (fighter.valentine) {
                fighter.valentine->facing_right = fighter.throw_facing_right;
                fighter.valentine->start((air ? 165u : 155u) + unsigned(side != attacker_side));
            }
            if (fighter.parasoul) {
                fighter.parasoul->facing_right = fighter.throw_facing_right;
                fighter.parasoul->start((air ? 178u : 169u) + unsigned(side != attacker_side));
            } else if (fighter.robofortune) {
                fighter.robofortune->facing_right=fighter.throw_facing_right;
                fighter.robofortune->start((air?157u:148u)+unsigned(side!=attacker_side));
            } else if (fighter.painwheel) {
                fighter.painwheel->facing_right = fighter.throw_facing_right;
                fighter.painwheel->start((air ? 168u : 154u) + unsigned(side != attacker_side));
            } else if (fighter.msfortune) {
                fighter.msfortune->facing_right = fighter.throw_facing_right;
                fighter.msfortune->start((air ? 152u : 143u) + unsigned(side != attacker_side));
            } else fighter.advance_throw();
        }
        spawn_combat_spark(attacker_side, CombatEffect::throw_tech, impact);
        hud_stats[attacker_side].notice = L"Throw Tech";
        hud_stats[attacker_side].notice_ticks = 45;
    }

    void resolve_practice_throws(float left, float right) {
        using Phase = PracticeFighter::ThrowPhase;
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& attacker = practice_players[side];
            auto& defender = practice_players[1 - side];
            if (attacker.scripted()) continue;
            const float forward = attacker.throw_facing_right ? 1.0f : -1.0f;
            const bool air = attacker.aerial_throw;
            const bool defender_air = defender.jump_height > 0 || defender.jump_velocity > 0;
            const unsigned grab_pose = air ? 441 : 401;
            // Ground throw tests contact as move 117/118 advances through its first grab record.
            if (attacker.throw_phase == Phase::start && attacker.throw_tick >= (air ? 10u : 7u) &&
                (!air || attacker.throw_tick <= 12) &&
                attacker.movement_sprite == grab_pose && (!air || attacker.jump_height > 0) &&
                defender.throw_phase != Phase::grabbed && !defender.fenrir_captured && defender.hurt_group < 0 &&
                defender.throw_phase != Phase::tech && !practice_point_snapped(1 - side) && !defender.ground_tech_direction &&
                !defender.burst_protected() && defender_air == air &&
                practice_slot_playable(1 - side, practice_teams[1 - side].active_slot)) {
                const auto& grab = combat_metadata[grab_pose - 1].box_groups[0];
                const unsigned pose = defender.sprite();
                const auto& hurt = practice_metadata(defender)[pose - 1].box_groups[1];
                for (const auto& box : grab) {
                    if (attacker.throw_phase != Phase::start) break;
                    const auto world_grab = skullgirls::foit_world_box(box, static_cast<int>(attacker.practice_x),
                        static_cast<int>(attacker.jump_height), attacker.throw_facing_right);
                    for (const auto& target : hurt) {
                        const auto world_hurt = skullgirls::foit_world_box(target, static_cast<int>(defender.practice_x),
                            static_cast<int>(defender.jump_height), !defender.mirror(practice_facing_right(1 - side)));
                        if (skullgirls::foit_boxes_overlap(world_grab, world_hurt)) {
                            const auto impact = contact_center(world_grab, world_hurt);
                            if (practice_throw_tech_requested(side)) {
                                begin_practice_throw_tech(side, air, impact);
                                break;
                            }
                            attacker.throw_phase = Phase::capture;
                            attacker.throw_tick = 0;
                            if (air) attacker.jump_velocity = 0;
                            defender.throw_phase = Phase::grabbed;
                            defender.aerial_throw = air;
                            defender.throw_facing_right = !attacker.throw_facing_right;
                            defender.active_normal = defender.queued_followup = -1;
                            set_throw_victim_pose(defender, skullgirls::throw_victim_pose(1));
                            spawn_combat_spark(side, CombatEffect::grab, impact);
                            break;
                        }
                    }
                }
            }
            if (defender.throw_phase != Phase::grabbed) continue;
            if (attacker.throw_phase == Phase::capture && (air || attacker.movement_sprite != 417)) {
                if (air) {
                    const auto pose = attacker.movement_sprite;
                    set_throw_victim_pose(defender, skullgirls::air_throw_victim_pose(pose));
                    defender.throw_facing_right = pose >= 446 && pose <= 453 ?
                        attacker.throw_facing_right : !attacker.throw_facing_right;
                    // Move 133 advances the target offsets at each pose from 445 through 451.
                    const auto tick = attacker.throw_tick;
                    const int updates = (tick >= 19) + (tick >= 23) + (tick >= 27) +
                        (tick >= 31) + (tick >= 35) + (tick >= 39) + (tick >= 45);
                    const float offset_x = -8.0f / 9.0f * updates;
                    const float offset_y = 28.0f * updates - 1.875f * updates * (updates - 1) * 0.5f;
                    defender.practice_x = std::clamp(attacker.practice_x + offset_x * forward, left, right);
                    defender.jump_height = std::max(0.0f, attacker.jump_height - offset_y);
                    defender.jump_velocity = 0;
                    continue;
                }
                // Move 122 selects Throw_Stun pose 4 once the victim is enveloped.
                set_throw_victim_pose(defender, skullgirls::throw_victim_pose(
                    attacker.movement_sprite >= 411 ? 4 : 1));
                // Target offsets written at frames 0, 4, 7 and 10 of move 122.
                const unsigned tick = attacker.throw_tick;
                const float separation = tick <= 15 ? 242.0f : tick <= 18 ? 170.0f : tick <= 21 ? 99.0f : 0.0f;
                defender.practice_x = std::clamp(attacker.practice_x + separation * forward, left, right);
            } else {
                const skullgirls::NormalHit release{0, 0, 0, 3};
                if (defender.aerial_throw) {
                    defender.take_hit(release, !attacker.throw_facing_right, -1,
                        {skullgirls::HeavyReaction::back_knockdown, 25, 15});
                } else {
                    // Moves 122/123 release behind the current facing with X = -26, Y = +8.
                    defender.practice_x = std::clamp(attacker.practice_x - 140.0f * forward, left, right);
                    defender.jump_height = attacker.jump_height + 75.0f;
                    defender.take_hit(release, attacker.throw_facing_right, -1,
                        {skullgirls::HeavyReaction::knockback, 26, 8});
                }
            }
        }
    }

    bool training_grid_enabled() const {
        return training_value("OPT_STR_TRAININGGRID") == 1;
    }

    static int hud_life_width(float life, float width) {
        return static_cast<int>(std::round(width * std::clamp(life / 15500.0f, 0.0f, 1.0f)));
    }

    HudOverlayState practice_hud_state() const {
        const auto attack_data = practice_presentation.phase == PracticePresentation::Phase::none ?
            training_value("OPT_STR_ATTACK_DATA", 1) : 0;
        HudOverlayState result{attack_data, {}};
        for (std::size_t side = 0; side < practice_teams.size(); ++side) {
            const auto& team = practice_teams[side];
            const auto& stats = hud_stats[side];
            const auto& point = practice_players[side];
            auto& state = result.second[side];
            state.active_slot = team.active_slot;
            state.meter = point.tension;
            state.assist_ready = !team.assist && !team.tag && !team.departing &&
                !team.assist_cooldown && next_practice_reserve(side).has_value();
            for (std::size_t slot = 0; slot < fighters[side].size(); ++slot) {
                const bool active = slot == team.active_slot;
                const PracticeLife& life = active ? static_cast<const PracticeLife&>(stats) : team.health[slot];
                const float width = active ? 375.0f : 286.0f;
                state.life_bars[slot] = {hud_life_width(life.displayed_life, width), hud_life_width(life.damage_trail, width)};
            }
            if (attack_data) state.attack_stats = {stats.life, stats.last_hit, stats.damage, stats.maximum, stats.hits};
            if (attack_data == 2 && point.hurt_group >= 0) state.hitstun = point.hurt_hold;
        }
        return result;
    }

    void paint_practice_hud(Gdiplus::Graphics& dc) const {
        if (!hud_atlas[0].bitmap) return;
        constexpr int width = 1280, height = 286;
        const auto state = practice_hud_state();
        if (!hud_overlay) {
            auto image = std::make_unique<Gdiplus::Bitmap>(width, height, PixelFormat32bppPARGB);
            if (image->GetLastStatus() == Gdiplus::Ok) hud_overlay = std::move(image);
        }
        if (hud_overlay) {
            if (hud_overlay_state != state) {
                Gdiplus::Graphics raster(hud_overlay.get());
                const bool both = !hud_overlay_state || hud_overlay_state->first != state.first;
                const bool left = both || !(hud_overlay_state->second[0] == state.second[0]);
                const bool right = both || !(hud_overlay_state->second[1] == state.second[1]);
                const Gdiplus::Rect redraw(left ? 0 : width / 2, 0, left && right ? width : width / 2, height);
                raster.SetClip(redraw);
                raster.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
                const Gdiplus::SolidBrush transparent(Gdiplus::Color::Transparent);
                raster.FillRectangle(&transparent, redraw);
                raster.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
                raster.SetInterpolationMode(dc.GetInterpolationMode());
                raster.SetPixelOffsetMode(dc.GetPixelOffsetMode());
                paint_practice_hud_overlay(raster, state);
                hud_overlay_state = state;
            }
            const auto saved = dc.Save();
            dc.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
            dc.DrawImage(hud_overlay.get(), 0, 0, width, height);
            dc.Restore(saved);
        } else {
            paint_practice_hud_overlay(dc, state);
        }
        paint_practice_inputs(dc);
    }

    void paint_practice_hud_overlay(Gdiplus::Graphics& dc, const HudOverlayState& overlay) const {
        const auto tile = [&](unsigned page, int sx, int sy, int sr, int sb, Gdiplus::RectF bounds) {
            const auto& atlas = hud_atlas[page];
            const auto x = std::floor(bounds.X), y = std::floor(bounds.Y);
            const auto offset_x = bounds.X - x, offset_y = bounds.Y - y;
            const HudTileKey key{{static_cast<int>(page), sx, sy, sr, sb},
                {bounds.Width, bounds.Height, offset_x, offset_y}};
            const auto [entry, inserted] = hud_tiles.try_emplace(key);
            if (inserted) {
                // Rasterize the atlas region once at HUD scale, including its subpixel placement.
                auto image = std::make_unique<Gdiplus::Bitmap>(
                    static_cast<int>(std::ceil(offset_x + bounds.Width)),
                    static_cast<int>(std::ceil(offset_y + bounds.Height)), PixelFormat32bppPARGB);
                if (image->GetLastStatus() == Gdiplus::Ok) {
                    Gdiplus::Graphics raster(image.get());
                    raster.Clear(Gdiplus::Color::Transparent);
                    raster.SetInterpolationMode(dc.GetInterpolationMode());
                    raster.SetPixelOffsetMode(dc.GetPixelOffsetMode());
                    const Gdiplus::RectF destination(offset_x, offset_y, bounds.Width, bounds.Height);
                    if (raster.DrawImage(atlas.pixels(), destination, sx, sy, sr - sx, sb - sy,
                        Gdiplus::UnitPixel) == Gdiplus::Ok) entry->second = std::move(image);
                }
            }
            if (entry->second) {
                auto* image = entry->second.get();
                dc.DrawImage(image, static_cast<INT>(x), static_cast<INT>(y),
                    static_cast<INT>(image->GetWidth()), static_cast<INT>(image->GetHeight()));
            } else {
                dc.DrawImage(atlas.pixels(), bounds, sx, sy, sr - sx, sb - sy, Gdiplus::UnitPixel);
            }
        };
        const auto state = dc.Save();
        dc.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        tile(0, 958, 401, 1290, 551, {529, 85, 221, 100});
        for (std::size_t side = 0; side < hud_stats.size(); ++side) {
            const auto mirrored = dc.Save();
            if (side) {
                dc.TranslateTransform(1280, 0);
                dc.ScaleTransform(-1, 1);
            }
            // P1_Bar is at -5; mirroring its 562-wide group gives P2_Bar at 723.
            dc.TranslateTransform(-5, 0);
            // InGame.gbs P1/P2_F1–F3 health objects and the two roster-size covers.
            const auto& team = practice_teams[side];
            const auto& hud = overlay.second[side];
            const auto count = fighters[side].size();
            tile(0, 592, 579, 1165, 663, {170, 52, 382, 56});
            tile(0, 592, 752, 1435, 1006, {4, 13, 562, 169});
            const auto draw_life = [&](const std::array<int, 2>& bars, Gdiplus::RectF bounds, unsigned row) {
                const float fill = static_cast<float>(bars[0]);
                const float trail = static_cast<float>(bars[1]);
                const auto layer = [&](unsigned page, int left, int top, int right, int bottom, float visible) {
                    if (visible <= 0) return;
                    const auto clip = dc.Save();
                    dc.SetClip(Gdiplus::RectF(bounds.X, bounds.Y, visible, bounds.Height),
                               Gdiplus::CombineModeIntersect);
                    tile(page, left, top, right, bottom, bounds);
                    dc.Restore(clip);
                };
                // Original black covers, red damage trail, and distinct main,
                // middle and top energy gradients from InGame.gbs's atlas table.
                if (row == 0) tile(0, 592, 703, 1437, 748, bounds);
                else tile(0, 592, 667, 1236, 699, bounds);
                layer(0, 1169, 579, 1732, 609, trail);
                if (row == 0) layer(1, 825, 1559, 1388, 1589, fill);
                else if (row == 1) layer(1, 825, 1593, 1254, 1614, fill);
                else layer(1, 825, 1677, 1254, 1698, fill);
                if (row == 0 && trail - fill > 0.5f && fill > 0)
                    tile(0, 581, 492, 612, 549, {bounds.X + fill - 10.5f, 78.75f, 21, 38});
            };
            draw_life(hud.life_bars[hud.active_slot], {175, 84, 375, 20}, 0);
            if (count == 1)
                tile(0, 1441, 703, 1993, 747, {167, 54, 368, 29});
            else if (count == 2)
                tile(0, 1439, 752, 1969, 776, {166, 53, 353, 16});

            struct EyePortrait { std::string_view fighter; unsigned page; int x, y; };
            static constexpr EyePortrait eyes[] = {
                {"cerebella", 0, 697, 467}, {"filia", 1, 689, 66}, {"peacock", 1, 689, 86},
                {"parasoul", 1, 781, 66}, {"msfortune", 1, 781, 86}, {"painwheel", 1, 938, 90},
                {"double", 1, 1030, 90}, {"marie", 1, 1122, 90}, {"valentine", 1, 1214, 90},
                {"squigly", 0, 697, 447}, {"bigband", 1, 1523, 89}, {"fukua", 1, 1689, 89},
                {"eliza", 1, 1855, 89}, {"beowulf", 1, 1947, 89}, {"robofortune", 1, 554, 997},
                {"annie", 1, 553, 1016}, {"umbrella", 1, 205, 1200},
                {"blackdahlia", 1, 205, 1218}, {"bossmarie", 1, 659, 1357},
            };
            unsigned reserve_row = 0;
            for (std::size_t slot = 0; slot < count; ++slot) {
                if (slot == team.active_slot) continue;
                const bool middle = reserve_row++ == 0;
                draw_life(hud.life_bars[slot], {middle ? 250.0f : 234.0f, middle ? 69.0f : 55.0f, 286, 14},
                    middle ? 1 : 2);
                tile(0, 651, 447, 693, 485, {middle ? 233.0f : 220.0f, middle ? 62.0f : 47.0f, 28, 25});
                const auto eye = std::find_if(std::begin(eyes), std::end(eyes), [&](const auto& entry) {
                    return entry.fighter == roster[fighters[side][slot]];
                });
                if (eye != std::end(eyes))
                    tile(eye->page, eye->x, eye->y, eye->x + 88, eye->y + 16,
                         {middle ? 173.0f : 166.0f, middle ? 70.5f : 57.0f, 59, 11});
            }
            if (count > 1) {
                if (hud.assist_ready) tile(0, 789, 526, 851, 559, {205.59f, 105, 41, 22});
                else tile(1, 873, 66, 934, 99, {205.59f, 105, 41, 22});
            }
            const auto stock = hud.meter;
            // The authored CCRA SuperBar_0..5 objects use these stock colors.
            constexpr Gdiplus::ARGB meter_colors[] = {
                0xff000000, 0xff008000, 0xff40e0d0, 0xff8a2be2, 0xfff08080, 0xffffa500
            };
            const Gdiplus::RectF arc(9.38f, -8.864f, 178.89f, 181.11f);
            Gdiplus::Pen meter_edge(Gdiplus::Color(255, 64, 30, 16), 13);
            Gdiplus::Pen meter{Gdiplus::Color(meter_colors[stock]), 9};
            dc.DrawArc(&meter_edge, arc, 44, 155);
            dc.DrawArc(&meter, arc, 44, 155);
            // PortraitBg has an opaque black interior; the palettized portrait
            // follows it in the original scene's draw order.
            tile(0, 88, 864, 325, 1092, {17, 8, 158, 152});
            const auto& portrait = hud_portraits[side][team.active_slot];
            if (portrait) {
                const auto portrait_clip = dc.Save();
                dc.SetClip(Gdiplus::RectF(17, 0, 158, 114), Gdiplus::CombineModeIntersect);
                dc.DrawImage(portrait.get(), Gdiplus::RectF(-3, -174, 202, 288));
                dc.Restore(portrait_clip);
            }
            for (const float x : {462.0f, 500.0f})
                tile(0, 1341, 515, 1396, 570, {x, 125, 37, 37});
            struct CharacterHudArt {
                std::string_view fighter;
                std::array<int, 5> symbol, name; // atlas page, left, top, right, bottom
            };
            // Original InGame.gbs texture table: character symbols and name logos.
            static constexpr CharacterHudArt character_art[] = {
                {"annie", {1, 700, 1827, 776, 1903}, {1, 1070, 1923, 1232, 1998}},
                {"beowulf", {1, 701, 1907, 777, 1983}, {1, 991, 1845, 1153, 1920}},
                {"bigband", {1, 615, 1748, 691, 1824}, {1, 535, 1907, 697, 1982}},
                {"blackdahlia", {1, 1500, 1457, 1576, 1533}, {1, 1578, 1457, 1740, 1532}},
                {"bossmarie", {1, 1501, 1536, 1577, 1612}, {1, 1581, 1536, 1743, 1611}},
                {"cerebella", {0, 1477, 300, 1553, 376}, {0, 1341, 436, 1503, 511}},
                {"double", {1, 138, 1737, 214, 1813}, {1, 1523, 10, 1685, 85}},
                {"eliza", {1, 695, 1748, 771, 1824}, {1, 825, 1845, 987, 1920}},
                {"filia", {0, 1507, 436, 1583, 512}, {1, 1689, 10, 1851, 85}},
                {"fukua", {1, 535, 1748, 611, 1824}, {1, 535, 1828, 697, 1903}},
                {"marie", {1, 138, 1577, 214, 1653}, {1, 1855, 10, 2017, 85}},
                {"msfortune", {1, 311, 15, 387, 91}, {1, 222, 918, 384, 993}},
                {"painwheel", {1, 391, 15, 467, 91}, {1, 388, 918, 550, 993}},
                {"parasoul", {1, 471, 15, 547, 91}, {1, 554, 918, 716, 993}},
                {"peacock", {1, 551, 15, 627, 91}, {1, 206, 1043, 368, 1118}},
                {"robofortune", {1, 991, 1924, 1067, 2000}, {1, 825, 1924, 987, 1999}},
                {"squigly", {0, 697, 487, 773, 563}, {0, 789, 447, 951, 522}},
                {"umbrella", {1, 1156, 1844, 1232, 1920}, {1, 1257, 1819, 1419, 1894}},
                {"unknown", {1, 1443, 10, 1519, 86}, {1, 206, 1122, 368, 1197}},
                {"valentine", {1, 138, 1657, 214, 1733}, {1, 372, 1043, 534, 1118}},
            };
            const std::string_view active_fighter = roster[fighters[side][team.active_slot]];
            const auto artwork = std::find_if(std::begin(character_art), std::end(character_art),
                [&](const auto& entry) { return entry.fighter == active_fighter; });
            const auto& art = artwork != std::end(character_art) ? *artwork :
                *std::find_if(std::begin(character_art), std::end(character_art),
                    [](const auto& entry) { return entry.fighter == "unknown"; });
            tile(art.symbol[0], art.symbol[1], art.symbol[2], art.symbol[3], art.symbol[4], {30, 21, 51, 51});
            tile(0, 88, 1370, 362, 1415, {39, 128, 118, 20});
            dc.Restore(mirrored);
            constexpr int digits[][2] = {{1341, 300}, {538, 1043}, {674, 1043},
                {2, 1247}, {2, 1383}, {2, 1519}};
            const auto* digit = digits[stock];
            tile(stock ? 1u : 0u, digit[0], digit[1], digit[0] + 132, digit[1] + 132,
                {side ? 1078.0f : 158.0f, 101, 44, 44});
            const float name_x = side ? 1133.0f : 39.0f;
            tile(art.name[0], art.name[1], art.name[2], art.name[3], art.name[4], {name_x, 113, 108, 50});

            const float stats_x = side ? 722.0f : 199.0f;
            const auto attack_data = overlay.first;
            if (!attack_data) continue;
            Gdiplus::SolidBrush backing(Gdiplus::Color(148, 11, 12, 14));
            Gdiplus::SolidBrush line(Gdiplus::Color(210, 217, 208, 179));
            dc.FillRectangle(&backing, stats_x, 184.0f, 368.0f, 100.0f);
            dc.FillRectangle(&line, stats_x, 284.0f, 368.0f, 2.0f);
            const auto& [hp, last_hit, total_damage, maximum, combo_hits] = hud.attack_stats;
            const auto life = L"Life:     " + std::to_wstring(hp) + L" / 15500";
            const auto damage = L"Hit:    " + std::to_wstring(last_hit) + L"   / Dmg: " +
                std::to_wstring(total_damage) + L"   / Max: " + std::to_wstring(maximum);
            auto hits = L"Combo Hits:    " + std::to_wstring(combo_hits);
            if (attack_data == 2) hits += L"     Hitstun: " + std::to_wstring(hud.hitstun);
            if (!hud_stats_font)
                hud_stats_font = std::make_unique<Gdiplus::Font>(L"Arial", 18.0f, Gdiplus::FontStyleRegular,
                                                               Gdiplus::UnitPixel);
            const auto draw_stat = [&](const wchar_t* text, float y) {
                draw_text(dc, text, {stats_x + 8, y, 352, 25}, *hud_stats_font, Gdiplus::StringAlignmentNear);
            };
            draw_stat(life.c_str(), 186);
            draw_stat(L"Damage Multiplier:  1.00", 210);
            draw_stat(damage.c_str(), 234);
            draw_stat(hits.c_str(), 258);
        }
        tile(0, 88, 1096, 394, 1366, {538, -18, 204, 180});
        for (const float x : {597.0f, 625.0f}) {
            tile(0, 1859, 251, 1943, 357, {x, 33, 56, 71});
            tile(0, 1947, 251, 2031, 357, {x, 33, 56, 71});
        }
        dc.Restore(state);
    }

    std::wstring fighter_names_label(std::size_t side) const {
        const std::string_view name = roster[fighters[side][practice_teams[side].active_slot]];
        std::wstring label(name.begin(), name.end());
        for (auto& ch : label) if (ch >= L'a' && ch <= L'z') ch -= L'a' - L'A';
        return label;
    }

    void paint_practice_inputs(Gdiplus::Graphics& dc) const {
        const auto players = training_value("OPT_STR_INPUT_DISPLAY", 1);
        if (!players) return;
        const auto display = training_value("OPT_STR_INPUT_DISPLAY_TYPE");
        Gdiplus::SolidBrush backing(Gdiplus::Color(115, 15, 16, 19));
        dc.FillRectangle(&backing, 0, 610, 1280, 90);
        const auto state = dc.Save();
        dc.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen stem(Gdiplus::Color(255, 214, 154, 66), 4);
        Gdiplus::SolidBrush base(Gdiplus::Color(255, 49, 48, 47));
        Gdiplus::SolidBrush knob(Gdiplus::Color(255, 255, 252, 242));
        Gdiplus::Pen outline(Gdiplus::Color(255, 20, 19, 18), 2);
        Gdiplus::Pen pressed_outline(Gdiplus::Color(255, 255, 252, 242), 2);
        constexpr std::array<Gdiplus::ARGB, 6> button_colors{
            0xffdab84b, 0xffda853b, 0xffcf4d34, 0xff7fceda, 0xff738fca, 0xffb181bc};
        const auto draw_input = [&](unsigned input, float x, bool stick, bool buttons) {
            if (buttons) {
                for (unsigned button = 0; button < 6; ++button) {
                    const bool pressed = (input & (16u << button)) != 0;
                    Gdiplus::SolidBrush color{Gdiplus::Color(button_colors[button])};
                    const Gdiplus::RectF bounds(x - 26 + 22.0f * (button % 3), 633 + 22.0f * (button / 3), 18, 18);
                    dc.FillEllipse(&color, bounds);
                    if (!pressed) dc.FillEllipse(&backing, bounds);
                    dc.DrawEllipse(pressed ? &pressed_outline : &outline, bounds);
                }
            }
            if (stick) {
                const float end_x = x + 20.0f * (int(bool(input & 2)) - int(bool(input & 1)));
                const float end_y = 655.0f + 20.0f * (int(bool(input & 4)) - int(bool(input & 8)));
                dc.FillEllipse(&base, x - 11, 644.0f, 22.0f, 22.0f);
                dc.DrawLine(&stem, x, 655.0f, end_x, end_y);
                dc.FillEllipse(&knob, end_x - 8, end_y - 8, 16.0f, 16.0f);
                dc.DrawEllipse(&outline, end_x - 8, end_y - 8, 16.0f, 16.0f);
            }
        };
        for (unsigned side = 0; side < hud_inputs.size(); ++side) {
            if (players != 3 && players != side + 1) continue;
            const auto& history = hud_inputs[side];
            const bool split = players == 3;
            const float right = split && !side ? 578.0f : 1218.0f;
            if (display != 0) {
                const float live = split ? (side ? 682.0f : 42.0f) : 42.0f;
                draw_input(history.last, live, true, false);
                draw_input(history.last, live + 70, false, true);
            }
            if (display == 1) continue;
            const std::size_t slots = (split ? 6 : 12) - (display == 2 ? 2 : 0);
            for (std::size_t i = 0; i < std::min(history.count, slots); ++i) {
                const unsigned input = history.entries[i];
                const bool buttons = (input & 0x3f0) != 0;
                draw_input(input, right - float(i) * 100.0f, !buttons, buttons);
            }
        }
        dc.Restore(state);
    }

    void paint_practice_hitboxes(Gdiplus::Graphics& dc, float camera, float floor) const {
        const auto level = training_value("OPT_STR_HITBOXES");
        if (!level) return;
        const auto draw = [&](unsigned sprite, float x, float y, bool facing, unsigned group,
                              const auto& metadata) {
            const auto color = group == 0 ? Gdiplus::Color(232, 36, 32) :
                group == 1 ? Gdiplus::Color(46, 221, 86) : Gdiplus::Color(58, 153, 247);
            Gdiplus::Pen edge(color, 1.5f);
            Gdiplus::SolidBrush fill(Gdiplus::Color(54, color.GetR(), color.GetG(), color.GetB()));
            for (const auto& box : metadata[sprite - 1].box_groups[group]) {
                const auto world = skullgirls::foit_world_box(box, static_cast<int>(x), static_cast<int>(y), facing);
                const Gdiplus::RectF bounds(float(world[2]) - camera, floor - float(world[0]),
                    float(world[3] - world[2]), float(world[0] - world[1]));
                dc.FillRectangle(&fill, bounds);
                dc.DrawRectangle(&edge, bounds);
            }
        };
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            if (!practice_slot_playable(side, practice_teams[side].active_slot)) continue;
            const auto& fighter = practice_players[side];
            const auto& metadata = practice_metadata(fighter);
            const bool facing = !fighter.mirror(practice_facing_right(side));
            draw(fighter.sprite(), fighter.practice_x, fighter.jump_height, facing, 1, metadata);
            if (level == 2) draw(fighter.sprite(), fighter.practice_x, fighter.jump_height, facing, 2, metadata);
            if (fighter.normal_hit || (fighter.scripted() && fighter.scripted()->hit) ||
                fighter.throw_phase == PracticeFighter::ThrowPhase::start)
                draw(fighter.sprite(), fighter.practice_x, fighter.jump_height, facing, 0, metadata);
            for (const auto& spike : fighter.ringlet_spikes)
                if (spike.can_hit()) draw(spike.sprite(), spike.x, 0, spike.facing_right, 0, combat_metadata);
            if (const auto& projectile = fighter.tricobezoar; projectile && projectile->can_hit())
                draw(projectile->sprite(), projectile->x, projectile->y, projectile->facing_right, 0, combat_metadata);
        }
    }

    void paint_recovery_bars(Gdiplus::Graphics& dc, float camera, float floor) const {
        if (training_value("OPT_STR_HITSTUNBAR") != 1) return;
        for (const auto& fighter : practice_players) {
            const unsigned ticks = fighter.hurt_group >= 0 ? fighter.hurt_hold : fighter.blockstun;
            if (!ticks) continue;
            const auto countdown_color = [](unsigned remaining) {
                return remaining >= 10 ? Gdiplus::Color(34, 248, 30) :
                    remaining >= 5 ? Gdiplus::Color(255, 164, 22) : Gdiplus::Color(231, 30, 30);
            };
            const float x = fighter.practice_x - camera - 34;
            const float y = floor - fighter.jump_height + 10;
            const auto draw_bar = [&](unsigned remaining, float top, Gdiplus::Color color) {
                constexpr float segment = 15.0f;
                const auto count = std::min(remaining, 55u);
                Gdiplus::SolidBrush border(Gdiplus::Color(210, 26, 25, 26));
                Gdiplus::SolidBrush fill(color);
                dc.FillRectangle(&border, x - 1, top - 1, segment * count + 1, 11.0f);
                for (unsigned i = 0; i < count; ++i)
                    dc.FillRectangle(&fill, x + segment * i, top, segment - 1, 9.0f);
            };
            draw_bar(ticks, y, fighter.hit_stop ? Gdiplus::Color(40, 252, 255) : countdown_color(ticks));
            if (fighter.hit_stop) draw_bar(fighter.hit_stop, y + 12, countdown_color(fighter.hit_stop));
        }
    }

    void draw_combat_tile(Gdiplus::Graphics& dc, unsigned id, Gdiplus::RectF bounds,
                          float opacity=1) const {
        const auto found=combat_scene.tiles.find(id);
        if (found==combat_scene.tiles.end()) return;
        const auto& tile=found->second;
        const auto& atlas=hud_atlas[tile.page];
        if (!atlas.bitmap) return;
        const float left=tile.uv[0]*atlas.width, top=tile.uv[1]*atlas.height;
        const float width=(tile.uv[2]-tile.uv[0])*atlas.width;
        const float height=(tile.uv[3]-tile.uv[1])*atlas.height;
        const int x=std::max(0,int(std::floor(left))), y=std::max(0,int(std::floor(top)));
        const int right=std::min(atlas.width,int(std::ceil(left+width)));
        const int bottom=std::min(atlas.height,int(std::ceil(top+height)));
        if (right<=x || bottom<=y) return;
        const auto [cached,inserted]=combat_tiles.try_emplace(id);
        if (inserted) {
            // GDI+ filters the source image during transformed draws. Keep each
            // animated UI tile independent of the large shared atlas.
#ifdef _WIN32
            cached->second.reset(atlas.pixels()->Clone(x,y,right-x,bottom-y,PixelFormat32bppPARGB));
#else
            cached->second=std::make_unique<Gdiplus::Bitmap>(right-x,bottom-y,PixelFormat32bppPARGB);
            Gdiplus::Graphics copy(cached->second.get());
            copy.DrawImage(atlas.pixels(),Gdiplus::RectF(0,0,float(right-x),float(bottom-y)),
                float(x),float(y),float(right-x),float(bottom-y),Gdiplus::UnitPixel);
#endif
            if (cached->second && cached->second->GetLastStatus()!=Gdiplus::Ok) cached->second.reset();
        }
        auto* image=cached->second ? cached->second.get() : atlas.pixels();
        const float source_x=cached->second ? left-x : left;
        const float source_y=cached->second ? top-y : top;
#ifdef _WIN32
        if(sprite_renderer_active) {
            try {
                const float w=float(image->GetWidth()),h=float(image->GetHeight());
                sprite_renderer->queue(dc,*image,bounds,{1,1,1,std::clamp(opacity,0.0f,1.0f)},combat_gpu_tiles[id],
                    {source_x/w,source_y/h,(source_x+width)/w,(source_y+height)/h});
                return;
            } catch(const std::exception&) {fail_sprite_renderer(dc);}
        }
#endif

        if (opacity>=1) {
            dc.DrawImage(image,bounds,source_x,source_y,width,height,Gdiplus::UnitPixel);
            return;
        }
        Gdiplus::ColorMatrix matrix{};
        for (unsigned i=0;i<5;++i) matrix.m[i][i]=1;
        matrix.m[3][3]=std::clamp(opacity,0.0f,1.0f);
        Gdiplus::ImageAttributes attributes; attributes.SetColorMatrix(&matrix);
        dc.DrawImage(image,bounds,source_x,source_y,width,height,Gdiplus::UnitPixel,&attributes);
    }

    void prepare_combat_art() const {
        Gdiplus::Bitmap scratch(1,1,PixelFormat32bppPARGB);
        Gdiplus::Graphics dc(&scratch);
        const auto prime=[&](auto&& self,const auto& node)->void {
            if (node.children.empty()) draw_combat_tile(dc,node.texture,{0,0,1,1});
            else for (const auto& child:node.children) self(self,child);
        };
        for (const auto name:{"P1_Filmstrip","P2_Filmstrip","Vingette","P1_Combo","P2_Combo"})
            if (const auto* node=combat_scene.find(name)) prime(prime,*node);
    }

    void prepare_super_portrait(unsigned id) const {
        if (!super_portraits[id] || super_portrait_tints[id]) return;
        const auto& texture=*super_portraits[id];
        auto image=std::make_unique<Gdiplus::Bitmap>(texture.width,texture.height,PixelFormat32bppPARGB);
        if (image->GetLastStatus()!=Gdiplus::Ok) return;
        Gdiplus::Graphics raster(image.get());
        raster.Clear(Gdiplus::Color::Transparent);
        Gdiplus::ColorMatrix color{};
        color.m[0][0]=1; color.m[1][1]=0.76f; color.m[2][2]=0.47f;
        color.m[3][3]=color.m[4][4]=1;
        Gdiplus::ImageAttributes attributes; attributes.SetColorMatrix(&color);
        if (raster.DrawImage(texture.pixels(),Gdiplus::RectF(0,0,float(texture.width),float(texture.height)),
            0,0,float(texture.width),float(texture.height),Gdiplus::UnitPixel,&attributes)==Gdiplus::Ok)
            super_portrait_tints[id]=std::move(image);
    }

    void draw_combat_node(Gdiplus::Graphics& dc, const skullgirls::CombatScene::Node& node,
                          const skullgirls::CombatScene::Animation& animation, float tick,
                          std::size_t side, float opacity=1) const {
        if (node.name.find("OFF")!=std::string::npos || node.name.find("Wear_")!=std::string::npos) return;
        const auto transform=animation.sample(node,tick);
        opacity*=transform.opacity;
        if (opacity<=0) return;
        const auto saved=dc.Save();
        dc.TranslateTransform(transform.x,transform.y);
        dc.RotateTransform(transform.rotation);
        dc.TranslateTransform(-node.pivot_x,-node.pivot_y);
        if (!node.children.empty()) {
            if (node.transform.width && node.transform.height)
                dc.ScaleTransform(transform.width/node.transform.width,transform.height/node.transform.height);
            for (const auto& child:node.children) draw_combat_node(dc,child,animation,tick,side,opacity);
        } else if (node.name=="P1_Face01" || node.name=="P2_Face01") {
            const auto id=fighters[side][practice_teams[side].active_slot];
            if (super_portraits[id]) {
                prepare_super_portrait(id);
                const auto& texture=*super_portraits[id];
#ifdef _WIN32
                if(sprite_renderer_active && super_portrait_tints[id]) {
                    try {
                        sprite_renderer->queue(dc,*super_portrait_tints[id],{0,0,transform.width,transform.height},
                            {1,1,1,std::clamp(opacity,0.0f,1.0f)},super_gpu_portraits[id]);
                        dc.Restore(saved);return;
                    } catch(const std::exception&) {fail_sprite_renderer(dc);}
                }
#endif

                if (super_portrait_tints[id] && opacity>=1) {
                    dc.DrawImage(super_portrait_tints[id].get(),Gdiplus::RectF(0,0,transform.width,transform.height),
                        0,0,float(texture.width),float(texture.height),Gdiplus::UnitPixel);
                    dc.Restore(saved);
                    return;
                }
                Gdiplus::ColorMatrix color{};
                color.m[0][0]=1;
                color.m[1][1]=super_portrait_tints[id]?1:0.76f;
                color.m[2][2]=super_portrait_tints[id]?1:0.47f;
                color.m[3][3]=opacity; color.m[4][4]=1;
                Gdiplus::ImageAttributes attributes; attributes.SetColorMatrix(&color);
                dc.DrawImage(super_portrait_tints[id]?super_portrait_tints[id].get():texture.pixels(),Gdiplus::RectF(0,0,transform.width,transform.height),
                    0,0,float(texture.width),float(texture.height),Gdiplus::UnitPixel,&attributes);
            }
        } else draw_combat_tile(dc,node.texture,{0,0,transform.width,transform.height},opacity);
        dc.Restore(saved);
    }

    void paint_super_cutins(Gdiplus::Graphics& dc) const {
        const auto filtering=dc.Save();
        // Original D3D9 MAG/MIN sampler states are LINEAR (FUN_005624f0).
        dc.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
        for (std::size_t side=0;side<2;++side) {
            const auto& cutin=combat_presentation.cutins[side];
            if (!cutin.visible()) continue;
            const auto prefix=side ? "P2_" : "P1_";
            const auto found=combat_scene.animations.find(std::string(prefix)+(cutin.frozen?"Super_Anim_In":"Super_Anim_Out"));
            const auto* strip=combat_scene.find(std::string(prefix)+"Filmstrip");
            if (!strip || found==combat_scene.animations.end()) continue;
            const float tick=float(cutin.frozen?cutin.frame:cutin.exit_frame);
            draw_combat_node(dc,*strip,found->second,tick,side);
            if (const auto* vignette=combat_scene.find("Vingette")) draw_combat_node(dc,*vignette,found->second,tick,side);
        }
        dc.Restore(filtering);
    }

    void paint_combo_counters(Gdiplus::Graphics& dc) const {
        for (std::size_t side=0;side<2;++side) {
            const auto& combo=combat_presentation.combos[side];
            if (!combo.visible()) continue;
            const auto prefix=side ? "P2_" : "P1_";
            const auto* panel=combat_scene.find(std::string(prefix)+"Combo");
            const auto animation=combat_scene.animations.find(std::string(prefix)+(combo.running?"Combo_ShowAnim":"Combo_HideAnim"));
            if (!panel || animation==combat_scene.animations.end()) continue;
            const float tick=combo.running?4.0f:float(combo.age);
            draw_combat_node(dc,*panel,animation->second,tick,side);
            const auto transform=animation->second.sample(*panel,tick);
            const float center=transform.x+(side?129.5f:201.5f);
            const auto text=std::to_string(std::min(combo.hits,9999u));
            const float pop=1+combo.pop*0.015f;
            const float advance=48*pop, height=86*pop;
            float x=center-(advance*(text.size()-1)+66*pop)*0.5f;
            for (char digit:text) {
                const std::string name="Textures/HUD/combo_number_0"+std::string(1,digit)+"_color.png";
                const auto tile=combat_scene.texture_names.find(name);
                if (tile!=combat_scene.texture_names.end()) draw_combat_tile(dc,tile->second,{x,transform.y+48-height*0.1f,66*pop,height});
                x+=advance;
            }
            const auto label=combat_scene.texture_names.find("Textures/HUD/combo_meter_hits_normal01.png");
            if (label!=combat_scene.texture_names.end()) draw_combat_tile(dc,label->second,{center-38,transform.y+143,76,24});
            if (!combo.running && combo.age<59 && !combo_sayings.empty()) {
                auto saying=combo_sayings.upper_bound(combo.hits);
                if (saying!=combo_sayings.begin()) {
                    --saying;
                    draw_text(dc,saying->second.c_str(),{center-140,transform.y+201,280,36},23,Gdiplus::StringAlignmentCenter);
                }
            }
        }
    }

    // Draw the current stage and fighters, followed by HUD and Training overlays.
    struct PracticeViewport { float width, height, bottom, left, focus; };
    PracticeViewport practice_viewport() const {
        float view_width = 1280.0f + 320.0f * practice_camera_zoom;
        float view_height = 720.0f + 180.0f * practice_camera_zoom;
        float camera_bottom = float(practice_camera_bottom);
        float center = float(practice_camera_center), focus = 0;
        for (std::size_t side=0;side<2;++side) {
            const auto& cutin=combat_presentation.cutins[side];
            if (!cutin.visible()) continue;
            const float amount=cutin.frozen?std::min(cutin.frame/14.0f,1.0f):1-cutin.exit_frame/11.0f;
            if (amount<focus) continue;
            focus=amount;
            center=practice_camera_center+(practice_players[side].practice_x-practice_camera_center)*focus;
        }
        view_width += (921.6f-view_width)*focus;
        view_height += (518.4f-view_height)*focus;
        camera_bottom += (std::max(0.0f,practice_floor-view_height/9.0f)-camera_bottom)*focus;
        const float camera = std::clamp(center - view_width * 0.5f,
                                        0.0f, std::max(0.0f, practice_width - view_width));
        return {view_width, view_height, camera_bottom, camera, focus};
    }

    std::array<float, 2> practice_effect_bounds(const skullgirls::FoitEffectPlayback& effect) const {
        // Egrets enter and leave at the visible camera edges, rather than the stage limits.
        if (effect.root == &parasoul_assets &&
            std::any_of(parasoul_assets.children.begin() + 42, parasoul_assets.children.begin() + 46,
                [&](const auto& child) { return effect.table == &child; })) {
            const auto view = practice_viewport();
            return {view.left, view.left + view.width};
        }
        return {0.0f, practice_width};
    }

    void prepare_sprite_renderer() const {
#ifdef _WIN32
        if(!sprite_renderer && !sprite_renderer_failed) {
            try {sprite_renderer=std::make_unique<skullgirls::SpriteGpu>();}
            catch(const std::exception&) {sprite_renderer_failed=true;}
        }
#endif
    }

#ifdef _WIN32
    void fail_sprite_renderer(Gdiplus::Graphics& dc) const {
        sprite_renderer->fallback(dc);sprite_renderer.reset();sprite_renderer_active=false;sprite_renderer_failed=true;
    }
    void begin_sprite_layer(Gdiplus::Graphics& dc) const {
        prepare_sprite_renderer();
        if(sprite_renderer) {sprite_renderer->flush_overlay(dc);sprite_renderer->begin();sprite_renderer_active=true;}
    }
    void finish_sprite_layer(Gdiplus::Graphics& dc) const {
        if(sprite_renderer_active) {
            try {sprite_renderer->finish(dc);}
            catch(const std::exception&) {fail_sprite_renderer(dc);}
            sprite_renderer_active=false;
        }
    }
#endif

    void paint_practice(Gdiplus::Graphics& dc) const {
        const auto view = practice_viewport();
        const float view_width=view.width, view_height=view.height, camera_bottom=view.bottom;
        const float camera=view.left, focus=view.focus;
        if (practice_scene)
            practice_scene->draw(dc, camera + view_width * 0.5f, view_width, view_height,
                                 camera_bottom, practice_stage_tick
#ifdef _WIN32
                                 ,sprite_renderer && sprite_renderer->presenting()?sprite_renderer.get():nullptr
#endif
                                 );
        const auto world = dc.Save();
        dc.ScaleTransform(1280.0f / view_width, 720.0f / view_height);
        if (practice_background)
            practice_background->draw(dc, -camera, view_height - practice_height + camera_bottom,
                                      practice_width, practice_height);
        const float shade_amount=focus>0?0.25f*super_flash_amount:super_flash_amount;
        const unsigned dark=BYTE(190*shade_amount), light=BYTE(82*focus);
        if (dark || light) {
            // The black dimmer followed by the yellow flash is one equivalent
            // source-over layer; avoid blending the entire viewport twice.
            const float alpha=light+dark*(1-light/255.0f);
            const auto channel=[&](unsigned color){return BYTE(std::lround(color*light/alpha));};
            Gdiplus::SolidBrush shade(Gdiplus::Color(BYTE(std::lround(alpha)),channel(255),channel(231),channel(70)));
            dc.FillRectangle(&shade,0.0f,0.0f,view_width,view_height);
        }
        const float floor = view_height - practice_floor + camera_bottom;
        if (training_grid_enabled()) {
            // The original debug overlay uses a 200-world-unit grid, starting at BottomClearance.
            constexpr int spacing = 200;
            Gdiplus::Pen pen(Gdiplus::Color(0x66, 255, 255, 255));
            for (int x = static_cast<int>(camera) / spacing * spacing; x < camera + view_width; x += spacing)
                dc.DrawLine(&pen, x - camera, 0.0f, x - camera, floor);
            for (float y = floor; y >= 0; y -= spacing)
                dc.DrawLine(&pen, 0.0f, y, view_width, y);
        }
        const auto draw_practice_idle = [&](int side, float x, bool mirror, bool shadow) {
            const auto slot = practice_teams[side].active_slot;
            const auto fighter = fighters[side][slot];
            const auto palette = locked_colors[side][slot];
            if (std::string_view(roster[fighter]) != "filia") {
                draw_idle(dc, fighter, palette, x, floor, mirror, shadow);
                return;
            }
            draw_practice_frame(dc, side, practice_players[side].sprite(), x, floor, mirror, shadow);
        };
        const auto draw_players = [&](bool shadow) {
            for (int side : {1, 0}) {
                const auto draw_travel = [&](const PracticeTeamActor& actor) {
                    const auto& travel = actor.travel;
                    if (!travel.pose) return;
                    const auto sprite = practice_slot_is_squigly(side, actor.slot) ? squigly_team_pose(travel.pose) : practice_slot_is_fukua(side, actor.slot) ? fukua_team_pose(travel.pose) : practice_slot_is_valentine(side, actor.slot) ? valentine_team_pose(travel.pose) : practice_slot_is_robofortune(side, actor.slot) ? robofortune_team_pose(travel.pose) : practice_slot_is_painwheel(side, actor.slot) ? painwheel_team_pose(travel.pose) : practice_slot_is_msfortune(side, actor.slot) ? msfortune_team_pose(travel.pose) : practice_slot_is_parasoul(side, actor.slot) ? parasoul_team_pose(travel.pose) : practice_slot_is_peacock(side, actor.slot) ? peacock_team_pose(travel.pose) :
                        practice_slot_is_cerebella(side, actor.slot) ?
                        cerebella_team_pose(travel.pose) : travel.pose;
                    draw_practice_frame(dc, side, sprite, travel.x - camera,
                        shadow ? floor : floor - travel.height, !travel.facing_right, shadow, actor.slot);
                };
                const auto& team = practice_teams[side];
                if (team.departing) draw_travel(*team.departing);
                if (team.tag) {
                    draw_travel(team.tag->incoming);
                    if (team.tag->forced_exit)
                        draw_travel({team.active_slot, team.tag->incoming.home_on_left, *team.tag->forced_exit});
                }
                if (const auto& assist = team.assist) {
                    if (assist->phase == PracticeAssist::Phase::acting) {
                        const auto& fighter = assist->fighter;
                        draw_practice_frame(dc, side, fighter.sprite(), fighter.practice_x - camera,
                            shadow ? floor : floor - fighter.jump_height,
                            fighter.mirror(practice_facing_right(side)), shadow, assist->actor.slot);
                    } else {
                        draw_travel(assist->actor);
                    }
                }
                if (practice_point_snapped(side)) continue;
                const auto& player = practice_players[side];
                const float x = player.practice_x - camera;
                const auto& actor = practice_presentation.actors[side];
                if (actor.intro) {
                    const auto& playback = *actor.intro;
                    if (actor.pose <= 1 || playback.value(0xa4) == 0 || playback.value(0xa5) == 0) continue;
                    const auto saved = dc.Save();
                    dc.TranslateTransform(playback.x - camera, shadow ? floor : floor - playback.y);
                    dc.RotateTransform(playback.value(0xa3));
                    dc.ScaleTransform(playback.value(0xa4), playback.value(0xa5));
                    draw_practice_frame(dc, side, actor.pose, 0, 0, !actor.facing_right, shadow, {}, playback.value(0xa6));
                    dc.Restore(saved);
                    continue;
                }
                if (actor.pose) {
                    const bool turning = practice_presentation.phase == PracticePresentation::Phase::results &&
                        actor.pose >= 50 && actor.pose <= 57;
                    draw_practice_frame(dc, side, actor.pose, x, floor,
                        turning ? !actor.starts_facing_right : !actor.facing_right, shadow);
                    if (!shadow && actor.effect_pose)
                        draw_practice_frame(dc, side, actor.effect_pose, x, floor, !actor.facing_right, false);
                    continue;
                }
                const bool mirror = player.mirror(practice_facing_right(side));
                if (player.scripted() || player.movement_sprite) {
                    draw_practice_frame(dc, side, player.sprite(), x,
                        shadow ? floor : floor - player.jump_height, mirror, shadow);
                } else {
                    draw_practice_idle(side, x, mirror, shadow);
                }
            }
        };
#ifdef _WIN32
        begin_sprite_layer(dc);
#endif
        paint_practice_effects(dc, camera, floor, true);
        draw_players(true);
        for (std::size_t side=0;side<2;++side) for (const auto& ghost:combat_presentation.ghosts[side]) {
            const auto tint=std::array<float,3>{0.65f,1.0f,0.26f};
            draw_practice_frame(dc,side,ghost.sprite,ghost.x-camera,floor-ghost.y,ghost.mirror,false,
                ghost.slot,0.4f*(1-ghost.age/18.0f),tint);
        }
        draw_players(false);
        paint_practice_effects(dc, camera, floor, false);
        for (int side : {1, 0}) {
            if (const auto& assist = practice_teams[side].assist) {
                for (const auto& spike : assist->fighter.ringlet_spikes) {
                    if (spike.sprite()) draw_practice_frame(dc, side, spike.sprite(), spike.x - camera,
                        floor, !spike.facing_right, false, spike.palette_slot);
                }
            }
            if (const auto& projectile = practice_players[side].tricobezoar)
                draw_practice_frame(dc, side, projectile->sprite(), projectile->x - camera,
                    floor - projectile->y, !projectile->facing_right, false, projectile->palette_slot);
            for (const auto& spike : practice_players[side].ringlet_spikes) {
                if (spike.sprite())
                    draw_practice_frame(dc, side, spike.sprite(), spike.x - camera,
                        floor, !spike.facing_right, false, spike.palette_slot);
            }
        }
        for (const auto& stats : hud_stats) {
            for (const auto& spark : stats.sparks) {
#ifdef _WIN32
                if(sprite_renderer_active) {
                    try {
                        combat_effects[static_cast<std::size_t>(spark.effect)].draw_at(dc,spark.age,
                            spark.position.X-camera,floor-spark.position.Y,spark.scale,sprite_renderer.get());
                        continue;
                    } catch(const std::exception&) {fail_sprite_renderer(dc);}
                }
#endif
                combat_effects[static_cast<std::size_t>(spark.effect)].draw_at(dc, spark.age,
                    spark.position.X - camera, floor - spark.position.Y, spark.scale);
            }
        }
#ifdef _WIN32
        finish_sprite_layer(dc);
#endif
        if (practice_presentation.phase == PracticePresentation::Phase::none) {
            paint_practice_hitboxes(dc, camera, floor);
            paint_recovery_bars(dc, camera, floor);
        }
        dc.Restore(world);
#ifdef _WIN32
        begin_sprite_layer(dc);
#endif
        paint_super_cutins(dc);
#ifdef _WIN32
        finish_sprite_layer(dc);
#endif
        paint_practice_hud(dc);
        paint_combo_counters(dc);
        for (std::size_t side = 0; side < hud_stats.size(); ++side) {
            const auto& stats = hud_stats[side];
            if (practice_players[side].burst_available && practice_players[side].hurt_group >= 0)
                draw_text(dc, L"Burst ready", {side * 640.0f, 239, 640, 36}, 24, Gdiplus::StringAlignmentCenter);
            else if (stats.notice_ticks && stats.notice)
                draw_text(dc, stats.notice, {side * 640.0f, 239, 640, 36}, 24, Gdiplus::StringAlignmentCenter);
        }
        if (dummy_input_override()) {
            const auto slot = dummy_recording ? *dummy_recording :
                dummy_playing ? *dummy_playing : training_value("OPT_STR_PLAYBACKSLOT");
            const auto status = std::wstring(dummy_armed ? L"Ready to record slot " :
                dummy_recording ? L"Recording slot " : L"Playing slot ") +
                std::to_wstring(slot + 1);
            draw_text(dc, status.c_str(), {320, 195, 640, 32}, 20, Gdiplus::StringAlignmentCenter);
        }
        using Phase = PracticePresentation::Phase;
        if (practice_presentation.phase == Phase::knockout) {
            draw_text(dc, L"K.O.", {320, 304, 640, 75}, 42, Gdiplus::StringAlignmentCenter);
        } else if (practice_presentation.phase == Phase::results) {
            const auto& defeated = practice_presentation.defeated;
            const wchar_t* result = defeated[0] && defeated[1] ? L"Draw" :
                defeated[0] ? L"Player 2 wins" : L"Player 1 wins";
            draw_text(dc, result, {320, 159, 640, 60}, 42, Gdiplus::StringAlignmentCenter);
            draw_text(dc, L"R to reset", {320, 219, 640, 35}, 22, Gdiplus::StringAlignmentCenter);
        }
        if (practice_paused) paint_practice_pause(dc);
    }

    void load_command_lists(skullgirls::SpriteArchive& core, skullgirls::SpriteArchive& ui) {
        const auto read = [](skullgirls::SpriteArchive& archive, const std::string& path) {
            const auto bytes = archive.read(path);
            return std::istringstream(std::string(bytes.begin(), bytes.end()));
        };
        const auto fold = [](std::string key) {
            for (auto& ch : key) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
            return key;
        };
        std::map<std::string, std::wstring> labels;
        auto localization = read(core, "core/localization/en_loc.json");
        // The installed localization stores one string entry per line.
        for (std::string line; std::getline(localization, line);) {
            std::istringstream entry(line);
            std::string key, value;
            char separator;
            entry >> std::ws;
            if (entry.peek() != '"' || !(entry >> std::quoted(key) >> separator >> std::ws) ||
                separator != ':' || entry.peek() != '"') continue;
            entry.get();
            for (char ch; entry.get(ch) && ch != '"';) {
                if (ch == '\\' && entry.get(ch)) {
                    if (ch == 'r' || ch == 'n' || ch == 't') ch = ' ';
                }
                value += ch;
            }
            const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
            std::wstring wide(length, L'\0');
            if (length) MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), length);
            labels.emplace(fold(key), std::move(wide));
        }
        const auto label = [&](const std::string& key) {
            const auto match = labels.find(fold(key));
            return match != labels.end() ? match->second : std::wstring(key.begin(), key.end());
        };
        auto sayings = read(ui, "ui/config/InGame/comboSayings.ini");
        for (std::string line;std::getline(sayings,line);) {
            std::istringstream entry(line);
            unsigned count; std::string key;
            if (entry >> count >> key) combo_sayings.emplace(count,label(key));
        }
        const auto clean = [](std::string line) {
            if (const auto comment = line.find('#'); comment != std::string::npos) line.erase(comment);
            line.erase(line.find_last_not_of(" \t\r") + 1);
            return line;
        };
        const auto input_text = [](std::wstring source) {
            static const std::map<std::wstring, std::wstring> glyphs{
                {L"qcf", L"↓ ↘ →"}, {L"qcb", L"↓ ↙ ←"}, {L"dp", L"→ ↓ ↘"}, {L"rdp", L"← ↓ ↙"},
                {L"f", L"→"}, {L"b", L"←"}, {L"d", L"↓"}, {L"df", L"↘"}, {L"u", L"↑"},
                {L"ub", L"↖"}, {L"uf", L"↗"}, {L"360", L"360°"}, {L"seq", L", "}, {L"seqs", L", "},
                {L"ap", L"Punch"}, {L"ak", L"Kick"}, {L"pp", L"2 Punches"}, {L"kk", L"2 Kicks"},
                {L"plus", L" + "},
            };
            std::wstring result;
            for (std::size_t i = 0; i < source.size();) {
                const auto end = source[i] == L'{' ? source.find(L'}', i + 1) : std::wstring::npos;
                if (end == std::wstring::npos) {
                    result += source[i++];
                    continue;
                }
                const auto token = source.substr(i + 1, end - i - 1);
                const auto glyph = glyphs.find(token);
                if (glyph != glyphs.end()) result += glyph->second;
                else {
                    for (std::size_t j = 0; j < token.size(); ++j) {
                        if (j && j % 2 == 0) result += L" + ";
                        result += static_cast<wchar_t>(std::towupper(token[j]));
                    }
                }
                i = end + 1;
            }
            return result;
        };
        auto index = read(ui, "ui/config/Options/movelists.ini");
        for (std::string line; std::getline(index, line);) {
            line = clean(std::move(line));
            if (line.rfind("Text: ", 0) != 0) continue;
            const auto key = line.substr(6);
            CommandList list{fold(key), label(key), {}};
            auto config = read(ui, "ui/config/Options/movelist_" + key + ".ini");
            for (std::string field, name; std::getline(config, field);) {
                field = clean(std::move(field));
                if (field.rfind("Text: ", 0) == 0) name = field.substr(6);
                if (field.rfind("Description: ", 0) == 0 && !name.empty()) {
                    list.moves.push_back({label(name), input_text(label(field.substr(13)))});
                    name.clear();
                }
            }
            command_lists.push_back(std::move(list));
        }
    }

    bool command_input(WPARAM key, bool accept, bool back) {
        if (back) {
            if (command_page == CommandPage::moves)
                command_page = command_menu_cursor == 2 ? CommandPage::characters : CommandPage::menu;
            else if (command_page == CommandPage::characters) command_page = CommandPage::menu;
            else pause_page.reset();
            return true;
        }
        if (command_lists.empty()) return false;
        auto& cursor = command_page == CommandPage::menu ? command_menu_cursor :
            command_page == CommandPage::characters ? command_character_cursor : command_move_cursor;
        const auto count = command_page == CommandPage::menu ? 3u :
            command_page == CommandPage::characters ? command_lists.size() : command_lists[command_list].moves.size();
        if (count && (key == VK_UP || key == VK_DOWN)) {
            cursor = (cursor + (key == VK_UP ? count - 1 : 1)) % count;
            return true;
        }
        if (!accept || command_page == CommandPage::moves) return false;
        if (command_page == CommandPage::menu && cursor == 2) {
            command_page = CommandPage::characters;
            return true;
        }
        const std::string_view wanted = command_page == CommandPage::characters ? std::string_view(command_lists[cursor].key) :
            cursor == 1 ? "universal" : roster[fighters[0].front()];
        const auto list = std::find_if(command_lists.begin(), command_lists.end(),
            [&](const CommandList& candidate) { return candidate.key == wanted; });
        if (list == command_lists.end()) return false;
        command_list = list - command_lists.begin();
        command_move_cursor = 0;
        command_page = CommandPage::moves;
        return true;
    }

    void paint_command_list(Gdiplus::Graphics& dc) const {
        constexpr const wchar_t* choices[] = {L"Current Character", L"Universal Commands", L"All Characters..."};
        const bool moves = command_page == CommandPage::moves;
        const bool characters = command_page == CommandPage::characters;
        const auto& list = command_lists[command_list];
        const auto* title = moves ? list.title.c_str() : characters ? L"All Characters" : L"Command Lists";
        draw_text(dc, title, {393, 213, 494, 42}, 30, Gdiplus::StringAlignmentCenter);
        const auto count = moves ? list.moves.size() : characters ? command_lists.size() : std::size(choices);
        const auto cursor = moves ? command_move_cursor : characters ? command_character_cursor : command_menu_cursor;
        const std::size_t visible = moves ? 5 : 7;
        const auto first = cursor >= visible ? cursor - visible + 1 : 0;
        const float height = moves ? 54.0f : 39.0f;
        Gdiplus::Pen outline(Gdiplus::Color(255, 164, 20, 18), 2);
        for (auto i = first; i < std::min(count, first + visible); ++i) {
            const float y = 258.0f + height * float(i - first);
            if (i == cursor) dc.DrawRectangle(&outline, 388.0f, y, 504.0f, height - 3);
            if (moves) {
                draw_text(dc, list.moves[i].name.c_str(), {396, y + 1, 488, 24}, 20, Gdiplus::StringAlignmentNear);
                draw_text(dc, list.moves[i].input.c_str(), {396, y + 23, 488, 29}, 15, Gdiplus::StringAlignmentNear);
            } else {
                const auto* label = characters ? command_lists[i].title.c_str() : choices[i];
                draw_text(dc, label, {396, y + 2, 488, 34}, 24, Gdiplus::StringAlignmentNear);
            }
        }
        const auto* hint = moves ? L"↑ ↓  Scroll     Esc  Back" : L"↑ ↓  Select     Enter  Open     Esc  Back";
        draw_text(dc, hint, {390, 541, 500, 31}, 19, Gdiplus::StringAlignmentCenter);
    }

    void paint_practice_pause(Gdiplus::Graphics& dc) const {
        Gdiplus::SolidBrush dim(Gdiplus::Color(190, 0, 0, 0));
        dc.FillRectangle(&dim, 0, 0, 1280, 720);
        training_frame.draw(dc, 352, 184, 576, 404);
        player_header.draw(dc, 549, 171, 182, 30);
        draw_text(dc, L"★ PLAYER 1 ★", {549, 174, 182, 27}, 20, Gdiplus::StringAlignmentCenter);
        color_wing.draw(dc, 466, 200, 45, 38);
        const auto wing_state = dc.Save();
        dc.TranslateTransform(1284, 0);
        dc.ScaleTransform(-1, 1);
        color_wing.draw(dc, 466, 200, 45, 38);
        dc.Restore(wing_state);
        if (pause_page == PauseOption::commands && !command_lists.empty()) {
            paint_command_list(dc);
            return;
        }
        const auto* heading = pause_page ? pause_options[static_cast<std::size_t>(*pause_page)] : L"PLAYER 1 PAUSE";
        draw_text(dc, heading, {512, 213, 256, 42}, 30, Gdiplus::StringAlignmentCenter);
        if (!pause_page || *pause_page == PauseOption::options) {
            const auto draw_rows = [&](const auto& rows, std::size_t selected) {
                Gdiplus::Pen outline(Gdiplus::Color(255, 164, 20, 18), 2);
                for (std::size_t i = 0; i < rows.size(); ++i) {
                    const float y = 258.0f + 39.0f * float(i);
                    if (i == selected) dc.DrawRectangle(&outline, 388.0f, y, 504.0f, 36.0f);
                    draw_text(dc, rows[i], {394, y + 2, 492, 34}, 27, Gdiplus::StringAlignmentNear);
                }
            };
            if (!pause_page) {
                draw_rows(pause_options, pause_cursor);
                return;
            }
            constexpr std::size_t visible = 7;
            const auto count = pause_training_count();
            const auto first = pause_training_cursor >= visible ? pause_training_cursor - visible + 1 : 0;
            Gdiplus::Pen outline(Gdiplus::Color(255, 164, 20, 18), 2);
            for (std::size_t i = first; i < std::min(first + visible, count); ++i) {
                const float y = 258.0f + 39.0f * float(i - first);
                if (i == pause_training_cursor) dc.DrawRectangle(&outline, 388.0f, y, 504.0f, 36.0f);
                if (i < pause_training_options.size()) {
                    draw_text(dc, pause_training_options[i], {394, y + 2, 492, 34}, 24, Gdiplus::StringAlignmentNear);
                } else {
                    const auto& setting = training_options[i - pause_training_options.size() + 1];
                    draw_text(dc, setting.label.c_str(), {394, y + 2, 268, 34}, 23, Gdiplus::StringAlignmentNear);
                    const auto value = L"‹  " + setting.choices[setting.choice] + L"  ›";
                    draw_text(dc, value.c_str(), {665, y + 2, 221, 34}, 23, Gdiplus::StringAlignmentCenter);
                }
            }
            draw_text(dc, L"↑ ↓  Select     ← →  Change     Esc  Back",
                {390, 541, 500, 31}, 19, Gdiplus::StringAlignmentCenter);
            return;
        }
        const auto* details = *pause_page == PauseOption::help ?
            L"Player 1   Arrows, A S D, Z X C\nPlayer 2   I J K L, V B N, F G H\nR   Reset both fighters\nF6 / F7   Save / load state\nF8 / F9   Record / play dummy input" :
            L"Unavailable in this preview.";
        draw_text(dc, details, {394, 266, 492, 180}, 25, Gdiplus::StringAlignmentNear);
        draw_text(dc, L"Esc  Back", {394, 520, 492, 38}, 22, Gdiplus::StringAlignmentNear);
    }

    bool showing_stage_select() const {
        return training == TrainingView::stage_select || training == TrainingView::stage_confirmed;
    }

    void load_stages(skullgirls::SpriteArchive& core, skullgirls::SpriteArchive& ui_win) {
        stage_layout = skullgirls::read_stage_select_layout(ui_win.read("ui/Win/Charselect.gbs"));
        const auto localization = core.read_if_present("core/localization/en_loc.json");
        const auto config = core.read("core/stages.ini");
        std::istringstream stream(std::string(config.begin(), config.end()));
        for (std::string line; std::getline(stream, line);) {
            line.erase(std::min(line.size(), line.find('#')));
            std::istringstream row(line);
            std::string id, level, picture;
            if (!(row >> id >> level >> picture)) continue;
            std::string parent;
            bool beta_only = false, exclude_from_random = false;
            for (std::string option; row >> option;) {
                if (option == "BetaOnly") beta_only = true;
                if (option == "ExcludeFromRandom") exclude_from_random = true;
                if (option.rfind("IsAlt>", 0) == 0) parent = option.substr(6);
            }
            if (beta_only) continue;
            auto preview = std::make_unique<Texture>();
            preview->load(ui_win.read("ui/Win/Textures/charselect/stages/" + picture + ".dds"));
            auto label = id;
            if (!label.empty() && label.front() == '_') label.erase(0, 1);
            std::replace(label.begin(), label.end(), '_', ' ');
            auto display_name = option_label(label);
            if (localization) {
                const std::string_view json(reinterpret_cast<const char*>(localization->data()), localization->size());
                if (const auto localized = skullgirls::stage_localized_name(json, id)) {
                    const int count = MultiByteToWideChar(CP_UTF8, 0, localized->data(), int(localized->size()), nullptr, 0);
                    display_name.resize(count);
                    if (count) MultiByteToWideChar(CP_UTF8, 0, localized->data(), int(localized->size()), display_name.data(), count);
                }
            }
            if (!exclude_from_random) random_stages.push_back(stages.size());
            stages.push_back({std::move(display_name), std::move(id), std::move(parent), std::move(level), std::move(preview)});
        }
        for (std::size_t i = 0; i < stages.size(); ++i) {
            if (stages[i].parent.empty()) stage_groups.push_back({i});
        }
        if (stage_groups.empty()) throw std::runtime_error("Missing stage select entries");
        for (std::size_t i = 0; i < stages.size(); ++i) {
            if (stages[i].parent.empty()) continue;
            const auto group = std::find_if(stage_groups.begin(), stage_groups.end(), [&](const auto& versions) {
                const auto& id = stages[versions.front()].id;
                return CompareStringOrdinal(std::wstring(id.begin(), id.end()).c_str(), -1,
                    std::wstring(stages[i].parent.begin(), stages[i].parent.end()).c_str(), -1, TRUE) == CSTR_EQUAL;
            });
            if (group == stage_groups.end()) throw std::runtime_error("Unknown alternate stage parent: " + stages[i].parent);
            group->push_back(i);
        }
    }
    bool choosing_palette = false;
    struct NameplateState {
        int fighter = -1;
        bool exiting = false;
        std::chrono::steady_clock::time_point started{};
    };
    std::array<std::array<NameplateState, 3>, 2> nameplate_states{};

    void nameplate(int side, std::size_t slot, int fighter, bool exiting) {
        auto& state = nameplate_states[side][slot];
        if (state.fighter == fighter && state.exiting == exiting) return;
        state = {fighter, exiting, std::chrono::steady_clock::now()};
    }

    void draw_nameplates(Gdiplus::Graphics& dc) const {
        const auto now = std::chrono::steady_clock::now();
        for (std::size_t side = 0; side < nameplate_states.size(); ++side) {
            for (std::size_t slot = 0; slot < nameplate_states[side].size(); ++slot) {
                const auto& state = nameplate_states[side][slot];
                if (state.fighter < 0) continue;
                const auto& keys = pedestal_layout.nameplates[side][slot][state.exiting].keys;
                const auto& a = keys.front();
                const auto& b = keys.back();
                const auto tick = std::chrono::duration<float>(now - state.started).count() * 60;
                const auto t = std::clamp((tick - a.tick) / (b.tick - a.tick), 0.0f, 1.0f);
                const auto mix = [t](float from, float to) { return from + (to - from) * t; };
                const auto opacity = static_cast<BYTE>(std::lround(mix(a.alpha, b.alpha)));
                if (opacity) pedestal_names[state.fighter].draw(dc,
                    mix(a.bounds.x, b.bounds.x), mix(a.bounds.y, b.bounds.y),
                    mix(a.bounds.width, b.bounds.width), mix(a.bounds.height, b.bounds.height), opacity);
            }
        }
    }
    std::size_t palette_cursor = 0;
    struct Palette { unsigned number; skullgirls::SpriteBytes colors; };
    std::array<std::vector<Palette>, roster.size()> palettes;
    std::array<std::vector<skullgirls::SelectFrame>, roster.size()> idle_frames;
    std::array<std::vector<std::size_t>, 2> locked_colors;
    std::size_t available_palette(int fighter, std::size_t requested) const {
        // FUN_004506f0 advances through palette numbers, wrapping after the last one.
        const auto count = palettes[fighter].size();
        for (std::size_t offset = 0; offset < count; ++offset) {
            const auto candidate = (requested + offset) % count;
            bool taken = false;
            for (std::size_t side = 0; side < fighters.size() && !taken; ++side) {
                for (std::size_t slot = 0; slot < fighters[side].size(); ++slot) {
                    if (fighters[side][slot] == fighter && locked_colors[side][slot] == candidate) {
                        taken = true;
                        break;
                    }
                }
            }
            if (!taken) return candidate;
        }
        return requested;
    }
    std::chrono::steady_clock::time_point idle_start{};
    mutable std::map<std::tuple<int, std::size_t, std::size_t>, std::unique_ptr<Gdiplus::Bitmap>> idle_cache;

    void draw_idle(Gdiplus::Graphics& dc, int fighter, std::size_t palette,
                   float x, float y, bool mirror, bool shadow = false) const {
        const auto& frames = idle_frames[fighter];
        if (frames.empty()) return;
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - idle_start).count();
        std::uint64_t total = 0;
        for (const auto& frame : frames) total += frame.ticks;
        auto tick = static_cast<std::uint64_t>(elapsed * 60) % total;
        std::size_t index = 0;
        while (tick >= frames[index].ticks) tick -= frames[index++].ticks;
        draw_fighter_frame(dc, fighter, palette, frames[index], index, x, y, mirror, shadow);
    }

    template <typename ColorAt>
    std::unique_ptr<Gdiplus::Bitmap> make_fighter_bitmap(const skullgirls::SelectFrame& frame,
                                                        ColorAt color_at) const {
        auto bitmap = std::make_unique<Gdiplus::Bitmap>(frame.width, frame.height, PixelFormat32bppPARGB);
        Gdiplus::BitmapData data{};
        Gdiplus::Rect bounds(0, 0, frame.width, frame.height);
        if (bitmap->LockBits(&bounds, Gdiplus::ImageLockModeWrite, PixelFormat32bppPARGB, &data) != Gdiplus::Ok)
            return {};
        for (unsigned row = 0; row < frame.height; ++row) {
            auto* target = reinterpret_cast<std::uint32_t*>(
                static_cast<BYTE*>(data.Scan0) + static_cast<std::ptrdiff_t>(row) * data.Stride);
            for (unsigned col = 0; col < frame.width; ++col) {
                const auto pixel = row * frame.width + col;
                const auto color=color_at(pixel), alpha=color>>24;
                target[col] = alpha<<24;
                for(unsigned shift:{0u,8u,16u})
                    target[col] |= ((((color>>shift)&255)*alpha+127)/255)<<shift;
            }
        }
        bitmap->UnlockBits(&data);
        return bitmap;
    }

    std::unique_ptr<Gdiplus::Bitmap> make_fighter_bitmap(const skullgirls::SelectFrame& frame,
                                                        int fighter, std::size_t palette) const {
        const auto rgba = skullgirls::colorize_sprite_with_palette(frame.pixels, palettes[fighter][palette].colors);
        return make_fighter_bitmap(frame, [&](std::size_t pixel) {
            if (!frame.pixels[pixel * 3]) return 0u;
            const auto* color = rgba.data() + pixel * 4;
            return (unsigned(color[3]) << 24) | (unsigned(color[0]) << 16) |
                   (unsigned(color[1]) << 8) | color[2];
        });
    }

    decltype(fighters) staged_fighters;
    decltype(locked_colors) staged_colors;
    std::map<std::pair<int, std::size_t>, skullgirls::SpritePaletteLookup> practice_palette_lookups;
    struct CachedPracticeBitmap {
        std::shared_ptr<Gdiplus::Bitmap> bitmap;
        std::uint64_t used = 0;
        std::map<std::array<float,4>,std::unique_ptr<Gdiplus::Bitmap>> variants;
        std::size_t variant_bytes = 0;
#ifdef _WIN32
        skullgirls::SpriteGpu::Texture gpu;
#endif
    };
    mutable std::map<std::tuple<int, std::size_t, unsigned>, CachedPracticeBitmap> practice_palette_cache;
    mutable std::uint64_t practice_bitmap_use = 0;
    mutable std::size_t practice_variant_bytes = 0;

    std::unique_ptr<Gdiplus::Bitmap> make_practice_bitmap(const skullgirls::SelectFrame& frame,
                                                          const skullgirls::SelectFrame* overlay,
                                                          const skullgirls::SpritePaletteLookup& lookup) const {
        if (!frame.width || !frame.height) return {};
        const auto make_bitmap = [&](const skullgirls::SelectFrame& image) {
            return make_fighter_bitmap(image, [&](std::size_t pixel) {
                const auto* texel = image.pixels.data() + pixel * 3;
                return texel[1] || texel[2] ? lookup.argb(texel[2], texel[1]) : 0u;
            });
        };
        auto bitmap = make_bitmap(frame);
        if (!bitmap) return {};
        if (overlay) {
            auto effect = make_bitmap(*overlay);
            if (!effect) return {};
            Gdiplus::Graphics composite(bitmap.get());
            if (composite.DrawImage(effect.get(), 0, 0, frame.width, frame.height) != Gdiplus::Ok) return {};
        }
        return bitmap;
    }

    // Prepare lookups for the selected palettes. Sprite bitmaps are cached as they are drawn.
    void prepare_practice_bitmaps() {
        practice_palette_lookups.clear();
        practice_palette_cache.clear();
        practice_variant_bytes = 0;
        practice_bitmap_use = 0;
        for (std::size_t side = 0; side < fighters.size(); ++side)
        for (std::size_t slot = 0; slot < fighters[side].size(); ++slot) {
            if (!practice_slot_playable(side, slot)) continue;
            const auto fighter = fighters[side][slot];
            const auto palette = locked_colors[side][slot];
            practice_palette_lookups.try_emplace(std::pair{fighter, palette}, palettes[fighter][palette].colors);
        }
    }

    void draw_fighter_frame(Gdiplus::Graphics& dc, int fighter, std::size_t palette,
                            const skullgirls::SelectFrame& frame, std::size_t index,
                            float x, float y, bool mirror, bool shadow = false) const {
        if (!frame.width || !frame.height) return;
        const auto key = std::make_tuple(fighter, palette, index);
        auto found = idle_cache.find(key);
        if (found == idle_cache.end()) {
            if (idle_cache.size() >= 96) idle_cache.erase(idle_cache.begin());
            auto bitmap = make_fighter_bitmap(frame, fighter, palette);
            if (!bitmap) return;
            found = idle_cache.emplace(key, std::move(bitmap)).first;
        }
        draw_fighter_bitmap(dc, *found->second, frame, x, y, mirror, shadow);
    }

    void draw_practice_frame(Gdiplus::Graphics& dc, std::size_t side, unsigned sprite,
                             float x, float y, bool mirror, bool shadow,
                             std::optional<std::size_t> slot = {}, float opacity = 1.0f,
                             std::optional<std::array<float,3>> tint = {}, float beam_end=0.0f) const {
        const auto active = slot.value_or(practice_teams[side].active_slot);
        const auto fighter = fighters[side][active];
        const auto palette = locked_colors[side][active];
        const bool bella = practice_slot_is_cerebella(side, active);
        const bool peacock = practice_slot_is_peacock(side, active);
        const auto* owner=&practice_players[side];
        if(practice_teams[side].assist && practice_teams[side].assist->actor.slot==active)owner=&practice_teams[side].assist->fighter;
        const auto& intro = practice_presentation.actors[side].intro;
        const bool headless = owner->msfortune && (intro ? intro->value(0x8b) : owner->msfortune->value(0x8b)) != 0;
        const auto& frames = practice_slot_is_squigly(side, active) ? squigly_frames : practice_slot_is_fukua(side, active) ? fukua_frames : practice_slot_is_valentine(side, active) ? valentine_frames : practice_slot_is_robofortune(side, active) ? robofortune_frames : practice_slot_is_painwheel(side, active) ? painwheel_frames : practice_slot_is_msfortune(side, active) ? (headless?msfortune_headless_frames:msfortune_frames) : practice_slot_is_parasoul(side, active) ? parasoul_frames : peacock ? peacock_frames : bella ? cerebella_frames : movement_frames;
        if (sprite >= frames.size()) return;
        const auto& frame = frames[sprite];
        const auto& overlays = practice_slot_is_squigly(side, active) ? squigly_overlays : practice_slot_is_fukua(side, active) ? fukua_overlays : practice_slot_is_valentine(side, active) ? valentine_overlays : practice_slot_is_robofortune(side, active) ? robofortune_overlays : practice_slot_is_painwheel(side, active) ? painwheel_overlays : practice_slot_is_msfortune(side, active) ? (headless?msfortune_headless_overlays:msfortune_overlays) : practice_slot_is_parasoul(side, active) ? parasoul_overlays : peacock ? peacock_overlays : bella ? cerebella_overlays : jab_overlays;
        const auto lookup = practice_palette_lookups.find({fighter, palette});
        if (lookup == practice_palette_lookups.end()) return;
        const auto key = std::tuple{fighter, palette, sprite | (headless?0x80000000u:0u)};
        auto cached = practice_palette_cache.find(key);
        if (cached == practice_palette_cache.end()) {
            constexpr std::size_t cache_limit = 128;
            if (practice_palette_cache.size() >= cache_limit) {
                const auto oldest = std::min_element(practice_palette_cache.begin(), practice_palette_cache.end(),
                    [](const auto& a, const auto& b) { return a.second.used < b.second.used; });
                practice_variant_bytes-=oldest->second.variant_bytes;
                practice_palette_cache.erase(oldest);
            }
            const auto overlay = overlays.find(sprite);
            auto bitmap = make_practice_bitmap(frame, overlay == overlays.end() ? nullptr : &overlay->second,
                                               lookup->second);
            if (!bitmap) return;
            cached = practice_palette_cache.emplace(key, CachedPracticeBitmap{std::move(bitmap)}).first;

        }
        cached->second.used = ++practice_bitmap_use;
        draw_fighter_bitmap(dc, *cached->second.bitmap, frame, x, y, mirror, shadow, opacity, tint,&cached->second,beam_end);
    }

    void draw_fighter_bitmap(Gdiplus::Graphics& dc, Gdiplus::Bitmap& bitmap,
                             const skullgirls::SelectFrame& frame, float x, float y,
                             bool mirror, bool shadow, float opacity = 1.0f,
                             std::optional<std::array<float,3>> tint = {}, CachedPracticeBitmap* cache=nullptr,float beam_end=0.0f) const {
        const auto state = dc.Save();
        dc.TranslateTransform(x, y);
        if (mirror) dc.ScaleTransform(-1, 1);
        const Gdiplus::RectF bounds(-frame.origin_x, -frame.origin_y, float(frame.width), float(frame.height));
        if (shadow) dc.ScaleTransform(1.0f,-0.23f);
        // Continue a narrow strip of the straight beam body. The muzzle and
        // cannon retain their authored size instead of stretching with the ray.
        const float tail_start=bounds.X+bounds.Width*0.75f;
        const Gdiplus::RectF tail(tail_start,bounds.Y,std::max(0.0f,beam_end-tail_start),bounds.Height);
        if(beam_end>0 && tail.Width>0 && beam_end>bounds.X+bounds.Width) {
#ifdef _WIN32
            if(sprite_renderer_active) {
                skullgirls::SpriteGpu::Texture temporary;
                sprite_renderer->queue(dc,bitmap,tail,{1,1,1,std::clamp(opacity,0.0f,1.0f)},
                    cache?cache->gpu:temporary,{0.75f,0,0.76f,1},cache?cache->bitmap:std::shared_ptr<Gdiplus::Bitmap>{});
            } else
#endif
            {
                Gdiplus::ImageAttributes attributes;Gdiplus::ColorMatrix color{};
                for(unsigned i=0;i<5;++i)color.m[i][i]=1;color.m[3][3]=std::clamp(opacity,0.0f,1.0f);
                attributes.SetColorMatrix(&color);
                dc.DrawImage(&bitmap,tail,frame.width*0.75f,0,frame.width*0.01f,float(frame.height),Gdiplus::UnitPixel,&attributes);
            }
        }

#ifdef _WIN32
        if (!dc.IsVisible(bounds)) { dc.Restore(state);return; }
        if(sprite_renderer_active) {
            skullgirls::SpriteGpu::Texture temporary;
            const auto rgb=tint.value_or(std::array<float,3>{1,1,1});
            const std::array<float,4> color=shadow?std::array<float,4>{0,0,0,0.55f}:
                std::array<float,4>{rgb[0],rgb[1],rgb[2],std::clamp(opacity,0.0f,1.0f)};
            try {
                sprite_renderer->queue(dc,bitmap,bounds,color,cache?cache->gpu:temporary,{0,0,1,1},
                    cache?cache->bitmap:std::shared_ptr<Gdiplus::Bitmap>{});
                dc.Restore(state);return;
            } catch(const std::exception&) {
                sprite_renderer->fallback(dc);sprite_renderer.reset();sprite_renderer_active=false;sprite_renderer_failed=true;
            }
        }
#endif
        const auto variant=[&](const Gdiplus::ColorMatrix& color,const std::array<float,4>& key)->Gdiplus::Bitmap* {
            if (!cache) return nullptr;
            if (const auto found=cache->variants.find(key);found!=cache->variants.end()) return found->second.get();
            const std::size_t bytes=std::size_t(frame.width)*frame.height*4;
            constexpr std::size_t limit=64*1024*1024;
            if (bytes>limit) return nullptr;
            if (practice_variant_bytes+bytes>limit) {
                for(auto& entry:practice_palette_cache) { entry.second.variants.clear();entry.second.variant_bytes=0; }
                practice_variant_bytes=0;
            }
            auto image=std::make_unique<Gdiplus::Bitmap>(frame.width,frame.height,PixelFormat32bppPARGB);
            if (image->GetLastStatus()!=Gdiplus::Ok) return nullptr;
#ifdef _WIN32
            Gdiplus::BitmapData source{},destination{};
            Gdiplus::Rect region(0,0,frame.width,frame.height);
            if (bitmap.LockBits(&region,Gdiplus::ImageLockModeRead,PixelFormat32bppPARGB,&source)!=Gdiplus::Ok) return nullptr;
            if (image->LockBits(&region,Gdiplus::ImageLockModeWrite,PixelFormat32bppPARGB,&destination)!=Gdiplus::Ok) {
                bitmap.UnlockBits(&source);return nullptr;
            }
            std::array<unsigned,4> scale{};
            for(unsigned i=0;i<4;++i)
                scale[i]=unsigned(std::lround(std::clamp((i==3?1.0f:key[2-i])*key[3],0.0f,1.0f)*65536));
            for(unsigned y=0;y<frame.height;++y) {
                const auto* in=reinterpret_cast<const unsigned*>(static_cast<const BYTE*>(source.Scan0)+y*source.Stride);
                auto* out=reinterpret_cast<unsigned*>(static_cast<BYTE*>(destination.Scan0)+y*destination.Stride);
                for(unsigned x=0;x<frame.width;++x) {
                    const unsigned pixel=in[x];unsigned converted=0;
                    for(unsigned i=0;i<4;++i) {
                        const unsigned channel=(((pixel>>(i*8))&255)*scale[i]+32768)>>16;
                        converted|=channel<<(i*8);
                    }
                    out[x]=converted;
                }
            }
            image->UnlockBits(&destination);bitmap.UnlockBits(&source);
#else
            Gdiplus::Graphics raster(image.get());raster.Clear(Gdiplus::Color::Transparent);
            Gdiplus::ImageAttributes attributes;attributes.SetColorMatrix(&color);
            if (raster.DrawImage(&bitmap,Gdiplus::RectF(0,0,float(frame.width),float(frame.height)),
                0,0,float(frame.width),float(frame.height),Gdiplus::UnitPixel,&attributes)!=Gdiplus::Ok) return nullptr;
#endif
            auto* result=image.get();cache->variants.emplace(key,std::move(image));
            cache->variant_bytes+=bytes;practice_variant_bytes+=bytes;
            return result;
        };
        if (shadow) {
            // Project the current silhouette onto the floor and blend its alpha.
            Gdiplus::ColorMatrix silhouette{};
            silhouette.m[3][3] = 0.55f;
            silhouette.m[4][4] = 1.0f;
            if (auto* image=variant(silhouette,{0,0,0,0.55f})) {
                dc.DrawImage(image,bounds);dc.Restore(state);return;
            }
            Gdiplus::ImageAttributes attributes;
            attributes.SetColorMatrix(&silhouette);
            dc.DrawImage(&bitmap, bounds, 0, 0, float(frame.width),
                         float(frame.height), Gdiplus::UnitPixel, &attributes);
        } else if (opacity < 1 || tint) {
            Gdiplus::ColorMatrix color{};
            color.m[0][0] = color.m[1][1] = color.m[2][2] = color.m[4][4] = 1;
            if (tint) for (unsigned i=0;i<3;++i) color.m[i][i]=(*tint)[i];
            color.m[3][3] = std::clamp(opacity, 0.0f, 1.0f);
            if (auto* image=variant(color,{color.m[0][0],color.m[1][1],color.m[2][2],color.m[3][3]})) {
                dc.DrawImage(image,bounds);dc.Restore(state);return;
            }
            Gdiplus::ImageAttributes attributes;
            attributes.SetColorMatrix(&color);
            dc.DrawImage(&bitmap, bounds, 0, 0, float(frame.width), float(frame.height), Gdiplus::UnitPixel, &attributes);
        } else {
            dc.DrawImage(&bitmap, bounds);
        }
        dc.Restore(state);
    }

    void start_character_select() {
        if (audio) { audio->stop_combat(); audio->music("select_intro", "select_loop"); }
        for (auto& team : fighters) team.clear();
        for (auto& colors : locked_colors) colors.clear();
        nameplate_states = {};
        choosing_palette = false;
        player = 0;
        training = TrainingView::team_size;
    }

    // Apply navigation to the current setup screen and start loading after stage confirmation.
    void training_input(WPARAM key, bool accept, bool cancel) {
        if (audio && training != TrainingView::practice) {
            if (cancel) audio->ui("ui.nope.wav");
            else if (accept) audio->ui(training == TrainingView::stage_select ? "stageselect.whoosh.wav" : "submenu.select.wav");
            else if (key == VK_RIGHT || key == VK_LEFT || key == VK_UP || key == VK_DOWN)
                audio->ui(player ? "smallswitch.c.p2.wav" : "smallswitch.c.p1.wav");
        }
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
                if (player == 1 && !fighters[0].empty()) {
                    player = 0;
                    fighter_cursor = fighters[0].back();
                    fighters[0].pop_back();
                    locked_colors[0].pop_back();
                    palette_cursor = 0;
                    nameplate(0, fighters[0].size(), fighter_cursor, true);
                    training = TrainingView::fighters;
                } else {
                    training = TrainingView::options;
                }
            } else if (accept) {
                fighter_cursor = 9;
                nameplate(player, fighters[player].size(), fighter_cursor, false);
                idle_start = std::chrono::steady_clock::now();
                palette_cursor = 0;
                training = TrainingView::fighters;
            } else {
                team_size[player] = std::clamp(team_size[player] + horizontal, 1, 3);
            }
        } else if (training == TrainingView::fighters) {
            constexpr std::array<WPARAM, 6> attack_keys{'A', 'S', 'D', 'Z', 'X', 'C'};
            const auto attack = std::find(attack_keys.begin(), attack_keys.end(), key);
            const bool shortcut = attack != attack_keys.end();
            if (choosing_palette && key != VK_RETURN && !accept) {
                if (cancel) {
                    choosing_palette = false;
                    palette_cursor = 0;
                    nameplate(player, fighters[player].size(), fighter_cursor, true);
                }
                else if (horizontal) {
                    const auto count = palettes[fighter_cursor].size();
                    palette_cursor = horizontal < 0 ? (palette_cursor + count - 1) % count : (palette_cursor + 1) % count;
                }
            } else if (key == VK_ESCAPE) {
                auto& team = fighters[player];
                if (team.empty()) training = TrainingView::team_size;
                else {
                    fighter_cursor = team.back();
                    team.pop_back();
                    locked_colors[player].pop_back();
                    palette_cursor = 0;
                    nameplate(player, team.size(), fighter_cursor, true);
                }
            } else if (shortcut || key == VK_RETURN) {
                auto& team = fighters[player];
                if (fighter_cursor == 10) {
                    std::uniform_int_distribution<int> random_fighter(0, int(roster.size()) - 1);
                    static std::mt19937 generator(std::random_device{}());
                    do { fighter_cursor = random_fighter(generator); }
                    while (!*roster[fighter_cursor] || fighter_cursor == 10);
                }
                if (!choosing_palette) {
                    if (key == VK_RETURN) {
                        choosing_palette = true;
                        nameplate(player, team.size(), fighter_cursor, false);
                        return;
                    }
                    // The single-button masks map to palettes 1 through 6 in FUN_0044ea70.
                    const auto number = static_cast<unsigned>(attack - attack_keys.begin() + 1);
                    const auto& colors = palettes[fighter_cursor];
                    const auto clamped = std::min(number, colors.back().number);
                    const auto selected_color = std::lower_bound(colors.begin(), colors.end(), clamped,
                        [](const Palette& color, unsigned value) { return color.number < value; });
                    palette_cursor = static_cast<std::size_t>(selected_color - colors.begin());
                    nameplate(player, team.size(), fighter_cursor, false);
                }
                choosing_palette = false;
                const auto color = available_palette(fighter_cursor, palette_cursor);
                if (audio) audio->ui(std::string("charconfirm.") + roster[fighter_cursor] + (player ? ".p2.wav" : ".p1.wav"));
                team.push_back(fighter_cursor);
                locked_colors[player].push_back(color);
                palette_cursor = 0;
                if (int(team.size()) == team_size[player]) {
                    if (player == 0) {
                        player = 1;
                        training = TrainingView::team_size;
                    } else {
                        enter_stage_select();
                    }
                } else {
                    nameplate(player, team.size(), fighter_cursor, false);
                }
            } else if (horizontal || vertical) {
                int row = fighter_cursor / 3;
                int column = fighter_cursor % 3;
                do {
                    row = (row + vertical + 7) % 7;
                    column = (column + horizontal + 3) % 3;
                } while (!*roster[row * 3 + column]);
                fighter_cursor = row * 3 + column;
                const auto slot = fighters[player].size();
                if (fighter_cursor == 10) {
                    const auto previous = nameplate_states[player][slot].fighter;
                    if (previous >= 0) nameplate(player, slot, previous, true);
                } else {
                    nameplate(player, slot, fighter_cursor, false);
                }
                idle_start = std::chrono::steady_clock::now();
                palette_cursor = 0;
            }
        } else if (showing_stage_select() && !cancel) {
            if (training == TrainingView::stage_select) {
                if (horizontal || vertical) {
                    const auto previous = selected_stage_preview();
                    const bool changed = horizontal ? stage_selection.horizontal(horizontal, stage_groups)
                                                    : stage_selection.vertical(vertical, stage_groups);
                    if (changed) {
                        previous_stage_preview = previous;
                        stage_switched = std::chrono::steady_clock::now();
                    }
                } else if (accept || key == VK_RETURN) {
                    confirm_stage_selection();
                    if (practice_slot_playable(0, 0)) {
                        start_practice();
                    } else {
                        training = TrainingView::stage_confirmed;
                    }
                }
            }
        } else if (cancel) {
            if (training == TrainingView::stage_confirmed) {
                stage_selection.resolved.reset();
                training = TrainingView::stage_select;
                return;
            }
            fighter_cursor = fighters[1].back();
            palette_cursor = 0;
            fighters[1].pop_back();
            locked_colors[1].pop_back();
            nameplate(1, fighters[1].size(), fighter_cursor, true);
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
        if (audio) audio->ui("menu.select.intro.wav");
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
        if (audio) audio->ui("menu.select.wav");
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
        if (audio) audio->ui(open ? "submenu.select.wav" : "ui.nope.wav");
        const auto now = Clock::now();
        submenu_from = submenu_position(now);
        submenu_started = now;
        submenu = open;
    }
    bool exit_prompt = false;
    bool unavailable = false;

    // Load the front-end assets and configuration. Training loads its fighters and stage separately.
    void load(const std::filesystem::path& directory) {
        skullgirls::SpriteArchive core(directory / "core.gfs");
        skullgirls::SpriteArchive ui(directory / "ui.gfs");
        load_command_lists(core, ui);
        game_data = directory;
        skullgirls::SpriteArchive sprites_archive(directory / "sprites.gfs");
        loading_animation.load(sprites_archive, "loading_en", "bomb_en");
        constexpr const char* effect_assets[] = {
            "hitspark_omni_l", "hitspark_omni_m", "hitspark_omni_h", "block", "pushblock",
            "grab", "throwtech2", "hitspark_ground_impact_m", "hitspark_wall_impact_h", "auto_recovery_effect",
        };
        static_assert(std::size(effect_assets) == static_cast<std::size_t>(CombatEffect::burst));
        for (std::size_t i = 0; i < std::size(effect_assets); ++i)
            combat_effects[i].load(sprites_archive, effect_assets[i], effect_assets[i]);
        combat_effects[static_cast<std::size_t>(CombatEffect::burst)].load(
            sprites_archive, "infinite_burst_break_blue", "infinite_burst_break_fx_BLUE");
        skullgirls::SpriteArchive ui_win(directory / "ui-win.gfs");
        const auto scene = ui_win.read("ui/Win/Main.gbs");
        timings = skullgirls::read_menu_timings(scene);
        title = skullgirls::read_title_scene(scene);
        for (std::size_t i = 0; i < title_logo.size(); ++i)
            title_logo[i].load(ui_win.read(title.logo[i].texture));
        for (const auto& frame : title.shield) {
            auto texture = std::make_unique<Texture>();
            texture->load(ui_win.read(frame.sprite.texture));
            title_shield.push_back(std::move(texture));
        }
        title_font.load(ui_win.read(title.font_atlas));
        const auto load_texture = [&](Texture& texture, const std::string& name) {
            texture.load(ui_win.read("ui/Win/Textures/" + name + ".dds"));
        };
        combat_scene = skullgirls::read_combat_scene(ui_win.read("ui/Win/InGame.gbs"));
        load_texture(hud_atlas[0], "TA_1001_1");
        load_texture(hud_atlas[1], "TA_1001_2");
        hud_tiles.clear();
        combat_tiles.clear();
#ifdef _WIN32
        combat_gpu_tiles.clear();
#endif
        hud_overlay_state.reset();
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
        load_texture(stage_bar, "charselect/stageselect_frame01");
        load_texture(stage_arrow, "charselect/UI_option_arrow01");
        load_texture(stage_variant_arrow, "charselect/UI_option_arrow02");
        load_texture(stage_header, "charselect/SG_Team_Select_Size_Main_Header02");
        load_stages(core, ui_win);
        load_texture(select_film, "charselect/charselect_filmstrip_center01");
        load_texture(select_overlay, "charselect/charselect_filmstrip_center02");
        load_texture(select_side, "charselect/charselect_filmstrip_sub01");
        load_texture(select_pedestal, "charselect/charselect_pedestal_base01");
        load_texture(select_cursor, "charselect/charselect_selector_red01");
        load_texture(select_cursor_p2, "charselect/charselect_selector_blue01");
        load_texture(team_frame, "charselect/SG_Assist_Frame01");
        load_texture(team_film, "charselect/SG_Team_Select_Size_Metal_Film01");
        load_texture(fighter_frame, "charselect/charselect_data_frame01");
        load_texture(fighter_bracket, "charselect/charselect_data_frame_bracket01");
        load_texture(coin_base, "charselect/data_frame_icon_basecircle01");
        load_texture(coin_empty, "charselect/charselect_data_charcoin_nopick1");
        load_texture(coin_pending, "charselect/data_charcoin_unknown_glow01");
        for (int side = 0; side < 2; ++side)
            load_texture(player_bar_label[side], "charselect/charselect_data_frame_player0" + std::to_string(side + 1));
        for (int i = 0; i < 3; ++i)
            load_texture(team_coin[i], "charselect/charselect_data_frame_teamnumber_0" + std::to_string(i + 1));
        load_texture(color_frame, "charselect/SG_Team_Select_Size_Frame01");
        load_texture(color_backer, "charselect/SG_Team_Select_Size_Frame_Backer01");
        load_texture(color_wing, "charselect/SG_UI_Wing01");
        load_texture(color_arrow, "MainMenu/arrow01");
        load_texture(color_of, "charselect/UI_gold_circle01");
        load_texture(pedestal_divider, "charselect/charselect_portrait_div_bar01");
        constexpr std::array button_names{"LP", "MP", "HP", "LK", "MK", "HK"};
        for (std::size_t i = 0; i < button_names.size(); ++i) {
            load_texture(color_buttons[i], "Buttons/Arcade_Buttons_" + std::string(button_names[i]) + "_02");
            load_texture(color_buttons_lit[i], "Buttons/Arcade_Buttons_" + std::string(button_names[i]) + "_01");
        }
        load_texture(portrait_frame, "charselect/charselect_portrait_frame01");
        const auto charselect = ui_win.read("ui/Win/Charselect.gbs");
        pedestal_layout = skullgirls::read_pedestal_layout(charselect);
        for (int side = 0; side < 2; ++side)
            fighter_layout[side] = skullgirls::read_fighter_select_layout(charselect, side == 1);
        for (int i = 0; i < 3; ++i) {
            const auto base = "charselect/teamsize_0" + std::to_string(i + 1);
            load_texture(team_normal[i], base + "A");
            load_texture(team_glow[i], base + "B");
        }
        for (std::size_t i = 0; i < roster.size(); ++i) {
            if (*roster[i])
                load_texture(portraits[i], "charselect/charselect_select_icons_" + std::string(roster[i]) + "01");
            if (*roster[i] && std::string_view(roster[i]) != "random") {
                load_texture(fighter_portraits[i], "charselect/charselect_portraits_" + std::string(roster[i]) + "01");
                load_texture(fighter_names[i], "charselect/charselect_portrait_name_" + std::string(roster[i]) + "01");
                load_texture(pedestal_names[i], "charselect/charselect_pedestal_name_" + std::string(roster[i]) + "01");
                const std::string_view name = roster[i];

                load_texture(fighter_coin[i], "charselect/data_charcoin_" + std::string(name) + "_glow01");
                const auto prefix = "sprites/character_select_" + std::string(roster[i]);
                idle_frames[i] = skullgirls::read_select_idle(
                    sprites_archive.read(prefix + ".spr.msb"), sprites_archive.read(prefix + ".dds"));
                const auto palette_prefix = "sprites/" + std::string(roster[i]) + "_";
                for (const auto& entry : sprites_archive.entries()) {
                    const std::string_view name = entry.name;
                    if (name.substr(0, palette_prefix.size()) != palette_prefix ||
                        name.size() <= palette_prefix.size() + 5 || name.substr(name.size() - 5) != "p.dds")
                        continue;
                    const auto suffix = name.substr(palette_prefix.size(), name.size() - palette_prefix.size() - 5);
                    if (suffix.find_first_not_of("0123456789") != std::string_view::npos) continue;
                    palettes[i].push_back({static_cast<unsigned>(std::stoul(std::string(suffix))),
                        skullgirls::read_dds_palette(sprites_archive.read(entry.name))});
                }
                auto& colors = palettes[i];
                std::sort(colors.begin(), colors.end(), [](const auto& a, const auto& b) { return a.number < b.number; });
                if (colors.empty()) throw std::runtime_error("Missing fighter palettes");
            }
        }
        const auto training_bytes = ui.read("ui/config/Options/trainingopt.ini");
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
                if (line.rfind("Keyword: ", 0) == 0) option.keyword = line.substr(9);
                if (line.rfind("DefaultChoice: ", 0) == 0) {
                    option.choice = option.choices.size();
                    option.choices.push_back(option_label(line.substr(15)));
                }
                if (line.rfind("Choice: ", 0) == 0) option.choices.push_back(option_label(line.substr(8)));
            }
        }
        constexpr std::array supported_training_options{
            "ACT_PRACTICE", "OPT_STR_SLOWMOTION", "OPT_STR_ATTACK_DATA", "OPT_STR_HITBOXES",
            "OPT_STR_HITSTUNBAR", "OPT_STR_TRAININGGRID", "OPT_STR_INPUT_DISPLAY", "OPT_STR_INPUT_DISPLAY_TYPE",
            "OPT_STR_LIFEBAR_LEVEL", "OPT_STR_LIFEBAR_SETTING", "OPT_STR_METERLEVEL",
            "OPT_STR_TRAININGSHOWCROSSUPS", "OPT_STR_DEATHALLOWED", "OPT_STR_SELECTBUTTON",
            "OPT_STR_PLAYBACKMODE", "OPT_STR_PLAYBACKSLOT", "OPT_STR_PLAYBACKSIDECORRECTION",
            "OPT_STR_DUMMYACTION", "OPT_STR_TRAINING_CPU_DIFFICULTY", "OPT_STR_BLOCKING", "OPT_STR_BLOCKTYPE",
            "OPT_STR_PUSHBLOCKAFTERNHITS", "OPT_STR_PUSHBLOCKHOWOFTEN", "OPT_STR_THROWTECH",
            "OPT_STR_FORCECOUNTERHIT"};
        training_options.erase(std::remove_if(training_options.begin(), training_options.end(), [&](const auto& option) {
            return std::find(supported_training_options.begin(), supported_training_options.end(), option.keyword) ==
                supported_training_options.end();
        }), training_options.end());
        if (training_options.empty() || training_options.front().keyword != "ACT_PRACTICE")
            throw std::runtime_error("No Training Options in game config");
        training_options.push_back({L"Dummy Hit Reaction",
            {L"Default", L"Sliding Knockdown", L"Ceiling Bounce", L"Crumple", L"Stagger", L"Freeze"},
            0, "PRACTICE_HIT_REACTION"});
        training_options.push_back({L"Filia Intro", {L"Cycle", L"Hat", L"Trip", L"Bow", L"Off"},
            0, "PRACTICE_FILIA_INTRO"});
        training_options.push_back({L"Filia Win Pose", {L"Cycle", L"Hair Blown", L"Sassy", L"Filia Blown"},
            0, "PRACTICE_FILIA_WIN"});
        const std::vector<std::wstring> assist_choices{
            L"Updo (Light)", L"Updo (Medium)", L"Updo (Heavy)",
            L"Hairball (Light)", L"Hairball (Medium)", L"Hairball (Heavy)",
            L"Ringlet Spike (Light)", L"Ringlet Spike (Medium)", L"Ringlet Spike (Heavy)",
        };
        constexpr std::array<std::size_t, 3> assist_defaults{2, 4, 6};
        for (std::size_t i = 0; i < practice_assist_options.size(); ++i) {
            auto label = L"P" + std::to_wstring(i / 3 + 1) + L" Filia " + std::to_wstring(i % 3 + 1) + L" Assist";
            training_options.push_back({std::move(label), assist_choices, assist_defaults[i % 3], practice_assist_options[i]});
        }
        for (std::size_t i = 0; i < configs.size(); ++i) {
            if (!*configs[i]) continue;
            const auto bytes = ui.read("ui/config/MainMenu/" + std::string(configs[i]) + ".ini");
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
        if (showing_stage_select()) {
            city.draw(dc, 0, 0, 1280, 720);
            const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - stage_switched).count();
            const auto fade = std::clamp(elapsed * 60 / std::max(1u, stage_layout.fade_frames - 1), 0.0, 1.0);
            const auto& preview = stage_layout.preview;
            const auto* selected = selected_stage_preview();
            if (previous_stage_preview)
                previous_stage_preview->draw(dc, preview.x, preview.y, preview.width, preview.height,
                                             selected ? 255 : static_cast<BYTE>(255 * (1 - fade)));
            if (selected)
                selected->draw(dc, preview.x, preview.y, preview.width, preview.height, static_cast<BYTE>(255 * fade));
            frame.draw(dc, 0, 0, 1280, 720);
            spot.draw(dc, 0, 0, 1280, 720, 180);
        } else {
        select_side.draw(dc, 398.9f, 0, 492, 720);
        select_film.draw(dc, 500.5f, 0, 289, 720);
        for (std::size_t i = 0; i < roster.size(); ++i) {
            if (!*roster[i]) continue;
            portraits[i].draw(dc, 535.5f + (i % 3) * 71, 103.0f + (i / 3) * 71, 68, 70);
        }
        select_overlay.draw(dc, 500.5f, 0, 289, 720);
        }
        select_header.draw(dc, 407.6f, 35.2f, 465, 70);
        for (int side = 0; side < 2; ++side) {
            const auto left = side ? 839.0f : 71.0f;
            for (int slot : {2, 1, 0}) {
                const auto& base = pedestal_layout.bases[side][slot];
                select_pedestal.draw(dc, base.x, base.y, base.width, base.height);
            }
            if (showing_stage_select()) {
                for (std::size_t slot = 0; slot < fighters[side].size(); ++slot) {
                    const auto& anchor = pedestal_layout.anchors[side][slot];
                    draw_idle(dc, fighters[side][slot], locked_colors[side][slot], anchor.x, anchor.y, side == 1);
                }
                continue;
            }
            const auto divider_state = dc.Save();
            if (side) {
                dc.TranslateTransform(1280, 0);
                dc.ScaleTransform(-1, 1);
            }
            pedestal_divider.draw(dc, 0, 241, 522, 128);
            dc.Restore(divider_state);
            const bool choosing = training == TrainingView::fighters && side == player;
            const bool locked = !fighters[side].empty() && (side < player);
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
                const auto color = choosing ? palette_cursor : locked_colors[side].back();
                for (std::size_t slot = 0; slot < fighters[side].size(); ++slot) {
                    const auto& anchor = pedestal_layout.anchors[side][slot];
                    draw_idle(dc, fighters[side][slot], locked_colors[side][slot], anchor.x, anchor.y, side == 1);
                }
                if (choosing) {
                    const auto& anchor = pedestal_layout.anchors[side][fighters[side].size()];
                    draw_idle(dc, current, color, anchor.x, anchor.y, side == 1);
                }
                if (!(choosing && choosing_palette) && fighter_portraits[current].bitmap) {
                    draw(fighter_portraits[current], layout.portrait, side == 1);
                    draw(portrait_frame, layout.name, side == 1);
                    draw(fighter_names[current], layout.name, false);
                }
                draw(fighter_frame, layout.frame, side == 1);
                if (choosing && choosing_palette) {
                    paint_palette_panel(dc, side, current, color);
                }
                draw(fighter_bracket, layout.bracket, side == 1);
                const auto& label = layout.player_label;
                const auto& number = layout.player_number;
                const float left = std::min(label.x, number.x);
                const float right = std::max(label.x + label.width, number.x + number.width);
                player_bar_label[side].draw(dc, left, label.y, right - left, label.height);
                for (std::size_t i = 0; i < layout.coins.size(); ++i) {
                    const auto& rect = layout.coins[i];
                    draw(coin_base, rect, false);
                    if (i == 0)
                        draw(team_coin[team_size[side] - 1], rect, false);
                    else if (i <= fighters[side].size())
                        draw(fighter_coin[fighters[side][i - 1]], rect, false);
                    else if (choosing && choosing_palette && i == fighters[side].size() + 1)
                        draw(fighter_coin[current], rect, false);
                    else if (i <= static_cast<std::size_t>(team_size[side]))
                        draw(coin_pending, rect, false);
                    else
                        draw(coin_empty, rect, false);
                }
                continue;
            }
            team_frame.draw(dc, left + 10, 389, 370, 210);
            player_header.draw(dc, left + 104, 374, 182, 30);
            draw_text(dc, side ? L"★ PLAYER 2 ★" : L"★ PLAYER 1 ★",
                      {left + 104, 378, 182, 30}, 20, Gdiplus::StringAlignmentCenter);
            draw_text(dc, L"SELECT SIZE", {left + 10, 415, 370, 40}, 27, Gdiplus::StringAlignmentCenter);
            team_film.draw(dc, left - 4, 463, 394, 68);
            for (int i = 0; i < 3; ++i) {
                const auto& number = team_size[side] == i + 1 ? team_glow[i] : team_normal[i];
                number.draw(dc, left + 54 + i * 89, 448, 82, 82);
            }
            std::wstring picks;
            for (int pick : fighters[side]) {
                if (!picks.empty()) picks += L"  /  ";
                picks += option_label(roster[pick]);
            }
            if (picks.empty()) {
                constexpr std::array size_descriptions{L"One Super-Powerful Character",
                    L"Two High-Powered Characters", L"Three Balanced Characters"};
                draw_text(dc, size_descriptions[team_size[side] - 1],
                          {left + 10, 551, 370, 36}, 21, Gdiplus::StringAlignmentCenter);
            }
            if (!picks.empty()) draw_text(dc, picks.c_str(), {left + 10, 551, 370, 36}, 23, Gdiplus::StringAlignmentCenter);
        }
        draw_nameplates(dc);
        if (showing_stage_select()) {
            const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - stage_entered).count();
            const auto draw_texture = [&](const Texture& texture, const skullgirls::TitleRect& rect, bool mirrored = false) {
                const auto saved = dc.Save();
                if (mirrored) { dc.TranslateTransform(rect.x * 2 + rect.width, 0); dc.ScaleTransform(-1, 1); }
                texture.draw(dc, rect.x, rect.y, rect.width, rect.height);
                dc.Restore(saved);
            };
            draw_texture(stage_header, stage_layout.header);
            const auto& header = stage_layout.header_text;
            draw_text(dc, L"SELECT STAGE", {header.x, header.y, header.width, header.height},
                      16, Gdiplus::StringAlignmentCenter);
            const auto controls = dc.Save();
            dc.TranslateTransform(0, stage_layout.entrance_offset(elapsed));
            draw_texture(stage_bar, stage_layout.bar);
            draw_texture(stage_arrow, stage_layout.arrows[0]);
            draw_texture(stage_arrow, stage_layout.arrows[1], true);
            if (!stage_selection.random() && stage_groups[stage_selection.choice - 1].size() > 1) {
                for (unsigned i = 0; i < 2; ++i) {
                    const auto arrow = dc.Save();
                    const auto& rect = stage_layout.variant_arrows[i];
                    dc.TranslateTransform(rect.x, rect.y);
                    dc.RotateTransform(stage_layout.variant_rotation[i]);
                    stage_variant_arrow.draw(dc, 0, 0, rect.width, rect.height);
                    dc.Restore(arrow);
                }
            }
            const auto& label = stage_layout.name;
            draw_text(dc, stage_selection_name().c_str(), {label.x, label.y, label.width, label.height},
                      26, Gdiplus::StringAlignmentCenter);
            dc.Restore(controls);
            if (training == TrainingView::stage_confirmed)
                draw_text(dc, L"Stage selected", {420, 690, 440, 28}, 21, Gdiplus::StringAlignmentCenter);
        }
        if (training == TrainingView::fighters) {
            (player ? select_cursor_p2 : select_cursor).draw(dc,
                521.5f + (fighter_cursor % 3) * 71, 90.0f + (fighter_cursor / 3) * 71, 96, 96);
        }

    }

    // Render the active screen into the back buffer before the window scales it.
    void paint(HDC buffer) const {
        Gdiplus::Graphics dc(buffer);
        paint(dc);
    }

    void paint(Gdiplus::Bitmap& buffer) const {
        Gdiplus::Graphics dc(&buffer);
        dc.Clear(Gdiplus::Color(255,0,0,0));
        paint(dc);
    }

    void paint(Gdiplus::Graphics& dc) const {
        if (practice_load.valid()) {
            const auto elapsed = std::chrono::duration<double>(Clock::now() - loading_started).count();
            loading_animation.draw(dc, static_cast<std::uint64_t>(elapsed * 60));
            return;
        }
        dc.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
        dc.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        if (training == TrainingView::practice) {
            dc.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
            paint_practice(dc);
            return;
        }
        dc.DrawImage(background.get(), 0, 0, 1280, 720);
        if (training == TrainingView::team_size || training == TrainingView::fighters ||
            showing_stage_select()) {
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
#ifdef _WIN32
HDC canvas_dc = nullptr;
HBITMAP canvas_bitmap = nullptr;
HGDIOBJ canvas_previous = nullptr;
std::unique_ptr<Gdiplus::Bitmap> canvas_image;
BYTE* canvas_pixels=nullptr;
int canvas_stride=0;

bool create_canvas(HWND window) {
    const auto dc = GetDC(window);
    if (!dc) return false;
    canvas_dc = CreateCompatibleDC(dc);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 1280;
    info.bmiHeader.biHeight = -720;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    canvas_bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    ReleaseDC(window, dc);
    if (!canvas_dc || !canvas_bitmap) return false;
    const auto previous = SelectObject(canvas_dc, canvas_bitmap);
    if (!previous || previous == HGDI_ERROR) return false;
    canvas_previous = previous;
    DIBSECTION section{};
    if (!GetObjectW(canvas_bitmap,sizeof(section),&section)) return false;
    canvas_pixels=static_cast<BYTE*>(section.dsBm.bmBits);canvas_stride=section.dsBm.bmWidthBytes;
    canvas_image=std::make_unique<Gdiplus::Bitmap>(1280,720,1280*4,PixelFormat32bppPARGB,
        static_cast<BYTE*>(section.dsBm.bmBits));
    if (canvas_image->GetLastStatus()!=Gdiplus::Ok) { canvas_image.reset(); return false; }
    return true;
}

#endif

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto& menu = *menu_instance;
    switch (message) {
    case WM_LBUTTONDOWN:
        if (menu.practice_load.valid()) return 0;
        if (!menu.title_visible) break;
        menu.leave_title();
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_KILLFOCUS:
        menu.practice_keys_held = {};
        menu.practice_keys_previous = {};
        menu.practice_attack_edges = {};
        for (auto& player : menu.practice_players) {
            player.practice_left = player.practice_right = false;
            player.practice_down = player.practice_up = false;
            player.normal_input = -1;
            player.queued_followup = -1;
            player.normal_inputs = {};
            player.buffered_special.reset();
            player.throw_input = false;
            player.pressed_punches = player.pressed_kicks = 0;
            player.held_attack_buttons = 0;
            player.dash_input_history.fill(skullgirls::filia_neutral);
        }
        break;
    case WM_KEYUP:
        if (menu.training == Menu::TrainingView::practice) {
            menu.practice_key(wparam, false);
            return 0;
        }
        break;
    case WM_KEYDOWN: {
        if (menu.practice_load.valid()) return 0;
        if (menu.training == Menu::TrainingView::practice) {
            const bool repeated = (lparam & (1L << 30)) != 0;
            if (menu.practice_input(wparam, repeated, GetKeyState(VK_SHIFT) < 0)) {
                const wchar_t* caption = L"Skullgirls Main Menu Reconstruction";
                if (menu.training == Menu::TrainingView::stage_select)
                    caption = L"Skullgirls - Select Stage";
                else if (menu.training == Menu::TrainingView::practice)
                    caption = menu.practice_paused ? L"Skullgirls - Paused" : L"Skullgirls - Training Room";
                SetWindowTextW(window, caption);
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }
        const bool repeated = (lparam & (1L << 30)) != 0;
        if (repeated && wparam != VK_UP && wparam != VK_DOWN &&
            wparam != VK_LEFT && wparam != VK_RIGHT) return 0;
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
            try {
                menu.training_input(wparam, accept, cancel);
            } catch (const std::exception& error) {
                MessageBoxA(window, error.what(), "Training Room", MB_OK | MB_ICONERROR);
            }
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
        if (menu.practice_load.valid()) {
            SetWindowTextW(window, L"Skullgirls - Loading");
            RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            return 0;
        }
        const wchar_t* caption = L"Skullgirls Main Menu Reconstruction";
        switch (menu.training) {
        case Menu::TrainingView::options: caption = L"Skullgirls - Training Options"; break;
        case Menu::TrainingView::team_size: caption = L"Skullgirls - Select Team Size"; break;
        case Menu::TrainingView::fighters: caption = menu.choosing_palette ? L"Skullgirls - Select Palette" : L"Skullgirls - Select Fighter"; break;
        case Menu::TrainingView::practice: caption = L"Skullgirls - Training Room"; break;
        case Menu::TrainingView::stage_select: caption = L"Skullgirls - Select Stage"; break;
        case Menu::TrainingView::stage_confirmed: caption = L"Skullgirls - Stage Selected"; break;
        default: break;
        }
        if (menu.showing_stage_select()) {
            const auto title = std::wstring(caption) + L" — " + menu.stage_selection_name();
            SetWindowTextW(window, title.c_str());
        } else {
            SetWindowTextW(window, caption);
        }
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    #ifdef _WIN32
    case WM_CREATE:
        return create_canvas(window) ? 0 : -1;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        if(wparam==SIZE_MINIMIZED && menu.sprite_renderer)menu.sprite_renderer->mark_occluded();
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const auto dc = BeginPaint(window, &paint);
        RECT rect{};
        GetClientRect(window, &rect);
        const auto width = std::min(rect.right, rect.bottom * 16 / 9);
        const auto height = width * 9 / 16;
        if (width > 0 && height > 0 && canvas_dc && !IsIconic(window)) {
            bool presented=false;
            if(!menu.direct_present_failed) {
                try {
                    menu.prepare_sprite_renderer();
                    if(menu.sprite_renderer) {
                        auto* renderer=menu.sprite_renderer.get();
                        if(renderer->begin_frame(window,canvas_pixels,canvas_stride)) {
                            Gdiplus::Graphics raster(canvas_image.get());raster.Clear(Gdiplus::Color::Transparent);
                            menu.paint(raster);
                            if(menu.sprite_renderer.get()!=renderer) throw std::runtime_error("GPU renderer changed during presentation");
                            renderer->present(raster);
                        }
                        presented=true;
                    }
                } catch(const std::exception& error) {
                    if(menu.sprite_renderer){menu.sprite_renderer->abort_frame();menu.sprite_renderer->detach_window();}
                    menu.direct_present_failed=true;OutputDebugStringA(error.what());
                }
            }
            if(!presented) {
                const auto black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
                // Draw directly into the DIB's premultiplied pixels. GDI+'s HDC
                // target is much slower for the transformed translucent super art.
                menu.paint(*canvas_image);

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
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_NCDESTROY:
        if(menu.sprite_renderer)menu.sprite_renderer->detach_window();
        canvas_pixels=nullptr;canvas_stride=0;
        canvas_image.reset();
        if (canvas_previous) SelectObject(canvas_dc, canvas_previous);
        if (canvas_bitmap) DeleteObject(canvas_bitmap);
        if (canvas_dc) DeleteDC(canvas_dc);
        return DefWindowProcW(window, message, wparam, lparam);
    #endif
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

// Slow motion uses a fraction of the normal update opportunities.
struct PracticeCadence {
    unsigned speed = 0;
    unsigned credit = 0;

    bool update_due(unsigned percent) {
        if (percent != speed) {
            speed = percent;
            credit = 100 - speed;
        }
        credit += speed;
        if (credit < 100) return false;
        credit -= 100;
        return true;
    }
};

// FUN_00519490 advances the fight before deciding whether to draw that frame.
bool render_practice_frame(std::chrono::steady_clock::time_point now,
                           std::chrono::steady_clock::time_point deadline, unsigned& skipped) {
    if (skipped < 3 && now - deadline > std::chrono::microseconds(5500)) {
        ++skipped;
        return false;
    }
    skipped = 0;
    return true;
}
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    struct GraphicsRuntime {
        ULONG_PTR token = 0;
        ~GraphicsRuntime() { if (token) Gdiplus::GdiplusShutdown(token); }
    } graphics;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&graphics.token, &input, nullptr) != Gdiplus::Ok) return 1;
    auto menu_storage = std::make_unique<Menu>(); auto& menu = *menu_storage;
    menu_instance = &menu;
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const std::filesystem::path requested = argv && argc == 2 ? argv[1] : L"";
    LocalFree(argv);
    try {
        if (argc > 2) throw std::runtime_error("Usage: skullgirls-main-menu [data02 folder or runtime folder]");
        std::wstring executable(32768, L'\0');
        const auto length = GetModuleFileNameW(nullptr, executable.data(), DWORD(executable.size()));
        if (!length || length >= executable.size()) throw std::runtime_error("Cannot locate the preview executable.");
        executable.resize(length);
        const auto path = skullgirls::resolve_runtime_data(executable, requested);
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
    PracticeCadence practice_cadence;
    unsigned skipped_practice_frames = 0;
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
            if (menu.training == Menu::TrainingView::practice && !menu.practice_paused) {
                // Bound catch-up after suspension or a long stall. Keep the simulation
                // on its own 60 Hz clock between presentations.
                if (now - deadline > 4 * frame_period) deadline = now;
                deadline += frame_period;
                if (practice_cadence.update_due(menu.practice_speed()))
                    menu.practice_tick();
                if (render_practice_frame(Clock::now(), deadline, skipped_practice_frames) && !IsIconic(window))
                    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                continue;
            }
            skipped_practice_frames = 0;
            practice_cadence = {};
            deadline += frame_period * (1 + (now - deadline) / frame_period);
            if (menu.practice_load.valid() &&
                menu.practice_load.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                try {
                    menu.finish_practice(window);
                    SetWindowTextW(window, L"Skullgirls - Training Room");
                } catch (const std::exception& error) {
                    menu.training = Menu::TrainingView::stage_select;
                    SetWindowTextW(window, L"Skullgirls - Select Stage");
                    MessageBoxA(window, error.what(), "Training Room", MB_OK | MB_ICONERROR);
                }
                deadline = Clock::now() + frame_period;
            }
            if (!menu.practice_paused && !IsIconic(window))
                RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
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

#else
#include "linux_main.hpp"
#endif
