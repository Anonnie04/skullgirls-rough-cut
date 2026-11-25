#define NOMINMAX
#include <windows.h>
#include "sprite_gpu.hpp"
#include <iostream>
#include <memory>
#include <cmath>

int main() {
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;
    if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return 1;
    int result=0;
    {
        try {
            std::unique_ptr<skullgirls::SpriteGpu> gpu;
            try {gpu=std::make_unique<skullgirls::SpriteGpu>();}
            catch(const skullgirls::SpriteGpu::Unavailable& e){std::cout<<"No hardware sprite renderer: "<<e.what()<<'\n';result=77;}
            if(gpu) {
                Gdiplus::Bitmap source(64,64,PixelFormat32bppPARGB);
                Gdiplus::BitmapData data{};Gdiplus::Rect region(0,0,64,64);
                source.LockBits(&region,Gdiplus::ImageLockModeWrite,PixelFormat32bppPARGB,&data);
                for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
                    const unsigned a=x<2||y<2||x>61||y>61?0:((x+y)%7<3?128:220);
                    auto* row=reinterpret_cast<unsigned*>(static_cast<BYTE*>(data.Scan0)+y*data.Stride);
                    row[x]=a<<24|((x*3*a+127)/255)<<16|((y*3*a+127)/255)<<8|(96*a+127)/255;
                }
                source.UnlockBits(&data);
                Gdiplus::Bitmap actual(1280,720,PixelFormat32bppPARGB),expected(1280,720,PixelFormat32bppPARGB),fallback(1280,720,PixelFormat32bppPARGB);
                Gdiplus::Graphics draw(&actual),reference(&expected),replay(&fallback);
                for(auto* dc:{&draw,&reference,&replay}) {
                    dc->Clear(Gdiplus::Color(255,24,36,48));
                    dc->SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
                    dc->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                }
                skullgirls::SpriteGpu::Texture texture;gpu->begin();
                for(unsigned i=0;i<8;++i) {
                    const std::array<float,4> tint=i==0?std::array<float,4>{0,0,0,0.55f}:
                        i==7?std::array<float,4>{1,1,1,1}:std::array<float,4>{0.65f,1,0.26f,0.4f};
                    const auto state=draw.Save(),other=reference.Save();
                    for(auto* dc:{&draw,&reference}) {
                        dc->TranslateTransform(330-i*16.5f,240+i*7.25f);
                        dc->RotateTransform(i*2.5f);dc->ScaleTransform(i%2?-2.0f:2.0f,i==0?-0.46f:2.0f);
                    }
                    const Gdiplus::RectF bounds(-32,-64,64,64);
                    const std::array<float,4> uv=i==6?std::array<float,4>{0.125f,0.125f,0.625f,0.875f}:
                        std::array<float,4>{0,0,1,1};
                    gpu->queue(draw,source,bounds,tint,texture,uv);
                    Gdiplus::ColorMatrix color{};
                    color.m[0][0]=tint[0];color.m[1][1]=tint[1];color.m[2][2]=tint[2];color.m[3][3]=tint[3];color.m[4][4]=1;
                    Gdiplus::ImageAttributes attributes;attributes.SetColorMatrix(&color);
                    reference.DrawImage(&source,bounds,uv[0]*64,uv[1]*64,(uv[2]-uv[0])*64,(uv[3]-uv[1])*64,Gdiplus::UnitPixel,&attributes);
                    draw.Restore(state);reference.Restore(other);
                }
                gpu->finish(draw);gpu->fallback(replay);
                double error=0,fallback_error=0;unsigned samples=0,painted=0;
                for(int y=60;y<380;++y)for(int x=100;x<460;++x) {
                    Gdiplus::Color a,b,c;actual.GetPixel(x,y,&a);expected.GetPixel(x,y,&b);fallback.GetPixel(x,y,&c);
                    painted+=a.GetValue()!=0xff182430;
                    for(unsigned shift:{0u,8u,16u,24u}) {
                        const auto channel=[&](Gdiplus::Color color){return int((color.GetValue()>>shift)&255);};
                        error+=std::abs(channel(a)-channel(b));fallback_error+=std::abs(channel(c)-channel(b));++samples;
                    }
                }
                if(painted<5000 || error/samples>3 || fallback_error/samples>0.1)
                    throw std::runtime_error("GPU sprite transforms, alpha, draw order or fallback differ from the CPU reference");
                // Compose a complete GPU frame with a CPU overlay, and verify
                // that no download occurs until the explicit capture request.
                std::vector<unsigned> overlay_pixels(1280*720);
                Gdiplus::Bitmap overlay_image(1280,720,1280*4,PixelFormat32bppPARGB,
                    reinterpret_cast<BYTE*>(overlay_pixels.data()));
                Gdiplus::Graphics overlay(&overlay_image);overlay.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                const auto before_downloads=gpu->readback_count();
                gpu->begin_frame(nullptr,reinterpret_cast<const BYTE*>(overlay_pixels.data()),1280*4);
                overlay.Clear(Gdiplus::Color(255,24,36,48));gpu->flush_overlay(overlay);gpu->begin();
                for(unsigned i=0;i<8;++i) {
                    const auto state=overlay.Save();overlay.TranslateTransform(330-i*16.5f,240+i*7.25f);
                    overlay.RotateTransform(i*2.5f);overlay.ScaleTransform(i%2?-2.0f:2.0f,i==0?-0.46f:2.0f);
                    const std::array<float,4> tint=i==0?std::array<float,4>{0,0,0,0.55f}:
                        i==7?std::array<float,4>{1,1,1,1}:std::array<float,4>{0.65f,1,0.26f,0.4f};
                    const std::array<float,4> uv=i==6?std::array<float,4>{0.125f,0.125f,0.625f,0.875f}:
                        std::array<float,4>{0,0,1,1};
                    gpu->queue(overlay,source,{-32,-64,64,64},tint,texture,uv);overlay.Restore(state);
                }
                gpu->finish(overlay);
                Gdiplus::SolidBrush hud(Gdiplus::Color(150,225,80,120));
                overlay.FillRectangle(&hud,130,80,280,40);reference.FillRectangle(&hud,130,80,280,40);
                gpu->flush_overlay(overlay);
                if(gpu->readback_count()!=before_downloads)throw std::runtime_error("GPU frame composition performed a CPU readback");
                auto& captured=gpu->capture_frame();double frame_error=0;unsigned frame_samples=0;
                for(int y=60;y<380;y+=2)for(int x=100;x<460;x+=2) {
                    Gdiplus::Color a,b;captured.GetPixel(x,y,&a);expected.GetPixel(x,y,&b);
                    for(unsigned shift:{0u,8u,16u,24u}) {
                        frame_error+=std::abs(int((a.GetValue()>>shift)&255)-int((b.GetValue()>>shift)&255));++frame_samples;
                    }
                }
                if(frame_error/frame_samples>3)throw std::runtime_error("GPU frame composition reordered or lost its CPU overlay");
                gpu->abort_frame();
                const auto portrait=skullgirls::SpriteGpu::fitted_viewport(960,720);
                const auto wide=skullgirls::SpriteGpu::fitted_viewport(1600,720);
                if(portrait.TopLeftY!=90 || portrait.Width!=960 || portrait.Height!=540 ||
                   wide.TopLeftX!=160 || wide.Width!=1280 || wide.Height!=720)
                    throw std::runtime_error("Presentation viewport stretched the frame");
                // Exercise the real swap chain across resize and minimize/restore.
                const auto window=CreateWindowExW(0,L"STATIC",L"GPU presentation test",WS_OVERLAPPEDWINDOW,
                    0,0,960,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
                if(!window)throw std::runtime_error("Cannot create presentation test window");
                ShowWindow(window,SW_SHOWNOACTIVATE);
                try {
                    for(unsigned frame=0;frame<4;++frame) {
                        if(frame==1)SetWindowPos(window,nullptr,0,0,1100,700,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
                        if(frame==2) {
                            ShowWindow(window,SW_MINIMIZE);gpu->mark_occluded();
                            ShowWindow(window,SW_SHOWNOACTIVATE);
                        }
                        if(!gpu->begin_frame(window,reinterpret_cast<const BYTE*>(overlay_pixels.data()),1280*4))
                            throw std::runtime_error("Restored presentation remained occluded");
                        overlay.Clear(Gdiplus::Color(255,24,36,48));gpu->present(overlay);
                        RECT client{};GetClientRect(window,&client);
                        if(gpu->back_buffer_size()!=std::array<unsigned,2>{unsigned(client.right),unsigned(client.bottom)})
                            throw std::runtime_error("Swap chain did not resize with its window");
                    }
                    if(gpu->readback_count()!=before_downloads+1 || gpu->present_count()!=4)
                        throw std::runtime_error("Presentation downloaded pixels or lost frames");
                } catch(...) {gpu->detach_window();DestroyWindow(window);throw;}
                gpu->detach_window();DestroyWindow(window);

                auto* uploaded=texture.view.Get();gpu->begin();
                gpu->queue(draw,source,{0,0,64,64},{1,1,1,1},texture);
                if(texture.view.Get()!=uploaded)throw std::runtime_error("Sprite texture was uploaded again");
                gpu->begin();
                auto temporary=std::make_shared<Gdiplus::Bitmap>(8,8,PixelFormat32bppPARGB);
                std::weak_ptr<Gdiplus::Bitmap> lifetime=temporary;
                {Gdiplus::Graphics fill(temporary.get());fill.Clear(Gdiplus::Color(255,255,0,0));}
                skullgirls::SpriteGpu::Texture temporary_texture;
                gpu->queue(draw,*temporary,{0,0,8,8},{1,1,1,1},temporary_texture,{0,0,1,1},temporary);
                temporary.reset();
                if(lifetime.expired())throw std::runtime_error("Deferred sprite lost its CPU fallback pixels");
                gpu->fallback(replay);gpu->begin();
                if(!lifetime.expired())throw std::runtime_error("Finished sprite batch retained old CPU pixels");
                std::cout<<"GPU sprites: mirrored/rotated transforms, premultiplied alpha, shadows, six trails, ordering, fallback and texture reuse passed; mean channel error "<<error/samples<<'\n';
            }
        } catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}
    }
    Gdiplus::GdiplusShutdown(token);return result;
}
