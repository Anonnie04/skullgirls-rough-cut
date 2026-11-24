#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"

#include <iostream>

namespace {

void check(bool ok, const std::string& what)
{
    if (!ok) throw std::runtime_error(what);
}

void verify_commands(Menu& menu, bool facing_right)
{
    using Fighter = Menu::PracticeFighter;
    const auto ready = [&](bool airborne) {
        Fighter fighter;
        auto& bella = fighter.cerebella.emplace();
        bella.facing_right = facing_right;
        fighter.practice_x = 800;
        if (airborne) {
            bella.start(62);
            bella.state.variables[0x3b] = 2.0f;
            fighter.jump_height = 450;
        }
        return fighter;
    };
    const auto tick = [&](Fighter& fighter, unsigned direction, unsigned buttons = 0, unsigned held = 0) {
        constexpr unsigned mirror[] = {4, 3, 2, 1, 0, 7, 6, 5, 8};
        if (!facing_right) direction = mirror[direction];
        fighter.practice_left = direction >= 3 && direction <= 5;
        fighter.practice_right = direction == 0 || direction == 1 || direction == 7;
        fighter.practice_down = direction >= 1 && direction <= 3;
        fighter.practice_up = direction >= 5 && direction <= 7;
        fighter.pressed_punches = buttons & 7u;
        fighter.pressed_kicks = buttons >> 3;
        fighter.held_attack_buttons = buttons | held;
        menu.tick_cerebella(fighter, facing_right ? 1500 : 100, 0, 1700, facing_right);
    };
    const auto label = [&](unsigned move) -> const std::string& { return menu.cerebella_assets.names.at(move); };
    const auto entered = [&](const Fighter& fighter, unsigned target) {
        check(fighter.cerebella->move == target, "Expected " + label(target) + ", got " + label(fighter.cerebella->move));
    };
    const auto finishes = [&](Fighter& fighter, unsigned move) {
        for (unsigned i = 0; i < 240 && fighter.cerebella->attacking(); ++i) tick(fighter, 8);
        check(!fighter.cerebella->attacking(), label(move) + " must leave attack recovery");
    };
    constexpr unsigned normals[][6] = {{98, 101, 103, 100, 102, 105}, {106, 109, 111, 108, 110, 112},
        {113, 118, 126, 117, 125, 133}};
    for (unsigned stance = 0; stance < 3; ++stance) {
        for (unsigned button = 0; button < 6; ++button) {
            auto fighter = ready(stance == 2);
            tick(fighter, stance == 1 ? 2 : 8, 1u << button);
            entered(fighter, normals[stance][button]);
            finishes(fighter, normals[stance][button]);
        }
    }
    for (const bool contact : {false, true}) {
        auto chain = ready(false);
        tick(chain, 8, 1);
        for (unsigned i = 0; i < 30 && !chain.cerebella->hit; ++i) tick(chain, 8);
        check(chain.cerebella->hit != nullptr, "Standing LP reaches its active record");
        if (contact) chain.cerebella->report_contact(false);
        tick(chain, 8, 2);
        entered(chain, contact ? 101u : 98u);
    }
    struct Command { std::initializer_list<unsigned> motion; unsigned buttons, entry, cost; bool airborne = false; };
    const Command commands[] = {
        {{0}, 4, 104, 0}, {{2}, 2, 119, 0, true}, {{2, 1, 0}, 1, 187, 0}, {{2, 1, 0}, 2, 189, 0},
        {{2, 1, 0}, 4, 191, 0}, {{0, 2, 1}, 1, 182, 0}, {{0, 2, 1}, 2, 184, 0}, {{0, 2, 1}, 4, 185, 0},
        {{2, 1, 0}, 9, 173, 0}, {{2, 3, 4}, 9, 166, 0}, {{0, 2, 1}, 9, 178, 0},
        {{8}, 9, 147, 0, true}, {{2, 1, 0}, 9, 157, 0, true},
        {{2, 1, 0}, 3, 228, 1}, {{2, 3, 4}, 3, 227, 3},
    };
    for (const auto& command : commands) {
        auto fighter = ready(command.airborne);
        for (auto dir = command.motion.begin(); dir != command.motion.end(); ++dir)
            tick(fighter, *dir, dir + 1 == command.motion.end() ? command.buttons : 0);
        entered(fighter, command.entry);
        check(fighter.tension == 5 - command.cost, label(command.entry) + " meter cost");
        finishes(fighter, command.entry);
        if (command.cost) {
            auto empty = ready(command.airborne);
            empty.tension = command.cost - 1;
            for (auto dir = command.motion.begin(); dir != command.motion.end(); ++dir)
                tick(empty, *dir, dir + 1 == command.motion.end() ? command.buttons : 0);
            check(empty.cerebella->move != command.entry && empty.tension == command.cost - 1,
                label(command.entry) + " requires enough meter");
        }
    }
    auto showstopper = ready(false);
    tick(showstopper, 0); tick(showstopper, 2); tick(showstopper, 4); tick(showstopper, 6, 9);
    entered(showstopper, 218);
    check(showstopper.tension == 4, "Showstopper spends one bar");

    for (const auto buttons : {18u, 36u}) {
        auto snap = ready(false);
        tick(snap, 2); tick(snap, 1); tick(snap, 0, buttons);
        entered(snap, 238);
        check(snap.tension == 4 && snap.snapback_reserve == (buttons == 36 ? 1u : 0u),
            "Snapback spends one bar and selects a reserve");
        finishes(snap, 238);
    }

    auto glide = ready(true);
    tick(glide, 8, 4);
    bool saw_glide = false;
    for (unsigned i = 0; i < 55; ++i) {
        tick(glide, 8, 0, 4);
        saw_glide |= glide.cerebella->move == 128;
    }
    check(saw_glide, "Held jump HP reaches the Glide loop");

    auto run = ready(false);
    for (unsigned i = 0; i < 38; ++i) tick(run, 4);
    tick(run, 0, 8);
    for (unsigned i = 0; i < 60 && run.cerebella->move != 198; ++i) tick(run, 8);
    entered(run, 198);
    tick(run, 8, 32);
    entered(run, 217);
    finishes(run, 217);

    for (const auto& followup : {std::pair{8u, 199u}, std::pair{16u, 201u}, std::pair{2u, 202u}, std::pair{9u, 209u}}) {
        auto runner = ready(false);
        for (unsigned i = 0; i < 38; ++i) tick(runner, 4);
        tick(runner, 0, 8);
        for (unsigned i = 0; i < 60 && runner.cerebella->move != 198; ++i) tick(runner, 8);
        entered(runner, 198);
        tick(runner, 8, followup.first);
        check(runner.cerebella->move == followup.second ||
            ((followup.second == 201 || followup.second == 202) && runner.cerebella->move == 203),
            "Tumbling Run selects its authored follow-up " + std::to_string(followup.second) +
                ", got " + std::to_string(runner.cerebella->move));
        finishes(runner, followup.second);
    }
    auto taunt = ready(false);
    tick(taunt, 8, 1); tick(taunt, 8, 2); tick(taunt, 4); tick(taunt, 4, 8); tick(taunt, 8, 16);
    entered(taunt, 233);
    finishes(taunt, 233);

    auto walk = ready(false);
    const auto start = walk.practice_x;
    for (unsigned i = 0; i < 38; ++i) tick(walk, 0);
    entered(walk, 27);
    check(facing_right ? walk.practice_x > start : walk.practice_x < start, "Forward walk translates");
    for (unsigned i = 0; i < 20; ++i) tick(walk, 8);
    entered(walk, 20);

    auto jump = ready(false);
    for (unsigned i = 0; i < 12 && !jump.jump_height; ++i) tick(jump, 6);
    check(jump.jump_height > 0 && jump.cerebella->move >= 61, "Cerebella jump launches from the authored startup");
    for (unsigned i = 0; i < 120 && jump.jump_height; ++i) tick(jump, 8);
    check(!jump.jump_height, "Cerebella jump lands");

    for (const unsigned direction : {0u, 4u}) {
        auto dash = ready(false);
        tick(dash, direction); tick(dash, 8); tick(dash, direction);
        entered(dash, direction == 0 ? 36 : 38);
        const auto dash_start = dash.practice_x;
        for (unsigned i = 0; i < 45; ++i) tick(dash, 8);
        check((dash.practice_x > dash_start) == (facing_right == (direction == 0)),
            "Dash direction " + std::to_string(direction) + " facing " + std::to_string(facing_right) +
            " from " + std::to_string(dash_start) + " to " + std::to_string(dash.practice_x) +
            " move " + std::to_string(dash.cerebella->move) + " velocity " + std::to_string(dash.cerebella->value(0x9e)));
        for (unsigned i = 0; i < 80; ++i) tick(dash, 8);
        entered(dash, 20);
    }
}

void verify_training(Menu& menu)
{
    const auto roster_index = [](std::string_view name) {
        return static_cast<int>(std::find(roster.begin(), roster.end(), name) - roster.begin());
    };
    const int cerebella = roster_index("cerebella"), filia = roster_index("filia");
    menu.load_movement_frames();
    for (const unsigned reference : {1012u, 1013u, 1014u, 119u, 134u, 135u, 150u, 153u, 176u, 180u, 113u}) {
        const auto pose = menu.cerebella_team_pose(reference);
        check(pose > 1 && menu.cerebella_frames[pose].width, "Cerebella's team travel retains a visible pose");
    }
    for (const int id : {cerebella, filia}) {
        const auto palette = skullgirls::CharacterAssets(menu.game_data, roster[id]).read_default_palette();
        menu.palettes[id].push_back({0, palette});
    }
    menu.practice_width = 1700;
    menu.practice_height = 900;
    menu.practice_floor = 145;
    menu.practice_starts = {760, 878};
    for (const int partner : {filia, cerebella}) {
        menu.fighters[0] = {cerebella};
        menu.fighters[1] = {partner};
        for (auto& colors : menu.locked_colors) colors = {0};
        menu.prepare_practice_bitmaps();
        check(menu.practice_slot_playable(0, 0) && menu.practice_slot_playable(1, 0) &&
              menu.practice_palette_lookups.size() == (partner == cerebella ? 1 : 2), "Training roster and palettes");
        menu.reset_practice();
        menu.start_practice_intro();
        check(menu.practice_presentation.phase == Menu::PracticePresentation::Phase::intro, "Cerebella starts her intro");
        for (unsigned i = 0; i < 450 && menu.practice_presentation.phase != Menu::PracticePresentation::Phase::none; ++i)
            menu.practice_tick();
        check(menu.practice_presentation.phase == Menu::PracticePresentation::Phase::none &&
            menu.practice_players[0].cerebella->move == 20, "Cerebella's intro returns to idle");

        Gdiplus::Bitmap canvas(640, 480, PixelFormat32bppARGB);
        Gdiplus::Graphics image(&canvas);
        image.Clear(Gdiplus::Color::Transparent);
        menu.draw_practice_frame(image, 0, menu.practice_players[0].sprite(), 320, 410, false, false);
        bool painted = false;
        for (int y = 20; y < 440 && !painted; y += 12) for (int x = 120; x <= 520 && !painted; x += 12) {
            Gdiplus::Color pixel;
            painted = canvas.GetPixel(x, y, &pixel) == Gdiplus::Ok && pixel.GetAlpha() != 0;
        }
        check(painted, "Cerebella's Training sprite renders from her own palette and art");

        for (const std::size_t side : {0u, 1u}) {
            menu.reset_practice();
            const WPARAM jab = side ? 'V' : 'A';
            menu.practice_key(jab, true);
            menu.practice_tick();
            menu.practice_key(jab, false);
            for (unsigned i = 0; i < 35 && !menu.hud_stats[side].damage; ++i) menu.practice_tick();
            check(menu.hud_stats[side].damage == 300, "A jab uses the attacker's damage and the opponent's hurtboxes");
        }

        menu.reset_practice();
        menu.training_options.push_back({L"Block", {}, 2, "OPT_STR_BLOCKING"});
        menu.practice_key('A', true);
        menu.practice_tick();
        menu.practice_key('A', false);
        for (unsigned i = 0; i < 35 && !menu.hud_stats[0].blocked_hits; ++i) menu.practice_tick();
        check(menu.hud_stats[0].blocked_hits && !menu.hud_stats[0].damage, "Cerebella's jab can be guarded");
        menu.training_options.pop_back();

        struct Grab { std::initializer_list<unsigned> directions; unsigned entry, buttons; };
        const Grab grabs[] = {{{8}, 135, 9}, {{4}, 134, 9}, {{2, 1, 0}, 173, 9},
            {{2, 3, 4}, 166, 9}, {{0, 2, 4, 6}, 218, 9}};
        const auto input = [&](unsigned dir, unsigned buttons) {
            constexpr std::array<WPARAM, 6> attacks{'A', 'S', 'D', 'Z', 'X', 'C'};
            for (const auto& key : {std::pair{WPARAM(VK_LEFT), dir >= 3 && dir <= 5},
                                   std::pair{WPARAM(VK_RIGHT), dir == 0 || dir == 1 || dir == 7},
                                   std::pair{WPARAM(VK_DOWN), dir >= 1 && dir <= 3},
                                   std::pair{WPARAM(VK_UP), dir >= 5 && dir <= 7}})
                menu.practice_key(key.first, key.second);
            for (unsigned i = 0; i < attacks.size(); ++i) menu.practice_key(attacks[i], (buttons & (1u << i)) != 0);
            menu.practice_tick();
        };
        for (const auto& command : grabs) {
            menu.practice_keys_held = {};
            menu.reset_practice();
            if (command.entry == 166) menu.practice_players[1].practice_x = 1060;
            for (auto dir = command.directions.begin(); dir != command.directions.end(); ++dir)
                input(*dir, dir + 1 == command.directions.end() ? command.buttons : 0);
            bool captured = false;
            for (unsigned i = 0; i < 450; ++i) {
                input(8, 0);
                captured |= menu.practice_players[0].cerebella->captured_target;
            }
            const auto& owner = *menu.practice_players[0].cerebella;
            check(captured, "Grab must capture its target " + std::to_string(command.entry));
            check(!owner.captured_target && menu.practice_players[1].throw_phase == Menu::PracticeFighter::ThrowPhase::none,
                "Grab must release its target " + std::to_string(command.entry) + " phase " + std::to_string(owner.move));
            check(menu.hud_stats[0].damage > 0, "Grab deals its authored damage " + std::to_string(command.entry));
        }
        for (const unsigned move : {147u, 157u, 178u, 209u}) {
            menu.practice_keys_held = {};
            menu.reset_practice();
            auto& attacker = menu.practice_players[0];
            auto& target = menu.practice_players[1];
            attacker.cerebella->start(move);
            const bool aerial_attacker = move == 147 || move == 157;
            attacker.jump_height = aerial_attacker ? 250.0f : 0.0f;
            attacker.cerebella->state.variables[0x3b] = aerial_attacker ? 2.0f : 3.0f;
            target.jump_height = move == 209 ? 0.0f : 250.0f;
            if (target.cerebella && move != 209) {
                target.cerebella->start(84);
                target.cerebella->state.variables[0x3b] = 2.0f;
            }
            if (move == 178) {
                constexpr skullgirls::NormalHit setup{0, 60, 0, 3};
                target.take_hit(setup, false, -1, {skullgirls::HeavyReaction::air_launch, 0, 0});
            }
            bool captured = false;
            for (unsigned i = 0; i < 450; ++i) {
                menu.practice_tick();
                captured |= attacker.cerebella->captured_target;
            }
            check(captured && !attacker.cerebella->captured_target && menu.hud_stats[0].damage > 0,
                "Air grab captures, damages, and releases its target " + std::to_string(move) +
                " captured=" + std::to_string(captured) + " holding=" + std::to_string(attacker.cerebella->captured_target) +
                " phase=" + std::to_string(attacker.cerebella->move) + " y=" + std::to_string(attacker.jump_height) +
                " damage=" + std::to_string(menu.hud_stats[0].damage));
        }
    }

    menu.practice_keys_held = {};
    menu.fighters[0] = {cerebella, filia, cerebella};
    menu.locked_colors[0] = {0, 0, 0};
    menu.reset_practice();
    auto& team = menu.practice_teams[0];
    for (const std::size_t expected : {1u, 0u, 2u}) {
        menu.practice_players[0].pressed_punches = menu.practice_players[0].pressed_kicks = expected == 2 ? 4 : 2;
        menu.sample_practice_team_actions();
        check(team.tag.has_value(), "Mixed teams can start a regular tag");
        for (unsigned i = 0; i < 160 && team.tag; ++i) menu.advance_practice_tags();
        check(team.active_slot == expected, "Mixed teams select the requested reserve");
        check(menu.practice_players[0].cerebella.has_value() == (expected != 1), "Tag preserves the incoming character type");
        for (unsigned i = 0; i < 160; ++i) menu.practice_tick();
    }
    menu.practice_players[0].pressed_punches = 1;
    menu.practice_players[0].pressed_kicks = 2;
    menu.sample_practice_team_actions();
    check(team.assist.has_value(), "Cerebella can call a mixed-team assist");
    for (unsigned i = 0; i < 400 && team.assist; ++i) menu.advance_practice_assists(0, 1700);
    check(!team.assist, "Mixed-team assist returns offscreen");
}

void verify_defense(Menu& menu)
{
    using Fighter = Menu::PracticeFighter;
    for (unsigned stance = 0; stance < 3; ++stance) {
        menu.reset_practice();
        auto& defender = menu.practice_players[0];
        defender.blockstun = 30;
        defender.crouching = stance == 1;
        defender.jump_height = stance == 2 ? 450.0f : 0.0f;
        defender.pressed_punches = defender.held_attack_buttons = 3;
        menu.advance_training_pushblock(0, 1700);
        check(defender.cerebella->move == (stance == 2 ? 437u : stance == 1 ? 431u : 422u),
            "Pushblock uses Cerebella's stance-specific animation");
        for (unsigned i = 0; i < 80; ++i) menu.tick_cerebella(defender, 1500, 0, 1700, true);
        check(!defender.blockstun && defender.guard_phase == Fighter::GuardPhase::none,
            "Pushblock releases guard stun");
    }
    for (const bool airborne : {false, true}) {
        menu.reset_practice();
        auto& defender = menu.practice_players[0];
        defender.hurt_group = skullgirls::air_mid;
        defender.burst_available = true;
        defender.jump_height = airborne ? 450.0f : 0.0f;
        defender.pressed_punches = 1;
        menu.advance_training_bursts();
        check(defender.burst_ticks && defender.cerebella->move == 448, "Burst starts Cerebella's animation");
        bool falling = false;
        for (unsigned i = 0; i < 300; ++i) {
            menu.tick_cerebella(defender, 1500, 0, 1700, true);
            falling |= defender.burst_fall_tick != 0;
        }
        check(!defender.burst_ticks && !defender.burst_fall_tick && defender.jump_height == 0 &&
            defender.cerebella->move == 20, "Burst recovers to the ground and idle, airborne=" + std::to_string(airborne) +
                " move=" + std::to_string(defender.cerebella->move) + " height=" + std::to_string(defender.jump_height) +
                " ticks=" + std::to_string(defender.burst_ticks) + " fall=" + std::to_string(defender.burst_fall_tick));
        check(!airborne || falling, "Airborne burst passes through Cerebella's fall animation");
    }

    menu.reset_practice();
    auto& bella = *menu.practice_players[0].cerebella;
    bella.start(187);
    bella.animate(menu.cerebella_assets);
    const auto saved_record = bella.record;
    menu.save_or_restore_practice(false);
    bella.start(103);
    menu.save_or_restore_practice(true);
    check(bella.move == 187 && bella.record == saved_record, "Practice save restores Cerebella's move state");

    const auto& launcher = menu.cerebella_assets.animations[111][14].hit;
    check(launcher && launcher->damage == 950, "Crouching HP hit data loads");
    const auto reaction = Menu::cerebella_hit_reaction(*launcher, menu.practice_players[1]);
    check(reaction.type == skullgirls::HeavyReaction::air_launch && reaction.upward == 39 && reaction.horizontal == 6,
        "Crouching HP reads the original grounded launch script");
}

void verify_throw_tech(Menu& menu)
{
    const int bella = int(std::find(roster.begin(), roster.end(), std::string_view("cerebella")) - roster.begin());
    const int filia = int(std::find(roster.begin(), roster.end(), std::string_view("filia")) - roster.begin());
    menu.training_options.push_back({L"Throw Tech", {}, 1, "OPT_STR_THROWTECH"});
    for (const int character : {bella, filia}) for (const bool air : {false, true}) {
        menu.practice_keys_held = {};
        menu.fighters[0] = {character};
        menu.fighters[1] = {bella};
        menu.reset_practice();
        if (air) for (auto& fighter : menu.practice_players) {
            fighter.jump_height = 250;
            if (fighter.cerebella) {
                fighter.cerebella->start(84);
                fighter.cerebella->state.variables[0x3b] = 2.0f;
            }
        }
        menu.practice_key('A', true);
        menu.practice_key('Z', true);
        menu.practice_tick();
        menu.practice_key('A', false);
        menu.practice_key('Z', false);
        bool teched = false;
        for (unsigned i = 0; i < 240; ++i) {
            menu.practice_tick();
            teched |= menu.practice_players[0].throw_phase == Menu::PracticeFighter::ThrowPhase::tech;
        }
        check(teched && !menu.hud_stats[0].damage &&
            menu.practice_players[0].throw_phase == Menu::PracticeFighter::ThrowPhase::none &&
            menu.practice_players[1].throw_phase == Menu::PracticeFighter::ThrowPhase::none,
            "Throw tech succeeds and both characters recover, attacker=" + std::string(roster[character]) +
                " air=" + std::to_string(air) + " phase=" + std::to_string(menu.practice_players[1].cerebella->move));
    }
    menu.training_options.pop_back();
}

void verify_visual_effects(Menu& menu)
{
    const int bella = int(std::find(roster.begin(), roster.end(), std::string_view("cerebella")) - roster.begin());
    const int filia = int(std::find(roster.begin(), roster.end(), std::string_view("filia")) - roster.begin());
    menu.load_practice_effects();
    const auto spark = Menu::effect_name_hash("super_spark_cross");
    const auto lightning = Menu::effect_name_hash("super_spark_lightning");
    check(menu.effect_textures.count(spark) && menu.effect_textures.count(lightning), "Original super spark textures load");
    check(menu.effect_textures.at(spark).frames.size() == 6 &&
        menu.effect_textures.at(Menu::effect_name_hash("super_spark_particles")).frames.size() == 15 &&
        menu.effect_textures.at(lightning).frames.size() == 14,
        "Shared texture sequences retain their individual frame ranges");
    check(menu.movement_frames[1185].width && menu.filia_move_table.children[2].animations[0].back().destroys_entity,
        "Fenrir's child art and destruction flag load");
    struct Super { int character; unsigned move; bool air; };
    for (const auto& super : {Super{filia, skullgirls::gregor_start, false},
             Super{filia, skullgirls::gregor_air_start, true}, Super{filia, skullgirls::fenrir_start, false},
             Super{filia, skullgirls::tricobezoar, false}, Super{bella, 218, false},
             Super{bella, 227, false}, Super{bella, 228, false}}) {
        menu.practice_keys_held = {};
        menu.fighters[0] = {super.character};
        menu.fighters[1] = {bella};
        menu.prepare_practice_bitmaps();
        menu.reset_practice();
        auto& caster = menu.practice_players[0];
        caster.practice_x = 450;
        menu.practice_players[1].practice_x = super.character == bella && super.move == 218 ? 550.0f : 1300.0f;
        caster.jump_height = super.air ? 300.0f : 0.0f;
        if (caster.cerebella) caster.cerebella->start(super.move);
        else {
            caster.active_normal = int(super.move);
            caster.normal_facing_right = true;
        }
        menu.practice_key('L', true);
        unsigned frozen_ticks = 0;
        bool saw_spark = false, painted = false, saved = false, saw_child = false;
        for (unsigned i = 0; i < 600; ++i) {
            const bool frozen = menu.practice_super_frozen();
            const float before = menu.practice_players[1].practice_x;
            const auto stage_tick = menu.practice_stage_tick;
            menu.practice_tick();
            if (frozen && menu.practice_super_frozen()) {
                ++frozen_ticks;
                check(menu.practice_players[1].practice_x == before && menu.practice_stage_tick == stage_tick,
                    "Super freeze pauses the opponent and stage");
            }
            for (const auto& effect : menu.practice_effects) {
                saw_spark |= effect.texture() == spark || effect.texture() == lightning;
                if (super.character == filia && super.move == skullgirls::fenrir_start)
                    saw_child |= effect.table == &menu.filia_move_table.children[2] && effect.pose >= 1185;
                if (super.character == bella && super.move == 227)
                    saw_child |= effect.table == &menu.cerebella_assets.children[5] && effect.pose >= 1392;
                if (super.character == bella && super.move == 218)
                    saw_child |= effect.table == &menu.cerebella_assets.children[7];
            }
            if (saw_spark && !painted) {
                Gdiplus::Bitmap canvas(1280, 720, PixelFormat32bppARGB);
                Gdiplus::Graphics image(&canvas);
                image.Clear(Gdiplus::Color::Transparent);
                menu.paint_practice_effects(image, 0, 620, true);
                menu.paint_practice_effects(image, 0, 620, false);
                for (int y = 0; y < 720 && !painted; y += 8) for (int x = 0; x < 1280 && !painted; x += 8) {
                    Gdiplus::Color pixel;
                    painted = canvas.GetPixel(x, y, &pixel) == Gdiplus::Ok && pixel.GetAlpha() != 0;
                }
            }
            if (saw_spark && !saved && menu.practice_super_frozen()) {
                const auto count = menu.practice_effects.size();
                menu.save_or_restore_practice(false);
                menu.practice_effects.clear();
                Menu::fighter_visuals(caster).freeze = 0;
                menu.save_or_restore_practice(true);
                check(menu.practice_super_frozen() && menu.practice_effects.size() == count,
                    "Save state restores super freeze and active effects");
                saved = true;
            }
        }
        check(frozen_ticks && saw_spark && painted && !menu.practice_super_frozen(),
            "Super freeze and spark complete for " + std::string(roster[super.character]) + " " +
                std::to_string(super.move) + " frozen=" + std::to_string(frozen_ticks) + " spark=" + std::to_string(saw_spark));
        if (!menu.practice_effects.empty()) {
            const auto& effect = menu.practice_effects.front();
            check(false, "Super effects clean up after playback, move " + std::to_string(super.move) +
                " remaining=" + std::to_string(menu.practice_effects.size()) + " effect=" + effect.table->names[effect.move] +
                " record=" + std::to_string(effect.record) + " y=" + std::to_string(effect.y) +
                " velocity=" + std::to_string(effect.value(0x9f)));
        }
        if ((super.character == filia && super.move == skullgirls::fenrir_start) ||
            (super.character == bella && (super.move == 227 || super.move == 218)))
            check(saw_child, "Super spawns its character-specific effect " + std::to_string(super.move));
        menu.practice_key('L', false);
    }
    menu.practice_keys_held = {};
    menu.fighters[0] = menu.fighters[1] = {bella};
    menu.reset_practice();
    Menu::fighter_visuals(menu.practice_players[0]).freeze = 1;
    menu.practice_key('V', true);
    menu.practice_tick();
    check(menu.practice_players[1].cerebella->move == 20, "Opponent waits during super freeze");
    Menu::fighter_visuals(menu.practice_players[0]).freeze = 0;
    menu.practice_tick();
    check(menu.practice_players[1].cerebella->move == 98, "Input sampled during freeze enters the normal buffer");
    menu.practice_key('V', false);
    menu.practice_keys_held = {};
    menu.reset_practice();
    Menu::fighter_visuals(menu.practice_players[0]).freeze = 1;
    menu.practice_key('V', true);
    for (unsigned i = 0; i < 7; ++i) menu.practice_tick();
    Menu::fighter_visuals(menu.practice_players[0]).freeze = 0;
    menu.practice_tick();
    check(menu.practice_players[1].cerebella->move == 20, "Freeze input buffer expires after five frames");
    menu.practice_key('V', false);

    menu.practice_keys_held = {};
    menu.fighters[0] = menu.fighters[1] = {filia};
    menu.reset_practice();
    Menu::fighter_visuals(menu.practice_players[0]).freeze = 1;
    for (auto& fighter : menu.practice_players) {
        fighter.ringlet_spikes.push_back({1000, false, 1, 0});
        fighter.tricobezoar = Menu::Tricobezoar{1000, 300, false, 2, 0};
    }
    menu.practice_tick();
    for (const auto& fighter : menu.practice_players)
        check(fighter.ringlet_spikes[0].tick == 0 && fighter.tricobezoar->age == 0,
            "Super freeze pauses both sides' existing projectiles");
    menu.reset_practice();
}

}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    if (argc != 2) return 2;
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR graphics = 0;
    if (Gdiplus::GdiplusStartup(&graphics, &input, nullptr) != Gdiplus::Ok) return 2;
    try {
        Menu menu;
        menu.game_data = argv[1];
        menu.load_cerebella();
        check(menu.cerebella_assets.animations.size() == 564 && menu.cerebella_frames[2].width,
            "Cerebella move data and stand sprite load");
        verify_commands(menu, true);
        verify_commands(menu, false);
        verify_training(menu);
        verify_defense(menu);
        verify_throw_tech(menu);
        verify_visual_effects(menu);
        std::cout << "Cerebella move data and Training commands passed in both facings\n";
    } catch (const std::exception& e) {
        Gdiplus::GdiplusShutdown(graphics);
        std::cerr << e.what() << '\n';
        return 1;
    }
    Gdiplus::GdiplusShutdown(graphics);
    return 0;
}
