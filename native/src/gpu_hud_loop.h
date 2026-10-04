#pragma once
#include "gpu_hud_renderer.h"
LRESULT CALLBACK gpu_hud_proc(HWND w,UINT msg,WPARAM a,LPARAM b){
    if(msg==WM_NCHITTEST)return HTTRANSPARENT;
    if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){PAINTSTRUCT p{};BeginPaint(w,&p);EndPaint(w,&p);return 0;}
    return DefWindowProcW(w,msg,a,b);
}
bool run_gpu_hud(){
    WNDCLASSW wc{};wc.lpfnWndProc=gpu_hud_proc;wc.hInstance=self_module;wc.lpszClassName=L"AC8GpuHud";RegisterClassW(&wc);
    HWND window=CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"AC8GpuHud",L"AC8 GPU HUD",WS_POPUP,0,0,1,1,nullptr,nullptr,self_module,nullptr);
    if(!window)return false;
    SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);overlay_window=window;
    bool shown=false,failed=false;HWND owner=nullptr;RECT last_rect{};POINT last_origin{};
    std::unique_ptr<gpu_hud::Renderer> renderer;
    HudFrameCache<HudFrame> cache;HudPacer pacer;
    uint64_t window_check=0,last_draw=0,report=perf_clock(),drawn=0,repeats=0,source_frames=0,last_sequence=0,busy=0,misses=0,errors=0,total_gap=0,peak_gap=0;
    unsigned recovery=0;
    while(running.load()){
        MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        if(!game_window||!IsWindow(game_window)||GetTickCount64()-window_check>1000){if(HWND found=locate_game_window())game_window=found;window_check=GetTickCount64();}
        auto now=GetTickCount64();bool wanted=game_window&&foreground_is_game()&&!IsIconic(game_window)&&active.load()&&enabled.load()&&hud_enabled.load()&&!game_paused.load()&&!gaze_active.load()&&now-pose_tick.load()<250;
        if(!wanted){cache.reset();last_sequence=0;last_draw=0;}
        HudFrame received{};bool read=read_hud_frame(received);if(wanted){cache.observe(read,received);if(!read)++misses;}
        now=GetTickCount64();bool show=wanted&&cache.fresh(aircraft.load(),now);
        if(show){
            RECT client{};POINT origin{};GetClientRect(game_window,&client);ClientToScreen(game_window,&origin);
            UINT width=UINT(std::max(0L,client.right)),height=UINT(std::max(0L,client.bottom));
            if(width&&height){
                if(owner!=game_window){SetWindowLongPtrW(window,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(game_window));owner=game_window;}
                if(!renderer){
                    renderer=std::make_unique<gpu_hud::Renderer>();HRESULT hr=renderer->initialize(window,width,height);
                    if(FAILED(hr)){log_line("GPU_HUD init failed hr=%08lX; legacy fallback",static_cast<unsigned long>(hr));failed=true;break;}
                    log_line("GPU_HUD ready: D3D11 + Direct2D + DirectComposition; premultiplied flip chain; frame_latency=1");
                }
                HRESULT hr=renderer->resize(width,height);
                if(FAILED(hr)){log_line("GPU_HUD resize failed hr=%08lX",static_cast<unsigned long>(hr));failed=true;break;}
                if(!shown||origin.x!=last_origin.x||origin.y!=last_origin.y||client.right!=last_rect.right||client.bottom!=last_rect.bottom){
                    SetWindowPos(window,HWND_TOPMOST,origin.x,origin.y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);shown=true;last_origin=origin;last_rect=client;
                }
                if(renderer->ready()){
                    const auto& f=cache.value;const auto view=flight::basis(f.cp,f.cy,f.cr);const flight::V offset{f.ox,f.oy,f.oz};
                    auto project=[&](flight::V v,hud_geometry::Point& p){v=v*50000-offset;float z=flight::dot(v,view.f);if(z<=.01f)return false;
                        float focal=width*.5f/std::tan(std::clamp(f.fov,30.f,150.f)*.5f*flight::rad);p={width*.5f+focal*flight::dot(v,view.r)/z,height*.5f-focal*flight::dot(v,view.u)/z};
                        return std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=0&&p.y>=0&&p.x<width&&p.y<height;};
                    gpu_hud::Layout frame;frame.scale=std::max(.75f,height/1080.f);frame.opacity=hud_opacity.load();frame.connector=hud_connector.load();frame.always=hud_link_always.load();
                    if(mode_notice_pending.exchange(false))mode_notice_until=now+2500;
                    auto notice_end=mode_notice_until.load();
                    if(now<notice_end){frame.mode_notice=control_mode.load();frame.notice_alpha=std::min(1.f,float(notice_end-now)/400.f);}
                    frame.target=project(flight::basis(f.tp,f.ty,0).f,frame.target_point);frame.nose=project(flight::basis(f.p,f.y,f.r).f,frame.nose_point);
                    auto tick=perf_clock();float dt=last_draw?float(perf_us(tick-last_draw))/1e6f:1.f/120;
                    hr=renderer->draw(frame,dt);if(SUCCEEDED(hr))hr=renderer->present();
                    if(hr==DXGI_ERROR_WAS_STILL_DRAWING){++busy;}
                    else if(FAILED(hr)){
                        ++errors;log_line("GPU_HUD device error hr=%08lX",static_cast<unsigned long>(hr));ShowWindow(window,SW_HIDE);shown=false;renderer.reset();
                        if(++recovery>2){failed=true;break;}
                    }else{
                        if(last_draw){auto gap=perf_us(tick-last_draw);total_gap+=gap;peak_gap=std::max(peak_gap,gap);}last_draw=tick;++drawn;
                        if(f.sequence==last_sequence)++repeats;else{if(last_sequence&&f.sequence>last_sequence)source_frames+=f.sequence-last_sequence;last_sequence=f.sequence;}
                    }
                }else ++busy;
            }
        }else if(shown){ShowWindow(window,SW_HIDE);shown=false;}
        if(perf_us(perf_clock()-report)>=10000000){
            log_line("GPU_HUD_CADENCE submitted=%llu avg_gap_ms=%.3f max_gap_ms=%.3f repeat_frames=%llu source_frames=%llu queue_not_ready=%llu read_misses=%llu errors=%llu (not measured display FPS)",drawn,drawn>1?double(total_gap)/(drawn-1)/1000:0,double(peak_gap)/1000,repeats,source_frames,busy,misses,errors);
            drawn=repeats=source_frames=busy=misses=errors=total_gap=peak_gap=0;last_draw=0;report=perf_clock();
        }
        pacer.wait(wanted?hud_target_hz.load():10);
    }
    if(shown)ShowWindow(window,SW_HIDE);renderer.reset();overlay_window=nullptr;DestroyWindow(window);UnregisterClassW(L"AC8GpuHud",self_module);return !failed;
}
void overlay_loop(){
    if(hud_renderer.load()==0){legacy_overlay_loop();return;}
    HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);bool ok=false;
    if(SUCCEEDED(com)){ok=run_gpu_hud();CoUninitialize();}
    else log_line("GPU_HUD COM init failed hr=%08lX",static_cast<unsigned long>(com));
    if(!ok&&running.load()){log_line("GPU_HUD fallback: legacy markers only; connector unavailable");legacy_overlay_loop();}
}
