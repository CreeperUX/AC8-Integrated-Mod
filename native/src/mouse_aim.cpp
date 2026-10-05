// AC8 Mouse Aim: offline-only closed-loop mouse flight control for ACE COMBAT 8.
// Reads aircraft attitude supplied by UE4SS Lua, captures non-exclusive mouse
// deltas, and replaces only the three player input axes. Flight-model state is
// never edited.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <mutex>
#include <array>
#include "vendor/minhook/include/MinHook.h"
#include "yaw_signature.h"
#include "flight_math.h"
#include "free_look.h"
#include "paired_mouse.h"
#include "hud_timing.h"
#include "lua_bridge.h"
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>
#include <dwrite.h>
#pragma comment(lib,"dwrite.lib")
#include <dxgi1_3.h>
#include <d2d1_1.h>
#include <dcomp.h>
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dcomp.lib")
#pragma comment(lib,"ole32.lib")

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace {
using Processor = uintptr_t(__fastcall*)(unsigned char*, unsigned char*);
using GetRawInputDataFn = UINT(WINAPI*)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);

struct Config {
    float sensitivity = 0.10f;
    float mouse_reference_fov=62.0f;
    float arrival_braking=1.15f;
    float model_assist_strength=.20f;
    float roll_gain = 0.022f;
    float pitch_gain = 0.026f;
    float yaw_gain = 0.012f;
    float turn_pull = 0.42f;
    float max_bank = 65.0f;
    float smoothing = 0.12f;
    float roll_damping = 0.008f;
    float pitch_damping = 0.010f;
    float yaw_damping = 0.30f;
    float dead_zone = 1.25f;
    float pitch_sign = 1.0f;
    float roll_sign = -1.0f;
    float yaw_sign = -1.0f;
    float horizontal_fov = 100.0f;
    float vertical_fov = 62.0f;
    int pitch_slot = 0;
    int roll_slot = 2;
};

struct Pose {
    uintptr_t pawn = 0;
    float pitch = 0, yaw = 0, roll = 0;
    unsigned long long tick = 0;
};

struct HudFrame {
    uintptr_t pawn=0; unsigned long long tick=0;
    float p=0,y=0,r=0,cp=0,cy=0,cr=0,tp=0,ty=0,fov=100,ox=0,oy=0,oz=0;
    uint64_t sequence=0,pose_qpc=0,camera_qpc=0,camera_sequence=0,epoch=0;uintptr_t camera_manager=0;
};
inline uint64_t hud_qpc(){LARGE_INTEGER t{};QueryPerformanceCounter(&t);return uint64_t(t.QuadPart);}
inline uint64_t hud_frequency(){static const uint64_t f=[](){LARGE_INTEGER v{};QueryPerformanceFrequency(&v);return uint64_t(v.QuadPart);}();return f;}
HudTiming raw_gaps,target_gaps,pose_gaps,camera_gaps,submit_gaps,camera_ages,pose_ages;
std::atomic<uint64_t> hud_epoch{1},camera_sequence{0};
std::mutex hud_frame_mutex;
HudFrame hud_frame,pending_hud_frame;
std::atomic<uint64_t> hud_source_sequence{0};
std::recursive_mutex target_mutex;
std::atomic<bool> independent_mouse{false};
std::atomic<float> input_reference_fov{62};
std::atomic<uint64_t> input_goal_sequence{0},rate_input_poll{0},rate_input_move{0},rate_raw_move{0};
flight::V control_last_goal{1,0,0};bool control_goal_seen=false;uint64_t control_last_sequence=0;
void target_input_tick();
std::atomic<uint64_t> rate_bridge{0},rate_stage_try{0},rate_stage_ok{0},rate_camera{0},rate_camera_ok{0},rate_publish_try{0},rate_publish_ok{0};
void stage_hud_frame(const HudFrame& frame){
    rate_stage_try.fetch_add(1,std::memory_order_relaxed);
    std::unique_lock<std::mutex> lock(hud_frame_mutex,std::try_to_lock);
    if(lock.owns_lock()){pending_hud_frame=frame;pending_hud_frame.pose_qpc=frame.pose_qpc?frame.pose_qpc:hud_qpc();pending_hud_frame.epoch=hud_epoch.load();pending_hud_frame.sequence=++hud_source_sequence;rate_stage_ok.fetch_add(1,std::memory_order_relaxed);}
}
bool read_pending_hud_frame(HudFrame& frame){
    std::unique_lock<std::mutex> lock(hud_frame_mutex,std::try_to_lock);
    if(!lock.owns_lock())return false;
    frame=pending_hud_frame;return frame.pawn!=0;
}
void publish_hud_frame(const HudFrame& frame){
    rate_publish_try.fetch_add(1,std::memory_order_relaxed);
    std::unique_lock<std::mutex> lock(hud_frame_mutex,std::try_to_lock);
    if(lock.owns_lock()){hud_frame=frame;rate_publish_ok.fetch_add(1,std::memory_order_relaxed);}
}
bool read_hud_frame(HudFrame& frame){
    std::unique_lock<std::mutex> lock(hud_frame_mutex,std::try_to_lock);
    if(!lock.owns_lock())return false;
    frame=hud_frame;return frame.pawn!=0;
}

HMODULE self_module{};
Processor original{};
void* hook_address{};
bool hook_created = false;
std::atomic<bool> running{false};
std::atomic<bool> enabled{true};
std::atomic<bool> hud_enabled{true};
std::atomic<bool> game_paused{false};
std::atomic<bool> gaze_active{false};
std::atomic<bool> context_suspended{true},transition_center_requested{false};
std::atomic<bool> manual_camera_active{false},native_rig_reset_requested{true};
std::atomic<float> free_look_zoom{1.75f},effective_camera_fov{0};
std::atomic<float> configured_zoom{1.75f};
std::atomic<uint64_t> zoom_notice_until{0};
void zoom_keys(bool up,bool down,bool reset,bool allowed);
std::atomic<uint64_t> effective_fov_tick{0};
std::atomic<uintptr_t> effective_fov_manager{0};
std::atomic<int> camera_view_mode{1};
std::atomic<float> camera_distance_cm{3600},camera_height_cm{600};
void sensitivity_keys(bool up,bool down,bool reset,bool allowed);
std::atomic<float> live_sensitivity{.10f},configured_sensitivity{.10f};
std::atomic<uint64_t> sensitivity_notice_until{0};
std::atomic<bool> settings_panel{false},hud_prediction_requested{true};
std::atomic<bool> camera_toggle_requested{false};
std::atomic<uint64_t> camera_notice_until{0};
std::atomic<bool> helmet_enabled{false};
std::atomic<int> helmet_notice{-1};
std::atomic<uint64_t> helmet_notice_until{0};
std::atomic<bool> resume_center_requested{false};
std::atomic<bool> active{false};
std::atomic<bool> model_assist_enabled{true},assist_environment_unsafe{true};
std::atomic<int> control_mode{1}; // 0: v2.0 limits; 1: current agility schedule.
std::atomic<float> control_mode_blend{1};
std::atomic<bool> mode_notice_pending{true};
std::atomic<uint64_t> mode_notice_until{0};
std::atomic<float> assist_speed{0},assist_brake{1},assist_delta_pitch{0},assist_delta_roll{0};
std::atomic<uint64_t> assist_speed_tick{0};
std::atomic<uintptr_t> assist_speed_pawn{0};
std::atomic<int> assist_plane_type{-1},assist_profile{0};
std::atomic<float> assist_conf_pitch{0},assist_conf_roll{0},assist_gain_pitch{1},assist_gain_roll{1};
std::atomic<float> assist_learn_tau_pitch{1},assist_learn_tau_roll{1},assist_learn_weight_pitch{0},assist_learn_weight_roll{0};
std::atomic<unsigned> assist_learn_promotions_pitch{0},assist_learn_promotions_roll{0};
std::atomic<float> assist_conf_yaw{0},assist_gain_yaw{1},assist_learn_tau_yaw{.72080938f},assist_learn_weight_yaw{0};
std::atomic<unsigned> assist_learn_promotions_yaw{0};
std::array<std::atomic<float>,3> assist_learn_delay{},assist_primary_weight{},assist_primary_command{},assist_primary_delta{},assist_primary_stop{},assist_primary_horizon{},assist_primary_ratecap{};
std::array<std::atomic<float>,3> assist_primary_checks{},assist_primary_model_rmse{},assist_primary_base_rmse{};
std::array<std::atomic<float>,3> assist_reference_rate{};
std::array<std::atomic<float>,3> assist_candidate_weight{},assist_model_source{},assist_deployed_models{};
std::array<std::atomic<float>,3> assist_pursuit{},assist_effective_damping{},assist_input_limit{};
std::atomic<uintptr_t> aircraft{0};
std::atomic<float> pose_pitch{0}, pose_yaw{0}, pose_roll{0};
std::atomic<float> camera_pitch{0}, camera_yaw{0}, camera_roll{0};
std::atomic<float> view_fov{100};
std::atomic<float> view_offset_x{0}, view_offset_y{0}, view_offset_z{0};
std::atomic<float> roll_reference{0};
std::atomic<unsigned long long> pose_tick{0};
PairedMouse mouse_delta;
std::atomic<float> command_pitch{0}, command_roll{0}, command_yaw{0};
std::atomic<float> target_pitch{0}, target_yaw{0};
// Camera destination is independent of the flight target while C is held.
std::atomic<float> look_pitch{0}, look_yaw{0};
flight::FreeLook free_look;
std::atomic<bool> recenter_requested{true};
float previous_pitch{}, previous_yaw{}, previous_roll{};
float filtered_pitch_rate{}, filtered_yaw_rate{}, filtered_roll_rate{};
flight::LevelBlend roll_level_blend;
flight::ArrivalState pitch_arrival,roll_arrival;
unsigned long long previous_pose_tick{}, telemetry_tick{};
Config config;
std::atomic<int> hud_target_hz{120},hud_renderer{1};
std::atomic<bool> hud_connector{true},hud_link_always{false};
std::atomic<float> hud_opacity{.65f};
wchar_t module_folder[MAX_PATH]{};
wchar_t status_path[MAX_PATH]{};
wchar_t config_path[MAX_PATH]{};
wchar_t request_path[MAX_PATH]{};
HANDLE journal = INVALID_HANDLE_VALUE;
HWND game_window{};
HWND overlay_window{};
IDirectInput8W* direct_input{};
IDirectInputDevice8W* mouse_device{};
GetRawInputDataFn original_get_raw_input_data{};
std::atomic<bool> raw_input_hooked{false};

float clamp_axis(float value) { return std::clamp(value, -1.0f, 1.0f); }
float wrap_degrees(float value) {
    while (value > 180.0f) value -= 360.0f;
    while (value < -180.0f) value += 360.0f;
    return value;
}

void init_paths() {
    if (module_folder[0]) return;
    GetModuleFileNameW(self_module, module_folder, MAX_PATH);
    if (auto* slash = wcsrchr(module_folder, L'\\')) *slash = 0;
    swprintf_s(status_path, L"%s\\mouse-aim-status.txt", module_folder);
    swprintf_s(config_path, L"%s\\..\\config.ini", module_folder);
    swprintf_s(request_path, L"%s\\mouse-aim-request.txt", module_folder);
}

void shadow_drain();
#include "async_diagnostics.h"
#include "shadow_recorder.h"
#include "shared_assist_runtime.h"
#include "full_model_runtime.h"

float read_config_float(const wchar_t* key, float fallback) {
    wchar_t value[64]{};
    wchar_t fallback_text[64]{};
    swprintf_s(fallback_text, L"%.6f", fallback);
    GetPrivateProfileStringW(L"control", key, fallback_text, value, 64, config_path);
    wchar_t* end{};
    float parsed = wcstof(value, &end);
    return end != value && std::isfinite(parsed) ? parsed : fallback;
}

int read_config_int(const wchar_t* key, int fallback) {
    return GetPrivateProfileIntW(L"control", key, fallback, config_path);
}

void load_config() {
    init_paths();
    camera_view_mode.store(std::clamp(read_config_int(L"camera_mode",1),0,1));
    camera_distance_cm.store(100*std::clamp(read_config_float(L"camera_distance_m",36),10.f,100.f));
    camera_height_cm.store(100*std::clamp(read_config_float(L"camera_height_m",6),0.f,20.f));
    hud_target_hz.store(std::clamp(read_config_int(L"hud_fps",120),60,240));
    free_look_zoom=std::clamp(read_config_float(L"free_look_zoom",1.75f),1.f,3.f);configured_zoom=free_look_zoom.load();
    config.mouse_reference_fov=std::clamp(read_config_float(L"mouse_reference_fov",62),30.0f,150.0f);input_reference_fov=config.mouse_reference_fov;
    config.arrival_braking=std::clamp(read_config_float(L"arrival_braking",1.15f),1.0f,1.35f);
    model_assist_enabled.store(read_config_int(L"model_assist",1)!=0);
    control_mode.store(std::clamp(read_config_int(L"control_mode",1),0,1));mode_notice_pending=true;
    config.model_assist_strength=std::clamp(read_config_float(L"model_assist_strength",.20f),0.f,.35f);
    hud_renderer.store(std::clamp(read_config_int(L"hud_renderer",3),0,3));
    hud_connector.store(read_config_int(L"hud_connector",1)!=0);
    hud_link_always.store(read_config_int(L"hud_link_always",0)!=0);
    hud_opacity.store(std::clamp(read_config_float(L"hud_opacity",.65f),.15f,1.f));
    config.sensitivity = std::clamp(read_config_float(L"sensitivity", config.sensitivity), 0.01f, 1.0f);
    live_sensitivity=config.sensitivity;configured_sensitivity=config.sensitivity;
    config.roll_gain = std::clamp(read_config_float(L"roll_gain", config.roll_gain), 0.001f, 0.2f);
    config.pitch_gain = std::clamp(read_config_float(L"pitch_gain", config.pitch_gain), 0.001f, 0.2f);
    config.yaw_gain = std::clamp(read_config_float(L"yaw_gain", config.yaw_gain), 0.0f, 0.2f);
    config.turn_pull = std::clamp(read_config_float(L"turn_pull", config.turn_pull), 0.0f, 1.0f);
    config.max_bank = std::clamp(read_config_float(L"max_bank", config.max_bank), 20.0f, 89.0f);
    config.smoothing = std::clamp(read_config_float(L"smoothing", config.smoothing), 0.02f, 1.0f);
    config.roll_damping = std::clamp(read_config_float(L"roll_damping", config.roll_damping), 0.0f, 0.05f);
    config.pitch_damping = std::clamp(read_config_float(L"pitch_damping", config.pitch_damping), 0.0f, 0.05f);
    config.yaw_damping = std::clamp(read_config_float(L"yaw_damping", config.yaw_damping), 0.0f, 2.0f);
    config.dead_zone = std::clamp(read_config_float(L"dead_zone", config.dead_zone), 0.1f, 5.0f);
    config.pitch_sign = read_config_float(L"pitch_sign", config.pitch_sign) < 0 ? -1.0f : 1.0f;
    config.roll_sign = read_config_float(L"roll_sign", config.roll_sign) < 0 ? -1.0f : 1.0f;
    config.yaw_sign = read_config_float(L"yaw_sign", config.yaw_sign) < 0 ? -1.0f : 1.0f;
    config.horizontal_fov = std::clamp(read_config_float(L"horizontal_fov", config.horizontal_fov), 40.0f, 170.0f);
    config.vertical_fov = std::clamp(read_config_float(L"vertical_fov", config.vertical_fov), 30.0f, 120.0f);
    config.pitch_slot = std::clamp(read_config_int(L"pitch_slot", config.pitch_slot), 0, 2);
    config.roll_slot = std::clamp(read_config_int(L"roll_slot", config.roll_slot), 0, 2);
    if (config.roll_slot == config.pitch_slot || config.pitch_slot == 1 || config.roll_slot == 1) {
        config.pitch_slot = 0;
        config.roll_slot = 2;
    }
}

struct WindowCandidate {
    HWND window{};
    long long area{};
};

BOOL CALLBACK find_game_window(HWND window, LPARAM result) {
    DWORD process{};
    GetWindowThreadProcessId(window, &process);
    if (process == GetCurrentProcessId() && window != overlay_window &&
        IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) {
        wchar_t class_name[64]{};
        GetClassNameW(window, class_name, 64);
        if (wcscmp(class_name, L"AC8MouseAimOverlay") == 0) return TRUE;
        RECT client{};
        if (GetClientRect(window, &client)) {
            const long long width = client.right - client.left;
            const long long height = client.bottom - client.top;
            const long long area = width * height;
            auto* candidate = reinterpret_cast<WindowCandidate*>(result);
            if (width >= 640 && height >= 360 && area > candidate->area) {
                candidate->window = window;
                candidate->area = area;
            }
        }
    }
    return TRUE;
}

HWND locate_game_window() {
    WindowCandidate candidate{};
    EnumWindows(find_game_window, reinterpret_cast<LPARAM>(&candidate));
    return candidate.window;
}

bool foreground_is_game() {
    HWND foreground = GetForegroundWindow();
    DWORD process{};
    GetWindowThreadProcessId(foreground, &process);
    const bool is_game = process == GetCurrentProcessId() && foreground != overlay_window;
    if (is_game) game_window = foreground;
    return is_game;
}

UINT WINAPI capture_get_raw_input_data(HRAWINPUT input, UINT command, LPVOID data,
                                       PUINT size, UINT header_size) {
    const UINT result = original_get_raw_input_data(input, command, data, size, header_size);
    if (result != static_cast<UINT>(-1) && command == RID_INPUT && data &&
        result >= sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE)) {
        const auto* raw = static_cast<const RAWINPUT*>(data);
        if (raw->header.dwType == RIM_TYPEMOUSE &&
            !(raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) &&
            foreground_is_game() && active.load() && enabled.load()) {
            if(!game_paused.load() && !gaze_active.load() && !context_suspended.load()) {
                std::lock_guard<std::recursive_mutex> lock(target_mutex);
                mouse_delta.add(raw->data.mouse.lLastX,raw->data.mouse.lLastY);
                if(raw->data.mouse.lLastX||raw->data.mouse.lLastY){rate_raw_move.fetch_add(1,std::memory_order_relaxed);raw_gaps.event(hud_qpc(),hud_frequency());}
            }
        }
    }
    return result;
}

bool prepare_raw_input_capture() {
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress) return false;
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
    for (; descriptor->Name; ++descriptor) {
        if (!descriptor->OriginalFirstThunk) continue;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->OriginalFirstThunk);
        auto* imports = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++imports) {
            if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)) continue;
            const auto* by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (strcmp(reinterpret_cast<const char*>(by_name->Name), "GetRawInputData") != 0) continue;
            original_get_raw_input_data = reinterpret_cast<GetRawInputDataFn>(imports->u1.Function);
            DWORD previous{};
            if (!VirtualProtect(&imports->u1.Function, sizeof(imports->u1.Function),
                                PAGE_READWRITE, &previous)) return false;
            InterlockedExchangePointer(reinterpret_cast<void**>(&imports->u1.Function),
                                       reinterpret_cast<void*>(&capture_get_raw_input_data));
            DWORD ignored{};
            VirtualProtect(&imports->u1.Function, sizeof(imports->u1.Function), previous, &ignored);
            raw_input_hooked.store(true);
            return true;
        }
    }
    return false;
}

bool prepare_mouse() {
    if (!game_window) game_window = locate_game_window();
    if (!game_window) return false;
    if (!direct_input && FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION,
            IID_IDirectInput8W, reinterpret_cast<void**>(&direct_input), nullptr))) return false;
    if (!mouse_device && FAILED(direct_input->CreateDevice(GUID_SysMouse, &mouse_device, nullptr))) return false;
    if (FAILED(mouse_device->SetDataFormat(&c_dfDIMouse2))) return false;
    if (FAILED(mouse_device->SetCooperativeLevel(game_window, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND))) return false;
    return SUCCEEDED(mouse_device->Acquire()) || GetLastError() == ERROR_SUCCESS;
}

void mouse_loop() {
    HANDLE input_timer=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
    log_line("INPUT_SOURCE Raw Input preferred; independent target worker; DirectInput fallback only if raw hook unavailable");
    bool f8_down = false, f9_down = false;
    while (running.load()) {
        // Independent consumption does not require replacing the proven capture source.
        // Use exactly one producer: game Raw Input, or DirectInput only when the hook is unavailable.
        independent_mouse=true;
        if(!raw_input_hooked.load()&&(mouse_device||prepare_mouse())){
            DIMOUSESTATE2 state{};HRESULT result=mouse_device->GetDeviceState(sizeof(state),&state);
            if(SUCCEEDED(result)){
                std::lock_guard<std::recursive_mutex> lock(target_mutex);
                if(foreground_is_game()&&active.load()&&enabled.load()&&!game_paused.load()&&!gaze_active.load()&&!context_suspended.load())mouse_delta.add(state.lX,state.lY);
            }else mouse_device->Acquire();
        }
        rate_input_poll.fetch_add(1,std::memory_order_relaxed);
        target_input_tick();
        static bool f7_down=false;
        bool f7=(GetAsyncKeyState(VK_F7)&0x8000)!=0;
        if(f7 && !f7_down && foreground_is_game()) {
            hud_enabled.store(!hud_enabled.load());
            log_line("HUD only: %s",hud_enabled.load()?"ON":"OFF");
        }
        f7_down=f7;
        bool plain=(GetAsyncKeyState(VK_MENU)&0x8000)==0&&(GetAsyncKeyState(VK_CONTROL)&0x8000)==0&&(GetAsyncKeyState(VK_SHIFT)&0x8000)==0;
        const bool sensitivity_allowed=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0 &&
            (GetAsyncKeyState(VK_MENU)&0x8000)==0 && (GetAsyncKeyState(VK_SHIFT)&0x8000)==0 &&
            foreground_is_game()&&enabled.load()&&active.load()&&!game_paused.load()&&!gaze_active.load()&&!context_suspended.load();
        sensitivity_keys((GetAsyncKeyState(VK_PRIOR)&0x8000)!=0,(GetAsyncKeyState(VK_NEXT)&0x8000)!=0,
            (GetAsyncKeyState(VK_HOME)&0x8000)!=0,sensitivity_allowed);
        const bool zoom_allowed=(GetAsyncKeyState(VK_MENU)&0x8000)!=0 &&
            (GetAsyncKeyState(VK_CONTROL)&0x8000)==0 && (GetAsyncKeyState(VK_SHIFT)&0x8000)==0 &&
            foreground_is_game()&&enabled.load()&&active.load()&&!game_paused.load()&&!gaze_active.load()&&!context_suspended.load();
        zoom_keys((GetAsyncKeyState(VK_PRIOR)&0x8000)!=0,(GetAsyncKeyState(VK_NEXT)&0x8000)!=0,
            (GetAsyncKeyState(VK_HOME)&0x8000)!=0,zoom_allowed);
        static control_modes::KeyLatch smoothing_key;
        if(smoothing_key.press((GetAsyncKeyState(VK_END)&0x8000)!=0,sensitivity_allowed)){
            hud_prediction_requested=!hud_prediction_requested.load();settings_panel=true;hud_enabled=true;
            log_line("HUD_PREDICTION requested=%d display-only bounded camera prediction",hud_prediction_requested.load()?1:0);
        }
        static control_modes::KeyLatch panel_key;
        if(panel_key.press((GetAsyncKeyState(VK_F1)&0x8000)!=0,plain&&foreground_is_game()&&enabled.load()&&active.load()&&!game_paused.load()&&!gaze_active.load()&&!context_suspended.load())) {
            settings_panel=!settings_panel.load();if(settings_panel.load())hud_enabled=true;
            log_line("SETTINGS_PANEL visible=%d",settings_panel.load()?1:0);
        }
        static control_modes::KeyLatch helmet_key;
        if(helmet_key.press((GetAsyncKeyState(VK_F2)&0x8000)!=0,plain&&foreground_is_game()&&enabled.load()&&active.load()&&!game_paused.load()&&!gaze_active.load()&&!context_suspended.load())) {
            helmet_enabled.store(!helmet_enabled.load());
            helmet_notice.store(helmet_enabled.load()?1:0);helmet_notice_until=GetTickCount64()+2500;
            log_line("HMD mode requested=%d",helmet_enabled.load()?1:0);
        }
        static control_modes::KeyLatch camera_key;
        const bool f3=(GetAsyncKeyState(VK_F3)&0x8000)!=0;
        if(camera_key.press(f3,plain&&foreground_is_game()&&enabled.load()&&active.load()&&!game_paused.load()&&!gaze_active.load()&&!context_suspended.load()))camera_toggle_requested=true;
        static control_modes::KeyLatch mode_key;
        bool f4=(GetAsyncKeyState(VK_F4)&0x8000)!=0;
        if(mode_key.press(f4,plain&&foreground_is_game()&&active.load())) {
            int next=control_mode.load()==1?0:1;control_mode.store(next);mode_notice_pending=true;
            log_line("CONTROL_MODE selected=%d name=%s; learned models retained",next,next?"AGILE 2.1":"CLASSIC 2.0");
        }
        bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        if (f8 && !f8_down && foreground_is_game()) {enabled.store(!enabled.load());if(enabled.load())transition_center_requested=true;}
        if (f9 && !f9_down && foreground_is_game()) recenter_requested.store(true);
        f8_down = f8;
        f9_down = f9;
        LARGE_INTEGER due{};due.QuadPart=-40000;
        if(input_timer&&SetWaitableTimer(input_timer,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(input_timer,20);else Sleep(1);
    }
    if(input_timer)CloseHandle(input_timer);
}

void sensitivity_keys(bool up,bool down,bool reset,bool allowed) {
    static control_modes::KeyLatch up_key,down_key,reset_key;
    const bool u=up_key.press(up,allowed),d=down_key.press(down,allowed),r=reset_key.press(reset,allowed);
    if(!r && (u==d))return;
    float next=r?configured_sensitivity.load():live_sensitivity.load()*(u?1.1f:1.f/1.1f);
    next=std::clamp(std::round(next*100000.f)/100000.f,.01f,1.f);
    live_sensitivity=next;sensitivity_notice_until=GetTickCount64()+2500;
    log_line("SENSITIVITY session=%.5f configured=%.5f",next,configured_sensitivity.load());
}

void zoom_keys(bool up,bool down,bool reset,bool allowed){
    static control_modes::KeyLatch uk,dk,rk;
    const bool u=uk.press(up,allowed),d=dk.press(down,allowed),r=rk.press(reset,allowed);
    if(!r&&(u==d))return;
    free_look_zoom=r?configured_zoom.load():std::clamp(free_look_zoom.load()+(u?.25f:-.25f),1.f,3.f);
    zoom_notice_until=GetTickCount64()+2500;
    log_line("FREE_LOOK_ZOOM session=%.2f configured=%.2f",free_look_zoom.load(),configured_zoom.load());
}

void target_input_tick(){
    if(!independent_mouse.load())return;
    HudFrame frame;if(!read_hud_frame(frame))return;
    std::lock_guard<std::recursive_mutex> lock(target_mutex);
    if(!active.load()||!enabled.load()||game_paused.load()||gaze_active.load()||context_suspended.load()||
       !foreground_is_game()||frame.pawn!=aircraft.load()||GetTickCount64()-frame.tick>=250||
       recenter_requested.load()||resume_center_requested.load()||transition_center_requested.load()){
        mouse_delta.clear();return;
    }
    const bool looking=(GetAsyncKeyState('C')&0x8000)!=0;
    const auto delta=mouse_delta.take();
    if(!delta.x&&!delta.y&&looking==free_look.held)return;
    auto aim=flight::basis(target_pitch.load(),target_yaw.load(),0).f;
    const auto view=flight::basis(frame.cp,frame.cy,frame.cr);const float sensitivity=live_sensitivity.load();
    auto look=free_look.step(looking,aim,view,
        delta.x*sensitivity,delta.y*sensitivity,frame.fov,input_reference_fov.load(),{frame.ox,frame.oy,frame.oz});
    target_pitch=flight::pitch(aim);target_yaw=flight::yaw(aim);look_pitch=flight::pitch(look);look_yaw=flight::yaw(look);
    if(delta.x||delta.y){target_gaps.event(hud_qpc(),hud_frequency());++input_goal_sequence;rate_input_move.fetch_add(1,std::memory_order_relaxed);}
}

void update_commands() {
    std::lock_guard<std::recursive_mutex> lock(target_mutex);
    using namespace flight;
    if (!active.load() || !enabled.load() || game_paused.load() || gaze_active.load() || context_suspended.load()) {
        free_look.reset();control_goal_seen=false;reset_model_assist();
        command_pitch.store(0); command_roll.store(0); command_yaw.store(0);
        if(game_paused.load() || gaze_active.load() || context_suspended.load()) { mouse_delta.clear(); }
        previous_pose_tick = 0;
        return;
    }
    if(transition_center_requested.exchange(false)){
        recenter_requested=true;resume_center_requested=false;previous_pose_tick=0;control_goal_seen=false;
        filtered_pitch_rate=filtered_yaw_rate=filtered_roll_rate=0;mouse_delta.clear();free_look.reset();
    }
    const auto now = GetTickCount64();
    const float dt = previous_pose_tick ? std::clamp((now-previous_pose_tick)/1000.0f,0.001f,0.1f) : 1.0f/60;
    const Basis b = basis(pose_pitch.load(),pose_yaw.load(),pose_roll.load());
    const Basis old = basis(previous_pitch,previous_yaw,previous_roll);
    // Estimate body angular velocity from the moving basis, avoiding Euler wrap/pole artifacts.
    V omega = (cross(old.f,b.f)+cross(old.r,b.r)+cross(old.u,b.u))*(0.5f/dt/rad);
    float a = 1-std::exp(-12*dt);
    if (!previous_pose_tick) { omega={}; roll_level_blend.reset();pitch_arrival.reset();roll_arrival.reset();for(auto& ref:target_references)ref.reset();have_roll_goal=false; }
    filtered_pitch_rate += (-dot(omega,b.r)-filtered_pitch_rate)*a;
    filtered_yaw_rate += (dot(omega,b.u)-filtered_yaw_rate)*a;
    filtered_roll_rate += (-dot(omega,b.f)-filtered_roll_rate)*a;
    previous_pitch=pose_pitch.load(); previous_yaw=pose_yaw.load(); previous_roll=pose_roll.load();
    previous_pose_tick=now;
    V aim=basis(target_pitch.load(),target_yaw.load(),0).f;
    const bool manual = !foreground_is_game();
    if (recenter_requested.exchange(false) || manual) {
        aim=b.f;control_goal_seen=false;++hud_epoch;
        mouse_delta.clear();
        for(auto& ref:target_references)ref.reset();have_roll_goal=false;
    }
    const Basis view=basis(camera_pitch.load(),camera_yaw.load(),camera_roll.load());
    if(resume_center_requested.exchange(false)) {
        control_goal_seen=false;
        for(auto& ref:target_references)ref.reset();have_roll_goal=false;
        // Intersect the camera-centre ray with the HUD's 500m aim sphere.
        V offset{view_offset_x.load(),view_offset_y.load(),view_offset_z.load()};
        const float along=dot(offset,view.f);
        const float t=-along+std::sqrt(std::max(0.0f,along*along+50000.0f*50000.0f-dot(offset,offset)));
        aim=unit(offset+view.f*t);
        mouse_delta.clear();
        filtered_pitch_rate=filtered_yaw_rate=filtered_roll_rate=0;
    }
    const V goal_before_input=independent_mouse.load()&&control_goal_seen?control_last_goal:aim;
    const float sensitivity=live_sensitivity.load();
    const bool looking=!manual && (GetAsyncKeyState('C')&0x8000)!=0;
    MouseDelta delta{};
    if(independent_mouse.load()){
        const auto sequence=input_goal_sequence.load();delta.x=sequence!=control_last_sequence?1:0;control_last_sequence=sequence;
        // Only the input thread advances FreeLook; game-thread recenter still owns lifecycle resets.
        target_pitch=flight::pitch(aim);target_yaw=flight::yaw(aim);
        if(!looking){look_pitch=target_pitch.load();look_yaw=target_yaw.load();}
    }else{
        delta=mouse_delta.take();
        const V camera_target=free_look.step(looking,aim,view,delta.x*sensitivity,delta.y*sensitivity,view_fov.load(),config.mouse_reference_fov,
            {view_offset_x.load(),view_offset_y.load(),view_offset_z.load()});
        look_pitch=flight::pitch(camera_target);look_yaw=flight::yaw(camera_target);
        target_pitch=flight::pitch(aim);target_yaw=flight::yaw(aim);
    }
    control_last_goal=aim;control_goal_seen=true;
    const float f=dot(aim,b.f), right=dot(aim,b.r), up=dot(aim,b.u);
    const float angle=std::acos(std::clamp(f,-1.0f,1.0f))/rad;
    // Direct MouseFlight Plane.RunAutopilot port: normalized local target * 5.
    // Unity (right, up, forward) -> Unreal (forward, right, up).
    // Positive UE Pitch raises the nose; positive UE Roll lowers the right wing.
    const float p=up*5.0f, y=right*5.0f;
    // AC-specific rate-limited PD control. Body rates are degrees/second.
    // The reference proportional demand alone does not brake AC's fast roll response.
    const float pitch_error=std::atan2(up,std::max(0.02f,f))/rad;
    const float yaw_error=std::atan2(right,std::max(0.02f,f))/rad;
    const float level_error=std::atan2(b.r.z,b.u.z)/rad;
    const float near_blend=tracking_weight(angle);
    // Bank until the target lies above the aircraft, then pull toward it.
    const float turn_bank_error=std::clamp(std::atan2(right,std::max(0.12f,up))/rad,-90.0f,90.0f);
    const float roll_blend=roll_level_blend.step(angle,dt);
    const float roll_error=level_error*(1-roll_blend)+turn_bank_error*roll_blend;
    // Separate fast tracking from gentler final leveling. Braking remains available.
    const float roll_speed=70.0f+70.0f*roll_blend;
    float rcmd=roll_arrival.command(roll_error,filtered_roll_rate,roll_speed,240.0f,3.5f,1.0f,170.0f,1.0f,config.arrival_braking,dt);
    const float final_gain=1.0f-near_blend;
    float pcmd=pitch_arrival.command(pitch_error,filtered_pitch_rate,45.0f,90.0f,1.8f+0.8f*final_gain,0.2f,55.0f,0.85f,config.arrival_braking,dt);
    const float yaw_rate=std::clamp(dead(yaw_error,0.2f)*(1.2f+0.6f*final_gain),-7.0f,7.0f);
    float ycmd=std::clamp((yaw_rate-filtered_yaw_rate*0.6f)/10.0f,-0.7f,0.7f);
    const V goal_omega=cross(goal_before_input,aim)*(1.f/dt/rad);
    const bool moving_goal=!manual&&!looking&&(delta.x!=0||delta.y!=0);
    if(looking){target_references[0].reset();target_references[2].reset();}
    const float roll_goal=wrap_degrees(pose_roll.load()+roll_error);
    const float goal_roll_rate=have_roll_goal?wrap_degrees(roll_goal-previous_roll_goal)/dt:0;
    previous_roll_goal=roll_goal;have_roll_goal=true;
    std::array<float,3> references{
        target_references[0].update(-dot(goal_omega,b.r),moving_goal,dt,45.f),
        target_references[1].update(goal_roll_rate,std::abs(pose_pitch.load())<60.f,dt,roll_speed),
        target_references[2].update(dot(goal_omega,b.u),moving_goal,dt,7.f)};
    if(!manual)apply_model_control(pitch_error,roll_error,yaw_error,filtered_pitch_rate,filtered_roll_rate,filtered_yaw_rate,-dot(omega,b.r),-dot(omega,b.f),dot(omega,b.u),dt,pcmd,rcmd,ycmd,roll_speed,references);else reset_model_assist();
    command_pitch.store(manual?0:pcmd*config.pitch_sign);
    command_yaw.store(manual?0:ycmd*config.yaw_sign);
    command_roll.store(manual?0:rcmd*config.roll_sign);
    if(now-telemetry_tick>=10000) {
        telemetry_tick=now;
        log_line("0.2.18 adaptive localScaled=(%.3f,%.3f) angle=%.1f roll=%.1f rollErrorDeg=%.3f bodyrate=(%.1f,%.1f,%.1f) cmd=(%.2f,%.2f,%.2f) rollBlend=%.3f leveling=%d",
          p,y,angle,pose_roll.load(),roll_error,filtered_pitch_rate,filtered_yaw_rate,filtered_roll_rate,
          command_pitch.load(),command_yaw.load(),command_roll.load(),roll_blend,roll_level_blend.leveling);
    }
}
#include "native_camera.h"

void release_controls() {
    std::lock_guard<std::recursive_mutex> lock(target_mutex);
    control_goal_seen=false;
    full_model_new_flight=true;
    active.store(false); aircraft.store(0);
    command_pitch.store(0); command_yaw.store(0); command_roll.store(0);
    mouse_delta.clear();
    previous_pose_tick=0; free_look.reset();
    receive_camera(0,0,0,0,0);
}

// Called synchronously on the game thread. Fixed numeric arguments replace
// the pipe queue, sscanf and the camera-target disk snapshot entirely.
void receive_pose(const double (&v)[13]) {
    const auto pose_received=hud_qpc();pose_gaps.event(pose_received,hud_frequency());
    std::lock_guard<std::recursive_mutex> lock(target_mutex);
    const uintptr_t address=static_cast<uintptr_t>(v[0]);
    const float pitch=float(v[1]),yaw=float(v[2]),roll=float(v[3]);
    const float view_pitch=float(v[4]),view_yaw=float(v[5]),view_roll=float(v[6]);
    const float fov=float(v[7]),ox=float(v[8]),oy=float(v[9]),oz=float(v[10]);
    const bool paused=v[11]!=0,gazing=v[12]!=0;
    if(gaze_active.exchange(gazing)!=gazing) {
        ++hud_epoch;
        mouse_delta.clear();
        command_pitch.store(0); command_yaw.store(0); command_roll.store(0);
        log_line("gaze: %s",gazing?"native camera and controls; mouse target frozen":"mouse mode resumed");
    }
    const bool was_paused=game_paused.exchange(paused);
    if(was_paused!=paused) {
        ++hud_epoch;
        mouse_delta.clear();
        if(was_paused) resume_center_requested.store(true);
        else { command_pitch.store(0); command_roll.store(0); command_yaw.store(0); }
        log_line("game pause: %s",paused?"paused; aim frozen":"resumed; centre aim on next pose");
    }
    view_offset_x.store(ox); view_offset_y.store(oy); view_offset_z.store(oz);
    view_fov.store(std::clamp(fov,15.0f,150.0f));
    if (aircraft.load() != static_cast<uintptr_t>(address)) {
        aircraft.store(static_cast<uintptr_t>(address));
        roll_reference.store(roll);
        previous_pose_tick = 0;
        telemetry_tick = 0;
        recenter_requested.store(true);control_goal_seen=false;
        free_look.reset();
        log_line("aircraft acquired 0x%llX pose=(%.3f,%.3f,%.3f) camera=(%.3f,%.3f,%.3f)",
                 static_cast<unsigned long long>(address), pitch, yaw, roll, view_pitch, view_yaw, view_roll);
    }
    pose_pitch.store(pitch);
    pose_yaw.store(yaw);
    pose_roll.store(roll);
    camera_pitch.store(view_pitch);
    camera_yaw.store(view_yaw);
    camera_roll.store(view_roll);
    pose_tick.store(GetTickCount64());
    active.store(true);
    update_commands();
    stage_hud_frame({address,GetTickCount64(),pitch,yaw,roll,view_pitch,view_yaw,view_roll,
        target_pitch.load(),target_yaw.load(),fov,ox,oy,oz,0,pose_received});
}

#include "smooth_overlay.h"
#include "gpu_hud_loop.h"

unsigned char* find_hook() {
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    auto* sections = IMAGE_FIRST_SECTION(nt);
    unsigned char* found{};
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        auto& section = sections[index];
        if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        auto* begin = base + section.VirtualAddress;
        const size_t size = section.Misc.VirtualSize;
        for (size_t offset = 0; offset + sizeof(yawCode) <= size; ++offset) {
            if (begin[offset] == yawCode[0] && matchesYawCode(begin + offset, size - offset)) {
                if (found) return nullptr;
                found = begin + offset;
            }
        }
    }
    return found;
}

bool create_absolute_hook(void* target, void* detour, void** trampoline_out) {
    // The matched AC8 function starts with 16 bytes of complete instructions and
    // none of them use RIP-relative addressing. Copying those instructions lets
    // us place the trampoline anywhere in the 64-bit address space instead of
    // relying on MinHook finding a free executable page within +/-2 GB.
    constexpr size_t copied_size = 16;
    constexpr size_t jump_size = 14;
    auto* target_bytes = static_cast<unsigned char*>(target);
    if (memcmp(target_bytes, yawCode, copied_size) != 0) return false;

    auto* trampoline = static_cast<unsigned char*>(VirtualAlloc(
        nullptr, copied_size + jump_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!trampoline) return false;

    auto write_absolute_jump = [](unsigned char* destination, const void* address) {
        destination[0] = 0xFF;
        destination[1] = 0x25;
        *reinterpret_cast<uint32_t*>(destination + 2) = 0;
        *reinterpret_cast<uintptr_t*>(destination + 6) = reinterpret_cast<uintptr_t>(address);
    };

    memcpy(trampoline, target_bytes, copied_size);
    write_absolute_jump(trampoline + copied_size, target_bytes + copied_size);
    DWORD previous_trampoline_protection{};
    if (!VirtualProtect(trampoline, copied_size + jump_size, PAGE_EXECUTE_READ,
                        &previous_trampoline_protection)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }
    FlushInstructionCache(GetCurrentProcess(), trampoline, copied_size + jump_size);

    DWORD previous_target_protection{};
    if (!VirtualProtect(target_bytes, copied_size, PAGE_EXECUTE_READWRITE,
                        &previous_target_protection)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }
    write_absolute_jump(target_bytes, detour);
    memset(target_bytes + jump_size, 0x90, copied_size - jump_size);
    FlushInstructionCache(GetCurrentProcess(), target_bytes, copied_size);
    DWORD ignored{};
    VirtualProtect(target_bytes, copied_size, previous_target_protection, &ignored);

    *trampoline_out = trampoline;
    return true;
}

uintptr_t __fastcall process_input(unsigned char* state, unsigned char* context) {
    // Enemy/non-player invocations do not query the keyboard or foreground.
    const uintptr_t pawn = aircraft.load();
    if (!pawn || reinterpret_cast<uintptr_t>(state)!=pawn+0x22a0) {
        if(perf_enabled.load()) ++perf_other_inputs;
        return original(state,context);
    }
    if(perf_enabled.load()) ++perf_player_inputs;
    if(!active.load() || GetTickCount64()-pose_tick.load()>1000 || !enabled.load() ||
       game_paused.load() || gaze_active.load() || context_suspended.load() || !foreground_is_game()) return original(state,context);
    // Manual pitch also suspends automatic roll, matching MouseFlight maneuvers.
    // Preserve AC's native keyboard values, including opposing-key handling.
    auto held=[](int key) { return (GetAsyncKeyState(key)&0x8000)!=0; };
    const bool keyboard_pitch=held('W') || held('S');
    const bool keyboard_roll=keyboard_pitch || held('A') || held('D');
    const bool keyboard_yaw=held('Q') || held('E');
    bool override_input = true; // Lifecycle/foreground already checked above.
    if (override_input && reinterpret_cast<uintptr_t>(state) == pawn + 0x22a0) {
        __try {
            if (*reinterpret_cast<uintptr_t*>(context) != pawn + 0x2268 ||
                *reinterpret_cast<uintptr_t*>(context + 8) != pawn + 0x2c28) {
                override_input = false;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            override_input = false;
        }
    }
    float observed_p=0,observed_y=0,observed_r=0;bool observed_valid=false;
    if (override_input && reinterpret_cast<uintptr_t>(state) == pawn + 0x22a0) {
        __try {
            float* axes = reinterpret_cast<float*>(pawn + 0x2268);
            if(!keyboard_pitch) axes[config.pitch_slot] = command_pitch.load();
            if(!keyboard_yaw) axes[1] = command_yaw.load();
            if(!keyboard_roll) axes[config.roll_slot] = command_roll.load();
            observed_p=axes[config.pitch_slot]*config.pitch_sign;observed_y=axes[1]*config.yaw_sign;observed_r=axes[config.roll_slot]*config.roll_sign;observed_valid=true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            active.store(false);
            override_input = false;
        }
    }
    if(observed_valid)observe_adaptive_input(pawn,observed_p,observed_r,observed_y);
    if(observed_valid)observe_assist_input(pawn,observed_p,observed_r,(keyboard_pitch?1:0)|(keyboard_yaw?2:0)|(keyboard_roll?4:0),observed_y);
    if(observed_valid)shadow_capture_input(pawn,observed_p,observed_y,observed_r,(keyboard_pitch?1:0)|(keyboard_yaw?2:0)|(keyboard_roll?4:0));
    uintptr_t result = original(state, context);
    if (override_input && reinterpret_cast<uintptr_t>(state) == pawn + 0x22a0) {
        __try {
            // AC8's stock yaw path turns every non-zero axis value into full yaw.
            // Preserve only the proportional input target; downstream response remains stock.
            if(!keyboard_yaw)
                *reinterpret_cast<double*>(state + 8) = static_cast<double>(command_yaw.load());
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            active.store(false);
        }
    }
    return result;
}

bool prepare_hook() {
    if (hook_created) return true;
    hook_address = find_hook();
    if (!hook_address) {
        log_line("error input processor signature missing or ambiguous");
        return false;
    }
    MH_STATUS result = MH_Initialize();
    if (result != MH_OK && result != MH_ERROR_ALREADY_INITIALIZED) return false;
    result = MH_CreateHook(hook_address, reinterpret_cast<void*>(&process_input), reinterpret_cast<void**>(&original));
    if (result == MH_ERROR_MEMORY_ALLOC) {
        if (create_absolute_hook(hook_address, reinterpret_cast<void*>(&process_input),
                                 reinterpret_cast<void**>(&original))) {
            hook_created = true;
            log_line("input hook installed with absolute trampoline fallback");
            return true;
        }
        log_line("error absolute hook fallback: win32=%lu", GetLastError());
    }
    if (result != MH_OK) {
        log_line("error create hook: %s", MH_StatusToString(result));
        return false;
    }
    result = MH_EnableHook(hook_address);
    if (result != MH_OK) {
        log_line("error enable hook: %s", MH_StatusToString(result));
        return false;
    }
    hook_created = true;
    return true;
}

bool offline_authorized() {
    wchar_t value[16]{};
    return GetEnvironmentVariableW(L"EOS_USE_ANTICHEATCLIENTNULL", value, 16) > 0 && wcscmp(value, L"1") == 0;
}
bool bridge_verified=false;
DWORD bridge_thread=0;
std::atomic<bool> reload_requested{false};
bool on_bridge_thread() { return bridge_thread && bridge_thread==GetCurrentThreadId(); }
#include "helmet_selection.h"
} // namespace

extern "C" __declspec(dllexport) int ac8_mouseaim_start(lua_State* state) {
    init_paths();
    if(!logger_started.exchange(true)) {
        QueryPerformanceFrequency(&perf_frequency);
        std::thread(logger_loop).detach();
    }
    if(!bridge_verified) bridge_verified=compatible_lua_runtime();
    if(!bridge_verified) { log_line("bridge refused: UE4SS runtime hash mismatch"); return 0; }
    LuaView lua(state);
    double handshake[2]{};
    if(!read_numbers(lua,handshake) || handshake[0]!=1729 || handshake[1]!=0.125) return 0;
    if(running.load()) { lua.set_number(30); return 1; }
    if (!offline_authorized()) {
        log_line("inactive: offline launch marker missing; multiplayer-safe refusal");
        return 0;
    }
    load_config();
    log_line("CAMERA_SETTINGS mode=%d distance_m=%.1f height_m=%.1f F3=toggle; mission/cinematic recenter enabled",camera_view_mode.load(),camera_distance_cm.load()/100,camera_height_cm.load()/100);
    log_line("PROFILE 2.2.2 CAMERA-ORBIT SWITCHABLE MODEL CONTROL default_mode=%d sensitivity=%.4f hud_target_hz=%d reference_fov=%.1f braking=%.2f",control_mode.load(),config.sensitivity,hud_target_hz.load(),config.mouse_reference_fov,config.arrival_braking);
    if (!prepare_hook()) return 0;
    install_native_camera();
    helmet::install();
    if (prepare_raw_input_capture()) {
        log_line("mouse capture attached to AC8 raw input");
    } else {
        log_line("warning raw input hook unavailable; using DirectInput fallback");
    }
    shadow_start();
    running.store(true);
    std::thread(mouse_loop).detach();
    std::thread(overlay_loop).detach();
    log_line("ready: hold C for free look, release to return; F8 toggle, F9 recenter; RMB reserved");
    log_line("HUDSync1.3.7: cached last valid frame, active-rate retry, batched marker movement, source cadence diagnostics");
    lua.set_number(30);
    return 1;
}

extern "C" __declspec(dllexport) int ac8_mouseaim_reload(void*) {
    reload_requested.store(true);
    return 0;
}

struct CanvasCounters {uint64_t callbacks=0,eligible=0,drawn=0,lines=0,errors=0;double cost_ms=0,max_ms=0;ULONGLONG since=0;} canvas_counts;
extern "C" __declspec(dllexport) int ac8_mouseaim_canvas_mode(lua_State* state){
    LuaView lua(state);lua.set_number(hud_renderer.load());return 1;
}
extern "C" __declspec(dllexport) int ac8_mouseaim_canvas_gate(lua_State* state){
    if(!running.load()||!on_bridge_thread())return 0;
    ++canvas_counts.callbacks;
    LuaView lua(state);
    const int reason=(!active.load()?1:0)|(!enabled.load()?2:0)|(!hud_enabled.load()?4:0)|(game_paused.load()?8:0)|(gaze_active.load()?16:0)|(context_suspended.load()?32:0)|(!foreground_is_game()?64:0);
    lua.set_number((hud_renderer.load()!=2&&hud_renderer.load()!=3)?0:reason==0?1:2);lua.set_number(reason);return 2;
}
// Game-thread-only draw transaction: ring and nose must use one published frame.
HudFrame canvas_draw_frame;
bool canvas_draw_valid=false;
int canvas_position_status(uintptr_t pawn,float width,float height,float& x,float& y,float& radius){
    canvas_draw_valid=false;
    if(width<320||height<200||width>16384||height>16384)return -1;
    HudFrame f;if(!read_hud_frame(f))return -2;
    if(f.pawn!=pawn||pawn!=aircraft.load())return -3;
    if(GetTickCount64()-f.tick>=250||f.epoch!=hud_epoch.load())return -4;
    // Do not mix a newer asynchronous mouse goal with an older camera sample.
    auto view=flight::basis(f.cp,f.cy,f.cr);auto point=flight::basis(f.tp,f.ty,0).f*50000-flight::V{f.ox,f.oy,f.oz};
    float depth=flight::dot(point,view.f);if(depth<=.01f)return -5;if(!std::isfinite(f.fov)||f.fov<15||f.fov>150)return -6;
    float focal=width*.5f/std::tan(f.fov*.5f*flight::rad);x=width*.5f+focal*flight::dot(point,view.r)/depth;y=height*.5f-focal*flight::dot(point,view.u)/depth;
    radius=30*height/1080.f;
    canvas_draw_frame=f;canvas_draw_valid=true;
    return std::isfinite(x)&&std::isfinite(y)&&x>=radius&&x<=width-radius&&y>=radius&&y<=height-radius?1:-7;
}
bool canvas_position(uintptr_t pawn,float width,float height,float& x,float& y,float& radius){return canvas_position_status(pawn,width,height,x,y,radius)==1;}
extern "C" __declspec(dllexport) int ac8_mouseaim_canvas_viewport(lua_State* state){
    if(!running.load()||!on_bridge_thread())return 0;RECT rect{};LuaView lua(state);
    if(!game_window||!GetClientRect(game_window,&rect))return 0;
    lua.set_number(rect.right);lua.set_number(rect.bottom);lua.set_number(double(hud_qpc())*1000/hud_frequency());return 3;
}
extern "C" __declspec(dllexport) int ac8_mouseaim_canvas_ring(lua_State* state){
    if(!running.load()||!on_bridge_thread())return 0;
    LuaView lua(state);double v[3]{};if(!read_numbers(lua,v)||!live_pointer_number(v[0]))return 0;
    if((hud_renderer.load()!=2&&hud_renderer.load()!=3)||!active.load()||!enabled.load()||!hud_enabled.load()||game_paused.load()||gaze_active.load()||context_suspended.load()||!foreground_is_game()){lua.set_number(-8);return 1;}
    float x=0,y=0,radius=0;int status=canvas_position_status(uintptr_t(v[0]),float(v[1]),float(v[2]),x,y,radius);lua.set_number(status);if(status!=1)return 1;
    ++canvas_counts.eligible;lua.set_number(x);lua.set_number(y);lua.set_number(radius);lua.set_number(std::max(1.f,float(v[2])/1080.f*1.3f));lua.set_number(double(hud_qpc())*1000/hud_frequency());return 6;
}

double umg_update_hz=0;
extern "C" __declspec(dllexport) int ac8_mouseaim_canvas_ui(lua_State* state){
 if(!running.load()||!on_bridge_thread())return 0;LuaView lua(state);auto now=GetTickCount64();
 if(mode_notice_pending.exchange(false))mode_notice_until=now+2500;
 int toast=0;float value=0;uint64_t end=0;
 auto choose=[&](uint64_t until,int id,float v){if(until>now&&until>=end){end=until;toast=id;value=v;}};
 choose(mode_notice_until.load(),1,float(control_mode.load()));choose(camera_notice_until.load(),2,float(camera_view_mode.load()));
 choose(helmet_notice_until.load(),3,float(helmet_notice.load()));choose(sensitivity_notice_until.load(),4,live_sensitivity.load());choose(zoom_notice_until.load(),5,free_look_zoom.load());
 const double values[]={settings_panel.load()?1.:0.,helmet_enabled.load()?1.:0.,double(control_mode.load()),double(camera_view_mode.load()),live_sensitivity.load(),configured_sensitivity.load(),free_look_zoom.load(),configured_zoom.load(),view_fov.load(),umg_update_hz,double(toast),value,end?std::min(1.,double(end-now)/90.):0.,hud_opacity.load(),hud_connector.load()?1.:0.,hud_link_always.load()?1.:0.};
 for(double v:values)lua.set_number(v);return 16;
}
bool canvas_nose_position(uintptr_t pawn,float width,float height,float& x,float& y){
 if(!canvas_draw_valid||width<320||height<200)return false;
 const auto& f=canvas_draw_frame;
 if(f.pawn!=pawn||pawn!=aircraft.load()||GetTickCount64()-f.tick>=250||f.epoch!=hud_epoch.load())return false;
 auto view=flight::basis(f.cp,f.cy,f.cr);auto point=flight::basis(f.p,f.y,f.r).f*50000-flight::V{f.ox,f.oy,f.oz};float z=flight::dot(point,view.f);if(z<=.01)return false;
 float focal=width*.5f/std::tan(std::clamp(f.fov,15.f,150.f)*.5f*flight::rad);
 x=width*.5f+focal*flight::dot(point,view.r)/z;y=height*.5f-focal*flight::dot(point,view.u)/z;
 return std::isfinite(x)&&std::isfinite(y);
}
extern "C" __declspec(dllexport) int ac8_mouseaim_canvas_nose(lua_State* state){
 if(!running.load()||!on_bridge_thread())return 0;LuaView lua(state);double v[3]{};if(!read_numbers(lua,v)||!live_pointer_number(v[0]))return 0;
 float x=0,y=0;if(!canvas_nose_position(uintptr_t(v[0]),float(v[1]),float(v[2]),x,y))return 0;
 lua.set_number(1);lua.set_number(x);lua.set_number(y);return 3;
}
extern "C" __declspec(dllexport) int ac8_mouseaim_canvas_report(lua_State* state){
    if(!running.load()||!on_bridge_thread())return 0;
    LuaView lua(state);double v[2]{};if(!read_numbers(lua,v))return 0;
    if(v[0]<0){++canvas_counts.errors;return 0;}
    double elapsed=double(hud_qpc())*1000/hud_frequency()-v[1];
    if(v[0]!=24||elapsed<0||elapsed>1000)return 0;
    ++canvas_counts.drawn;canvas_counts.lines+=24;canvas_counts.cost_ms+=elapsed;canvas_counts.max_ms=std::max(canvas_counts.max_ms,elapsed);return 0;
}
extern "C" __declspec(dllexport) int ac8_mouseaim_begin(void*) {
    if(!running.load()) return 0;
    if(!bridge_thread) bridge_thread=GetCurrentThreadId();
    if(!on_bridge_thread()) return 0;
    rate_bridge.fetch_add(1,std::memory_order_relaxed);
    if(hud_renderer.load()==2||hud_renderer.load()==3){
        auto now=GetTickCount64();if(!canvas_counts.since)canvas_counts.since=now;
        if(now-canvas_counts.since>=10000){double seconds=(now-canvas_counts.since)/1000.;
            log_line("UMG_HUD checks=%llu eligible=%llu updates=%llu update_hz=%.2f mean_cpu_ms=%.3f max_cpu_ms=%.3f errors=%llu external_overlay=OFF (not display FPS)",canvas_counts.callbacks,canvas_counts.eligible,canvas_counts.drawn,canvas_counts.drawn/seconds,canvas_counts.drawn?canvas_counts.cost_ms/canvas_counts.drawn:0,canvas_counts.max_ms,canvas_counts.errors);
            umg_update_hz=canvas_counts.drawn/seconds;canvas_counts={};canvas_counts.since=now;
        }
    }

    if(camera_toggle_requested.exchange(false)){
        ++hud_epoch;
        camera_view_mode=1-camera_view_mode.load();camera_notice_until=GetTickCount64()+2500;
        log_line("CAMERA_MODE selected=%d name=%s distance_m=%.1f height_m=%.1f",camera_view_mode.load(),camera_view_mode.load()?"FAR":"GAME",camera_distance_cm.load()/100,camera_height_cm.load()/100);
    }
    if(reload_requested.exchange(false)) {
        load_config(); recenter_requested.store(true); log_line("configuration reloaded; camera_mode=%d distance_m=%.1f height_m=%.1f",camera_view_mode.load(),camera_distance_cm.load()/100,camera_height_cm.load()/100);
    }
    if(perf_enabled.load()) {
        script_start=perf_clock();
        if(previous_frame_start) {
            const auto gap=perf_us(script_start-previous_frame_start);
            perf_frame_gap.add(gap);
            if(gap>50000) ++perf_hitches;
        }
        previous_frame_start=script_start;
    } else { script_start=0; previous_frame_start=0; }
    return 0;
}

// Read the effective final-POV FOV, not a cached pre-override game property.
extern "C" __declspec(dllexport) int ac8_mouseaim_camera_fov(lua_State* state){
    if(!running.load()||!on_bridge_thread())return 0;
    LuaView lua(state);double v[1]{};if(!read_numbers(lua,v)||!live_pointer_number(v[0]))return 0;
    const bool fresh=uintptr_t(v[0])==effective_fov_manager.load()&&GetTickCount64()-effective_fov_tick.load()<250;
    lua.set_number(fresh?effective_camera_fov.load():0);return 1;
}

extern "C" __declspec(dllexport) int ac8_mouseaim_manual_look(lua_State* state){
    if(!running.load()||!on_bridge_thread())return 0;
    LuaView lua(state);if(lua.get_stack_size()!=0)return 0;
    lua.set_number(foreground_is_game()&&(GetAsyncKeyState('C')&0x8000)?1:0);return 1;
}

extern "C" __declspec(dllexport) int ac8_mouseaim_context(lua_State* state){
    if(!running.load()||!on_bridge_thread())return 0;
    LuaView lua(state);double v[3]{};
    if(!read_numbers(lua,v)||(v[0]!=0&&v[0]!=1)||(v[1]!=0&&v[1]!=1)||(v[2]!=0&&v[2]!=1))return 0;
    bool manual_changed=manual_camera_active.exchange(v[2]!=0)!=(v[2]!=0);
    bool changed=context_suspended.exchange(v[0]!=0)!=(v[0]!=0);
    if(v[1]!=0){transition_center_requested=true;native_rig_reset_requested=true;}
    if(changed||v[1]!=0||manual_changed)++hud_epoch;
    if(changed||v[1]!=0||manual_changed)log_line("VIEW_CONTEXT suspended=%d recenter=%d manual_orbit=%d",int(v[0]),int(v[1]),int(v[2]));
    if(v[0]!=0){command_pitch=0;command_roll=0;command_yaw=0;mouse_delta.clear();}
    lua.set_number(1);lua.set_number(camera_view_mode.load());return 2;
}

extern "C" __declspec(dllexport) int ac8_mouseaim_frame(lua_State* state) {
    std::lock_guard<std::recursive_mutex> lock(target_mutex);
    if(!running.load() || !on_bridge_thread()) return 0;
    PerfSpan timing(perf_bridge);
    LuaView lua(state); double v[13]{};
    if(!read_numbers(lua,v) || !live_pointer_number(v[0])) { release_controls(); return 0; }
    for(size_t i=1;i<11;++i) if(std::abs(v[i])>1e12) { release_controls(); return 0; }
    if((v[11]!=0 && v[11]!=1) || (v[12]!=0 && v[12]!=1)) { release_controls(); return 0; }
    receive_pose(v);
    const bool on=enabled.load() && !game_paused.load() && !gaze_active.load() && !context_suspended.load() && foreground_is_game();
    lua.set_number(on?1:0); lua.set_number(look_pitch.load()); lua.set_number(look_yaw.load());
    return 3;
}

// Separate from frame(): that API returns the camera destination during C.
extern "C" __declspec(dllexport) int ac8_mouseaim_helmet_state(lua_State* state) {
    std::lock_guard<std::recursive_mutex> lock(target_mutex);
    if(!running.load()||!on_bridge_thread())return 0;
    LuaView lua(state);if(lua.get_stack_size()!=0)return 0;
    if(helmet_enabled.load()&&!helmet::supported){helmet_enabled=false;helmet_notice=2;helmet_notice_until=GetTickCount64()+2500;}
    const bool usable=helmet_enabled.load()&&helmet::supported&&enabled.load()&&active.load()&&
        !game_paused.load()&&!gaze_active.load()&&!context_suspended.load()&&foreground_is_game();
    RECT client{};if(game_window)GetClientRect(game_window,&client);
    const double aspect=client.bottom>0?double(client.right)/client.bottom:0;
    if(!usable)helmet::snapshot={};
    lua.set_number(usable?1:0);lua.set_number(target_pitch.load());lua.set_number(target_yaw.load());lua.set_number(aspect);return 4;
}
extern "C" __declspec(dllexport) int ac8_mouseaim_helmet_submit(lua_State* state) {
    if(!running.load()||!on_bridge_thread())return 0;
    LuaView lua(state);double v[4]{};helmet::snapshot={};
    if(!read_numbers(lua,v))return 0;
    if(v[0]==-1){helmet_enabled=false;helmet_notice=2;helmet_notice_until=GetTickCount64()+2500;return 0;}
    if(v[0]==0)return 0;
    if(!helmet_enabled.load()||!helmet::supported||static_cast<uintptr_t>(v[0])!=aircraft.load())return 0;
    for(double n:v)if(n!=0&&!live_pointer_number(n))return 0;
    helmet::snapshot={uintptr_t(v[0]),uintptr_t(v[1]),uintptr_t(v[2]),uintptr_t(v[3]),GetTickCount64()};return 0;
}

extern "C" __declspec(dllexport) int ac8_mouseaim_camera(lua_State* state) {
    if(!running.load() || !on_bridge_thread()) return 0;
    LuaView lua(state); double v[5]{};
    if(!read_numbers(lua,v) ||
       std::abs(v[2])>1e9 || std::abs(v[3])>1e9 || std::abs(v[4])>1e9 ||
       (v[0]!=0 && (!live_pointer_number(v[0]) || !live_pointer_number(v[1]) ||
                    static_cast<uintptr_t>(v[1])!=aircraft.load()))) {
        release_controls(); return 0;
    }
    const bool accepted=receive_camera(static_cast<uintptr_t>(v[0]),v[0]?static_cast<uintptr_t>(v[1]):0,v[2],v[3],v[4]);
    if(script_start) { perf_script.add(perf_us(perf_clock()-script_start)); script_start=0; }
    lua.set_number(accepted?1:0);
    return 1;
}

extern "C" __declspec(dllexport) int ac8_mouseaim_release(void*) {
    if(running.load() && on_bridge_thread()) { release_controls(); script_start=0; shadow_new_flight=true; }
    return 0;
}

extern "C" __declspec(dllexport) int ac8_mouseaim_perf(void*) {
    if(running.load()) {
        const bool enabled_now=!perf_enabled.load(); perf_enabled.store(enabled_now);
        if(!enabled_now) perf_flush.store(true);
        log_line("PERF capture %s (10-second summaries, no per-frame disk writes)",enabled_now?"ON":"OFF");
    }
    return 0;
}

#include "control_observation.h"
extern "C" __declspec(dllexport) int ac8_mouseaim_control_observe(lua_State* state){
    if(!running.load()||!on_bridge_thread())return 0;
    LuaView lua(state);double v[7]{};
    int status=read_numbers(lua,v)?accept_control_observation(v):-1;
    static int last=0;static uint64_t tick=0;
    auto now=GetTickCount64();
    if(last==0||now-tick>=10000){
        log_line("CONTROL_OBSERVATION status=%d plane=%.0f speed=%.2f",status,v[5],assist_speed.load());last=status;tick=now;
    }
    lua.set_number(status);return 1;
}
extern "C" __declspec(dllexport) int ac8_mouseaim_observe(lua_State* state){
    if(!running.load()||!on_bridge_thread())return 0;
    LuaView lua(state);double v[13]{};
    int status=1;
    if(!read_numbers(lua,v))status=-1;
    else if(!active.load()||game_paused.load()||gaze_active.load()||context_suspended.load())status=-2;
    else if(!live_pointer_number(v[0])||static_cast<uintptr_t>(v[0])!=aircraft.load())status=-3;
    else {
        for(int i=1;i<13;++i)if(std::abs(v[i])>1e12)status=-4;
        if(v[9]<0||v[9]>2147483647.0||v[9]!=std::floor(v[9]))status=-5;
        if(v[10]<0||v[10]>.25||v[11]<0)status=-6;
        if(v[12]!=0&&v[12]!=1)status=-7;
    }
    if(status==1)shadow_capture_state(v);
    lua.set_number(status);return 1;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        self_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
