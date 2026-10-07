#pragma once
#include "hud_geometry.h"
#include "control_modes.h"
#include <dwrite_3.h>
#include <string>
#include "creeperux_tokens.h"
namespace gpu_hud {
using Microsoft::WRL::ComPtr;
struct Layout {std::wstring free_look_key=L"C",zoom_key=L"MouseRight";bool target=false,nose=false;hud_geometry::Point target_point,nose_point;float scale=1,opacity=.65f,link_opacity=1;bool connector=true,always=false,helmet_active=false;int mode_notice=-1,camera_notice=-1,helmet_notice=-1;float notice_alpha=1,sensitivity_notice=-1,zoom_notice=-1,zoom=1.75f,zoom_start=1.75f;int hud_hz=120;float source_hz=0,submit_hz=0;bool smoothing=false,independent=false,predicted=false;bool ui_only=false;float prediction_ms=0,prediction_shift=0;uint64_t prediction_fallbacks=0;bool settings=false;float sensitivity=.1f,configured_sensitivity=.1f,fov=62;unsigned pose_age=0;int selected_mode=0,selected_camera=1;};
struct Renderer {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> immediate;
    ComPtr<IDXGISwapChain1> swap;ComPtr<IDXGISwapChain2> swap2;
    ComPtr<ID2D1Factory1> factory;ComPtr<ID2D1Device> d2device;ComPtr<ID2D1DeviceContext> dc;
    ComPtr<ID2D1Bitmap1> target;ComPtr<ID2D1SolidColorBrush> ink,outline;
    ComPtr<IDWriteFactory> text_factory;ComPtr<IDWriteTextFormat> text_format;
    ComPtr<IDWriteFontCollection1> kit_fonts;
    ComPtr<IDWriteTextFormat> ui_body,ui_small,ui_title,ui_mono,ui_value;
    ComPtr<ID2D1SolidColorBrush> ui_paint;
    bool bundled_fonts=false;
    ComPtr<IDCompositionDevice> composition;ComPtr<IDCompositionTarget> composition_target;ComPtr<IDCompositionVisual> visual;
    HANDLE latency=nullptr;UINT width=0,height=0;
    ~Renderer(){if(latency)CloseHandle(latency);if(dc)dc->SetTarget(nullptr);}
    HRESULT bind_target(){
        ComPtr<IDXGISurface> surface;HRESULT hr=swap->GetBuffer(0,IID_PPV_ARGS(surface.GetAddressOf()));if(FAILED(hr))return hr;
        D2D1_BITMAP_PROPERTIES1 props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
        hr=dc->CreateBitmapFromDxgiSurface(surface.Get(),&props,target.GetAddressOf());if(FAILED(hr))return hr;
        dc->SetTarget(target.Get());dc->SetDpi(96,96);dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);return S_OK;
    }
    HRESULT initialize(HWND window,UINT w,UINT h,const wchar_t* font_directory=nullptr){
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
        if(text_factory){
            ComPtr<IDWriteFactory3> factory3;
            if(SUCCEEDED(text_factory.As(&factory3))){
                wchar_t folder[MAX_PATH]{};
                if(font_directory)GetFullPathNameW(font_directory,MAX_PATH,folder,nullptr);
                else {
                    HMODULE module=nullptr;
                    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        reinterpret_cast<LPCWSTR>(&font_module_anchor),&module);
                    GetModuleFileNameW(module,folder,MAX_PATH);
                    if(auto slash=wcsrchr(folder,L'\\'))*(slash+1)=0;
                    wcscat_s(folder,L"ui-fonts");
                }
                ComPtr<IDWriteFontSetBuilder> builder;
                HRESULT fonts_hr=factory3->CreateFontSetBuilder(&builder);
                for(const auto name:{L"chakra-petch-600-latin.ttf",L"share-tech-mono-400-latin.ttf"}){
                    if(FAILED(fonts_hr))break;
                    std::wstring file=std::wstring(folder)+L"\\"+name;
                    ComPtr<IDWriteFontFaceReference> face;
                    fonts_hr=factory3->CreateFontFaceReference(file.c_str(),nullptr,0,DWRITE_FONT_SIMULATIONS_NONE,&face);
                    if(SUCCEEDED(fonts_hr))fonts_hr=builder->AddFontFaceReference(face.Get());
                }
                if(SUCCEEDED(fonts_hr)){
                    ComPtr<IDWriteFontSet> set;fonts_hr=builder->CreateFontSet(&set);
                    if(SUCCEEDED(fonts_hr))fonts_hr=factory3->CreateFontCollectionFromFontSet(set.Get(),&kit_fonts);
                    bundled_fonts=SUCCEEDED(fonts_hr);
                }
            }
            auto format=[&](const wchar_t* family,float size,IDWriteFontCollection* collection,ComPtr<IDWriteTextFormat>& out){
                text_factory->CreateTextFormat(family,collection,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&out);
                if(out){out->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);out->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);}
            };
            format(L"Segoe UI",creeperux::type_md,nullptr,ui_body);
            format(L"Segoe UI",creeperux::type_sm,nullptr,ui_small);
            format(bundled_fonts?L"Chakra Petch":L"Segoe UI",creeperux::type_xxl,kit_fonts.Get(),ui_title);
            format(bundled_fonts?L"Share Tech Mono":L"Consolas",creeperux::type_md,kit_fonts.Get(),ui_mono);
            format(bundled_fonts?L"Share Tech Mono":L"Consolas",creeperux::type_xxl,kit_fonts.Get(),ui_value);
        }
        hr=dc->CreateSolidColorBrush(creeperux::text,&ui_paint);if(FAILED(hr))return hr;
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
    static void font_module_anchor(){}
    void ui_rect(D2D1_RECT_F box,D2D1_COLOR_F color,float radius=0,float alpha=1,bool border=false){
        color.a*=alpha;ui_paint->SetColor(color);
        auto rounded=D2D1::RoundedRect(box,radius,radius);
        if(border)dc->DrawRoundedRectangle(rounded,ui_paint.Get(),1);else dc->FillRoundedRectangle(rounded,ui_paint.Get());
    }
    void ui_text(const wchar_t* label,D2D1_RECT_F box,IDWriteTextFormat* format,D2D1_COLOR_F color,float alpha=1,bool right=false){
        if(!format)return;color.a*=alpha;ui_paint->SetColor(color);
        format->SetTextAlignment(right?DWRITE_TEXT_ALIGNMENT_TRAILING:DWRITE_TEXT_ALIGNMENT_LEADING);
        dc->DrawText(label,UINT32(wcslen(label)),format,box,ui_paint.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    void ui_key(const wchar_t* key,float x,float y,float width,float alpha=1){
        auto box=D2D1::RectF(x,y,x+width,y+26);
        ui_rect(box,creeperux::raised,creeperux::radius_chip,alpha);
        ui_rect(box,creeperux::boundary,creeperux::radius_chip,alpha,true);
        ui_text(key,D2D1::RectF(x+7,y,x+width-7,y+26),ui_mono.Get(),creeperux::muted,alpha);
    }
    void draw_ui(const Layout& frame){
        if(!ui_paint||!ui_body)return;
        const float scale=std::max(.25f,std::min({std::clamp(float(height)/720.f,.75f,2.f), (float(height)-40)/602.f,(float(width)-40)/420.f}));
        const float viewport_w=width/scale,viewport_h=height/scale;
        dc->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale));
        const bool notice=frame.mode_notice>=0||frame.camera_notice>=0||frame.helmet_notice>=0||frame.sensitivity_notice>=0||frame.zoom_notice>=0;
        if(notice){
            const wchar_t* key=L"F4";const wchar_t* category=L"FLIGHT CONTROL";const wchar_t* title=control_modes::wname(frame.mode_notice);
            const wchar_t* detail=L"Learned response model retained";auto status=creeperux::info;
            if(frame.camera_notice>=0){key=L"F3";category=L"CAMERA";title=frame.camera_notice?L"Far follow camera":L"Native camera framing";detail=L"Mouse-follow direction retained";}
            if(frame.helmet_notice>=0){key=L"F2";category=L"TARGET SELECTION";title=frame.helmet_notice==2?L"HMD unavailable":frame.helmet_notice?L"HMD enabled":L"HMD disabled";detail=frame.helmet_notice==2?L"Stock target selection remains active":L"Use the game's target-switch key";if(frame.helmet_notice==2)status=creeperux::warn;}
            wchar_t value[96]{};
            if(frame.sensitivity_notice>=0){key=L"CTRL";category=L"MOUSE SENSITIVITY";swprintf_s(value,L"Sensitivity %.4f",frame.sensitivity_notice);title=value;detail=L"This session only / Ctrl + Home to reset";}
            if(frame.zoom_notice>=0){key=L"ALT";category=L"FREE-LOOK ZOOM";swprintf_s(value,L"Extra zoom %.2fx",frame.zoom_notice);title=value;detail=L"Hold C + right mouse / Alt + Home to reset";}
            const float available=frame.settings?viewport_w-460:viewport_w;
            const float left=(available-360)*.5f,top=viewport_h*.10f;
            const float alpha=std::clamp(frame.notice_alpha,0.f,1.f);
            auto box=D2D1::RectF(left,top,left+360,top+86);
            ui_rect(box,creeperux::panel,creeperux::radius_panel,alpha);
            ui_rect(box,creeperux::line,creeperux::radius_panel,alpha,true);
            ui_rect(D2D1::RectF(left,top+12,left+3,top+74),status,0,alpha);
            ui_key(key,left+14,top+16,(frame.sensitivity_notice>=0||frame.zoom_notice>=0)?50.f:36.f,alpha);
            float text_x=left+((frame.sensitivity_notice>=0||frame.zoom_notice>=0)?78.f:64.f);
            ui_text(category,D2D1::RectF(text_x,top+10,left+346,top+28),ui_mono.Get(),creeperux::quiet,alpha);
            ui_text(title,D2D1::RectF(text_x,top+28,left+346,top+54),ui_title.Get(),creeperux::text,alpha);
            ui_text(detail,D2D1::RectF(left+14,top+58,left+346,top+78),ui_small.Get(),creeperux::muted,alpha);
        }
        if(frame.settings){
            const float w=420,left=viewport_w-w-20,top=20;
            auto box=D2D1::RectF(left,top,left+w,top+602);
            ui_rect(box,creeperux::panel,creeperux::radius_panel);
            ui_rect(box,creeperux::line,creeperux::radius_panel,1,true);
            ui_text(L"AC8 / IN-FLIGHT TOOLS",D2D1::RectF(left+20,top+14,left+300,top+32),ui_mono.Get(),creeperux::quiet);
            ui_text(L"Flight settings",D2D1::RectF(left+20,top+34,left+300,top+66),ui_title.Get(),creeperux::text);
            ui_key(L"F1",left+w-60,top+28,40);
            ui_rect(D2D1::RectF(left+20,top+80,left+w-20,top+81),creeperux::line);
            ui_text(L"MOUSE SENSITIVITY",D2D1::RectF(left+20,top+94,left+270,top+114),ui_mono.Get(),creeperux::quiet);
            wchar_t value[128]{};swprintf_s(value,L"%.4f",frame.sensitivity);
            ui_text(value,D2D1::RectF(left+20,top+118,left+160,top+153),ui_value.Get(),creeperux::text);
            swprintf_s(value,L"Startup %.4f",frame.configured_sensitivity);
            ui_text(value,D2D1::RectF(left+180,top+120,left+w-20,top+150),ui_mono.Get(),creeperux::muted,1,true);
            ui_rect(D2D1::RectF(left+20,top+158,left+w-20,top+190),creeperux::accent_soft,creeperux::radius_control);
            ui_text(L"Ctrl + PgUp / PgDn",D2D1::RectF(left+30,top+158,left+245,top+190),ui_mono.Get(),creeperux::accent_text);
            ui_text(L"+10% / -9.1%",D2D1::RectF(left+240,top+158,left+w-30,top+190),ui_mono.Get(),creeperux::accent_text,1,true);
            ui_text(L"Ctrl + Home: reset    /    This session only",D2D1::RectF(left+20,top+198,left+w-20,top+222),ui_small.Get(),creeperux::muted);
            swprintf_s(value,L"ZOOM: %s + %s",frame.free_look_key.c_str(),frame.zoom_key.c_str());
            ui_text(value,D2D1::RectF(left+20,top+230,left+220,top+254),ui_mono.Get(),creeperux::quiet);
            swprintf_s(value,L"%.2fx / start %.2fx",frame.zoom,frame.zoom_start);
            ui_text(value,D2D1::RectF(left+215,top+230,left+w-20,top+254),ui_mono.Get(),creeperux::text,1,true);
            ui_rect(D2D1::RectF(left+20,top+260,left+w-20,top+292),creeperux::raised,creeperux::radius_control);
            ui_text(L"Alt + PgUp / PgDn    +/- 0.25x",D2D1::RectF(left+30,top+260,left+w-30,top+292),ui_mono.Get(),creeperux::text);
            auto row=[&](const wchar_t* key,const wchar_t* label,const wchar_t* state,float y){
                ui_key(key,left+20,top+y+4,34);
                ui_text(label,D2D1::RectF(left+66,top+y,left+220,top+y+34),ui_body.Get(),creeperux::muted);
                ui_text(state,D2D1::RectF(left+210,top+y,left+w-20,top+y+34),ui_mono.Get(),creeperux::text,1,true);
            };
            row(L"F4",L"Flight control",control_modes::wname(frame.selected_mode),310);
            row(L"F3",L"Camera",frame.selected_camera?L"FAR FOLLOW":L"NATIVE FRAMING",344);
            row(L"F2",L"Target selection",frame.helmet_active?L"HMD ON":L"HMD OFF",378);
            ui_rect(D2D1::RectF(left+20,top+426,left+w-20,top+427),creeperux::line);
            swprintf_s(value,L"CAMERA FOV  %.1f",frame.fov);
            ui_text(value,D2D1::RectF(left+20,top+438,left+220,top+460),ui_mono.Get(),creeperux::quiet);
            swprintf_s(value,L"POSE AGE  %u ms",frame.pose_age);
            ui_text(value,D2D1::RectF(left+210,top+438,left+w-20,top+460),ui_mono.Get(),creeperux::quiet,1,true);
            ui_text(L"F5 Diagnostics  /  F6 Camera  /  F7 HUD",D2D1::RectF(left+20,top+472,left+w-20,top+494),ui_small.Get(),creeperux::muted);
            ui_text(L"F8 Flight assist  /  F9 Center  /  F10 Reload",D2D1::RectF(left+20,top+496,left+w-20,top+518),ui_small.Get(),creeperux::muted);
            ui_text(frame.ui_only?L"Native reticle / On-demand panel overlay":frame.smoothing?L"Ctrl + End: display prediction requested":L"Ctrl + End: raw display",D2D1::RectF(left+20,top+520,left+w-20,top+542),ui_small.Get(),creeperux::muted);
            if(frame.ui_only)swprintf_s(value,L"UI targets: panel20Hz / notices60Hz");else swprintf_s(value,L"%s | Pose %.0f / Submit %.0f Hz",frame.independent?L"Independent input":L"Game input fallback",frame.source_hz,frame.submit_hz);
            ui_text(value,D2D1::RectF(left+20,top+544,left+w-20,top+562),ui_small.Get(),creeperux::quiet);
            if(frame.ui_only)swprintf_s(value,L"Reticle: native UMG / no display prediction");else swprintf_s(value,L"%s | %.2f ms | %.2f px | fallback %llu",frame.ui_only?L"NATIVE":frame.predicted?L"ACTIVE":frame.smoothing?L"WAITING":L"RAW",frame.prediction_ms,frame.prediction_shift,static_cast<unsigned long long>(frame.prediction_fallbacks));
            ui_text(value,D2D1::RectF(left+20,top+570,left+w-20,top+594),ui_small.Get(),creeperux::muted);
        }
        dc->SetTransform(D2D1::Matrix3x2F::Identity());
    }
    HRESULT draw(const Layout& frame,float dt,bool preview=false){
        const float scale=frame.scale,alpha=std::clamp(frame.opacity,0.f,1.f);

        dc->BeginDraw();dc->SetTransform(D2D1::Matrix3x2F::Identity());dc->Clear(preview?D2D1::ColorF(.045f,.065f,.09f,1):D2D1::ColorF(0,0,0,0));
        if(preview){ink->SetOpacity(.08f);for(float x=0;x<width;x+=80)dc->DrawLine({x,0},{x,float(height)},ink.Get(),1);for(float y=0;y<height;y+=80)dc->DrawLine({0,y},{float(width),y},ink.Get(),1);}
        if(!frame.ui_only&&frame.connector&&frame.nose&&frame.target){
            auto link=hud_geometry::connector(frame.nose_point,frame.target_point,scale,frame.always);
            for(const auto& tick:link.ticks)if(tick.opacity>0)
                line(tick.a,tick.b,1.6f*scale,alpha*tick.opacity*frame.link_opacity);
        }
        if(!frame.ui_only&&frame.target){
            D2D1_ELLIPSE ring{point(frame.target_point),hud_geometry::ring_radius*scale,hud_geometry::ring_radius*scale};outline->SetOpacity(alpha*.40f);ink->SetOpacity(alpha);
            dc->DrawEllipse(ring,outline.Get(),3.2f*scale);dc->DrawEllipse(ring,ink.Get(),1.3f*scale);
            if(frame.helmet_active){
                // HMD mode A: four inward ticks, same ring and clear centre.
                const auto c=frame.target_point;
                for(const auto axis:{hud_geometry::Point{1,0},hud_geometry::Point{-1,0},hud_geometry::Point{0,1},hud_geometry::Point{0,-1}})
                    line({c.x+axis.x*14*scale,c.y+axis.y*14*scale},
                         {c.x+axis.x*21*scale,c.y+axis.y*21*scale},1.3f*scale,alpha);
            }
        }
        // The game already renders its crosshair. Nose projection drives ticks only.
        draw_ui(frame);
        return dc->EndDraw();
    }
    HRESULT present(){return swap->Present(0,DXGI_PRESENT_DO_NOT_WAIT);}
};
}
