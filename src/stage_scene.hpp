#pragma once

#include "sprite_asset.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <cstring>
#include <cmath>

namespace skullgirls {

// Bounds-checked reader for the big-endian stage and tiled-animation formats.
class StageReader {
    const SpriteBytes& bytes_;
    std::size_t offset_ = 0;
public:
    explicit StageReader(const SpriteBytes& bytes) : bytes_(bytes) {}
    std::size_t remaining() const { return bytes_.size() - offset_; }
    void skip(std::size_t size) {
        if (size > remaining()) throw std::runtime_error("Truncated stage asset");
        offset_ += size;
    }
    std::size_t count(unsigned stride) {
        const auto size = word(8);
        if (size > remaining() / stride) throw std::runtime_error("Invalid stage array length");
        return static_cast<std::size_t>(size);
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

// Row-major affine UV transform, with translation in the last column.
using StageUv = std::array<float, 6>;
inline constexpr StageUv identity_stage_uv{1, 0, 0, 0, 1, 0};

struct StageUvClip {
    struct Sample { float sx, sy, rotation, u, v; };
    float seconds = 0;
    std::uint32_t discrete_frames = 0;
    std::vector<Sample> samples;

    static StageUv transform(const Sample& frame) {
        const float c = std::cos(frame.rotation), s = std::sin(frame.rotation);
        return {c * frame.sx, -s * frame.sy, frame.u, s * frame.sx, c * frame.sy, frame.v};
    }

    StageUv sample(double elapsed) const {
        double phase = std::clamp(elapsed / seconds, 0.0, 1.0);
        if (discrete_frames > 1)
            phase = std::min(1.0, std::floor(phase * discrete_frames) / (discrete_frames - 1));
        const double frame = phase * (samples.size() - 1);
        const auto i = static_cast<std::size_t>(frame);
        const auto& a = samples[i];
        const auto& b = samples[std::min(i + 1, samples.size() - 1)];
        const auto lerp = [t = static_cast<float>(frame - i)](float start, float end) {
            return start + t * (end - start);
        };
        return transform({lerp(a.sx, b.sx), lerp(a.sy, b.sy), lerp(a.rotation, b.rotation),
                          lerp(a.u, b.u), lerp(a.v, b.v)});
    }

    bool scrolls(unsigned axis) const {
        if (discrete_frames > 1 || samples.empty()) return false;
        const auto& first = samples.front();
        const auto& last = samples.back();
        const float span = axis == 0 ? last.u - first.u : last.v - first.v;
        return std::abs(span) >= 0.999f;
    }

    bool repeats_atlas(const std::vector<StageVertex>& vertices, unsigned axis) const {
        if (discrete_frames <= 1 || vertices.empty()) return false;
        // Some sprite frames move the entire mesh into a neighboring copy of the atlas.
        return std::any_of(samples.begin(), samples.end(), [&](const Sample& frame) {
            const auto uv = transform(frame);
            const auto row = axis * 3;
            const auto coordinate = [&](const StageVertex& vertex) {
                return uv[row] * vertex.u + uv[row + 1] * vertex.v + uv[row + 2];
            };
            float low = coordinate(vertices.front()), high = low;
            for (const auto& vertex : vertices) {
                const auto value = coordinate(vertex);
                low = std::min(low, value);
                high = std::max(high, value);
            }
            return (low >= 1.0f && high > 1.0f) || (high <= 0.0f && low < 0.0f);
        });
    }
};

struct StageMesh {
    std::string material;
    std::vector<StageVertex> vertices;
    std::vector<unsigned> indices;
    StageUvClip idle_uv, intro_uv;
    bool wrap_u = false, wrap_v = false;

    bool animates_uv() const { return !idle_uv.samples.empty() || !intro_uv.samples.empty(); }
    StageUv uv_at(double seconds) const {
        if (seconds < intro_uv.seconds) return intro_uv.sample(seconds);
        if (idle_uv.samples.empty())
            return intro_uv.samples.empty() ? identity_stage_uv : intro_uv.sample(intro_uv.seconds);
        return idle_uv.sample(std::fmod(seconds - intro_uv.seconds, idle_uv.seconds));
    }
};

inline StageUvClip read_stage_uv_clip(const SpriteBytes& bytes) {
    StageReader reader(bytes);
    if (reader.string() != "3.0") return {};
    StageUvClip clip;
    clip.discrete_frames = static_cast<std::uint32_t>(reader.word(4));
    const auto joints = reader.word(8);
    const auto uv_tracks = reader.word(8);
    clip.seconds = reader.real();
    if (!uv_tracks) return {};
    if (!std::isfinite(clip.seconds) || clip.seconds <= 0)
        throw std::runtime_error("Invalid stage UV clip duration");
    // A clip can animate both the skeleton and the texture.
    for (std::uint64_t i = 0; i < joints; ++i) {
        reader.string();
        for (unsigned stride : {12u, 16u, 12u})
            reader.skip(reader.count(stride) * stride);
    }
    for (std::uint64_t i = 0; i < uv_tracks; ++i) {
        const auto name = reader.string();
        const auto frames = reader.count(8);
        clip.samples.resize(frames);
        for (auto& frame : clip.samples) {
            frame.sx = reader.real();
            frame.sy = reader.real();
        }
        if (reader.count(4) != frames) throw std::runtime_error("Invalid stage rotation track");
        for (auto& frame : clip.samples) frame.rotation = reader.real();
        if (reader.count(8) != frames) throw std::runtime_error("Invalid stage translation track");
        for (auto& frame : clip.samples) {
            frame.u = reader.real();
            frame.v = reader.real();
            if (!std::isfinite(frame.u) || !std::isfinite(frame.v) ||
                !std::isfinite(frame.rotation) || !std::isfinite(frame.sx) || !std::isfinite(frame.sy))
                throw std::runtime_error("Invalid stage UV sample");
        }
        if (name == "uv_track" && !clip.samples.empty()) return clip;
    }
    return {};
}

inline StageMesh read_stage_mesh(const SpriteBytes& bytes, const std::array<float, 16>& transform) {
    StageReader reader(bytes);
    if (reader.string() != "2.0") throw std::runtime_error("Unsupported stage mesh version");
    StageMesh mesh;
    mesh.material = reader.string();
    reader.skip(52);
    const auto layout = reader.string();
    const auto stride = reader.word(8);
    const bool rigid = layout == "float p[3],n[3],uv[2]; uchar4 c;" && stride == 36;
    const bool skinned = layout == "float p[3],n[3],uv[2]; uchar4 c; uchar4 j[4],jw[4];" && stride == 44;
    if (!rigid && !skinned)
        throw std::runtime_error("Unsupported stage vertex layout");
    const auto vertices = reader.word(8);
    const auto triangles = reader.word(8);
    reader.skip(8);
    if (vertices > bytes.size() / stride || triangles > bytes.size() / 6)
        throw std::runtime_error("Invalid stage mesh size");
    for (std::uint64_t i = 0; i < vertices; ++i) {
        const float x = reader.real(), y = reader.real(), z = reader.real();
        reader.skip(12);
        mesh.vertices.push_back({
            x * transform[0] + y * transform[4] + z * transform[8] + transform[12],
            x * transform[1] + y * transform[5] + z * transform[9] + transform[13],
            x * transform[2] + y * transform[6] + z * transform[10] + transform[14],
            reader.real(), reader.real(), static_cast<unsigned>(reader.word(4))});
        if (skinned) reader.skip(8); // Draw skinned scenery in the bind pose.
    }
    for (std::uint64_t i = 0; i < triangles * 3; ++i) {
        const auto index = reader.word(2);
        if (index >= vertices) throw std::runtime_error("Invalid stage mesh index");
        mesh.indices.push_back(static_cast<unsigned>(index));
    }
    return mesh;
}

inline std::vector<StageMesh> read_stage_scene(SpriteArchive& assets,
                                              const std::string& level, float width,
                                              float height, float floor) {
    const auto prefix = "stages/" + level + "/";
    const auto bytes = assets.read(prefix + "background.sgi.msb");
    StageReader reader(bytes);
    const auto version = reader.string();
    if (version != "2.0" && version != "2.1") throw std::runtime_error("Unsupported stage scene version");
    const auto count = reader.word(8);
    std::vector<StageMesh> meshes;
    StageMesh plane;
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto node = reader.string(), shape = reader.string();
        std::array<float, 16> transform;
        for (auto& value : transform) value = reader.real();
        reader.skip(version == "2.1" ? 6 : 2);
        const auto tracks = reader.word(8);
        std::string idle, intro;
        for (std::uint64_t track = 0; track < tracks; ++track) {
            const auto event = reader.string(), clip = reader.string();
            if (event == "idle" || event == "Idle") idle = clip;
            if (event == "intro") intro = clip;
        }
        auto mesh = read_stage_mesh(assets.read(prefix + shape + ".sgm.msb"), transform);
        if (node == "FightPlane") {
            plane = std::move(mesh);
        } else if (!mesh.indices.empty()) {
            if (!idle.empty())
                mesh.idle_uv = read_stage_uv_clip(assets.read(prefix + idle + ".sga.msb"));
            if (!intro.empty())
                mesh.intro_uv = read_stage_uv_clip(assets.read(prefix + intro + ".sga.msb"));
            const auto wraps = [&](unsigned axis) {
                return mesh.idle_uv.scrolls(axis) || mesh.intro_uv.scrolls(axis) ||
                    mesh.idle_uv.repeats_atlas(mesh.vertices, axis) || mesh.intro_uv.repeats_atlas(mesh.vertices, axis);
            };
            mesh.wrap_u = wraps(0);
            mesh.wrap_v = wraps(1);
            meshes.push_back(std::move(mesh));
        }
    }
    // Use the FightPlane bounds to align the scenery with Training coordinates.
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

inline std::vector<StageMesh> read_stage_scene(const std::filesystem::path& archive,
                                              const std::string& level, float width,
                                              float height, float floor) {
    SpriteArchive assets(archive);
    return read_stage_scene(assets, level, width, height, floor);
}
}
