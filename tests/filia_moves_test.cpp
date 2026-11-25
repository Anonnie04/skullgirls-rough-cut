// Training's fighter and input handling are internal to the Windows preview.
#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"

#include <cstring>
#include <initializer_list>
#include <iostream>
#include <numeric>

namespace {

using namespace skullgirls;
using Fighter = Menu::PracticeFighter;

void check(bool passed, const std::string& description)
{
    if (!passed) throw std::runtime_error(description);
}

void verify_timelines(const FoitAnimationAsset& asset)
{
    const auto records = [](const FoitMoveTable& table) {
        return std::accumulate(table.animations.begin(), table.animations.end(), std::size_t{0},
            [](std::size_t total, const auto& clip) { return total + clip.size(); });
    };
    check(asset.animations.size() == 529 && records(asset) == 2797, "Filia main move table");
    check(asset.children.size() == 13, "Filia child tables");
    std::size_t child_moves = 0, child_records = 0, links = 0;
    for (const auto& child : asset.children) {
        child_moves += child.animations.size();
        child_records += records(child);
    }
    for (const auto& transitions : asset.transitions) links += transitions.size();
    check(child_moves == 22 && child_records == 180, "Filia child move records");
    check(links == 10105, "Filia conditional transition count");

    const auto compare = [&](const auto& sequence, std::initializer_list<unsigned> source, std::size_t index = 0) {
        for (auto move : source) {
            for (const auto& frame : asset.animations.at(move)) {
                check(index < std::size(sequence) && sequence[index].sprite == frame.sprites[1] &&
                    sequence[index].ticks == frame.ticks, asset.names.at(move) + " sprite or duration");
                ++index;
            }
        }
        check(index == std::size(sequence) || !sequence[index].ticks, "Unexpected extra animation records");
    };
    for (unsigned normal = 0; normal < std::size(filia_foit_normals); ++normal)
        compare(filia_normals[normal], {filia_foit_normals[normal]});
    for (unsigned strength = 0; strength < 3; ++strength) {
        compare(filia_normals[updo_light + strength], {161 + strength, 164, 165, 166, 167, 168});
        compare(filia_normals[ground_hairball], {148 + strength, 154, 156});
        compare(filia_normals[air_hairball], {151 + strength, 155, 157});
    }
    const unsigned other_moves[] = {
        147, 146, 170, 191, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181,
        182, 183, 184, 185, 186, 187, 188, 189, 190, 196,
    };
    for (unsigned i = 0; i < std::size(other_moves); ++i) {
        if (ringlet_psych + i == fenrir_fall)
            compare(filia_normals[updo_light], {185, 167, 168}, fenrir_fall_start);
        else
            compare(filia_normals[ringlet_psych + i], {other_moves[i]});
    }
    check(asset.successors[185] == 167 && asset.successors[167] == 167 && asset.successors[168] == 20,
        "Fenrir whiff fall and landing successors");
}

void verify_movement_defaults(const std::filesystem::path& directory)
{
    const auto foit = SpriteArchive(directory / "characters-foits.gfs").read("characters/filia.foit");
    // FUN_004927d0 loads 177 initial slots after Filia's palette and metadata.
    constexpr std::size_t initial_slots = 0x55ce;
    const auto slot = [&](std::size_t index) {
        const auto offset = initial_slots + index * sizeof(float);
        check(foit.size() >= offset + sizeof(float), "Filia initial slot is present");
        float value;
        std::memcpy(&value, foit.data() + offset, sizeof(value));
        return value;
    };
    check(slot(0x36) == filia_gravity, "Filia gravity matches FOIT");
    check(slot(0x7c) == 0.0f && slot(0x7d) == 1.0f, "Filia has one air dash and zero air jumps");
    check(slot(0x80) == filia_air_dash_lockout, "Filia's takeoff dash lockout matches FOIT");
}

void verify_ringlet(const FoitAnimation& original)
{
    using Spin = Menu::RingletSpike::Spin;
    check(original.size() == std::size(ringlet_spike_frames), "Ringlet child record count");
    for (std::size_t i = 0; i < original.size(); ++i) {
        check(original[i].sprites[1] == ringlet_spike_frames[i].sprite &&
            original[i].ticks == ringlet_spike_frames[i].ticks, "Ringlet child sprite and stored duration");
    }
    for (const auto spin : {Spin::none, Spin::standard, Spin::otg}) {
        const FoitInputHistory inputs{};
        FoitScriptState state;
        state.bits[1] = state.bits[2] = false;
        std::vector<unsigned> expected;
        for (unsigned i = 0; i < original.size(); ++i) {
            const auto& record = original[i];
            if (i == ringlet_spike_clear_frame) {
                state.bits[1] = spin == Spin::otg;
                state.bits[2] = spin == Spin::standard;
            }
            state.variables[0x3a] = 0.0f;
            for (const auto& assignment : record.script) apply_foit_assignment(assignment, state);
            for (const auto& variant : record.variants) {
                if (!match_foit_condition(variant.condition, inputs, true, state)) continue;
                for (const auto& assignment : variant.script) apply_foit_assignment(assignment, state);
            }
            if (state.variables[0x3a] == 0.0f)
                expected.insert(expected.end(), record.ticks, record.sprites[1]);
        }

        Menu::RingletSpike child{0, true, 1, 0};
        child.spin = spin;
        std::vector<unsigned> actual;
        do {
            actual.push_back(child.sprite());
            check(child.locks_owner() == (child.sprite() < 1178), "Ringlet lock clears before the dissolve ends");
        } while (child.advance());
        check(actual == expected, "Ringlet playback follows the archived spin conditions");
    }
}

void verify_commands(const FoitMoveTable& asset, bool facing_right)
{
    const char* facing = facing_right ? " facing right" : " facing left";
    const auto ready = [&](bool airborne, unsigned meter = 5) {
        Fighter fighter;
        fighter.practice_x = 640;
        fighter.jump_height = airborne ? 450.0f : 0.0f;
        fighter.jump_facing_right = facing_right;
        fighter.tension = meter;
        return fighter;
    };
    const auto tick = [&](Fighter& fighter, unsigned direction = 8, unsigned buttons = 0) {
        constexpr unsigned mirror[] = {4, 3, 2, 1, 0, 7, 6, 5, 8};
        if (!facing_right) direction = mirror[direction];
        fighter.practice_left = direction >= 3 && direction <= 5;
        fighter.practice_right = direction == 0 || direction == 1 || direction == 7;
        fighter.practice_down = direction >= 1 && direction <= 3;
        fighter.practice_up = direction >= 5 && direction <= 7;
        fighter.pressed_punches = buttons & 7u;
        fighter.pressed_kicks = buttons >> 3;
        fighter.held_attack_buttons = buttons;
        fighter.tick(facing_right ? 1100.0f : 200.0f, 0, 1700, facing_right, 650, &asset);
    };
    const auto motion = [&](Fighter& fighter, std::initializer_list<unsigned> directions, unsigned buttons) {
        for (auto direction = directions.begin(); direction != directions.end(); ++direction)
            tick(fighter, *direction, direction + 1 == directions.end() ? buttons : 0);
    };
    const auto enter = [&](const Fighter& fighter, int expected, const std::string& move) {
        check(fighter.active_normal == expected && fighter.normal_facing_right == facing_right, move + facing);
    };
    const auto finish = [&](Fighter& fighter, const std::string& move) {
        for (unsigned i = 0; i < 400 && fighter.active_normal >= 0; ++i) tick(fighter);
        check(fighter.active_normal < 0, move + " recovery" + facing);
    };

    for (int stance = 0; stance < 3; ++stance) {
        for (int button = 0; button < 6; ++button) {
            auto fighter = ready(stance == 2);
            tick(fighter, stance == 1 ? 2 : 8, 1u << button);
            const int move = stance * 6 + button;
            const auto& name = asset.names.at(filia_foit_normals[move]);
            enter(fighter, move, name);
            finish(fighter, name);
        }
    }
    for (bool crouching : {false, true}) {
        auto fighter = ready(false);
        const unsigned direction = crouching ? 2 : 8;
        tick(fighter, direction, 1);
        for (unsigned i = 0; i < 70 && !fighter.script_state.bits[0].value_or(false); ++i)
            tick(fighter, direction);
        tick(fighter, direction, 1);
        enter(fighter, crouching ? rapid_crouch_lp : rapid_stand_lp, "Rapid-fire jab");
        finish(fighter, "Rapid-fire jab");
    }
    struct Throw { bool airborne; unsigned direction; bool forward; };
    const Throw throws[] = {{false, 8, true}, {false, 4, false}, {true, 8, false}};
    for (const auto& command : throws) {
        auto fighter = ready(command.airborne);
        tick(fighter, command.direction, 9);
        check(fighter.throw_phase == Fighter::ThrowPhase::start && fighter.aerial_throw == command.airborne &&
            fighter.forward_ground_throw == command.forward, std::string("Throw direction and stance") + facing);
        for (unsigned i = 0; i < 150 && fighter.throw_phase != Fighter::ThrowPhase::none; ++i) tick(fighter);
        check(fighter.throw_phase == Fighter::ThrowPhase::none, std::string("Throw whiff recovery") + facing);
    }
    for (unsigned strength = 0; strength < 3; ++strength) {
        const unsigned punch = 1u << strength, kick = 8u << strength;
        for (bool half_circle : {false, true}) {
            auto fighter = ready(false);
            if (half_circle) motion(fighter, {4, 3, 2, 1, 0}, punch);
            else motion(fighter, {2, 1, 0}, punch);
            enter(fighter, ringlet_spike, "Ringlet Spike");
            check(fighter.ringlet_strength == strength, std::string("Ringlet Spike distance") + facing);
            for (unsigned i = 0; i < 100 && fighter.ringlet_spikes.empty(); ++i) tick(fighter);
            check(!fighter.ringlet_spikes.empty(), std::string("Ringlet Spike spawns its child") + facing);
            finish(fighter, "Ringlet Spike");
        }
        auto fighter = ready(false);
        motion(fighter, {2, 1, 0}, kick);
        enter(fighter, ringlet_psych, "Ringlet Psych");
        finish(fighter, "Ringlet Psych");
        fighter = ready(false);
        motion(fighter, {0, 2, 1}, punch);
        enter(fighter, updo_light + strength, "Updo");
        finish(fighter, "Updo");
        for (bool airborne : {false, true}) {
            fighter = ready(airborne);
            motion(fighter, {2, 3, 4}, kick);
            enter(fighter, airborne ? air_hairball : ground_hairball, airborne ? "Airball" : "Hairball");
            finish(fighter, airborne ? "Airball" : "Hairball");
        }
    }
    const auto launch = [&](bool super) {
        auto fighter = ready(false);
        if (super) tick(fighter, 2);
        for (unsigned i = 0; i < 4; ++i) tick(fighter, 6);
        check(fighter.jump_velocity == (super ? 39.0f : 32.0f) && fighter.super_jump == super,
            std::string("Ground jump launch") + facing);
        return fighter;
    };
    for (bool super : {false, true}) {
        auto fighter = launch(super);
        float height = 0, velocity = super ? 39.0f : 32.0f;
        for (unsigned i = 0; i < 80 && !fighter.landing; ++i) {
            height = std::max(0.0f, height + velocity);
            velocity = height > 0 ? velocity - filia_gravity : 0.0f;
            // A fresh Up after Down used to start a double jump during this trace.
            tick(fighter, i == 4 ? 2 : i == 5 ? 6 : 8);
            check(fighter.jump_height == height && fighter.jump_velocity == velocity,
                std::string("Jump trajectory and air-jump allowance") + facing);
        }
        check(fighter.landing && !fighter.jump_height, std::string("Ground jump lands") + facing);
    }
    auto early_dash = launch(false);
    tick(early_dash, 8, 3);
    check(early_dash.pending_air_dash == 1 && !early_dash.air_dash_direction,
        std::string("Air dash queues during takeoff lockout") + facing);
    for (unsigned i = 1; i < filia_air_dash_lockout; ++i) tick(early_dash);
    check(!early_dash.air_dash_direction, "Queued air dash waits for the takeoff lockout");
    tick(early_dash);
    check(early_dash.air_dash_direction == 1, "Queued air dash starts when the takeoff lockout expires");

    for (bool airborne : {false, true}) {
        for (bool forward : {false, true}) {
            auto fighter = ready(airborne);
            const unsigned direction = forward ? 0 : 4;
            motion(fighter, {direction, 8, direction}, 0);
            const int dash = airborne ? fighter.air_dash_direction : fighter.dash_direction;
            check(dash == (forward ? 1 : -1), std::string("Directional dash") + facing);
        }
    }
    for (bool double_tap : {false, true}) {
        for (unsigned contacts : {0u, 1u, 2u}) {
            auto fighter = ready(true);
            motion(fighter, {2, 3, 4}, 8);
            for (unsigned i = 0; i < 120 && fighter.active_normal == air_hairball &&
                 fighter.normal_frame < hairball_end_frame; ++i) tick(fighter);
            fighter.airball_hits = contacts == 1;
            fighter.airball_blocks = contacts == 2;
            if (double_tap) motion(fighter, {0, 8, 0}, 0);
            else tick(fighter, 8, 3);
            check(fighter.airball_dash_cancelled && fighter.air_dash_direction == 1,
                std::string(double_tap ? "Airball double-tap cancel" : "Airball two-punch cancel") + facing);
            check(fighter.script_state.bits[64] == (contacts != 0), "Airball cancel contact flag");
        }
    }
    auto spent_dash = ready(true);
    motion(spent_dash, {2, 3, 4}, 8);
    for (unsigned i = 0; i < 120 && spent_dash.active_normal == air_hairball &&
         spent_dash.normal_frame < hairball_end_frame; ++i) tick(spent_dash);
    spent_dash.air_dash_used = true;
    tick(spent_dash, 8, 3);
    check(!spent_dash.airball_dash_cancelled && spent_dash.active_normal == air_hairball,
        std::string("Airball cancel needs a remaining air dash") + facing);
    auto low_airball = ready(true);
    low_airball.jump_height = 25.0f;
    motion(low_airball, {2, 3, 4}, 32);
    enter(low_airball, air_hairball, "Low heavy Airball");
    bool saw_landed_airball = false, saw_airball_ending = false;
    for (unsigned i = 0; i < 140 && low_airball.active_normal >= 0; ++i) {
        tick(low_airball);
        saw_landed_airball |= low_airball.active_normal == air_hairball && !low_airball.jump_height;
        saw_airball_ending |= low_airball.sprite() == 506;
        check(low_airball.active_normal != ground_hairball, "Airball keeps its aerial sequence on floor contact");
    }
    check(saw_landed_airball && saw_airball_ending && low_airball.landing,
        std::string("Low Airball completes its ending before landing recovery") + facing);

    struct Blockbuster {
        std::initializer_list<unsigned> directions;
        unsigned buttons, cost;
        bool airborne;
        int move;
        const char* name;
    };
    const Blockbuster blockbusters[] = {
        {{0, 2, 1}, 3, 1, false, fenrir_start, "Fenrir Drive"},
        {{2, 3, 4}, 24, 1, false, gregor_start, "Gregor Samson"},
        {{2, 3, 4}, 24, 1, true, gregor_air_start, "Aerial Gregor Samson"},
        {{2, 3, 4}, 3, 3, false, tricobezoar, "Tricobezoar"},
    };
    for (const auto& command : blockbusters) {
        auto fighter = ready(command.airborne, command.cost - 1);
        motion(fighter, command.directions, command.buttons);
        check(fighter.active_normal != command.move && fighter.tension == command.cost - 1,
            std::string(command.name) + " meter gate" + facing);
        fighter = ready(command.airborne, command.cost);
        motion(fighter, command.directions, command.buttons);
        enter(fighter, command.move, command.name);
        check(fighter.tension == 0, std::string(command.name) + " meter spend" + facing);
        if (command.move != fenrir_start) {
            finish(fighter, command.name);
            continue;
        }

        std::vector<unsigned> expect_landing, landing;
        for (const auto& frame : asset.animations.at(168)) {
            if (frame.sprites[1]) expect_landing.insert(expect_landing.end(), frame.ticks, frame.sprites[1]);
        }
        for (unsigned i = 0; i < 400 && fighter.active_normal >= 0; ++i) {
            tick(fighter);
            // The live evaluator presents the last fall pose on floor contact;
            // compare the complete authored landing once its link has executed.
            if (fighter.striking_normal == fenrir_fall && !fighter.jump_height &&
                (!fighter.blockbuster_playback || fighter.blockbuster_playback->move==168))
                landing.push_back(fighter.sprite());
        }
        check(landing == expect_landing && fighter.active_normal < 0 && !fighter.landing,
            std::string("Fenrir whiff plays the original landing recovery") + facing);

        // Starting high enough keeps the fall loop visible before ground contact.
        fighter = ready(true, 0);
        fighter.enter_blockbuster_phase(fenrir_fall);
        std::vector<unsigned> expect_loop, descent;
        for (const auto& frame : asset.animations.at(167)) {
            if (frame.sprites[1]) expect_loop.insert(expect_loop.end(), frame.ticks, frame.sprites[1]);
        }
        for (unsigned i = 0; i < 35 && fighter.jump_height > 0; ++i) {
            tick(fighter);
            if (fighter.sprite() >= 527 && fighter.sprite() <= 530) descent.push_back(fighter.sprite());
        }
        check(descent.size() >= expect_loop.size() && std::equal(expect_loop.begin(), expect_loop.end(), descent.begin()),
            std::string("Fenrir whiff plays the original fall loop") + facing);
    }

    auto fighter = ready(false);
    tick(fighter, 8, 1); tick(fighter, 8, 2); tick(fighter, 0);
    tick(fighter, 8, 16); tick(fighter); tick(fighter, 8, 16);
    enter(fighter, filia_taunt, "Born With It");
    finish(fighter, "Born With It");
    for (unsigned buttons : {18u, 36u}) {
        fighter = ready(false, 1);
        motion(fighter, {2, 1, 0}, buttons);
        enter(fighter, filia_snapback, "Snapback");
        check(fighter.snapback_reserve == (buttons == 36) && fighter.tension == 0,
            std::string("Snapback reserve and meter") + facing);
        finish(fighter, "Snapback");
    }
}

void verify_fenrir_contact(Menu& menu)
{
    menu.training_options.push_back({L"Block", {}, 0, "OPT_STR_BLOCKING"});
    for (const bool block : {false, true}) {
        menu.training_options.front().choice = block ? 2 : 0;
        menu.reset_practice();
        menu.practice_key(VK_RIGHT, true);
        menu.practice_tick();
        menu.practice_key(VK_RIGHT, false);
        menu.practice_key(VK_DOWN, true);
        menu.practice_tick();
        for (const WPARAM key : {WPARAM(VK_RIGHT), WPARAM('A'), WPARAM('S')}) menu.practice_key(key, true);
        menu.practice_tick();
        for (const WPARAM key : {WPARAM(VK_RIGHT), WPARAM(VK_DOWN), WPARAM('A'), WPARAM('S')})
            menu.practice_key(key, false);
        bool captured = false, guarded = false, whiff_recovery = false;
        for (unsigned i = 0; i < 250; ++i) {
            menu.practice_tick();
            const auto& attacker = menu.practice_players[0];
            const auto& defender = menu.practice_players[1];
            captured |= defender.fenrir_captured;
            guarded |= defender.blockstun != 0;
            whiff_recovery |= attacker.striking_normal == fenrir_fall && attacker.sprite() == 542;
        }
        check(captured == !block && guarded == block && whiff_recovery == block,
            block ? "Blocked Fenrir uses the whiff recovery" : "Connecting Fenrir uses the capture and release");
        check(!menu.practice_players[1].fenrir_captured, "Fenrir releases the defender");
    }
    menu.training_options.pop_back();
}

void verify_stunt_double(Menu& menu, int filia)
{
    menu.fighters[0] = {filia, filia, filia};
    for (const bool falling : {false, true}) {
        menu.reset_practice();
        auto& fighter = menu.practice_players[0];
        fighter.hurt_group = air_mid;
        fighter.burst_available = true;
        fighter.jump_height = falling ? 240.0f : 0.0f;
        const auto meter = fighter.tension;
        menu.practice_key('A', true);
        menu.practice_tick();
        menu.practice_key('A', false);
        check(fighter.burst_ticks != 0, "Training starts the burst before Stunt Double");
        if (falling) {
            for (unsigned i = 0; i < 150 && !fighter.burst_fall_tick; ++i) menu.practice_tick();
            check(fighter.burst_fall_tick != 0, "Training enters the airborne burst fall");
        }
        const std::array<WPARAM, 3> keys{VK_RIGHT, WPARAM(falling ? 'D' : 'S'), WPARAM(falling ? 'C' : 'X')};
        for (auto key : keys) menu.practice_key(key, true);
        menu.practice_tick();
        for (auto key : keys) menu.practice_key(key, false);
        const auto& team = menu.practice_teams[0];
        const std::size_t expected_slot = falling ? 2 : 1;
        check(team.tag && team.tag->from_burst && team.tag->incoming.slot == expected_slot,
            "Stunt Double cancels burst and selects the requested teammate");
        check(!fighter.burst_ticks && !fighter.burst_fall_tick, "Outgoing fighter stops the burst on Stunt Double");
        const auto assist = menu.practice_assist_move(0, expected_slot);
        for (unsigned i = 0; i < 120 && team.tag; ++i) menu.practice_tick();
        check(team.active_slot == expected_slot && fighter.tension == meter - 1 && fighter.active_normal == assist.move,
            "Stunt Double spends one bar and starts the teammate's assist");
    }
    menu.reset_practice();
    auto& fighter = menu.practice_players[0];
    fighter.burst_ticks = Fighter::burst_clip.duration();
    fighter.tension = 0;
    for (const WPARAM key : {WPARAM(VK_RIGHT), WPARAM('S'), WPARAM('X')}) menu.practice_key(key, true);
    menu.practice_tick();
    check(!menu.practice_teams[0].tag && fighter.burst_ticks, "Stunt Double cannot tag without meter");
}

}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    if (argc != 2) return 2;
    try {
        const std::filesystem::path directory = argv[1];
        const auto asset = CharacterAssets(directory, "filia").read_animations(true);
        verify_movement_defaults(directory);
        verify_timelines(asset);
        verify_ringlet(asset.children.at(0).animations.at(0));
        verify_commands(asset, true);
        verify_commands(asset, false);
        Menu menu;
        menu.game_data = directory;
        menu.load_movement_frames();
        const auto filia = std::find(roster.begin(), roster.end(), std::string_view("filia"));
        for (auto& team : menu.fighters) team = {static_cast<int>(filia - roster.begin())};
        menu.practice_width = 1700;
        menu.practice_height = 850;
        menu.practice_floor = 145;
        menu.practice_starts = {700, 850};
        verify_fenrir_contact(menu);
        verify_stunt_double(menu, menu.fighters[0].front());
        std::cout << "Filia move data, commands, Ringlet playback, Fenrir recovery, and Stunt Double passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
