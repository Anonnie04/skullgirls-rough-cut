#pragma once

// Native SDL/Cairo adapter for the preview's existing drawing vocabulary.
// No Wine or Windows libraries are used by this backend.
#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <cairo/cairo.h>
#include <algorithm>
#include <cmath>
#include <codecvt>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <locale>
#include <memory>
#include <stdexcept>
#include <string>
#include <strings.h>
#include <cwchar>
#include <vector>

using BYTE = unsigned char;
using UINT = unsigned;
using WPARAM = unsigned;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
using ULONG_PTR = std::uintptr_t;
using HWND = SDL_Window*;
#define CALLBACK
constexpr int FALSE = 0, MB_OK = 0, MB_ICONERROR = 0, CP_UTF8 = 65001;
constexpr int TRUE=1, CSTR_EQUAL=2;
inline int _stricmp(const char* a,const char* b) { return strcasecmp(a,b); }
inline int CompareStringOrdinal(const wchar_t* a,int,const wchar_t* b,int,int ignore_case) {
    return (ignore_case?wcscasecmp(a,b):wcscmp(a,b))==0 ? CSTR_EQUAL : 0;
}
constexpr int VK_BACK=8, VK_RETURN=13, VK_SHIFT=16, VK_ESCAPE=27;
constexpr int VK_LEFT=37, VK_UP=38, VK_RIGHT=39, VK_DOWN=40;
constexpr int VK_F6=117, VK_F7=118, VK_F8=119, VK_F9=120;
constexpr unsigned WM_LBUTTONDOWN=1, WM_KILLFOCUS=2, WM_KEYUP=3, WM_KEYDOWN=4, WM_DESTROY=5;
constexpr int RDW_INVALIDATE=0, RDW_UPDATENOW=0;
inline bool linux_running = true;
inline std::string utf8(const std::wstring& value) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(value);
}
inline int MultiByteToWideChar(int, int, const char* text, int length, wchar_t* output, int capacity) {
    const auto wide = std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(text, text+length);
    if (output) std::copy_n(wide.data(), std::min<int>(capacity, wide.size()), output);
    return wide.size();
}
inline void OutputDebugStringA(const char* text) { std::fputs(text, stderr); }
inline int MessageBoxA(HWND window, const char* text, const char* title, int) {
    std::fprintf(stderr, "%s: %s\n", title, text);
    return SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, window);
}
inline void SetWindowTextW(HWND window, const wchar_t* text) { SDL_SetWindowTitle(window, utf8(text).c_str()); }
inline void InvalidateRect(HWND, void*, int) {}
inline void RedrawWindow(HWND, void*, void*, int) {}
inline void DestroyWindow(HWND) { linux_running=false; }
inline void PostQuitMessage(int) { linux_running=false; }
inline LRESULT DefWindowProcW(HWND, UINT, WPARAM, LPARAM) { return 0; }
inline int GetKeyState(int key) { return key==VK_SHIFT && (SDL_GetModState() & KMOD_SHIFT) ? -1 : 0; }

struct LinuxDib { int width, height; std::vector<unsigned> pixels; };
using HBITMAP = LinuxDib*;
using HGDIOBJ = LinuxDib*;
struct LinuxDc { LinuxDib* bitmap=nullptr; };
using HDC = LinuxDc*;
struct BITMAPINFOHEADER { unsigned biSize=0; int biWidth=0, biHeight=0; unsigned biPlanes=0, biBitCount=0; };
struct BITMAPINFO { BITMAPINFOHEADER bmiHeader; };
struct DIBSECTION { struct { void* bmBits=nullptr; int bmWidthBytes=0; } dsBm; };
constexpr int DIB_RGB_COLORS=0;
inline HDC CreateCompatibleDC(void*) { return new LinuxDc; }
inline HBITMAP CreateDIBSection(HDC, const BITMAPINFO* info, int, void** data, void*, int) {
    auto* image=new LinuxDib{info->bmiHeader.biWidth, std::abs(info->bmiHeader.biHeight), {}};
    image->pixels.resize(std::size_t(image->width)*image->height);
    *data=image->pixels.data(); return image;
}
inline HGDIOBJ SelectObject(HDC dc,HBITMAP bitmap) { auto* old=dc->bitmap; dc->bitmap=bitmap; return old; }
inline void DeleteObject(HBITMAP bitmap) { delete bitmap; }
inline void DeleteDC(HDC dc) { delete dc; }
inline int GetObjectW(HBITMAP bitmap, int, DIBSECTION* section) {
    if (!bitmap) return 0;
    section->dsBm.bmBits=bitmap->pixels.data(); section->dsBm.bmWidthBytes=bitmap->width*4; return 1;
}
constexpr int PixelFormat32bppPARGB=0, PixelFormat32bppARGB=1;

namespace Gdiplus {
constexpr int Ok=0, UnitPixel=0, WrapModeTileFlipXY=0, FontStyleRegular=0;
constexpr int InterpolationModeBilinear=0, PixelOffsetModeHalf=0, SmoothingModeAntiAlias=0;
constexpr int CombineModeIntersect=0, ImageLockModeWrite=0, CompositingModeSourceCopy=1;
// Cairo requires no process-wide graphics startup; retained for shared tests.
struct GdiplusStartupInput {};
inline int GdiplusStartup(ULONG_PTR* token,GdiplusStartupInput*,void*) { *token=1; return Ok; }
inline void GdiplusShutdown(ULONG_PTR) {}
using ARGB=std::uint32_t;
struct Color {
    ARGB value;
    constexpr Color(ARGB v=0) : value(v) {}
    constexpr Color(unsigned a,unsigned r,unsigned g,unsigned b):value((a<<24)|(r<<16)|(g<<8)|b){}
    constexpr Color(unsigned r,unsigned g,unsigned b):Color(255,r,g,b){}
    unsigned GetR() const { return (value>>16)&255; }
    unsigned GetG() const { return (value>>8)&255; }
    unsigned GetB() const { return value&255; }
    unsigned GetAlpha() const { return value>>24; }
    static constexpr ARGB Black=0xff000000, White=0xffffffff, Transparent=0;
};
struct RectF {
    float X,Y,Width,Height;
    RectF(float x=0,float y=0,float w=0,float h=0):X(x),Y(y),Width(w),Height(h){}
    void Offset(float x,float y) { X+=x; Y+=y; }
};
struct Rect { int X,Y,Width,Height; Rect(int x,int y,int w,int h):X(x),Y(y),Width(w),Height(h){} };
struct PointF { float X=0,Y=0; };
struct ColorMatrix { float m[5][5]{}; };
struct ImageAttributes {
    ColorMatrix matrix{}; bool colored=false;
    void SetColorMatrix(const ColorMatrix* value) { matrix=*value; colored=true; }
    void SetWrapMode(int) {}
};
struct BitmapData { void* Scan0=nullptr; int Stride=0; };
class Bitmap {
    std::vector<unsigned char> owned;
    bool straight=false;
public:
    int width,height,stride;
    unsigned char* bytes;
    cairo_surface_t* surface=nullptr;
    Bitmap(int w,int h,int format):owned(std::size_t(w)*h*4),straight(format==PixelFormat32bppARGB),
        width(w),height(h),stride(w*4),bytes(owned.data()) { init(); }
    Bitmap(int w,int h,int pitch,int format,BYTE* data):straight(format==PixelFormat32bppARGB),
        width(w),height(h),stride(pitch),bytes(data) { init(); }
    Bitmap(const Bitmap&)=delete;
    void init() {
        surface=cairo_image_surface_create_for_data(bytes,CAIRO_FORMAT_ARGB32,width,height,stride);
        if(cairo_surface_status(surface)!=CAIRO_STATUS_SUCCESS) throw std::runtime_error("Cannot allocate Cairo bitmap");
    }
    ~Bitmap() { cairo_surface_destroy(surface); }
    int GetLastStatus() const { return Ok; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    int GetPixel(int x,int y,Color* color) {
        if(x<0 || y<0 || x>=width || y>=height) return 1;
        cairo_surface_flush(surface);
        const auto* p=bytes+y*stride+x*4;
        const auto a=p[3];
        *color=Color(a,a?std::min(255u,unsigned(p[2])*255/a):0,
            a?std::min(255u,unsigned(p[1])*255/a):0,a?std::min(255u,unsigned(p[0])*255/a):0);
        return Ok;
    }
    int LockBits(Rect* rect,int,int,BitmapData* data) {
        cairo_surface_flush(surface); data->Stride=stride; data->Scan0=bytes+rect->Y*stride+rect->X*4; return Ok;
    }
    void UnlockBits(BitmapData*) {
        if(straight) {
            for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
                auto* p=bytes+y*stride+x*4;
                for(int c=0;c<3;++c) p[c]=unsigned(p[c])*p[3]/255;
            }
        }
        cairo_surface_mark_dirty(surface);
    }
};
struct SolidBrush { Color color; explicit SolidBrush(Color c):color(c){} };
struct Pen { Color color; float width; explicit Pen(Color c,float w=1):color(c),width(w){} };
enum StringAlignment { StringAlignmentNear, StringAlignmentCenter, StringAlignmentFar };
struct StringFormat { StringAlignment alignment=StringAlignmentNear; void SetAlignment(StringAlignment a){alignment=a;} };
struct Font { float size; Font(const wchar_t*,float s,int,int):size(s){} };
class Graphics {
    std::unique_ptr<Bitmap> view;
    cairo_t* context;
    int composition=0, depth=0;
    void source(Color c) { cairo_set_source_rgba(context,c.GetR()/255.,c.GetG()/255.,c.GetB()/255.,(c.value>>24)/255.); }
    void rectangle(RectF r) { cairo_rectangle(context,r.X,r.Y,r.Width,r.Height); }
    void ellipse(RectF r) {
        cairo_save(context); cairo_translate(context,r.X+r.Width/2,r.Y+r.Height/2);
        cairo_scale(context,r.Width/2,r.Height/2); cairo_arc(context,0,0,1,0,6.28318530718); cairo_restore(context);
    }
public:
    explicit Graphics(Bitmap* b):context(cairo_create(b->surface)) { cairo_surface_mark_dirty(b->surface); }
    explicit Graphics(HDC dc):view(std::make_unique<Bitmap>(dc->bitmap->width,dc->bitmap->height,
        dc->bitmap->width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(dc->bitmap->pixels.data()))),
        context(cairo_create(view->surface)){}
    ~Graphics() { cairo_surface_flush(cairo_get_target(context)); cairo_destroy(context); }
    int Save() { cairo_save(context); return ++depth; }
    void Restore(int state) { while(depth>=state && depth>0) { cairo_restore(context); --depth; } }
    void TranslateTransform(float x,float y) { cairo_translate(context,x,y); }
    void ScaleTransform(float x,float y) { cairo_scale(context,x,y); }
    void RotateTransform(float degrees) { cairo_rotate(context,degrees*3.141592653589793/180); }
    void SetInterpolationMode(int) {}
    void SetPixelOffsetMode(int) {}
    void SetSmoothingMode(int) {}
    void SetClip(RectF r,int=0) { rectangle(r); cairo_clip(context); }
    int GetCompositingMode() const { return composition; }
    void SetCompositingMode(int mode) { composition=mode; cairo_set_operator(context,mode==CompositingModeSourceCopy?CAIRO_OPERATOR_SOURCE:CAIRO_OPERATOR_OVER); }
    void Clear(Color color) { cairo_save(context); cairo_set_operator(context,CAIRO_OPERATOR_SOURCE); source(color); cairo_paint(context); cairo_restore(context); }
    int DrawImage(Bitmap* bitmap,RectF bounds,float sx,float sy,float sw,float sh,int,ImageAttributes* attr=nullptr) {
        if(!bitmap || sw<=0 || sh<=0 || bounds.Width==0 || bounds.Height==0) return Ok;
        cairo_surface_mark_dirty(bitmap->surface);
        cairo_surface_t* surface=bitmap->surface;
        std::unique_ptr<Bitmap> tinted;
        float opacity=1;
        if(attr && attr->colored) {
            const auto& m=attr->matrix.m;
            opacity=std::clamp(m[3][3],0.f,1.f);
            if(m[0][0]!=1 || m[1][1]!=1 || m[2][2]!=1 || m[4][0]!=0 || m[4][1]!=0 || m[4][2]!=0) {
                tinted=std::make_unique<Bitmap>(bitmap->width,bitmap->height,PixelFormat32bppPARGB);
                for(int y=0;y<bitmap->height;++y) for(int x=0;x<bitmap->width;++x) {
                    auto* in=bitmap->bytes+y*bitmap->stride+x*4;
                    auto* out=tinted->bytes+y*tinted->stride+x*4;
                    float a=in[3]/255.f;
                    float channels[5]={a?in[2]/(255.f*a):0,a?in[1]/(255.f*a):0,a?in[0]/(255.f*a):0,a,1};
                    for(int c=0;c<3;++c) {
                        float v=0; for(int k=0;k<5;++k) v+=channels[k]*m[k][c];
                        out[2-c]=std::clamp(v,0.f,1.f)*a*255;
                    }
                    out[3]=in[3];
                }
                surface=tinted->surface; cairo_surface_mark_dirty(surface);
            }
        }
        cairo_save(context); rectangle(bounds); cairo_clip(context);
        cairo_translate(context,bounds.X,bounds.Y); cairo_scale(context,bounds.Width/sw,bounds.Height/sh);
        cairo_set_source_surface(context,surface,-sx,-sy);
        cairo_pattern_set_filter(cairo_get_source(context),CAIRO_FILTER_BILINEAR);
        cairo_paint_with_alpha(context,opacity); cairo_restore(context); return Ok;
    }
    int DrawImage(Bitmap* b,RectF r) { return DrawImage(b,r,0,0,b->width,b->height,UnitPixel); }
    int DrawImage(Bitmap* b,float x,float y,float w,float h) { return DrawImage(b,RectF{x,y,w,h}); }
    void FillRectangle(SolidBrush* b,RectF r) { source(b->color); rectangle(r); cairo_fill(context); }
    void FillRectangle(SolidBrush* b,float x,float y,float w,float h) { FillRectangle(b,{x,y,w,h}); }
    void DrawRectangle(Pen* p,RectF r) { source(p->color); cairo_set_line_width(context,p->width); rectangle(r); cairo_stroke(context); }
    void DrawRectangle(Pen* p,float x,float y,float w,float h) { DrawRectangle(p,{x,y,w,h}); }
    void FillEllipse(SolidBrush* b,RectF r) { source(b->color); ellipse(r); cairo_fill(context); }
    void FillEllipse(SolidBrush* b,float x,float y,float w,float h) { FillEllipse(b,{x,y,w,h}); }
    void DrawEllipse(Pen* p,RectF r) { source(p->color); cairo_set_line_width(context,p->width); ellipse(r); cairo_stroke(context); }
    void DrawEllipse(Pen* p,float x,float y,float w,float h) { DrawEllipse(p,{x,y,w,h}); }
    void DrawLine(Pen* p,float x,float y,float xx,float yy) { source(p->color); cairo_set_line_width(context,p->width); cairo_move_to(context,x,y); cairo_line_to(context,xx,yy); cairo_stroke(context); }
    void DrawArc(Pen* p,RectF r,float start,float sweep) {
        cairo_save(context); source(p->color); cairo_translate(context,r.X+r.Width/2,r.Y+r.Height/2);
        cairo_scale(context,r.Width/2,r.Height/2); cairo_set_line_width(context,p->width/(r.Width/2));
        cairo_arc(context,0,0,1,start*3.14159265/180,(start+sweep)*3.14159265/180); cairo_stroke(context); cairo_restore(context);
    }
    void DrawString(const wchar_t* text,int,const Font* font,RectF bounds,StringFormat* format,SolidBrush* brush) {
        cairo_save(context); rectangle(bounds); cairo_clip(context); source(brush->color);
        cairo_select_font_face(context,"sans",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL); cairo_set_font_size(context,font->size);
        // Wrap at word boundaries while retaining explicit line breaks.
        std::string input=utf8(text),line;
        float y=bounds.Y+font->size;
        const auto draw=[&] {
            cairo_text_extents_t ext; cairo_text_extents(context,line.c_str(),&ext);
            float x=bounds.X;
            if(format->alignment==StringAlignmentCenter) x+=(bounds.Width-ext.x_advance)/2;
            if(format->alignment==StringAlignmentFar) x+=bounds.Width-ext.x_advance;
            cairo_move_to(context,x,y); cairo_show_text(context,line.c_str()); y+=font->size*1.2f;
        };
        for(std::size_t pos=0;pos<input.size();) {
            auto end=input.find_first_of(" \n",pos); if(end==std::string::npos) end=input.size();
            auto word=input.substr(pos,end-pos); auto trial=line.empty()?word:line+" "+word;
            cairo_text_extents_t ext; cairo_text_extents(context,trial.c_str(),&ext);
            if(!line.empty() && ext.x_advance>bounds.Width) { draw(); line=word; } else line=trial;
            if(end<input.size() && input[end]=='\n') { draw(); line.clear(); }
            pos=end+1;
        }
        if(!line.empty()) draw(); cairo_restore(context);
    }
};
}
