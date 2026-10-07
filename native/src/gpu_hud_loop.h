#pragma once
#include "gpu_hud_renderer.h"
#include "hybrid_hud_policy.h"
#include "hud_prediction.h"
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
    HudFrameCache<HudFrame> cache;HudPacer pacer;hud_prediction::Predictor predictor;bool previous_requested=false;uint64_t predicted_count=0,fallback_count=0;float peak_shift=0;
    UINT interpolation_width=0,interpolation_height=0;float interpolation_fov=0;
    uint64_t window_check=0,last_draw=0,report=perf_clock(),drawn=0,repeats=0,source_frames=0,last_sequence=0,busy=0,misses=0,errors=0,total_gap=0,peak_gap=0;
    unsigned recovery=0;
    bool queue_retry=false;
    uint64_t fresh_reads=0,total_source_age=0;
    float measured_source_hz=0,measured_submit_hz=0;
    while(running.load()){
        queue_retry=false;
        MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        if(!game_window||!IsWindow(game_window)||GetTickCount64()-window_check>1000){if(HWND found=locate_game_window())game_window=found;window_check=GetTickCount64();}
        auto now=GetTickCount64();const bool ui_only=hud_renderer.load()==3;
        const bool has_notice=mode_notice_pending.load()||now<mode_notice_until.load()||now<camera_notice_until.load()||now<helmet_notice_until.load()||now<sensitivity_notice_until.load()||now<zoom_notice_until.load();
        bool wanted=game_window&&foreground_is_game()&&!IsIconic(game_window)&&active.load()&&enabled.load()&&hud_enabled.load()&&!game_paused.load()&&!gaze_active.load()&&!context_suspended.load()&&now-pose_tick.load()<250&&hybrid_hud::needs_overlay(ui_only,settings_panel.load(),has_notice);
        if(!wanted){predictor.reset();cache.reset();last_sequence=0;last_draw=0;}
        HudFrame received{};bool read=false;if(wanted&&!ui_only)read=read_hud_frame(received);if(wanted&&!ui_only){cache.observe(read,received);if(!read)++misses;}
        now=GetTickCount64();bool show=wanted&&(ui_only||cache.fresh(aircraft.load(),now));
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
                    HudFrame f{};f.tick=pose_tick.load();f.fov=view_fov.load();
                    gpu_hud::Layout frame;frame.ui_only=ui_only;frame.scale=std::max(.75f,height/1080.f);frame.opacity=hud_opacity.load();frame.connector=!ui_only&&hud_connector.load();frame.always=hud_link_always.load();frame.helmet_active=helmet_enabled.load();
                    if(!ui_only){
                    // Queue readiness may arrive after our initial snapshot: render the newest coherent POV.
                    HudFrame latest{};if(read_hud_frame(latest)){cache.observe(true,latest);++fresh_reads;}
                    now=GetTickCount64();
                    if(!cache.fresh(aircraft.load(),now)){pacer.wait(1000);continue;}
                    f=cache.value;
                    if(independent_mouse.load()){
                        std::lock_guard<std::recursive_mutex> lock(target_mutex);
                        if(f.pawn==aircraft.load()){f.tp=target_pitch.load();f.ty=target_yaw.load();}
                    }
                    const flight::V offset{f.ox,f.oy,f.oz};
                    const bool requested=hud_prediction_requested.load();
                    if(requested!=previous_requested||width!=interpolation_width||height!=interpolation_height){predictor.reset();previous_requested=requested;}
                    interpolation_width=width;interpolation_height=height;
                    const double clock_ms=1000.0/perf_frequency.QuadPart;
                    predictor.observe({f.pawn,f.camera_manager,f.epoch,f.camera_sequence,f.camera_qpc*clock_ms,f.cp,f.cy,f.cr,f.fov});
                    const bool eligible=!ui_only&&requested&&!manual_camera_active.load()&&!gaze_active.load()&&!context_suspended.load();
                    auto prediction=predictor.evaluate(hud_qpc()*clock_ms,flight::basis(f.tp,f.ty,0).f,flight::basis(f.p,f.y,f.r).f,offset,float(width),float(height),eligible);
                    auto view=prediction.applied?prediction.view:flight::basis(f.cp,f.cy,f.cr);
                    if(prediction.applied){++predicted_count;peak_shift=std::max(peak_shift,prediction.shift_px);}else if(requested)++fallback_count;
                    auto project=[&](flight::V v,hud_geometry::Point& p){v=v*50000-offset;float z=flight::dot(v,view.f);if(z<=.01f)return false;
                        float focal=width*.5f/std::tan(std::clamp(f.fov,15.f,150.f)*.5f*flight::rad);p={width*.5f+focal*flight::dot(v,view.r)/z,height*.5f-focal*flight::dot(v,view.u)/z};
                        return std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=0&&p.y>=0&&p.x<width&&p.y<height;};
                    frame.independent=independent_mouse.load();frame.smoothing=requested;frame.predicted=prediction.applied;frame.prediction_ms=prediction.horizon_ms;frame.prediction_shift=prediction.shift_px;frame.prediction_fallbacks=fallback_count;
                    frame.target=project(flight::basis(f.tp,f.ty,0).f,frame.target_point);frame.nose=project(flight::basis(f.p,f.y,f.r).f,frame.nose_point);
                    }
                    frame.source_hz=measured_source_hz;frame.submit_hz=measured_submit_hz;
                    const auto bindings=keybindings.load();frame.free_look_key=custom_keys::label(custom_keys::get(bindings,custom_keys::Action::FreeLook));frame.zoom_key=custom_keys::label(custom_keys::get(bindings,custom_keys::Action::Zoom));
                    frame.zoom=free_look_zoom.load();frame.zoom_start=configured_zoom.load();frame.hud_hz=hud_target_hz.load();
                    frame.settings=settings_panel.load();frame.sensitivity=live_sensitivity.load();frame.configured_sensitivity=configured_sensitivity.load();
                    frame.selected_mode=control_mode.load();frame.selected_camera=camera_view_mode.load();frame.fov=f.fov;frame.pose_age=unsigned(now-f.tick);
                    if(mode_notice_pending.exchange(false))mode_notice_until=now+2500;
                    auto notice_end=mode_notice_until.load();
                    if(now<notice_end){frame.mode_notice=control_mode.load();frame.notice_alpha=std::min(1.f,float(notice_end-now)/(creeperux::notice_exit*1000.f));}
                    auto camera_end=camera_notice_until.load();
                    if(now<camera_end&&camera_end>=notice_end){frame.camera_notice=camera_view_mode.load();frame.notice_alpha=std::min(1.f,float(camera_end-now)/(creeperux::notice_exit*1000.f));}
                    auto helmet_end=helmet_notice_until.load();
                    if(now<helmet_end&&helmet_end>=std::max(camera_end,notice_end)){frame.helmet_notice=helmet_notice.load();frame.notice_alpha=std::min(1.f,float(helmet_end-now)/(creeperux::notice_exit*1000.f));}
                    auto sensitivity_end=sensitivity_notice_until.load();
                    if(now<sensitivity_end&&sensitivity_end>=std::max({helmet_end,camera_end,notice_end})){
                        frame.sensitivity_notice=live_sensitivity.load();frame.notice_alpha=std::min(1.f,float(sensitivity_end-now)/(creeperux::notice_exit*1000.f));
                    }
                    auto zoom_end=zoom_notice_until.load();
                    if(now<zoom_end&&zoom_end>=std::max({sensitivity_end,helmet_end,camera_end,notice_end})){
                        frame.zoom_notice=free_look_zoom.load();frame.notice_alpha=std::min(1.f,float(zoom_end-now)/(creeperux::notice_exit*1000.f));
                    }
                    if(ui_only){frame.target=false;frame.nose=false;frame.connector=false;}
                    auto tick=perf_clock();float dt=last_draw?float(perf_us(tick-last_draw))/1e6f:1.f/120;
                    hr=renderer->draw(frame,dt);if(SUCCEEDED(hr))hr=renderer->present();
                    if(hr==DXGI_ERROR_WAS_STILL_DRAWING){++busy;queue_retry=true;}
                    else if(FAILED(hr)){
                        ++errors;log_line("GPU_HUD device error hr=%08lX",static_cast<unsigned long>(hr));ShowWindow(window,SW_HIDE);shown=false;renderer.reset();
                        if(++recovery>2){failed=true;break;}
                    }else{
                        if(last_draw){auto gap=perf_us(tick-last_draw);total_gap+=gap;peak_gap=std::max(peak_gap,gap);}last_draw=tick;++drawn;submit_gaps.event(tick,hud_frequency());if(!ui_only){const double clock_ms=1000.0/perf_frequency.QuadPart;camera_ages.sample(double(hud_qpc()-f.camera_qpc)*clock_ms);pose_ages.sample(double(hud_qpc()-f.pose_qpc)*clock_ms);}total_source_age+=GetTickCount64()-f.tick;
                        if(f.sequence==last_sequence)++repeats;else{if(last_sequence&&f.sequence>last_sequence)source_frames+=f.sequence-last_sequence;last_sequence=f.sequence;}
                    }
                }else {++busy;queue_retry=true;}
            }
        }else if(shown){ShowWindow(window,SW_HIDE);shown=false;}
        if(perf_us(perf_clock()-report)>=10000000){
            const double interval=double(perf_us(perf_clock()-report))/1e6;
            log_line("HUD_PIPELINE seconds=%.3f bridge_hz=%.2f stage_try_hz=%.2f stage_ok_hz=%.2f camera_hook_hz=%.2f camera_applied_hz=%.2f publish_try_hz=%.2f publish_ok_hz=%.2f submit_hz=%.2f smooth=%d (callback/CPU submissions, NOT engine render FPS)",interval,
                rate_bridge.exchange(0)/interval,rate_stage_try.exchange(0)/interval,rate_stage_ok.exchange(0)/interval,
                rate_camera.exchange(0)/interval,rate_camera_ok.exchange(0)/interval,rate_publish_try.exchange(0)/interval,rate_publish_ok.exchange(0)/interval,drawn/interval,hud_prediction_requested.load()?1:0);
            log_line("HUD_INPUT worker_hz=%.2f movement_update_hz=%.2f independent=%d display_prediction=%d raw_nonzero_hz=%.2f",rate_input_poll.exchange(0)/interval,rate_input_move.exchange(0)/interval,independent_mouse.load()?1:0,hud_prediction_requested.load()?1:0,rate_raw_move.exchange(0)/interval);
            measured_source_hz=float(source_frames/interval);measured_submit_hz=float(drawn/interval);
            log_line("GPU_HUD_CADENCE submitted=%llu avg_gap_ms=%.3f max_gap_ms=%.3f repeat_frames=%llu source_frames=%llu queue_not_ready=%llu read_misses=%llu errors=%llu (not measured display FPS)",drawn,drawn>1?double(total_gap)/(drawn-1)/1000:0,double(peak_gap)/1000,repeats,source_frames,busy,misses,errors);
            log_line("GPU_HUD_FRESHNESS mean_pose_age_ms=%.2f refreshed_reads=%llu target_hz=%d; queue retry target=1ms, latest snapshot before draw",drawn?double(total_source_age)/drawn:0,fresh_reads,hud_target_hz.load());
            log_line("HUD_PREDICTION display_frames=%llu fallback_frames=%llu peak_shift_px=%.3f requested=%d",predicted_count,fallback_count,peak_shift,hud_prediction_requested.load()?1:0);
            auto timing=[&](const char* name,HudTiming& h){auto v=h.take();log_line("HUD_TIMING metric=%s n=%llu p50_ms=%.2f p95_ms=%.2f p99_ms=%.2f (0.25ms bins,32 means32plus,idle gaps excluded)",name,v.n,v.p50,v.p95,v.p99);};
            timing("raw_arrival",raw_gaps);timing("target_update",target_gaps);timing("pose_receipt",pose_gaps);timing("camera_final",camera_gaps);timing("submit",submit_gaps);timing("camera_age",camera_ages);timing("pose_age",pose_ages);
            predicted_count=fallback_count=0;peak_shift=0;
            total_source_age=fresh_reads=0;
            drawn=repeats=source_frames=busy=misses=errors=total_gap=peak_gap=0;last_draw=0;report=perf_clock();
        }
        pacer.wait(hybrid_hud::pacing(wanted,queue_retry,ui_only,has_notice,hud_target_hz.load()));
    }
    if(shown)ShowWindow(window,SW_HIDE);renderer.reset();overlay_window=nullptr;DestroyWindow(window);UnregisterClassW(L"AC8GpuHud",self_module);return !failed;
}
void overlay_loop(){
    if(hud_renderer.load()==2){log_line("CANVAS_HUD selected: external overlay thread exits without creating a window/device/swapchain");return;}
    if(hud_renderer.load()==0){legacy_overlay_loop();return;}
    HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);bool ok=false;
    if(SUCCEEDED(com)){ok=run_gpu_hud();CoUninitialize();}
    else log_line("GPU_HUD COM init failed hr=%08lX",static_cast<unsigned long>(com));
    if(!ok&&running.load()&&hud_renderer.load()==3){log_line("HYBRID_HUD UI overlay unavailable; native reticle retained without legacy overlay fallback");return;}
    if(!ok&&running.load()){log_line("GPU_HUD fallback: legacy markers only; connector unavailable");legacy_overlay_loop();}
}
