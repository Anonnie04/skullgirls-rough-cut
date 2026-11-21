#include "stage_scene.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void check(bool passed, const char* message)
{
    if (!passed) throw std::runtime_error(message);
}

}

int main()
{
    using skullgirls::StageUvClip;
    using skullgirls::StageVertex;
    try {
        const std::vector<StageVertex> vertices{
            {0, 0, 0, 0.1f, 0.855f, 0xffffffff},
            {1, 0, 0, 0.2f, 0.855f, 0xffffffff},
            {0, 1, 0, 0.1f, 1.0f, 0xffffffff},
        };
        const StageUvClip neighboring_row{1, 2, {{1, 1, 0, 0, 0}, {1, 1, 0, 0, 0.855f}}};
        check(neighboring_row.repeats_atlas(vertices, 1), "Frames above the atlas must wrap vertically");
        check(!neighboring_row.repeats_atlas(vertices, 0), "Vertical atlas animation leaves U clamped");
        check(!neighboring_row.scrolls(1), "Frame changes are distinct from continuous scrolling");

        const StageUvClip neighboring_column{1, 2, {{1, 1, 0, 0, 0}, {1, 1, 0, -0.5f, 0}}};
        check(neighboring_column.repeats_atlas(vertices, 0), "Frames left of the atlas must wrap horizontally");
        check(!neighboring_column.repeats_atlas(vertices, 1), "A vertex on the atlas edge keeps V clamped");

        const StageUvClip same_atlas{1, 2, {{1, 1, 0, 0, 0}, {1, 1, 0, 0.4f, -0.855f}}};
        check(!same_atlas.repeats_atlas(vertices, 0) && !same_atlas.repeats_atlas(vertices, 1),
              "Animation within one atlas keeps both axes clamped");

        const StageUvClip scroll{1, 0, {{1, 1, 0, 0, 0}, {1, 1, 0, 1, 0}}};
        check(scroll.scrolls(0) && !scroll.scrolls(1), "Continuous one-tile motion still scrolls");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
