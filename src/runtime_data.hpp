#pragma once

#include <filesystem>
#include <stdexcept>

namespace skullgirls {

inline bool is_runtime_data(const std::filesystem::path& directory)
{
    for (const auto* name : {"core.gfs", "ui.gfs", "ui-win.gfs", "sprites.gfs",
                             "characters-foits.gfs", "characters-art.gfs", "stages.gfs",
                             "stages-textures.gfs", "stages-textures2d.gfs"})
        if (!std::filesystem::is_regular_file(directory / name)) return false;
    return true;
}

// Resolve from the executable so launch shortcuts and other working directories
// use the same portable assets. Steam's Linux depot uses data01; prefer data02
// when both depots are available. An explicit path can name either or its parent.
inline std::filesystem::path resolve_runtime_data(const std::filesystem::path& executable,
                                                 const std::filesystem::path& requested = {})
{
    const auto find = [](const std::filesystem::path& root) -> std::filesystem::path {
        for (const auto& directory : {root / "data02", root / "data01", root})
            if (is_runtime_data(directory)) return std::filesystem::absolute(directory).lexically_normal();
        return {};
    };
    if (!requested.empty()) {
        if (const auto found = find(requested); !found.empty()) return found;
        throw std::runtime_error("The selected folder does not contain the required data01/data02 archives.");
    }
    // Check the executable's directory first, then nearby development and runtime folders.
    auto directory = std::filesystem::absolute(executable).parent_path();
    for (unsigned depth = 0; depth < 4; ++depth) {
        if (const auto found = find(directory); !found.empty()) return found;
        if (const auto found = find(directory / "runtime"); !found.empty()) return found;
        const auto parent = directory.parent_path();
        if (parent == directory) break;
        directory = parent;
    }
    throw std::runtime_error("Local game data was not found. Prepare runtime with scripts/prepare-runtime.sh "
                             "(Linux) or scripts/prepare-runtime.ps1 (Windows), or pass a game data folder.");
}

}
