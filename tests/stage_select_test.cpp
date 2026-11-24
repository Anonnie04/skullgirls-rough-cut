#include "stage_select.hpp"
#include <iostream>
#include <set>

int main() {
    try {
        const auto check = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
        const std::vector<std::vector<std::size_t>> groups{{0}, {1, 2, 3}, {4}};
        // Stage 2 is an excluded alternate; 4 is an excluded standalone stage.
        const std::vector<std::size_t> eligible{0, 1, 3};
        skullgirls::StageSelectState selection;
        std::mt19937 generator(42);
        check(selection.random() && !selection.preview(groups), "Random is the default, without a fixed preview");
        check(!selection.vertical(1, groups), "Random has no alternates");
        selection.horizontal(-1, groups);
        check(selection.selected(groups) == 4, "Left wraps from Random to the final map");
        selection.horizontal(1, groups);
        check(selection.random(), "Right wraps back to Random");
        selection.horizontal(1, groups);
        check(selection.selected(groups) == 0, "Right from Random selects the first map");
        selection.horizontal(1, groups);
        selection.vertical(-1, groups);
        check(selection.selected(groups) == 3, "Up wraps through alternate maps");
        selection.vertical(1, groups);
        selection.vertical(1, groups);
        check(selection.confirm(groups, eligible, generator) == 2, "Excluded random maps remain manually selectable");
        selection.horizontal(1, groups);
        check(selection.variant == 0 && !selection.resolved, "Changing groups resets the alternate and confirmed choice");
        std::set<std::size_t> seen;
        for (unsigned i = 0; i < 200; ++i) {
            selection.reset();
            const auto index = selection.confirm(groups, eligible, generator);
            check(std::find(eligible.begin(), eligible.end(), index) != eligible.end(), "Random obeys its eligible pool");
            check(selection.confirm(groups, eligible, generator) == index, "Confirming/loading again does not reroll the map");
            seen.insert(index);
        }
        check(seen.size() == eligible.size(), "All eligible maps can be selected");
        selection.reset();
        bool rejected = false;
        try { selection.confirm(groups, {}, generator); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "An empty random pool reports an error");
        skullgirls::StageSelectLayout layout;
        layout.entrance = {{0, 300}, {11, 20}, {17, -40}};
        check(layout.entrance_offset(0) == 340 && layout.entrance_offset(1) == 0,
              "The original entrance ends at the authored resting position");
        const auto localized = skullgirls::stage_localized_name(
            R"({"New_Meridian":"Streets of New Meridian","Unicode":"Caf\u00e9 \ud83c\udf03"})", "New_Meridian");
        check(localized && *localized == "Streets of New Meridian", "Use original display names rather than stage IDs");
        const auto unicode = skullgirls::stage_localized_name(R"({"Unicode":"Caf\u00e9 \ud83c\udf03"})", "Unicode");
        check(unicode && *unicode == "Caf\xc3\xa9 \xf0\x9f\x8c\x83", "Decode escaped Unicode stage names");
        check(!skullgirls::stage_localized_name("{}", "Absent"), "Missing translations permit a readable fallback");
        std::cout << "Stage Random default, wrapping, alternates, exclusions, stable confirmation and entrance passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
