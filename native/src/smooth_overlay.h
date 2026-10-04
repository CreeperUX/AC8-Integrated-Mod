// Small layered surfaces avoid clearing/submitting an entire 4K framebuffer.
// No per-frame file writes. Submission cadence is not display presentation FPS.
#include <mmsystem.h>
#include "hud_frame_cache.h"
#pragma comment(lib,"winmm.lib")
LRESULT CALLBACK smooth_overlay_proc(HWND w,UINT m,WPARAM a,LPARAM b) {
    if(m==WM_PAINT){PAINTSTRUCT p{};BeginPaint(w,&p);EndPaint(w,&p);return 0;}
    if(m==WM_ERASEBKGND)return 1;
    return DefWindowProcW(w,m,a,b);
}
struct HudSurface {
    HWND window=nullptr,owner=nullptr; HDC dc=nullptr;
    HBITMAP bitmap=nullptr; HGDIOBJ previous=nullptr;
    uint32_t* pixels=nullptr; int width=0,height=0;
    bool uploaded=false,shown=false;
    bool create() {
        window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
            L"AC8SmoothHud",L"AC8 Mouse Aim",WS_POPUP,0,0,1,1,nullptr,nullptr,self_module,nullptr);
        dc=CreateCompatibleDC(nullptr);return window && dc;
    }
    bool resize(int w,int h) {
        if(bitmap && width==w && height==h)return true;
        if(bitmap){SelectObject(dc,previous);DeleteObject(bitmap);bitmap=nullptr;}
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
        if(!bitmap)return false;
        previous=SelectObject(dc,bitmap);width=w;height=h;uploaded=false;return true;
    }
    bool place(POINT centre,bool visible,HDWP* batch){
        UINT flags=SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOOWNERZORDER|(visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW);
        if(batch){
            if(!*batch)return false;
            *batch=DeferWindowPos(*batch,window,HWND_TOPMOST,centre.x-width/2,centre.y-height/2,width,height,flags);
            shown=*batch && visible;return *batch!=nullptr;
        }
        bool ok=SetWindowPos(window,HWND_TOPMOST,centre.x-width/2,centre.y-height/2,width,height,flags)!=0;
        shown=ok && visible;return ok;
    }
    bool submit(HWND game,POINT centre,int kind,float scale,bool visible=true,HDWP* batch=nullptr) {
        // kind 0 = target ring, 1 = aircraft nose, 2 = offscreen notice.
        const int r=std::max(8,int(std::lround((kind==0?29:16)*scale)));
        if(!resize(kind==2?230:2*r+1,kind==2?28:2*r+1))return false;
        // Shape and alpha are immutable at a fixed scale. Move the cached
        // surface rather than repainting and uploading it every frame.
        if(uploaded){
            if(owner!=game){SetWindowLongPtrW(window,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(game));owner=game;}
            return place(centre,visible,batch);
        }
        GdiFlush();memset(pixels,0,size_t(width)*height*4);
        if(kind==2){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(245,245,245));
            const char* label="TARGET OUTSIDE VIEW";TextOutA(dc,6,6,label,int(strlen(label)));
        }else{
            auto pen1=CreatePen(PS_SOLID,std::max(1,int(std::lround(4*scale))),RGB(12,12,12));
            auto pen2=CreatePen(PS_SOLID,std::max(1,int(std::lround(2*scale))),RGB(255,255,255));
            auto oldpen=SelectObject(dc,pen1);auto brush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
            for(int pass=0;pass<2;++pass){
                if(pass)SelectObject(dc,pen2);
                if(kind==0){int n=int(std::lround(25*scale));Ellipse(dc,r-n,r-n,r+n,r+n);}
                else{int n=int(std::lround(12*scale)),g=int(std::lround(5*scale));
                    MoveToEx(dc,r-n,r,nullptr);LineTo(dc,r-g,r);MoveToEx(dc,r+g,r,nullptr);LineTo(dc,r+n,r);
                    MoveToEx(dc,r,r-n,nullptr);LineTo(dc,r,r-g);MoveToEx(dc,r,r+g,nullptr);LineTo(dc,r,r+n);}
            }
            SelectObject(dc,brush);SelectObject(dc,oldpen);DeleteObject(pen1);DeleteObject(pen2);
        }
        GdiFlush();for(size_t i=0;i<size_t(width)*height;++i)if(pixels[i]&0xFFFFFF)pixels[i]|=0xFF000000;
        if(owner!=game){SetWindowLongPtrW(window,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(game));owner=game;}
        POINT origin{centre.x-width/2,centre.y-height/2},source{};SIZE size{width,height};
        BLENDFUNCTION blend{AC_SRC_OVER,0,115,AC_SRC_ALPHA};
        bool ok=UpdateLayeredWindow(window,nullptr,&origin,&size,dc,&source,0,&blend,ULW_ALPHA)!=0;
        uploaded=ok;
        if(!ok){hide();return false;}
        return place(centre,visible,batch);
    }
    void hide(HDWP* batch=nullptr){
        if(!window || !shown)return;
        if(batch && *batch){
            *batch=DeferWindowPos(*batch,window,nullptr,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_HIDEWINDOW);
            shown=false;
        }else{ShowWindow(window,SW_HIDE);shown=false;}
    }
    ~HudSurface(){if(bitmap){SelectObject(dc,previous);DeleteObject(bitmap);}if(dc)DeleteDC(dc);if(window)DestroyWindow(window);}
};
struct HudPacer {
    HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
    bool high_resolution=timer!=nullptr,resolution=false;
    double next=double(perf_clock());
    HudPacer(){if(!timer){timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);resolution=timeBeginPeriod(1)==TIMERR_NOERROR;}}
    void wait(int hz){
        double period=double(perf_frequency.QuadPart)/hz;next+=period;
        double now=double(perf_clock());if(next<=now){next=now;return;}
        auto remaining=static_cast<long long>((next-now)*10000000.0/perf_frequency.QuadPart);
        LARGE_INTEGER due{};due.QuadPart=-std::max(1ll,remaining);
        if(timer && SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(timer,100);
        else Sleep(1);
    }
    ~HudPacer(){if(timer)CloseHandle(timer);if(resolution)timeEndPeriod(1);}
};
void legacy_overlay_loop() {
    WNDCLASSW wc{};wc.lpfnWndProc=smooth_overlay_proc;wc.hInstance=self_module;wc.lpszClassName=L"AC8SmoothHud";
    RegisterClassW(&wc);
    HudSurface ring,nose,notice;
    if(!ring.create()||!nose.create()||!notice.create()){log_line("HUD small surface creation failed");return;}
    overlay_window=ring.window;
    HudPacer pacer;
    log_line("HUD small surfaces target_hz=%d high_resolution_timer=%d",hud_target_hz.load(),pacer.high_resolution);
    auto report=perf_clock(),last=0ull; unsigned long long samples=0,sum=0,peak=0,over=0,failures=0,work_us=0;
    unsigned long long window_check=0;
    HudFrameCache<HudFrame> cache;
    uint64_t previous_sequence=0,source_frames=0,new_frames=0,repeats=0,read_misses=0,max_age_ms=0;
    uintptr_t previous_pawn=0;
    while(running.load()){
        auto start=perf_clock();MSG message{};
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        if(!game_window || !IsWindow(game_window) || GetTickCount64()-window_check>1000){
            if(HWND found=locate_game_window())game_window=found;window_check=GetTickCount64();}
        auto now=GetTickCount64();
        const bool wants_visible=game_window && foreground_is_game() && !IsIconic(game_window) && active.load() && enabled.load()
            && hud_enabled.load() && !game_paused.load() && !gaze_active.load() && now-pose_tick.load()<250;
        if(!wants_visible){cache.reset();previous_sequence=0;previous_pawn=0;}
        HudFrame incoming{};
        const bool received=read_hud_frame(incoming);
        if(wants_visible){cache.observe(received,incoming);if(!received)++read_misses;}
        now=GetTickCount64();
        bool show=wants_visible && cache.fresh(aircraft.load(),now);
        const HudFrame& frame=cache.value;
        if(show){
            RECT client{};GetClientRect(game_window,&client);POINT origin{};ClientToScreen(game_window,&origin);
            const int w=client.right,h=client.bottom;
            const auto view=flight::basis(frame.cp,frame.cy,frame.cr);
            const flight::V offset{frame.ox,frame.oy,frame.oz};
            auto project=[&](flight::V v,POINT& out){
                v=v*50000.0f-offset;float depth=flight::dot(v,view.f);
                if(depth<=0.01f||w<=0||h<=0)return false;
                float focal=w*0.5f/std::tan(std::clamp(frame.fov,30.0f,150.0f)*0.5f*flight::rad);
                float x=w*0.5f+focal*flight::dot(v,view.r)/depth,y=h*0.5f-focal*flight::dot(v,view.u)/depth;
                if(!std::isfinite(x)||!std::isfinite(y)||x<0||y<0||x>=w||y>=h)return false;
                out={origin.x+LONG(std::lround(x)),origin.y+LONG(std::lround(y))};return true;};
            if(previous_pawn!=frame.pawn){previous_sequence=0;previous_pawn=frame.pawn;}
            if(frame.sequence!=previous_sequence){
                ++new_frames;if(previous_sequence && frame.sequence>previous_sequence)source_frames+=frame.sequence-previous_sequence;
                previous_sequence=frame.sequence;
            }else ++repeats;
            max_age_ms=std::max(max_age_ms,uint64_t(now-frame.tick));
            POINT pt{};float scale=std::max(0.75f,h/1080.0f);
            HDWP batch=BeginDeferWindowPos(3);bool submitted=batch!=nullptr;
            if(project(flight::basis(frame.tp,frame.ty,0).f,pt)){
                submitted=ring.submit(game_window,pt,0,scale,true,&batch);notice.hide(&batch);
            }else{ring.hide(&batch);submitted=notice.submit(game_window,{origin.x+145,origin.y+70},2,scale,true,&batch);}
            nose.hide(&batch); // Use the game's native crosshair; no duplicate Mod cross.
            if(batch)submitted=(EndDeferWindowPos(batch)!=0)&&submitted;else submitted=false;
            if(!submitted){ring.shown=nose.shown=notice.shown=true;ring.hide();nose.hide();notice.hide();}
            if(submitted){auto stamp=perf_clock();if(last){auto us=perf_us(stamp-last);++samples;sum+=us;peak=std::max(peak,us);over+=us>16667;}last=stamp;}
            else{++failures;last=0;}
        }else{ring.hide();nose.hide();notice.hide();last=0;}
        work_us+=perf_us(perf_clock()-start);
        if(perf_us(perf_clock()-report)>=10000000){
            log_line("HUD_CADENCE target_hz=%d samples=%llu update_fps=%.2f avg_ms=%.3f max_ms=%.3f gaps_over_16_67ms=%llu failures=%llu work_ms=%.3f (cached window updates, not display FPS)",
                hud_target_hz.load(),samples,sum?1e6*double(samples)/sum:0,samples?double(sum)/samples/1000:0,double(peak)/1000,over,failures,double(work_us)/1000);
            log_line("HUD_SOURCE source_hz=%.2f new_frames=%llu repeated_updates=%llu transient_read_misses=%llu max_pose_age_ms=%llu",sum?1e6*double(source_frames)/sum:0,new_frames,repeats,read_misses,max_age_ms);
            source_frames=new_frames=repeats=read_misses=max_age_ms=0;previous_sequence=0;
            samples=sum=peak=over=failures=work_us=0;report=perf_clock();last=0;
        }
        // Absolute QPC deadline: work time does not add to every frame period.
        pacer.wait(HudFrameCache<HudFrame>::pacing(wants_visible,hud_target_hz.load()));
    }
    overlay_window=nullptr;
}
