#pragma once
// In-game HUD toggle keys. F7 shows/hides the mod HUD; Alt+F7 shows/hides only the reference gun cross (2.4.1).
namespace hud_toggles {
enum class F7Action{None,Hud,GunCross};
// edge: F7 newly pressed while the game is in the foreground. Plain F7 keeps its 2.4.0 behaviour (Ctrl/Shift held
// for throttle do not block it); Alt+F7 toggles the cross; Alt together with Ctrl or Shift does nothing.
inline F7Action f7_action(bool edge,bool alt,bool ctrl,bool shift){
    if(!edge)return F7Action::None;
    if(!alt)return F7Action::Hud;
    return (ctrl||shift)?F7Action::None:F7Action::GunCross;
}
}
