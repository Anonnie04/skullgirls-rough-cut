#pragma once

#ifdef _WIN32
#include <d3d11.h>
#include <d3dcompiler.h>
#include <gdiplus.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <memory>

namespace skullgirls {

// One transparent hardware layer for the ordered fighter/shadow/trail draws.
// Keep it separate from the stage target so a stationary stage remains cached.
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

    void finish(Gdiplus::Graphics& dc) {
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
        const float clear[4]{};context->ClearRenderTargetView(view.Get(),clear);
        context->OMSetRenderTargets(1,view.GetAddressOf(),nullptr);context->OMSetBlendState(blend.Get(),nullptr,~0u);
        context->OMSetDepthStencilState(depth.Get(),0);context->RSSetState(raster.Get());
        const D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&viewport);
        context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        const UINT stride=sizeof(Vertex),offset=0;context->IASetVertexBuffers(0,1,vertices.GetAddressOf(),&stride,&offset);
        context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
        context->PSSetSamplers(0,1,sampler.GetAddressOf());unsigned start=0;
        for(const auto& command:commands){auto* texture=command.texture.Get();context->PSSetShaderResources(0,1,&texture);context->Draw(6,start);start+=6;}
        context->CopyResource(staging.Get(),target.Get());check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
        for(unsigned y=0;y<height;++y)std::memcpy(pixels.data()+y*width,static_cast<const BYTE*>(mapped.pData)+y*mapped.RowPitch,width*4);
        context->Unmap(staging.Get(),0);
        const auto saved=dc.Save();dc.ResetTransform();dc.DrawImage(&image,0,0,width,height);dc.Restore(saved);
    }

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
