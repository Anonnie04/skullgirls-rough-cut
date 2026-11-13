#pragma once

#include "sprite_asset.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <cstring>
#include <cmath>

namespace skullgirls {

class StageReader {
    const SpriteBytes& bytes_;
    std::size_t offset_ = 0;
public:
    explicit StageReader(const SpriteBytes& bytes) : bytes_(bytes) {}
    void skip(std::size_t size) {
        if (size > bytes_.size() - offset_) throw std::runtime_error("Truncated stage asset");
        offset_ += size;
    }
    std::uint64_t word(unsigned size) {
        const auto start = offset_;
        skip(size);
        std::uint64_t value = 0;
        for (auto i = start; i < offset_; ++i) value = (value << 8) | bytes_[i];
        return value;
    }
    std::string string() {
        const auto size = word(8);
        if (size > bytes_.size() - offset_) throw std::runtime_error("Invalid stage string");
        const auto start = offset_;
        skip(static_cast<std::size_t>(size));
        return {bytes_.begin() + start, bytes_.begin() + offset_};
    }
    float real() {
        const auto bits = static_cast<std::uint32_t>(word(4));
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
};

struct StageVertex {
    float x, y, z, u, v;
    unsigned rgba;
};
struct StageMesh {
    std::string material;
    std::vector<StageVertex> vertices;
    std::vector<unsigned> indices;
    std::vector<std::array<float, 2>> uv_offsets;
    std::vector<std::array<float, 2>> intro_uv_offsets;
};

inline std::vector<std::array<float, 2>> read_stage_uv_offsets(const SpriteBytes& bytes) {
    StageReader reader(bytes);
    if (reader.string() != "3.0") throw std::runtime_error("Unsupported stage animation version");
    const auto frames = reader.word(4);
    reader.skip(16);
    reader.real();
    if (reader.string() != "uv_track")
        throw std::runtime_error("Unsupported stage UV track");
    if (!frames || frames > bytes.size() / 20 || reader.word(8) != frames)
        throw std::runtime_error("Invalid stage UV track length");
    // Two scale channels and a rotation channel precede the UV translations.
    reader.skip(static_cast<std::size_t>(frames) * 8);
    if (reader.word(8) != frames) throw std::runtime_error("Invalid stage rotation track");
    reader.skip(static_cast<std::size_t>(frames) * 4);
    if (reader.word(8) != frames) throw std::runtime_error("Invalid stage translation track");
    std::vector<std::array<float, 2>> offsets;
    for (std::uint64_t i = 0; i < frames; ++i) offsets.push_back({reader.real(), reader.real()});
    return offsets;
}

inline StageMesh read_stage_mesh(const SpriteBytes& bytes, const std::array<float, 16>& transform) {
    StageReader reader(bytes);
    if (reader.string() != "2.0") throw std::runtime_error("Unsupported stage mesh version");
    StageMesh mesh;
    mesh.material = reader.string();
    reader.skip(52);
    if (reader.string() != "float p[3],n[3],uv[2]; uchar4 c;" || reader.word(8) != 36)
        throw std::runtime_error("Unsupported stage vertex layout");
    const auto vertices = reader.word(8);
    const auto triangles = reader.word(8);
    reader.skip(8);
    if (vertices > bytes.size() / 36 || triangles > bytes.size() / 6)
        throw std::runtime_error("Invalid stage mesh size");
    for (std::uint64_t i = 0; i < vertices; ++i) {
        const float x = reader.real(), y = reader.real(), z = reader.real();
        reader.skip(12);
        mesh.vertices.push_back({
            x * transform[0] + y * transform[4] + z * transform[8] + transform[12],
            x * transform[1] + y * transform[5] + z * transform[9] + transform[13],
            x * transform[2] + y * transform[6] + z * transform[10] + transform[14],
            reader.real(), reader.real(), static_cast<unsigned>(reader.word(4))});
    }
    for (std::uint64_t i = 0; i < triangles * 3; ++i) {
        const auto index = reader.word(2);
        if (index >= vertices) throw std::runtime_error("Invalid stage mesh index");
        mesh.indices.push_back(static_cast<unsigned>(index));
    }
    return mesh;
}

inline std::vector<StageMesh> read_stage_scene(const std::filesystem::path& archive,
                                               const std::string& level, float width,
                                               float height, float floor) {
    const auto prefix = "stages/" + level + "/";
    const auto bytes = read_sprite_asset(archive, prefix + "background.sgi.msb");
    StageReader reader(bytes);
    if (reader.string() != "2.0") throw std::runtime_error("Unsupported stage scene version");
    const auto count = reader.word(8);
    std::vector<StageMesh> meshes;
    StageMesh plane;
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto node = reader.string(), shape = reader.string();
        std::array<float, 16> transform;
        for (auto& value : transform) value = reader.real();
        reader.skip(2);
        const auto tracks = reader.word(8);
        std::string idle, intro;
        for (std::uint64_t track = 0; track < tracks; ++track) {
            const auto event = reader.string(), clip = reader.string();
            if (event == "idle") idle = clip;
            if (event == "intro") intro = clip;
        }
        auto mesh = read_stage_mesh(read_sprite_asset(archive, prefix + shape + ".sgm.msb"), transform);
        if (node == "|class_msvictoria" && !idle.empty())
            mesh.uv_offsets = read_stage_uv_offsets(read_sprite_asset(archive, prefix + idle + ".sga.msb"));
        if (node == "|class_msvictoria" && !intro.empty())
            mesh.intro_uv_offsets = read_stage_uv_offsets(read_sprite_asset(archive, prefix + intro + ".sga.msb"));
        if (node == "FightPlane") plane = std::move(mesh);
        else meshes.push_back(std::move(mesh));
    }
    if (plane.vertices.empty()) throw std::runtime_error("Stage has no FightPlane");
    auto lo = plane.vertices.front(), hi = lo;
    for (const auto& v : plane.vertices) {
        lo.x = std::min(lo.x, v.x);
        lo.y = std::min(lo.y, v.y);
        lo.z = std::min(lo.z, v.z);
        hi.x = std::max(hi.x, v.x);
        hi.y = std::max(hi.y, v.y);
        hi.z = std::max(hi.z, v.z);
    }
    if (hi.x <= lo.x || hi.y <= lo.y) throw std::runtime_error("Invalid FightPlane bounds");
    const float sx = width / (hi.x - lo.x), sy = height / (hi.y - lo.y);
    for (auto& mesh : meshes) {
        for (auto& v : mesh.vertices) {
            v.x = (v.x - lo.x) * sx;
            v.y = (v.y - lo.y) * sy + floor;
            v.z = (v.z - (lo.z + hi.z) * 0.5f) * sx;
        }
    }
    return meshes;
}
}
