#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif


#include "../src/main_menu.cpp"
#include <iostream>

namespace {
void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
int character(std::string_view name) {
    return int(std::find(roster.begin(), roster.end(), name) - roster.begin());
}
void setup(Menu& menu) {
    menu.load_movement_frames();
    menu.load_robofortune();
    for (const auto* name : {"filia", "robofortune"}) {
        const auto id = character(name);
        menu.palettes[id].push_back({0, skullgirls::CharacterAssets(menu.game_data, name).read_default_palette()});
    }
    menu.fighters[0] = {character("filia")};
    menu.fighters[1] = {character("robofortune")};
    menu.locked_colors[0] = menu.locked_colors[1] = {0};
    menu.practice_width = 1700;
    menu.practice_height = 2000;
    menu.practice_floor = 64;
    menu.practice_starts = {400, 1250};
    skullgirls::SpriteArchive ui_win(menu.game_data/"ui-win.gfs"), ui(menu.game_data/"ui.gfs");
    menu.combat_scene=skullgirls::read_combat_scene(ui_win.read("ui/Win/InGame.gbs"));
    for (unsigned i=0;i<2;++i) menu.hud_atlas[i].load(ui_win.read("ui/Win/Textures/TA_1001_"+std::to_string(i+1)+".dds"));
    for (const auto* name:{"filia","robofortune"}) {
        const auto id=character(name);
        auto portrait=std::make_unique<Texture>();
        portrait->load(ui.read("ui/true_color_hud/super_"+std::string(name)+"01.dds"));
        menu.super_portraits[id]=std::move(portrait);
    }
    menu.practice_background=std::make_unique<Texture>();
    menu.practice_background->load(skullgirls::read_sprite_asset(menu.game_data/"stages-textures2d.gfs","stages/textures/2D/stage_class_notes-alt.dds"),Texture::Encoding::stage_rgb);
    menu.load_practice_effects();
    skullgirls::SpriteArchive sprites(menu.game_data/"sprites.gfs");
    const char* sparks[]={"hitspark_omni_l","hitspark_omni_m","hitspark_omni_h"};
    for (unsigned i=0;i<3;++i) menu.combat_effects[i].load(sprites,sparks[i],sparks[i]);
    menu.prepare_hud_portraits();
    menu.prepare_practice_bitmaps();
    menu.reset_practice();
}
#ifdef _WIN32
void combat_art_cache(Menu& menu) {
    skullgirls::SelectFrame pixel_fixture{};pixel_fixture.width=2;pixel_fixture.height=1;
    auto premultiplied=menu.make_fighter_bitmap(pixel_fixture,[](std::size_t i){return i?0x00ffffffu:0x80c86432u;});
    Gdiplus::BitmapData pixel_data{};Gdiplus::Rect pixel_region(0,0,2,1);
    check(premultiplied && premultiplied->LockBits(&pixel_region,Gdiplus::ImageLockModeRead,
        PixelFormat32bppPARGB,&pixel_data)==Gdiplus::Ok,"Read prepared premultiplied sprite pixels");
    const auto* prepared=static_cast<const unsigned*>(pixel_data.Scan0);
    const bool correct_pixels=prepared[0]==0x80643219u && prepared[1]==0;
    premultiplied->UnlockBits(&pixel_data);
    check(correct_pixels,"Prepare alpha once and remove hidden RGB from transparent sprite pixels");
    menu.prepare_combat_art();
    const auto count=menu.combat_tiles.size();
    check(count>0,"Prepare blockbuster art before combat");
    for (auto& entry:menu.combat_tiles) {
        check(bool(entry.second),"Blockbuster tile cache contains decoded art");
        for (float opacity:{1.0f,0.45f}) {
            Gdiplus::Bitmap cached(256,160,PixelFormat32bppPARGB), atlas(256,160,PixelFormat32bppPARGB);
            const auto draw=[&](Gdiplus::Bitmap& image) {
                Gdiplus::Graphics dc(&image);dc.Clear(Gdiplus::Color(255,0,0,0));
                dc.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
                dc.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                dc.TranslateTransform(128,80);dc.RotateTransform(11);
                menu.draw_combat_tile(dc,entry.first,{-90,-45,180,90},opacity);
            };
            draw(cached);
            auto image=std::move(entry.second); // Null entry exercises the uncropped atlas fallback.
            draw(atlas);entry.second=std::move(image);
            unsigned difference=0,samples=0;
            for(int y=4;y<156;y+=8)for(int x=4;x<252;x+=8) {
                Gdiplus::Color a,b;cached.GetPixel(x,y,&a);atlas.GetPixel(x,y,&b);
                for(unsigned shift:{0u,8u,16u,24u}) {
                    difference+=unsigned(std::abs(int((a.GetValue()>>shift)&255)-int((b.GetValue()>>shift)&255)));++samples;
                }
            }
            check(difference<samples*3,"Cropped blockbuster art preserves rotated and faded atlas rendering");
        }
    }
    menu.prepare_combat_art();
    check(menu.combat_tiles.size()==count,"Blockbuster art cache stays bounded across repeated preparation");
    unsigned sprite=2;
    while(sprite<menu.movement_frames.size() && !menu.movement_frames[sprite].width)++sprite;
    Gdiplus::Bitmap warm(640,480,PixelFormat32bppPARGB);
    { Gdiplus::Graphics dc(&warm);menu.draw_practice_frame(dc,0,sprite,320,400,false,false); }
    auto& cached=menu.practice_palette_cache.at({character("filia"),0,sprite});
    const auto overlay=menu.jab_overlays.find(sprite);
    auto original=menu.make_practice_bitmap(menu.movement_frames[sprite],
        overlay==menu.jab_overlays.end()?nullptr:&overlay->second,
        menu.practice_palette_lookups.at({character("filia"),0}));
    for(bool shadow:{false,true}) {
        Gdiplus::Bitmap baked(640,480,PixelFormat32bppPARGB), direct(640,480,PixelFormat32bppPARGB);
        const auto draw=[&](Gdiplus::Bitmap& target,Menu::CachedPracticeBitmap* variants) {
            Gdiplus::Graphics dc(&target);dc.Clear(Gdiplus::Color(255,0,0,0));
            dc.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);dc.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            dc.ScaleTransform(0.75f,0.75f);
            menu.draw_fighter_bitmap(dc,variants?*cached.bitmap:*original,
                menu.movement_frames[sprite],400,450,false,shadow,
                0.4f,std::array<float,3>{0.65f,1,0.26f},variants);
        };
        draw(baked,&cached);const auto variants=cached.variants.size();draw(direct,nullptr);
        unsigned difference=0,samples=0;
        for(int y=0;y<480;y+=8)for(int x=0;x<640;x+=8) {
            Gdiplus::Color a,b;baked.GetPixel(x,y,&a);direct.GetPixel(x,y,&b);
            for(unsigned shift:{0u,8u,16u,24u}) {
                difference+=unsigned(std::abs(int((a.GetValue()>>shift)&255)-int((b.GetValue()>>shift)&255)));++samples;
            }
        }
        check(difference<samples*3,"Cached shadows and afterimages preserve transformed color-matrix rendering");
        draw(baked,&cached);check(cached.variants.size()==variants,"Reuse the same shadow/afterimage variant");
    }
    check(menu.practice_variant_bytes<=64*1024*1024,"Translucent sprite cache respects its memory budget");
    const auto presentation=menu.combat_presentation;
    for(bool cutin:{false,true}) {
        menu.combat_presentation.cutins[0].frozen=cutin;
        menu.combat_presentation.cutins[0].frame=40;
        menu.combat_presentation.cutins[0].exit_frame=cutin?0:11;
        Gdiplus::Bitmap cpu(1280,720,PixelFormat32bppPARGB),gpu(1280,720,PixelFormat32bppPARGB);
        menu.sprite_renderer.reset();menu.sprite_renderer_failed=true;
        {Gdiplus::Graphics dc(&cpu);dc.Clear(Gdiplus::Color(255,0,0,0));menu.paint_practice(dc);}
        menu.sprite_renderer_failed=false;menu.prepare_sprite_renderer();
        if(menu.sprite_renderer) {
            {Gdiplus::Graphics dc(&gpu);dc.Clear(Gdiplus::Color(255,0,0,0));menu.paint_practice(dc);}
            unsigned difference=0,samples=0;
            for(int y=0;y<720;y+=4)for(int x=0;x<1280;x+=4) {
                Gdiplus::Color a,b;cpu.GetPixel(x,y,&a);gpu.GetPixel(x,y,&b);
                for(unsigned shift:{0u,8u,16u,24u}) {
                    difference+=unsigned(std::abs(int((a.GetValue()>>shift)&255)-int((b.GetValue()>>shift)&255)));++samples;
                }
            }
            check(difference<samples*3,"GPU fighter and cut-in layers preserve the complete CPU scene composition");
        }
    }
    menu.combat_presentation=presentation;
}
#endif
#ifdef _WIN32
void direct_composition(Menu& menu) {
    menu.prepare_sprite_renderer();if(!menu.sprite_renderer)return;
    auto background=std::move(menu.practice_background);
    auto scene=std::move(menu.practice_scene);
    const auto presentation=menu.combat_presentation;
    menu.practice_scene=std::make_unique<PracticeScene>(menu.game_data,"class_notes_3d",3750.0f,2000.0f,
        menu.practice_floor,66.0f,200.0f,24000.0f);
    for(auto& mesh:menu.practice_scene->meshes)for(auto& v:mesh.vertices)v.x+=(menu.practice_width-3750)*0.5f;
    menu.practice_scene->initialize_gpu(nullptr,menu.sprite_renderer.get());
    check(bool(menu.practice_scene->gpu),"Initialize the shared GPU stage for presentation");
    for(bool super:{false,true}) {
        menu.combat_presentation.cutins[0].frozen=super;
        menu.combat_presentation.cutins[0].frame=40;
        menu.combat_presentation.cutins[0].exit_frame=super?0:11;
        Gdiplus::Bitmap expected(1280,720,PixelFormat32bppPARGB);
        {Gdiplus::Graphics dc(&expected);dc.Clear(Gdiplus::Color(255,0,0,0));
         dc.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);dc.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
         menu.paint_practice(dc);}
        std::vector<unsigned> pixels(1280*720);
        Gdiplus::Bitmap overlay(1280,720,1280*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(pixels.data()));
        Gdiplus::Graphics dc(&overlay);dc.Clear(Gdiplus::Color::Transparent);
        dc.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);dc.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        const auto sprite_reads=menu.sprite_renderer->readback_count(),stage_reads=menu.practice_scene->gpu->readbacks;
        menu.practice_scene->has_drawn=false;
        menu.sprite_renderer->begin_frame(nullptr,reinterpret_cast<const BYTE*>(pixels.data()),1280*4);
        menu.paint_practice(dc);menu.sprite_renderer->flush_overlay(dc);
        check(menu.sprite_renderer->readback_count()==sprite_reads && menu.practice_scene->gpu->readbacks==stage_reads,
            "Complete GPU frame keeps stage, fighters and cut-ins on the GPU");
        auto& actual=menu.sprite_renderer->capture_frame();unsigned difference=0,samples=0;
        for(int y=0;y<720;y+=4)for(int x=0;x<1280;x+=4) {
            Gdiplus::Color a,b;actual.GetPixel(x,y,&a);expected.GetPixel(x,y,&b);
            for(unsigned shift:{0u,8u,16u,24u}) {
                difference+=unsigned(std::abs(int((a.GetValue()>>shift)&255)-int((b.GetValue()>>shift)&255)));++samples;
            }
        }
        menu.sprite_renderer->abort_frame();
        check(difference<samples*3,"Direct GPU frame preserves stage, teacher, sprites, cut-ins and HUD composition");
    }
    menu.combat_presentation=presentation;
    menu.practice_scene=std::move(scene);menu.practice_background=std::move(background);
}
#endif

void robo_commands(Menu& menu) {
    constexpr unsigned normals[2][6]={{108,111,114,110,112,115},{119,122,124,121,123,125}};
    for (bool facing:{false,true}) for (unsigned stance=0;stance<2;++stance) for (unsigned button=0;button<6;++button) {
        Menu::PracticeFighter fighter;
        fighter.robofortune.emplace().facing_right=facing;
        fighter.practice_x=800;fighter.practice_down=stance!=0;
        fighter.pressed_punches=button<3?1u<<button:0;
        fighter.pressed_kicks=button>=3?1u<<(button-3):0;
        fighter.held_attack_buttons=1u<<button;
        menu.tick_robofortune(fighter,facing?1500.0f:100.0f,0,1700,facing);
        check(fighter.robofortune->move==normals[stance][button],"Robo-Fortune uses her own normal commands in both facings");
    }
}

void sequence(Menu& menu, const std::filesystem::path& output = {}, bool mirrored=false) {
    const auto step = [&](std::initializer_list<WPARAM> keys) {
        for (auto key : keys) menu.practice_key(key, true);
        menu.practice_tick();
        for (auto key : keys) menu.practice_key(key, false);
    };
    const WPARAM back=mirrored?VK_RIGHT:VK_LEFT;
    step({VK_DOWN}); step({VK_DOWN,back}); step({back,'Z','X'});
    check(menu.practice_players[0].active_normal == skullgirls::gregor_start, "Quarter-circle back + two kicks starts Gregor");
    unsigned old_hits = 0;
    bool second = false, captured=false, pause_checked=false, snapshot_checked=false;
    unsigned freezes=0; bool frozen=false;
    for (unsigned tick = 0; tick < 600; ++tick) {
        auto& filia = menu.practice_players[0];
        auto& robo = menu.practice_players[1];
        if (!second && filia.active_normal < 0 && menu.hud_stats[0].hits) {
            const WPARAM forward = filia.practice_x < robo.practice_x ? VK_RIGHT : VK_LEFT;
            step({forward}); step({VK_DOWN}); step({VK_DOWN, forward, 'A', 'S'});
            second = true;
            check(filia.active_normal==skullgirls::fenrir_start,"Dragon-punch + two punches starts the Fenrir follow-up");
        }
        const bool was_frozen=menu.practice_super_frozen();
        const auto ghost_count=menu.combat_presentation.ghosts[0].size();
        menu.practice_tick();
        if (was_frozen) check(menu.combat_presentation.ghosts[0].size()==ghost_count,
            "Super-freeze holds existing afterimages without adding stationary duplicates");
        const bool freeze=filia.visual.freeze!=0;
        if (freeze && !frozen) ++freezes;
        frozen=freeze;
        captured|=robo.fenrir_captured;
        if (second && menu.hud_stats[0].hits<5) throw std::runtime_error("The combo dropped between supers");
        if (freeze && !pause_checked) {
            const auto before=filia.blockbuster_playback->record;
            const auto stage_tick=menu.practice_stage_tick;
            const auto presentation_tick=menu.combat_presentation.tick;
            menu.practice_paused=true;
            for (unsigned i=0;i<10;++i) menu.practice_tick();
            check(filia.blockbuster_playback->record==before && menu.practice_stage_tick==stage_tick &&
                menu.combat_presentation.tick==presentation_tick,"Pause freezes combat, camera presentation and effects together");
            menu.practice_paused=false;pause_checked=true;
        }
        if (freeze && second && !snapshot_checked) {
            const auto record=filia.blockbuster_playback->record;
            const auto ticks=filia.blockbuster_playback->ticks;
            const auto presentation=menu.combat_presentation.tick;
            menu.save_or_restore_practice(false);
            for (unsigned i=0;i<12;++i) menu.practice_tick();
            menu.save_or_restore_practice(true);
            check(filia.blockbuster_playback->record==record && filia.blockbuster_playback->ticks==ticks &&
                menu.combat_presentation.tick==presentation,"Save states restore an independent live super timeline and cut-in clock");
            snapshot_checked=true;
        }
#ifdef _WIN32
        if (!output.empty() && tick%3==0) {
            Gdiplus::Bitmap canvas(1280,720,PixelFormat32bppPARGB);
            { Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color(255,0,0,0));menu.paint_practice(dc); }
            CLSID png;
            CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
            auto file=output/("frame-"+std::to_string(1000+tick/3)+".png");
            check(canvas.Save(file.c_str(),&png)==Gdiplus::Ok,"Render reference-sequence preview");
        }
#endif
        if (menu.hud_stats[0].hits != old_hits) {
            old_hits = menu.hud_stats[0].hits;
        }
    }
    if (!captured || freezes!=2) std::cerr<<"Sequence hits="<<menu.hud_stats[0].hits<<" freezes="<<freezes<<" captured="<<captured<<" damage="<<menu.hud_stats[0].damage<<"\n";
    check(captured && freezes==2,"Both super freezes and Fenrir capture occur during the sequence");
    check(second, "Gregor recovers for the Fenrir follow-up");
    check(menu.hud_stats[0].hits==15,"Gregor into Fenrir reproduces the reference clip's 15 hits");
    check(menu.practice_players[0].tension==3,"The two level-one supers spend two bars");
    check(menu.practice_players[0].active_normal<0 && !menu.practice_players[0].blockbuster_playback,"Filia recovers after the slam");
    check(!menu.practice_players[1].fenrir_captured && menu.practice_players[1].hurt_group<0,"Robo-Fortune is released and gets up");
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    if (argc < 2 || argc > 3) return 2;
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR graphics = 0;
    if (Gdiplus::GdiplusStartup(&graphics, &input, nullptr) != Gdiplus::Ok) return 2;
    int result = 0;
    try {
        Menu menu;
        menu.game_data = argv[1];
        setup(menu);
#ifdef _WIN32
        combat_art_cache(menu);
        direct_composition(menu);
#endif
        const auto output=argc==3?std::filesystem::path(argv[2]):std::filesystem::path{};
        if (!output.empty()) {
            std::filesystem::create_directories(output);
            menu.practice_background.reset();
            menu.practice_scene=std::make_unique<PracticeScene>(menu.game_data,"class_notes_3d",
                3750,2000,menu.practice_floor,66,200,24000);
            // Keep the compact combat fixture, but preserve the authored scenery proportions.
            for(auto& mesh:menu.practice_scene->meshes) for(auto& vertex:mesh.vertices)
                vertex.x+=(menu.practice_width-3750)*0.5f;
            menu.practice_scene->initialize_gpu(nullptr);
        }
        robo_commands(menu);
        sequence(menu,output);
        menu.reset_practice();
        std::swap(menu.practice_players[0].practice_x,menu.practice_players[1].practice_x);
        menu.update_practice_camera(true);
        sequence(menu,{},true);
        std::cout << "Filia/Robo-Fortune 15-hit sequence, both facings, two meter spends, capture/release, recovery, pause and presentation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    Gdiplus::GdiplusShutdown(graphics);
    return result;
}
