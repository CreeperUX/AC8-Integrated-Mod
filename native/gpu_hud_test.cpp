#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <dwrite.h>
#pragma comment(lib,"dwrite.lib")
#include <cassert>
#include <cstdio>
#include <limits>
#include <vector>
#include "src/gpu_hud_renderer.h"
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dcomp.lib")
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"user32.lib")
void check(HRESULT hr,const char* stage){if(FAILED(hr)){printf("FAIL %s %08lX\n",stage,(unsigned long)hr);exit(1);}}
void dump(gpu_hud::Renderer& r,const char* path,bool transparent){
    Microsoft::WRL::ComPtr<ID3D11Texture2D> source,staging;
    check(r.swap->GetBuffer(0,IID_PPV_ARGS(source.GetAddressOf())),"get backbuffer");
    D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    check(r.device->CreateTexture2D(&desc,nullptr,staging.GetAddressOf()),"staging");r.immediate->CopyResource(staging.Get(),source.Get());
    D3D11_MAPPED_SUBRESOURCE map{};check(r.immediate->Map(staging.Get(),0,D3D11_MAP_READ,0,&map),"map own backbuffer");
    FILE* f=nullptr;fopen_s(&f,path,"wb");assert(f);unsigned painted=0,clear=0;
    for(UINT y=0;y<desc.Height;++y){auto p=static_cast<unsigned char*>(map.pData)+y*map.RowPitch;fwrite(p,4,desc.Width,f);
        for(UINT x=0;x<desc.Width;++x){auto c=p+4*x;if(c[3])++painted;else ++clear;
            if(transparent){assert(c[0]<=c[3]+1&&c[1]<=c[3]+1&&c[2]<=c[3]+1);assert(c[3]<=220);
                if(x>=640&&x<=680&&y>=265&&y<=305)assert(c[3]==0); // No duplicate nose cross.
            }
        }
    }
    fclose(f);r.immediate->Unmap(staging.Get(),0);assert(painted>100);if(transparent)assert(clear>desc.Width*desc.Height*.98);
}
int main(){
    unsigned cases=0;
    for(float scale:{.75f,1.f,2.f,4.f})for(int a=0;a<360;a+=5)for(int d=0;d<1500;d+=5){
        float rad=a*3.14159265f/180;hud_geometry::Point n{float(d)*cosf(rad),float(d)*sinf(rad)};
        auto l=hud_geometry::connector(n,{},scale,false);
        for(auto t:l.ticks){assert(std::isfinite(t.opacity)&&t.opacity>=0&&t.opacity<=1);if(t.opacity){
            auto cx=(t.a.x+t.b.x)*.5f,cy=(t.a.y+t.b.y)*.5f;
            assert(hypotf(cx,cy)>hud_geometry::ring_radius*scale);
            assert(hypotf(n.x-cx,n.y-cy)>=16*scale-.01f);
            assert(fabsf((t.b.x-t.a.x)*n.x+(t.b.y-t.a.y)*n.y)<2.f);
        }}++cases;
    }
    for(auto t:hud_geometry::connector({NAN,0},{},1,false).ticks)assert(t.opacity==0);
    for(auto t:hud_geometry::connector({100,0},{},NAN,false).ticks)assert(t.opacity==0);
    printf("PASS geometry %u cases; overlap, finite, direction, marker clearances\n",cases);
    check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"AC8OwnRenderTest";assert(RegisterClassW(&wc));
    HWND w=CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"Hidden renderer test",WS_POPUP,0,0,960,540,nullptr,nullptr,wc.hInstance,nullptr);assert(w);
    assert(SetLayeredWindowAttributes(w,0,255,LWA_ALPHA));
    {
        gpu_hud::Renderer r;check(r.initialize(w,960,540,L"test-runtime\\ui-fonts"),"hardware initialize");
        gpu_hud::Layout f;f.target=f.nose=true;f.target_point={300,230};f.nose_point={660,285};
        assert(r.bundled_fonts && r.ui_body && r.ui_mono && r.ui_title);
        unsigned submitted=0,busy=0;LARGE_INTEGER start,end,freq;QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&start);
        for(int i=0;i<180;++i){
            auto wait=WaitForSingleObject(r.latency,1000);assert(wait==WAIT_OBJECT_0);
            if(i==60)check(r.resize(1920,1080),"resize fullHD");
            if(i==120)check(r.resize(960,540),"resize restore");
            f.target_point={300+float(i)*.35f,230+10*sinf(i*.05f)};
            check(r.draw(f,1.f/120),"draw");
            if(i==0)dump(r,"test-runtime/previews/gpu-separated.bgra",true);
            HRESULT hr=r.present();if(hr==DXGI_ERROR_WAS_STILL_DRAWING)++busy;else{check(hr,"present");++submitted;}
        }
        QueryPerformanceCounter(&end);printf("PASS hidden hardware render/resize/present submitted=%u busy=%u elapsed=%.3fs; NOT display FPS\n",submitted,busy,double(end.QuadPart-start.QuadPart)/freq.QuadPart);
        for(int i=0;i<3;++i){
            assert(WaitForSingleObject(r.latency,1000)==WAIT_OBJECT_0);
            f.mode_notice=i%2;assert(r.text_format);f.target_point={400,260};f.nose_point=i==0?hud_geometry::Point{660,285}:i==1?hud_geometry::Point{430,270}:hud_geometry::Point{210,100};
            check(r.draw(f,1.f/120,true),"preview");
            char name[160];sprintf_s(name,"test-runtime/previews/gpu-preview-%d.bgra",i);dump(r,name,false);check(r.present(),"preview present");
        }
        for(int state=0;state<3;++state){
            assert(WaitForSingleObject(r.latency,1000)==WAIT_OBJECT_0);
            f.mode_notice=-1;f.camera_notice=-1;f.helmet_notice=state;f.helmet_active=state==1;f.notice_alpha=1;
            check(r.draw(f,1.f/120,true),"HMD notice");
            char name[160];sprintf_s(name,"test-runtime/previews/hmd-%d.bgra",state);dump(r,name,false);check(r.present(),"HMD present");
        }
        assert(WaitForSingleObject(r.latency,1000)==WAIT_OBJECT_0);
        f.helmet_active=true;f.helmet_notice=-1;f.camera_notice=-1;f.mode_notice=-1;
        check(r.draw(f,1.f/120,true),"persistent HMD reticle after notice");
        dump(r,"test-runtime/previews/hmd-persistent.bgra",false);
        check(r.present(),"persistent HMD present");
        assert(WaitForSingleObject(r.latency,1000)==WAIT_OBJECT_0);
        f.ui_only=true;f.settings=true;f.smoothing=true;f.predicted=true;f.prediction_ms=6.5f;f.prediction_shift=3.2f;f.prediction_fallbacks=4;f.zoom=2.f;f.zoom_start=1.75f;f.sensitivity=.11f;f.configured_sensitivity=.1f;f.pose_age=8;f.fov=62;
        check(r.draw(f,1.f/120,true),"settings panel");dump(r,"test-runtime/previews/settings-panel.bgra",false);check(r.present(),"settings present");
        for(auto size:{std::pair<UINT,UINT>{640,360},{1280,720},{1920,1080},{3840,2160}}){
            assert(WaitForSingleObject(r.latency,1000)==WAIT_OBJECT_0);check(r.resize(size.first,size.second),"UI kit resize");
            f.settings=true;f.zoom_notice=2.f;f.sensitivity_notice=-1;f.mode_notice=-1;f.camera_notice=-1;f.helmet_notice=-1;
            check(r.draw(f,1.f/120,true),"UI kit panel and sensitivity toast");
            char name[160];sprintf_s(name,"test-runtime/previews/uikit-%ux%u.bgra",size.first,size.second);dump(r,name,false);check(r.present(),"UI kit present");
        }
        assert(WaitForSingleObject(r.latency,1000)==WAIT_OBJECT_0);check(r.resize(960,540),"UI kit restore");
        check(r.draw(f,1.f/120,true),"UI kit restore draw");check(r.present(),"UI kit restore present");
        f.selected_mode=0;f.mode_notice=0;f.camera_notice=-1;f.helmet_notice=-1;f.zoom_notice=-1;f.settings=true;
        check(r.draw(f,1.f/120,true),"PEACE policy panel");dump(r,"test-runtime/previews/peace-panel.bgra",false);check(r.present(),"capture present");
        f.selected_mode=3;f.mode_notice=3;check(r.draw(f,1.f/120,true),"WAR policy panel");dump(r,"test-runtime/previews/war-panel.bgra",false);check(r.present(),"vector present");
        for(int mode=0;mode<2;++mode){
            for(int kind=0;kind<2;++kind){assert(WaitForSingleObject(r.latency,1000)==WAIT_OBJECT_0);
                f.settings=false;f.zoom_notice=-1;f.sensitivity_notice=-1;f.mode_notice=kind==0?mode:-1;f.camera_notice=kind==1?mode:-1;
                check(r.draw(f,1.f/120,true),"UI kit switching card");check(r.present(),"UI kit switch present");}
        }


    }
    DestroyWindow(w);UnregisterClassW(wc.lpszClassName,wc.hInstance);CoUninitialize();
    puts("PASS transparent premultiplied pixels; own-buffer preview; resource teardown");
}


