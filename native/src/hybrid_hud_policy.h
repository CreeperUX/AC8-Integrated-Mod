#pragma once
namespace hybrid_hud {
inline bool needs_overlay(bool hybrid,bool panel,bool notice){return !hybrid||panel||notice;}
inline int pacing(bool wanted,bool retry,bool hybrid,bool notice,int configured){return !wanted?10:retry?1000:hybrid?(notice?60:20):configured;}
}
