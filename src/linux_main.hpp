// Native Linux entry point, sharing the Windows preview's menu and simulation.
#ifndef SKULLGIRLS_NO_MAIN
int main(int argc,char** argv) {
    try {
        bool smoke=false, audio_smoke=false, no_audio=false;
        std::filesystem::path requested;
        for(int i=1;i<argc;++i) {
            if(std::string_view(argv[i])=="--smoke-test") smoke=true;
            else if(std::string_view(argv[i])=="--audio-smoke-test") audio_smoke=true;
            else if(std::string_view(argv[i])=="--no-audio") no_audio=true;
            else if(requested.empty()) requested=argv[i];
            else throw std::runtime_error("Usage: skullgirls-main-menu [game data folder] [--smoke-test] [--audio-smoke-test] [--no-audio]");
        }
        const auto data=skullgirls::resolve_runtime_data(std::filesystem::read_symlink("/proc/self/exe"),requested);
        if (audio_smoke) { skullgirls::run_audio_smoke_test(data); return 0; }
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_TIMER)!=0) throw std::runtime_error(SDL_GetError());
        struct SdlRuntime { ~SdlRuntime(){ SDL_Quit(); } } sdl;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
        using Window=std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)>;
        Window window(SDL_CreateWindow("Skullgirls: Rough Cut",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
            1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|(smoke?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN)),SDL_DestroyWindow);
        if(!window) throw std::runtime_error(SDL_GetError());
        auto context=SDL_GL_CreateContext(window.get());
        if(!context) throw std::runtime_error(SDL_GetError());
        struct Context { SDL_GLContext value; ~Context(){SDL_GL_DeleteContext(value);} } context_owner{context};
        SDL_GL_SetSwapInterval(0);
        std::fprintf(stderr,"Linux backend: SDL %d.%d.%d, OpenGL %s\n",SDL_MAJOR_VERSION,SDL_MINOR_VERSION,SDL_PATCHLEVEL,glGetString(GL_VERSION));
        auto menu=std::make_unique<Menu>();
        menu_instance=menu.get();
        menu->load(data);
        if (!no_audio) {
            try { menu->audio=skullgirls::create_linux_audio(data); menu->audio->music("menu_intro","menu_loop"); }
            catch(const std::exception& error) { menu->audio.reset(); std::fprintf(stderr,"Audio unavailable: %s\n",error.what()); }
        }
        std::fprintf(stderr,"Loaded game archives from %s\n",data.c_str());
        LinuxDib canvas{1280,720,std::vector<unsigned>(1280*720)};
        LinuxDc dc{&canvas};
        GLuint screen=0; glGenTextures(1,&screen);
        struct Screen { GLuint value; ~Screen(){glDeleteTextures(1,&value);} } screen_owner{screen};
        glBindTexture(GL_TEXTURE_2D,screen);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1280,720,0,GL_BGRA,GL_UNSIGNED_BYTE,nullptr);
        const auto present=[&] {
            std::fill(canvas.pixels.begin(),canvas.pixels.end(),0xff000000);
            menu->paint(&dc);
            int w,h; SDL_GL_GetDrawableSize(window.get(),&w,&h);
            glViewport(0,0,w,h); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
            const int width=std::min(w,h*16/9),height=width*9/16;
            glViewport((w-width)/2,(h-height)/2,width,height);
            glDisable(GL_DEPTH_TEST); glDisable(GL_ALPHA_TEST); glDisable(GL_BLEND);
            glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,screen);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1280,720,GL_BGRA,GL_UNSIGNED_BYTE,canvas.pixels.data());
            glMatrixMode(GL_PROJECTION); glLoadIdentity(); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
            glColor4f(1,1,1,1); glBegin(GL_QUADS);
            glTexCoord2f(0,1);glVertex2f(-1,-1);glTexCoord2f(1,1);glVertex2f(1,-1);
            glTexCoord2f(1,0);glVertex2f(1,1);glTexCoord2f(0,0);glVertex2f(-1,1);glEnd();
            SDL_GL_SwapWindow(window.get());
        };
        const auto key=[](SDL_Keycode code)->unsigned {
            if(code>=SDLK_a && code<=SDLK_z) return 'A'+code-SDLK_a;
            switch(code) {
            case SDLK_LEFT:return VK_LEFT;case SDLK_RIGHT:return VK_RIGHT;case SDLK_UP:return VK_UP;case SDLK_DOWN:return VK_DOWN;
            case SDLK_RETURN:return VK_RETURN;case SDLK_ESCAPE:return VK_ESCAPE;case SDLK_BACKSPACE:return VK_BACK;
            case SDLK_F6:return VK_F6;case SDLK_F7:return VK_F7;case SDLK_F8:return VK_F8;case SDLK_F9:return VK_F9;
            default:return code>0 && code<128 ? unsigned(code) : 0;
            }
        };
        using Clock=std::chrono::steady_clock;
        constexpr auto period=std::chrono::nanoseconds(16666667);
        menu->animation_start=Clock::now(); auto deadline=Clock::now();
        PracticeCadence cadence; unsigned skipped=0;
        if(smoke) {
            present();
            Gdiplus::Bitmap screenshot(1280,720,1280*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(canvas.pixels.data()));
            const auto capture=[&](const char* name) {
                cairo_surface_mark_dirty(screenshot.surface);
                if(cairo_surface_write_to_png(screenshot.surface,name)!=CAIRO_STATUS_SUCCESS)
                    throw std::runtime_error("Cannot write smoke-test screenshot");
            };
            capture("linux-title.png");
            window_proc(window.get(),WM_KEYDOWN,key(SDLK_SPACE),0);
            if(menu->title_visible) throw std::runtime_error("Title screen keyboard input failed");
            menu->menu_started=Clock::now()-std::chrono::seconds(3); present();
            capture("linux-menu.png");
            const auto fighter=std::find(roster.begin(),roster.end(),std::string_view("filia"))-roster.begin();
            menu->fighters[0]=menu->fighters[1]={int(fighter)};
            menu->locked_colors[0]=menu->locked_colors[1]={0};
            menu->training=Menu::TrainingView::fighters; present(); capture("linux-select.png");
            // Complete P2's final fighter pick through the real menu input path.
            menu->player=1; menu->team_size[1]=1;
            menu->fighters[1].clear(); menu->locked_colors[1].clear();
            menu->fighter_cursor=int(fighter);
            menu->training_input('A',true,false);
            menu->stage_entered-=std::chrono::seconds(1);
            if(menu->training!=Menu::TrainingView::stage_select || !menu->stage_selection.random() || menu->stage_selection_name()!=L"Random")
                throw std::runtime_error("Stage select must start on Random");
            present(); capture("linux-stage-random.png");
            menu->training_input(VK_LEFT,false,false);
            menu->training_input(VK_RIGHT,false,false);
            if(!menu->stage_selection.random()) throw std::runtime_error("Stage selection failed to wrap to Random");
            menu->training_input(VK_RIGHT,false,false);
            menu->stage_switched-=std::chrono::seconds(1);
            present(); capture("linux-stage-selected.png");
            for(unsigned i=0;i<menu->stage_groups.size() && menu->current_stage().id!="Arena";++i)
                menu->training_input(VK_RIGHT,false,false);
            if(menu->current_stage().id!="Arena") throw std::runtime_error("Arena stage group is missing");
            menu->training_input(VK_DOWN,false,false);
            if(menu->stage_selection.variant!=1) throw std::runtime_error("Stage alternate input failed");
            menu->stage_switched-=std::chrono::seconds(1);
            present(); capture("linux-stage-alternate.png");
            for(const auto index:menu->random_stages)
                if(menu->stages[index].id=="Gehenna" || menu->stages[index].id=="Arena_Evo" || menu->stages[index].id=="Arena_SGCS")
                    throw std::runtime_error("Random includes an excluded stage");
            menu->enter_stage_select();
            menu->confirm_stage_selection();
            const auto random_stage=menu->stage_selection.resolved;
            menu->confirm_stage_selection();
            if(menu->stage_selection.resolved!=random_stage) throw std::runtime_error("Random stage rerolled on confirmation");
            menu->training_input(VK_ESCAPE,false,true);
            if(menu->training!=Menu::TrainingView::fighters) throw std::runtime_error("Stage cancel did not return to fighter select");
            menu->training_input('A',true,false);
            if(!menu->stage_selection.random() || menu->stage_selection.resolved)
                throw std::runtime_error("Reentering stage select did not reset to Random");
            // Resolve Random to New Meridian for a reproducible render regression.
            const auto eligible=menu->random_stages;
            menu->random_stages={menu->stage_groups.front().front()};
            menu->training_input(VK_RETURN,false,false);
            menu->random_stages=eligible;
            if(!menu->practice_load.valid()) throw std::runtime_error("Confirming Random did not start the match");
            menu->finish_practice(window.get());
            for(int i=0;i<120;++i) menu->practice_tick();
            present(); capture("linux-training.png");
            unsigned portrait_pixels=0;
            for(unsigned y=30;y<100;++y) for(unsigned x=80;x<125;++x) {
                const auto pixel=canvas.pixels[y*1280+x];
                const auto r=(pixel>>16)&255,g=(pixel>>8)&255,b=pixel&255;
                if(std::max({r,g,b})-std::min({r,g,b})>10) ++portrait_pixels;
            }
            if(portrait_pixels<100) throw std::runtime_error("Training portrait was obscured by its background");
            if(menu->practice_scene && !menu->practice_scene->gpu)
                throw std::runtime_error("Smoke test expected OpenGL stage rendering");
            SDL_SetWindowSize(window.get(),960,540);
            for(int i=0;i<30;++i) menu->practice_tick();
            present(); capture("linux-training-resized.png");
            menu->practice_input(VK_RETURN,false,false); present(); capture("linux-pause.png");
            if(!menu->practice_paused) throw std::runtime_error("Pause input failed");
            menu->practice_paused=false;
            for(unsigned count : {1u,2u,3u}) {
                for(unsigned side=0;side<2;++side) {
                    menu->fighters[side].assign(count,int(fighter));
                    menu->locked_colors[side].resize(count);
                    for(unsigned slot=0;slot<count;++slot) menu->locked_colors[side][slot]=slot+side;
                }
                menu->prepare_practice_bitmaps();
                menu->prepare_hud_portraits();
                menu->reset_practice();
                for(unsigned side=0;side<2;++side) {
                    menu->hud_stats[side].life=4000;
                    menu->hud_stats[side].displayed_life=5000;
                    menu->hud_stats[side].damage_trail=9000;
                    for(unsigned slot=0;slot<count;++slot)
                        menu->practice_teams[side].health[slot].reset_life(15500-slot*4000);
                }
                present();
                const auto name="linux-hud-"+std::to_string(count)+".png";
                capture(name.c_str());
            }
            std::fprintf(stderr,"Linux smoke test passed: title, menu, character select, training, and pause rendered.\n");
            return 0;
        }
        while(linux_running) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_QUIT) linux_running=false;
                else if(event.type==SDL_KEYDOWN || event.type==SDL_KEYUP) {
                    if(auto mapped=key(event.key.keysym.sym)) window_proc(window.get(),event.type==SDL_KEYDOWN?WM_KEYDOWN:WM_KEYUP,mapped,event.key.repeat?(1L<<30):0);
                } else if(event.type==SDL_MOUSEBUTTONDOWN) window_proc(window.get(),WM_LBUTTONDOWN,0,0);
                else if(event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_FOCUS_LOST) window_proc(window.get(),WM_KILLFOCUS,0,0);
            }
            const auto now=Clock::now();
            if(now<deadline) { SDL_Delay(1); continue; }
            if(now-deadline>4*period) deadline=now;
            deadline+=period;
            if(menu->practice_load.valid() && menu->practice_load.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
                try { menu->finish_practice(window.get()); SetWindowTextW(window.get(),menu->local_versus?L"Skullgirls - Local Versus":L"Skullgirls - Training Room"); }
                catch(const std::exception& error) { menu->training=Menu::TrainingView::stage_select; MessageBoxA(window.get(),error.what(),"Training Room",0); }
                deadline=Clock::now()+period;
            }
            if (menu->audio) {
                menu->audio->pause_combat(menu->practice_paused);
                menu->audio->tick();
            }
            menu->story_tick();
            const bool practicing=menu->training==Menu::TrainingView::practice && !menu->practice_paused;
            if(practicing && cadence.update_due(menu->practice_speed())) menu->practice_tick();
            if(!practicing) { cadence={}; skipped=0; }
            if(!(SDL_GetWindowFlags(window.get())&SDL_WINDOW_MINIMIZED) &&
                (!practicing || render_practice_frame(Clock::now(),deadline,skipped))) present();
        }
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr,"Skullgirls: Rough Cut: %s\n",error.what()); return 1;
    }
}
#endif
