#pragma once
#include "hud_geometry.h"
namespace gpu_hud {
using Microsoft::WRL::ComPtr;
struct Layout {bool target=false,nose=false;hud_geometry::Point target_point,nose_point;float scale=1,opacity=.65f,link_opacity=1;bool connector=true,always=false;int mode_notice=-1,camera_notice=-1;float notice_alpha=1;};
struct Renderer {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> immediate;
    ComPtr<IDXGISwapChain1> swap;ComPtr<IDXGISwapChain2> swap2;
    ComPtr<ID2D1Factory1> factory;ComPtr<ID2D1Device> d2device;ComPtr<ID2D1DeviceContext> dc;
    ComPtr<ID2D1Bitmap1> target;ComPtr<ID2D1SolidColorBrush> ink,outline;
    ComPtr<IDWriteFactory> text_factory;ComPtr<IDWriteTextFormat> text_format;
    ComPtr<IDCompositionDevice> composition;ComPtr<IDCompositionTarget> composition_target;ComPtr<IDCompositionVisual> visual;
    HANDLE latency=nullptr;UINT width=0,height=0;
    ~Renderer(){if(latency)CloseHandle(latency);if(dc)dc->SetTarget(nullptr);}
    HRESULT bind_target(){
        ComPtr<IDXGISurface> surface;HRESULT hr=swap->GetBuffer(0,IID_PPV_ARGS(surface.GetAddressOf()));if(FAILED(hr))return hr;
        D2D1_BITMAP_PROPERTIES1 props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
        hr=dc->CreateBitmapFromDxgiSurface(surface.Get(),&props,target.GetAddressOf());if(FAILED(hr))return hr;
        dc->SetTarget(target.Get());dc->SetDpi(96,96);dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);return S_OK;
    }
    HRESULT initialize(HWND window,UINT w,UINT h){
        D3D_FEATURE_LEVEL level;HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,device.GetAddressOf(),&level,immediate.GetAddressOf());if(FAILED(hr))return hr;
        ComPtr<IDXGIDevice> dxgi;hr=device.As(&dxgi);if(FAILED(hr))return hr;
        ComPtr<IDXGIAdapter> adapter;hr=dxgi->GetAdapter(adapter.GetAddressOf());if(FAILED(hr))return hr;
        ComPtr<IDXGIFactory2> dxgi_factory;hr=adapter->GetParent(IID_PPV_ARGS(dxgi_factory.GetAddressOf()));if(FAILED(hr))return hr;
        DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=w;desc.Height=h;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;
        desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.Scaling=DXGI_SCALING_STRETCH;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;desc.Flags=DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        hr=dxgi_factory->CreateSwapChainForComposition(device.Get(),&desc,nullptr,swap.GetAddressOf());if(FAILED(hr))return hr;
        hr=swap.As(&swap2);if(FAILED(hr))return hr;hr=swap2->SetMaximumFrameLatency(1);if(FAILED(hr))return hr;
        latency=swap2->GetFrameLatencyWaitableObject();if(!latency)return E_FAIL;
        hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),nullptr,reinterpret_cast<void**>(factory.GetAddressOf()));if(FAILED(hr))return hr;
        hr=factory->CreateDevice(dxgi.Get(),d2device.GetAddressOf());if(FAILED(hr))return hr;
        hr=d2device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,dc.GetAddressOf());if(FAILED(hr))return hr;
        hr=dc->CreateSolidColorBrush(D2D1::ColorF(.32f,.72f,.80f,1),ink.GetAddressOf());if(FAILED(hr))return hr;
        hr=dc->CreateSolidColorBrush(D2D1::ColorF(0,0,0,1),outline.GetAddressOf());if(FAILED(hr))return hr;
        if(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(text_factory.GetAddressOf())))){
            text_factory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,18,L"en-us",text_format.GetAddressOf());
            if(text_format)text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        hr=bind_target();if(FAILED(hr))return hr;
        hr=DCompositionCreateDevice(dxgi.Get(),__uuidof(IDCompositionDevice),reinterpret_cast<void**>(composition.GetAddressOf()));if(FAILED(hr))return hr;
        hr=composition->CreateTargetForHwnd(window,TRUE,composition_target.GetAddressOf());if(FAILED(hr))return hr;
        hr=composition->CreateVisual(visual.GetAddressOf());if(FAILED(hr))return hr;
        hr=visual->SetContent(swap.Get());if(FAILED(hr))return hr;hr=composition_target->SetRoot(visual.Get());if(FAILED(hr))return hr;
        hr=composition->Commit();if(FAILED(hr))return hr;width=w;height=h;return S_OK;
    }
    HRESULT resize(UINT w,UINT h){
        if(w==width&&h==height)return S_OK;
        dc->SetTarget(nullptr);target.Reset();immediate->ClearState();immediate->Flush();
        HRESULT hr=swap->ResizeBuffers(2,w,h,DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
        if(FAILED(hr))return hr;width=w;height=h;return bind_target();
    }
    bool ready()const{return latency && WaitForSingleObject(latency,0)==WAIT_OBJECT_0;}
    static D2D1_POINT_2F point(hud_geometry::Point p){return D2D1::Point2F(p.x,p.y);}
    void line(hud_geometry::Point a,hud_geometry::Point b,float size,float alpha){
        outline->SetOpacity(alpha*.28f);ink->SetOpacity(alpha);dc->DrawLine(point(a),point(b),outline.Get(),size+1.5f);dc->DrawLine(point(a),point(b),ink.Get(),size);
    }
    HRESULT draw(const Layout& frame,float dt,bool preview=false){
        const float scale=frame.scale,alpha=std::clamp(frame.opacity,0.f,1.f);
        auto link=hud_geometry::connector(frame.nose_point,frame.target_point,scale,frame.always);
        dc->BeginDraw();dc->SetTransform(D2D1::Matrix3x2F::Identity());dc->Clear(preview?D2D1::ColorF(.045f,.065f,.09f,1):D2D1::ColorF(0,0,0,0));
        if(preview){ink->SetOpacity(.08f);for(float x=0;x<width;x+=80)dc->DrawLine({x,0},{x,float(height)},ink.Get(),1);for(float y=0;y<height;y+=80)dc->DrawLine({0,y},{float(width),y},ink.Get(),1);}
        if(frame.connector&&frame.nose&&frame.target){
            for(const auto& tick:link.ticks)if(tick.opacity>0)
                line(tick.a,tick.b,1.6f*scale,alpha*tick.opacity*frame.link_opacity);
        }
        if(frame.target){
            D2D1_ELLIPSE ring{point(frame.target_point),hud_geometry::ring_radius*scale,hud_geometry::ring_radius*scale};outline->SetOpacity(alpha*.40f);ink->SetOpacity(alpha);
            dc->DrawEllipse(ring,outline.Get(),3.2f*scale);dc->DrawEllipse(ring,ink.Get(),1.3f*scale);
        }
        // The game already renders its crosshair. Nose projection drives ticks only.
        if((frame.mode_notice>=0||frame.camera_notice>=0)&&text_format){
            const wchar_t* label=frame.camera_notice>=0?(frame.camera_notice?L"F3: FAR CAMERA":L"F3: NATIVE POSITION"):(frame.mode_notice?L"F4: AGILE 2.1":L"F4: CLASSIC 2.0");
            ink->SetOpacity(frame.notice_alpha);outline->SetOpacity(.6f*frame.notice_alpha);
            D2D1_RECT_F box=D2D1::RectF(width*.5f-130,height*.12f,width*.5f+130,height*.12f+32);
            dc->FillRoundedRectangle(D2D1::RoundedRect(box,5,5),outline.Get());
            box.top+=3;dc->DrawText(label,UINT32(wcslen(label)),text_format.Get(),box,ink.Get());
        }
        return dc->EndDraw();
    }
    HRESULT present(){return swap->Present(0,DXGI_PRESENT_DO_NOT_WAIT);}
};
}
