#include "../src/main_menu.cpp"
#include <iostream>
#include <stdexcept>

namespace {
using Phase = Menu::PracticePresentation::Phase;
void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
int character(std::string_view name) {
    const auto found = std::find(roster.begin(), roster.end(), name);
    check(found != roster.end(), "Unknown test character");
    return int(found - roster.begin());
}
void select(Menu& menu, std::string_view name, unsigned cycle) {
    menu.fighters[0] = menu.fighters[1] = {character(name)};
    menu.locked_colors[0] = menu.locked_colors[1] = {0};
    menu.reset_practice();
    menu.practice_intro_cycle = cycle;
    menu.start_practice_intro();
    check(menu.practice_presentation.phase == Phase::intro, "Intro starts: " + std::string(name));
}
const skullgirls::FoitEffectPlayback* child(const Menu& menu, std::size_t side, const skullgirls::FoitMoveTable& table) {
    for (const auto& effect : menu.practice_effects)
        if (effect.side == side && effect.table == &table && effect.alive) return &effect;
    return nullptr;
}
void ready(Menu& menu, std::string_view name) {
    for (unsigned side = 0; side < 2; ++side) {
        const auto& fighter = menu.practice_players[side];
        if (const auto* actor = fighter.scripted()) {
            check(actor->move == (name == "msfortune" || name == "painwheel" ? 21u : 20u), "Returns to playable idle: " + std::string(name));
            check(actor->visual.freeze == 0, "Intro leaves no freeze");
        }
        if (fighter.msfortune) {
            check(fighter.msfortune->value(0x8b) == 0, "Ms. Fortune starts headed on both sides");
            check(child(menu, side, menu.msfortune_assets.children[0]), "Combat creates the persistent attached head");
        }
    }
    for (const auto& effect : menu.practice_effects)
        check(!effect.authored_timeline, "Intro proxy does not survive the round handoff");
}
void variants_and_skips(Menu& menu, std::initializer_list<const char*> names = {"filia", "peacock", "msfortune", "cerebella", "parasoul"}) {
    for (auto name : names) {
        const unsigned variants = std::string_view(name) == "peacock" ? 4 : 3;
        for (unsigned cycle = 0; cycle < variants; ++cycle) {
            select(menu, name, cycle);
            unsigned ticks = 0;
            while (menu.practice_presentation.phase == Phase::intro && ticks++ < 600) menu.practice_tick();
            check(menu.practice_presentation.phase == Phase::none, "Native done signals complete: " + std::string(name));
            ready(menu, name);
            // These checkpoints include the hidden body, the proxy handoff,
            // and Ms. Fortune's partial reassembly, for every variant/side.
            for (unsigned skip : {0u, 20u, 60u, 110u, 170u}) {
                select(menu, name, cycle);
                for (unsigned i = 0; i < skip && menu.practice_presentation.phase == Phase::intro; ++i) menu.practice_tick();
                for (unsigned side = 0; side < 2; ++side) menu.practice_attack_edges[side] = 1;
                menu.practice_tick();
                check(menu.practice_presentation.phase == Phase::none, "Attack skips the intro");
                ready(menu, name);
                for (unsigned i = 0; i < 40; ++i) menu.practice_tick();
                ready(menu, name);
            }
        }
    }
}
void authored_events(Menu& menu) {
    // Filia's falling/tripping intro uses an invisible body and child 10.
    // A flat sprite timeline cannot reproduce this overlap.
    select(menu, "filia", 1);
    menu.practice_tick();
    check(menu.practice_presentation.actors[0].intro->value(0xa4) == 0, "Filia's trip hides the body");
    check(child(menu, 0, menu.filia_move_table.children[10]), "Filia's trip spawns its original proxy");
    for (unsigned i = 1; i < 50; ++i) menu.practice_tick();
    check(menu.practice_presentation.actors[0].intro->value(0xa4) == 1, "Filia becomes visible after the proxy handoff");
    select(menu, "filia", 2);
    bool hat = false;
    for (unsigned i = 0; i < 140; ++i) {
        menu.practice_tick();
        hat |= child(menu, 0, menu.filia_move_table.children[11]) != nullptr;
    }
    check(hat, "Filia's bow spawns the authored hat child");

    select(menu, "peacock", 3);
    menu.practice_tick();
    check(menu.practice_presentation.actors[0].intro->value(0xa4) == 0, "Peacock's car hides the body");
    check(child(menu, 0, menu.peacock_assets.children[65]), "Peacock's car proxy survives outside an attack");
    for (unsigned i = 1; i < 207; ++i) menu.practice_tick();
    check(menu.practice_presentation.phase == Phase::intro, "Peacock waits for her authored car sequence");

    // The spawn script executes on the child (timer 105), while 0x91
    // addresses its spawner (timer 88). Check both, before reassembly.
    select(menu, "msfortune", 1);
    menu.practice_tick();
    auto& pieces = *menu.practice_presentation.actors[0].intro;
    const auto* proxy = child(menu, 0, menu.msfortune_assets.children[40]);
    check(proxy && proxy->value(0x81) == 104 && pieces.value(0x81) == 88, "Pieces proxy and body have distinct native timers");
    check(pieces.value(0x8b) == 1, "Pieces intro begins headless");
    for (unsigned i = 1; i < 200; ++i) menu.practice_tick();
    check(menu.practice_presentation.phase == Phase::intro, "Pieces reassembles before the other intro finishes");
    check(pieces.value(0x8b) == 0 && pieces.value(0xa4) == 1, "The script restores the head and visibility before gameplay");

    select(menu, "msfortune", 2);
    for (unsigned i = 0; i < 25; ++i) menu.practice_tick();
    check(menu.practice_presentation.actors[0].intro->value(0xa4) == 0 &&
        child(menu, 0, menu.msfortune_assets.children[39]), "Nyawn hides the body while its proxy animates");
    for (unsigned i = 25; i < 120; ++i) menu.practice_tick();
    check(menu.practice_presentation.actors[0].intro->value(0xa4) == 1, "Move changes restore visible idle after Nyawn");

    // Save/restore must preserve the live wait and stable spawn references.
    select(menu, "msfortune", 1);
    for (unsigned i = 0; i < 35; ++i) menu.practice_tick();
    const auto timer = menu.practice_presentation.actors[0].intro->value(0x81);
    menu.save_or_restore_practice(false);
    for (unsigned i = 0; i < 35; ++i) menu.practice_tick();
    menu.save_or_restore_practice(true);
    check(menu.practice_presentation.actors[0].intro->value(0x81) == timer, "Practice save restores the live intro clock");
    for (unsigned i = 0; i < 500 && menu.practice_presentation.phase == Phase::intro; ++i) menu.practice_tick();
    ready(menu, "msfortune");
}
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    try {
        { Menu menu;
        menu.game_data = argv[1];
        menu.load_movement_frames(); menu.load_peacock(); menu.load_msfortune(); menu.load_cerebella(); menu.load_parasoul();
        menu.practice_width = 1700; menu.practice_height = 900; menu.practice_floor = 145;
        menu.practice_starts = {700, 1000};
        authored_events(menu);
        variants_and_skips(menu); }
        { Menu menu; menu.game_data=argv[1];menu.load_movement_frames();menu.load_painwheel();
          menu.practice_width=1700;menu.practice_height=900;menu.practice_floor=145;menu.practice_starts={700,1000};
          variants_and_skips(menu,{"painwheel"}); }
        std::cout << "All six characters' intro variants, child events, visibility, head restoration, skips, and saves passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
