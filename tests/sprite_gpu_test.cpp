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
