#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <d3d9.h>
#include <wrl/client.h>

#include "sprite_asset.hpp"
#include "menu_timings.hpp"
#include "title_scene.hpp"
#include "fighter_select.hpp"
#include "select_idle.hpp"
#include "foit.hpp"
#include "filia_movement.hpp"
#include "filia_hurt.hpp"
#include "gfs.hpp"
#include "stage_scene.hpp"
#include <tuple>

#include <algorithm>
#include <chrono>
#include <cmath>
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
        bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &memory, nullptr, 0);
        if (!bitmap) throw std::runtime_error("Cannot allocate menu texture");
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

struct LoadingAnimation {
    struct Frame {
        unsigned first, count, ticks;
        float origin_x, origin_y;
    };
    struct Tile { unsigned x, y, u, v; };
    struct Image {
        float x, y;
        std::vector<unsigned> pixels;
        std::unique_ptr<Gdiplus::Bitmap> bitmap;
    };
    std::vector<Frame> frames;
    std::vector<Image> images;
    unsigned ticks = 0;

    void load(const std::filesystem::path& archive) {
        using skullgirls::read_sprite_asset;
        const auto bytes = read_sprite_asset(archive, "sprites/loading_en.spr.msb");
        skullgirls::StageReader reader(bytes);
        if (reader.string() != "2.0" || reader.string() != "loading_en")
            throw std::runtime_error("Unsupported loading sprite");
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
        atlas.load(read_sprite_asset(archive, "sprites/loading_en.dds"));
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
        if (!sequence_count || reader.string() != "bomb_en")
            throw std::runtime_error("Missing bomb_en loading animation");
        const auto count = reader.word(8), loop_end = reader.word(8);
        if (!count || count > frame_count || loop_end != count - 1)
            throw std::runtime_error("Invalid loading animation sequence");
        frames.assign(all_frames.begin(), all_frames.begin() + count);
        DIBSECTION dib{};
        if (!GetObjectW(atlas.bitmap, sizeof(dib), &dib))
            throw std::runtime_error("Cannot read loading atlas pixels");
        const auto* atlas_pixels = static_cast<const unsigned*>(dib.dsBm.bmBits);
        images.reserve(frames.size());
        for (const auto& frame : frames) {
            ticks += frame.ticks;
            unsigned width = 16, height = 16;
            for (unsigned i = frame.first; i < frame.first + frame.count; ++i) {
                width = std::max(width, (tiles[i].x + 1) * 16);
                height = std::max(height, (tiles[i].y + 1) * 16);
            }
            Image image{640 - frame.origin_x, 360 - frame.origin_y};
            image.pixels.resize(width * height);
            for (unsigned i = frame.first; i < frame.first + frame.count; ++i) {
                const auto& tile = tiles[i];
                for (unsigned y = 0; y < 16; ++y)
                    std::copy_n(atlas_pixels + (tile.v * 16 + y) * atlas.width + tile.u * 16, 16,
                                image.pixels.data() + (tile.y * 16 + y) * width + tile.x * 16);
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
        auto remaining = tick % ticks;
        std::size_t index = 0;
        while (remaining >= frames[index].ticks) remaining -= frames[index++].ticks;
        const auto& image = images[index];
        dc.DrawImage(image.bitmap.get(), image.x, image.y,
                     static_cast<float>(image.bitmap->GetWidth()), static_cast<float>(image.bitmap->GetHeight()));
    }
};

using Microsoft::WRL::ComPtr;

struct StageGpu {
    struct Vertex { float x, y, z, w; DWORD color; float u, v; };
    struct Mesh {
        ComPtr<IDirect3DVertexBuffer9> vertices;
        ComPtr<IDirect3DIndexBuffer9> indices;
        ComPtr<IDirect3DTexture9> texture;
    };
    ComPtr<IDirect3DDevice9> device;
    ComPtr<IDirect3DSurface9> target, staging;
    std::vector<Mesh> buffers;

    static void check(HRESULT result) {
        if (FAILED(result)) throw std::runtime_error("Direct3D stage operation failed");
    }

    StageGpu(HWND window, int width, int height, const std::vector<skullgirls::StageMesh>& meshes,
             const std::map<std::string, std::unique_ptr<Texture>>& textures) {
        ComPtr<IDirect3D9> d3d;
        d3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
        if (!d3d) throw std::runtime_error("Direct3D 9 is unavailable");
        D3DPRESENT_PARAMETERS params{};
        params.Windowed = TRUE;
        params.SwapEffect = D3DSWAPEFFECT_DISCARD;
        params.BackBufferWidth = width;
        params.BackBufferHeight = height;
        params.BackBufferFormat = D3DFMT_X8R8G8B8;
        params.EnableAutoDepthStencil = TRUE;
        params.AutoDepthStencilFormat = D3DFMT_D24S8;
        params.hDeviceWindow = window;
        check(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &params, &device));
        check(device->GetRenderTarget(0, &target));
        check(device->CreateOffscreenPlainSurface(width, height, D3DFMT_X8R8G8B8,
            D3DPOOL_SYSTEMMEM, &staging, nullptr));

        std::map<std::string, ComPtr<IDirect3DTexture9>> uploaded;
        for (const auto& mesh : meshes) {
            Mesh buffer;
            const auto& source = *textures.at(mesh.material);
            auto& texture = uploaded[mesh.material];
            if (!texture) {
                check(device->CreateTexture(source.width, source.height, 1, 0, D3DFMT_A8R8G8B8,
                    D3DPOOL_MANAGED, &texture, nullptr));
                DIBSECTION dib{};
                if (!GetObjectW(source.bitmap, sizeof(dib), &dib))
                    throw std::runtime_error("Cannot read stage texture pixels");
                D3DLOCKED_RECT lock{};
                check(texture->LockRect(0, &lock, nullptr, 0));
                for (int y = 0; y < source.height; ++y)
                    std::memcpy(static_cast<BYTE*>(lock.pBits) + y * lock.Pitch,
                        static_cast<const BYTE*>(dib.dsBm.bmBits) + y * dib.dsBm.bmWidthBytes,
                        source.width * 4);
                check(texture->UnlockRect(0));
            }
            buffer.texture = texture;
            check(device->CreateVertexBuffer(static_cast<UINT>(mesh.vertices.size() * sizeof(Vertex)),
                D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1,
                D3DPOOL_DEFAULT, &buffer.vertices, nullptr));
            check(device->CreateIndexBuffer(static_cast<UINT>(mesh.indices.size() * sizeof(unsigned)),
                D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_MANAGED, &buffer.indices, nullptr));
            void* index_data = nullptr;
            check(buffer.indices->Lock(0, 0, &index_data, 0));
            std::memcpy(index_data, mesh.indices.data(), mesh.indices.size() * sizeof(unsigned));
            check(buffer.indices->Unlock());
            buffers.push_back(std::move(buffer));
        }
    }

    void draw(int width, int height, const std::vector<skullgirls::StageMesh>& meshes,
              const std::vector<std::array<float, 2>>& uv, float center, float view_height,
              float bottom, float distance, float focal, float near_clip, float far_clip,
              std::vector<unsigned>& color) {
        check(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffaeb9c7, 1.0f, 0));
        check(device->BeginScene());
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_ZENABLE, TRUE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        device->SetRenderState(D3DRS_ALPHAREF, 255);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            const auto& mesh = meshes[i];
            const auto& buffer = buffers[i];
            void* data = nullptr;
            check(buffer.vertices->Lock(0, 0, &data, D3DLOCK_DISCARD));
            auto* vertex = static_cast<Vertex*>(data);
            for (const auto& v : mesh.vertices) {
                const float z = distance - v.z;
                const float q = 1.0f / std::max(z, near_clip);
                const unsigned a = v.rgba & 255;
                const auto premultiply = [a](unsigned c) { return (c & 255) * a / 255; };
                *vertex++ = {width * 0.5f + (v.x - center) * focal * q - 0.5f,
                    height * 0.5f - (v.y - bottom - view_height * 0.5f) * focal * q - 0.5f,
                    (z - near_clip) / (far_clip - near_clip), q,
                    D3DCOLOR_ARGB(a, premultiply(v.rgba >> 24), premultiply(v.rgba >> 16),
                                  premultiply(v.rgba >> 8)), v.u + uv[i][0], 1.0f - v.v - uv[i][1]};
            }
            check(buffer.vertices->Unlock());
            device->SetTexture(0, buffer.texture.Get());
            const auto filter = mesh.uv_offsets.empty() ? D3DTEXF_POINT : D3DTEXF_LINEAR;
            device->SetSamplerState(0, D3DSAMP_MINFILTER, filter);
            device->SetSamplerState(0, D3DSAMP_MAGFILTER, filter);
            device->SetStreamSource(0, buffer.vertices.Get(), 0, sizeof(Vertex));
            device->SetIndices(buffer.indices.Get());
            for (bool opaque : {true, false}) {
                device->SetRenderState(D3DRS_ALPHAFUNC, opaque ? D3DCMP_EQUAL : D3DCMP_LESS);
                device->SetRenderState(D3DRS_ALPHABLENDENABLE, !opaque);
                device->SetRenderState(D3DRS_ZWRITEENABLE, opaque);
                check(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                    static_cast<UINT>(mesh.vertices.size()), 0, static_cast<UINT>(mesh.indices.size() / 3)));
            }
        }
        check(device->EndScene());
        check(device->GetRenderTargetData(target.Get(), staging.Get()));
        D3DLOCKED_RECT lock{};
        check(staging->LockRect(&lock, nullptr, D3DLOCK_READONLY));
        for (int y = 0; y < height; ++y) {
            const auto* row = reinterpret_cast<const unsigned*>(static_cast<const BYTE*>(lock.pBits) + y * lock.Pitch);
            for (int x = 0; x < width; ++x) color[y * width + x] = row[x] | 0xff000000;
        }
        check(staging->UnlockRect());
    }
};

struct PracticeScene {
    static constexpr int width = 960, height = 540;
    struct ProjectedVertex { float x, y, q, u, v; unsigned rgba; };
    std::vector<skullgirls::StageMesh> meshes;
    std::vector<ProjectedVertex> points;
    std::vector<std::array<float, 2>> drawn_uv, current_uv;
    std::array<float, 4> drawn_camera{};
    bool has_drawn = false;
    std::unique_ptr<StageGpu> gpu;
    std::map<std::string, std::unique_ptr<Texture>> textures;
    std::vector<unsigned> color = std::vector<unsigned>(width * height);
    std::vector<float> depth = std::vector<float>(width * height);
    Gdiplus::Bitmap image{width, height, width * 4, PixelFormat32bppPARGB,
                          reinterpret_cast<BYTE*>(color.data())};
    float fov = 0, near_clip = 0, far_clip = 0;

    PracticeScene(const std::filesystem::path& directory, const std::string& level,
                  float stage_width, float stage_height, float floor,
                  float field_of_view, float near_plane, float far_plane)
        : meshes(skullgirls::read_stage_scene(directory / "stages.gfs", level, stage_width, stage_height, floor)),
          fov(field_of_view), near_clip(near_plane), far_clip(far_plane) {
        for (const auto& mesh : meshes) {
            if (textures.find(mesh.material) != textures.end()) continue;
            auto texture = std::make_unique<Texture>();
            texture->load(skullgirls::read_sprite_asset(directory / "stages-textures.gfs",
                                                        "stages/textures/" + mesh.material + ".dds"));
            textures.emplace(mesh.material, std::move(texture));
        }
        // Draw the classroom before its translucent fog and character layers.
        std::stable_partition(meshes.begin(), meshes.end(),
            [](const auto& mesh) { return mesh.material == "class_notes_3d"; });
        for (const auto& mesh : meshes)
            if (mesh.vertices.size() > points.capacity()) points.reserve(mesh.vertices.size());
        drawn_uv.resize(meshes.size());
        current_uv.resize(meshes.size());
    }

    void initialize_gpu(HWND window) {
        try {
            gpu = std::make_unique<StageGpu>(window, width, height, meshes, textures);
        } catch (const std::runtime_error&) {
            OutputDebugStringW(L"Class Notes Direct3D initialization failed. Using software rendering.\n");
        }
    }

    void draw(Gdiplus::Graphics& target, float center, float view_width, float view_height, float bottom,
              std::uint64_t tick) {
        const std::array<float, 4> camera{center, view_width, view_height, bottom};
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            const auto& mesh = meshes[i];
            if (tick < mesh.intro_uv_offsets.size()) {
                current_uv[i] = mesh.intro_uv_offsets[tick];
            } else if (!mesh.uv_offsets.empty()) {
                current_uv[i] = mesh.uv_offsets[(tick - mesh.intro_uv_offsets.size()) % mesh.uv_offsets.size()];
            } else {
                current_uv[i] = {};
            }
        }
        if (!has_drawn || camera != drawn_camera || current_uv != drawn_uv) {
            if (gpu) {
                const float distance = view_width * 0.5f / std::tan(fov * 0.00872664626f);
                try {
                    gpu->draw(width, height, meshes, current_uv, center, view_height, bottom,
                              distance, width * distance / view_width, near_clip, far_clip, color);
                } catch (const std::runtime_error&) {
                    gpu.reset();
                    OutputDebugStringW(L"Class Notes Direct3D rendering failed. Using software rendering.\n");
                }
            }
            if (!gpu) rasterize(center, view_width, view_height, bottom);
            drawn_camera = camera;
            drawn_uv.swap(current_uv);
            has_drawn = true;
        }
        target.DrawImage(&image, 0, 0, 1280, 720);
    }

    void rasterize(float center, float view_width, float view_height, float bottom) {
        std::fill(color.begin(), color.end(), 0xffaeb9c7);
        std::fill(depth.begin(), depth.end(), 0.0f);
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
            for (const auto& v : mesh.vertices) {
                const float z = distance - v.z;
                const float q = z >= near_clip && z <= far_clip ? 1.0f / z : 0.0f;
                points.push_back({width * 0.5f + (v.x - center) * focal * q,
                    height * 0.5f - (v.y - bottom - view_height * 0.5f) * focal * q, q,
                    (v.u + uv[0]) * q, (v.v + uv[1]) * q, v.rgba});
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
                        const float tx = std::clamp(u * texture.width - 0.5f, 0.0f, float(texture.width - 1));
                        const float ty = std::clamp((1 - v) * texture.height - 0.5f, 0.0f, float(texture.height - 1));
                        const int x0 = static_cast<int>(tx), y0 = static_cast<int>(ty);
                        unsigned texel = texels[y0 * texture.width + x0];
                        if (!mesh.uv_offsets.empty()) {
                            const int x1 = std::min(x0 + 1, texture.width - 1);
                            const int y1 = std::min(y0 + 1, texture.height - 1);
                            const auto tr = texels[y0 * texture.width + x1];
                            const auto bl = texels[y1 * texture.width + x0];
                            const auto br = texels[y1 * texture.width + x1];
                            const float fx = tx - x0, fy = ty - y0;
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

// Filia FOIT moves 117, 121, 122 and 124. Zero sprite records hold the prior pose.
constexpr skullgirls::NormalFrame throw_start[] = {
    {399, 3}, {400, 3}, {401, 5}
};
constexpr skullgirls::NormalFrame throw_whiff[] = {
    {401, 1}, {402, 9}, {401, 4}, {403, 3}, {404, 3}, {405, 3}, {406, 3}, {407, 3}
};
constexpr skullgirls::NormalFrame throw_capture[] = {
    {401, 1}, {408, 16}, {409, 5}, {410, 4}, {411, 4},
    {412, 4}, {418, 2}, {419, 1}, {420, 1},
    {412, 4}, {418, 2}, {419, 1}, {420, 1},
    {412, 4}, {418, 2}, {419, 1}, {420, 1},
    {412, 3}, {418, 1}, {419, 1}, {420, 2},
    {413, 5}, {414, 5}, {415, 4}, {416, 4}, {417, 4}
};
constexpr skullgirls::NormalFrame throw_recovery[] = {
    {417, 1}, {421, 6}, {422, 4}, {423, 3}, {407, 4}
};

struct Menu {
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
    std::size_t stage_index = 0;
    std::vector<std::vector<std::size_t>> stage_groups;
    std::size_t stage_variant = 0;
    Texture stage_bar, stage_arrow, stage_variant_arrow, stage_header;

    const Stage& current_stage() const {
        return stages[stage_groups[stage_index][stage_variant]];
    }

    std::filesystem::path game_data;
    std::unique_ptr<Texture> practice_background;
    std::unique_ptr<PracticeScene> practice_scene;
    struct PracticeFighter {
        float practice_x = 0;
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
        unsigned dash_frame = 0, dash_tick = 0, movement_sprite = 0, walk_tick = 0;
        int walk_direction = 0;
        bool practice_down = false, practice_jump = false;
        unsigned stance_tick = 0;
        bool crouching = false, rising = false;
        unsigned jump_start = 0, air_ticks = 0, landing = 0;
        float jump_height = 0, jump_velocity = 0, jump_horizontal = 0;
        int jump_direction = 0;
        bool jump_facing_right = true;
        bool air_dash_used = false, double_jump_used = false;
        unsigned air_dash_tick = 0, double_jump_tick = 0;
        unsigned fall_tick = 0;
        int air_dash_direction = 0;
        int normal_input = -1, active_normal = -1;
        unsigned pressed_punches = 0, pressed_kicks = 0;
        float hairball_speed = 0;
        unsigned hairball_roll_ticks = 0, updo_flight_ticks = 0;
        int queued_followup = -1;
        unsigned normal_frame = 0, normal_tick = 0, normal_pose = 0;
        bool normal_facing_right = true, normal_contact = false;
        const skullgirls::NormalHit* normal_hit = nullptr;
        const skullgirls::NormalHit* last_contact = nullptr;
        unsigned hit_stop = 0, hurt_frame = 0, hurt_tick = 0, hurt_hold = 0;
        int hurt_group = -1;
        unsigned hurt_phase = 0;
        bool hurt_facing_right = true;
        skullgirls::HeavyReaction::Type heavy_reaction = skullgirls::HeavyReaction::none;
        float recoil_x = 0;
        unsigned recoil_ticks = 0;
        bool recoil_landed = false, waking_up = false;
        int striking_normal = -1;
        bool throw_input = false;
        bool throw_facing_right = true;
        unsigned throw_tick = 0;
        enum class ThrowPhase { none, start, whiff, capture, recovery, grabbed };
        ThrowPhase throw_phase = ThrowPhase::none;

        void take_hit(const skullgirls::NormalHit& hit, bool facing_right, int move,
                      skullgirls::HeavyReaction forced = {}) {
            const auto reaction = forced.type != skullgirls::HeavyReaction::none ? forced :
                (jump_height > 0 ? skullgirls::HeavyReaction{} : skullgirls::filia_heavy_reaction(move));
            throw_phase = ThrowPhase::none;
            heavy_reaction = reaction.type;
            recoil_x = facing_right ? -reaction.horizontal : reaction.horizontal;
            recoil_ticks = 0;
            recoil_landed = false;
            waking_up = false;
            if (heavy_reaction != skullgirls::HeavyReaction::none)
                jump_velocity = reaction.upward;
            const bool low = hit.height_mask == 2;
            hurt_group = jump_height > 0 || jump_velocity > 0 ? skullgirls::air_mid :
                crouching ? (low ? skullgirls::crouch_low : skullgirls::crouch_high) :
                low ? skullgirls::stand_low : skullgirls::stand_mid;
            hurt_facing_right = facing_right;
            hurt_phase = hurt_frame = hurt_tick = 0;
            hurt_hold = hit.stun;
            hit_stop = std::max(hit_stop, hit.stop);
            movement_sprite = skullgirls::filia_hurt[hurt_group][0][0].sprite;
            active_normal = normal_input = -1;
            queued_followup = -1;
            normal_hit = nullptr;
            dash_direction = air_dash_direction = 0;
            jump_start = landing = walk_tick = 0;
            walk_direction = 0;
            dash_input_history.fill(skullgirls::filia_neutral);
        }

        void tick_hurt(float left, float right) {
            using Reaction = skullgirls::HeavyReaction;
            if (heavy_reaction != Reaction::none) {
                if (!recoil_landed) {
                    practice_x = std::clamp(practice_x + recoil_x, left, right);
                    jump_height = std::max(0.0f, jump_height + jump_velocity);
                    jump_velocity -= 1.0f;
                    if (!jump_height && jump_velocity < 0) {
                        recoil_landed = true;
                        jump_velocity = 0;
                        recoil_ticks = 0;
                    }
                }
                if (heavy_reaction == Reaction::launch) {
                    movement_sprite = jump_velocity > 0 ? 762 : 768;
                    if (recoil_landed) {
                        heavy_reaction = Reaction::none;
                        hurt_group = -1;
                        movement_sprite = 113;
                        landing = 1;
                    }
                    return;
                }
                const bool front = heavy_reaction == Reaction::sweep;
                const auto pose = [&](const auto& frames) {
                    unsigned tick = recoil_ticks++;
                    for (const auto& frame : frames) {
                        if (tick < frame.ticks) return frame.sprite;
                        tick -= frame.ticks;
                    }
                    return recoil_landed ? 0u : frames[std::size(frames) - 1].sprite;
                };
                if (!recoil_landed) {
                    movement_sprite = front ? pose(skullgirls::filia_front_fall) : pose(skullgirls::filia_knockback);
                    return;
                }
                if (!waking_up) {
                    movement_sprite = front ? pose(skullgirls::filia_front_down) : pose(skullgirls::filia_back_down);
                    if (movement_sprite) return;
                    waking_up = true;
                    recoil_ticks = 0;
                }
                movement_sprite = front ? pose(skullgirls::filia_front_wakeup) : pose(skullgirls::filia_back_wakeup);
                if (!movement_sprite) {
                    heavy_reaction = Reaction::none;
                    hurt_group = -1;
                    crouching = rising = false;
                    stance_tick = 0;
                }
                return;
            }

            const auto& sequence = skullgirls::filia_hurt[hurt_group][hurt_phase];
            const auto& frame = sequence[hurt_frame];
            movement_sprite = frame.sprite;
            if (hurt_hold) --hurt_hold;
            // Release the terminal recovery pose when returning control to the player.
            if (++hurt_tick >= (frame.ticks == 300 ? 1u : frame.ticks)) {
                hurt_tick = 0;
                if (++hurt_frame == std::size(sequence) || !sequence[hurt_frame].ticks) {
                    hurt_frame = 0;
                    if (hurt_phase == 0) hurt_phase = 1;
                    else if (hurt_phase == 1 && !hurt_hold) hurt_phase = hurt_group == skullgirls::air_mid ? 2 : 3;
                    else if (hurt_phase >= 2) hurt_group = -1;
                }
            }
            if (jump_height > 0 || jump_velocity > 0) {
                jump_height = std::max(0.0f, jump_height + jump_velocity);
                jump_velocity = jump_height > 0 ? jump_velocity - 1.0f : 0.0f;
            }
        }


        void advance_throw() {
            const auto play = [&](const auto& frames) {
                unsigned tick = throw_tick++;
                for (const auto& frame : frames) {
                    if (tick < frame.ticks) {
                        movement_sprite = frame.sprite;
                        return true;
                    }
                    tick -= frame.ticks;
                }
                throw_tick = 0;
                return false;
            };
            switch (throw_phase) {
            case ThrowPhase::start:
                if (!play(throw_start)) {
                    throw_phase = ThrowPhase::whiff;
                    play(throw_whiff);
                }
                break;
            case ThrowPhase::whiff:
                if (!play(throw_whiff)) throw_phase = ThrowPhase::none;
                break;
            case ThrowPhase::capture:
                if (!play(throw_capture)) {
                    throw_phase = ThrowPhase::recovery;
                    play(throw_recovery);
                }
                break;
            case ThrowPhase::recovery:
                if (!play(throw_recovery)) throw_phase = ThrowPhase::none;
                break;
            default: break;
            }
            if (throw_phase == ThrowPhase::none) movement_sprite = 0;
        }

        bool update_dash_input(int horizontal, bool airborne) {
            std::move_backward(dash_input_history.begin(), dash_input_history.end() - 1,
                               dash_input_history.end());
            const int vertical = int(practice_up) - int(practice_down);
            dash_input_history.front() = skullgirls::filia_input_direction(horizontal, vertical);
            return skullgirls::filia_double_tap(dash_input_history, horizontal, airborne);
        }

        void tick(float opponent_x, float left, float right, bool faces_right_at_tie,
                  const std::array<unsigned, std::size(skullgirls::filia_normals)>& multi_hit_end) {
            normal_hit = nullptr;
            striking_normal = -1;
            const bool wants_throw = std::exchange(throw_input, false);
            const unsigned punches = std::exchange(pressed_punches, 0u);
            const unsigned kicks = std::exchange(pressed_kicks, 0u);
            const int direction = int(practice_right) - int(practice_left);
            const bool airborne = jump_height > 0 || jump_velocity > 0;
            const bool repeated_direction = update_dash_input(direction, airborne);
            if (throw_phase != ThrowPhase::none) {
                normal_input = -1;
                practice_jump = false;
                advance_throw();
                return;
            }
            if (hit_stop || hurt_group >= 0) {
                practice_jump = false;
                if (hurt_group >= 0) normal_input = -1;
                if (hit_stop) --hit_stop;
                else tick_hurt(left, right);
                return;
            }
            const bool facing_right = (practice_x < opponent_x || (practice_x == opponent_x && faces_right_at_tie));
            if (wants_throw && active_normal < 0 && !airborne && !jump_start && !landing) {
                throw_phase = ThrowPhase::start;
                throw_facing_right = facing_right;
                throw_tick = 0;
                normal_input = -1;
                dash_direction = 0;
                advance_throw();
                return;
            }
            int next_normal = -1;
            // A fresh LP press can be queued during jab startup, then resolved on contact.
            const bool repeat_jab = normal_input == 0 &&
                (active_normal == 0 || active_normal == 6 ||
                 active_normal == skullgirls::rapid_stand_lp || active_normal == skullgirls::rapid_crouch_lp);
            if (active_normal >= 0 && normal_input >= 0 && (normal_contact || repeat_jab)) {
                const auto followup = skullgirls::filia_normal_followup(
                    active_normal, normal_input, airborne, practice_down);
                if (followup >= 0) queued_followup = followup;
            }
            if (normal_input >= 0 && !jump_start && !landing) {
                if (active_normal < 0) {
                    if (!airborne && (punches || normal_input < 3) &&
                        skullgirls::filia_updo_motion(dash_input_history, facing_right)) {
                        const int strength = (punches & 4u) ? 2 : (punches & 2u) ? 1 :
                            punches ? 0 : normal_input;
                        next_normal = skullgirls::updo_light + strength;
                    } else if (kicks && skullgirls::filia_quarter_circle(
                                   dash_input_history, !facing_right, airborne)) {
                        next_normal = airborne ? skullgirls::air_hairball : skullgirls::ground_hairball;
                        // Moves 148-150 set horizontal speed in 0xa8 and the roll counter in 0xa9.
                        const unsigned strength = (kicks & 4u) ? 2 : (kicks & 2u) ? 1 : 0;
                        constexpr float ground_speed[] = {18, 22, 23};
                        constexpr unsigned ground_roll_ticks[] = {11, 17, 29};
                        hairball_speed = airborne ? 22.0f : ground_speed[strength];
                        hairball_roll_ticks = airborne ? 17 : ground_roll_ticks[strength];
                    } else if (!airborne && (kicks & 4u) && skullgirls::filia_quarter_circle(
                                   dash_input_history, facing_right, false)) {
                        next_normal = skullgirls::ringlet_psych;
                    } else {
                        next_normal = normal_input + (airborne ? 12 : practice_down ? 6 : 0);
                    }
                }
            }
            if (active_normal >= 0 && queued_followup >= 0) {
                const bool rapid_fire = queued_followup == skullgirls::rapid_stand_lp ||
                    queued_followup == skullgirls::rapid_crouch_lp;
                const bool regular_jab = active_normal == 0 || active_normal == 6;
                const bool past_active_frames = (regular_jab && !rapid_fire) ||
                    normal_frame >= multi_hit_end[active_normal];
                if ((normal_contact || rapid_fire) && past_active_frames) next_normal = queued_followup;
            }
            if (next_normal >= 0) {
                if (active_normal < 0) normal_facing_right = airborne ? jump_facing_right : facing_right;
                active_normal = next_normal;
                updo_flight_ticks = 0;
                normal_frame = normal_tick = 0;
                // The repeat starts with a script-only record. Keep the displayed jab pose
                // until that record supplies a replacement sprite.
                if (next_normal != skullgirls::rapid_stand_lp && next_normal != skullgirls::rapid_crouch_lp)
                    normal_pose = 0;
                normal_contact = false;
                last_contact = nullptr;
                queued_followup = -1;
                dash_direction = 0;
                walk_tick = 0;
                walk_direction = 0;
                if ((next_normal >= 6 && next_normal < 12) || next_normal == skullgirls::rapid_crouch_lp) {
                    crouching = true;
                    rising = false;
                    stance_tick = 19;
                }
            }
            normal_input = -1;
            const int relative = facing_right ? direction : -direction;
            const bool doing_updo = active_normal >= skullgirls::updo_light && active_normal < skullgirls::ground_hairball;
            if (active_normal < 0 && repeated_direction && !dash_direction && !practice_down && !crouching &&
                !rising && !jump_start && !jump_velocity && !jump_height && !landing && !practice_jump) {
                dash_direction = relative;
                dash_facing_right = facing_right;
                dash_frame = dash_tick = 0;
            }
            const float walk_speed = relative > 0 ? skullgirls::walk_forward.front().velocity :
                skullgirls::walk_backward.front().velocity;
            float movement = relative ? (facing_right ? walk_speed : -walk_speed) : 0.0f;
            movement_sprite = 0;
            const bool was_dashing = dash_direction != 0;
            const bool forward_dash_jump = dash_direction > 0 && relative > 0;
            if (walk_direction != relative || dash_direction || active_normal >= 0) walk_tick = 0;
            walk_direction = relative;
            if (dash_direction) {
                const auto& frames = dash_direction > 0 ? skullgirls::dash_forward : skullgirls::dash_backward;
                const auto& frame = frames[dash_frame];
                movement_sprite = frame.sprite;
                movement = dash_facing_right ? frame.velocity : -frame.velocity;
                if (++dash_tick == frame.ticks) {
                    dash_tick = 0;
                    if (++dash_frame == frames.size()) dash_direction = 0;
                }
            }
            if (!movement_sprite && relative) {
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
            if (active_normal < 0 && practice_jump && !jump_start && !jump_velocity && !jump_height && !landing &&
                (!was_dashing || forward_dash_jump)) {
                jump_start = 1;
                jump_direction = relative;
                jump_facing_right = facing_right;
                // Move 52 sets +7/-7. The dash jump in move 53 sets +13 forward.
                const float speed = forward_dash_jump ? 13.0f : 7.0f;
                jump_horizontal = speed * direction;
                dash_direction = 0;
                air_ticks = 0;
                fall_tick = 0;
                air_dash_used = double_jump_used = false;
                crouching = rising = false;
            }
            const bool jump_pressed = std::exchange(practice_jump, false);
            if (jump_start) {
                constexpr std::array<unsigned, 4> startup{119, 120, 121, 121};
                movement = 0;
                movement_sprite = startup[jump_start - 1];
                if (++jump_start > startup.size()) {
                    jump_start = 0;
                    jump_velocity = 32.0f;
                }
            } else if (!doing_updo && (jump_velocity || jump_height)) {
                const int air_relative = jump_facing_right ? direction : -direction;
                if (active_normal < 0 && repeated_direction && !air_dash_used && !air_dash_direction) {
                    air_dash_used = true;
                    air_dash_direction = air_relative;
                    air_dash_tick = 0;
                    fall_tick = 0;
                    jump_velocity = 0;
                }
                if (active_normal < 0 && jump_pressed && !double_jump_used && !air_dash_direction) {
                    double_jump_used = true;
                    double_jump_tick = 1;
                    fall_tick = 0;
                    jump_direction = air_relative;
                    jump_horizontal = 10.0f * direction;
                    jump_velocity = 39.0f;
                }
                movement = jump_horizontal;
                const unsigned first_pose = jump_direction > 0 ? 132 : jump_direction < 0 ? 134 : 130;
                movement_sprite = first_pose + (air_ticks++ >= 4);
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
                        air_dash_direction = 0;
                        jump_horizontal = 0;
                        return 0.0f;
                    };
                    movement = air_dash_direction > 0 ? step(skullgirls::air_dash_forward) :
                        step(skullgirls::air_dash_backward);
                } else if (double_jump_tick) {
                    constexpr std::array<unsigned, 10> poses{0,0,0,1,1,1,2,2,3,3};
                    const unsigned base = jump_direction > 0 ? 171 : jump_direction < 0 ? 176 : 166;
                    const auto tick = double_jump_tick++ - 1;
                    movement_sprite = tick < poses.size() ? base + poses[tick] : base + 4;
                    if (tick >= poses.size()) double_jump_tick = static_cast<unsigned>(poses.size()) + 1;
                }
                if (!air_dash_direction) {
                    // FOIT moves 60/62/64 branch to 71/72/73 below +7 vertical velocity.
                    // Their successors 77/79/81 play the descent into the held fall pose.
                    if (jump_velocity < 7.0f && !double_jump_tick) {
                        constexpr std::array<unsigned, 20> descent{
                            0,0,1,1,1,2,2,3,3,3,4,4,4,5,5,5,6,6,7,7};
                        const unsigned approach = jump_direction > 0 ? 146 : jump_direction < 0 ? 150 : 142;
                        const unsigned fall = jump_direction > 0 ? 171 : jump_direction < 0 ? 176 : 166;
                        if (fall_tick < descent.size()) {
                            const unsigned pose = descent[fall_tick++];
                            movement_sprite = pose < 4 ? approach + pose : fall + pose - 4;
                        } else {
                            movement_sprite = fall + 4;
                        }
                    }
                    jump_height += jump_velocity;
                    jump_velocity -= 1.0f;
                }
                if (!air_dash_direction && jump_height <= 0 && jump_velocity < 0) {
                    jump_height = jump_velocity = 0;
                    double_jump_tick = 0;
                    if (active_normal == skullgirls::air_hairball) {
                        active_normal = skullgirls::ground_hairball;
                        normal_frame = skullgirls::hairball_end_frame;
                        normal_tick = 0;
                        hairball_roll_ticks = 0;
                        last_contact = &skullgirls::filia_normal_hits[active_normal][1];
                    } else {
                        movement_sprite = 113;
                        active_normal = -1;
                        queued_followup = -1;
                        landing = 1;
                    }
                }
            } else if (landing) {
                constexpr std::array<unsigned, 17> poses{
                    114,114,114,115,115,115,116,116,116,116,117,117,117,118,118,118,118};
                movement_sprite = poses[landing - 1];
                movement = 0;
                if (++landing > poses.size()) landing = 0;
            } else if (active_normal < 0 && !was_dashing && (practice_down || crouching || rising)) {
                constexpr std::array<unsigned, 19> down_poses{
                    40,40,40,40,40,40,41,41,41,42,42,42,43,43,43,44,44,44,44};
                constexpr std::array<unsigned, 21> up_poses{
                    44,44,44,45,45,45,46,46,46,47,47,47,47,48,48,48,48,49,49,49,49};
                constexpr std::array<unsigned, 18> idle_duration{5,5,5,5,4,5,4,5,4,5,5,5,4,5,4,5,4,5};
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
                        last_contact = &skullgirls::filia_normal_hits[active_normal][0];
                    }
                }
            }
            if (active_normal >= 0) {
                striking_normal = active_normal;
                const auto& sequence = skullgirls::filia_normals[active_normal];
                const auto& frame = sequence[normal_frame];
                if (frame.sprite) normal_pose = frame.sprite;
                if (normal_pose) movement_sprite = normal_pose;
                for (const auto& hit : skullgirls::filia_normal_hits[active_normal]) {
                    if (!hit.frame || hit.frame > normal_frame + 1) break;
                    normal_hit = &hit;
                }
                if (doing_updo &&
                    normal_frame >= skullgirls::updo_motion[active_normal - skullgirls::updo_light].flight_frame +
                        skullgirls::updo_air_loop_end)
                    normal_hit = nullptr;
                // Move 113 exposes its attack boxes before its conditional hit record.
                // Use that record for the preceding active poses in the preview.
                if (active_normal == skullgirls::air_mp && !normal_hit && normal_pose)
                    normal_hit = &skullgirls::filia_normal_hits[active_normal][0];
                if (active_normal < 12 || active_normal >= skullgirls::rapid_stand_lp) movement = 0;
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
                if (++normal_tick >= frame.ticks) {
                    normal_tick = 0;
                    ++normal_frame;
                    if (hairball_roll && normal_frame == skullgirls::hairball_end_frame) {
                        normal_frame = skullgirls::hairball_loop_frame;
                    }
                    if (doing_updo && jump_height > 0) {
                        const auto flight = skullgirls::updo_motion[active_normal - skullgirls::updo_light].flight_frame;
                        if (normal_frame == flight + skullgirls::updo_air_loop_end)
                            normal_frame = flight + skullgirls::updo_air_loop_start;
                    }
                    if (normal_frame == std::size(sequence) || !sequence[normal_frame].ticks) {
                        active_normal = -1;
                        queued_followup = -1;
                    }
                }
            }
            practice_x = std::clamp(practice_x + movement, left, right);
        }

        bool mirror(bool facing_right) const {
            if (throw_phase != ThrowPhase::none) return !throw_facing_right;
            if (movement_sprite >= 702) return !hurt_facing_right;
            if (movement_sprite >= 198) return !normal_facing_right;
            if (movement_sprite >= 113) return !jump_facing_right;
            if (movement_sprite >= 93) return !dash_facing_right;
            return !facing_right;
        }
    };
    std::array<PracticeFighter, 2> practice_players;
    std::array<float, 2> practice_starts{};
    float practice_width = 0, practice_height = 0, practice_floor = 0;
    std::array<skullgirls::SelectFrame, 953> movement_frames;
    std::vector<skullgirls::SpriteFrame> combat_metadata;
    std::map<unsigned, skullgirls::SelectFrame> jab_overlays;
    std::array<unsigned, std::size(skullgirls::filia_normals)> multi_hit_end{};

    void load_movement_frames() {
        if (movement_frames[2].width) return;
        using skullgirls::read_sprite_asset;
        const auto art = read_sprite_asset(game_data / "characters-art.gfs", "characters/filia.art");
        const auto alternate = read_sprite_asset(game_data / "characters-art-pt.gfs", "characters/filia.art-pt");
        auto frames = skullgirls::read_foit_frames(
            read_sprite_asset(game_data / "characters-foits.gfs", "characters/filia.foit"), art.size());
        decltype(movement_frames) decoded;
        decltype(jab_overlays) overlays;
        for (unsigned ref = 2; ref < decoded.size(); ++ref) {
            if ((ref >= 50 && ref < 59) || (ref >= 122 && ref < 130) ||
                (ref >= 136 && ref < 142) || (ref >= 154 && ref < 166) ||
                (ref >= 181 && ref < 190) ||
                (ref >= 424 && ref < 466) || ref == 509 || (ref >= 543 && ref < 702) || ref == 729 ||
                (ref >= 773 && ref < 828) || (ref >= 829 && ref < 933)) continue;
            const auto& source = frames.at(ref - 1);
            const auto& layer = source.layers[0].value();
            auto& frame = decoded[ref];
            frame.width = layer.tile_columns * 16;
            frame.height = layer.tile_rows * 16;
            frame.origin_x = float(source.origin[0]) - float(source.primary_layer_offset[0]);
            frame.origin_y = float(source.origin[1]) - float(source.primary_layer_offset[1]);
            const auto& bank = source.art_bank == 4 ? alternate : art;
            frame.pixels = skullgirls::decode_sprite_layer(layer, bank);
            // Jab recovery and rapid-fire smear art share the primary layer's canvas.
            if ((ref == 201 || (ref >= 204 && ref <= 207)) && source.layers[1]) {
                const auto& overlay = *source.layers[1];
                if (overlay.tile_columns != layer.tile_columns || overlay.tile_rows != layer.tile_rows)
                    throw std::runtime_error("Unexpected Filia jab layer dimensions");
                auto& effect = overlays[ref];
                effect.width = frame.width;
                effect.height = frame.height;
                effect.pixels = skullgirls::decode_sprite_layer(overlay, bank);
            }
        }
        movement_frames = std::move(decoded);
        jab_overlays = std::move(overlays);
        combat_metadata = std::move(frames);
        for (std::size_t move = 0; move < multi_hit_end.size(); ++move) {
            const bool jab = move == 0 || move == 6;
            if (!jab && !skullgirls::filia_normal_hits[move][1].frame) continue;
            unsigned sprite = 0, frame_number = 0;
            for (const auto& frame : skullgirls::filia_normals[move]) {
                if (!frame.ticks) break;
                ++frame_number;
                if (frame.sprite) sprite = frame.sprite;
                if (sprite && !combat_metadata.at(sprite - 1).box_groups[0].empty())
                    multi_hit_end[move] = frame_number;
            }
        }
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
        const float zoom = std::max(horizontal_zoom, vertical_zoom);
        practice_camera_zoom = snap ? zoom : practice_camera_zoom + (zoom - practice_camera_zoom) / 2.5f;
        const int view_height = static_cast<int>(720.0f + 180.0f * practice_camera_zoom);
        int target_bottom = 0;
        for (const auto& player : practice_players) {
            if (player.jump_height <= 0) continue;
            const float top = player.movement_sprite ? movement_frames[player.movement_sprite].origin_y : 0.0f;
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
                                            static_cast<int>(practice_width) - half_width);
    }

    LoadingAnimation loading_animation;
    std::chrono::steady_clock::time_point loading_started{};
    std::future<void> practice_load;

    ~Menu() { if (practice_load.valid()) practice_load.wait(); }

    void start_practice() {
        loading_started = std::chrono::steady_clock::now();
        practice_load = std::async(std::launch::async, [this] { load_practice(); });
    }

    void load_practice() {
        load_movement_frames();
        prepare_practice_bitmaps();
        const auto config = skullgirls::read_sprite_asset(game_data / "stages.gfs", "stages/" + current_stage().level + ".lvl");
        std::istringstream stream(std::string(config.begin(), config.end()));
        std::string background_asset;
        float fov = 0, near_plane = 0, far_plane = 0;
        for (std::string line; std::getline(stream, line);) {
            std::istringstream row(line);
            std::string key;
            row >> key;
            if (key == "StageSize:") row >> practice_width >> practice_height;
            else if (key == "BottomClearance:") row >> practice_floor;
            else if (key == "Start1:") row >> practice_starts[0];
            else if (key == "Start2:") row >> practice_starts[1];
            else if (key == "2D") row >> background_asset;
            else if (key == "CAMERA") row >> fov >> near_plane >> far_plane;
        }
        if (background_asset.empty() || practice_width < 1600 || practice_height < 900)
            throw std::runtime_error("Invalid training stage layout");
        std::unique_ptr<Texture> backdrop;
        std::unique_ptr<PracticeScene> scene;
        if (current_stage().level == "class_notes_3d" && fov > 0 && near_plane > 0 && far_plane > near_plane) {
            scene = std::make_unique<PracticeScene>(game_data, current_stage().level, practice_width,
                practice_height, practice_floor, fov, near_plane, far_plane);
        } else {
            backdrop = std::make_unique<Texture>();
            backdrop->load(skullgirls::read_sprite_asset(game_data / "stages-textures2d.gfs", background_asset));
        }
        practice_scene = std::move(scene);
        practice_background = std::move(backdrop);
    }

    void finish_practice(HWND window) {
        practice_load.get();
        if (practice_scene) practice_scene->initialize_gpu(window);
        practice_stage_tick = 0;
        practice_keys_held = {};
        reset_practice();
        idle_start = std::chrono::steady_clock::now();
        training = TrainingView::practice;
    }

    // FUN_0051e6a0 copies the Windows keyboard state to the current snapshot
    // and preserves the previous snapshot. Combat commands read the snapshots.
    std::array<unsigned, 2> practice_keys_held{};
    std::array<unsigned, 2> practice_keys_previous{};

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
            if (pressed) practice_keys_held[side] |= mask;
            else practice_keys_held[side] &= ~mask;
            return;
        }
    }

    void sample_practice_keys() {
        constexpr unsigned throw_buttons = (1u << 4) | (1u << 7);
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            const unsigned held = practice_keys_held[side];
            const unsigned pressed = held & ~practice_keys_previous[side];
            practice_keys_previous[side] = held;
            auto& player = practice_players[side];
            player.practice_left = (held & 1u) != 0;
            player.practice_right = (held & 2u) != 0;
            player.practice_down = (held & 4u) != 0;
            player.practice_up = (held & 8u) != 0;
            player.practice_jump = (pressed & 8u) != 0;
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
        practice_keys_previous = practice_keys_held;
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            practice_players[side] = {};
            practice_players[side].practice_x = practice_starts[side];
        }
        update_practice_camera(true);
    }

    bool practice_facing_right(std::size_t side) const {
        const float x = practice_players[side].practice_x;
        const float other = practice_players[1 - side].practice_x;
        return x < other || (x == other && side == 0);
    }

    void practice_tick() {
        sample_practice_keys();
        ++practice_stage_tick;
        const float half_width = 640.0f + 160.0f * practice_camera_zoom;
        // FUN_0057f150 publishes fighter limits 95 units inside the camera edges.
        const float left = std::max(0.0f, practice_camera_center - half_width + 95.0f);
        const float right = std::min(practice_width, practice_camera_center + half_width - 95.0f);
        const float x1 = practice_players[0].practice_x;
        const float x2 = practice_players[1].practice_x;
        practice_players[0].tick(x2, left, right, true, multi_hit_end);
        if (std::string_view(roster[fighters[1].front()]) == "filia")
            practice_players[1].tick(x1, left, right, false, multi_hit_end);

        std::array<const skullgirls::NormalHit*, 2> contacts{};
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& player = practice_players[side];
            const auto& opponent = practice_players[1 - side];
            if (player.movement_sprite < 198 || !player.normal_hit || player.normal_hit == player.last_contact ||
                std::string_view(roster[fighters[1 - side].front()]) != "filia") continue;
            const unsigned defender_sprite = opponent.movement_sprite ? opponent.movement_sprite : practice_idle_sprite();
            const auto& attacks = combat_metadata[player.movement_sprite - 1].box_groups[0];
            const auto& hurts = combat_metadata[defender_sprite - 1].box_groups[1];
            for (const auto& hit : attacks) {
                const auto world_hit = skullgirls::foit_world_box(hit, static_cast<int>(player.practice_x),
                    static_cast<int>(player.jump_height), player.normal_facing_right);
                for (const auto& hurt : hurts) {
                    const auto world_hurt = skullgirls::foit_world_box(hurt, static_cast<int>(opponent.practice_x),
                        static_cast<int>(opponent.jump_height), !opponent.mirror(practice_facing_right(1 - side)));
                    if (skullgirls::foit_boxes_overlap(world_hit, world_hurt)) contacts[side] = player.normal_hit;
                }
            }
        }
        for (std::size_t side = 0; side < contacts.size(); ++side) {
            const auto* hit = contacts[side];
            if (!hit) continue;
            auto& player = practice_players[side];
            player.last_contact = hit;
            player.normal_contact = true;
            player.hit_stop = std::max(player.hit_stop, hit->stop);
            practice_players[1 - side].take_hit(*hit, !player.normal_facing_right, player.striking_normal);
        }
        resolve_practice_throws(left, right);
        update_practice_camera(false);
    }

    void resolve_practice_throws(float left, float right) {
        using Phase = PracticeFighter::ThrowPhase;
        for (std::size_t side = 0; side < practice_players.size(); ++side) {
            auto& attacker = practice_players[side];
            auto& defender = practice_players[1 - side];
            const float forward = attacker.throw_facing_right ? 1.0f : -1.0f;
            // Move 117 arms the grab on its ninth serialized tick.
            if (attacker.throw_phase == Phase::start && attacker.throw_tick >= 9 && attacker.movement_sprite == 401 &&
                defender.throw_phase != Phase::grabbed && defender.hurt_group < 0 &&
                defender.jump_height == 0 && defender.jump_velocity == 0 &&
                std::string_view(roster[fighters[1 - side].front()]) == "filia") {
                const auto& grab = combat_metadata[400].box_groups[0];
                const unsigned pose = defender.movement_sprite ? defender.movement_sprite : practice_idle_sprite();
                const auto& hurt = combat_metadata[pose - 1].box_groups[1];
                for (const auto& box : grab) {
                    const auto world_grab = skullgirls::foit_world_box(box, static_cast<int>(attacker.practice_x), 0,
                        attacker.throw_facing_right);
                    for (const auto& target : hurt) {
                        const auto world_hurt = skullgirls::foit_world_box(target, static_cast<int>(defender.practice_x),
                            0, !defender.mirror(practice_facing_right(1 - side)));
                        if (skullgirls::foit_boxes_overlap(world_grab, world_hurt)) {
                            attacker.throw_phase = Phase::capture;
                            attacker.throw_tick = 0;
                            defender.throw_phase = Phase::grabbed;
                            defender.throw_facing_right = !attacker.throw_facing_right;
                            defender.active_normal = defender.queued_followup = -1;
                            defender.movement_sprite = 828;
                            break;
                        }
                    }
                }
            }
            if (defender.throw_phase != Phase::grabbed) continue;
            if (attacker.throw_phase == Phase::capture) {
                // Target offsets written at frames 0, 4, 7 and 10 of move 122.
                const unsigned tick = attacker.throw_tick;
                const float separation = tick <= 17 ? 242.0f : tick <= 22 ? 170.0f : tick <= 27 ? 99.0f : 0.0f;
                defender.practice_x = std::clamp(attacker.practice_x + separation * forward, left, right);
            } else {
                const skullgirls::NormalHit release{0, 0, 0, 3};
                // Move 122 sets +8 horizontal and -26 vertical at release.
                defender.take_hit(release, !attacker.throw_facing_right, -1,
                    {skullgirls::HeavyReaction::knockback, 8, 26});
            }
        }
    }

    unsigned practice_idle_sprite() const {
        constexpr std::array<unsigned, 20> duration{
            5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4};
        auto tick = practice_stage_tick % 83;
        unsigned index = 0;
        while (tick >= duration[index]) tick -= duration[index++];
        return 2 + index;
    }

    bool training_grid_enabled() const {
        const auto grid = std::find_if(training_options.begin(), training_options.end(),
            [](const auto& option) { return option.keyword == "OPT_STR_TRAININGGRID"; });
        return grid != training_options.end() && !grid->choices.empty() &&
               grid->choices[grid->choice] == L"On";
    }

    void paint_practice(Gdiplus::Graphics& dc) const {
        const float view_width = 1280.0f + 320.0f * practice_camera_zoom;
        const float view_height = 720.0f + 180.0f * practice_camera_zoom;
        const float camera = std::clamp(practice_camera_center - view_width * 0.5f,
                                        0.0f, practice_width - view_width);
        if (practice_scene)
            practice_scene->draw(dc, camera + view_width * 0.5f, view_width, view_height,
                                 static_cast<float>(practice_camera_bottom), practice_stage_tick);
        const auto world = dc.Save();
        dc.ScaleTransform(1280.0f / view_width, 720.0f / view_height);
        if (practice_background)
            practice_background->draw(dc, -camera, view_height - practice_height + practice_camera_bottom,
                                      practice_width, practice_height);
        const float floor = view_height - practice_floor + practice_camera_bottom;
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
            const auto fighter = fighters[side].front();
            const auto palette = locked_colors[side].front();
            if (std::string_view(roster[fighter]) != "filia") {
                draw_idle(dc, fighter, palette, x, floor, mirror, shadow);
                return;
            }
            const unsigned sprite = practice_idle_sprite();
            draw_practice_frame(dc, side, sprite, x, floor, mirror, shadow);
        };
        const auto draw_players = [&](bool shadow) {
            for (int side : {1, 0}) {
                const auto& player = practice_players[side];
                const float x = player.practice_x - camera;
                const bool mirror = player.mirror(practice_facing_right(side));
                if (player.movement_sprite) {
                    draw_practice_frame(dc, side, player.movement_sprite, x,
                        shadow ? floor : floor - player.jump_height, mirror, shadow);
                } else {
                    draw_practice_idle(side, x, mirror, shadow);
                }
            }
        };
        draw_players(true);
        draw_players(false);
        dc.Restore(world);
        draw_text(dc, L"P1  Arrows   A S D / Z X C        P2  I J K L   V B N / F G H        R Reset",
                  {0, 675, 1280, 38}, 22, Gdiplus::StringAlignmentCenter);
        for (int side : {0, 1}) {
            if (practice_players[side].normal_contact)
                draw_text(dc, L"Hit", {side * 640.0f, 30, 640, 36}, 22, Gdiplus::StringAlignmentCenter);
        }
    }

    bool showing_stage_select() const {
        return training == TrainingView::stage_select || training == TrainingView::stage_confirmed;
    }

    void load_stages(const std::filesystem::path& directory) {
        const auto config = skullgirls::read_sprite_asset(directory / "core.gfs", "core/stages.ini");
        std::istringstream stream(std::string(config.begin(), config.end()));
        for (std::string line; std::getline(stream, line);) {
            line.erase(std::min(line.size(), line.find('#')));
            std::istringstream row(line);
            std::string id, level, picture;
            if (!(row >> id >> level >> picture)) continue;
            std::string parent;
            bool beta_only = false;
            for (std::string option; row >> option;) {
                if (option == "BetaOnly") beta_only = true;
                if (option.rfind("IsAlt>", 0) == 0) parent = option.substr(6);
            }
            if (beta_only) continue;
            auto preview = std::make_unique<Texture>();
            preview->load(skullgirls::read_sprite_asset(directory / "ui-win.gfs",
                "ui/Win/Textures/charselect/stages/" + picture + ".dds"));
            auto label = id;
            if (!label.empty() && label.front() == '_') label.erase(0, 1);
            std::replace(label.begin(), label.end(), '_', ' ');
            stages.push_back({option_label(label), std::move(id), std::move(parent), std::move(level), std::move(preview)});
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

    std::unique_ptr<Gdiplus::Bitmap> make_fighter_bitmap(const skullgirls::SelectFrame& frame,
                                                        int fighter, std::size_t palette) const {
        const auto rgba = skullgirls::colorize_sprite_with_palette(frame.pixels, palettes[fighter][palette].colors);
        auto bitmap = std::make_unique<Gdiplus::Bitmap>(frame.width, frame.height, PixelFormat32bppARGB);
        Gdiplus::BitmapData data{};
        Gdiplus::Rect bounds(0, 0, frame.width, frame.height);
        if (bitmap->LockBits(&bounds, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
            return {};
        for (unsigned row = 0; row < frame.height; ++row) {
            auto* target = reinterpret_cast<std::uint32_t*>(
                static_cast<BYTE*>(data.Scan0) + static_cast<std::ptrdiff_t>(row) * data.Stride);
            for (unsigned col = 0; col < frame.width; ++col) {
                const auto pixel = row * frame.width + col;
                const auto* color = rgba.data() + pixel * 4;
                target[col] = frame.pixels[pixel * 3] ?
                    (unsigned(color[3]) << 24) | (unsigned(color[0]) << 16) |
                    (unsigned(color[1]) << 8) | color[2] : 0;
            }
        }
        bitmap->UnlockBits(&data);
        return bitmap;
    }

    using PracticeBitmaps = std::vector<std::unique_ptr<Gdiplus::Bitmap>>;
    std::array<PracticeBitmaps, 2> practice_bitmaps;
    std::array<std::size_t, 2> practice_bitmap_side{};

    void prepare_practice_bitmaps() {
        decltype(practice_bitmaps) prepared;
        for (std::size_t side = 0; side < prepared.size(); ++side) {
            practice_bitmap_side[side] = side;
            const auto fighter = fighters[side].front();
            if (std::string_view(roster[fighter]) != "filia") continue;
            const auto palette = locked_colors[side].front();
            if (side == 1 && fighter == fighters[0].front() && palette == locked_colors[0].front()) {
                practice_bitmap_side[side] = 0;
                continue;
            }
            auto& sprites = prepared[side];
            sprites.resize(movement_frames.size());
            for (std::size_t i = 0; i < movement_frames.size(); ++i) {
                const auto& frame = movement_frames[i];
                if (frame.width && frame.height) {
                    sprites[i] = make_fighter_bitmap(frame, fighter, palette);
                    if (!sprites[i]) throw std::runtime_error("Cannot prepare training sprite bitmap");
                    const auto overlay = jab_overlays.find(static_cast<unsigned>(i));
                    if (overlay != jab_overlays.end()) {
                        auto effect = make_fighter_bitmap(overlay->second, fighter, palette);
                        if (!effect) throw std::runtime_error("Cannot prepare Filia jab overlay");
                        Gdiplus::Graphics composite(sprites[i].get());
                        if (composite.DrawImage(effect.get(), 0, 0, frame.width, frame.height) != Gdiplus::Ok)
                            throw std::runtime_error("Cannot composite Filia jab overlay");
                    }
                }
            }
        }
        practice_bitmaps = std::move(prepared);
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
                             float x, float y, bool mirror, bool shadow) const {
        const auto& bitmap = practice_bitmaps[practice_bitmap_side[side]][sprite];
        if (bitmap) draw_fighter_bitmap(dc, *bitmap, movement_frames[sprite], x, y, mirror, shadow);
    }

    void draw_fighter_bitmap(Gdiplus::Graphics& dc, Gdiplus::Bitmap& bitmap,
                             const skullgirls::SelectFrame& frame, float x, float y,
                             bool mirror, bool shadow) const {
        const auto state = dc.Save();
        dc.TranslateTransform(x, y);
        if (mirror) dc.ScaleTransform(-1, 1);
        const Gdiplus::RectF bounds(-frame.origin_x, -frame.origin_y, float(frame.width), float(frame.height));
        if (shadow) {
            // Project the current silhouette onto the floor and blend its alpha.
            dc.ScaleTransform(1.0f, -0.23f);
            Gdiplus::ColorMatrix silhouette{};
            silhouette.m[3][3] = 0.55f;
            silhouette.m[4][4] = 1.0f;
            Gdiplus::ImageAttributes attributes;
            attributes.SetColorMatrix(&silhouette);
            dc.DrawImage(&bitmap, bounds, 0, 0, float(frame.width),
                         float(frame.height), Gdiplus::UnitPixel, &attributes);
        } else {
            dc.DrawImage(&bitmap, bounds);
        }
        dc.Restore(state);
    }

    void start_character_select() {
        for (auto& team : fighters) team.clear();
        for (auto& colors : locked_colors) colors.clear();
        nameplate_states = {};
        choosing_palette = false;
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
                team.push_back(fighter_cursor);
                locked_colors[player].push_back(color);
                palette_cursor = 0;
                if (int(team.size()) == team_size[player]) {
                    if (player == 0) {
                        player = 1;
                        training = TrainingView::team_size;
                    } else {
                        training = TrainingView::stage_select;
                        stage_index = stage_variant = 0;
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
                if (horizontal) {
                    stage_index = horizontal < 0 ? (stage_index + stage_groups.size() - 1) % stage_groups.size()
                                                 : (stage_index + 1) % stage_groups.size();
                    stage_variant = 0;
                } else if (vertical) {
                    const auto count = stage_groups[stage_index].size();
                    stage_variant = vertical < 0 ? (stage_variant + count - 1) % count
                                                 : (stage_variant + 1) % count;
                } else if (accept || key == VK_RETURN) {
                    if (current_stage().level == "class_notes_3d" &&
                        std::string_view(roster[fighters[0].front()]) == "filia") {
                        start_practice();
                    } else {
                        training = TrainingView::stage_confirmed;
                    }
                }
            }
        } else if (cancel) {
            if (training == TrainingView::stage_confirmed) {
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
        game_data = directory;
        loading_animation.load(directory / "sprites.gfs");
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
        load_texture(stage_bar, "charselect/stageselect_frame01");
        load_texture(stage_arrow, "charselect/UI_option_arrow01");
        load_texture(stage_variant_arrow, "charselect/UI_option_arrow02");
        load_texture(stage_header, "charselect/SG_Team_Select_Size_Main_Header02");
        load_stages(directory);
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
        const auto charselect = skullgirls::read_sprite_asset(archive, "ui/Win/Charselect.gbs");
        pedestal_layout = skullgirls::read_pedestal_layout(charselect);
        const auto sprites_archive = directory / "sprites.gfs";
        const auto sprite_entries = skullgirls::read_archive_index(sprites_archive);
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
                    skullgirls::read_sprite_asset(sprites_archive, prefix + ".spr.msb"),
                    skullgirls::read_sprite_asset(sprites_archive, prefix + ".dds"));
                const auto palette_prefix = "sprites/" + std::string(roster[i]) + "_";
                for (const auto& entry : sprite_entries) {
                    const std::string_view name = entry.name;
                    if (name.substr(0, palette_prefix.size()) != palette_prefix ||
                        name.size() <= palette_prefix.size() + 5 || name.substr(name.size() - 5) != "p.dds")
                        continue;
                    const auto suffix = name.substr(palette_prefix.size(), name.size() - palette_prefix.size() - 5);
                    if (suffix.find_first_not_of("0123456789") != std::string_view::npos) continue;
                    palettes[i].push_back({static_cast<unsigned>(std::stoul(std::string(suffix))),
                        skullgirls::read_dds_palette(skullgirls::read_sprite_asset(sprites_archive, entry.name))});
                }
                auto& colors = palettes[i];
                std::sort(colors.begin(), colors.end(), [](const auto& a, const auto& b) { return a.number < b.number; });
                if (colors.empty()) throw std::runtime_error("Missing fighter palettes");
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
                if (line.rfind("Keyword: ", 0) == 0) option.keyword = line.substr(9);
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
        if (showing_stage_select()) {
            current_stage().preview->draw(dc, 0, 0, 1280, 720);
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
            // Convert the visible positions in the recording to the 1280x720 scene.
            stage_header.draw(dc, 546, 145, 188, 38);
            draw_text(dc, L"★ SELECT STAGE ★", {546, 151, 188, 30}, 22, Gdiplus::StringAlignmentCenter);
            stage_bar.draw(dc, 264, 625, 752, 64);
            stage_arrow.draw(dc, 890, 642, 20, 27);
            const auto state = dc.Save();
            dc.TranslateTransform(1280, 0);
            dc.ScaleTransform(-1, 1);
            stage_arrow.draw(dc, 890, 642, 20, 27);
            dc.Restore(state);
            if (stage_groups[stage_index].size() > 1) {
                for (int direction : {-1, 1}) {
                    const auto arrow = dc.Save();
                    dc.TranslateTransform(640, direction < 0 ? 624 : 687);
                    dc.RotateTransform(float(direction * 90));
                    stage_variant_arrow.draw(dc, -10, -13.5f, 20, 27);
                    dc.Restore(arrow);
                }
            }
            draw_text(dc, current_stage().name.c_str(), {392, 641, 496, 36},
                      26, Gdiplus::StringAlignmentCenter);
            if (training == TrainingView::stage_confirmed)
                draw_text(dc, L"Stage selected", {420, 690, 440, 28}, 21, Gdiplus::StringAlignmentCenter);
        }
        if (training == TrainingView::fighters) {
            (player ? select_cursor_p2 : select_cursor).draw(dc,
                521.5f + (fighter_cursor % 3) * 71, 90.0f + (fighter_cursor / 3) * 71, 96, 96);
        }

    }

    void paint(HDC buffer) const {
        Gdiplus::Graphics dc(buffer);
        if (practice_load.valid()) {
            const auto elapsed = std::chrono::duration<double>(Clock::now() - loading_started).count();
            loading_animation.draw(dc, static_cast<std::uint64_t>(elapsed * 60));
            return;
        }
        dc.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
        dc.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        if (training == TrainingView::practice) {
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
        if (menu.practice_load.valid()) return 0;
        if (!menu.title_visible) break;
        menu.leave_title();
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_KILLFOCUS:
        menu.practice_keys_held = {};
        menu.practice_keys_previous = {};
        for (auto& player : menu.practice_players) {
            player.practice_left = player.practice_right = false;
            player.practice_down = player.practice_up = player.practice_jump = false;
            player.normal_input = -1;
            player.queued_followup = -1;
            player.throw_input = false;
            player.pressed_punches = player.pressed_kicks = 0;
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
            menu.practice_key(wparam, true);
            if (wparam == 'R' && !repeated) menu.reset_practice();
            if (wparam == VK_ESCAPE) {
                menu.practice_bitmaps = {};
                menu.training = Menu::TrainingView::stage_select;
                SetWindowTextW(window, L"Skullgirls - Select Stage");
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
            const auto title = std::wstring(caption) + L" — " + menu.current_stage().name;
            SetWindowTextW(window, title.c_str());
        } else {
            SetWindowTextW(window, caption);
        }
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
            const auto elapsed_ticks = 1 + (now - deadline) / frame_period;
            deadline += frame_period * elapsed_ticks;
            if (menu.practice_load.valid() &&
                menu.practice_load.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                try {
                    menu.finish_practice(window);
                    SetWindowTextW(window, L"Skullgirls - Training Room");
                } catch (const std::exception& error) {
                    SetWindowTextW(window, L"Skullgirls - Select Stage");
                    MessageBoxA(window, error.what(), "Training Room", MB_OK | MB_ICONERROR);
                }
                deadline = Clock::now() + frame_period;
            } else if (menu.training == Menu::TrainingView::practice) {
                // Bound catch-up after a long stall without dropping ordinary late ticks.
                for (auto ticks = std::min<decltype(elapsed_ticks)>(elapsed_ticks, 8); ticks > 0; --ticks)
                    menu.practice_tick();
            }
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
