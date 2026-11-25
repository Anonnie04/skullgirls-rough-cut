#pragma once

#ifdef _WIN32
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>
#include <chrono>
#include <gdiplus.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <memory>

namespace skullgirls {

// Ordered sprite batches and direct window presentation. The stage retains its
// own cached target on the same device; CPU overlays upload between GPU layers.
class SpriteGpu {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    static constexpr unsigned width=1280,height=720;
    struct Vertex { float x,y,u,v,r,g,b,a; };
    struct Command {
        Ptr<ID3D11ShaderResourceView> texture;
        std::array<Vertex,6> vertices;
        Gdiplus::Bitmap* source;
        std::shared_ptr<Gdiplus::Bitmap> source_owner;
        Gdiplus::RectF bounds;
        Gdiplus::RectF source_rect;
        std::array<float,6> transform;
        std::array<float,4> tint;
    };
    Ptr<ID3D11Device> device;
    Ptr<ID3D11DeviceContext> context;
    Ptr<ID3D11Texture2D> target,staging;
    Ptr<ID3D11RenderTargetView> view;
    Ptr<ID3D11VertexShader> vs;
    Ptr<ID3D11PixelShader> ps;
    Ptr<ID3D11InputLayout> layout;
    Ptr<ID3D11BlendState> blend;
    Ptr<ID3D11DepthStencilState> depth;
    Ptr<ID3D11RasterizerState> raster;
    Ptr<ID3D11SamplerState> sampler;
    Ptr<ID3D11Buffer> vertices;
    unsigned capacity=0;
    Ptr<ID3D11Texture2D> frame_target,overlay_target;
    Ptr<ID3D11RenderTargetView> frame_view,back_view;
    Ptr<ID3D11ShaderResourceView> frame_texture,overlay_texture;
    Ptr<IDXGISwapChain2> swap_chain;
    HWND swap_window=nullptr;
    HANDLE frame_ready=nullptr;
    UINT swap_width=0,swap_height=0;
    bool frame_active=false;
    bool occluded=false;
    const BYTE* overlay_pixels=nullptr;
    int overlay_stride=0;
    std::chrono::steady_clock::time_point frame_started{};
    std::uint64_t readbacks=0,presents=0;
    double render_ms=0,present_ms=0;
    // Two queued presents let the fixed 60 Hz clock overlap rendering and display.
    // A single slot stalls on refresh rates such as 75 Hz.
    unsigned queue_depth=2;
    double ready_wait_ms=0;

    std::vector<Command> commands;
    std::vector<unsigned> pixels=std::vector<unsigned>(width*height);
    Gdiplus::Bitmap image{width,height,width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(pixels.data())};

    static void check(HRESULT hr) { if(FAILED(hr)) throw std::runtime_error("Sprite GPU rendering failed"); }
    static Ptr<ID3DBlob> compile(const char* source,const char* entry,const char* version) {
        Ptr<ID3DBlob> code,error;
        check(D3DCompile(source,std::strlen(source),nullptr,nullptr,nullptr,entry,version,
            D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error));return code;
    }

public:
    struct Unavailable:std::runtime_error {using std::runtime_error::runtime_error;};
    struct Texture {
        Ptr<ID3D11ShaderResourceView> view;
        ID3D11Device* owner=nullptr;
    };
    SpriteGpu() {
        if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))
            throw Unavailable("No hardware Direct3D 11 device");
        D3D11_TEXTURE2D_DESC spec{};
        spec.Width=width;spec.Height=height;spec.MipLevels=spec.ArraySize=1;
        spec.Format=DXGI_FORMAT_B8G8R8A8_UNORM;spec.SampleDesc.Count=1;
        spec.BindFlags=D3D11_BIND_RENDER_TARGET;
        check(device->CreateTexture2D(&spec,nullptr,&target));
        check(device->CreateRenderTargetView(target.Get(),nullptr,&view));
        spec.BindFlags=0;spec.Usage=D3D11_USAGE_STAGING;spec.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        check(device->CreateTexture2D(&spec,nullptr,&staging));
        constexpr auto source=R"(
Texture2D art:register(t0);SamplerState filtering:register(s0);
struct V {float2 p:POSITION;float2 uv:TEXCOORD;float4 tint:COLOR;};
struct P {float4 p:SV_POSITION;float2 uv:TEXCOORD;float4 tint:COLOR;};
P vertex(V i){P o;o.p=float4(i.p,0,1);o.uv=i.uv;o.tint=i.tint;return o;}
float4 pixel(P i):SV_TARGET {float4 c=art.Sample(filtering,i.uv);return c*float4(i.tint.rgb*i.tint.a,i.tint.a);}
)";
        auto vertex=compile(source,"vertex","vs_4_0"),pixel=compile(source,"pixel","ps_4_0");
        check(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs));
        check(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps));
        const D3D11_INPUT_ELEMENT_DESC elements[]={
            {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};
        check(device->CreateInputLayout(elements,3,vertex->GetBufferPointer(),vertex->GetBufferSize(),&layout));
        D3D11_BLEND_DESC blending{};auto& b=blending.RenderTarget[0];b.BlendEnable=TRUE;
        b.SrcBlend=b.SrcBlendAlpha=D3D11_BLEND_ONE;b.DestBlend=b.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
        b.BlendOp=b.BlendOpAlpha=D3D11_BLEND_OP_ADD;b.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        check(device->CreateBlendState(&blending,&blend));
        D3D11_DEPTH_STENCIL_DESC d{};d.DepthEnable=FALSE;check(device->CreateDepthStencilState(&d,&depth));
        D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;
        check(device->CreateRasterizerState(&r,&raster));
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;s.MaxLOD=D3D11_FLOAT32_MAX;
        check(device->CreateSamplerState(&s,&sampler));
    }

    void begin() {commands.clear();}
    void queue(Gdiplus::Graphics& dc,Gdiplus::Bitmap& bitmap,const Gdiplus::RectF& bounds,
               const std::array<float,4>& tint,Texture& cached,const std::array<float,4>& uv={0,0,1,1},
               std::shared_ptr<Gdiplus::Bitmap> source_owner={}) {
        if(cached.owner!=device.Get() || !cached.view) {
            Gdiplus::Rect region(0,0,bitmap.GetWidth(),bitmap.GetHeight());Gdiplus::BitmapData data{};
            if(bitmap.LockBits(&region,Gdiplus::ImageLockModeRead,PixelFormat32bppPARGB,&data)!=Gdiplus::Ok)
                throw std::runtime_error("Cannot upload sprite pixels");
            D3D11_TEXTURE2D_DESC spec{};spec.Width=region.Width;spec.Height=region.Height;
            spec.MipLevels=spec.ArraySize=1;spec.Format=DXGI_FORMAT_B8G8R8A8_UNORM;spec.SampleDesc.Count=1;
            spec.Usage=D3D11_USAGE_IMMUTABLE;spec.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA source{data.Scan0,UINT(data.Stride),0};Ptr<ID3D11Texture2D> texture;
            const auto hr=device->CreateTexture2D(&spec,&source,&texture);bitmap.UnlockBits(&data);check(hr);
            cached.view.Reset();check(device->CreateShaderResourceView(texture.Get(),nullptr,&cached.view));cached.owner=device.Get();
        }
        Gdiplus::Matrix matrix;dc.GetTransform(&matrix);std::array<float,6> m{};matrix.GetElements(m.data());
        const auto v=[&](float x,float y,float u,float w) {
            return Vertex{(x*m[0]+y*m[2]+m[4])/640-1,1-(x*m[1]+y*m[3]+m[5])/360,
                u,w,tint[0],tint[1],tint[2],tint[3]};
        };
        auto a=v(bounds.X,bounds.Y,uv[0],uv[1]),b=v(bounds.X+bounds.Width,bounds.Y,uv[2],uv[1]);
        auto c=v(bounds.X,bounds.Y+bounds.Height,uv[0],uv[3]),d=v(bounds.X+bounds.Width,bounds.Y+bounds.Height,uv[2],uv[3]);
        const Gdiplus::RectF source(uv[0]*bitmap.GetWidth(),uv[1]*bitmap.GetHeight(),
            (uv[2]-uv[0])*bitmap.GetWidth(),(uv[3]-uv[1])*bitmap.GetHeight());
        commands.push_back({cached.view,{a,b,c,c,b,d},&bitmap,std::move(source_owner),bounds,source,m,tint});
    }

    void render_commands(ID3D11RenderTargetView* destination,bool clear_target,
                         const D3D11_VIEWPORT* custom_viewport=nullptr) {
        if(commands.empty())return;
        const auto count=unsigned(commands.size()*6);
        if(count>capacity) {
            vertices.Reset();capacity=std::max(count,256u);
            D3D11_BUFFER_DESC b{};b.ByteWidth=capacity*sizeof(Vertex);b.Usage=D3D11_USAGE_DYNAMIC;
            b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            check(device->CreateBuffer(&b,nullptr,&vertices));
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped));
        auto* out=static_cast<Vertex*>(mapped.pData);
        for(const auto& command:commands){std::copy(command.vertices.begin(),command.vertices.end(),out);out+=6;}
        context->Unmap(vertices.Get(),0);
        const float clear[4]{};
        ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);
        if(clear_target)context->ClearRenderTargetView(destination,clear);
        context->OMSetRenderTargets(1,&destination,nullptr);context->OMSetBlendState(blend.Get(),nullptr,~0u);
        context->OMSetDepthStencilState(depth.Get(),0);context->RSSetState(raster.Get());
        const D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context->RSSetViewports(1,custom_viewport?custom_viewport:&viewport);
        context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        const UINT stride=sizeof(Vertex),offset=0;context->IASetVertexBuffers(0,1,vertices.GetAddressOf(),&stride,&offset);
        context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
        context->PSSetSamplers(0,1,sampler.GetAddressOf());unsigned start=0;
        for(const auto& command:commands){auto* texture=command.texture.Get();context->PSSetShaderResources(0,1,&texture);context->Draw(6,start);start+=6;}
    }

    Gdiplus::Bitmap& readback(ID3D11Texture2D* source) {
        context->CopyResource(staging.Get(),source);D3D11_MAPPED_SUBRESOURCE mapped{};
        check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
        for(unsigned y=0;y<height;++y)std::memcpy(pixels.data()+y*width,static_cast<const BYTE*>(mapped.pData)+y*mapped.RowPitch,width*4);
        context->Unmap(staging.Get(),0);++readbacks;return image;
    }

    void finish(Gdiplus::Graphics& dc) {
        if(commands.empty())return;
        if(frame_active) {render_commands(frame_view.Get(),false);return;}
        render_commands(view.Get(),true);readback(target.Get());
        const auto saved=dc.Save();dc.ResetTransform();dc.DrawImage(&image,0,0,width,height);dc.Restore(saved);
    }

    ID3D11Device* native_device() const {return device.Get();}
    ID3D11DeviceContext* native_context() const {return context.Get();}
    bool presenting() const {return frame_active;}
    std::uint64_t readback_count() const {return readbacks;}
    std::uint64_t present_count() const {return presents;}
    double last_render_ms() const {return render_ms;}
    double last_present_ms() const {return present_ms;}
    double last_ready_wait_ms() const {return ready_wait_ms;}
    void set_queue_depth(unsigned depth) {
        queue_depth=std::clamp(depth,1u,3u);
        if(swap_chain)check(swap_chain->SetMaximumFrameLatency(queue_depth));
    }
    HANDLE latency_handle() const {return frame_ready;}
    bool is_occluded() const {return occluded;}
    void mark_occluded(){occluded=true;}
    std::array<unsigned,2> back_buffer_size() const {return {swap_width,swap_height};}
    static D3D11_VIEWPORT fitted_viewport(unsigned w,unsigned h) {
        const float vw=std::min(float(w),float(h)*16/9),vh=vw*9/16;
        return {(w-vw)*.5f,(h-vh)*.5f,vw,vh,0,1};
    }
    bool frame_statistics(DXGI_FRAME_STATISTICS& statistics) const {
        return swap_chain && swap_chain->GetFrameStatistics(&statistics)==S_OK;
    }

    void detach_window() {
        if(context)context->OMSetRenderTargets(0,nullptr,nullptr);
        frame_active=false;occluded=false;back_view.Reset();swap_chain.Reset();swap_window=nullptr;
        if(frame_ready){CloseHandle(frame_ready);frame_ready=nullptr;}
        swap_width=swap_height=0;
    }
    ~SpriteGpu(){detach_window();}

    void attach_window(HWND window) {
        if(swap_window==window && swap_chain)return;
        detach_window();
        Ptr<IDXGIDevice> dxgi;Ptr<IDXGIAdapter> adapter;Ptr<IDXGIFactory2> factory;
        check(device.As(&dxgi));check(dxgi->GetAdapter(&adapter));check(adapter->GetParent(IID_PPV_ARGS(&factory)));
        DXGI_SWAP_CHAIN_DESC1 spec{};spec.Format=DXGI_FORMAT_B8G8R8A8_UNORM;spec.SampleDesc.Count=1;
        spec.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;spec.BufferCount=2;
        spec.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;spec.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
        spec.Scaling=DXGI_SCALING_STRETCH;spec.Flags=DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        Ptr<IDXGISwapChain1> created;check(factory->CreateSwapChainForHwnd(device.Get(),window,&spec,nullptr,nullptr,&created));
        check(created.As(&swap_chain));check(swap_chain->SetMaximumFrameLatency(queue_depth));
        frame_ready=swap_chain->GetFrameLatencyWaitableObject();
        if(!frame_ready)throw std::runtime_error("No frame-latency signal");
        check(factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER));swap_window=window;
    }

    void ensure_frame_target() {
        if(frame_target)return;
        D3D11_TEXTURE2D_DESC spec{};spec.Width=width;spec.Height=height;spec.MipLevels=spec.ArraySize=1;
        spec.Format=DXGI_FORMAT_B8G8R8A8_UNORM;spec.SampleDesc.Count=1;
        spec.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        check(device->CreateTexture2D(&spec,nullptr,&frame_target));
        check(device->CreateRenderTargetView(frame_target.Get(),nullptr,&frame_view));
        check(device->CreateShaderResourceView(frame_target.Get(),nullptr,&frame_texture));
        spec.BindFlags=D3D11_BIND_SHADER_RESOURCE;spec.Usage=D3D11_USAGE_DYNAMIC;spec.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        check(device->CreateTexture2D(&spec,nullptr,&overlay_target));
        check(device->CreateShaderResourceView(overlay_target.Get(),nullptr,&overlay_texture));
    }

    bool begin_frame(HWND window,const BYTE* cpu_overlay,int stride) {
        if(window) {
            attach_window(window);
            const bool returning=occluded;
            if(occluded) {
                const auto status=swap_chain->Present(0,DXGI_PRESENT_TEST);
                if(status==DXGI_STATUS_OCCLUDED){abort_frame();return false;}
                check(status);occluded=false;
            }
            const auto wait_started=std::chrono::steady_clock::now();
            if(!returning && WaitForSingleObject(frame_ready,100)!=WAIT_OBJECT_0)throw std::runtime_error("Presentation queue stalled");
            ready_wait_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-wait_started).count();
        }
        ensure_frame_target();
        overlay_pixels=cpu_overlay;overlay_stride=stride;begin();frame_active=true;
        frame_started=std::chrono::steady_clock::now();
        ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);
        const float black[]={0,0,0,1};context->ClearRenderTargetView(frame_view.Get(),black);
        return true;
    }
    void abort_frame(){frame_active=false;commands.clear();}

    void composite(ID3D11ShaderResourceView* texture,ID3D11RenderTargetView* destination,
                   const D3D11_VIEWPORT* viewport=nullptr) {
        begin();Command c{};c.texture=texture;
        const Vertex a{-1,1,0,0,1,1,1,1},b{1,1,1,0,1,1,1,1},d{1,-1,1,1,1,1,1,1},e{-1,-1,0,1,1,1,1,1};
        c.vertices={a,b,e,e,b,d};commands.push_back(std::move(c));render_commands(destination,false,viewport);
    }
    void composite_stage(ID3D11ShaderResourceView* texture) {composite(texture,frame_view.Get());}

    void flush_overlay(Gdiplus::Graphics& dc) {
        if(!frame_active)return;
        dc.Flush(Gdiplus::FlushIntentionSync);
        bool visible=false;
        for(unsigned y=0;y<height && !visible;++y) {
            const auto* row=reinterpret_cast<const unsigned*>(overlay_pixels+std::ptrdiff_t(y)*overlay_stride);
            for(unsigned x=0;x<width;++x)if(row[x]>>24){visible=true;break;}
        }
        if(visible) {
            D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(overlay_target.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped));
            for(unsigned y=0;y<height;++y)std::memcpy(static_cast<BYTE*>(mapped.pData)+y*mapped.RowPitch,
                overlay_pixels+std::ptrdiff_t(y)*overlay_stride,width*4);
            context->Unmap(overlay_target.Get(),0);composite(overlay_texture.Get(),frame_view.Get());
        }
        const auto saved=dc.Save();dc.ResetTransform();dc.ResetClip();dc.Clear(Gdiplus::Color::Transparent);dc.Restore(saved);
    }

    void present(Gdiplus::Graphics& dc,bool vsync=true) {
        flush_overlay(dc);
        RECT client{};GetClientRect(swap_window,&client);const UINT w=client.right,h=client.bottom;
        if(!w || !h){abort_frame();return;}
        if(w!=swap_width || h!=swap_height || !back_view) {
            context->OMSetRenderTargets(0,nullptr,nullptr);back_view.Reset();
            check(swap_chain->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));
            Ptr<ID3D11Texture2D> back;check(swap_chain->GetBuffer(0,IID_PPV_ARGS(&back)));
            check(device->CreateRenderTargetView(back.Get(),nullptr,&back_view));swap_width=w;swap_height=h;
        }
        const float black[]={0,0,0,1};context->ClearRenderTargetView(back_view.Get(),black);
        const auto viewport=fitted_viewport(w,h);
        composite(frame_texture.Get(),back_view.Get(),&viewport);
        render_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_started).count();
        const auto start=std::chrono::steady_clock::now();const auto status=swap_chain->Present(vsync?1:0,0);check(status);
        occluded=status==DXGI_STATUS_OCCLUDED;
        present_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        ++presents;frame_active=false;commands.clear();
    }
    Gdiplus::Bitmap& capture_frame(){return readback(frame_target.Get());}

    void fallback(Gdiplus::Graphics& dc) const {
        for(const auto& c:commands) {
            const auto saved=dc.Save();Gdiplus::Matrix m(c.transform[0],c.transform[1],c.transform[2],c.transform[3],c.transform[4],c.transform[5]);dc.SetTransform(&m);
            Gdiplus::ColorMatrix color{};color.m[0][0]=c.tint[0];color.m[1][1]=c.tint[1];color.m[2][2]=c.tint[2];color.m[3][3]=c.tint[3];color.m[4][4]=1;
            Gdiplus::ImageAttributes attributes;attributes.SetColorMatrix(&color);
            dc.DrawImage(c.source,c.bounds,c.source_rect.X,c.source_rect.Y,c.source_rect.Width,c.source_rect.Height,
                Gdiplus::UnitPixel,&attributes);dc.Restore(saved);
        }
    }
};
}
#endif
